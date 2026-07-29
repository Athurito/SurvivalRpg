#include "Harvesting/RpgHarvestableCorpseComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/RpgGameplayAbility_HarvestCorpse.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgCorpseHarvestProfile.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestToolSelection.h"
#include "Misc/DataValidation.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseProfile.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestableCorpseComponent)

namespace
{
	bool IsLivingHarvester(AActor* Harvester)
	{
		if (!IsValid(Harvester))
		{
			return false;
		}
		const ARpgPlayerState* PlayerState =
			FRpgHarvestRewardService::ResolveHarvesterPlayerState(Harvester);
		const UAbilitySystemComponent* AbilitySystem = PlayerState
			? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState)
			: nullptr;
		return !AbilitySystem ||
			!AbilitySystem->HasMatchingGameplayTag(RpgGameplayTags::Status_Death);
	}
}

URpgHarvestableCorpseComponent::URpgHarvestableCorpseComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgHarvestableCorpseComponent::BeginPlay()
{
	Super::BeginPlay();

	CorpseLifecycle = GetOwner()
		? GetOwner()->FindComponentByClass<URpgCorpseLifecycleComponent>()
		: nullptr;
	if (CorpseLifecycle)
	{
		CorpseLifecycle->OnCorpseAvailabilityChangedNative().AddUObject(
			this,
			&ThisClass::HandleCorpseAvailabilityChanged);
	}
	BroadcastStateChanged();
}

void URpgHarvestableCorpseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* EndingHarvester = ReservedHarvester.Get();
	const int32 EndingRevision = HarvestState.Revision;
	const bool bHadReservation = HarvestState.bReserved;
	if (CorpseLifecycle)
	{
		CorpseLifecycle->OnCorpseAvailabilityChangedNative().RemoveAll(this);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReservationTimeoutHandle);
	}
	ReservedHarvester.Reset();
	ReservedToolItemId.Reset();
	HarvestState.bReserved = false;
	if (bHadReservation && EndingHarvester)
	{
		HarvestReservationEndedNative.Broadcast(this, EndingHarvester, EndingRevision);
	}
	HarvestReservationEndedNative.Clear();
	Super::EndPlay(EndPlayReason);
}

void URpgHarvestableCorpseComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, HarvestState);
}

void URpgHarvestableCorpseComponent::GatherInteractionOptions(
	const FInteractionQuery& InteractQuery,
	FInteractionOptionBuilder& OptionBuilder)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !HarvestProfile || !CorpseLifecycle || !IsHarvestAvailable())
	{
		return;
	}
	if (InteractQuery.QueryMode != ERpgInteractionQueryMode::Nearby &&
		InteractQuery.CandidateHit.GetComponent() != CorpseLifecycle)
	{
		return;
	}

	FInteractionOption Option;
	Option.InteractionTag = RpgHarvestingMagicGameplayTags::Rpg_Interaction_Action_Harvest_Corpse;
	Option.TargetRef.TargetActor = OwningActor;
	Option.TargetRef.TargetComponent = CorpseLifecycle;
	Option.TargetRef.WorldLocation = CorpseLifecycle->GetInteractionWorldLocation();
	Option.TargetRef.WorldNormal = FVector::UpVector;
	Option.TargetRef.Revision = HarvestState.Revision;
	Option.Prompt = HarvestProfile->InteractionPrompt;
	Option.Prompt.SanitizeRanges();
	Option.InteractionAbilityToGrant = URpgGameplayAbility_HarvestCorpse::StaticClass();
	Option.Availability = ERpgInteractionAvailability::Available;

	AActor* Harvester = InteractQuery.RequestingAvatar.Get();
	if (!FRpgHarvestRewardService::MeetsSkillGate(HarvestProfile, Harvester))
	{
		Option.Availability = ERpgInteractionAvailability::Blocked;
		Option.Prompt.BlockedReason = HarvestProfile->InsufficientSkillReason;
	}
	else
	{
		const URpgInventoryManagerComponent* Inventory = ResolveHarvesterInventory(Harvester);
		if (!FRpgHarvestToolSelection::FindBestOwnedTool(
			Inventory,
			HarvestProfile->RequiredToolTag).IsValid())
		{
			Option.Availability = ERpgInteractionAvailability::Blocked;
			Option.Prompt.BlockedReason = HarvestProfile->MissingToolReason;
		}
	}
	OptionBuilder.AddInteractionOption(Option);
}

