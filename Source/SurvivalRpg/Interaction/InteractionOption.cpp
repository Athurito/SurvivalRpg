// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionOption.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InteractionOption)

void FInteractionOption::NormalizeLegacyFields()
{
	if (Prompt.ActionText.IsEmpty() && !Text.IsEmpty())
	{
		Prompt.ActionText = Text;
	}
	if (Prompt.TargetText.IsEmpty() && !SubText.IsEmpty())
	{
		Prompt.TargetText = SubText;
	}
	if (Prompt.FocusWidgetClass.IsNull() && !InteractionWidgetClass.IsNull())
	{
		Prompt.FocusWidgetClass = InteractionWidgetClass;
	}
	Prompt.SanitizeRanges();
}

FVector FInteractionOption::GetInteractionWorldLocation() const
{
	if (!TargetRef.WorldLocation.IsNearlyZero())
	{
		return TargetRef.WorldLocation;
	}
	if (const UPrimitiveComponent* Component = TargetRef.TargetComponent.Get())
	{
		return Component->Bounds.Origin;
	}
	if (const AActor* Actor = TargetRef.TargetActor.Get())
	{
		return Actor->GetActorLocation();
	}
	return FVector::ZeroVector;
}
