#include "Portals/RpgPortalActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/Abilities/RpgGameplayAbility_ClosePortal.h"
#include "AbilitySystem/Abilities/RpgGameplayAbility_EnterPortal.h"
#include "SurvivalRpg/Combat/RpgCombatMessages.h"
#include "GameplayTags/RpgPortalGameplayTags.h"
#include "Portals/RpgPortalDungeonMarkerActor.h"
#include "Portals/RpgPortalEncounterDefinition.h"
#include "Portals/RpgPortalExitActor.h"
#include "Portals/RpgPortalMessages.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalActor)

DEFINE_LOG_CATEGORY_STATIC(LogRpgPortalActor, Log, All);

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

	for (AActor* DungeonOccupant : DungeonOccupants)
	{
		if (DungeonOccupant)
		{
			DungeonOccupant->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleDungeonOccupantDestroyed);
		}
	}
	DungeonOccupants.Reset();
	PendingDungeonEntrants.Reset();

	DestroyExitPortal();
	UnloadDungeonLevelInstance();

	Super::EndPlay(EndPlayReason);
}

void ARpgPortalActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PortalState);
	DOREPLIFETIME(ThisClass, CurrentStability);
	DOREPLIFETIME(ThisClass, DefeatedTrackedEnemyCount);
	DOREPLIFETIME(ThisClass, TotalTrackedEnemyCount);
	DOREPLIFETIME(ThisClass, bDungeonBossDefeated);
	DOREPLIFETIME(ThisClass, DungeonOccupantCount);
	DOREPLIFETIME(ThisClass, EncounterDefinition);
	DOREPLIFETIME(ThisClass, DungeonLevelInstanceTransform);
	DOREPLIFETIME(ThisClass, DungeonLevelInstanceName);
}

void ARpgPortalActor::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (IsDungeonEncounterMode() && EnterPortalAbilityClass
		&& (PortalState == ERpgPortalState::Active || PortalState == ERpgPortalState::DungeonLoading || PortalState == ERpgPortalState::DungeonInProgress))
	{
		FInteractionOption Option;
		Option.InteractionAbilityToGrant = EnterPortalAbilityClass;
		Option.Text = EncounterDefinition ? EncounterDefinition->EnterInteractionText : NSLOCTEXT("RpgPortal", "EnterPortal", "Enter Portal");
		Option.SubText = EncounterDefinition ? EncounterDefinition->EnterInteractionSubText : NSLOCTEXT("RpgPortal", "EnterPortalSubText", "Cross into the rift");

		InteractionBuilder.AddInteractionOption(Option);
		return;
	}

	if (PortalState != ERpgPortalState::Sealable || !ClosePortalAbilityClass)
	{
		return;
	}

	FInteractionOption Option;
	Option.InteractionAbilityToGrant = ClosePortalAbilityClass;
	Option.Text = EncounterDefinition ? EncounterDefinition->CloseInteractionText : NSLOCTEXT("RpgPortal", "ClosePortal", "Close Portal");
	Option.SubText = EncounterDefinition ? EncounterDefinition->CloseInteractionSubText : NSLOCTEXT("RpgPortal", "ClosePortalSubText", "Stabilize the rift");

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
	bDungeonBossDefeated = false;
	DungeonOccupantCount = 0;
	TrackedEnemies.Reset();
	DungeonOccupants.Reset();
	PendingDungeonEntrants.Reset();
	TrackedBoss = nullptr;
	ClearDungeonMarkers();

	if (IsDungeonEncounterMode())
	{
		EnsureDungeonLevelStreamingConfig();
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

	if (PortalState != ERpgPortalState::Dormant || TotalTrackedEnemyCount > 0 || DefeatedTrackedEnemyCount > 0 || !TrackedEnemies.IsEmpty() || TrackedBoss || !DungeonOccupants.IsEmpty())
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s ignored encounter definition reconfiguration after the encounter started."), *GetNameSafe(this));
		return;
	}

	EncounterDefinition = InEncounterDefinition;
}