bool URpgHarvestableCorpseComponent::IsHarvestAvailable() const
{
	return CorpseLifecycle && CorpseLifecycle->IsCorpseAvailable() &&
		!HarvestState.bReserved && !HarvestState.bCompleted;
}

bool URpgHarvestableCorpseComponent::CanBeginHarvest(
	AActor* Harvester,
	const int32 ExpectedRevision,
	const FRpgInventoryItemId ToolItemId) const
{
	const AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || !HarvestProfile ||
		!IsLivingHarvester(Harvester) ||
		ExpectedRevision != HarvestState.Revision || !IsHarvestAvailable() ||
		!IsHarvesterInRange(Harvester) ||
		!FRpgHarvestRewardService::MeetsSkillGate(HarvestProfile, Harvester))
	{
		return false;
	}

	const URpgInventoryManagerComponent* Inventory = ResolveHarvesterInventory(Harvester);
	return FRpgHarvestToolSelection::FindOwnedToolById(
		Inventory,
		ToolItemId,
		HarvestProfile->RequiredToolTag).IsValid();
}

bool URpgHarvestableCorpseComponent::TryReserveHarvest(
	AActor* Harvester,
	const int32 ExpectedRevision,
	const FRpgInventoryItemId ToolItemId,
	int32& OutReservationRevision)
{
	OutReservationRevision = INDEX_NONE;
	if (!CanBeginHarvest(Harvester, ExpectedRevision, ToolItemId))
	{
		return false;
	}

	ReservedHarvester = Harvester;
	ReservedToolItemId = ToolItemId;
	SetHarvestState(true, false, false);
	OutReservationRevision = HarvestState.Revision;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReservationTimeoutHandle,
			this,
			&ThisClass::HandleReservationTimeout,
			FMath::Max(0.1f, HarvestProfile->ReservationTimeoutSeconds),
			false);
	}
	TWeakObjectPtr<URpgHarvestableCorpseComponent> WeakThis(this);
	BroadcastStateChanged();
	return WeakThis.IsValid() && IsValid(WeakThis->GetOwner()) &&
		WeakThis->IsMatchingReservation(Harvester, OutReservationRevision);
}

