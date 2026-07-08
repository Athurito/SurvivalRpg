#include "RpgGameplayAbility_ApplyItemEffects.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemUseContext.h"
#include "UObject/ObjectKey.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_ApplyItemEffects)

namespace
{
	struct FRpgPendingItemUseContextKey
	{
		TObjectKey<UAbilitySystemComponent> AbilitySystemComponent;
		TObjectKey<URpgInventoryItemInstance> ItemInstance;

		friend bool operator==(const FRpgPendingItemUseContextKey& A, const FRpgPendingItemUseContextKey& B)
		{
			return A.AbilitySystemComponent == B.AbilitySystemComponent && A.ItemInstance == B.ItemInstance;
		}
	};

	uint32 GetTypeHash(const FRpgPendingItemUseContextKey& Key)
	{
		return HashCombine(GetTypeHash(Key.AbilitySystemComponent), GetTypeHash(Key.ItemInstance));
	}

	TMap<FRpgPendingItemUseContextKey, TWeakObjectPtr<URpgInventoryItemUseContext>> GPendingItemUseContexts;

	FRpgPendingItemUseContextKey MakePendingUseContextKey(UAbilitySystemComponent* AbilitySystemComponent, URpgInventoryItemInstance* ItemInstance)
	{
		return FRpgPendingItemUseContextKey{TObjectKey<UAbilitySystemComponent>(AbilitySystemComponent), TObjectKey<URpgInventoryItemInstance>(ItemInstance)};
	}

	bool HasValidEffect(const TArray<FRpgInventoryUsableItemEffect>& Effects)
	{
		for (const FRpgInventoryUsableItemEffect& EffectConfig : Effects)
		{
			if (EffectConfig.GameplayEffect)
			{
				return true;
			}
		}

		return false;
	}

	bool HasValidCue(const TArray<FRpgInventoryUsableItemGameplayCue>& Cues)
	{
		for (const FRpgInventoryUsableItemGameplayCue& CueConfig : Cues)
		{
			if (CueConfig.GameplayCueTag.IsValid())
			{
				return true;
			}
		}

		return false;
	}

	bool StepHasWork(const FRpgInventoryUsableItemUseStep& Step)
	{
		return Step.bConsumeItem || HasValidEffect(Step.EffectsToApply) || HasValidCue(Step.GameplayCues);
	}

	bool HasMissingHealth(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		if (!AbilitySystemComponent)
		{
			return false;
		}

		const float Health = AbilitySystemComponent->GetNumericAttribute(URpgHealthSet::GetHealthAttribute());
		const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(URpgHealthSet::GetMaxHealthAttribute());
		return MaxHealth > 0.0f && Health < MaxHealth;
	}
}

URpgGameplayAbility_ApplyItemEffects::URpgGameplayAbility_ApplyItemEffects(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationPolicy = ERpgAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = ERpgAbilityActivationGroup::Independent;
}

void URpgGameplayAbility_ApplyItemEffects::RegisterPendingUseContext(
	UAbilitySystemComponent* AbilitySystemComponent,
	URpgInventoryItemInstance* ItemInstance,
	URpgInventoryItemUseContext* UseContext)
{
	if (!AbilitySystemComponent || !ItemInstance || !UseContext)
	{
		return;
	}

	GPendingItemUseContexts.FindOrAdd(MakePendingUseContextKey(AbilitySystemComponent, ItemInstance)) = UseContext;
}

void URpgGameplayAbility_ApplyItemEffects::ClearPendingUseContext(UAbilitySystemComponent* AbilitySystemComponent, URpgInventoryItemInstance* ItemInstance)
{
	if (!AbilitySystemComponent || !ItemInstance)
	{
		return;
	}

	GPendingItemUseContexts.Remove(MakePendingUseContextKey(AbilitySystemComponent, ItemInstance));
}

bool URpgGameplayAbility_ApplyItemEffects::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const URpgInventoryItemInstance* ItemInstance = ResolveSourceItem(Handle, ActorInfo);
	return HasAnyConfiguredUseStep(ItemInstance) && MeetsConfiguredUseRequirements(ItemInstance, ActorInfo);
}