void ARpgPortalActor::ConfigureDungeonLevelInstanceTransform(const FTransform& InDungeonLevelInstanceTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	if (PortalState != ERpgPortalState::Dormant)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s ignored dungeon level instance transform configuration after the encounter started."), *GetNameSafe(this));
		return;
	}

	DungeonLevelInstanceTransform = InDungeonLevelInstanceTransform;
	if (DungeonLevelInstanceName.IsEmpty())
	{
		DungeonLevelInstanceName = GetDefaultDungeonLevelInstanceName();
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
	DestroyExitPortal();
	UnloadDungeonLevelInstance();

	FRpgPortalCompletedMessage Message;
	Message.Portal = this;
	Message.Instigator = ClosingActor;
	Message.EncounterDefinition = EncounterDefinition;
	Message.CompletionTags = EncounterDefinition ? EncounterDefinition->CompletionTags : FGameplayTagContainer();
	Message.FinalStability = CurrentStability;
	Message.bRewardsRequireBossDefeat = EncounterDefinition ? EncounterDefinition->bRewardsRequireBossDefeat : true;
	Message.bBossDefeated = bDungeonBossDefeated;
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
	if (!HasAuthority() || !IsDungeonEncounterMode() || !EnteringActor)
	{
		return false;
	}

	if (PortalState != ERpgPortalState::Active && PortalState != ERpgPortalState::DungeonLoading && PortalState != ERpgPortalState::DungeonInProgress)
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
		StartDungeonEncounter();
	}

	if (PortalState == ERpgPortalState::DungeonLoading)
	{
		PendingDungeonEntrants.AddUnique(TravelActor);
		return true;
	}

	if (PortalState == ERpgPortalState::DungeonInProgress)
	{
		return TeleportActorToDungeon(TravelActor);
	}

	return false;
}

bool ARpgPortalActor::TryExitPortal(AActor* ExitingActor)
{
	if (!HasAuthority() || !IsDungeonEncounterMode() || !ExitingActor || !IsExitOpen())
	{
		return false;
	}

	AActor* TravelActor = ResolveTravelActor(ExitingActor);
	if (!TravelActor)
	{
		return false;
	}

	TravelActor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleDungeonOccupantDestroyed);
	DungeonOccupants.RemoveSingleSwap(TravelActor);
	RefreshDungeonOccupantCount();

	const FTransform ReturnTransform = GetOverworldReturnTransform();
	const bool bTeleported = TravelActor->TeleportTo(ReturnTransform.GetLocation(), ReturnTransform.Rotator(), false, true);
	if (!bTeleported)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to teleport %s out of the portal dungeon."), *GetNameSafe(this), *GetNameSafe(TravelActor));
		return false;
	}

	if (bDungeonBossDefeated && DungeonOccupants.IsEmpty() && PortalState != ERpgPortalState::Closed)
	{
		SetPortalState(ERpgPortalState::Sealable);
		DestroyExitPortal();
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

	if (IsDungeonEncounterMode())
	{
		if (ShouldDungeonLevelBeLoadedForState())
		{
			LoadDungeonLevelInstance();
		}
		else if (PortalState == ERpgPortalState::Closed)
		{
			UnloadDungeonLevelInstance();
		}
	}

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
	if (IsDungeonEncounterMode() && ShouldDungeonLevelBeLoadedForState())
	{
		LoadDungeonLevelInstance();
	}
}

void ARpgPortalActor::OnRep_DungeonLevelStreamingConfig()
{
	if (IsDungeonEncounterMode() && ShouldDungeonLevelBeLoadedForState())
	{
		LoadDungeonLevelInstance();
	}
}

void ARpgPortalActor::HandleTrackedEnemyDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !IsBrokenOutbreakMode() || PortalState != ERpgPortalState::Active)
	{
		return;
	}

	MarkTrackedEnemyDefeated(DestroyedActor);
}

void ARpgPortalActor::HandleDungeonOccupantDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority())
	{
		return;
	}

	DungeonOccupants.RemoveSingleSwap(DestroyedActor);
	PendingDungeonEntrants.RemoveSingleSwap(DestroyedActor);
	RefreshDungeonOccupantCount();
}

