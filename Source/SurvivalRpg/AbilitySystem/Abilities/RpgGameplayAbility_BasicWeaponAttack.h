#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility_FromEquipment.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "RpgGameplayAbility_BasicWeaponAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilitySystemComponent;

/**
 * Basic, data-driven weapon attack.
 *
 * The equipped URpgWeaponInstance supplies montage, damage GE, SetByCaller damage,
 * trace shape, and optional camera mode. Damage is applied only on authority.
 *
 * An attack definition with ComboNextAttack chains into that attack within the same activation: the owner buffers the
 * input, starts the next step in a new prediction window inside the montage's Combo Window and asks the server, which
 * accepts it from its own montage time and plays the same montage under the owner's prediction key. After each step's
 * attack window the ability becomes replaceable, so dodge and block may cancel the recovery.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_BasicWeaponAttack : public URpgGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_BasicWeaponAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	/** Emits the opt-in server-side rejection record before GAS forwards a remote failure to its owning client. */
	void LogAbilitySystemActivationFailure(
		FGameplayAbilitySpecHandle Handle,
		const AActor* AvatarActor,
		const FGameplayTagContainer& FailedReason,
		const FString& PredictionKey) const;

	/**
	 * Zero-based index of the running combo step, zero for the entry attack and while inactive. Read on the server by AI
	 * drivers that press the next step once per running step.
	 */
	int32 GetComboStepIndex() const { return IsActive() ? ComboStepIndex : 0; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Stable authority lifecycle counters used by rendered multiplayer contract tests. */
	uint32 GetAuthorityWindowOpenCountForTests() const { return AuthorityWindowOpenCountForTests; }
	uint32 GetAuthorityWindowCloseCountForTests() const { return AuthorityWindowCloseCountForTests; }
	uint32 GetAuthorityTraceSampleCountForTests() const { return AuthorityTraceSampleCountForTests; }
	uint32 GetAuthorityDamageHitCountForTests() const { return AuthorityDamageHitCountForTests; }
	/** Impact cues the owning client played from its own feedback trace, one per target and attack window. */
	uint32 GetOwnerPredictedImpactCountForTests() const { return OwnerPredictedImpactCountForTests; }
	bool IsAttackWindowOpenForTests() const { return bAttackWindowOpen; }
	/** Attack definition of the running combo step; empty while inactive. */
	FGameplayTag GetActiveAttackTagForTests() const { return ActiveAttackTag; }
	/** Zero-based index of the running combo step; zero for the entry attack. */
	int32 GetComboStepIndexForTests() const { return ComboStepIndex; }
	/** Whether the running step's recovery may be canceled by another exclusive ability. */
	bool IsRecoveryCancelableForTests() const { return bRecoveryCancelable; }
	bool HasPendingAttackTimersForTests() const;
	bool HasResidualAttackRuntimeStateForTests() const;
	/** Exercises the production auto-blend timing contract without requiring a mutable montage asset. */
	static bool IsAttackWindowEndBeforeAutoBlendOutForTests(
		float MontageLength,
		float WindowEndTime,
		float EffectivePlayRate,
		float AuthoredBlendOutTriggerTime);
#endif

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const override;

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

	/** Receives the authored local window-start signal; authority records it as telemetry only. */
	UFUNCTION()
	void OnAttackWindowStarted(FGameplayEventData Payload);

	/** Receives the authored local window-end signal; authority records it as telemetry only. */
	UFUNCTION()
	void OnAttackWindowEnded(FGameplayEventData Payload);

	/** Finishes a normally completed attack montage. */
	UFUNCTION()
	void OnMontageCompleted();

	/** Finishes the ability when the montage begins its normal blend-out. */
	UFUNCTION()
	void OnMontageBlendOut();

	/** Cancels attack runtime state after a montage interruption. */
	UFUNCTION()
	void OnMontageInterrupted();

	/** Cancels attack runtime state after the montage task is cancelled. */
	UFUNCTION()
	void OnMontageCancelled();

	/** Opens the server-authoritative trace window from the one-shot authority schedule. */
	UFUNCTION()
	void OnAuthorityAttackWindowStarted();

	/** Closes the server-authoritative trace window exactly once from the authority schedule. */
	UFUNCTION()
	void OnAuthorityAttackWindowEnded();

	/** Opens the running step's combo window; chains a buffered press or a pending server request. */
	UFUNCTION()
	void OnComboWindowOpened();

	/** Closes the running step's combo window on the owner; later presses no longer chain. */
	UFUNCTION()
	void OnComboWindowClosed();

	/** Makes the running step's recovery replaceable, so dodge and block may cancel it. */
	UFUNCTION()
	void OnRecoveryCancelOpened();

private:
	/**
	 * Copies Source into OutStep with the activation's itemization scale and checks montage, trace, play rate and attack
	 * window. Logs and returns false when the step cannot run.
	 */
	bool PrepareAttackStep(
		const FRpgWeaponAttackDefinition& Source,
		FRpgWeaponAttackDefinition& OutStep,
		float& OutWindowStartTime,
		float& OutWindowEndTime);
	/** Plays the running step's montage and schedules its authority, combo and recovery windows. False aborts the attack. */
	bool StartAttackMontage();
	/** Stops listening to the running montage task, so replacing its montage does not end the ability. */
	void DetachAttackMontageTask();
	void ScheduleComboTimers();
	void ClearComboTimers();
	void OnComboInputPressed();
	void OnComboStepRequested();
	/** Owner side: starts the next step, inside a new prediction window that the server request carries. */
	void ChainComboFromOwner();
	/** Replaces the running step with NextAttackTag; false leaves the running step untouched. */
	bool StartComboStep(FGameplayTag NextAttackTag);
	/** Server: swaps the attacker's step tags (OwnerTagsDuringAttack) for those of the running step. */
	void ApplyStepOwnerTags();
	/** Server: removes the step tags this activation added. */
	void RemoveStepOwnerTags();
	FGameplayTag ResolveAttackDefinitionTag(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;
	bool TryGetSocketLocationFromWeapon(const URpgWeaponInstance* WeaponInstance, FName SocketName, FVector& OutLocation) const;
	bool TryGetSocketLocationFromAvatar(FName SocketName, FVector& OutLocation) const;
	bool GatherTracePointLocations(TArray<FVector>& OutLocations);
	void BuildInterpolatedTracePointPairs(
		const TArray<FVector>& PreviousSocketLocations,
		const TArray<FVector>& CurrentSocketLocations,
		TArray<FVector>& OutPreviousTraceLocations,
		TArray<FVector>& OutCurrentTraceLocations) const;
	static bool IsAttackWindowEndBeforeAutoBlendOut(
		float MontageLength,
		float WindowEndTime,
		float EffectivePlayRate,
		float AuthoredBlendOutTriggerTime,
		float& OutRemainingPlayTime,
		float& OutBlendOutTriggerSeconds);
	bool ResolveAttackWindowTiming(
		const FRpgWeaponAttackDefinition& AttackDefinition,
		float EffectivePlayRate,
		float& OutStartTime,
		float& OutEndTime,
		FString& OutFailureReason) const;
	bool ScheduleAuthorityAttackWindow();
	void ClearAuthorityAttackWindowSchedule();
	void LogAttackLifecycle(const TCHAR* Stage, const FString& Detail = FString()) const;
	void LogAttackLifecycleLazy(const TCHAR* Stage, TFunctionRef<FString()> DetailBuilder) const;
	void WriteAttackLifecycle(const TCHAR* Stage, const FString& Detail) const;
	void HandleMontageEnded(const TCHAR* Stage, bool bWasCancelled);
	void OpenAttackWindow();
	void CloseAttackWindow(bool bLogMissingEndNotify);
	void PerformBladeTraceSample();
	void BuildTraceQueryParams(FCollisionQueryParams& QueryParams) const;
	void PerformLineTraceRibbon(
		const TArray<FVector>& PreviousSocketLocations,
		const TArray<FVector>& CurrentSocketLocations,
		const FCollisionQueryParams& QueryParams);
	void PerformSweepTraceRibbon(
		const TArray<FVector>& PreviousSocketLocations,
		const TArray<FVector>& CurrentSocketLocations,
		const FCollisionQueryParams& QueryParams);
	void TraceDamageLine(const FVector& TraceStart, const FVector& TraceEnd, const FCollisionQueryParams& QueryParams);
	void TraceDamageSweep(const FVector& TraceStart, const FVector& TraceEnd, const FCollisionQueryParams& QueryParams);
	void HandleTraceHitResults(const TArray<FHitResult>& HitResults);
	void EvaluateConditionalModifiers(const UAbilitySystemComponent* TargetASC, float& Damage, float& StaggerDamage) const;
	FGameplayEffectSpecHandle MakeWeaponDamageEffectSpec(const FHitResult& HitResult, const UAbilitySystemComponent* TargetASC) const;
	void ApplyDamageToHitActor(AActor* TargetActor, const FHitResult& HitResult);
	/** Whether this instance is a remote owner that traces its attack windows for local hit feedback only. */
	bool ShouldPredictImpactFeedback() const;
	/** Owner side: plays the impact cue the server will resolve for this hit, from the target's replicated state; presentation only. */
	void PredictImpactFeedback(AActor* TargetActor, const FHitResult& HitResult);
	void SendHitReactionEvent(AActor* TargetActor, const FHitResult& HitResult, const FGameplayEffectSpec* DamageSpec) const;
	void FinishAttack(bool bWasCancelled);

private:
	/** Designer-selected equipment attack key used when input routing does not select a definition. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Attack"))
	FGameplayTag AttackDefinitionTag;

	/** Designer opt-in to resolve primary/secondary attack definitions from the granted spec's input tag. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	bool bRouteAttackDefinitionFromInputTag = false;

	/**
	 * Designer yaw rate in deg/s at which a character with rotation modes may still turn toward the camera while the
	 * attack winds up. From the attack window start until the attack ends, its facing is held. Zero holds from activation.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Facing", meta = (ClampMin = "0.0", UIMax = "720.0", Units = "DegreesPerSecond"))
	float WindupSteeringRate = 360.0f;

	/**
	 * Seconds by which the server opens a remote owner's combo window and recovery cancel ahead of its own montage time.
	 * Absorbs jitter between the owner's and the server's montage clocks, so a step or dodge sent at the owner's window
	 * edge is not refused. Designer netcode tuning, 0 to about 0.3 s; the owner itself never chains before the window.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Combo", meta = (ClampMin = "0.0", UIMax = "0.3", Units = "s"))
	float ComboServerLeadTolerance = 0.15f;

	UPROPERTY(Transient)
	TObjectPtr<URpgWeaponInstance> ActiveWeaponInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	FRpgWeaponAttackDefinition ActiveAttackDefinition;
	/** Loose tags the server added for the running step; removed on the next step or when the attack ends. */
	FGameplayTagContainer AppliedStepOwnerTags;
	FGameplayTag ActiveAttackTag;
	float ActiveAttackWindowEndTime = 0.0f;
	int32 ComboStepIndex = 0;
	/** Item-rolled damage replacing the entry attack's authored damage; negative keeps authored values. */
	float RolledDamage = -1.0f;
	float RolledStaggerDamage = -1.0f;
	float EntryAuthoredDamage = 0.0f;
	float EntryAuthoredStaggerDamage = 0.0f;
	bool bComboWindowOpen = false;
	bool bComboWindowClosed = false;
	bool bComboInputBuffered = false;
	bool bComboRequestPending = false;
	bool bRecoveryCancelable = false;
	FPredictionKey PendingComboPredictionKey;
	FDelegateHandle ComboInputPressedHandle;
	FDelegateHandle ComboRequestHandle;
	FTimerHandle ComboWindowOpenTimerHandle;
	FTimerHandle ComboWindowCloseTimerHandle;
	FTimerHandle RecoveryCancelTimerHandle;
	bool bWaitingForMontage = false;
	bool bFinishingAttack = false;
	bool bAttackWindowOpen = false;
	bool bReceivedAttackWindowStart = false;
	bool bReceivedAttackWindowEnd = false;
	bool bAuthorityAttackWindowScheduled = false;
	bool bAuthorityWindowOpenedBySchedule = false;
	bool bAuthorityWindowClosedBySchedule = false;
	int32 AuthorityTraceSamplesThisActivation = 0;
	int32 AuthorityTracePointFailuresThisActivation = 0;
	int32 AuthorityDamageHitsThisActivation = 0;
	int32 AuthorityDuplicateHitsSkippedThisActivation = 0;
	bool bLoggedTracePointFailureThisActivation = false;

	FTimerHandle TraceSampleTimerHandle;
	FTimerHandle AuthorityAttackWindowStartTimerHandle;
	FTimerHandle AuthorityAttackWindowEndTimerHandle;
	TArray<FVector> PreviousTracePointLocations;
	TSet<TObjectKey<AActor>> HitActorsThisWindow;

#if WITH_DEV_AUTOMATION_TESTS
	uint32 AuthorityWindowOpenCountForTests = 0;
	uint32 AuthorityWindowCloseCountForTests = 0;
	uint32 AuthorityTraceSampleCountForTests = 0;
	uint32 AuthorityDamageHitCountForTests = 0;
	uint32 OwnerPredictedImpactCountForTests = 0;
#endif
};
