#include "RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_ItemTraits)

bool URpgInventoryFragment_ItemTraits::IsMaterial() const
{
	return bTreatAsMaterial || ItemCategory == ERpgInventoryItemCategory::Material;
}

bool URpgInventoryFragment_ItemTraits::CanDropForMode(ERpgPlayerDeathDropMode DropMode) const
{
	switch (DropMode)
	{
	case ERpgPlayerDeathDropMode::MaterialsOnly:
		return DeathDropRule == ERpgInventoryDeathDropRule::MaterialsOnly || IsMaterial();

	case ERpgPlayerDeathDropMode::AllBackpackExceptEquipment:
		return DeathDropRule == ERpgInventoryDeathDropRule::MaterialsOnly ||
			DeathDropRule == ERpgInventoryDeathDropRule::BackpackOnly ||
			IsMaterial();

	case ERpgPlayerDeathDropMode::None:
	default:
		return false;
	}
}

ERpgInventoryManualDropPolicy URpgInventoryFragment_ItemTraits::GetResolvedManualDropPolicy() const
{
	if (ManualDropPolicy != ERpgInventoryManualDropPolicy::Default)
	{
		return ManualDropPolicy;
	}

	if (ItemCategory == ERpgInventoryItemCategory::Quest)
	{
		return ERpgInventoryManualDropPolicy::Disabled;
	}

	if (ItemCategory == ERpgInventoryItemCategory::Weapon ||
		ItemCategory == ERpgInventoryItemCategory::Shield ||
		ItemCategory == ERpgInventoryItemCategory::Armor)
	{
		return ERpgInventoryManualDropPolicy::Confirm;
	}

	return ERpgInventoryManualDropPolicy::Direct;
}

int32 URpgInventoryFragment_ItemTraits::GetMaxStackSize() const
{
	return bCanStack ? FMath::Max(1, MaxStackSize) : 1;
}