void ARpgPortalActor::HandleTrackedBossDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !IsDungeonEncounterMode() || bDungeonBossDefeated)
	{
		return;
	}

	MarkTrackedBossDefeated(DestroyedActor);
}

void ARpgPortalActor::HandleDungeonLevelShown()
{
	if (!HasAuthority() || PortalState != ERpgPortalState::DungeonLoading)
	{
		return;
	}

	if (!ResolveDungeonMarkers())
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to resolve required dungeon markers and returned to Active state."), *GetNameSafe(this));
		PendingDungeonEntrants.Reset();
		UnloadDungeonLevelInstance();
		SetPortalState(ERpgPortalState::Active);
		return;
	}

	SetPortalState(ERpgPortalState::DungeonInProgress);
	SpawnDungeonBoss();
	TeleportPendingDungeonEntrants();

	if (!TrackedBoss)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s has no dungeon boss and will open its exit immediately."), *GetNameSafe(this));
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

	if (IsDungeonEncounterMode() && PortalState == ERpgPortalState::DungeonInProgress && Message.Victim == TrackedBoss)
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

void ARpgPortalActor::StartDungeonEncounter()
{
	if (!HasAuthority() || !IsDungeonEncounterMode() || PortalState != ERpgPortalState::Active)
	{
		return;
	}

	EnsureDungeonLevelStreamingConfig();
	SetPortalState(ERpgPortalState::DungeonLoading);
	if (!LoadDungeonLevelInstance())
	{
		SetPortalState(ERpgPortalState::Active);
	}
}

void ARpgPortalActor::EnsureDungeonLevelStreamingConfig()
{
	if (!HasAuthority())
	{
		return;
	}

	if (DungeonLevelInstanceTransform.Equals(FTransform::Identity))
	{
		DungeonLevelInstanceTransform = GetDefaultDungeonLevelInstanceTransform();
	}

	if (DungeonLevelInstanceName.IsEmpty())
	{
		DungeonLevelInstanceName = GetDefaultDungeonLevelInstanceName();
	}
}

bool ARpgPortalActor::LoadDungeonLevelInstance()
{
	if (!EncounterDefinition || EncounterDefinition->DungeonLevel.IsNull())
	{
		return true;
	}

	if (DungeonLevelStreaming)
	{
		return true;
	}

	if (HasAuthority())
	{
		EnsureDungeonLevelStreamingConfig();
	}

	if (DungeonLevelInstanceName.IsEmpty() || DungeonLevelInstanceTransform.Equals(FTransform::Identity))
	{
		return false;
	}

	bool bLevelLoaded = false;
	DungeonLevelStreaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		EncounterDefinition->DungeonLevel,
		DungeonLevelInstanceTransform,
		bLevelLoaded,
		DungeonLevelInstanceName);

	if (!bLevelLoaded || !DungeonLevelStreaming)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to stream dungeon level %s."), *GetNameSafe(this), *EncounterDefinition->DungeonLevel.ToString());
		DungeonLevelStreaming = nullptr;
		return false;
	}

	DungeonLevelStreaming->OnLevelShown.AddDynamic(this, &ThisClass::HandleDungeonLevelShown);
	if (DungeonLevelStreaming->GetLoadedLevel())
	{
		HandleDungeonLevelShown();
	}

	return true;
}

void ARpgPortalActor::UnloadDungeonLevelInstance()
{
	if (!DungeonLevelStreaming)
	{
		DungeonLevelStreaming = nullptr;
		ClearDungeonMarkers();
		return;
	}

	DungeonLevelStreaming->OnLevelShown.RemoveDynamic(this, &ThisClass::HandleDungeonLevelShown);
	DungeonLevelStreaming->SetShouldBeVisible(false);
	DungeonLevelStreaming->SetShouldBeLoaded(false);
	DungeonLevelStreaming->SetIsRequestingUnloadAndRemoval(true);
	DungeonLevelStreaming = nullptr;
	ClearDungeonMarkers();
}

