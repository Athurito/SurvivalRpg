#include "RpgRangedWeaponInstance.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRangedWeaponInstance)

#define LOCTEXT_NAMESPACE "RpgRangedWeaponInstance"

URpgRangedWeaponInstance::URpgRangedWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponTypeTag = RpgGameplayTags::Weapon_Type_Ranged;
	WeaponFamilyTag = FGameplayTag();

	// A ranged weapon shoots through its shot ability; it has no melee swing and no block.
	AttackDefinitions.Reset();
	BlockDefinition.bCanBlock = false;
	BlockDefinition.BlockableDamageTypeTags.Reset();
}

float URpgRangedWeaponInstance::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	return 1.0f;
}

float URpgRangedWeaponInstance::GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	return 1.0f;
}

#if WITH_EDITOR
EDataValidationResult URpgRangedWeaponInstance::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	// The class default of the native class has no content yet; only concrete weapons must shoot.
	if (HasAnyFlags(RF_ClassDefaultObject) && GetClass() == StaticClass())
	{
		return Result;
	}

	TArray<FText> Errors;
	RpgProjectileShot::ValidateDefinition(ShotDefinition, Errors);
	for (const FText& Error : Errors)
	{
		Context.AddError(FText::Format(LOCTEXT("InvalidShot", "Ranged weapon '{0}': ShotDefinition: {1}"),
			FText::FromString(GetPathName()), Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
