#include "RpgWeaponRuneDefinition.h"

#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWeaponRuneDefinition)

#define LOCTEXT_NAMESPACE "RpgWeaponRuneDefinition"

#if WITH_EDITOR
EDataValidationResult URpgWeaponRuneDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (EchoStrike.bEnabled)
	{
		if (!EchoStrike.DamageEffect)
		{
			Context.AddError(LOCTEXT("EchoNeedsDamageEffect", "The Echo strike needs a DamageEffect."));
			Result = EDataValidationResult::Invalid;
		}
		if (EchoStrike.Damage <= 0.0f && EchoStrike.StaggerDamage <= 0.0f)
		{
			Context.AddError(LOCTEXT("EchoNeedsDamage", "The Echo strike needs health or stagger damage."));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
