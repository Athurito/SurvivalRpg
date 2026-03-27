// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameplayAbility_SelfRevive.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"

URpgGameplayAbility_SelfRevive::URpgGameplayAbility_SelfRevive()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgGameplayAbility_SelfRevive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (SelfReviveDelay <= 0.0f)
	{
		OnSelfReviveFinished();
		return;
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, SelfReviveDelay);
	WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnSelfReviveFinished);
	WaitTask->ReadyForActivation();
}

void URpgGameplayAbility_SelfRevive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled)
	{
		if (URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(GetAvatarActorFromActorInfo()))
		{
			DownedComponent->CancelRevive(GetAvatarActorFromActorInfo());
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_SelfRevive::OnSelfReviveFinished()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	URpgAbilitySystemComponent* AbilitySystemComponent = Cast<URpgAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(Avatar);

	if (!Avatar || !AbilitySystemComponent || !HealthComponent)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(Avatar); DownedComponent && DownedComponent->IsDowned())
	{
		if (!DownedComponent->BeginRevive(Avatar))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}

		DownedComponent->CompleteRevive(Avatar);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	AbilitySystemComponent->ResetForRevive();
	AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealthComponent->GetMaxHealth() * ReviveHealthPercent);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
