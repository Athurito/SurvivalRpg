#include "RpgInventoryEquipmentPlacementPolicy.h"

#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemInstance.h"

bool FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	switch (EquipmentSlot)
	{
	case ERpgEquipmentSlot::MainHand:
	case ERpgEquipmentSlot::OffHand:
	case ERpgEquipmentSlot::Head:
	case ERpgEquipmentSlot::Chest:
	case ERpgEquipmentSlot::Hands:
	case ERpgEquipmentSlot::Legs:
	case ERpgEquipmentSlot::Feet:
	case ERpgEquipmentSlot::Backpack:
	case ERpgEquipmentSlot::Belt:
	case ERpgEquipmentSlot::Pouch:
	case ERpgEquipmentSlot::ResourceBag:
		return true;

	case ERpgEquipmentSlot::None:
	default:
		return false;
	}
}

bool FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return EquipmentSlot == ERpgEquipmentSlot::MainHand ||
		EquipmentSlot == ERpgEquipmentSlot::OffHand;
}

bool FRpgInventoryEquipmentPlacementPolicy::IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return EquipmentSlot == ERpgEquipmentSlot::Backpack ||
		EquipmentSlot == ERpgEquipmentSlot::Belt ||
		EquipmentSlot == ERpgEquipmentSlot::Pouch ||
		EquipmentSlot == ERpgEquipmentSlot::ResourceBag;
}

const URpgEquipmentDefinition* FRpgInventoryEquipmentPlacementPolicy::FindEquipmentDefinition(
	const URpgInventoryItemInstance* Item)
{
	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item
		? Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>()
		: nullptr;
	const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment
		? EquippableFragment->GetEquipmentDefinition()
		: nullptr;
	return EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
}

bool FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
	const URpgInventoryItemInstance* Item,
	ERpgEquipmentSlot EquipmentSlot)
{
	if (!Item || !IsManagedEquipmentSlot(EquipmentSlot))
	{
		return false;
	}

	const URpgEquipmentDefinition* EquipmentDefinition = FindEquipmentDefinition(Item);
	if (IsSlotContainerEquipmentSlot(EquipmentSlot))
	{
		if (!Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
		{
			return false;
		}

		return !EquipmentDefinition || EquipmentDefinition->CanEquipInSlot(EquipmentSlot);
	}

	return EquipmentDefinition && EquipmentDefinition->CanEquipInSlot(EquipmentSlot);
}
