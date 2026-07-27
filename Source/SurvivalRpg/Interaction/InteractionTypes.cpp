// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionTypes.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InteractionTypes)

bool FRpgInteractionTargetRef::IsValid() const
{
	return TargetActor.IsValid() && (InstanceIndex == INDEX_NONE || TargetComponent.IsValid());
}

bool FRpgInteractionTargetRef::IsSemanticallyEqual(const FRpgInteractionTargetRef& Other) const
{
	return TargetActor == Other.TargetActor &&
		TargetComponent == Other.TargetComponent &&
		InstanceIndex == Other.InstanceIndex &&
		WorldLocation.Equals(Other.WorldLocation, 0.1f) &&
		WorldNormal.Equals(Other.WorldNormal, 0.001f) &&
		Revision == Other.Revision;
}

void FRpgInteractionPromptDefinition::SanitizeRanges()
{
	AwarenessRange = FMath::Max(0.0f, AwarenessRange);
	FocusRange = FMath::Clamp(FocusRange, 0.0f, AwarenessRange);
	InteractionRange = FMath::Clamp(InteractionRange, 0.0f, FocusRange);
}

bool FRpgInteractionPromptDefinition::IsSemanticallyEqual(const FRpgInteractionPromptDefinition& Other) const
{
	return ActionText.IdenticalTo(Other.ActionText) &&
		TargetText.IdenticalTo(Other.TargetText) &&
		BlockedReason.IdenticalTo(Other.BlockedReason) &&
		Icon == Other.Icon &&
		FMath::IsNearlyEqual(AwarenessRange, Other.AwarenessRange) &&
		FMath::IsNearlyEqual(FocusRange, Other.FocusRange) &&
		FMath::IsNearlyEqual(InteractionRange, Other.InteractionRange) &&
		InteractionPriority == Other.InteractionPriority &&
		bShowNearbyIndicator == Other.bShowNearbyIndicator &&
		bRequiresLineOfSight == Other.bRequiresLineOfSight &&
		FocusWidgetClass == Other.FocusWidgetClass &&
		NearbyWidgetClass == Other.NearbyWidgetClass;
}
