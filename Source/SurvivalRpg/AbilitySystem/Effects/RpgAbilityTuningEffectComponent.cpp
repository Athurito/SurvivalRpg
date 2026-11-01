#include "RpgAbilityTuningEffectComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemStats.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAbilityTuningEffectComponent)

#define LOCTEXT_NAMESPACE "RpgAbilityTuningEffectComponent"

DECLARE_CYCLE_STAT(TEXT("Rpg Ability Tunings From Effects"), STAT_RpgAbilityTuningsFromEffects, STATGROUP_AbilitySystem);

URpgAbilityTuningEffectComponent::URpgAbilityTuningEffectComponent()
{
#if WITH_EDITORONLY_DATA
	EditorFriendlyName = TEXT("Ability Tunings");
#endif
}

void URpgAbilityTuningEffectComponent::AccumulateActiveEffectTunings(
	const UAbilitySystemComponent& AbilitySystem,
	const FGameplayTagContainer& AbilityIds,
	const FGameplayTag TuningTag,
	FRpgAbilityTuningAccumulator& Accumulator)
{
	// Every tuned read scans the active effects; tool swings read about 33 tags at swing start.
	SCOPE_CYCLE_COUNTER(STAT_RpgAbilityTuningsFromEffects);

	// The iterator already skips effects pending removal; the stack count never multiplies the entries.
	for (const FActiveGameplayEffect& ActiveEffect : &AbilitySystem.GetActiveGameplayEffects())
	{
		const UGameplayEffect* Effect = ActiveEffect.Spec.Def;
		const URpgAbilityTuningEffectComponent* Tunings =
			Effect && !ActiveEffect.bIsInhibited && !ActiveEffect.IsPendingRemove
				? Effect->FindComponent<URpgAbilityTuningEffectComponent>()
				: nullptr;
		if (!Tunings)
		{
			continue;
		}

		for (const FRpgSkillTreeAbilityTuning& Tuning : Tunings->AbilityTunings)
		{
			Accumulator.Apply(Tuning, AbilityIds, TuningTag);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
UGameplayEffect* URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(
	const TArray<FRpgSkillTreeAbilityTuning>& Tunings,
	const float DurationSeconds)
{
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
	Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
	Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(DurationSeconds));
	Effect->AddComponent<URpgAbilityTuningEffectComponent>().AbilityTunings = Tunings;
	return Effect;
}
#endif

#if WITH_EDITOR
EDataValidationResult URpgAbilityTuningEffectComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const UGameplayEffect* Owner = GetOwner();
	if (Owner && Owner->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		Context.AddError(LOCTEXT(
			"InstantEffect",
			"Ability Tunings apply only while their effect is active; an Instant effect never is. Use Has Duration or Infinite."));
		Result = EDataValidationResult::Invalid;
	}
	if (Owner && Owner->GetStackingType() != EGameplayEffectStackingType::None)
	{
		Context.AddWarning(LOCTEXT(
			"StackingEffect",
			"Ability Tunings count once per active effect; stacks do not multiply them."));
	}

	const FGameplayTag TuningRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability.Tuning"), false);
	for (int32 Index = 0; Index < AbilityTunings.Num(); ++Index)
	{
		const FRpgSkillTreeAbilityTuning& Tuning = AbilityTunings[Index];
		if (!Tuning.TuningTag.IsValid() || !TuningRoot.IsValid() || !Tuning.TuningTag.MatchesTag(TuningRoot))
		{
			Context.AddError(FText::Format(
				LOCTEXT("InvalidTuningTag", "Ability Tunings entry {0} needs a TuningTag under Ability.Tuning."),
				FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		if (!FMath::IsFinite(Tuning.Value))
		{
			Context.AddError(FText::Format(
				LOCTEXT("NonFiniteValue", "Ability Tunings entry {0} has a non-finite value."),
				FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		if (!Tuning.AbilityIdTag.IsValid())
		{
			Context.AddWarning(FText::Format(
				LOCTEXT(
					"MissingAbilityId",
					"Ability Tunings entry {0} has no AbilityIdTag, so it tunes every ability of the target that reads {1}, not only those of one item."),
				FText::AsNumber(Index),
				FText::FromName(Tuning.TuningTag.GetTagName())));
		}
		if (Tuning.Operation == ERpgSkillTreeTuningOperation::Set)
		{
			Context.AddWarning(FText::Format(
				LOCTEXT(
					"SetEntry",
					"Ability Tunings entry {0} uses Set, which replaces the skill tree and rune values; prefer Add or Multiply."),
				FText::AsNumber(Index)));
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
