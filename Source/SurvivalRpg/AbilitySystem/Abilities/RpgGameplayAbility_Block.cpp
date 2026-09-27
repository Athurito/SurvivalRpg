#include "RpgGameplayAbility_Block.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimInstance.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace
{
TAutoConsoleVariable<int32> CVarRpgCombatBlockAnimationDebug(
	TEXT("rpg.Combat.Block.AnimationDebug"),
	0,
	TEXT("Logs block ability animation montage transitions."));
}

URpgGameplayAbility_Block::URpgGameplayAbility_Block(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ERpgAbilityActivationPolicy::WhileInputActive;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Blocking;

	DefaultBlockDefinition.bCanBlock = false;
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
	if (!ActorInfo->AbilitySystemComponent->GetSet<URpgDefenseSet>())
	{
		return false;
	}

	const URpgHealthComponent* HealthComponent = ActorInfo ? URpgHealthComponent::FindHealthComponent(ActorInfo->AvatarActor.Get()) : nullptr;
	if (HealthComponent && HealthComponent->IsDeadOrDying())
	{
		return false;
	}

	if (const URpgEquipmentInstance* EquipmentInstance = Cast<URpgEquipmentInstance>(GetSourceObject(Handle, ActorInfo)))
	{
		if (!IsEquipmentActiveForInput(EquipmentInstance, GetInputTagFromSpec(Handle, ActorInfo)))
		{
			return false;
		}
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
	bEndingBlock = false;

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
	bBlockInputReleased = false;
	bBlockLoopStarted = false;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive() || bEndingBlock)
	{
		return;
	}
	if (ActorInfo->IsNetAuthority() && !ApplyBlockState(ActiveBlockDefinition))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActiveBlockDefinition.BlockStartMontage)
	{
		const float StartDuration = PlayBlockMontage(ActiveBlockDefinition.BlockStartMontage, MontagePlayRate);
		if (ActiveBlockDefinition.BlockLoopMontage)
		{
			QueueBlockLoopMontage(StartDuration);
		}
	}
	else
	{
		StartBlockLoopMontage();
	}

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
	if (!IsEndAbilityValid(Handle, ActorInfo) || bEndingBlock)
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	bEndingBlock = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BlockLoopTimerHandle);
	}

	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		ClearBlockState();
	}

	if (bAppliedBlockState || bBlockLoopStarted || ActiveBlockDefinition.BlockStartMontage)
	{
		PlayBlockMontage(ActiveBlockDefinition.BlockEndMontage, MontagePlayRate);
	}

	ActiveBlockDefinition = FRpgWeaponBlockDefinition();
	bBlockInputReleased = false;
	bBlockLoopStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Block::OnBlockInputReleased(float TimeHeld)
{
	bBlockInputReleased = true;

	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URpgGameplayAbility_Block::OnBlockEvent(FGameplayEventData Payload)
{
	const float HitDuration = PlayBlockMontage(ActiveBlockDefinition.BlockHitMontage, MontagePlayRate);
	if (HitDuration > 0.0f && ActiveBlockDefinition.BlockLoopMontage && !bBlockInputReleased)
	{
		bBlockLoopStarted = false;
		QueueBlockLoopMontage(HitDuration);
	}
}

void URpgGameplayAbility_Block::OnPerfectBlockEvent(FGameplayEventData Payload)
{
	const float HitDuration = PlayBlockMontage(
		ActiveBlockDefinition.PerfectBlockMontage ? ActiveBlockDefinition.PerfectBlockMontage : ActiveBlockDefinition.BlockHitMontage,
		MontagePlayRate);
	if (HitDuration > 0.0f && ActiveBlockDefinition.BlockLoopMontage && !bBlockInputReleased)
	{
		bBlockLoopStarted = false;
		QueueBlockLoopMontage(HitDuration);
	}
}

void URpgGameplayAbility_Block::EndPerfectBlockWindow()
{
	SetReplicatedLooseTagCount(RpgGameplayTags::State_PerfectBlockWindow, 0);
}

void URpgGameplayAbility_Block::QueueBlockLoopMontage(float Delay)
{
	if (!ActiveBlockDefinition.BlockLoopMontage)
	{
		return;
	}

	if (Delay > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BlockLoopTimerHandle);
			World->GetTimerManager().SetTimer(
				BlockLoopTimerHandle,
				this,
				&ThisClass::StartBlockLoopMontage,
				Delay,
				false);
		}
	}
	else
	{
		StartBlockLoopMontage();
	}
}

