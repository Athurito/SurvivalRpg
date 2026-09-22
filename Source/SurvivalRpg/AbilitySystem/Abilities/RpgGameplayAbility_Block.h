#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility_FromEquipment.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "RpgGameplayAbility_Block.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;
class URpgDefenseSet;

/** Equipment-configured block presentation with server-owned defensive attributes and teardown-safe cleanup. */
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

	bool ApplyBlockState(const FRpgWeaponBlockDefinition& BlockDefinition);
	void ClearBlockState();
	void SetReplicatedLooseTagCount(FGameplayTag Tag, int32 Count) const;
	float PlayBlockMontage(UAnimMontage* Montage, float PlayRate = 1.0f);

private:
	/** Designer fallback for grants without a weapon source; live weapon tuning comes from its equipment instance. */
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	FRpgWeaponBlockDefinition DefaultBlockDefinition;

	/** Positive playback multiplier for the configured block montages; does not alter the server's perfect-block window. */
	UPROPERTY(EditDefaultsOnly, Category = "Block|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	FRpgWeaponBlockDefinition ActiveBlockDefinition;

	bool bAppliedBlockState = false;
	bool bBlockInputReleased = false;
	bool bBlockLoopStarted = false;
	// GAS sets its ending guard only inside Super::EndAbility, after our synchronous cleanup callbacks.
	bool bEndingBlock = false;
	// This activation's base-value snapshot belongs only to these original, still-registered instances.
	TWeakObjectPtr<URpgAbilitySystemComponent> BlockStateASC;
	TWeakObjectPtr<const URpgDefenseSet> BlockStateDefenseSet;
	float PreviousBlockAngleDegrees = 0.0f;
	float PreviousBlockStaminaCost = 0.0f;
	float PreviousBlockDamageReduction = 0.0f;
	float PreviousBlockStaggerDamageMultiplier = 0.0f;
	float PreviousPerfectBlockStaminaRestore = 0.0f;
	float PreviousPerfectBlockStaggerDamage = 0.0f;

	FTimerHandle PerfectBlockTimerHandle;
	FTimerHandle BlockLoopTimerHandle;
};
