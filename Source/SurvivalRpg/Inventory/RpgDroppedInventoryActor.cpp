#include "RpgDroppedInventoryActor.h"

#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Collect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgDroppedInventoryActor)

void ARpgDroppedInventoryActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsureDefaultPickupInteractionOption();
}

void ARpgDroppedInventoryActor::SetPickupInventory(const FInventoryPickup& NewPickupInventory)
{
	if (HasAuthority())
	{
		EnsureDefaultPickupInteractionOption();
		StaticInventory = NewPickupInventory;
	}
}

void ARpgDroppedInventoryActor::EnsureDefaultPickupInteractionOption()
{
	if (!Option.InteractionAbilityToGrant)
	{
		Option.InteractionAbilityToGrant = URpgGameplayAbility_Collect::StaticClass();
	}

	if (Option.Text.IsEmpty())
	{
		Option.Text = NSLOCTEXT("RpgInventory", "PickupDroppedInventoryText", "Pick Up");
	}

	if (Option.SubText.IsEmpty())
	{
		Option.SubText = NSLOCTEXT("RpgInventory", "PickupDroppedInventorySubText", "Loot");
	}
}
