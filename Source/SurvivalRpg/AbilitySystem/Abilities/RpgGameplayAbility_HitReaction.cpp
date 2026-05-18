#include "RpgGameplayAbility_HitReaction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgGameplayAbility_HitReaction::URpgGameplayAbility_HitReaction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationPolicy = ERpgAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = RpgGameplayTags::GameplayEvent_HitReaction;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void URpgGameplayAbility_HitReaction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(ActorInfo->AvatarActor.Get());
	if (!HealthComponent || HealthComponent->IsDeadOrDying())
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

	if (HitReactionMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			HitReactionMontage,
			FMath::Max(0.01f, MontagePlayRate));

		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnHitReactionFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnHitReactionFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnHitReactionFinished);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnHitReactionFinished);
		MontageTask->ReadyForActivation();
		return;
	}

	if (FallbackDuration > 0.0f)
	{
		UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, FallbackDuration);
		WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnHitReactionFinished);
		WaitTask->ReadyForActivation();
		return;
	}

	OnHitReactionFinished();
}

void URpgGameplayAbility_HitReaction::OnHitReactionFinished()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