bool ARpgPortalActor::ResolveDungeonMarkers()
{
	ClearDungeonMarkers();

	ULevel* LoadedLevel = DungeonLevelStreaming ? DungeonLevelStreaming->GetLoadedLevel() : nullptr;
	if (!LoadedLevel)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot resolve dungeon markers because the dungeon level is not loaded."), *GetNameSafe(this));
		return false;
	}

	int32 EntryCount = 0;
	int32 BossSpawnCount = 0;
	int32 ExitPortalCount = 0;

	for (AActor* Actor : LoadedLevel->Actors)
	{
		ARpgPortalDungeonMarkerActor* Marker = Cast<ARpgPortalDungeonMarkerActor>(Actor);
		if (!Marker)
		{
			continue;
		}

		switch (Marker->GetMarkerRole())
		{
		case ERpgPortalDungeonMarkerRole::Entry:
			DungeonEntryMarker = Marker;
			++EntryCount;
			break;
		case ERpgPortalDungeonMarkerRole::BossSpawn:
			DungeonBossSpawnMarker = Marker;
			++BossSpawnCount;
			break;
		case ERpgPortalDungeonMarkerRole::ExitPortal:
			DungeonExitPortalMarker = Marker;
			++ExitPortalCount;
			break;
		default:
			break;
		}
	}

	const bool bHasExactlyOneMarkerOfEachRole = EntryCount == 1 && BossSpawnCount == 1 && ExitPortalCount == 1;
	if (!bHasExactlyOneMarkerOfEachRole)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s expected exactly one dungeon marker of each role but found Entry=%d, BossSpawn=%d, ExitPortal=%d."),
			*GetNameSafe(this),
			EntryCount,
			BossSpawnCount,
			ExitPortalCount);
		ClearDungeonMarkers();
		return false;
	}

	return true;
}

void ARpgPortalActor::ClearDungeonMarkers()
{
	DungeonEntryMarker = nullptr;
	DungeonBossSpawnMarker = nullptr;
	DungeonExitPortalMarker = nullptr;
}

void ARpgPortalActor::TeleportPendingDungeonEntrants()
{
	TArray<TObjectPtr<AActor>> PendingEntrants = MoveTemp(PendingDungeonEntrants);
	PendingDungeonEntrants.Reset();

	for (AActor* PendingEntrant : PendingEntrants)
	{
		if (IsValid(PendingEntrant))
		{
			TeleportActorToDungeon(PendingEntrant);
		}
	}
}

bool ARpgPortalActor::TeleportActorToDungeon(AActor* TravelActor)
{
	if (!TravelActor || !DungeonEntryMarker)
	{
		return false;
	}

	const FTransform EntryTransform = DungeonEntryMarker->GetMarkerTransform();
	const bool bTeleported = TravelActor->TeleportTo(EntryTransform.GetLocation(), EntryTransform.Rotator(), false, true);
	if (!bTeleported)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s failed to teleport %s into the portal dungeon."), *GetNameSafe(this), *GetNameSafe(TravelActor));
		return false;
	}

	RegisterDungeonOccupant(TravelActor);
	return true;
}

void ARpgPortalActor::RegisterDungeonOccupant(AActor* TravelActor)
{
	if (!TravelActor || DungeonOccupants.Contains(TravelActor))
	{
		return;
	}

	TravelActor->OnDestroyed.AddDynamic(this, &ThisClass::HandleDungeonOccupantDestroyed);
	DungeonOccupants.Add(TravelActor);
	RefreshDungeonOccupantCount();
}

