// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractionPromptData.h"

#include "SurvivalRpg/Interaction/InteractionOption.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractionPromptData)

bool URpgInteractionPromptData::UpdateFromOption(
	const FInteractionOption& Option,
	ERpgInteractionPromptState NewState)
{
	const FText& NewActionText = Option.Prompt.ActionText.IsEmpty()
		? Option.Text
		: Option.Prompt.ActionText;
	const FText& NewTargetText = Option.Prompt.TargetText.IsEmpty()
		? Option.SubText
		: Option.Prompt.TargetText;
	const bool bSameTargetIdentity =
		TargetRef.TargetActor == Option.TargetRef.TargetActor &&
		TargetRef.TargetComponent == Option.TargetRef.TargetComponent &&
		TargetRef.InstanceIndex == Option.TargetRef.InstanceIndex &&
		TargetRef.Revision == Option.TargetRef.Revision;

	const bool bChanged =
		State != NewState ||
		InteractionTag != Option.InteractionTag ||
		!bSameTargetIdentity ||
		!ActionText.IdenticalTo(NewActionText) ||
		!TargetText.IdenticalTo(NewTargetText) ||
		!BlockedReason.IdenticalTo(Option.Prompt.BlockedReason) ||
		Icon != Option.Prompt.Icon ||
		Availability != Option.Availability;

	// Moving actors and slightly different trace hit points still update the model, but do not
	// invalidate widget presentation or restart soft-icon work.
	TargetRef = Option.TargetRef;
	if (!bChanged)
	{
		return false;
	}

	State = NewState;
	InteractionTag = Option.InteractionTag;
	ActionText = NewActionText;
	TargetText = NewTargetText;
	BlockedReason = Option.Prompt.BlockedReason;
	Icon = Option.Prompt.Icon;
	Availability = Option.Availability;
	BroadcastChanged();
	return true;
}

void URpgInteractionPromptData::Clear()
{
	const FRpgInteractionTargetRef EmptyTargetRef;
	const bool bChanged =
		State != ERpgInteractionPromptState::Hidden ||
		InteractionTag.IsValid() ||
		!TargetRef.IsSemanticallyEqual(EmptyTargetRef) ||
		!ActionText.IsEmpty() ||
		!TargetText.IsEmpty() ||
		!BlockedReason.IsEmpty() ||
		!Icon.IsNull() ||
		Availability != ERpgInteractionAvailability::Hidden;

	if (!bChanged)
	{
		return;
	}

	State = ERpgInteractionPromptState::Hidden;
	InteractionTag = FGameplayTag();
	TargetRef = EmptyTargetRef;
	ActionText = FText::GetEmpty();
	TargetText = FText::GetEmpty();
	BlockedReason = FText::GetEmpty();
	Icon.Reset();
	Availability = ERpgInteractionAvailability::Hidden;
	BroadcastChanged();
}

void URpgInteractionPromptData::BroadcastChanged()
{
	PromptChangedNative.Broadcast(this);
	OnPromptChanged.Broadcast(this);
}
