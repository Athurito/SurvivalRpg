#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility_FromEquipment.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "RpgGameplayAbility_Block.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_Block : public URpgGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_Block(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	void OnBlockInputReleased(float TimeHeld);

	UFUNCTION()
	void OnBlockEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnPerfectBlockEvent(FGameplayEventData Payload);

	void EndPerfectBlockWindow();
	void QueueBlockLoopMontage(float Delay);
	void StartBlockLoopMontage();

private:
	const FRpgWeaponBlockDefinition* ResolveBlockDefinition(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	void ApplyBlockState(const FRpgWeaponBlockDefinition& BlockDefinition);
	void ClearBlockState();
	void SetReplicatedLooseTagCount(FGameplayTag Tag, int32 Count) const;
	float PlayBlockMontage(UAnimMontage* Montage, float PlayRate = 1.0f);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	FRpgWeaponBlockDefinition DefaultBlockDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Block|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	FRpgWeaponBlockDefinition ActiveBlockDefinition;

	bool bAppliedBlockState = false;
	bool bBlockInputReleased = false;
	bool bBlockLoopStarted = false;
	bool bStoredPreviousBlockAttributes = false;
	float PreviousBlockAngleDegrees = 0.0f;
	float PreviousBlockStaminaCost = 0.0f;
	float PreviousBlockDamageReduction = 0.0f;
	float PreviousBlockStaggerDamageMultiplier = 0.0f;
	float PreviousPerfectBlockStaminaRestore = 0.0f;
	float PreviousPerfectBlockStaggerDamage = 0.0f;

	FTimerHandle PerfectBlockTimerHandle;
	FTimerHandle BlockLoopTimerHandle;
};