void URpgGameplayAbility_Block::StartBlockLoopMontage()
{
	if (!IsActive() || bBlockInputReleased || bBlockLoopStarted || !ActiveBlockDefinition.BlockLoopMontage)
	{
		return;
	}

	bBlockLoopStarted = PlayBlockMontage(ActiveBlockDefinition.BlockLoopMontage, MontagePlayRate) > 0.0f;
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

bool URpgGameplayAbility_Block::ApplyBlockState(const FRpgWeaponBlockDefinition& BlockDefinition)
{
	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo();
	const URpgDefenseSet* DefenseSet = ASC ? ASC->GetSet<URpgDefenseSet>() : nullptr;
	if (!DefenseSet)
	{
		return false;
	}

	BlockStateASC = ASC;
	BlockStateDefenseSet = DefenseSet;
	PreviousBlockAngleDegrees = ASC->GetNumericAttributeBase(URpgDefenseSet::GetBlockAngleDegreesAttribute());
	PreviousBlockStaminaCost = ASC->GetNumericAttributeBase(URpgDefenseSet::GetBlockStaminaCostAttribute());
	PreviousBlockDamageReduction = ASC->GetNumericAttributeBase(URpgDefenseSet::GetBlockDamageReductionAttribute());
	PreviousBlockStaggerDamageMultiplier = ASC->GetNumericAttributeBase(URpgDefenseSet::GetBlockStaggerDamageMultiplierAttribute());
	PreviousPerfectBlockStaminaRestore = ASC->GetNumericAttributeBase(URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute());
	PreviousPerfectBlockStaggerDamage = ASC->GetNumericAttributeBase(URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute());

	const auto OwnsAttributes = [this, ASC, DefenseSet]()
	{
		return IsActive() && !bEndingBlock && BlockStateASC.Get() == ASC
			&& BlockStateDefenseSet.Get() == DefenseSet && ASC->GetSet<URpgDefenseSet>() == DefenseSet;
	};
	const auto ApplyAttribute = [ASC, &OwnsAttributes](FGameplayAttribute Attribute, float Value)
	{
		if (!OwnsAttributes()) return false;
		ASC->SetNumericAttributeBase(Attribute, Value);
		return true;
	};
	// Attribute/tag listeners may synchronously end the ability or remove its feature while applying state.
	if (!ApplyAttribute(URpgDefenseSet::GetBlockAngleDegreesAttribute(), BlockDefinition.BlockAngleDegrees)
		|| !ApplyAttribute(URpgDefenseSet::GetBlockStaminaCostAttribute(), BlockDefinition.StaminaCost)
		|| !ApplyAttribute(URpgDefenseSet::GetBlockDamageReductionAttribute(), BlockDefinition.DamageReduction)
		|| !ApplyAttribute(URpgDefenseSet::GetBlockStaggerDamageMultiplierAttribute(), BlockDefinition.BlockStaggerDamageMultiplier)
		|| !ApplyAttribute(URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute(), BlockDefinition.PerfectBlockStaminaRestore)
		|| !ApplyAttribute(URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute(),
			BlockDefinition.PerfectBlockStaggerDamage * FMath::Max(0.0f, BlockDefinition.PerfectBlockStaggerDamageMultiplier))
		|| !OwnsAttributes())
	{
		return false;
	}

	bAppliedBlockState = true;
	SetReplicatedLooseTagCount(RpgGameplayTags::State_Blocking, 1);
	if (!OwnsAttributes()) return false;

	if (BlockDefinition.bAllowPerfectBlock && BlockDefinition.PerfectBlockWindow > 0.0f)
	{
		SetReplicatedLooseTagCount(RpgGameplayTags::State_PerfectBlockWindow, 1);
		if (!OwnsAttributes()) return false;

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
	return OwnsAttributes();
}

void URpgGameplayAbility_Block::ClearBlockState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PerfectBlockTimerHandle);
		World->GetTimerManager().ClearTimer(BlockLoopTimerHandle);
	}

	const TWeakObjectPtr<URpgAbilitySystemComponent> SavedASC = BlockStateASC;
	const TWeakObjectPtr<const URpgDefenseSet> SavedDefenseSet = BlockStateDefenseSet;
	const TPair<FGameplayAttribute, float> SavedBases[] = {
		{ URpgDefenseSet::GetBlockAngleDegreesAttribute(), PreviousBlockAngleDegrees },
		{ URpgDefenseSet::GetBlockStaminaCostAttribute(), PreviousBlockStaminaCost },
		{ URpgDefenseSet::GetBlockDamageReductionAttribute(), PreviousBlockDamageReduction },
		{ URpgDefenseSet::GetBlockStaggerDamageMultiplierAttribute(), PreviousBlockStaggerDamageMultiplier },
		{ URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute(), PreviousPerfectBlockStaminaRestore },
		{ URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute(), PreviousPerfectBlockStaggerDamage }
	};
	// Consume ownership before callbacks, including when the original ASC has already gone away.
	BlockStateASC.Reset();
	BlockStateDefenseSet.Reset();
	bAppliedBlockState = false;

	if (URpgAbilitySystemComponent* ASC = SavedASC.Get())
	{
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Blocking, 0, EGameplayTagReplicationState::TagAndCountToAll);
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_PerfectBlockWindow, 0, EGameplayTagReplicationState::TagAndCountToAll);
	}
	for (const TPair<FGameplayAttribute, float>& SavedBase : SavedBases)
	{
		URpgAbilitySystemComponent* ASC = SavedASC.Get();
		// Recheck after every tag/attribute callback; a same-class replacement does not own this snapshot.
		if (!ASC || !SavedDefenseSet.IsValid() || ASC->GetSet<URpgDefenseSet>() != SavedDefenseSet.Get()) break;
		ASC->SetNumericAttributeBase(SavedBase.Key, SavedBase.Value);
	}
}

