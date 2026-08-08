// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_RpgFootPlacement.h"

#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_RpgFootPlacement)

FAnimNode_RpgFootPlacement::FAnimNode_RpgFootPlacement()
{
	AlphaInputType = EAnimAlphaInputType::Float;
}

void FAnimNode_RpgFootPlacement::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(
		TEXT("(Valid=%s, Left=%.2f, Right=%.2f)"),
		Snapshot.bValid ? TEXT("true") : TEXT("false"),
		Snapshot.LeftFoot.Weight,
		Snapshot.RightFoot.Weight);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_RpgFootPlacement::EvaluateSkeletalControl_AnyThread(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateSkeletalControl_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(RpgFootPlacement, !IsInGameThread());
	check(OutBoneTransforms.IsEmpty());

	if (!Snapshot.bValid || !Snapshot.bGrounded || Snapshot.bReset || LegsDefinition.Num() != 2)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FVector ComponentUpWorld = Snapshot.ComponentToWorld.GetUnitAxis(EAxis::Z);
	float FootOffsets[2] = {0.0f, 0.0f};
	const FRpgFootPlacementLegSnapshot* LegSnapshots[2] = {
		&Snapshot.LeftFoot,
		&Snapshot.RightFoot,
	};

	for (int32 LegIndex = 0; LegIndex < 2; ++LegIndex)
	{
		const FRpgFootPlacementNodeLegDefinition& LegDefinition = LegsDefinition[LegIndex];
		const FRpgFootPlacementLegSnapshot& LegSnapshot = *LegSnapshots[LegIndex];
		const float LegWeight = FMath::Clamp(LegSnapshot.Weight, 0.0f, 1.0f);
		if (!LegSnapshot.bHasWalkableGround || LegWeight <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FCompactPoseBoneIndex IKFootIndex = LegDefinition.IKFootBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex BallIndex = LegDefinition.BallBone.GetCompactPoseIndex(BoneContainer);
		if (!IKFootIndex.IsValid() || !BallIndex.IsValid())
		{
			continue;
		}

		const FTransform CurrentIKTransformCS = Output.Pose.GetComponentSpaceTransform(IKFootIndex);
		const FTransform BallTransformCS = Output.Pose.GetComponentSpaceTransform(BallIndex);
		const FTransform CurrentIKTransformWorld = CurrentIKTransformCS * Snapshot.ComponentToWorld;
		const FTransform BallTransformWorld = BallTransformCS * Snapshot.ComponentToWorld;
		FTransform TargetTransformWorld = LegSnapshot.bLocked
			? LegSnapshot.LockedFootTransformWorld
			: RpgFootPlacement::AlignFootToGroundPlane(
				CurrentIKTransformWorld,
				BallTransformWorld,
				LegSnapshot.GroundPointWorld,
				LegSnapshot.GroundNormalWorld,
				ComponentUpWorld,
				MaxFootTranslation,
				MaxFootRotation);
		const FVector ClampedTargetOffset = (
			TargetTransformWorld.GetLocation() - CurrentIKTransformWorld.GetLocation())
			.GetClampedToMaxSize(FMath::Max(MaxFootTranslation, 0.0f));
		TargetTransformWorld.SetLocation(CurrentIKTransformWorld.GetLocation() + ClampedTargetOffset);
		const float TargetRotationDegrees = FMath::RadiansToDegrees(
			CurrentIKTransformWorld.GetRotation().AngularDistance(TargetTransformWorld.GetRotation()));
		if (TargetRotationDegrees > MaxFootRotation && TargetRotationDegrees > UE_SMALL_NUMBER)
		{
			TargetTransformWorld.SetRotation(FQuat::Slerp(
				CurrentIKTransformWorld.GetRotation(),
				TargetTransformWorld.GetRotation(),
				MaxFootRotation / TargetRotationDegrees).GetNormalized());
		}

		const FTransform TargetTransformCS = TargetTransformWorld.GetRelativeTransform(Snapshot.ComponentToWorld);
		FTransform BlendedTransformCS = CurrentIKTransformCS;
		BlendedTransformCS.SetLocation(FMath::Lerp(
			CurrentIKTransformCS.GetLocation(),
			TargetTransformCS.GetLocation(),
			LegWeight));
		BlendedTransformCS.SetRotation(FQuat::Slerp(
			CurrentIKTransformCS.GetRotation(),
			TargetTransformCS.GetRotation(),
			LegWeight).GetNormalized());

		FootOffsets[LegIndex] = FVector::DotProduct(
			TargetTransformWorld.GetLocation() - CurrentIKTransformWorld.GetLocation(),
			ComponentUpWorld) * LegWeight;
		OutBoneTransforms.Emplace(IKFootIndex, BlendedTransformCS);
	}

	const FCompactPoseBoneIndex PelvisIndex = PelvisBone.GetCompactPoseIndex(BoneContainer);
	if (PelvisIndex.IsValid())
	{
		const float PelvisOffset = RpgFootPlacement::CalculatePelvisOffset(
			FootOffsets[0],
			FootOffsets[1],
			MaxPelvisOffset);
		if (!FMath::IsNearlyZero(PelvisOffset))
		{
			FTransform PelvisTransformCS = Output.Pose.GetComponentSpaceTransform(PelvisIndex);
			const FVector ComponentUpCS = Snapshot.ComponentToWorld.InverseTransformVectorNoScale(ComponentUpWorld);
			PelvisTransformCS.AddToTranslation(ComponentUpCS * PelvisOffset);
			OutBoneTransforms.Emplace(PelvisIndex, PelvisTransformCS);
		}
	}

	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

bool FAnimNode_RpgFootPlacement::IsValidToEvaluate(
	const USkeleton* Skeleton,
	const FBoneContainer& RequiredBones)
{
	if (!PelvisBone.IsValidToEvaluate(RequiredBones) || LegsDefinition.Num() != 2)
	{
		return false;
	}
	for (const FRpgFootPlacementNodeLegDefinition& LegDefinition : LegsDefinition)
	{
		if (!LegDefinition.IKFootBone.IsValidToEvaluate(RequiredBones) ||
			!LegDefinition.BallBone.IsValidToEvaluate(RequiredBones))
		{
			return false;
		}
	}
	return true;
}

void FAnimNode_RpgFootPlacement::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	PelvisBone.Initialize(RequiredBones);
	for (FRpgFootPlacementNodeLegDefinition& LegDefinition : LegsDefinition)
	{
		LegDefinition.IKFootBone.Initialize(RequiredBones);
		LegDefinition.BallBone.Initialize(RequiredBones);
	}
}
