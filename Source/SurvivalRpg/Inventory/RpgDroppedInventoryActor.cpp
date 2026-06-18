#include "RpgDroppedInventoryActor.h"

#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Collect.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

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

bool ARpgDroppedInventoryActor::MergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	if (!HasAuthority() || !ItemDefinition || StackCount <= 0)
	{
		return false;
	}

	EnsureDefaultPickupInteractionOption();
	for (FPickupTemplate& Template : StaticInventory.Templates)
	{
		if (Template.ItemDef == ItemDefinition)
		{
			Template.StackCount += StackCount;
			ForceNetUpdate();
			return true;
		}
	}

	FPickupTemplate& NewTemplate = StaticInventory.Templates.AddDefaulted_GetRef();
	NewTemplate.ItemDef = ItemDefinition;
	NewTemplate.StackCount = StackCount;
	ForceNetUpdate();
	return true;
}

bool ARpgDroppedInventoryActor::CanMergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition || StaticInventory.Instances.Num() > 0)
	{
		return false;
	}

	for (const FPickupTemplate& Template : StaticInventory.Templates)
	{
		if (Template.ItemDef == ItemDefinition)
		{
			return true;
		}
	}

	return StaticInventory.Templates.Num() == 0;
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
