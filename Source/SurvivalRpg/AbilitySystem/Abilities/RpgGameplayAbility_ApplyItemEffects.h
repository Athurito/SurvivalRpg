#pragma once

#include "GameplayTagContainer.h"
#include "RpgGameplayAbility.h"
#include "TimerManager.h"

#include "RpgGameplayAbility_ApplyItemEffects.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilitySystemComponent;
class URpgInventoryFragment_UsableItem;
class URpgInventoryItemInstance;
class URpgInventoryItemUseContext;
enum class ERpgInventoryUseStepTrigger : uint8;
struct FRpgInventoryUsableItemEffect;
struct FRpgInventoryUsableItemGameplayCue;
struct FRpgInventoryUsableItemSetByCallerMagnitude;
struct FRpgInventoryUsableItemUseStep;

/**
 * Generic server-authoritative item-use ability that runs a data-driven item sequence.
 *
 * Item definitions supply requirements, optional montage data, effect/cue steps, and consumption timing. Blueprint
 * children may override defaults or add cosmetic hooks, but inventory mutation remains in this C++ path.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_ApplyItemEffects : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_ApplyItemEffects(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Registers a server-local item-use context for the next immediate ApplyItemEffects activation. Not replicated. */
	static void RegisterPendingUseContext(UAbilitySystemComponent* AbilitySystemComponent, URpgInventoryItemInstance* ItemInstance, URpgInventoryItemUseContext* UseContext);

	/** Clears a pending server-local item-use context when activation is rejected before the ability can consume it. */
	static void ClearPendingUseContext(UAbilitySystemComponent* AbilitySystemComponent, URpgInventoryItemInstance* ItemInstance);

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
	void OnUseMontageFinished();

	UFUNCTION()
	void OnUseMontageCancelled();

	UFUNCTION()
	void OnUseMontageEventReceived(FGameplayEventData Payload);

private:
	const URpgInventoryItemInstance* ResolveSourceItem(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData = nullptr) const;

	const URpgInventoryFragment_UsableItem* ResolveUsableFragment(const URpgInventoryItemInstance* ItemInstance) const;
	bool HasAnyConfiguredUseStep(const URpgInventoryItemInstance* ItemInstance) const;
	bool MeetsConfiguredUseRequirements(const URpgInventoryItemInstance* ItemInstance, const FGameplayAbilityActorInfo* ActorInfo) const;

	void StartSequenceTimersAndListeners(const URpgInventoryFragment_UsableItem* UsableFragment);
	void ExecuteStepsForTrigger(ERpgInventoryUseStepTrigger Trigger, FGameplayTag EventTag = FGameplayTag());
	void ExecuteDelayedStepByIndex(int32 StepIndex);
	bool ExecuteUseStep(int32 StepIndex, const FRpgInventoryUsableItemUseStep& Step);
	bool ApplyConfiguredEffect(const FRpgInventoryUsableItemEffect& EffectConfig, float UseCount) const;
	void ExecuteConfiguredCue(const FRpgInventoryUsableItemGameplayCue& CueConfig, float UseCount) const;
	URpgInventoryItemUseContext* TakePendingUseContext(const FGameplayAbilityActorInfo* ActorInfo, const URpgInventoryItemInstance* ItemInstance) const;
	void FinishItemUse(bool bWasCancelled);

	const URpgInventoryFragment_UsableItem* ActiveUsableFragment = nullptr;
	float ActiveUseCount = 1.0f;
	bool bUseIsFinishing = false;
	bool bWaitingForUseMontage = false;
	int32 PendingDelayedStepCount = 0;
	TSet<int32> ExecutedStepIndices;
	TArray<FTimerHandle> DelayedStepTimerHandles;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryItemUseContext> ActiveUseContext;
};
