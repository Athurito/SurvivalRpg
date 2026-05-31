#include "RpgGameplayAbility_Block.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgGameplayAbility_Block::URpgGameplayAbility_Block(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ERpgAbilityActivationPolicy::WhileInputActive;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Blocking;

	DefaultBlockDefinition.bAllowPerfectBlock = false;
	DefaultBlockDefinition.PerfectBlockWindow = 0.0f;
	DefaultBlockDefinition.BlockableDamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
}

bool URpgGameplayAbility_Block::CanActivateAbility(
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

	const FRpgWeaponBlockDefinition* BlockDefinition = ResolveBlockDefinition(Handle, ActorInfo);
	return BlockDefinition && BlockDefinition->bCanBlock;
}

void URpgGameplayAbility_Block::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	const FRpgWeaponBlockDefinition* BlockDefinition = ResolveBlockDefinition(Handle, ActorInfo);
	if (!BlockDefinition || !BlockDefinition->bCanBlock)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveBlockDefinition = *BlockDefinition;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo->IsNetAuthority())
	{
		ApplyBlockState(ActiveBlockDefinition);
	}

	PlayBlockMontage(ActiveBlockDefinition.BlockStartMontage ? ActiveBlockDefinition.BlockStartMontage : ActiveBlockDefinition.BlockLoopMontage, MontagePlayRate);

	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnBlockInputReleased);
	ReleaseTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* BlockEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RpgGameplayTags::GameplayEvent_Block);
	BlockEventTask->EventReceived.AddDynamic(this, &ThisClass::OnBlockEvent);
	BlockEventTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* PerfectBlockEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RpgGameplayTags::GameplayEvent_PerfectBlock);
	PerfectBlockEventTask->EventReceived.AddDynamic(this, &ThisClass::OnPerfectBlockEvent);
	PerfectBlockEventTask->ReadyForActivation();
}

void URpgGameplayAbility_Block::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		ClearBlockState();
	}

	PlayBlockMontage(ActiveBlockDefinition.BlockEndMontage, MontagePlayRate);

	ActiveBlockDefinition = FRpgWeaponBlockDefinition();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Block::OnBlockInputReleased(float TimeHeld)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URpgGameplayAbility_Block::OnBlockEvent(FGameplayEventData Payload)
{
	PlayBlockMontage(ActiveBlockDefinition.BlockHitMontage, MontagePlayRate);
}

void URpgGameplayAbility_Block::OnPerfectBlockEvent(FGameplayEventData Payload)
{
	PlayBlockMontage(
		ActiveBlockDefinition.PerfectBlockMontage ? ActiveBlockDefinition.PerfectBlockMontage : ActiveBlockDefinition.BlockHitMontage,
		MontagePlayRate);
}

void URpgGameplayAbility_Block::EndPerfectBlockWindow()
{
	SetReplicatedLooseTagCount(RpgGameplayTags::State_PerfectBlockWindow, 0);
}

const FRpgWeaponBlockDefinition* URpgGameplayAbility_Block::ResolveBlockDefinition(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (const URpgWeaponInstance* WeaponInstance = Cast<URpgWeaponInstance>(GetSourceObject(Handle, ActorInfo)))
	{
		return &WeaponInstance->GetBlockDefinition();
	}

	return &DefaultBlockDefinition;
}

void URpgGameplayAbility_Block::ApplyBlockState(const FRpgWeaponBlockDefinition& BlockDefinition)
{
	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	PreviousBlockAngleDegrees = ASC->GetNumericAttribute(URpgDefenseSet::GetBlockAngleDegreesAttribute());
	PreviousBlockStaminaCost = ASC->GetNumericAttribute(URpgDefenseSet::GetBlockStaminaCostAttribute());
	PreviousBlockDamageReduction = ASC->GetNumericAttribute(URpgDefenseSet::GetBlockDamageReductionAttribute());
	PreviousBlockStaggerDamageMultiplier = ASC->GetNumericAttribute(URpgDefenseSet::GetBlockStaggerDamageMultiplierAttribute());
	PreviousPerfectBlockStaminaRestore = ASC->GetNumericAttribute(URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute());
	PreviousPerfectBlockStaggerDamage = ASC->GetNumericAttribute(URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute());
	bStoredPreviousBlockAttributes = true;

	ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockAngleDegreesAttribute(), BlockDefinition.BlockAngleDegrees);
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockStaminaCostAttribute(), BlockDefinition.StaminaCost);
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockDamageReductionAttribute(), BlockDefinition.DamageReduction);
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockStaggerDamageMultiplierAttribute(), BlockDefinition.BlockStaggerDamageMultiplier);
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute(), BlockDefinition.PerfectBlockStaminaRestore);
	ASC->SetNumericAttributeBase(
		URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute(),
		BlockDefinition.PerfectBlockStaggerDamage * FMath::Max(0.0f, BlockDefinition.PerfectBlockStaggerDamageMultiplier));

	SetReplicatedLooseTagCount(RpgGameplayTags::State_Blocking, 1);
	bAppliedBlockState = true;

	if (BlockDefinition.bAllowPerfectBlock && BlockDefinition.PerfectBlockWindow > 0.0f)
	{
		SetReplicatedLooseTagCount(RpgGameplayTags::State_PerfectBlockWindow, 1);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PerfectBlockTimerHandle,
				this,
				&ThisClass::EndPerfectBlockWindow,
				BlockDefinition.PerfectBlockWindow,
				false);
		}
	}
	else
	{
		SetReplicatedLooseTagCount(RpgGameplayTags::State_PerfectBlockWindow, 0);
	}
}

void URpgGameplayAbility_Block::ClearBlockState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PerfectBlockTimerHandle);
	}

	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	SetReplicatedLooseTagCount(RpgGameplayTags::State_Blocking, 0);
	SetReplicatedLooseTagCount(RpgGameplayTags::State_PerfectBlockWindow, 0);

	if (bStoredPreviousBlockAttributes)
	{
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockAngleDegreesAttribute(), PreviousBlockAngleDegrees);
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockStaminaCostAttribute(), PreviousBlockStaminaCost);
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockDamageReductionAttribute(), PreviousBlockDamageReduction);
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockStaggerDamageMultiplierAttribute(), PreviousBlockStaggerDamageMultiplier);
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute(), PreviousPerfectBlockStaminaRestore);
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute(), PreviousPerfectBlockStaggerDamage);
	}

	bAppliedBlockState = false;
	bStoredPreviousBlockAttributes = false;
}

void URpgGameplayAbility_Block::SetReplicatedLooseTagCount(FGameplayTag Tag, int32 Count) const
{
	if (URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(Tag, Count, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void URpgGameplayAbility_Block::PlayBlockMontage(UAnimMontage* Montage, float PlayRate) const
{
	if (!Montage || !CurrentActorInfo)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = CurrentActorInfo->GetAnimInstance())
	{
		AnimInstance->Montage_Play(Montage, FMath::Max(0.01f, PlayRate));
	}
}