void ARpgPortalActor::SpawnDungeonBoss()
{
	if (!EncounterDefinition)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn a dungeon boss without an EncounterDefinition."), *GetNameSafe(this));
		return;
	}

	if (!EncounterDefinition->BossClass)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn a dungeon boss because BossClass is unset."), *GetNameSafe(this));
		return;
	}

	if (!DungeonBossSpawnMarker)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn a dungeon boss because the BossSpawn marker is missing."), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.OverrideLevel = DungeonBossSpawnMarker->GetLevel();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	TrackedBoss = World->SpawnActor<AActor>(EncounterDefinition->BossClass, DungeonBossSpawnMarker->GetMarkerTransform(), SpawnParams);
	if (TrackedBoss)
	{
		TrackedBoss->OnDestroyed.AddDynamic(this, &ThisClass::HandleTrackedBossDestroyed);
	}
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

	if (!DungeonExitPortalMarker)
	{
		UE_LOG(LogRpgPortalActor, Warning, TEXT("%s cannot spawn its exit portal because the ExitPortal marker is missing."), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform ExitTransform = DungeonExitPortalMarker->GetMarkerTransform();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.OverrideLevel = DungeonExitPortalMarker->GetLevel();
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
	if (bDungeonBossDefeated)
	{
		return;
	}

	if (TrackedBoss)
	{
		TrackedBoss->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrackedBossDestroyed);
	}

	TrackedBoss = nullptr;
	bDungeonBossDefeated = true;
	CurrentStability = GetMaxStability();
	OnPortalStabilityChanged.Broadcast(CurrentStability, GetMaxStability());

	SpawnExitPortal();
	SetPortalState(DungeonOccupants.IsEmpty() ? ERpgPortalState::Sealable : ERpgPortalState::ExitOpen);
}

void ARpgPortalActor::RefreshStabilityFromProgress()
{
	const int32 TrackedEnemyCount = FMath::Max(TotalTrackedEnemyCount, 1);
	const float DefeatAlpha = FMath::Clamp(static_cast<float>(DefeatedTrackedEnemyCount) / static_cast<float>(TrackedEnemyCount), 0.0f, 1.0f);
	CurrentStability = GetMaxStability() * DefeatAlpha;
	OnPortalStabilityChanged.Broadcast(CurrentStability, GetMaxStability());
}

void ARpgPortalActor::RefreshDungeonOccupantCount()
{
	DungeonOccupants.RemoveAllSwap([](const AActor* Actor)
	{
		return Actor == nullptr || !IsValid(Actor);
	});

	DungeonOccupantCount = DungeonOccupants.Num();
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

FTransform ARpgPortalActor::GetDefaultDungeonLevelInstanceTransform() const
{
	constexpr double RegionGridSize = 100000.0;
	const FVector PortalLocation = GetActorLocation();
	const FVector RegionGridOrigin(
		FMath::GridSnap(PortalLocation.X, RegionGridSize),
		FMath::GridSnap(PortalLocation.Y, RegionGridSize),
		0.0);

	return FTransform(FRotator::ZeroRotator, FVector(1000000.0, 0.0, 0.0) + RegionGridOrigin);
}

FString ARpgPortalActor::GetDefaultDungeonLevelInstanceName() const
{
	return FString::Printf(TEXT("PortalDungeon_%s"), *GetFName().ToString());
}

FTransform ARpgPortalActor::GetOverworldReturnTransform() const
{
	return FTransform(GetActorRotation(), GetActorLocation() + GetActorForwardVector() * OverworldReturnDistance);
}

bool ARpgPortalActor::ShouldDungeonLevelBeLoadedForState() const
{
	return PortalState == ERpgPortalState::DungeonLoading
		|| PortalState == ERpgPortalState::DungeonInProgress
		|| PortalState == ERpgPortalState::ExitOpen
		|| PortalState == ERpgPortalState::Sealable;
}

bool ARpgPortalActor::IsTrackedEnemy(AActor* Actor) const
{
	return Actor && TrackedEnemies.Contains(Actor);
}

bool ARpgPortalActor::IsDungeonEncounterMode() const
{
	return !EncounterDefinition || EncounterDefinition->EncounterMode == ERpgPortalEncounterMode::Dungeon;
}

bool ARpgPortalActor::IsBrokenOutbreakMode() const
{
	return EncounterDefinition && EncounterDefinition->EncounterMode == ERpgPortalEncounterMode::BrokenOutbreak;
}

bool ARpgPortalActor::ShouldRewardsBeEligible() const
{
	const bool bRewardsRequireBossDefeat = EncounterDefinition ? EncounterDefinition->bRewardsRequireBossDefeat : true;
	return !bRewardsRequireBossDefeat || bDungeonBossDefeated;
}
