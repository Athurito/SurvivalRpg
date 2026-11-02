#include "RpgInventoryFragment_Ammunition.h"

#include "SurvivalRpg/Combat/RpgCombatProjectile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_Ammunition)

#define LOCTEXT_NAMESPACE "RpgInventoryFragment_Ammunition"

TSubclassOf<ARpgCombatProjectile> URpgInventoryFragment_Ammunition::FindProjectileClass(
	const TSubclassOf<URpgInventoryItemDefinition> ItemDef)
{
	const URpgInventoryItemDefinition* Definition = ItemDef ? ItemDef->GetDefaultObject<URpgInventoryItemDefinition>() : nullptr;
	const URpgInventoryFragment_Ammunition* Fragment = Definition
		? Cast<URpgInventoryFragment_Ammunition>(Definition->FindFragmentByClass(StaticClass()))
		: nullptr;
	return Fragment ? Fragment->ProjectileClass : nullptr;
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryFragment_Ammunition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	if (!ProjectileClass)
	{
		Context.AddError(LOCTEXT("MissingProjectile", "The Ammunition fragment needs a projectile class."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
