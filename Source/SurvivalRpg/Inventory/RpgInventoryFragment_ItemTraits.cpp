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

int32 URpgInventoryFragment_ItemTraits::GetMaxStackSize() const
{
	return bCanStack ? FMath::Max(1, MaxStackSize) : 1;
}
