#include "RpgPortalActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_ClosePortal.h"
#include "SurvivalRpg/Combat/RpgCombatMessages.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Portals/RpgPortalEncounterDefinition.h"
#include "SurvivalRpg/Portals/RpgPortalMessages.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalActor)

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

	Super::EndPlay(EndPlayReason);
}

void ARpgPortalActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PortalState);
	DOREPLIFETIME(ThisClass, CurrentStability);
	DOREPLIFETIME(ThisClass, DefeatedTrackedEnemyCount);
	DOREPLIFETIME(ThisClass, TotalTrackedEnemyCount);
}

void ARpgPortalActor::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
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
	TrackedEnemies.Reset();

	SetPortalState(ERpgPortalState::Active);
	SpawnEncounterEnemies();
	RefreshStabilityFromProgress();

	if (TrackedEnemies.IsEmpty())
	{
		CurrentStability = GetMaxStability();
		SetPortalState(ERpgPortalState::Sealable);
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

	FRpgPortalCompletedMessage Message;
	Message.Portal = this;
	Message.Instigator = ClosingActor;
	Message.EncounterDefinition = EncounterDefinition;
	Message.CompletionTags = EncounterDefinition ? EncounterDefinition->CompletionTags : FGameplayTagContainer();
	Message.FinalStability = CurrentStability;
	Message.bRewardsRequireBossDefeat = EncounterDefinition ? EncounterDefinition->bRewardsRequireBossDefeat : true;
	Message.bBossDefeated = false;
	Message.bRewardsEligible = ShouldRewardsBeEligible();

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.BroadcastMessage(RpgGameplayTags::Rpg_Portal_Message_Completed, Message);
	}

	return true;
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

void ARpgPortalActor::HandleTrackedEnemyDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || PortalState != ERpgPortalState::Active)
	{
		return;
	}

	MarkTrackedEnemyDefeated(DestroyedActor);
}

void ARpgPortalActor::HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message)
{
	if (!HasAuthority() || PortalState != ERpgPortalState::Active || !Message.Victim)
	{
		return;
	}

	MarkTrackedEnemyDefeated(Message.Victim);
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
			RpgGameplayTags::Rpg_Combat_Message_ActorKilled,
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
		UE_LOG(LogTemp, Warning, TEXT("%s cannot spawn portal enemies without an EncounterDefinition."), *GetNameSafe(this));
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
		UE_LOG(LogTemp, Warning, TEXT("%s cannot spawn portal enemies because EnemySpawnEntries is empty or invalid."), *GetNameSafe(this));
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

void ARpgPortalActor::RefreshStabilityFromProgress()
{
	const int32 TrackedEnemyCount = FMath::Max(TotalTrackedEnemyCount, 1);
	const float DefeatAlpha = FMath::Clamp(static_cast<float>(DefeatedTrackedEnemyCount) / static_cast<float>(TrackedEnemyCount), 0.0f, 1.0f);
	CurrentStability = GetMaxStability() * DefeatAlpha;
	OnPortalStabilityChanged.Broadcast(CurrentStability, GetMaxStability());
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

bool ARpgPortalActor::IsTrackedEnemy(AActor* Actor) const
{
	return Actor && TrackedEnemies.Contains(Actor);
}

bool ARpgPortalActor::ShouldRewardsBeEligible() const
{
	const bool bRewardsRequireBossDefeat = EncounterDefinition ? EncounterDefinition->bRewardsRequireBossDefeat : true;
	const bool bBossDefeated = false;
	return !bRewardsRequireBossDefeat || bBossDefeated;
}
