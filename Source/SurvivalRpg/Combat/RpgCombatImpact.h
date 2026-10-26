#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
class UAbilitySystemComponent;
class UWorld;
struct FGameplayCueParameters;
struct FGameplayEffectContextHandle;
struct FGameplayEffectSpec;
struct FHitResult;

/** How a melee impact resolves against the target's defense; the server's damage execution decides it. */
enum class ERpgCombatImpactOutcome : uint8
{
	/** No defense applies; the impact deals its full damage and stagger. */
	Hit,
	/** A paid normal block: reduced damage and stagger for the block stamina cost. */
	Block,
	/** A perfect block: no damage, and the attacker takes posture damage. */
	PerfectBlock,
};

/**
 * Shared melee impact rules and impact cue routing.
 *
 * The server's damage execution and the attacking owner's feedback prediction use these rules, so both
 * resolve a block from the same replicated target state: the block tags, the block cone attribute and stamina.
 * Impact cues are presentation only; the server's damage execution stays the authority for every outcome.
 */
namespace RpgCombatImpact
{
	/** Whether the faction rules let the context's instigators damage TargetActor. */
	SURVIVALRPG_API bool CanDamageTarget(const FGameplayEffectContextHandle& Context, const AActor* SourceActor, const AActor* TargetActor);

	/** Whether Spec carries DamageTypeTag in its asset tags or in its captured source tags. */
	SURVIVALRPG_API bool HasDamageType(const FGameplayEffectSpec& Spec, FGameplayTag DamageTypeTag);

	/** Whether SourceActor stands inside TargetActor's frontal block cone; BlockAngleDegrees is the full cone width, 359 or more blocks all around. */
	SURVIVALRPG_API bool IsInsideBlockCone(const AActor* TargetActor, const AActor* SourceActor, float BlockAngleDegrees);

	/** Resolves a melee impact from the target's block state; outside the cone, or without stamina for the block, it is a hit. */
	SURVIVALRPG_API ERpgCombatImpactOutcome ResolveMeleeOutcome(const UAbilitySystemComponent* TargetASC, const AActor* TargetActor, const AActor* SourceActor);

	/** The body-hit cue an attack put on its damage spec (a GameplayCue.Combat.Hit tag); invalid when the spec plays no impact cues. */
	SURVIVALRPG_API FGameplayTag FindHitCueTag(const FGameplayEffectSpec& Spec);

	/** Cue for an outcome: HitCueTag for a hit, the shared block and perfect-block cues otherwise. */
	SURVIVALRPG_API FGameplayTag GetOutcomeCueTag(ERpgCombatImpactOutcome Outcome, FGameplayTag HitCueTag);

	/** Whether the attacking client predicts this outcome's cue; it predicts a perfect block as a block. */
	SURVIVALRPG_API bool IsPredictedByOwner(ERpgCombatImpactOutcome Outcome);

	/**
	 * Impact cue parameters: the impact point and normal of HitResult, the attacker as instigator and causer,
	 * RawMagnitude the damage, NormalizedMagnitude the stagger damage as a fraction of the target's maximum stagger.
	 */
	SURVIVALRPG_API FGameplayCueParameters MakeCueParameters(
		const FGameplayEffectContextHandle& Context,
		AActor* SourceActor,
		const UAbilitySystemComponent* TargetASC,
		const FHitResult* HitResult,
		float Damage,
		float StaggerDamage);

	/**
	 * Server: executes a cue on TargetASC for every machine, the attacker included. Clears any prediction key scoped on
	 * TargetASC, so no client mistakes the cue for its own prediction. An impact cue notify on the attacking client
	 * absorbs the copy when the attacker already predicted it (URpgGameplayCueNotify_Impact).
	 */
	SURVIVALRPG_API void ExecuteServerCue(UAbilitySystemComponent* TargetASC, FGameplayTag CueTag, const FGameplayCueParameters& Parameters);

	/**
	 * Attacking client: plays an impact cue on Target locally at once and records the prediction, so the server's later
	 * copy of the same impact (same target and instigator) is absorbed instead of playing twice.
	 */
	SURVIVALRPG_API void PlayPredictedImpact(AActor* Target, FGameplayTag CueTag, const FGameplayCueParameters& Parameters);

	/** True while PlayPredictedImpact plays its cue; the prediction itself is never absorbed. */
	SURVIVALRPG_API bool IsPlayingPredictedImpact();

	/** Records a prediction of an impact by Instigator on Target; it expires after PredictedImpactLifetimeSeconds. */
	SURVIVALRPG_API void RecordPredictedImpact(const AActor* Target, const AActor* Instigator);

	/** Removes the oldest live prediction for Target and Instigator; true when one existed. */
	SURVIVALRPG_API bool ConsumePredictedImpact(const AActor* Target, const AActor* Instigator);

	/** Seconds a prediction waits for the server's copy; covers a high round trip but not the next combo hit's copy. */
	constexpr double PredictedImpactLifetimeSeconds = 0.75;

	/** Counts a server copy an impact notify absorbed on Target's machine. */
	void NoteAbsorbedImpact(const AActor* Target);

#if WITH_DEV_AUTOMATION_TESTS
	/** Server copies absorbed in World so far; routing observers see them although nothing plays. */
	SURVIVALRPG_API int32 GetAbsorbedImpactCountForTests(const UWorld* World);
#endif
}
