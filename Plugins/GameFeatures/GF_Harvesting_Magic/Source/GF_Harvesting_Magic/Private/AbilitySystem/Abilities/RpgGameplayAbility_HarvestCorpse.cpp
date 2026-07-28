#include "AbilitySystem/Abilities/RpgGameplayAbility_HarvestCorpse.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Controller.h"
#include "GameplayEffectTypes.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgCorpseHarvestProfile.h"
#include "Harvesting/RpgHarvestableCorpseComponent.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestToolSelection.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_HarvestCorpse)

URpgGameplayAbility_HarvestCorpse::URpgGameplayAbility_HarvestCorpse(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Blocking;
	AbilityDisplayName = NSLOCTEXT("RpgCorpseHarvest", "AbilityName", "Skinning");
	AbilityDescription = NSLOCTEXT(
		"RpgCorpseHarvest",
		"AbilityDescription",
		"Processes a settled animal corpse with the best matching tool in your inventory.");

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RpgHarvestingMagicGameplayTags::Ability_Harvesting_Skinning);
	SetAssetTags(AssetTags);
	ActivationBlockedTags.AddTag(RpgGameplayTags::Status_Death);
}

FGameplayTag URpgGameplayAbility_HarvestCorpse::GetHarvestCorpseAbilityId()
{
	return RpgHarvestingMagicGameplayTags::Ability_Harvesting_Skinning;
}

void URpgGameplayAbility_HarvestCorpse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	FInteractionOption ValidatedOption;
	FInteractionQuery AuthoritativeQuery;
	FText FailureReason;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ActorInfo->AvatarActor.IsValid() ||
		!UInteractionStatics::ValidateInteractionEventData(
			*ActorInfo,
			TriggerEventData,
			ValidatedOption,
			AuthoritativeQuery,
			FailureReason) ||
		ValidatedOption.InteractionTag !=
			RpgHarvestingMagicGameplayTags::Rpg_Interaction_Action_Harvest_Corpse)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	URpgHarvestableCorpseComponent* Corpse =
		Cast<URpgHarvestableCorpseComponent>(ValidatedOption.InteractableTarget.GetObject());
	const URpgCorpseHarvestProfile* Profile = Corpse ? Corpse->GetHarvestProfile() : nullptr;
	AActor* Harvester = ActorInfo->AvatarActor.Get();
	ARpgPlayerState* PlayerState = FRpgHarvestRewardService::ResolveHarvesterPlayerState(Harvester);
	URpgInventoryManagerComponent* Inventory =
		PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	const FRpgSelectedHarvestTool Tool = Profile
		? FRpgHarvestToolSelection::FindBestOwnedTool(Inventory, Profile->RequiredToolTag)
		: FRpgSelectedHarvestTool();
	if (!Corpse || !Profile || !Profile->HarvestMontage || !Profile->CommitEventTag.IsValid() ||
		!Tool.IsValid() ||
		!Corpse->CanBeginHarvest(Harvester, ValidatedOption.TargetRef.Revision, Tool.ItemId))
	{
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			ValidatedOption,
			Harvester,
			false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	int32 ReservationRevision = INDEX_NONE;
	if (!Corpse->TryReserveHarvest(
		Harvester,
		ValidatedOption.TargetRef.Revision,
		Tool.ItemId,
		ReservationRevision))
	{
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			ValidatedOption,
			Harvester,
			false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !IsActive() || !IsValid(Corpse) ||
		!Corpse->GetHarvestState().bReserved ||
		Corpse->GetHarvestRevision() != ReservationRevision)
	{
		if (IsValid(Corpse))
		{
			Corpse->CancelHarvestReservation(Harvester, ReservationRevision);
		}
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			ValidatedOption,
			Harvester,
			false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveCorpse = Corpse;
	ActiveInteractionOption = ValidatedOption;
	ActiveHarvester = Harvester;
	ActiveToolItemId = Tool.ItemId;
	ActiveReservationRevision = ReservationRevision;
	ActiveCorpse->OnHarvestReservationEndedNative().AddUObject(
		this,
		&ThisClass::HandleReservationEnded);
	bRewardCommitted = false;
	bFinishing = false;
	bReservationEndedExternally = false;
	BeginMovementBlock();
	BeginToolCue(Tool.ItemInstance);
	if (!IsActive() || !IsValid(ActiveCorpse))
	{
		return;
	}

	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Profile->CommitEventTag,
		nullptr,
		true,
		true);
	EventTask->EventReceived.AddDynamic(this, &ThisClass::HandleCommitEvent);
	EventTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			Profile->HarvestMontage,
			FMath::Max(0.01f, Profile->MontagePlayRate),
			Profile->MontageStartSection);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
}

void URpgGameplayAbility_HarvestCorpse::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (IsValid(ActiveCorpse))
	{
		ActiveCorpse->OnHarvestReservationEndedNative().RemoveAll(this);
	}
	if (IsValid(ActiveCorpse) && !bRewardCommitted && !bReservationEndedExternally)
	{
		ActiveCorpse->CancelHarvestReservation(
			ActiveHarvester.Get(),
			ActiveReservationRevision);
	}
	EndToolCue();
	EndMovementBlock();
	ActiveCorpse = nullptr;
	ActiveInteractionOption = FInteractionOption();
	ActiveHarvester.Reset();
	ActiveToolItemId.Reset();
	ActiveToolCueTag = FGameplayTag();
	ActiveReservationRevision = INDEX_NONE;
	bRewardCommitted = false;
	bFinishing = false;
	bReservationEndedExternally = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

