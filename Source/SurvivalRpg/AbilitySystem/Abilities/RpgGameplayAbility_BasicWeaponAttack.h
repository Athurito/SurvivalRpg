#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility_FromEquipment.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "RpgGameplayAbility_BasicWeaponAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;

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
	void OnTraceDelayFinished();

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

private:
	const FRpgWeaponAttackDefinition* GetAttackDefinitionFromEquipment() const;
	bool TryGetSocketLocationFromWeapon(const URpgWeaponInstance* WeaponInstance, FName SocketName, FVector& OutLocation) const;
	bool TryGetSocketLocationFromAvatar(FName SocketName, FVector& OutLocation) const;
	void ResolveTrace(FVector& OutStart, FVector& OutEnd) const;
	void PerformDamageTrace();
	void ApplyDamageToHitActor(AActor* TargetActor, const FHitResult& HitResult);
	void SendHitReactionEvent(AActor* TargetActor, const FHitResult& HitResult, const FGameplayEffectSpec* DamageSpec) const;
	void FinishAttack(bool bWasCancelled);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Attack"))
	FGameplayTag AttackDefinitionTag;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float NoMontageEndDelay = 0.05f;

	UPROPERTY(Transient)
	TObjectPtr<URpgWeaponInstance> ActiveWeaponInstance;

	FRpgWeaponAttackDefinition ActiveAttackDefinition;
	bool bTraceHasFired = false;
	bool bWaitingForMontage = false;
	bool bFinishingAttack = false;
};
