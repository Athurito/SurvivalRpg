#include "Portals/RpgPortalActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/Abilities/RpgGameplayAbility_ClosePortal.h"
#include "AbilitySystem/Abilities/RpgGameplayAbility_EnterPortal.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Combat/RpgCombatMessages.h"
#include "SurvivalRpg/System/RpgAssetManager.h"
#include "SurvivalRpg/System/RpgGameData.h"
#include "GameplayTags/RpgPortalGameplayTags.h"
#include "Portals/RpgPortalRealmMarkerActor.h"
#include "Portals/RpgPortalEncounterDefinition.h"
#include "Portals/RpgPortalExitActor.h"
#include "Portals/RpgPortalMessages.h"
#include "Portals/RpgPortalRealmEventDirector.h"
#include "Portals/RpgPortalTravelComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalActor)

DEFINE_LOG_CATEGORY_STATIC(LogRpgPortalActor, Log, All);

namespace
{
	constexpr float PortalParticipantSafeTransformSampleInterval = 1.0f;
}

ARpgPortalActor::ARpgPortalActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitSphereRadius(220.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("Interactable_OverlapDynamic"));
	InteractionCollision->SetGenerateOverlapEvents(true);

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(SceneRoot);
	PortalMesh->SetCollisionProfileName(TEXT("Interactable_BlockDynamic"));

	ClosePortalAbilityClass = URpgGameplayAbility_ClosePortal::StaticClass();
	EnterPortalAbilityClass = URpgGameplayAbility_EnterPortal::StaticClass();
	ExitPortalActorClass = ARpgPortalExitActor::StaticClass();
}

void ARpgPortalActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		RegisterCombatMessageListener();

		if (bAutoStartOnBeginPlay)
		{
			StartEncounter();
		}
	}
}

void ARpgPortalActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterCombatMessageListener();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParticipantLocationSampleTimerHandle);
	}

	for (AActor* TrackedEnemy : TrackedEnemies)
	{
		if (TrackedEnemy)
		{
			TrackedEnemy->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrackedEnemyDestroyed);
		}
	}
	TrackedEnemies.Reset();

	if (TrackedBoss)
	{
		TrackedBoss->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrackedBossDestroyed);
		TrackedBoss = nullptr;
	}

	for (AActor* RealmOccupant : RealmOccupants)
	{
		if (RealmOccupant)
		{
			RealmOccupant->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleRealmOccupantDestroyed);
		}
	}
	RealmOccupants.Reset();
	PendingRealmEntrants.Reset();
	if (RealmEventDirector)
	{
		RealmEventDirector->HandleRealmClosing(this);
	}
	RemoveAllRealmPersistentEffects();
	RealmParticipantStates.Reset();
	RealmOccupantPlayerNetIds.Reset();

	NotifyKnownTravelComponentsToUnload(ERpgPortalTravelState::Cancelled);
	DestroyExitPortal();
	DestroyRealmEventDirector();
	UnloadRealmLevelInstance();

	Super::EndPlay(EndPlayReason);
}

void ARpgPortalActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PortalState);
	DOREPLIFETIME(ThisClass, CurrentStability);
	DOREPLIFETIME(ThisClass, DefeatedTrackedEnemyCount);
	DOREPLIFETIME(ThisClass, TotalTrackedEnemyCount);
	DOREPLIFETIME(ThisClass, bRealmBossDefeated);
	DOREPLIFETIME(ThisClass, RealmOccupantCount);
	DOREPLIFETIME(ThisClass, EncounterDefinition);
	DOREPLIFETIME(ThisClass, RealmLevelInstanceTransform);
	DOREPLIFETIME(ThisClass, RealmLevelInstanceName);
}

void ARpgPortalActor::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (IsRealmEncounterMode() && EnterPortalAbilityClass
		&& (PortalState == ERpgPortalState::Active
			|| PortalState == ERpgPortalState::RealmLoading
			|| PortalState == ERpgPortalState::RealmInProgress
			|| PortalState == ERpgPortalState::ExitOpen))
	{
		FInteractionOption Option;
		Option.InteractionTag = RpgPortalGameplayTags::Rpg_Interaction_Action_Portal_Enter;
		Option.InteractionAbilityToGrant = EnterPortalAbilityClass;
		Option.Prompt.ActionText = EncounterDefinition ? EncounterDefinition->EnterInteractionText : NSLOCTEXT("RpgPortal", "EnterPortal", "Enter Portal");
		Option.Prompt.TargetText = EncounterDefinition ? EncounterDefinition->EnterInteractionSubText : NSLOCTEXT("RpgPortal", "EnterPortalSubText", "Cross into the rift");
		Option.Prompt.AwarenessRange = 1000.0f;
		Option.Prompt.FocusRange = 650.0f;
		Option.Prompt.InteractionRange = 300.0f;
		Option.Prompt.InteractionPriority = 80;
		Option.TargetRef.TargetActor = this;

		InteractionBuilder.AddInteractionOption(Option);
		return;
	}

	if (!ClosePortalAbilityClass || PortalState == ERpgPortalState::Dormant || PortalState == ERpgPortalState::Closed)
	{
		return;
	}

	FInteractionOption Option;
	Option.InteractionTag = RpgPortalGameplayTags::Rpg_Interaction_Action_Portal_Close;
	Option.InteractionAbilityToGrant = ClosePortalAbilityClass;
	Option.Prompt.ActionText = EncounterDefinition ? EncounterDefinition->CloseInteractionText : NSLOCTEXT("RpgPortal", "ClosePortal", "Close Portal");
	Option.Prompt.TargetText = EncounterDefinition ? EncounterDefinition->CloseInteractionSubText : NSLOCTEXT("RpgPortal", "ClosePortalSubText", "Stabilize the rift");
	Option.Prompt.AwarenessRange = 1000.0f;
	Option.Prompt.FocusRange = 650.0f;
	Option.Prompt.InteractionRange = 300.0f;
	Option.Prompt.InteractionPriority = 80;
	Option.TargetRef.TargetActor = this;
	if (PortalState != ERpgPortalState::Sealable)
	{
		Option.Availability = ERpgInteractionAvailability::Blocked;
		Option.Prompt.BlockedReason = NSLOCTEXT("RpgPortal", "PortalNotSealable", "The rift is not stable enough to close");
	}

	InteractionBuilder.AddInteractionOption(Option);
}

void ARpgPortalActor::CustomizeInteractionEventData(const FGameplayTag& InteractionEventTag, FGameplayEventData& InOutEventData)
{
	InOutEventData.Target = this;
	InOutEventData.OptionalObject = EncounterDefinition;
}

