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
 */
UCLASS()
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

#if WITH_DEV_AUTOMATION_TESTS
	/** Stable authority lifecycle counters used by rendered multiplayer contract tests. */
	uint32 GetAuthorityWindowOpenCountForTests() const { return AuthorityWindowOpenCountForTests; }
	uint32 GetAuthorityWindowCloseCountForTests() const { return AuthorityWindowCloseCountForTests; }
	uint32 GetAuthorityTraceSampleCountForTests() const { return AuthorityTraceSampleCountForTests; }
	uint32 GetAuthorityDamageHitCountForTests() const { return AuthorityDamageHitCountForTests; }
	bool IsAttackWindowOpenForTests() const { return bAttackWindowOpen; }
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

private:
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
	void SendHitReactionEvent(AActor* TargetActor, const FHitResult& HitResult, const FGameplayEffectSpec* DamageSpec) const;
	void FinishAttack(bool bWasCancelled);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Attack"))
	FGameplayTag AttackDefinitionTag;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	bool bRouteAttackDefinitionFromInputTag = true;

	UPROPERTY(Transient)
	TObjectPtr<URpgWeaponInstance> ActiveWeaponInstance;

	FRpgWeaponAttackDefinition ActiveAttackDefinition;
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
#endif
};
