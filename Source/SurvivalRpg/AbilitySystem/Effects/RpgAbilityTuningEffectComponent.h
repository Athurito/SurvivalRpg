#pragma once

#include "GameplayEffectComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"

#include "RpgAbilityTuningEffectComponent.generated.h"

class UAbilitySystemComponent;

/**
 * Changes ability tunings (Ability.Tuning.*) while its gameplay effect is active, for example a timed harvesting spell
 * that widens the swings of both tools. URpgGameplayAbility::GetTunedValue folds these entries after the skill tree
 * and rune tunings of the ability's source item, so an effect's Set wins over theirs; prefer Add and Multiply so
 * talents still stack. Each active effect counts once whatever its stack count, and an inhibited one not at all, so
 * OngoingTagRequirements can switch it on and off as data. Entries reach every ability of the effect's target that
 * reads the tag, also abilities without a source item; name ability ids to keep them scoped.
 *
 * The server and the owning client resolve the same values because the owner receives full active effects (Mixed
 * replication); simulated proxies never resolve tunings. Designer-tuned static data on gameplay effect assets.
 */
UCLASS(DisplayName = "Ability Tunings")
class SURVIVALRPG_API URpgAbilityTuningEffectComponent final : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	URpgAbilityTuningEffectComponent();

	/**
	 * Folds the entries for TuningTag of every active, uninhibited effect of AbilitySystem that carries this component
	 * into Accumulator, in container order, applying each effect once. AbilityIds are the ids of the reading ability.
	 */
	static void AccumulateActiveEffectTunings(
		const UAbilitySystemComponent& AbilitySystem,
		const FGameplayTagContainer& AbilityIds,
		FGameplayTag TuningTag,
		FRpgAbilityTuningAccumulator& Accumulator);

#if WITH_EDITOR
	/**
	 * Rejects entries without a tag under Ability.Tuning or with a non-finite value, and an Instant owning effect, which
	 * never becomes active. Warns on entries without ability id, on a stacking owning effect and on Set entries.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#if WITH_DEV_AUTOMATION_TESTS
	/** A transient Has Duration effect of DurationSeconds that carries Tunings, as a spell's effect asset would. */
	static UGameplayEffect* MakeTransientEffectForTests(
		const TArray<FRpgSkillTreeAbilityTuning>& Tunings,
		float DurationSeconds = 10.0f);
#endif

	/**
	 * Tunings that apply while the effect is active, with the skill tree's schema: AbilityIdTag names the ability (empty
	 * means every ability that reads the tag), TuningTag the value, Operation and Value how it changes. Values keep the
	 * unit of the tuned value and stay inside the clamps of the reading ability. Designer-tuned static data.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Tunings")
	TArray<FRpgSkillTreeAbilityTuning> AbilityTunings;
};