void ARpgPortalActor::StartEncounter()
{
	if (!HasAuthority() || PortalState != ERpgPortalState::Dormant)
	{
		return;
	}

	CurrentStability = 0.0f;
	DefeatedTrackedEnemyCount = 0;
	TotalTrackedEnemyCount = 0;
	bRealmBossDefeated = false;
	RealmOccupantCount = 0;
	TrackedEnemies.Reset();
	RealmOccupants.Reset();
	PendingRealmEntrants.Reset();
	KnownTravelComponents.Reset();
	RealmParticipantStates.Reset();
	RealmOccupantPlayerNetIds.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParticipantLocationSampleTimerHandle);
	}
	NextPortalTravelRequestId = 0;
	TrackedBoss = nullptr;
	DestroyRealmEventDirector();
	ClearRealmMarkers();

	if (IsRealmEncounterMode())
	{
		EnsureRealmLevelStreamingConfig();
	}

	SetPortalState(ERpgPortalState::Active);

	if (IsBrokenOutbreakMode())
	{
		SpawnEncounterEnemies();
		RefreshStabilityFromProgress();

		if (TrackedEnemies.IsEmpty())
		{
			UE_LOG(LogRpgPortalActor, Warning, TEXT("%s spawned no BrokenOutbreak enemies and is now sealable."), *GetNameSafe(this));
			CurrentStability = GetMaxStability();
			SetPortalState(ERpgPortalState::Sealable);
		}
	}
}

void ARpgPortalActor::ConfigureEncounterDefinition(const URpgPortalEncounterDefinition* InEncounterDefinition)
{
	if (!HasAuthority())
	{
		return;
	}

	if (PortalState != ERpgPortalState::Dormant || TotalTrackedEnemyCount > 0 || DefeatedTrackedEnemyCount > 0 || !TrackedEnemies.IsEmpty() || TrackedBoss || !RealmOccupants.IsEmpty())
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s ignored encounter definition reconfiguration after the encounter started."), *GetNameSafe(this));
		return;
	}

	EncounterDefinition = InEncounterDefinition;
}

void ARpgPortalActor::ConfigureRealmLevelInstanceTransform(const FTransform& InRealmLevelInstanceTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	if (PortalState != ERpgPortalState::Dormant)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s ignored realm level instance transform configuration after the encounter started."), *GetNameSafe(this));
		return;
	}

	RealmLevelInstanceTransform = InRealmLevelInstanceTransform;
	if (RealmLevelInstanceName.IsEmpty())
	{
		RealmLevelInstanceName = GetDefaultRealmLevelInstanceName();
	}
}

bool ARpgPortalActor::TryClosePortal(AActor* ClosingActor)
{
	if (!HasAuthority() || PortalState != ERpgPortalState::Sealable)
	{
		return false;
	}

	CurrentStability = GetMaxStability();
	SetPortalState(ERpgPortalState::Closed);
	ApplyClosedPresentation();
	if (RealmEventDirector)
	{
		RealmEventDirector->HandleRealmClosing(this);
	}
	RemoveAllRealmPersistentEffects();
	InvalidateParticipantResumeStates();
	NotifyKnownTravelComponentsToUnload(ERpgPortalTravelState::Cancelled);
	DestroyExitPortal();
	DestroyRealmEventDirector();
	UnloadRealmLevelInstance();

	FRpgPortalCompletedMessage Message;
	Message.Portal = this;
	Message.Instigator = ClosingActor;
	Message.EncounterDefinition = EncounterDefinition;
	Message.CompletionTags = EncounterDefinition ? EncounterDefinition->CompletionTags : FGameplayTagContainer();
	Message.FinalStability = CurrentStability;
	Message.bRewardsRequireBossDefeat = EncounterDefinition ? EncounterDefinition->bRewardsRequireBossDefeat : true;
	Message.bBossDefeated = bRealmBossDefeated;
	Message.bRewardsEligible = ShouldRewardsBeEligible();

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.BroadcastMessage(RpgPortalGameplayTags::Rpg_Portal_Message_Completed, Message);
	}

	return true;
}

bool ARpgPortalActor::TryEnterPortal(AActor* EnteringActor)
{
	if (!HasAuthority() || !IsRealmEncounterMode() || !EnteringActor)
	{
		return false;
	}

	if (PortalState != ERpgPortalState::Active
		&& PortalState != ERpgPortalState::RealmLoading
		&& PortalState != ERpgPortalState::RealmInProgress
		&& PortalState != ERpgPortalState::ExitOpen
		&& PortalState != ERpgPortalState::Sealable)
	{
		return false;
	}

	AActor* TravelActor = ResolveTravelActor(EnteringActor);
	if (!TravelActor)
	{
		return false;
	}

	if (PortalState == ERpgPortalState::Active)
	{
		StartRealmEncounter();
	}

	if (PortalState == ERpgPortalState::RealmLoading)
	{
		PendingRealmEntrants.AddUnique(TravelActor);
		return true;
	}

	if (PortalState == ERpgPortalState::RealmInProgress || PortalState == ERpgPortalState::ExitOpen || PortalState == ERpgPortalState::Sealable)
	{
		return BeginPortalTravelForActor(TravelActor);
	}

	return false;
}

bool ARpgPortalActor::TryExitPortal(AActor* ExitingActor)
{
	if (!HasAuthority() || !IsRealmEncounterMode() || !ExitingActor || !IsExitOpen())
	{
		return false;
	}

	AActor* TravelActor = ResolveTravelActor(ExitingActor);
	if (!TravelActor)
	{
		return false;
	}

	const FTransform ReturnTransform = GetOverworldReturnTransform();
	PrepareActorForPortalTeleport(TravelActor);
	const bool bTeleported = TravelActor->TeleportTo(ReturnTransform.GetLocation(), ReturnTransform.Rotator(), false, true);
	if (!bTeleported)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to teleport %s out of the portal realm."), *GetNameSafe(this), *GetNameSafe(TravelActor));
		return false;
	}
	TravelActor->ForceNetUpdate();
	UnregisterRealmOccupant(TravelActor);

	AController* TravelController = ResolveTravelController(TravelActor);
	if (URpgPortalTravelComponent* TravelComponent = URpgPortalTravelComponent::FindPortalTravelComponent(TravelController))
	{
		TravelComponent->BeginPortalExit(this);
		KnownTravelComponents.RemoveSingleSwap(TravelComponent);
	}

	if (bRealmBossDefeated && RealmOccupants.IsEmpty() && PortalState != ERpgPortalState::Closed)
	{
		SetPortalState(ERpgPortalState::Sealable);
	}

	return true;
}

bool ARpgPortalActor::CompletePortalTravel(URpgPortalTravelComponent* TravelComponent, int32 RequestId)
{
	if (!HasAuthority() || !TravelComponent || !TravelComponent->IsActiveRequest(this, RequestId))
	{
		return false;
	}

	if (PortalState != ERpgPortalState::RealmInProgress && PortalState != ERpgPortalState::ExitOpen && PortalState != ERpgPortalState::Sealable)
	{
		TravelComponent->FailPortalTravel(this, RequestId, TEXT("Portal is no longer in a realm-enterable state."));
		return false;
	}

	AActor* TravelActor = TravelComponent->GetTravelActorForRequest(this, RequestId);
	if (!TravelActor)
	{
		TravelComponent->FailPortalTravel(this, RequestId, TEXT("Portal travel request no longer has a pawn/actor."));
		return false;
	}

	if (!TravelComponent->MarkTeleporting(this, RequestId))
	{
		return false;
	}

	if (!TeleportActorToRealm(TravelActor))
	{
		TravelComponent->FailPortalTravel(this, RequestId, TEXT("Server failed to teleport pawn into realm."));
		return false;
	}

	TravelComponent->MarkInsideRealm(this, RequestId);
	KnownTravelComponents.AddUnique(TravelComponent);
	return true;
}