void URpgGameplayAbility_ApplyItemEffects::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	const URpgInventoryItemInstance* ItemInstance = ResolveSourceItem(Handle, ActorInfo, TriggerEventData);
	ActiveUseContext = TakePendingUseContext(ActorInfo, ItemInstance);
	const URpgInventoryFragment_UsableItem* UsableFragment = ResolveUsableFragment(ItemInstance);
	if (!UsableFragment || !HasAnyConfiguredUseStep(ItemInstance) || !MeetsConfiguredUseRequirements(ItemInstance, ActorInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ActiveUsableFragment = UsableFragment;
	ActiveUseCount = TriggerEventData ? FMath::Max(1.0f, TriggerEventData->EventMagnitude) : 1.0f;
	if (ActiveUseContext)
	{
		ActiveUseCount = FMath::Max(1.0f, static_cast<float>(ActiveUseContext->RequestedUseCount));
	}

	ExecuteStepsForTrigger(ERpgInventoryUseStepTrigger::OnActivate);
	if (!IsActive())
	{
		return;
	}

	bWaitingForUseMontage = UsableFragment->UseMontage != nullptr;
	StartSequenceTimersAndListeners(UsableFragment);

	if (UsableFragment->UseMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			UsableFragment->UseMontage,
			FMath::Max(0.01f, UsableFragment->MontagePlayRate),
			UsableFragment->MontageStartSection);

		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnUseMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnUseMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnUseMontageCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnUseMontageCancelled);
		MontageTask->ReadyForActivation();
		return;
	}

	if (PendingDelayedStepCount <= 0)
	{
		FinishItemUse(false);
	}
}

void URpgGameplayAbility_ApplyItemEffects::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& TimerHandle : DelayedStepTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}

	ActiveUseContext = nullptr;
	ActiveUsableFragment = nullptr;
	ActiveUseCount = 1.0f;
	bUseIsFinishing = false;
	bWaitingForUseMontage = false;
	PendingDelayedStepCount = 0;
	ExecutedStepIndices.Reset();
	DelayedStepTimerHandles.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_ApplyItemEffects::OnUseMontageFinished()
{
	if (!IsActive() || bUseIsFinishing)
	{
		return;
	}

	bWaitingForUseMontage = false;
	ExecuteStepsForTrigger(ERpgInventoryUseStepTrigger::OnMontageCompleted);
	FinishItemUse(false);
}

void URpgGameplayAbility_ApplyItemEffects::OnUseMontageCancelled()
{
	if (!IsActive() || bUseIsFinishing)
	{
		return;
	}

	bWaitingForUseMontage = false;
	ExecuteStepsForTrigger(ERpgInventoryUseStepTrigger::OnMontageInterrupted);
	FinishItemUse(true);
}

void URpgGameplayAbility_ApplyItemEffects::OnUseMontageEventReceived(FGameplayEventData Payload)
{
	if (!IsActive() || bUseIsFinishing)
	{
		return;
	}

	ExecuteStepsForTrigger(ERpgInventoryUseStepTrigger::OnMontageEvent, Payload.EventTag);
}

const URpgInventoryItemInstance* URpgGameplayAbility_ApplyItemEffects::ResolveSourceItem(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData) const
{
	if (const URpgInventoryItemInstance* ItemFromSpec = Cast<URpgInventoryItemInstance>(GetSourceObject(Handle, ActorInfo)))
	{
		return ItemFromSpec;
	}

	return TriggerEventData ? Cast<URpgInventoryItemInstance>(TriggerEventData->OptionalObject) : nullptr;
}

const URpgInventoryFragment_UsableItem* URpgGameplayAbility_ApplyItemEffects::ResolveUsableFragment(const URpgInventoryItemInstance* ItemInstance) const
{
	return ItemInstance ? ItemInstance->FindFragmentByClass<URpgInventoryFragment_UsableItem>() : nullptr;
}

bool URpgGameplayAbility_ApplyItemEffects::HasAnyConfiguredUseStep(const URpgInventoryItemInstance* ItemInstance) const
{
	const URpgInventoryFragment_UsableItem* UsableFragment = ResolveUsableFragment(ItemInstance);
	if (!UsableFragment)
	{
		return false;
	}

	for (const FRpgInventoryUsableItemUseStep& Step : UsableFragment->UseSequence)
	{
		if (StepHasWork(Step))
		{
			return true;
		}
	}

	return HasValidEffect(UsableFragment->EffectsToApply);
}

