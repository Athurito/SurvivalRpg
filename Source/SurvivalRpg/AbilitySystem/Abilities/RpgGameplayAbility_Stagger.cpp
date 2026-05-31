#include "RpgGameplayAbility_Stagger.h"

#include "AbilitySystemComponent.h"
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
	if (HealthComponent && HealthComponent->IsDeadOrDying())
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return ASC &&
		ASC->HasMatchingGameplayTag(RpgGameplayTags::Trait_Staggerable) &&
		!ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Staggered) &&
		!ASC->HasMatchingGameplayTag(RpgGameplayTags::State_GuardBroken) &&
		!ASC->HasMatchingGameplayTag(RpgGameplayTags::State_StaggerImmune);
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

	ActivationGroupBeforeStagger = ActivationGroup;
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

	const float StaggerDuration = GetStaggerDuration();
	UAnimMontage* MontageToPlay = GuardBreakMontage ? GuardBreakMontage : StaggerMontage;
	if (MontageToPlay)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			MontageToPlay,
			FMath::Max(0.01f, MontagePlayRate));

		if (StaggerDuration <= 0.0f)
		{
			MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnStaggerFinished);
			MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnStaggerFinished);
		}
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnStaggerFinished);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnStaggerFinished);
		MontageTask->ReadyForActivation();
	}

	if (StaggerDuration > 0.0f)
	{
		UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, StaggerDuration);
		WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnStaggerFinished);
		WaitTask->ReadyForActivation();
		return;
	}

	if (MontageToPlay)
	{
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
	const ERpgAbilityActivationGroup GroupToRestore = ActivationGroupBeforeStagger;

	ClearStaggerTags();
	if (!bWasCancelled)
	{
		ApplyStaggerImmunity();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	ActivationGroup = GroupToRestore;
	ActivationGroupBeforeStagger = ERpgAbilityActivationGroup::Independent;
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

void URpgGameplayAbility_Stagger::ApplyStaggerImmunity() const
{
	if (URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo())
	{
		const float ImmunityDuration = GetStaggerImmunityDuration();
		ASC->AddTimedLooseGameplayTag(
			RpgGameplayTags::State_StaggerImmune,
			ImmunityDuration,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

float URpgGameplayAbility_Stagger::GetStaggerDuration() const
{
	if (const URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo())
	{
		const float AttributeDuration = ASC->GetNumericAttribute(URpgDefenseSet::GetStaggerDurationAttribute());
		if (AttributeDuration > 0.0f)
		{
			return AttributeDuration;
		}
	}

	return FMath::Max(0.0f, FallbackDuration);
}

float URpgGameplayAbility_Stagger::GetStaggerImmunityDuration() const
{
	if (const URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo())
	{
		return FMath::Max(0.0f, ASC->GetNumericAttribute(URpgDefenseSet::GetStaggerImmunityDurationAttribute()));
	}

	return 0.0f;
}