void ARpgPortalActor::HandlePortalTravelFailed(URpgPortalTravelComponent* TravelComponent, int32 RequestId)
{
	if (!HasAuthority() || !TravelComponent)
	{
		return;
	}

	AActor* TravelActor = TravelComponent->GetTravelActorForRequest(this, RequestId);
	if (TravelActor)
	{
		PendingRealmEntrants.RemoveSingleSwap(TravelActor);
	}
}

bool ARpgPortalActor::TryRestoreReconnectController(AController* Controller)
{
	if (!HasAuthority() || !Controller || !IsRealmEncounterMode() || PortalState == ERpgPortalState::Closed)
	{
		return false;
	}

	const FUniqueNetIdRepl PlayerNetId = ResolvePlayerNetId(Controller);
	FRpgPortalRealmParticipantState* ParticipantState = FindParticipantState(PlayerNetId);
	if (!ParticipantState || !ParticipantState->bResumeAllowed)
	{
		return false;
	}

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	if (RealmOccupants.Contains(Pawn))
	{
		return true;
	}

	const bool bCanAttemptRealmResume = IsResumeStateUsable(*ParticipantState);
	if (!bCanAttemptRealmResume
		&& (PortalState == ERpgPortalState::RealmInProgress
			|| PortalState == ERpgPortalState::ExitOpen
			|| PortalState == ERpgPortalState::Sealable))
	{
		PrepareActorForPortalTeleport(Pawn);
		const FTransform SafeReturnTransform = GetOverworldReturnTransform();
		Pawn->TeleportTo(SafeReturnTransform.GetLocation(), SafeReturnTransform.Rotator(), false, true);
		Pawn->ForceNetUpdate();
		RemoveRealmPersistentEffects(*ParticipantState);
		ParticipantState->bInsideRealm = false;
		ParticipantState->bResumeAllowed = false;
		return true;
	}

	if (!bCanAttemptRealmResume)
	{
		return false;
	}

	PrepareActorForPortalTeleport(Pawn);
	const FTransform SafeReturnTransform = GetOverworldReturnTransform();
	if (!Pawn->TeleportTo(SafeReturnTransform.GetLocation(), SafeReturnTransform.Rotator(), false, true))
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to place reconnecting player %s at the overworld portal fallback before resume."),
			*GetNameSafe(this),
			*GetNameSafe(Pawn));
		return true;
	}
	Pawn->ForceNetUpdate();

	UE_LOG(LogRpgPortalActor, Log, TEXT("%s found live realm resume data for %s and is starting portal travel for instance %s."),
		*GetNameSafe(this),
		*GetNameSafe(Pawn),
		*RealmLevelInstanceName);

	if (!BeginPortalTravelForActor(Pawn))
	{
		RemoveRealmPersistentEffects(*ParticipantState);
		ParticipantState->bInsideRealm = false;
		ParticipantState->bResumeAllowed = false;
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s could not start resume travel for %s; player remains at overworld fallback."),
			*GetNameSafe(this),
			*GetNameSafe(Pawn));
	}

	return true;
}

FText ARpgPortalActor::GetExitInteractionText() const
{
	return EncounterDefinition ? EncounterDefinition->ExitInteractionText : NSLOCTEXT("RpgPortal", "ExitPortal", "Exit Portal");
}

FText ARpgPortalActor::GetExitInteractionSubText() const
{
	return EncounterDefinition ? EncounterDefinition->ExitInteractionSubText : NSLOCTEXT("RpgPortal", "ExitPortalSubText", "Return to the overworld");
}

float ARpgPortalActor::GetMaxStability() const
{
	return EncounterDefinition ? FMath::Max(1.0f, EncounterDefinition->MaxStability) : 3.0f;
}

int32 ARpgPortalActor::GetRemainingTrackedEnemyCount() const
{
	int32 RemainingCount = 0;
	for (const AActor* TrackedEnemy : TrackedEnemies)
	{
		if (TrackedEnemy)
		{
			++RemainingCount;
		}
	}

	return RemainingCount;
}

void ARpgPortalActor::OnRep_PortalState()
{
	OnPortalStateChanged.Broadcast(PortalState);

	if (PortalState == ERpgPortalState::Closed)
	{
		ApplyClosedPresentation();
	}
}

void ARpgPortalActor::OnRep_CurrentStability()
{
	OnPortalStabilityChanged.Broadcast(CurrentStability, GetMaxStability());
}

void ARpgPortalActor::OnRep_EncounterDefinition()
{
}

void ARpgPortalActor::OnRep_RealmLevelStreamingConfig()
{
}

void ARpgPortalActor::HandleTrackedEnemyDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !IsBrokenOutbreakMode() || PortalState != ERpgPortalState::Active)
	{
		return;
	}

	MarkTrackedEnemyDefeated(DestroyedActor);
}

void ARpgPortalActor::HandleRealmOccupantDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bWasRealmOccupant = RealmOccupants.Contains(DestroyedActor);
	if (bWasRealmOccupant)
	{
		const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(DestroyedActor);
		if (HealthComponent && HealthComponent->IsDeadOrDying())
		{
			MarkParticipantDiedInRealm(DestroyedActor);
		}
		else
		{
			MarkParticipantDisconnectedFromRealm(DestroyedActor);
		}
	}

	RealmOccupants.RemoveSingleSwap(DestroyedActor);
	RealmOccupantPlayerNetIds.Remove(FObjectKey(DestroyedActor));
	PendingRealmEntrants.RemoveSingleSwap(DestroyedActor);
	RefreshRealmOccupantCount();
	StopParticipantLocationSamplingIfIdle();

	for (URpgPortalTravelComponent* TravelComponent : KnownTravelComponents)
	{
		if (TravelComponent && TravelComponent->GetTravelActorForRequest(this, TravelComponent->GetActiveRequestId()) == DestroyedActor)
		{
			TravelComponent->CancelPortalTravel(this, TravelComponent->GetActiveRequestId(), TEXT("Realm traveller was destroyed."));
		}
	}
	KnownTravelComponents.RemoveAllSwap([](const URpgPortalTravelComponent* TravelComponent)
	{
		return TravelComponent == nullptr || !IsValid(TravelComponent);
	});

	if (bRealmBossDefeated && RealmOccupants.IsEmpty() && PortalState != ERpgPortalState::Closed)
	{
		SetPortalState(ERpgPortalState::Sealable);
	}
}

void ARpgPortalActor::HandleTrackedBossDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !IsRealmEncounterMode() || bRealmBossDefeated)
	{
		return;
	}

	MarkTrackedBossDefeated(DestroyedActor);
}

