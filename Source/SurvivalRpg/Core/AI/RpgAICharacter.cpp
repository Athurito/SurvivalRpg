#include "RpgAICharacter.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgEnemyCombatLoadout.h"
#include "SurvivalRpg/Core/AI/RpgAIPlayerState.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgLootSourceComponent.h"
#include "SurvivalRpg/Progression/RpgExperienceRewardComponent.h"

ARpgAICharacter::ARpgAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CombatArchetypeComponent = CreateDefaultSubobject<URpgEnemyCombatArchetypeComponent>(TEXT("CombatArchetypeComponent"));
	CombatLoadoutComponent = CreateDefaultSubobject<URpgEnemyCombatLoadoutComponent>(TEXT("CombatLoadoutComponent"));
	ExperienceRewardComponent = CreateDefaultSubobject<URpgExperienceRewardComponent>(TEXT("ExperienceRewardComponent"));
	LootInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("LootInventoryComponent"));
	LootContainerComponent = CreateDefaultSubobject<URpgInventoryContainerComponent>(TEXT("LootContainerComponent"));
	LootContainerComponent->ConfigureAsDeathLootContainer();
	LootSourceComponent = CreateDefaultSubobject<URpgLootSourceComponent>(TEXT("LootSourceComponent"));
	LootSourceComponent->SetAutomaticContainerUnlockEnabled(false);
	CorpseLifecycleComponent = CreateDefaultSubobject<URpgCorpseLifecycleComponent>(TEXT("CorpseLifecycleComponent"));
	CorpseLifecycleComponent->SetupAttachment(GetMesh());
}

void ARpgAICharacter::BeginPlay()
{
	Super::BeginPlay();

	LootContainerComponent->SetInteractionAnchor(CorpseLifecycleComponent);
	if (HasAuthority())
	{
		LootContainerComponent->SetInteractionRadius(
			CorpseLifecycleComponent->GetCorpseInteractionRadius());
		LootSourceComponent->SetAutomaticContainerUnlockEnabled(false);
		LootSourceComponent->OnLootPopulationCompleted.AddUObject(
			this,
			&ThisClass::HandleLootPopulationCompleted);
		LootInventoryComponent->OnInventoryPostCommit.AddUObject(
			this,
			&ThisClass::HandleInventoryPostCommit);
		CorpseLifecycleComponent->OnCorpseAvailabilityChangedNative().AddUObject(
			this,
			&ThisClass::HandleCorpseAvailabilityChanged);
		RefreshCorpseContainerState();
	}
}

void ARpgAICharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LootSourceComponent)
	{
		LootSourceComponent->OnLootPopulationCompleted.RemoveAll(this);
	}
	if (LootInventoryComponent)
	{
		LootInventoryComponent->OnInventoryPostCommit.RemoveAll(this);
	}
	if (CorpseLifecycleComponent)
	{
		CorpseLifecycleComponent->OnCorpseAvailabilityChangedNative().RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ARpgAICharacter::OnDeathStarted(AActor* OwningActor)
{
	if (HasAuthority() && CorpseLifecycleComponent)
	{
		// Capture before the base death path stops CharacterMovement and clears velocity.
		CorpseLifecycleComponent->NotifyDeathStarted(GetVelocity());
	}

	Super::OnDeathStarted(OwningActor);
}

void ARpgAICharacter::OnDeathFinished(AActor* OwningActor)
{
	if (HasAuthority() && CorpseLifecycleComponent)
	{
		CorpseLifecycleComponent->NotifyDeathFinished();
	}

	Super::OnDeathFinished(OwningActor);
}

void ARpgAICharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	ARpgAIPlayerState* AIPlayerState = Cast<ARpgAIPlayerState>(GetPlayerState());
	URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(this);
	if (AIPlayerState && PawnExtension)
	{
		PawnExtension->InitializeAbilitySystemComponent(AIPlayerState->GetRpgAbilitySystemComponent(), AIPlayerState);
	}
}

void ARpgAICharacter::HandleLootPopulationCompleted(
	URpgLootSourceComponent* Source,
	const bool bHasLoot)
{
	if (!HasAuthority() || Source != LootSourceComponent)
	{
		return;
	}

	CorpseLifecycleComponent->SetInventoryRequirementComplete(!bHasLoot);
	RefreshCorpseContainerState();
}

void ARpgAICharacter::HandleInventoryPostCommit(
	URpgInventoryManagerComponent* Inventory)
{
	if (!HasAuthority() || Inventory != LootInventoryComponent
		|| !LootSourceComponent->IsLootPopulated())
	{
		return;
	}

	RefreshCorpseContainerState();
}

void ARpgAICharacter::HandleCorpseAvailabilityChanged(
	URpgCorpseLifecycleComponent* Corpse,
	const bool bIsAvailable)
{
	(void)bIsAvailable;
	if (HasAuthority() && Corpse == CorpseLifecycleComponent)
	{
		RefreshCorpseContainerState();
	}
}

void ARpgAICharacter::RefreshCorpseContainerState()
{
	if (!HasAuthority() || !LootInventoryComponent || !LootContainerComponent
		|| !LootSourceComponent || !CorpseLifecycleComponent)
	{
		return;
	}

	const bool bPopulationComplete = LootSourceComponent->IsLootPopulated();
	const bool bInventoryEmpty = LootInventoryComponent->GetUsedEntryCount() == 0;
	if (bPopulationComplete)
	{
		CorpseLifecycleComponent->SetInventoryRequirementComplete(bInventoryEmpty);
	}

	LootContainerComponent->SetContainerAccessible(
		bPopulationComplete
		&& !bInventoryEmpty
		&& CorpseLifecycleComponent->IsCorpseAvailable());
}
