#include "RpgGameplayAbility_ActivateWeaponSet.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/AnimNotify_RpgWeaponToolPresentation.h"
#include "SurvivalRpg/Equipment/RpgEquipmentComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponPresentationComponent.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Visual.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"

URpgGameplayAbility_ActivateWeaponSet::URpgGameplayAbility_ActivateWeaponSet()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationOwnedTags.AddTag(RpgGameplayTags::Status_EquipTransition);
	ActivationBlockedTags.AddTag(RpgGameplayTags::Status_EquipTransition);
}

void URpgGameplayAbility_ActivateWeaponSet::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ResetEquipTasks();

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	URpgEquipmentComponent* EquipmentComponent = ResolveEquipmentComponent(ActorInfo);
	if (EquipmentComponent == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 PreviousActiveWeaponSetIndex = EquipmentComponent->GetActiveWeaponSetIndex();
	const bool bHolsterCurrentSet = PreviousActiveWeaponSetIndex == WeaponSetIndex;
	ExpectedVisibleWeaponSetIndex = bHolsterCurrentSet ? INDEX_NONE : WeaponSetIndex;

	if (ActorInfo != nullptr && ActorInfo->IsNetAuthority())
	{
		const bool bStateChanged = bHolsterCurrentSet
			? EquipmentComponent->ClearActiveWeaponSet()
			: EquipmentComponent->SetActiveWeaponSet(WeaponSetIndex);

		if (!bStateChanged)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	if (!ShouldDrivePresentationLocally(ActorInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	URpgWeaponPresentationComponent* PresentationComponent = ResolvePresentationComponent(ActorInfo);
	if (PresentationComponent == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const bool bUseEquipMontage = !bHolsterCurrentSet;
	const int32 MontageWeaponSetIndex = bUseEquipMontage ? WeaponSetIndex : PreviousActiveWeaponSetIndex;
	UAnimMontage* MontageToPlay = ResolvePresentationMontage(EquipmentComponent, MontageWeaponSetIndex, bUseEquipMontage);
	const bool bUsesPresentationNotify = MontageUsesPresentationNotify(MontageToPlay);

	if (!bUsesPresentationNotify)
	{
		ApplyPredictedVisibleState();
	}

	if (MontageToPlay == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	StartWaitingForEquipEvent(RpgGameplayTags::GameplayEvent_Equip_ApplyCurrentState);
	StartWaitingForEquipEvent(RpgGameplayTags::GameplayEvent_Equip_HolsterVisible);
	StartWaitingForEquipEvent(RpgGameplayTags::GameplayEvent_Equip_DrawActiveSet);

	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay);
	if (ActiveMontageTask == nullptr)
	{
		ApplyPredictedVisibleState();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	ActiveMontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnEquipMontageCompleted);
	ActiveMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnEquipMontageInterrupted);
	ActiveMontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnEquipMontageCancelled);
	ActiveMontageTask->ReadyForActivation();
}

void URpgGameplayAbility_ActivateWeaponSet::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ResetEquipTasks();

	if (bWasCancelled)
	{
		if (URpgWeaponPresentationComponent* PresentationComponent = ResolvePresentationComponent(ActorInfo))
		{
			PresentationComponent->SyncToAuthoritativeState();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_ActivateWeaponSet::OnEquipGameplayEventReceived(FGameplayEventData Payload)
{
	URpgWeaponPresentationComponent* PresentationComponent = ResolvePresentationComponent(CurrentActorInfo);
	if (PresentationComponent == nullptr)
	{
		return;
	}

	if (Payload.EventTag == RpgGameplayTags::GameplayEvent_Equip_HolsterVisible)
	{
		PresentationComponent->ShowHolstered();
		return;
	}

	if (Payload.EventTag == RpgGameplayTags::GameplayEvent_Equip_DrawActiveSet)
	{
		PresentationComponent->ShowWeaponSet(ExpectedVisibleWeaponSetIndex);
		return;
	}

	if (Payload.EventTag == RpgGameplayTags::GameplayEvent_Equip_ApplyCurrentState)
	{
		PresentationComponent->SyncToAuthoritativeState();
	}
}

void URpgGameplayAbility_ActivateWeaponSet::OnEquipMontageCompleted()
{
	ApplyPredictedVisibleState();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URpgGameplayAbility_ActivateWeaponSet::OnEquipMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URpgGameplayAbility_ActivateWeaponSet::OnEquipMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool URpgGameplayAbility_ActivateWeaponSet::ShouldDrivePresentationLocally(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	return AvatarPawn != nullptr && AvatarPawn->GetNetMode() != NM_DedicatedServer && ActorInfo->IsLocallyControlled();
}

void URpgGameplayAbility_ActivateWeaponSet::StartWaitingForEquipEvent(FGameplayTag EventTag)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag, nullptr, false, true);
	if (EventTask == nullptr)
	{
		return;
	}

	EventTask->EventReceived.AddDynamic(this, &ThisClass::OnEquipGameplayEventReceived);
	ActiveEquipEventTasks.Add(EventTask);
	EventTask->ReadyForActivation();
}

void URpgGameplayAbility_ActivateWeaponSet::ResetEquipTasks()
{
	if (ActiveMontageTask != nullptr)
	{
		ActiveMontageTask->EndTask();
		ActiveMontageTask = nullptr;
	}

	for (UAbilityTask_WaitGameplayEvent* EventTask : ActiveEquipEventTasks)
	{
		if (EventTask != nullptr)
		{
			EventTask->EndTask();
		}
	}

	ActiveEquipEventTasks.Reset();
}

URpgEquipmentComponent* URpgGameplayAbility_ActivateWeaponSet::ResolveEquipmentComponent(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (const AActor* OwnerActor = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr)
	{
		if (const ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(OwnerActor))
		{
			return PlayerState->GetEquipmentComponent();
		}
	}

	return nullptr;
}

URpgWeaponPresentationComponent* URpgGameplayAbility_ActivateWeaponSet::ResolvePresentationComponent(const FGameplayAbilityActorInfo* ActorInfo) const
{
	APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	return AvatarPawn ? AvatarPawn->FindComponentByClass<URpgWeaponPresentationComponent>() : nullptr;
}

const URpgItemFragment_Visual* URpgGameplayAbility_ActivateWeaponSet::GetPrimaryPresentationVisualFragmentForWeaponSet(const URpgEquipmentComponent* EquipmentComponent, int32 InWeaponSetIndex) const
{
	if (EquipmentComponent == nullptr)
	{
		return nullptr;
	}

	const FRpgEquippedWeaponSet WeaponSet = EquipmentComponent->GetWeaponSet(InWeaponSetIndex);
	URpgItemInstance* PresentationItem = WeaponSet.MainHandItem != nullptr ? WeaponSet.MainHandItem : WeaponSet.OffHandItem;
	return PresentationItem ? PresentationItem->FindFragmentByClass<URpgItemFragment_Visual>() : nullptr;
}

UAnimMontage* URpgGameplayAbility_ActivateWeaponSet::ResolvePresentationMontage(const URpgEquipmentComponent* EquipmentComponent, int32 InWeaponSetIndex, bool bUseEquipMontage) const
{
	const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(EquipmentComponent, InWeaponSetIndex);
	if (VisualFragment == nullptr)
	{
		return nullptr;
	}

	return bUseEquipMontage ? VisualFragment->GetEquipMontage() : VisualFragment->GetUnequipMontage();
}

bool URpgGameplayAbility_ActivateWeaponSet::MontageUsesPresentationNotify(const UAnimMontage* Montage) const
{
	if (Montage == nullptr)
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.Notify != nullptr && NotifyEvent.Notify->IsA<UAnimNotify_RpgWeaponToolPresentation>())
		{
			return true;
		}
	}

	return false;
}

void URpgGameplayAbility_ActivateWeaponSet::ApplyPredictedVisibleState() const
{
	if (URpgWeaponPresentationComponent* PresentationComponent = ResolvePresentationComponent(CurrentActorInfo))
	{
		if (ExpectedVisibleWeaponSetIndex == INDEX_NONE)
		{
			PresentationComponent->ShowHolstered();
		}
		else
		{
			PresentationComponent->ShowWeaponSet(ExpectedVisibleWeaponSetIndex);
		}
	}
}