void ARpgPortalActor::HandleRealmLevelShown()
{
	if (!HasAuthority() || PortalState != ERpgPortalState::RealmLoading)
	{
		return;
	}

	if (!ResolveRealmMarkers())
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to resolve required realm markers and returned to Active state."), *GetNameSafe(this));
		PendingRealmEntrants.Reset();
		UnloadRealmLevelInstance();
		SetPortalState(ERpgPortalState::Active);
		return;
	}

	SetPortalState(ERpgPortalState::RealmInProgress);
	SpawnRealmEventDirector();
	if (RealmEventDirector)
	{
		RealmEventDirector->HandleRealmStarted(this, EncounterDefinition);
	}
	SpawnRealmBoss();
	TeleportPendingRealmEntrants();

	if (!TrackedBoss)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s has no realm boss and will open its exit immediately."), *GetNameSafe(this));
		MarkTrackedBossDefeated(nullptr);
	}
}

void ARpgPortalActor::HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message)
{
	if (!HasAuthority() || !Message.Victim)
	{
		return;
	}

	if (IsBrokenOutbreakMode() && PortalState == ERpgPortalState::Active)
	{
		MarkTrackedEnemyDefeated(Message.Victim);
		return;
	}

	if (IsRealmEncounterMode() && PortalState == ERpgPortalState::RealmInProgress && Message.Victim == TrackedBoss)
	{
		MarkTrackedBossDefeated(Message.Victim);
	}
}

void ARpgPortalActor::RegisterCombatMessageListener()
{
	if (ActorKilledListenerHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		ActorKilledListenerHandle = MessageSubsystem.RegisterListener<FRpgCombatActorKilledMessage>(
			FGameplayTag::RequestGameplayTag(TEXT("Rpg.Combat.Message.ActorKilled")),
			this,
			&ThisClass::HandleActorKilled);
	}
}

void ARpgPortalActor::UnregisterCombatMessageListener()
{
	if (ActorKilledListenerHandle.IsValid())
	{
		ActorKilledListenerHandle.Unregister();
	}
}

void ARpgPortalActor::SpawnEncounterEnemies()
{
	if (!EncounterDefinition)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn portal enemies without an EncounterDefinition."), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 RequestedEnemyCount = 0;
	for (const FRpgPortalEnemySpawnEntry& SpawnEntry : EncounterDefinition->EnemySpawnEntries)
	{
		if (SpawnEntry.EnemyClass)
		{
			RequestedEnemyCount += FMath::Max(0, SpawnEntry.Count);
		}
	}

	if (RequestedEnemyCount <= 0)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn portal enemies because EnemySpawnEntries is empty or invalid."), *GetNameSafe(this));
		return;
	}

	const float SpawnRadius = FMath::Max(0.0f, EncounterDefinition->SpawnRadius);
	int32 SpawnIndex = 0;
	for (const FRpgPortalEnemySpawnEntry& SpawnEntry : EncounterDefinition->EnemySpawnEntries)
	{
		TSubclassOf<AActor> EnemyClass = SpawnEntry.EnemyClass;
		const int32 EntryEnemyCount = FMath::Max(0, SpawnEntry.Count);
		if (!EnemyClass || EntryEnemyCount <= 0)
		{
			continue;
		}

		for (int32 EntryEnemyIndex = 0; EntryEnemyIndex < EntryEnemyCount; ++EntryEnemyIndex)
		{
			const float AngleRadians = 2.0f * PI * static_cast<float>(SpawnIndex) / static_cast<float>(RequestedEnemyCount);
			const FVector Offset(FMath::Cos(AngleRadians) * SpawnRadius, FMath::Sin(AngleRadians) * SpawnRadius, 0.0f);
			const FTransform SpawnTransform(GetActorRotation(), GetActorLocation() + Offset);

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			if (AActor* SpawnedEnemy = World->SpawnActor<AActor>(EnemyClass, SpawnTransform, SpawnParams))
			{
				SpawnedEnemy->OnDestroyed.AddDynamic(this, &ThisClass::HandleTrackedEnemyDestroyed);
				TrackedEnemies.Add(SpawnedEnemy);
				++TotalTrackedEnemyCount;
			}

			++SpawnIndex;
		}
	}
}

void ARpgPortalActor::StartRealmEncounter()
{
	if (!HasAuthority() || !IsRealmEncounterMode() || PortalState != ERpgPortalState::Active)
	{
		return;
	}

	EnsureRealmLevelStreamingConfig();
	SetPortalState(ERpgPortalState::RealmLoading);
	if (!LoadRealmLevelInstance())
	{
		SetPortalState(ERpgPortalState::Active);
	}
}

void ARpgPortalActor::EnsureRealmLevelStreamingConfig()
{
	if (!HasAuthority())
	{
		return;
	}

	if (RealmLevelInstanceTransform.Equals(FTransform::Identity))
	{
		RealmLevelInstanceTransform = GetDefaultRealmLevelInstanceTransform();
	}

	if (RealmLevelInstanceName.IsEmpty())
	{
		RealmLevelInstanceName = GetDefaultRealmLevelInstanceName();
	}
}

bool ARpgPortalActor::LoadRealmLevelInstance()
{
	if (!EncounterDefinition || EncounterDefinition->RealmLevel.IsNull())
	{
		return true;
	}

	if (RealmLevelStreaming)
	{
		return true;
	}

	if (HasAuthority())
	{
		EnsureRealmLevelStreamingConfig();
	}

	if (RealmLevelInstanceName.IsEmpty() || RealmLevelInstanceTransform.Equals(FTransform::Identity))
	{
		return false;
	}

	bool bLevelLoaded = false;
	RealmLevelStreaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		EncounterDefinition->RealmLevel,
		RealmLevelInstanceTransform,
		bLevelLoaded,
		RealmLevelInstanceName);

	if (!bLevelLoaded || !RealmLevelStreaming)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to stream realm level %s."), *GetNameSafe(this), *EncounterDefinition->RealmLevel.ToString());
		RealmLevelStreaming = nullptr;
		return false;
	}

	RealmLevelStreaming->OnLevelShown.AddDynamic(this, &ThisClass::HandleRealmLevelShown);
	if (const ULevel* LoadedLevel = RealmLevelStreaming->GetLoadedLevel())
	{
		if (LoadedLevel->bIsVisible)
		{
			HandleRealmLevelShown();
		}
	}

	return true;
}

void ARpgPortalActor::UnloadRealmLevelInstance()
{
	DestroyRealmEventDirector();

	if (!RealmLevelStreaming)
	{
		RealmLevelStreaming = nullptr;
		ClearRealmMarkers();
		return;
	}

	RealmLevelStreaming->OnLevelShown.RemoveDynamic(this, &ThisClass::HandleRealmLevelShown);
	RealmLevelStreaming->SetShouldBeVisible(false);
	RealmLevelStreaming->SetShouldBeLoaded(false);
	RealmLevelStreaming->SetIsRequestingUnloadAndRemoval(true);
	RealmLevelStreaming = nullptr;
	ClearRealmMarkers();
}

