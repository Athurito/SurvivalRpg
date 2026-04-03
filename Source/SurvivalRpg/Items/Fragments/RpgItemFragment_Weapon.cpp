#include "RpgItemFragment_Weapon.h"

FGameplayTagContainer URpgItemFragment_Weapon::BuildCompatibilityTagContainer() const
{
	FGameplayTagContainer Result;

	if (WeaponTypeTag.IsValid())
	{
		Result.AddTag(WeaponTypeTag);
	}

	if (WeaponFamilyTag.IsValid())
	{
		Result.AddTag(WeaponFamilyTag);
	}

	Result.AppendTags(EquipmentTraitTags);
	Result.AppendTags(CompatibilityTags);
	return Result;
}
