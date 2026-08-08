// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "SurvivalRpg/Animation/AnimNode_RpgFootPlacement.h"
#include "AnimGraphNode_RpgFootPlacement.generated.h"

/** Editor representation of the project-local, snapshot-only RPG Foot Placement node. */
UCLASS(meta = (Keywords = "RPG Foot Placement IK Thread Safe"))
class SURVIVALRPGEDITOR_API UAnimGraphNode_RpgFootPlacement : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_RpgFootPlacement Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void ValidateAnimNodeDuringCompilation(
		USkeleton* ForSkeleton,
		FCompilerResultsLog& MessageLog) override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