bool URpgHarvestableCorpseComponent::TryCommitReservedHarvest(
	AActor* Harvester,
	const int32 ReservationRevision,
	const FRpgInventoryItemId ToolItemId)
{
	AActor* OwningActor = GetOwner();
	if (bCommitInProgress || !OwningActor || !OwningActor->HasAuthority() ||
		!HarvestProfile || !CorpseLifecycle ||
		!IsLivingHarvester(Harvester) ||
		!CorpseLifecycle->IsCorpseAvailable() || !IsMatchingReservation(Harvester, ReservationRevision) ||
		ToolItemId != ReservedToolItemId || !IsHarvesterInRange(Harvester) ||
		!FRpgHarvestRewardService::MeetsSkillGate(HarvestProfile, Harvester))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = ResolveHarvesterInventory(Harvester);
	const FRpgSelectedHarvestTool Tool = FRpgHarvestToolSelection::FindOwnedToolById(
		Inventory,
		ToolItemId,
		HarvestProfile->RequiredToolTag);
	if (!Tool.IsValid())
	{
		return false;
	}

	FRpgHarvestRewardRequest RewardRequest;
	RewardRequest.SourceActor = OwningActor;
	RewardRequest.Harvester = Harvester;
	RewardRequest.DeliveryTransform = FTransform(
		FRotator::ZeroRotator,
		CorpseLifecycle->GetInteractionWorldLocation());
	RewardRequest.HarvestPower = Tool.HarvestPower;
	RewardRequest.SeedSalt = ReservationRevision;
	TWeakObjectPtr<URpgHarvestableCorpseComponent> WeakThis(this);
	TWeakObjectPtr<AActor> WeakOwner(OwningActor);
	TGuardValue<bool> CommitGuard(bCommitInProgress, true);
	if (FRpgHarvestRewardService::DeliverReward(HarvestProfile, RewardRequest) ==
		ERpgHarvestRewardDeliveryResult::Failed)
	{
		return false;
	}
	if (!WeakThis.IsValid() || !WeakOwner.IsValid() ||
		WeakThis->GetOwner() != WeakOwner.Get())
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReservationTimeoutHandle);
	}
	ReservedHarvester.Reset();
	ReservedToolItemId.Reset();
	SetHarvestState(false, true, false);
	TWeakObjectPtr<URpgCorpseLifecycleComponent> WeakLifecycle(CorpseLifecycle);
	const FGameplayTag CompletionTag = HarvestProfile->CorpseCompletionTag;
	const FString ComponentPath = GetPathName();
	FRpgHarvestRewardService::AwardExperience(HarvestProfile, Harvester);

	if (WeakLifecycle.IsValid() &&
		!WeakLifecycle->CompleteExternalRequirement(CompletionTag))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s delivered its corpse-harvest reward, but lifecycle requirement %s was not configured or was already complete."),
			*ComponentPath,
			*CompletionTag.ToString());
	}
	// Core availability delegates and progression delegates are external/reentrant. Never dereference
	// this component after them unless the UObject and owner are both still valid.
	if (WeakThis.IsValid() && IsValid(WeakThis->GetOwner()))
	{
		WeakThis->BroadcastStateChanged();
		if (WeakThis.IsValid() && IsValid(WeakThis->GetOwner()))
		{
			WeakThis->OnCorpseHarvestCompleted.Broadcast(Harvester, ToolItemId);
		}
	}
	return true;
}

void URpgHarvestableCorpseComponent::CancelHarvestReservation(
	AActor* Harvester,
	const int32 ReservationRevision)
{
	AActor* OwningActor = GetOwner();
	if (bCommitInProgress || !OwningActor || !OwningActor->HasAuthority() ||
		!IsMatchingReservation(Harvester, ReservationRevision))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReservationTimeoutHandle);
	}
	AActor* EndingHarvester = ReservedHarvester.Get();
	const int32 EndingRevision = HarvestState.Revision;
	ReservedHarvester.Reset();
	ReservedToolItemId.Reset();
	SetHarvestState(false, false, false);
	TWeakObjectPtr<URpgHarvestableCorpseComponent> WeakThis(this);
	HarvestReservationEndedNative.Broadcast(this, EndingHarvester, EndingRevision);
	if (WeakThis.IsValid() && IsValid(WeakThis->GetOwner()))
	{
		WeakThis->BroadcastStateChanged();
	}
}

void URpgHarvestableCorpseComponent::OnRep_HarvestState()
{
	BroadcastStateChanged();
}

void URpgHarvestableCorpseComponent::HandleCorpseAvailabilityChanged(
	URpgCorpseLifecycleComponent* Component,
	const bool bIsAvailable)
{
	if (Component != CorpseLifecycle)
	{
		return;
	}
	if (!bIsAvailable && GetOwner() && GetOwner()->HasAuthority() && HarvestState.bReserved)
	{
		CancelHarvestReservation(ReservedHarvester.Get(), HarvestState.Revision);
		return;
	}
	BroadcastStateChanged();
}