void URpgGameplayAbility_Block::SetReplicatedLooseTagCount(FGameplayTag Tag, int32 Count) const
{
	if (URpgAbilitySystemComponent* ASC = BlockStateASC.Get())
	{
		ASC->SetLooseGameplayTagCount(Tag, Count, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

float URpgGameplayAbility_Block::PlayBlockMontage(UAnimMontage* Montage, float PlayRate)
{
	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponentFromActorInfo();
	if (!Montage || !ASC)
	{
		return 0.0f;
	}

	const float ActualPlayRate = FMath::Max(0.01f, PlayRate);
	if (ASC->GetCurrentMontage() == Montage)
	{
		if (CVarRpgCombatBlockAnimationDebug.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("BlockAbility[%s]: montage %s already current, skipping replay."),
				*GetNameSafe(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr),
				*GetNameSafe(Montage));
		}

		return Montage->GetPlayLength() / ActualPlayRate;
	}

	const float PlayedDuration = ASC->PlayMontage(this, CurrentActivationInfo, Montage, ActualPlayRate);
	if (CVarRpgCombatBlockAnimationDebug.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BlockAbility[%s]: play montage %s duration %.3f."),
			*GetNameSafe(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr),
			*GetNameSafe(Montage),
			PlayedDuration);
	}

	return PlayedDuration > 0.0f ? PlayedDuration : (Montage->GetPlayLength() / ActualPlayRate);
}