bool URpgGameplayAbility_ApplyItemEffects::MeetsConfiguredUseRequirements(const URpgInventoryItemInstance* ItemInstance, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const URpgInventoryFragment_UsableItem* UsableFragment = ResolveUsableFragment(ItemInstance);
	if (!UsableFragment)
	{
		return false;
	}

	for (const FRpgInventoryUsableItemRequirement& Requirement : UsableFragment->UseRequirements)
	{
		switch (Requirement.RequirementType)
		{
		case ERpgInventoryUseRequirementType::None:
			break;

		case ERpgInventoryUseRequirementType::HealthBelowMax:
			if (!HasMissingHealth(ActorInfo))
			{
				return false;
			}
			break;
		}
	}

	return true;
}

void URpgGameplayAbility_ApplyItemEffects::StartSequenceTimersAndListeners(const URpgInventoryFragment_UsableItem* UsableFragment)
{
	if (!UsableFragment)
	{
		return;
	}

	TSet<FGameplayTag> MontageEventTags;
	for (int32 StepIndex = 0; StepIndex < UsableFragment->UseSequence.Num(); ++StepIndex)
	{
		const FRpgInventoryUsableItemUseStep& Step = UsableFragment->UseSequence[StepIndex];
		if (!StepHasWork(Step))
		{
			continue;
		}

		if (Step.Trigger == ERpgInventoryUseStepTrigger::AfterDelay)
		{
			if (Step.Delay <= 0.0f)
			{
				ExecuteDelayedStepByIndex(StepIndex);
				continue;
			}

			if (UWorld* World = GetWorld())
			{
				FTimerHandle TimerHandle;
				FTimerDelegate TimerDelegate;
				TimerDelegate.BindUObject(this, &ThisClass::ExecuteDelayedStepByIndex, StepIndex);
				World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, Step.Delay, false);
				DelayedStepTimerHandles.Add(TimerHandle);
				++PendingDelayedStepCount;
			}
			continue;
		}

		if (Step.Trigger == ERpgInventoryUseStepTrigger::OnMontageEvent && Step.MontageEventTag.IsValid())
		{
			MontageEventTags.Add(Step.MontageEventTag);
		}
	}

	for (const FGameplayTag& MontageEventTag : MontageEventTags)
	{
		UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			MontageEventTag,
			nullptr,
			false,
			true);
		EventTask->EventReceived.AddDynamic(this, &ThisClass::OnUseMontageEventReceived);
		EventTask->ReadyForActivation();
	}
}

void URpgGameplayAbility_ApplyItemEffects::ExecuteStepsForTrigger(ERpgInventoryUseStepTrigger Trigger, FGameplayTag EventTag)
{
	if (!ActiveUsableFragment)
	{
		return;
	}

	if (ActiveUsableFragment->UseSequence.IsEmpty())
	{
		if (Trigger != ERpgInventoryUseStepTrigger::OnActivate || ExecutedStepIndices.Contains(INDEX_NONE))
		{
			return;
		}

		FRpgInventoryUsableItemUseStep LegacyStep;
		LegacyStep.Trigger = ERpgInventoryUseStepTrigger::OnActivate;
		LegacyStep.EffectsToApply = ActiveUsableFragment->EffectsToApply;
		ExecutedStepIndices.Add(INDEX_NONE);
		if (!ExecuteUseStep(INDEX_NONE, LegacyStep))
		{
			FinishItemUse(true);
		}
		return;
	}

	for (int32 StepIndex = 0; StepIndex < ActiveUsableFragment->UseSequence.Num(); ++StepIndex)
	{
		const FRpgInventoryUsableItemUseStep& Step = ActiveUsableFragment->UseSequence[StepIndex];
		if (Step.Trigger != Trigger || ExecutedStepIndices.Contains(StepIndex) || !StepHasWork(Step))
		{
			continue;
		}

		if (Trigger == ERpgInventoryUseStepTrigger::OnMontageEvent && Step.MontageEventTag != EventTag)
		{
			continue;
		}

		ExecutedStepIndices.Add(StepIndex);
		if (!ExecuteUseStep(StepIndex, Step))
		{
			FinishItemUse(true);
			return;
		}
	}
}

