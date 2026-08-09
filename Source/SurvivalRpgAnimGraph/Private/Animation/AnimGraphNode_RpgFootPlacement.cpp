// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimGraphNode_RpgFootPlacement.h"

#include "Kismet2/CompilerResultsLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_RpgFootPlacement)

#define LOCTEXT_NAMESPACE "RpgAnimGraphNodes"

FText UAnimGraphNode_RpgFootPlacement::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_RpgFootPlacement::GetTooltipText() const
{
	return LOCTEXT(
		"RpgFootPlacementTooltip",
		"Moves pelvis and IK-foot targets from a pointer-free game-thread snapshot. "
		"This node performs no traces or gameplay-object access during parallel evaluation.");
}

void UAnimGraphNode_RpgFootPlacement::ValidateAnimNodeDuringCompilation(
	USkeleton* ForSkeleton,
	FCompilerResultsLog& MessageLog)
{
	if (Node.LegsDefinition.Num() != 2)
	{
		MessageLog.Error(
			*LOCTEXT("RpgFootPlacementNeedsTwoLegs", "@@ requires exactly two leg definitions.").ToString(),
			this);
	}
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

FText UAnimGraphNode_RpgFootPlacement::GetControllerDescription() const
{
	return LOCTEXT("RpgFootPlacement", "RPG Foot Placement (Thread Safe)");
}

#undef LOCTEXT_NAMESPACE