bool ARpgPortalActor::ResolveRealmMarkers()
{
	ClearRealmMarkers();

	ULevel* LoadedLevel = RealmLevelStreaming ? RealmLevelStreaming->GetLoadedLevel() : nullptr;
	if (!LoadedLevel)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot resolve realm markers because the realm level is not loaded."), *GetNameSafe(this));
		return false;
	}

	int32 EntryCount = 0;
	int32 BossSpawnCount = 0;
	int32 ExitPortalCount = 0;

	for (AActor* Actor : LoadedLevel->Actors)
	{
		ARpgPortalRealmMarkerActor* Marker = Cast<ARpgPortalRealmMarkerActor>(Actor);
		if (!Marker)
		{
			continue;
		}

		switch (Marker->GetMarkerRole())
		{
		case ERpgPortalRealmMarkerRole::Entry:
			RealmEntryMarker = Marker;
			++EntryCount;
			break;
		case ERpgPortalRealmMarkerRole::BossSpawn:
			RealmBossSpawnMarker = Marker;
			++BossSpawnCount;
			break;
		case ERpgPortalRealmMarkerRole::ExitPortal:
			RealmExitPortalMarker = Marker;
			++ExitPortalCount;
			break;
		default:
			break;
		}
	}

	const bool bHasExactlyOneMarkerOfEachRole = EntryCount == 1 && BossSpawnCount == 1 && ExitPortalCount == 1;
	if (!bHasExactlyOneMarkerOfEachRole)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s expected exactly one realm marker of each role but found Entry=%d, BossSpawn=%d, ExitPortal=%d."),
			*GetNameSafe(this),
			EntryCount,
			BossSpawnCount,
			ExitPortalCount);
		ClearRealmMarkers();
		return false;
	}

	return true;
}

void ARpgPortalActor::ClearRealmMarkers()
{
	RealmEntryMarker = nullptr;
	RealmBossSpawnMarker = nullptr;
	RealmExitPortalMarker = nullptr;
}

void ARpgPortalActor::TeleportPendingRealmEntrants()
{
	TArray<TObjectPtr<AActor>> PendingEntrants = MoveTemp(PendingRealmEntrants);
	PendingRealmEntrants.Reset();

	for (AActor* PendingEntrant : PendingEntrants)
	{
		if (IsValid(PendingEntrant))
		{
			BeginPortalTravelForActor(PendingEntrant);
		}
	}
}

bool ARpgPortalActor::BeginPortalTravelForActor(AActor* TravelActor)
{
	if (!HasAuthority() || !TravelActor || !EncounterDefinition || EncounterDefinition->RealmLevel.IsNull())
	{
		return false;
	}

	if (!RealmEntryMarker)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot begin portal travel for %s because the Entry marker is not resolved."), *GetNameSafe(this), *GetNameSafe(TravelActor));
		return false;
	}

	AController* TravelController = ResolveTravelController(TravelActor);
	if (!TravelController)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot begin portal travel for %s because no controller was resolved."), *GetNameSafe(this), *GetNameSafe(TravelActor));
		return false;
	}

	URpgPortalTravelComponent* TravelComponent = URpgPortalTravelComponent::FindPortalTravelComponent(TravelController);
	if (!TravelComponent)
	{
		if (TravelController->HasAuthority() && TravelController->IsLocalController())
		{
			return TeleportActorToRealm(TravelActor);
		}

		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot begin portal travel for %s because %s has no RpgPortalTravelComponent."),
			*GetNameSafe(this),
			*GetNameSafe(TravelActor),
			*GetNameSafe(TravelController));
		return false;
	}

	const FName ExpectedPackageName = GetRealmLevelNetPackageName();
	if (ExpectedPackageName.IsNone())
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot begin portal travel for %s because the realm package name is unknown."),
			*GetNameSafe(this),
			*GetNameSafe(TravelActor));
		return false;
	}

	KnownTravelComponents.AddUnique(TravelComponent);
	return TravelComponent->BeginPortalTravel(
		this,
		TravelActor,
		EncounterDefinition->RealmLevel.ToSoftObjectPath(),
		RealmLevelInstanceTransform,
		RealmLevelInstanceName,
		ExpectedPackageName,
		++NextPortalTravelRequestId);
}

bool ARpgPortalActor::TeleportActorToRealm(AActor* TravelActor)
{
	if (!TravelActor || !RealmEntryMarker)
	{
		return false;
	}

	FTransform DestinationTransform = RealmEntryMarker->GetMarkerTransform();
	const bool bUsingResumeTransform = GetResumeRealmTransformForActor(TravelActor, DestinationTransform);
	PrepareActorForPortalTeleport(TravelActor);
	const bool bTeleported = TravelActor->TeleportTo(DestinationTransform.GetLocation(), DestinationTransform.Rotator(), false, true);
	if (!bTeleported)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to teleport %s into the portal realm."), *GetNameSafe(this), *GetNameSafe(TravelActor));
		return false;
	}

	if (bUsingResumeTransform)
	{
		UE_LOG(LogRpgPortalActor, Log, TEXT("%s restored %s to its last safe realm transform for instance %s."),
			*GetNameSafe(this),
			*GetNameSafe(TravelActor),
			*RealmLevelInstanceName);
	}

	RegisterRealmOccupant(TravelActor);
	TravelActor->ForceNetUpdate();
	return true;
}

void ARpgPortalActor::PrepareActorForPortalTeleport(AActor* TravelActor) const
{
	if (ACharacter* Character = Cast<ACharacter>(TravelActor))
	{
		Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr), NAME_None, false);

		if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
			MovementComponent->CurrentFloor.Clear();
			MovementComponent->bJustTeleported = true;
		}
	}
}

void ARpgPortalActor::RegisterRealmOccupant(AActor* TravelActor)
{
	if (!TravelActor || RealmOccupants.Contains(TravelActor))
	{
		return;
	}

	TravelActor->OnDestroyed.AddDynamic(this, &ThisClass::HandleRealmOccupantDestroyed);
	RealmOccupants.Add(TravelActor);
	MarkParticipantEnteredRealm(TravelActor);
	const FUniqueNetIdRepl PlayerNetId = ResolvePlayerNetId(TravelActor);
	if (PlayerNetId.IsValid())
	{
		RealmOccupantPlayerNetIds.Add(FObjectKey(TravelActor), PlayerNetId);
	}
	RefreshRealmOccupantCount();
	StartParticipantLocationSamplingIfNeeded();
}

void ARpgPortalActor::UnregisterRealmOccupant(AActor* TravelActor)
{
	if (!TravelActor)
	{
		return;
	}

	TravelActor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleRealmOccupantDestroyed);
	MarkParticipantExitedRealm(TravelActor);
	RealmOccupantPlayerNetIds.Remove(FObjectKey(TravelActor));
	RealmOccupants.RemoveSingleSwap(TravelActor);
	RefreshRealmOccupantCount();
	StopParticipantLocationSamplingIfIdle();
}

AController* ARpgPortalActor::ResolveTravelController(AActor* TravelActor) const
{
	if (!TravelActor)
	{
		return nullptr;
	}

	if (const APawn* TravelPawn = Cast<APawn>(TravelActor))
	{
		return TravelPawn->GetController();
	}

	return Cast<AController>(TravelActor);
}

