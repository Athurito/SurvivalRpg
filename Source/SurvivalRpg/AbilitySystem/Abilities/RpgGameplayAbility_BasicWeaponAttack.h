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

protected:
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

	UFUNCTION()
	void OnAttackWindowStarted(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackWindowEnded(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

private:
	FGameplayTag ResolveAttackDefinitionTag(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;
	bool TryGetSocketLocationFromWeapon(const URpgWeaponInstance* WeaponInstance, FName SocketName, FVector& OutLocation) const;
	bool TryGetSocketLocationFromAvatar(FName SocketName, FVector& OutLocation) const;
	bool GatherTracePointLocations(TArray<FVector>& OutLocations) const;
	void BuildInterpolatedTracePointPairs(
		const TArray<FVector>& PreviousSocketLocations,
		const TArray<FVector>& CurrentSocketLocations,
		TArray<FVector>& OutPreviousTraceLocations,
		TArray<FVector>& OutCurrentTraceLocations) const;
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

	FTimerHandle TraceSampleTimerHandle;
	TArray<FVector> PreviousTracePointLocations;
	TSet<TObjectKey<AActor>> HitActorsThisWindow;
};
