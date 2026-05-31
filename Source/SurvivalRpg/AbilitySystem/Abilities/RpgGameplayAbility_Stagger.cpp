#include "RpgGameplayAbility_Stagger.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgGameplayAbility_Stagger::URpgGameplayAbility_Stagger(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationPolicy = ERpgAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = ERpgAbilityActivationGroup::Independent;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = RpgGameplayTags::GameplayEvent_Stagger;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

bool URpgGameplayAbility_Stagger::CanActivateAbility(
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

	const URpgHealthComponent* HealthComponent = ActorInfo ? URpgHealthComponent::FindHealthComponent(ActorInfo->AvatarActor.Get()) : nullptr;
	return !HealthComponent || !HealthComponent->IsDeadOrDying();
}

void URpgGameplayAbility_Stagger::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	URpgAbilitySystemComponent* RpgASC = Cast<URpgAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (!RpgASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RpgASC->CancelActivationGroupAbilities(ERpgAbilityActivationGroup::Exclusive_Blocking, this, true);
	RpgASC->CancelActivationGroupAbilities(ERpgAbilityActivationGroup::Exclusive_Replaceable, this, true);

	if (!ChangeActivationGroup(ERpgAbilityActivationGroup::Exclusive_Blocking))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyStaggerTags();
	RpgASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerAttribute(), 0.0f);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAnimMontage* MontageToPlay = GuardBreakMontage ? GuardBreakMontage : StaggerMontage;
	if (MontageToPlay)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			MontageToPlay,
			FMath::Max(0.01f, MontagePlayRate));

		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnStaggerFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnStaggerFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnStaggerFinished);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnStaggerFinished);
		MontageTask->ReadyForActivation();
		return;
	}

	if (FallbackDuration > 0.0f)
	{
		UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, FallbackDuration);
		WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnStaggerFinished);
		WaitTask->ReadyForActivation();
		return;
	}

	OnStaggerFinished();
}

void URpgGameplayAbility_Stagger::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ClearStaggerTags();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Stagger::OnStaggerFinished()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URpgGameplayAbility_Stagger::ApplyStaggerTags() const
{
	if (URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Staggered, 1, EGameplayTagReplicationState::TagAndCountToAll);
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_GuardBroken, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void URpgGameplayAbility_Stagger::ClearStaggerTags() const
{
	if (URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Staggered, 0, EGameplayTagReplicationState::TagAndCountToAll);
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_GuardBroken, 0, EGameplayTagReplicationState::TagAndCountToAll);
	}
}