FUniqueNetIdRepl ARpgPortalActor::ResolvePlayerNetId(AActor* TravelActor) const
{
	if (!TravelActor)
	{
		return FUniqueNetIdRepl();
	}

	if (AController* Controller = ResolveTravelController(TravelActor))
	{
		const FUniqueNetIdRepl ControllerPlayerNetId = ResolvePlayerNetId(Controller);
		if (ControllerPlayerNetId.IsValid())
		{
			return ControllerPlayerNetId;
		}
	}

	if (const APawn* TravelPawn = Cast<APawn>(TravelActor))
	{
		if (const APlayerState* PlayerState = TravelPawn->GetPlayerState())
		{
			return PlayerState->GetUniqueId();
		}
	}

	if (const FUniqueNetIdRepl* CachedPlayerNetId = RealmOccupantPlayerNetIds.Find(FObjectKey(TravelActor)))
	{
		return *CachedPlayerNetId;
	}

	return FUniqueNetIdRepl();
}

FUniqueNetIdRepl ARpgPortalActor::ResolvePlayerNetId(AController* Controller) const
{
	if (Controller)
	{
		if (const APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>())
		{
			return PlayerState->GetUniqueId();
		}
	}

	return FUniqueNetIdRepl();
}

FRpgPortalRealmParticipantState* ARpgPortalActor::FindParticipantState(const FUniqueNetIdRepl& PlayerNetId)
{
	return PlayerNetId.IsValid() ? RealmParticipantStates.Find(PlayerNetId) : nullptr;
}

const FRpgPortalRealmParticipantState* ARpgPortalActor::FindParticipantState(const FUniqueNetIdRepl& PlayerNetId) const
{
	return PlayerNetId.IsValid() ? RealmParticipantStates.Find(PlayerNetId) : nullptr;
}

FRpgPortalRealmParticipantState* ARpgPortalActor::FindParticipantStateForActor(AActor* TravelActor)
{
	return FindParticipantState(ResolvePlayerNetId(TravelActor));
}

FRpgPortalRealmParticipantState* ARpgPortalActor::FindOrAddParticipantStateForActor(AActor* TravelActor)
{
	const FUniqueNetIdRepl PlayerNetId = ResolvePlayerNetId(TravelActor);
	if (!PlayerNetId.IsValid())
	{
		return nullptr;
	}

	FRpgPortalRealmParticipantState& ParticipantState = RealmParticipantStates.FindOrAdd(PlayerNetId);
	ParticipantState.PlayerNetId = PlayerNetId;
	return &ParticipantState;
}

bool ARpgPortalActor::IsResumeStateUsable(const FRpgPortalRealmParticipantState& ParticipantState) const
{
	const bool bPortalCanStillHostRealmTravel =
		PortalState == ERpgPortalState::RealmInProgress
		|| PortalState == ERpgPortalState::ExitOpen
		|| PortalState == ERpgPortalState::Sealable;

	return IsRealmEncounterMode()
		&& bPortalCanStillHostRealmTravel
		&& RealmEntryMarker != nullptr
		&& ParticipantState.PlayerNetId.IsValid()
		&& ParticipantState.bResumeAllowed
		&& ParticipantState.bHasLastSafeRealmTransform
		&& !ParticipantState.LastSafeRealmTransform.ContainsNaN();
}

bool ARpgPortalActor::GetResumeRealmTransformForActor(AActor* TravelActor, FTransform& OutTransform) const
{
	const FUniqueNetIdRepl PlayerNetId = ResolvePlayerNetId(TravelActor);
	const FRpgPortalRealmParticipantState* ParticipantState = FindParticipantState(PlayerNetId);
	if (!ParticipantState || !IsResumeStateUsable(*ParticipantState))
	{
		return false;
	}

	OutTransform = ParticipantState->LastSafeRealmTransform;
	return true;
}

void ARpgPortalActor::UpdateParticipantSafeTransform(AActor* TravelActor)
{
	if (!TravelActor)
	{
		return;
	}

	FRpgPortalRealmParticipantState* ParticipantState = FindOrAddParticipantStateForActor(TravelActor);
	if (!ParticipantState)
	{
		return;
	}

	const FTransform ActorTransform = TravelActor->GetActorTransform();
	if (ActorTransform.ContainsNaN())
	{
		return;
	}

	ParticipantState->LastSafeRealmTransform = ActorTransform;
	ParticipantState->bHasLastSafeRealmTransform = true;
}

void ARpgPortalActor::MarkParticipantEnteredRealm(AActor* TravelActor)
{
	FRpgPortalRealmParticipantState* ParticipantState = FindOrAddParticipantStateForActor(TravelActor);
	if (!ParticipantState)
	{
		return;
	}

	ParticipantState->bInsideRealm = true;
	ParticipantState->bResumeAllowed = true;
	UpdateParticipantSafeTransform(TravelActor);
	ApplyRealmEnterEffects(TravelActor, *ParticipantState);

	if (RealmEventDirector)
	{
		RealmEventDirector->HandleParticipantEntered(this, TravelActor);
	}
}

void ARpgPortalActor::MarkParticipantExitedRealm(AActor* TravelActor)
{
	FRpgPortalRealmParticipantState* ParticipantState = FindParticipantStateForActor(TravelActor);
	if (!ParticipantState)
	{
		return;
	}

	UpdateParticipantSafeTransform(TravelActor);
	RemoveRealmPersistentEffects(*ParticipantState);
	ApplyRealmExitEffects(TravelActor);
	ParticipantState->bInsideRealm = false;
	ParticipantState->bResumeAllowed = false;

	if (RealmEventDirector)
	{
		RealmEventDirector->HandleParticipantExited(this, TravelActor);
	}
}

void ARpgPortalActor::MarkParticipantDiedInRealm(AActor* TravelActor)
{
	FRpgPortalRealmParticipantState* ParticipantState = FindParticipantStateForActor(TravelActor);
	if (!ParticipantState)
	{
		return;
	}

	UpdateParticipantSafeTransform(TravelActor);
	RemoveRealmPersistentEffects(*ParticipantState);
	ParticipantState->bInsideRealm = false;
	ParticipantState->bResumeAllowed = false;
}

void ARpgPortalActor::MarkParticipantDisconnectedFromRealm(AActor* TravelActor)
{
	FRpgPortalRealmParticipantState* ParticipantState = FindOrAddParticipantStateForActor(TravelActor);
	if (!ParticipantState)
	{
		return;
	}

	UpdateParticipantSafeTransform(TravelActor);
	RemoveRealmPersistentEffects(*ParticipantState);
	ParticipantState->bInsideRealm = false;
	ParticipantState->bResumeAllowed = ParticipantState->bHasLastSafeRealmTransform;
}

void ARpgPortalActor::InvalidateParticipantResumeStates()
{
	for (TPair<FUniqueNetIdRepl, FRpgPortalRealmParticipantState>& ParticipantPair : RealmParticipantStates)
	{
		ParticipantPair.Value.bInsideRealm = false;
		ParticipantPair.Value.bResumeAllowed = false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParticipantLocationSampleTimerHandle);
	}
}