void URpgGameplayAbility_HarvestCorpse::HandleCommitEvent(FGameplayEventData Payload)
{
	if (!IsActive() || bFinishing || bRewardCommitted || !ActiveCorpse ||
		!ActiveHarvester.IsValid())
	{
		return;
	}

	const URpgCorpseHarvestProfile* Profile = ActiveCorpse->GetHarvestProfile();
	if (!Profile || Payload.EventTag != Profile->CommitEventTag ||
		!ActiveCorpse->TryCommitReservedHarvest(
			ActiveHarvester.Get(),
			ActiveReservationRevision,
			ActiveToolItemId))
	{
		FinishHarvest(true);
		return;
	}
	bRewardCommitted = true;
}

void URpgGameplayAbility_HarvestCorpse::HandleMontageCompleted()
{
	if (!IsActive() || bFinishing)
	{
		return;
	}
	// A montage without the configured notify is always a failed/cancelled harvest.
	FinishHarvest(!bRewardCommitted);
}

void URpgGameplayAbility_HarvestCorpse::HandleMontageCancelled()
{
	if (!IsActive() || bFinishing)
	{
		return;
	}
	// Once the authored commit frame has delivered the reward it cannot be rolled back.
	FinishHarvest(!bRewardCommitted);
}

void URpgGameplayAbility_HarvestCorpse::HandleReservationEnded(
	URpgHarvestableCorpseComponent* Component,
	AActor* Harvester,
	const int32 ReservationRevision)
{
	if (!IsActive() || bFinishing || bRewardCommitted || Component != ActiveCorpse ||
		Harvester != ActiveHarvester.Get() || ReservationRevision != ActiveReservationRevision)
	{
		return;
	}
	bReservationEndedExternally = true;
	FinishHarvest(true);
}

void URpgGameplayAbility_HarvestCorpse::BeginMovementBlock()
{
	if (bMovementBlocked)
	{
		return;
	}
	bMovementBlocked = true;
	if (AController* Controller = GetControllerFromActorInfo())
	{
		Controller->SetIgnoreMoveInput(true);
	}
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTag MovementStopped =
			FGameplayTag::RequestGameplayTag(TEXT("Gameplay.MovementStopped"), false);
		if (MovementStopped.IsValid())
		{
			AbilitySystem->AddLooseGameplayTag(
				MovementStopped,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
}

void URpgGameplayAbility_HarvestCorpse::EndMovementBlock()
{
	if (!bMovementBlocked)
	{
		return;
	}
	bMovementBlocked = false;
	if (AController* Controller = GetControllerFromActorInfo())
	{
		Controller->SetIgnoreMoveInput(false);
	}
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTag MovementStopped =
			FGameplayTag::RequestGameplayTag(TEXT("Gameplay.MovementStopped"), false);
		if (MovementStopped.IsValid())
		{
			AbilitySystem->RemoveLooseGameplayTag(
				MovementStopped,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
}

void URpgGameplayAbility_HarvestCorpse::BeginToolCue(const UObject* SourceObject)
{
	const URpgCorpseHarvestProfile* Profile = ActiveCorpse
		? ActiveCorpse->GetHarvestProfile()
		: nullptr;
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!Profile || !Profile->ToolGameplayCue.IsValid() || !AbilitySystem)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = ActiveHarvester.Get();
	CueParameters.EffectCauser = ActiveHarvester.Get();
	CueParameters.SourceObject = SourceObject;
	CueParameters.Location = ActiveCorpse->GetCorpseLifecycle()
		? ActiveCorpse->GetCorpseLifecycle()->GetInteractionWorldLocation()
		: FVector::ZeroVector;
	ActiveToolCueTag = Profile->ToolGameplayCue;
	AbilitySystem->AddGameplayCue(Profile->ToolGameplayCue, CueParameters);
	bToolCueActive = true;
}

void URpgGameplayAbility_HarvestCorpse::EndToolCue()
{
	if (!bToolCueActive)
	{
		return;
	}
	bToolCueActive = false;
	if (ActiveToolCueTag.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
		{
			AbilitySystem->RemoveGameplayCue(ActiveToolCueTag);
		}
	}
	ActiveToolCueTag = FGameplayTag();
}

void URpgGameplayAbility_HarvestCorpse::FinishHarvest(const bool bWasCancelled)
{
	if (!IsActive() || bFinishing)
	{
		return;
	}
	bFinishing = true;
	UInteractionStatics::BroadcastInteractionMessage(
		this,
		bWasCancelled
			? RpgGameplayTags::Rpg_Interaction_Message_Rejected
			: RpgGameplayTags::Rpg_Interaction_Message_Ended,
		ActiveInteractionOption,
		ActiveHarvester.Get(),
		!bWasCancelled);
	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		true,
		bWasCancelled);
}
