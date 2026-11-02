#include "RpgRangedAimAutomationTestTypes.h"

#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_RangedAim.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRangedAimAutomationTestTypes)

URpgRangedAimAutomationAbility::URpgRangedAimAutomationAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ERpgAbilityActivationPolicy::WhileInputActive;
	ActivationGroup = ERpgAbilityActivationGroup::Independent;
	FGameplayTagContainer Tags;
	Tags.AddTag(RpgGameplayTags::Ability_Type_RangedAim);
	SetAssetTags(Tags);
}

void URpgRangedAimAutomationAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AimTask = URpgAbilityTask_RangedAim::HoldRangedAim(this, AimDefinition);
	AimTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleInterrupted);
	AimTask->ReadyForActivation();
}

void URpgRangedAimAutomationAbility::HandleInterrupted(const FName Reason)
{
	++InterruptedCount;
	LastInterruption = Reason;
}

URpgRangedAimAutomationDrawAbility::URpgRangedAimAutomationDrawAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = ERpgAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
}

URpgRangedAimAutomationExclusiveAbility::URpgRangedAimAutomationExclusiveAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
}

URpgRangedAimAutomationIdleAbility::URpgRangedAimAutomationIdleAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Independent;
}
