// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameplayAbility_Revive.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

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
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	FInteractionQuery AuthoritativeQuery;
	FText FailureReason;
	if (!ActorInfo->IsNetAuthority() || !UInteractionStatics::ValidateInteractionEventData(
			*ActorInfo,
			TriggerEventData,
			ActiveReviveOption,
			AuthoritativeQuery,
			FailureReason))
	{
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	AActor* Target = ActiveReviveOption.TargetRef.TargetActor.Get();
	AActor* Reviver = GetAvatarActorFromActorInfo();
	URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(Target);
	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(Target);

	if (!Target || !Reviver || !DownedComponent || !CommitAbility(Handle, ActorInfo, ActivationInfo) ||
		!DownedComponent->BeginRevive(Reviver))
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
	ReviveInteractionEventData = TriggerEventData ? *TriggerEventData : FGameplayEventData();
	bInteractionStarted = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReviveValidationTimerHandle,
			this,
			&ThisClass::ValidateReviveInteraction,
			0.1f,
			true);
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReviveDuration);
	WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnReviveCastFinished);
	WaitTask->ReadyForActivation();

}

void URpgGameplayAbility_Revive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReviveValidationTimerHandle);
	}

	if (bWasCancelled)
	{
		if (URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(ReviveTarget.Get()))
		{
			DownedComponent->CancelRevive(GetAvatarActorFromActorInfo());
		}
		if (bInteractionStarted)
		{
			UInteractionStatics::BroadcastInteractionMessage(
				this,
				RpgGameplayTags::Rpg_Interaction_Message_Rejected,
				ActiveReviveOption,
				GetAvatarActorFromActorInfo(),
				false);
		}
	}

	ReviveTarget = nullptr;
	ReviveInteractionEventData = FGameplayEventData();
	ActiveReviveOption = FInteractionOption();
	bInteractionStarted = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Revive::ValidateReviveInteraction()
{
	if (!CurrentActorInfo || !IsActive())
	{
		return;
	}
	if (!IsReviveInteractionStillValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool URpgGameplayAbility_Revive::IsReviveInteractionStillValid() const
{
	if (!CurrentActorInfo || !ReviveTarget.IsValid())
	{
		return false;
	}

	FInteractionOption RevalidatedOption;
	FInteractionQuery AuthoritativeQuery;
	FText FailureReason;
	AActor* Reviver = GetAvatarActorFromActorInfo();
	URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(ReviveTarget.Get());
	return UInteractionStatics::ValidateInteractionEventData(
			*CurrentActorInfo,
			&ReviveInteractionEventData,
			RevalidatedOption,
			AuthoritativeQuery,
			FailureReason) &&
		RevalidatedOption.TargetRef.TargetActor.Get() == ReviveTarget.Get() &&
		DownedComponent && DownedComponent->CanBeRevivedBy(Reviver);
}

void URpgGameplayAbility_Revive::OnReviveCastFinished()
{
	AActor* Target = ReviveTarget.Get();
	AActor* Reviver = GetAvatarActorFromActorInfo();
	URpgDownedComponent* DownedComponent = URpgDownedComponent::FindDownedComponent(Target);

	if (!Target || !Reviver || !DownedComponent || !IsReviveInteractionStillValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	DownedComponent->CompleteRevive(Reviver);
	UInteractionStatics::BroadcastInteractionMessage(
		this,
		RpgGameplayTags::Rpg_Interaction_Message_Ended,
		ActiveReviveOption,
		Reviver,
		true);
	bInteractionStarted = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
