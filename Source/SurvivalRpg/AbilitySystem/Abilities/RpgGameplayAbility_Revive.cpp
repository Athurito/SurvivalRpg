// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameplayAbility_Revive.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgGameplayAbility_Revive::URpgGameplayAbility_Revive()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

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

	AActor* Target = TriggerEventData ? const_cast<AActor*>(Cast<AActor>(TriggerEventData->Target.Get())) : nullptr;
	AActor* Reviver = GetAvatarActorFromActorInfo();
	URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(Target);
	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(Target);

	if (!Target || !Reviver || !DownedComponent || !DownedComponent->BeginRevive(Reviver))
	{
		UE_LOG(
			LogRpg,
			Warning,
			TEXT("RpgGameplayAbility_Revive: Failed to start revive. Reviver=[%s] Target=[%s] DownedComponent=[%s] IsDowned=%s IsBeingRevived=%s DeathState=%d."),
			*GetNameSafe(Reviver),
			*GetNameSafe(Target),
			*GetNameSafe(DownedComponent),
			(DownedComponent && DownedComponent->IsDowned()) ? TEXT("true") : TEXT("false"),
			(DownedComponent && DownedComponent->IsBeingRevived()) ? TEXT("true") : TEXT("false"),
			HealthComponent ? static_cast<int32>(HealthComponent->GetDeathState()) : -1);
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	ReviveTarget = Target;

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReviveDuration);
	WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnReviveCastFinished);
	WaitTask->ReadyForActivation();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void URpgGameplayAbility_Revive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled)
	{
		if (URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(ReviveTarget.Get()))
		{
			DownedComponent->CancelRevive(GetAvatarActorFromActorInfo());
		}
	}

	ReviveTarget = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Revive::OnReviveCastFinished()
{
	AActor* Target = ReviveTarget.Get();
	AActor* Reviver = GetAvatarActorFromActorInfo();
	URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(Target);

	if (!Target || !Reviver || !DownedComponent || !DownedComponent->CanBeRevivedBy(Reviver))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	DownedComponent->CompleteRevive(Reviver);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
