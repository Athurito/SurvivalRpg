// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameplayAbility_Revive.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/SurvivalRpg.h"

URpgGameplayAbility_Revive::URpgGameplayAbility_Revive()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = RpgGameplayTags::GameplayEvent_Revive;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void URpgGameplayAbility_Revive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	// Get the target from the event payload.
	AActor* Target = TriggerEventData ? const_cast<AActor*>(Cast<AActor>(TriggerEventData->Target.Get())) : nullptr;

	if (!Target)
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgGameplayAbility_Revive: No target in event payload. Cancelling."));
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	// Verify the target is actually downed.
	URpgDownedComponent* DownedComp = URpgDownedComponent::FindDownedComponent(Target);
	if (!DownedComp || !DownedComp->IsDowned())
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgGameplayAbility_Revive: Target [%s] is not downed. Cancelling."), *GetNameSafe(Target));
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	ReviveTarget = Target;

	// Start the cast timer using WaitDelay ability task.
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReviveDuration);
	WaitTask->OnFinish.AddDynamic(this, &URpgGameplayAbility_Revive::OnReviveCastFinished);
	WaitTask->ReadyForActivation();

	UE_LOG(LogRpg, Log, TEXT("RpgGameplayAbility_Revive: [%s] started reviving [%s]. Duration: %.1fs"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(Target), ReviveDuration);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void URpgGameplayAbility_Revive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled)
	{
		UE_LOG(LogRpg, Log, TEXT("RpgGameplayAbility_Revive: Revive cancelled for target [%s]."), *GetNameSafe(ReviveTarget.Get()));
	}

	ReviveTarget = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Revive::OnReviveCastFinished()
{
	AActor* Target = ReviveTarget.Get();
	if (!Target)
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgGameplayAbility_Revive: Target lost during revive cast."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	URpgDownedComponent* DownedComp = URpgDownedComponent::FindDownedComponent(Target);
	if (!DownedComp || !DownedComp->IsDowned())
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgGameplayAbility_Revive: Target [%s] is no longer downed."), *GetNameSafe(Target));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Complete the revive on the target.
	DownedComp->CompleteRevive(GetAvatarActorFromActorInfo());

	UE_LOG(LogRpg, Log, TEXT("RpgGameplayAbility_Revive: Successfully revived [%s]."), *GetNameSafe(Target));

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