void ARpgPortalActor::ApplyRealmEnterEffects(AActor* TravelActor, FRpgPortalRealmParticipantState& ParticipantState)
{
	if (!EncounterDefinition || !TravelActor)
	{
		return;
	}

	RemoveRealmPersistentEffects(ParticipantState);
	ParticipantState.RealmAbilitySystem = ResolveAbilitySystemComponent(TravelActor);

	if (!ParticipantState.RealmAbilitySystem.IsValid())
	{
		return;
	}

	ParticipantState.ActiveRealmTagEffectHandle = ApplyRealmTagsToActor(TravelActor);
	ApplyGameplayEffectsToActor(TravelActor, EncounterDefinition->EffectsOnEnter);
	ApplyGameplayEffectsToActor(TravelActor, EncounterDefinition->EffectsWhileInside, &ParticipantState.ActiveRealmEffectHandles);
}

void ARpgPortalActor::ApplyRealmExitEffects(AActor* TravelActor) const
{
	if (!EncounterDefinition || !TravelActor)
	{
		return;
	}

	ApplyGameplayEffectsToActor(TravelActor, EncounterDefinition->EffectsOnExit);
}

void ARpgPortalActor::RemoveRealmPersistentEffects(FRpgPortalRealmParticipantState& ParticipantState)
{
	UAbilitySystemComponent* AbilitySystemComponent = ParticipantState.RealmAbilitySystem.Get();
	if (AbilitySystemComponent)
	{
		if (ParticipantState.ActiveRealmTagEffectHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(ParticipantState.ActiveRealmTagEffectHandle);
		}

		for (const FActiveGameplayEffectHandle& EffectHandle : ParticipantState.ActiveRealmEffectHandles)
		{
			if (EffectHandle.IsValid())
			{
				AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
			}
		}
	}

	ParticipantState.ActiveRealmTagEffectHandle = FActiveGameplayEffectHandle();
	ParticipantState.ActiveRealmEffectHandles.Reset();
	ParticipantState.RealmAbilitySystem.Reset();
}

void ARpgPortalActor::RemoveAllRealmPersistentEffects()
{
	for (TPair<FUniqueNetIdRepl, FRpgPortalRealmParticipantState>& ParticipantPair : RealmParticipantStates)
	{
		RemoveRealmPersistentEffects(ParticipantPair.Value);
	}
}

void ARpgPortalActor::ApplyGameplayEffectsToActor(AActor* TargetActor, const TArray<TSubclassOf<UGameplayEffect>>& Effects, TArray<FActiveGameplayEffectHandle>* OutHandles) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent(TargetActor);
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : Effects)
	{
		const UGameplayEffect* GameplayEffect = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!GameplayEffect)
		{
			continue;
		}

		const FActiveGameplayEffectHandle EffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(GameplayEffect, 1.0f, AbilitySystemComponent->MakeEffectContext());
		if (OutHandles && EffectHandle.IsValid())
		{
			OutHandles->Add(EffectHandle);
		}
	}
}

FActiveGameplayEffectHandle ARpgPortalActor::ApplyRealmTagsToActor(AActor* TargetActor) const
{
	if (!EncounterDefinition || EncounterDefinition->RealmTags.IsEmpty())
	{
		return FActiveGameplayEffectHandle();
	}

	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent(TargetActor);
	if (!AbilitySystemComponent)
	{
		return FActiveGameplayEffectHandle();
	}

	const TSubclassOf<UGameplayEffect> DynamicTagGameplayEffect = URpgAssetManager::GetSubclass(URpgGameData::Get().DynamicTagGameplayEffect);
	if (!DynamicTagGameplayEffect)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot apply realm tags because DynamicTagGameplayEffect is not configured."), *GetNameSafe(this));
		return FActiveGameplayEffectHandle();
	}

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DynamicTagGameplayEffect, 1.0f, AbilitySystemComponent->MakeEffectContext());
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return FActiveGameplayEffectHandle();
	}

	Spec->DynamicGrantedTags.AppendTags(EncounterDefinition->RealmTags);
	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
}

UAbilitySystemComponent* ARpgPortalActor::ResolveAbilitySystemComponent(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return nullptr;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
	{
		return AbilitySystemComponent;
	}

	if (const APawn* Pawn = Cast<APawn>(TargetActor))
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn->GetPlayerState());
	}

	if (const AController* Controller = Cast<AController>(TargetActor))
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller->PlayerState);
	}

	return nullptr;
}

void ARpgPortalActor::StartParticipantLocationSamplingIfNeeded()
{
	if (!HasAuthority() || RealmOccupants.IsEmpty())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(ParticipantLocationSampleTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				ParticipantLocationSampleTimerHandle,
				this,
				&ThisClass::SampleRealmParticipantLocations,
				PortalParticipantSafeTransformSampleInterval,
				true);
		}
	}
}

void ARpgPortalActor::StopParticipantLocationSamplingIfIdle()
{
	if (!RealmOccupants.IsEmpty())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParticipantLocationSampleTimerHandle);
	}
}

void ARpgPortalActor::SampleRealmParticipantLocations()
{
	if (!HasAuthority())
	{
		return;
	}

	RefreshRealmOccupantCount();
	for (AActor* RealmOccupant : RealmOccupants)
	{
		UpdateParticipantSafeTransform(RealmOccupant);
	}

	StopParticipantLocationSamplingIfIdle();
}

void ARpgPortalActor::NotifyKnownTravelComponentsToUnload(ERpgPortalTravelState TerminalState)
{
	TArray<TObjectPtr<URpgPortalTravelComponent>> TravelComponents = MoveTemp(KnownTravelComponents);
	KnownTravelComponents.Reset();

	for (URpgPortalTravelComponent* TravelComponent : TravelComponents)
	{
		if (!TravelComponent || !IsValid(TravelComponent))
		{
			continue;
		}

		const int32 RequestId = TravelComponent->GetActiveRequestId();
		if (TravelComponent->IsActiveRequest(this, RequestId))
		{
			if (TerminalState == ERpgPortalTravelState::Failed)
			{
				TravelComponent->FailPortalTravel(this, RequestId, TEXT("Portal failed its realm flow."));
			}
			else
			{
				TravelComponent->CancelPortalTravel(this, RequestId, TEXT("Portal unloaded or closed its realm instance."));
			}
		}
	}
}

void ARpgPortalActor::SpawnRealmBoss()
{
	if (!EncounterDefinition)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn a realm boss without an EncounterDefinition."), *GetNameSafe(this));
		return;
	}

	if (!EncounterDefinition->BossClass)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn a realm boss because BossClass is unset."), *GetNameSafe(this));
		return;
	}

	if (!RealmBossSpawnMarker)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn a realm boss because the BossSpawn marker is missing."), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.OverrideLevel = RealmBossSpawnMarker->GetLevel();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	TrackedBoss = World->SpawnActor<AActor>(EncounterDefinition->BossClass, RealmBossSpawnMarker->GetMarkerTransform(), SpawnParams);
	if (TrackedBoss)
	{
		TrackedBoss->OnDestroyed.AddDynamic(this, &ThisClass::HandleTrackedBossDestroyed);
	}
}

void ARpgPortalActor::SpawnRealmEventDirector()
{
	if (RealmEventDirector || !EncounterDefinition || !EncounterDefinition->RealmEventDirectorClass)
	{
		return;
	}

	if (!RealmEntryMarker)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn a realm event director because the Entry marker is missing."), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.OverrideLevel = RealmEntryMarker->GetLevel();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RealmEventDirector = World->SpawnActor<ARpgPortalRealmEventDirector>(
		EncounterDefinition->RealmEventDirectorClass,
		RealmEntryMarker->GetMarkerTransform(),
		SpawnParams);
}

