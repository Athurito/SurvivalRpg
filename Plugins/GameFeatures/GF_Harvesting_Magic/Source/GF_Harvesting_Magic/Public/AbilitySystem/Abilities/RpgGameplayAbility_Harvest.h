#pragma once

#include "Harvesting/RpgHarvestTargeting.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_FromEquipment.h"
#include "TimerManager.h"

#include "RpgGameplayAbility_Harvest.generated.h"

class UAnimMontage;
class URpgCameraMode;

/**
 * Abstract mechanism for tool swings and harvesting powers. Concrete abilities are GA_* Blueprint assets that only
 * configure identity, targeting, sections, montage, cues, cooldown, and unlock requirements.
 *
 * The native part owns what must not depend on Blueprint flow:
 * - Server-side target selection at the moment of extraction, with the same query the owning client previews.
 * - Exactly one commit per activation, at the authored montage notify time resolved from montage data,
 *   because server-side animation notifies are not authoritative.
 * - Cancel safety: ending or cancelling the ability before the commit, for example by switching tools, never yields loot.
 * - Optional hold-to-aim: the ability previews while its input is held and executes on release.
 *
 * Tool category and harvest power come from the source equipment's item (URpgInventoryFragment_HarvestingTool).
 * Blueprint children must not implement the ActivateAbility event; use On Harvest Resolved and cues for feedback.
 */
UCLASS(Abstract, Blueprintable)
class GF_HARVESTING_MAGIC_API URpgGameplayAbility_Harvest : public URpgGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_Harvest(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Selects and evaluates this ability's current targets for Spec without mutating anything.
	 * Valid on the server and on the owning client; the targeting preview uses it. The server commit runs the same
	 * query from the aim captured when execution started (press, or release of a held aim), so turning the camera
	 * during the swing does not change what is hit.
	 */
	void EvaluateTargets(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		FRpgHarvestPreview& OutPreview) const;

	/** Returns whether this ability previews while its input is held and executes on release. */
	bool IsAimWhileInputHeld() const { return bAimWhileInputHeld; }

	/** Returns whether this ability's swings can strike a resource's active weak point for bonus sections. */
	bool CanHitWeakPoints() const { return bCanHitWeakPoints && Targeting.Shape == ERpgHarvestTargetShape::SingleTarget; }

	/** Returns the stable harvest ability id carried by every request of this ability. */
	FGameplayTag GetHarvestAbilityId() const { return HarvestAbilityId; }

	/**
	 * Resolves the delay in seconds from montage start to the single notify that sends CommitEventTag.
	 * Fails when the notify is missing or duplicated, lies outside the montage, or the play rate is invalid.
	 */
	static bool ResolveCommitDelay(
		const UAnimMontage* Montage,
		FGameplayTag CommitEventTag,
		float PlayRate,
		float& OutDelaySeconds,
		FString& OutFailureReason);

protected:
	//~ UGameplayAbility interface
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility interface

	/**
	 * Server-side notification after the commit, with every selected target and its committed result.
	 * Use it for cosmetic follow-ups only; loot and stock have already been resolved.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Harvesting", DisplayName = "On Harvest Resolved")
	void K2_OnHarvestResolved(const TArray<FRpgHarvestTargetEvaluation>& Results);

	/** Stable semantic id below Ability.Harvesting sent with every request, for example Ability.Harvesting.PickaxeStrike. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting", meta = (Categories = "Ability.Harvesting"))
	FGameplayTag HarvestAbilityId;

	/** Target selection used by the server commit and by the owning client's preview. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Targeting")
	FRpgHarvestTargetingParams Targeting;

	/** Stock sections this ability requests from each target; targets clamp it to their remaining stock. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Yield", meta = (ClampMin = "1", UIMin = "1", UIMax = "16"))
	int32 SectionsPerTarget = 1;

	/**
	 * When true, a single-target swing that strikes a resource's active weak point takes that resource's weak-point
	 * bonus sections in addition, from the same stock. Ignored for area targeting. Designer-tuned static data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Yield")
	bool bCanHitWeakPoints = false;

	/** Multiplier applied to the source tool's HarvestPower; scales yield-sensitive loot rows. One keeps the tool's power. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Yield", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "5.0"))
	float HarvestPowerScale = 1.0f;

	/** Optional trade skill that unlocks this ability, for example Skill.Gathering.Mining. Empty means always unlocked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Unlock", meta = (Categories = "Skill"))
	FGameplayTag RequiredSkillTag;

	/** Minimum level of RequiredSkillTag; activation fails with Ability.ActivateFail.Harvesting.SkillLevel below it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Unlock", meta = (ClampMin = "1", UIMin = "1", UIMax = "100"))
	int32 MinimumSkillLevel = 1;

	/** When true, the ability previews its targets while the input is held and executes on release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution")
	bool bAimWhileInputHeld = false;

	/**
	 * Camera mode the owning client uses while a hold-to-aim harvest is held, for example a higher view onto the
	 * ground; it is cleared on release or when the ability ends. Empty keeps the pawn camera. Cosmetic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (EditCondition = "bAimWhileInputHeld"))
	TSubclassOf<URpgCameraMode> AimCameraMode;

	/** Montage played on execution. Must contain exactly one RPG Gameplay Event notify sending CommitEventTag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution")
	TObjectPtr<UAnimMontage> HarvestMontage;

	/** Montage play rate; scales the commit delay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0"))
	float MontagePlayRate = 1.0f;

	/** GameplayEvent tag of the montage notify that marks the moment of extraction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitEventTag;

	/** Delay in seconds from execution to the commit when no montage is set; zero commits immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0", Units = "s"))
	float CommitDelaySeconds = 0.0f;

	/** Cosmetic cue executed on the server at the first harvested target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Feedback", meta = (Categories = "GameplayCue"))
	FGameplayTag SuccessGameplayCue;

	/** Cosmetic cue executed on the server at the aim point when the commit harvested nothing, for example an empty node. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Feedback", meta = (Categories = "GameplayCue"))
	FGameplayTag NoYieldGameplayCue;

private:
	UFUNCTION()
	void HandleAimInputReleased(float TimeHeld);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	/** Spends cost and cooldown, plays the montage, and schedules the authoritative commit. */
	void BeginHarvestExecution();
	void ScheduleAuthorityCommit(float DelaySeconds);
	void ExecuteAuthorityCommit();
	void SetLocalAimPreview(bool bAiming) const;
	bool MeetsSkillRequirement(const FGameplayAbilityActorInfo& ActorInfo) const;
	void BuildRequestTemplate(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		FRpgHarvestRequest& OutRequest) const;
	static bool GetViewPoint(const FGameplayAbilityActorInfo& ActorInfo, FVector& OutLocation, FRotator& OutRotation);
	void EvaluateTargetsFromView(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		const FVector& ViewLocation,
		const FRotator& ViewRotation,
		FRpgHarvestPreview& OutPreview) const;

	/** Server-only wakeup for the pending commit of the current activation. */
	FTimerHandle CommitTimerHandle;

	/** True between a scheduled and an executed or cancelled commit of the current activation. */
	bool bCommitPending = false;

	/** Authority-only aim captured when execution starts; the commit selects its targets from it. */
	FVector CommitViewLocation = FVector::ZeroVector;
	FRotator CommitViewRotation = FRotator::ZeroRotator;
	bool bHasCommitView = false;
};
