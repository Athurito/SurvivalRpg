#include "RpgAbilityBindingResolver.h"

#include "Abilities/GameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/SurvivalRpg.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAbilityBindingResolver)

FRpgUniqueAbilityBindingResolution FRpgAbilityBindingResolver::ResolveUniqueAbilityId(
	const URpgAbilitySystemComponent* AbilitySystemComponent,
	FGameplayTag AbilityIdTag,
	const UObject* BindingOwner)
{
	FRpgUniqueAbilityBindingResolution Resolution;
	if (!AbilitySystemComponent || !AbilityIdTag.IsValid())
	{
		return Resolution;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		const bool bMatchesDynamicId = Spec.GetDynamicSpecSourceTags().HasTagExact(AbilityIdTag);
		const bool bMatchesStaticId = Spec.Ability->GetAssetTags().HasTagExact(AbilityIdTag);
		if (!bMatchesDynamicId && !bMatchesStaticId)
		{
			continue;
		}

		++Resolution.MatchCount;
		if (Resolution.MatchCount == 1)
		{
			Resolution.SpecHandle = Spec.Handle;
			Resolution.AbilityCDO = Cast<URpgGameplayAbility>(Spec.Ability);
		}
	}

	if (Resolution.MatchCount == 0)
	{
		Resolution.Result = ERpgAbilityBindingResolveResult::Missing;
		return Resolution;
	}

	if (Resolution.MatchCount == 1)
	{
		Resolution.Result = ERpgAbilityBindingResolveResult::Unique;
		return Resolution;
	}

	Resolution.Result = ERpgAbilityBindingResolveResult::Ambiguous;
	Resolution.SpecHandle = FGameplayAbilitySpecHandle();
	Resolution.AbilityCDO = nullptr;
	UE_LOG(
		LogRpgAbilitySystem,
		Error,
		TEXT("Ability binding blocked: id [%s] resolves to %d granted specs for [%s]. Every quick-access ability id must be unique."),
		*AbilityIdTag.ToString(),
		Resolution.MatchCount,
		*GetNameSafe(BindingOwner));
	return Resolution;
}
