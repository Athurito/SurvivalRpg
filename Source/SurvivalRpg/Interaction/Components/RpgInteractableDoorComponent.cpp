// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractableDoorComponent.h"

#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_ExecuteInteraction.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractableDoorComponent)

namespace
{
	void ConfigureDoorOption(
		FInteractionOption& Option,
		const FGameplayTag& InteractionTag,
		const FText& ActionText)
	{
		Option.InteractionTag = InteractionTag;
		Option.Prompt.ActionText = ActionText;
		Option.Prompt.TargetText = NSLOCTEXT("RpgInteraction", "DoorTarget", "Door");
		Option.Prompt.AwarenessRange = 800.0f;
		Option.Prompt.FocusRange = 500.0f;
		Option.Prompt.InteractionRange = 300.0f;
		Option.Prompt.InteractionPriority = 60;
		Option.InteractionAbilityToGrant = URpgGameplayAbility_ExecuteInteraction::StaticClass();
	}
}

URpgInteractableDoorComponent::URpgInteractableDoorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	ConfigureDoorOption(
		OpenInteractionOption,
		RpgGameplayTags::Rpg_Interaction_Action_Door_Open,
		NSLOCTEXT("RpgInteraction", "OpenDoorAction", "Open"));
	ConfigureDoorOption(
		CloseInteractionOption,
		RpgGameplayTags::Rpg_Interaction_Action_Door_Close,
		NSLOCTEXT("RpgInteraction", "CloseDoorAction", "Close"));
	ConfigureDoorOption(
		UnlockInteractionOption,
		RpgGameplayTags::Rpg_Interaction_Action_Door_Unlock,
		NSLOCTEXT("RpgInteraction", "UnlockDoorAction", "Unlock"));
}

void URpgInteractableDoorComponent::BeginPlay()
{
	Super::BeginPlay();
	NotifyPresentationStateChanged();
}

void URpgInteractableDoorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bIsOpen);
	DOREPLIFETIME(ThisClass, bIsLocked);
	DOREPLIFETIME(ThisClass, DoorStateRevision);
}

void URpgInteractableDoorComponent::GatherInteractionOptions(
	const FInteractionQuery& InteractQuery,
	FInteractionOptionBuilder& InteractionBuilder)
{
	if (!GetOwner())
	{
		return;
	}

	FInteractionOption Option;
	if (bIsLocked && bAllowInteractionUnlock)
	{
		Option = UnlockInteractionOption;
		if (!CanRequesterUnlock(InteractQuery.RequestingAvatar.Get()))
		{
			Option.Availability = ERpgInteractionAvailability::Blocked;
			Option.Prompt.BlockedReason = NSLOCTEXT("RpgInteraction", "DoorUnlockRequirementMissing", "A required key or condition is missing");
		}
	}
	else if (bIsOpen)
	{
		Option = CloseInteractionOption;
	}
	else
	{
		Option = OpenInteractionOption;
		if (bIsLocked)
		{
			Option.Availability = ERpgInteractionAvailability::Blocked;
			Option.Prompt.BlockedReason = NSLOCTEXT("RpgInteraction", "DoorLocked", "Locked");
		}
	}

	Option.TargetRef.TargetActor = GetOwner();
	Option.TargetRef.Revision = DoorStateRevision;
	InteractionBuilder.AddInteractionOption(Option);
}

bool URpgInteractableDoorComponent::CommitInteraction(
	const FInteractionQuery& AuthoritativeQuery,
	const FInteractionOption& ValidatedOption)
{
	AActor* OwnerActor = GetOwner();
	AActor* RequestingActor = AuthoritativeQuery.RequestingAvatar.Get();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !RequestingActor ||
		ValidatedOption.TargetRef.Revision != DoorStateRevision)
	{
		return false;
	}

	if (ValidatedOption.InteractionTag == RpgGameplayTags::Rpg_Interaction_Action_Door_Open)
	{
		return !bIsLocked && !bIsOpen && SetDoorOpen(true);
	}
	if (ValidatedOption.InteractionTag == RpgGameplayTags::Rpg_Interaction_Action_Door_Close)
	{
		return bIsOpen && SetDoorOpen(false);
	}
	if (ValidatedOption.InteractionTag == RpgGameplayTags::Rpg_Interaction_Action_Door_Unlock)
	{
		if (!bIsLocked || !bAllowInteractionUnlock || !CanRequesterUnlock(RequestingActor) ||
			!CommitUnlockRequirement(RequestingActor))
		{
			return false;
		}
		return SetDoorLocked(false);
	}

	return false;
}

bool URpgInteractableDoorComponent::SetDoorOpen(bool bNewOpen)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || (bNewOpen && bIsLocked))
	{
		return false;
	}
	if (bIsOpen == bNewOpen)
	{
		return true;
	}

	bIsOpen = bNewOpen;
	NotifyAuthoritativeStateChanged();
	return true;
}

bool URpgInteractableDoorComponent::SetDoorLocked(bool bNewLocked)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}
	const bool bStateChanged = bIsLocked != bNewLocked || (bNewLocked && bIsOpen);
	if (!bStateChanged)
	{
		return true;
	}

	bIsLocked = bNewLocked;
	if (bIsLocked)
	{
		bIsOpen = false;
	}
	NotifyAuthoritativeStateChanged();
	return true;
}

bool URpgInteractableDoorComponent::CanRequesterUnlock_Implementation(AActor* RequestingActor) const
{
	(void)RequestingActor;
	return false;
}

bool URpgInteractableDoorComponent::CommitUnlockRequirement_Implementation(AActor* RequestingActor)
{
	(void)RequestingActor;
	return false;
}

void URpgInteractableDoorComponent::OnRep_DoorState()
{
	NotifyPresentationStateChanged();
}

void URpgInteractableDoorComponent::NotifyAuthoritativeStateChanged()
{
	++DoorStateRevision;
	NotifyPresentationStateChanged();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void URpgInteractableDoorComponent::NotifyPresentationStateChanged()
{
	if (bHasPresentedState && bLastPresentedOpen == bIsOpen &&
		bLastPresentedLocked == bIsLocked && LastPresentedRevision == DoorStateRevision)
	{
		return;
	}
	bHasPresentedState = true;
	bLastPresentedOpen = bIsOpen;
	bLastPresentedLocked = bIsLocked;
	LastPresentedRevision = DoorStateRevision;
	OnDoorStateChanged.Broadcast(bIsOpen, bIsLocked);
	K2_OnDoorStateChanged(bIsOpen, bIsLocked);
}