void URpgGameplayAbility_ApplyItemEffects::ExecuteDelayedStepByIndex(int32 StepIndex)
{
	if (!IsActive() || bUseIsFinishing || !ActiveUsableFragment || !ActiveUsableFragment->UseSequence.IsValidIndex(StepIndex))
	{
		return;
	}

	PendingDelayedStepCount = FMath::Max(0, PendingDelayedStepCount - 1);

	const FRpgInventoryUsableItemUseStep& Step = ActiveUsableFragment->UseSequence[StepIndex];
	if (!ExecutedStepIndices.Contains(StepIndex))
	{
		ExecutedStepIndices.Add(StepIndex);
		if (!ExecuteUseStep(StepIndex, Step))
		{
			FinishItemUse(true);
			return;
		}
	}

	if (!bWaitingForUseMontage && PendingDelayedStepCount <= 0)
	{
		FinishItemUse(false);
	}
}

bool URpgGameplayAbility_ApplyItemEffects::ExecuteUseStep(int32 StepIndex, const FRpgInventoryUsableItemUseStep& Step)
{
	if (Step.bConsumeItem)
	{
		if (!ActiveUseContext || !ActiveUseContext->TryConsume())
		{
			return false;
		}
	}

	for (const FRpgInventoryUsableItemEffect& EffectConfig : Step.EffectsToApply)
	{
		ApplyConfiguredEffect(EffectConfig, ActiveUseCount);
	}

	for (const FRpgInventoryUsableItemGameplayCue& CueConfig : Step.GameplayCues)
	{
		ExecuteConfiguredCue(CueConfig, ActiveUseCount);
	}

	return true;
}

bool URpgGameplayAbility_ApplyItemEffects::ApplyConfiguredEffect(const FRpgInventoryUsableItemEffect& EffectConfig, float UseCount) const
{
	if (!EffectConfig.GameplayEffect)
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const float EffectLevel = EffectConfig.EffectLevel > 0.0f ? EffectConfig.EffectLevel : static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(EffectConfig.GameplayEffect, EffectLevel);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return false;
	}

	for (const FRpgInventoryUsableItemSetByCallerMagnitude& MagnitudeConfig : EffectConfig.SetByCallerMagnitudes)
	{
		if (!MagnitudeConfig.DataTag.IsValid())
		{
			continue;
		}

		const float Scale = MagnitudeConfig.bScaleByUseCount ? FMath::Max(1.0f, UseCount) : 1.0f;
		Spec->SetSetByCallerMagnitude(MagnitudeConfig.DataTag, MagnitudeConfig.Magnitude * Scale);
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
	return true;
}

void URpgGameplayAbility_ApplyItemEffects::ExecuteConfiguredCue(const FRpgInventoryUsableItemGameplayCue& CueConfig, float UseCount) const
{
	if (!CueConfig.GameplayCueTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.AbilityLevel = GetAbilityLevel();
	CueParameters.Instigator = GetAvatarActorFromActorInfo();
	CueParameters.EffectCauser = GetAvatarActorFromActorInfo();
	CueParameters.SourceObject = ActiveUseContext && ActiveUseContext->ItemInstance ? ActiveUseContext->ItemInstance.Get() : nullptr;
	CueParameters.RawMagnitude = CueConfig.RawMagnitude;

	AbilitySystemComponent->ExecuteGameplayCue(CueConfig.GameplayCueTag, CueParameters);
}

URpgInventoryItemUseContext* URpgGameplayAbility_ApplyItemEffects::TakePendingUseContext(
	const FGameplayAbilityActorInfo* ActorInfo,
	const URpgInventoryItemInstance* ItemInstance) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	URpgInventoryItemInstance* MutableItemInstance = const_cast<URpgInventoryItemInstance*>(ItemInstance);
	if (!AbilitySystemComponent || !MutableItemInstance)
	{
		return nullptr;
	}

	const FRpgPendingItemUseContextKey Key = MakePendingUseContextKey(AbilitySystemComponent, MutableItemInstance);
	TWeakObjectPtr<URpgInventoryItemUseContext> PendingContext;
	if (!GPendingItemUseContexts.RemoveAndCopyValue(Key, PendingContext))
	{
		return nullptr;
	}

	return PendingContext.Get();
}

void URpgGameplayAbility_ApplyItemEffects::FinishItemUse(bool bWasCancelled)
{
	if (!IsActive() || bUseIsFinishing)
	{
		return;
	}

	bUseIsFinishing = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