void URpgHarvestableCorpseComponent::HandleReservationTimeout()
{
	CancelHarvestReservation(ReservedHarvester.Get(), HarvestState.Revision);
}

void URpgHarvestableCorpseComponent::SetHarvestState(
	const bool bReserved,
	const bool bCompleted,
	const bool bBroadcast)
{
	HarvestState.Revision = HarvestState.Revision >= MAX_int32
		? 1
		: FMath::Max(1, HarvestState.Revision + 1);
	HarvestState.bReserved = bReserved;
	HarvestState.bCompleted = bCompleted;
	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->FlushNetDormancy();
		OwningActor->ForceNetUpdate();
	}
	if (bBroadcast)
	{
		BroadcastStateChanged();
	}
}

bool URpgHarvestableCorpseComponent::IsMatchingReservation(
	AActor* Harvester,
	const int32 ReservationRevision) const
{
	const bool bMatchesKnownHarvester = Harvester
		? ReservedHarvester.Get() == Harvester
		: !ReservedHarvester.IsValid();
	return HarvestState.bReserved && !HarvestState.bCompleted && bMatchesKnownHarvester &&
		HarvestState.Revision == ReservationRevision;
}

bool URpgHarvestableCorpseComponent::IsHarvesterInRange(AActor* Harvester) const
{
	if (!Harvester || !HarvestProfile || !CorpseLifecycle)
	{
		return false;
	}
	FRpgInteractionPromptDefinition Prompt = HarvestProfile->InteractionPrompt;
	Prompt.SanitizeRanges();
	return FVector::DistSquared(
		Harvester->GetActorLocation(),
		CorpseLifecycle->GetInteractionWorldLocation()) <= FMath::Square(Prompt.InteractionRange);
}

URpgInventoryManagerComponent* URpgHarvestableCorpseComponent::ResolveHarvesterInventory(
	AActor* Harvester) const
{
	ARpgPlayerState* PlayerState = FRpgHarvestRewardService::ResolveHarvesterPlayerState(Harvester);
	return PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
}

void URpgHarvestableCorpseComponent::BroadcastStateChanged()
{
	OnHarvestStateChanged.Broadcast(HarvestState, IsHarvestAvailable());
}

#if WITH_EDITOR
EDataValidationResult URpgHarvestableCorpseComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!HarvestProfile)
	{
		Context.AddError(NSLOCTEXT(
			"RpgHarvestableCorpse",
			"MissingProfile",
			"A harvestable corpse component requires a CorpseHarvestProfile."));
		Result = EDataValidationResult::Invalid;
	}
	const AActor* OwningActor = GetOwner();
	const URpgCorpseLifecycleComponent* Lifecycle = OwningActor
		? OwningActor->FindComponentByClass<URpgCorpseLifecycleComponent>()
		: nullptr;
	if (OwningActor && !Lifecycle)
	{
		Context.AddError(NSLOCTEXT(
			"RpgHarvestableCorpse",
			"MissingLifecycle",
			"A harvestable corpse actor also requires an RPG Corpse Lifecycle component."));
		Result = EDataValidationResult::Invalid;
	}
	else if (Lifecycle && HarvestProfile)
	{
		if (!Lifecycle->CorpseProfile)
		{
			Context.AddError(NSLOCTEXT(
				"RpgHarvestableCorpse",
				"MissingCorpseProfileForCompletion",
				"A harvestable corpse requires an explicit CorpseProfile containing its harvest completion tag."));
			Result = EDataValidationResult::Invalid;
		}
		else if (!Lifecycle->CorpseProfile->RequiredExternalCompletionTags.HasTagExact(
			HarvestProfile->CorpseCompletionTag))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT(
					"RpgHarvestableCorpse",
					"MissingHarvestCompletionRequirement",
					"CorpseProfile.RequiredExternalCompletionTags must contain {0}; otherwise successful harvesting cannot complete the corpse."),
				FText::FromString(HarvestProfile->CorpseCompletionTag.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
