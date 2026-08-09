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
		TEXT("(Valid=%s, Left=%.2f, Right=%.2f, Pelvis=%.2f)"),
		Snapshot.bValid ? TEXT("true") : TEXT("false"),
		Snapshot.LeftFoot.Weight,
		Snapshot.RightFoot.Weight,
		SmoothedPelvisOffset);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_RpgFootPlacement::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	const FGraphTraversalCounter& ProxyUpdateCounter = Context.AnimInstanceProxy->GetUpdateCounter();
	const bool bSkippedRelevantUpdate =
		UpdateCounter.HasEverBeenUpdated() &&
		!UpdateCounter.WasSynchronizedCounter(ProxyUpdateCounter);
	UpdateCounter.SynchronizeWith(ProxyUpdateCounter);

	if (bSkippedRelevantUpdate || !Snapshot.bValid || !Snapshot.bGrounded || Snapshot.bReset)
	{
		CachedDeltaTime = 0.0f;
		SmoothedPelvisOffset = 0.0f;
		return;
	}

	CachedDeltaTime += FMath::Max(Context.GetDeltaTime(), 0.0f);
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
		CachedDeltaTime = 0.0f;
		SmoothedPelvisOffset = 0.0f;
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
		const FCompactPoseBoneIndex IKFootIndex = LegDefinition.IKFootBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex FKFootIndex = LegDefinition.FKFootBone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex BallIndex = LegDefinition.BallBone.GetCompactPoseIndex(BoneContainer);
		if (!IKFootIndex.IsValid() || !FKFootIndex.IsValid() || !BallIndex.IsValid())
		{
			continue;
		}

		const FTransform FKFootTransformCS = Output.Pose.GetComponentSpaceTransform(FKFootIndex);
		const FTransform AuthoredBallTransformCS = Output.Pose.GetComponentSpaceTransform(BallIndex);
		const FTransform FKFootTransformWorld = FKFootTransformCS * Snapshot.ComponentToWorld;
		const FTransform AuthoredBallTransformWorld =
			AuthoredBallTransformCS * Snapshot.ComponentToWorld;
		FTransform ProceduralTargetWorld = FKFootTransformWorld;
		float GeometryWeight = 0.0f;

		const float LegWeight = FMath::Clamp(LegSnapshot.Weight, 0.0f, 1.0f);
		if (LegSnapshot.bHasWalkableGround && LegWeight > UE_KINDA_SMALL_NUMBER)
		{
			const FTransform LiveAlignedTargetWorld = RpgFootPlacement::AlignFootToGroundPlane(
				FKFootTransformWorld,
				AuthoredBallTransformWorld,
				LegSnapshot.GroundPointWorld,
				LegSnapshot.GroundNormalWorld,
				ComponentUpWorld,
				MaxFootTranslation,
				MaxFootRotation);
			const FTransform UnalignedTargetWorld = LegSnapshot.bLocked
				? RpgFootPlacement::PivotFootAroundBall(
					FKFootTransformWorld,
					AuthoredBallTransformWorld,
					LegSnapshot.LockedFootTransformWorld)
				: FKFootTransformWorld;
			const FTransform TargetBallTransformWorld = RpgFootPlacement::DeriveIKBallTransform(
				FKFootTransformWorld,
				AuthoredBallTransformWorld,
				UnalignedTargetWorld);
			ProceduralTargetWorld = RpgFootPlacement::AlignFootToGroundPlane(
				UnalignedTargetWorld,
				TargetBallTransformWorld,
				LegSnapshot.GroundPointWorld,
				LegSnapshot.GroundNormalWorld,
				ComponentUpWorld,
				MaxFootTranslation,
				MaxFootRotation);
			const FVector SafeGroundNormal = LegSnapshot.GroundNormalWorld.GetSafeNormal(
				UE_SMALL_NUMBER,
				FVector::UpVector);
			const float BallDistanceToPlane = FVector::DotProduct(
				AuthoredBallTransformWorld.GetLocation() - LegSnapshot.GroundPointWorld,
				SafeGroundNormal);
			const float PlanarLockDrift = (
				LiveAlignedTargetWorld.GetLocation() - ProceduralTargetWorld.GetLocation()).Size2D();
			GeometryWeight = RpgFootPlacement::CalculateGeometryWeight(
				BallDistanceToPlane,
				PlanarLockDrift,
				LegSnapshot.bLocked,
				PlantDistanceThreshold,
				UnplantRadius);
		}

		const float EffectiveLegWeight = RpgFootPlacement::CalculateEffectivePlacementWeight(
			LegSnapshot.bHasWalkableGround,
			LegWeight,
			GeometryWeight);
		if (EffectiveLegWeight > UE_KINDA_SMALL_NUMBER)
		{
			const FVector ClampedTargetOffset = (
				ProceduralTargetWorld.GetLocation() - FKFootTransformWorld.GetLocation())
				.GetClampedToMaxSize(FMath::Max(MaxFootTranslation, 0.0f));
			ProceduralTargetWorld.SetLocation(FKFootTransformWorld.GetLocation() + ClampedTargetOffset);
			const float TargetRotationDegrees = FMath::RadiansToDegrees(
				FKFootTransformWorld.GetRotation().AngularDistance(ProceduralTargetWorld.GetRotation()));
			if (TargetRotationDegrees > MaxFootRotation && TargetRotationDegrees > UE_SMALL_NUMBER)
			{
				ProceduralTargetWorld.SetRotation(FQuat::Slerp(
					FKFootTransformWorld.GetRotation(),
					ProceduralTargetWorld.GetRotation(),
					MaxFootRotation / TargetRotationDegrees).GetNormalized());
			}

			FootOffsets[LegIndex] = FVector::DotProduct(
				ProceduralTargetWorld.GetLocation() - FKFootTransformWorld.GetLocation(),
				ComponentUpWorld) * EffectiveLegWeight;
		}

		const FTransform ProceduralTargetCS =
			ProceduralTargetWorld.GetRelativeTransform(Snapshot.ComponentToWorld);
		// Stock Leg IK has one global alpha and always solves both legs. Writing the current
		// FK ankle here prevents a static or unsuitable authored IK track from pinning a swing leg.
		OutBoneTransforms.Emplace(
			IKFootIndex,
			RpgFootPlacement::ResolveIKFootTarget(
				FKFootTransformCS,
				ProceduralTargetCS,
				EffectiveLegWeight));
	}

	const FCompactPoseBoneIndex PelvisIndex = PelvisBone.GetCompactPoseIndex(BoneContainer);
	if (PelvisIndex.IsValid())
	{
		const float TargetPelvisOffset = RpgFootPlacement::CalculatePelvisOffset(
			FootOffsets[0],
			FootOffsets[1],
			MaxPelvisOffset);
		SmoothedPelvisOffset = RpgFootPlacement::SmoothPelvisOffset(
			SmoothedPelvisOffset,
			TargetPelvisOffset,
			CachedDeltaTime,
			PelvisBlendHalfLife,
			MaxPelvisSpeed);
		if (!FMath::IsNearlyZero(SmoothedPelvisOffset))
		{
			FTransform PelvisTransformCS = Output.Pose.GetComponentSpaceTransform(PelvisIndex);
			const FVector ComponentUpCS = Snapshot.ComponentToWorld.InverseTransformVectorNoScale(ComponentUpWorld);
			PelvisTransformCS.AddToTranslation(ComponentUpCS * SmoothedPelvisOffset);
			OutBoneTransforms.Emplace(PelvisIndex, PelvisTransformCS);
		}
	}
	else
	{
		SmoothedPelvisOffset = 0.0f;
	}
	CachedDeltaTime = 0.0f;

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
		if (!LegDefinition.FKFootBone.IsValidToEvaluate(RequiredBones) ||
			!LegDefinition.IKFootBone.IsValidToEvaluate(RequiredBones) ||
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
		LegDefinition.FKFootBone.Initialize(RequiredBones);
		LegDefinition.IKFootBone.Initialize(RequiredBones);
		LegDefinition.BallBone.Initialize(RequiredBones);
	}
}
