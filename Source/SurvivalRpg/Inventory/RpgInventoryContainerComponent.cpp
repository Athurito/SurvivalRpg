#include "RpgInventoryContainerComponent.h"

#include "Net/UnrealNetwork.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_OpenStorageContainer.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryContainerComponent)

URpgInventoryContainerComponent::URpgInventoryContainerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	OpenContainerOption.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenStorage;
	OpenContainerOption.Prompt.ActionText = NSLOCTEXT("RpgInventory", "OpenStorageContainerText", "Open");
	OpenContainerOption.Prompt.TargetText = NSLOCTEXT("RpgInventory", "OpenStorageContainerSubText", "Storage");
	OpenContainerOption.Prompt.InteractionPriority = 50;
	OpenContainerOption.InteractionAbilityToGrant = URpgGameplayAbility_OpenStorageContainer::StaticClass();
}

void URpgInventoryContainerComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	// Dropped inventories expose their owner-sensitive Collect option through the actor itself.
	// This component still supplies authoritative transfer/access checks after the loot screen opens.
	if (GetOwner() && GetOwner()->IsA<ARpgDroppedInventoryActor>())
	{
		return;
	}
	if (!bAccessible && bHideInteractionWhenInaccessible)
	{
		return;
	}

	FInteractionOption Option = OpenContainerOption;
	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenStorage;
	Option.TargetRef.TargetActor = GetOwner();
	Option.Prompt.InteractionRange = InteractionRadius > 0.0f
		? InteractionRadius
		: Option.Prompt.InteractionRange;
	const bool bSemanticallyAccessible = bAccessible && GetOwner() && InteractQuery.RequestingAvatar.IsValid();
	Option.Availability = bSemanticallyAccessible
		? ERpgInteractionAvailability::Available
		: ERpgInteractionAvailability::Blocked;
	if (!bSemanticallyAccessible)
	{
		Option.Prompt.BlockedReason = NSLOCTEXT("RpgInventory", "StorageUnavailable", "Storage is unavailable");
	}
	InteractionBuilder.AddInteractionOption(Option);
}

void URpgInventoryContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bAccessible);
	DOREPLIFETIME(ThisClass, PersistentContainerId);
}

URpgInventoryManagerComponent* URpgInventoryContainerComponent::GetInventoryManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
}

bool URpgInventoryContainerComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!bAccessible || OwnerActor == nullptr || RequestingActor == nullptr)
	{
		return false;
	}

	if (InteractionRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(OwnerActor->GetActorLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgInventoryContainerComponent::SetContainerAccessible(bool bNewAccessible)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		bAccessible = bNewAccessible;
	}
}

void URpgInventoryContainerComponent::ConfigureAsDeathLootContainer()
{
	bAccessible = false;
	bHideInteractionWhenInaccessible = true;
}
