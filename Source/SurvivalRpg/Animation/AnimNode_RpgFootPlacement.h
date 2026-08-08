// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "BoneControllers/BoneControllerTypes.h"
#include "RpgFootPlacementTypes.h"
#include "AnimNode_RpgFootPlacement.generated.h"

/** Bone configuration consumed only by the pointer-free RPG foot-placement skeletal control. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgFootPlacementNodeLegDefinition
{
	GENERATED_BODY()

	/** IK target bone adjusted before the downstream Leg IK solve. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference IKFootBone;

	/** Ball bone used as the pivot for ground-alignment rotation. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference BallBone;
};

/**
 * Worker-thread-safe foot-placement node backed exclusively by a game-thread POD snapshot.
 *
 * The node performs no traces and never reads an actor, world, movement component, skeletal
 * mesh component, or UObject during AnyThread evaluation. A downstream Leg IK node resolves
 * the FK leg chains toward the IK targets authored here.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SURVIVALRPG_API FAnimNode_RpgFootPlacement : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_RpgFootPlacement();

	/** Pelvis bone lowered when one planted target lies below the current animation. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference PelvisBone;

	/** Left and right IK/ball definitions; the pilot graph requires exactly two entries. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FRpgFootPlacementNodeLegDefinition> LegsDefinition;

	/** Immutable snapshot copied from FRpgAnimInstanceProxy::PreUpdate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot", meta = (PinShownByDefault))
	FRpgFootPlacementSnapshot Snapshot;

	/** Maximum translation applied to one IK target, in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxFootTranslation = 50.0f;

	/** Maximum ground-alignment rotation applied to one IK target, in degrees. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float MaxFootRotation = 60.0f;

	/** Maximum downward pelvis correction, in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxPelvisOffset = 50.0f;

	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void EvaluateSkeletalControl_AnyThread(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
};