void ARpgPortalActor::DestroyRealmEventDirector()
{
	if (!HasAuthority())
	{
		RealmEventDirector = nullptr;
		return;
	}

	if (RealmEventDirector && IsValid(RealmEventDirector))
	{
		RealmEventDirector->Destroy();
	}

	RealmEventDirector = nullptr;
}

void ARpgPortalActor::SpawnExitPortal()
{
	TSubclassOf<ARpgPortalExitActor> ExitClass = EncounterDefinition && EncounterDefinition->ExitPortalActorClass
		? EncounterDefinition->ExitPortalActorClass
		: ExitPortalActorClass;

	if (ExitPortalActor || !ExitClass)
	{
		return;
	}

	if (!RealmExitPortalMarker)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn its exit portal because the ExitPortal marker is missing."), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform ExitTransform = RealmExitPortalMarker->GetMarkerTransform();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.OverrideLevel = RealmExitPortalMarker->GetLevel();
	SpawnParams.bDeferConstruction = true;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARpgPortalExitActor* SpawnedExit = World->SpawnActor<ARpgPortalExitActor>(ExitClass, ExitTransform, SpawnParams);

	if (!SpawnedExit)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to spawn its exit portal."), *GetNameSafe(this));
		return;
	}

	SpawnedExit->ConfigureExitPortal(this);
	UGameplayStatics::FinishSpawningActor(SpawnedExit, ExitTransform);
	ExitPortalActor = SpawnedExit;

	if (RealmEventDirector)
	{
		RealmEventDirector->HandleExitOpened(this, ExitPortalActor);
	}
}

void ARpgPortalActor::DestroyExitPortal()
{
	if (!HasAuthority())
	{
		ExitPortalActor = nullptr;
		return;
	}

	if (ExitPortalActor && IsValid(ExitPortalActor))
	{
		ExitPortalActor->Destroy();
	}

	ExitPortalActor = nullptr;
}

void ARpgPortalActor::MarkTrackedEnemyDefeated(AActor* DefeatedEnemy)
{
	if (!DefeatedEnemy)
	{
		return;
	}

	const int32 RemovedCount = TrackedEnemies.RemoveSingleSwap(DefeatedEnemy);
	if (RemovedCount <= 0)
	{
		return;
	}

	DefeatedEnemy->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrackedEnemyDestroyed);
	++DefeatedTrackedEnemyCount;
	RefreshStabilityFromProgress();

	if (TrackedEnemies.IsEmpty())
	{
		CurrentStability = GetMaxStability();
		SetPortalState(ERpgPortalState::Sealable);
	}
}

void ARpgPortalActor::MarkTrackedBossDefeated(AActor* DefeatedBoss)
{
	if (bRealmBossDefeated)
	{
		return;
	}

	if (TrackedBoss)
	{
		TrackedBoss->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrackedBossDestroyed);
	}

	TrackedBoss = nullptr;
	bRealmBossDefeated = true;
	CurrentStability = GetMaxStability();
	OnPortalStabilityChanged.Broadcast(CurrentStability, GetMaxStability());

	if (RealmEventDirector)
	{
		RealmEventDirector->HandleBossDefeated(this, DefeatedBoss);
	}

	SpawnExitPortal();
	SetPortalState(RealmOccupants.IsEmpty() ? ERpgPortalState::Sealable : ERpgPortalState::ExitOpen);
}

void ARpgPortalActor::RefreshStabilityFromProgress()
{
	const int32 TrackedEnemyCount = FMath::Max(TotalTrackedEnemyCount, 1);
	const float DefeatAlpha = FMath::Clamp(static_cast<float>(DefeatedTrackedEnemyCount) / static_cast<float>(TrackedEnemyCount), 0.0f, 1.0f);
	CurrentStability = GetMaxStability() * DefeatAlpha;
	OnPortalStabilityChanged.Broadcast(CurrentStability, GetMaxStability());
}

void ARpgPortalActor::RefreshRealmOccupantCount()
{
	RealmOccupants.RemoveAllSwap([](const AActor* Actor)
	{
		return Actor == nullptr || !IsValid(Actor);
	});

	RealmOccupantCount = RealmOccupants.Num();
}

void ARpgPortalActor::SetPortalState(ERpgPortalState NewState)
{
	if (PortalState == NewState)
	{
		return;
	}

	PortalState = NewState;
	OnPortalStateChanged.Broadcast(PortalState);
}

void ARpgPortalActor::ApplyClosedPresentation()
{
	if (InteractionCollision)
	{
		InteractionCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (PortalMesh)
	{
		PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PortalMesh->SetHiddenInGame(true);
	}
}

AActor* ARpgPortalActor::ResolveTravelActor(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn;
	}

	if (AController* Controller = Cast<AController>(Actor))
	{
		return Controller->GetPawn();
	}

	return Actor;
}

FTransform ARpgPortalActor::GetDefaultRealmLevelInstanceTransform() const
{
	constexpr double RegionGridSize = 100000.0;
	const FVector PortalLocation = GetActorLocation();
	const FVector RegionGridOrigin(
		FMath::GridSnap(PortalLocation.X, RegionGridSize),
		FMath::GridSnap(PortalLocation.Y, RegionGridSize),
		0.0);

	return FTransform(FRotator::ZeroRotator, FVector(1000000.0, 0.0, 0.0) + RegionGridOrigin);
}

FString ARpgPortalActor::GetDefaultRealmLevelInstanceName() const
{
	return FString::Printf(TEXT("PortalRealm_%s"), *GetFName().ToString());
}

FName ARpgPortalActor::GetRealmLevelNetPackageName() const
{
	if (RealmLevelStreaming)
	{
		if (const ULevel* LoadedLevel = RealmLevelStreaming->GetLoadedLevel())
		{
			if (const UPackage* Package = LoadedLevel->GetOutermost())
			{
				return Package->GetFName();
			}
		}
	}

	return NAME_None;
}

FTransform ARpgPortalActor::GetOverworldReturnTransform() const
{
	return FTransform(GetActorRotation(), GetActorLocation() + GetActorForwardVector() * OverworldReturnDistance);
}

bool ARpgPortalActor::IsTrackedEnemy(AActor* Actor) const
{
	return Actor && TrackedEnemies.Contains(Actor);
}

bool ARpgPortalActor::IsRealmEncounterMode() const
{
	return !EncounterDefinition || EncounterDefinition->EncounterMode == ERpgPortalEncounterMode::Realm;
}

bool ARpgPortalActor::IsBrokenOutbreakMode() const
{
	return EncounterDefinition && EncounterDefinition->EncounterMode == ERpgPortalEncounterMode::BrokenOutbreak;
}

bool ARpgPortalActor::ShouldRewardsBeEligible() const
{
	const bool bRewardsRequireBossDefeat = EncounterDefinition ? EncounterDefinition->bRewardsRequireBossDefeat : true;
	return !bRewardsRequireBossDefeat || bRealmBossDefeated;
}
