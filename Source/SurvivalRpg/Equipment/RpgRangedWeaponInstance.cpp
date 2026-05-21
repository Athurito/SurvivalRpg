#include "RpgRangedWeaponInstance.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgRangedWeaponInstance::URpgRangedWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponTypeTag = RpgGameplayTags::Weapon_Type_Ranged;
	WeaponFamilyTag = FGameplayTag();
}

float URpgRangedWeaponInstance::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	return 1.0f;
}

float URpgRangedWeaponInstance::GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	return 1.0f;
}
