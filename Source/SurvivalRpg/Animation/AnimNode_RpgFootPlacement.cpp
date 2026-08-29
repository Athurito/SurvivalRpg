// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_RpgFootPlacement.h"

#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_RpgFootPlacement)

FAnimNode_RpgFootPlacement::FAnimNode_RpgFootPlacement()
{
	AlphaInputType = EAnimAlphaInputType::Float;
	ResetInterpolationState();
}

void FAnimNode_RpgFootPlacement::ResetInterpolationState()
{
	CachedDeltaTime = 0.0f;
	SmoothedPelvisOffset = 0.0f;
	UpdateCounter.Reset();
	for (int32 LegIndex = 0; LegIndex < 2; ++LegIndex)
	{
		SmoothedCorrectionOffsetsCS[LegIndex] = FTransform::Identity;
		PreviousOutputTargetsWorld[LegIndex] = FTransform::Identity;
		bHasSmoothedCorrection[LegIndex] = false;
		bHasPreviousOutputTarget[LegIndex] = false;
		bWasLocked[LegIndex] = false;
	}
}

void FAnimNode_RpgFootPlacement::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
	ResetInterpolationState();
}

FTransform FAnimNode_RpgFootPlacement::ResolveLegCorrectionTargetCS(
	int32 LegIndex,
	bool bLocked,
	const FTransform& FKFootTransformCS,
	const FTransform& DesiredResolvedTargetCS,
	const FTransform& ComponentToWorld)
{
	check(LegIndex >= 0 && LegIndex < 2);
	const FTransform DesiredCorrectionOffsetCS = RpgFootPlacement::CalculateFootCorrectionOffset(
		FKFootTransformCS,
		DesiredResolvedTargetCS);
	if (bLocked)
	{
		SmoothedCorrectionOffsetsCS[LegIndex] = DesiredCorrectionOffsetCS;
		bHasSmoothedCorrection[LegIndex] = true;
		return DesiredResolvedTargetCS;
	}

	if (bWasLocked[LegIndex] && bHasPreviousOutputTarget[LegIndex])
	{
		const FTransform PreviousOutputTargetCS =
			PreviousOutputTargetsWorld[LegIndex].GetRelativeTransform(ComponentToWorld);
		SmoothedCorrectionOffsetsCS[LegIndex] = RpgFootPlacement::CalculateFootCorrectionOffset(
			FKFootTransformCS,
			PreviousOutputTargetCS);
		bHasSmoothedCorrection[LegIndex] = true;
	}
	else if (!bHasSmoothedCorrection[LegIndex])
	{
		SmoothedCorrectionOffsetsCS[LegIndex] = DesiredCorrectionOffsetCS;
		bHasSmoothedCorrection[LegIndex] = true;
	}

	SmoothedCorrectionOffsetsCS[LegIndex] = RpgFootPlacement::SmoothFootCorrectionOffset(
		SmoothedCorrectionOffsetsCS[LegIndex],
		DesiredCorrectionOffsetCS,
		CachedDeltaTime,
		ReleaseTranslationBlendHalfLife,
		ReleaseRotationBlendHalfLife);
	FTransform OutputTargetCS = RpgFootPlacement::ApplyFootCorrectionOffset(
		FKFootTransformCS,
		SmoothedCorrectionOffsetsCS[LegIndex]);
	OutputTargetCS.SetScale3D(DesiredResolvedTargetCS.GetScale3D());
	return OutputTargetCS;
}

void FAnimNode_RpgFootPlacement::CommitLegCorrectionTarget(
	int32 LegIndex,
	bool bLocked,
	const FTransform& FKFootTransformCS,
	const FTransform& FinalOutputTargetCS,
	const FTransform& ComponentToWorld)
{
	check(LegIndex >= 0 && LegIndex < 2);
	SmoothedCorrectionOffsetsCS[LegIndex] = RpgFootPlacement::CalculateFootCorrectionOffset(
		FKFootTransformCS,
		FinalOutputTargetCS);
	bHasSmoothedCorrection[LegIndex] = true;
	PreviousOutputTargetsWorld[LegIndex] = FinalOutputTargetCS * ComponentToWorld;
	bHasPreviousOutputTarget[LegIndex] = true;
	bWasLocked[LegIndex] = bLocked;
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

	if (bSkippedRelevantUpdate || !Snapshot.bValid || !Snapshot.bGrounded ||
		Snapshot.bReset || LegsDefinition.Num() != 2)
	{
		ResetInterpolationState();
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
		ResetInterpolationState();
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

		const FTransform ProceduralTargetCS =
			ProceduralTargetWorld.GetRelativeTransform(Snapshot.ComponentToWorld);
		const FTransform LiveOutputTargetCS = RpgFootPlacement::ResolveIKFootTarget(
			FKFootTransformCS,
			ProceduralTargetCS,
			EffectiveLegWeight);
		FTransform OutputTargetCS = ResolveLegCorrectionTargetCS(
			LegIndex,
			LegSnapshot.bLocked,
			FKFootTransformCS,
			LiveOutputTargetCS,
			Snapshot.ComponentToWorld);

		FTransform OutputTargetWorld = OutputTargetCS * Snapshot.ComponentToWorld;
		const FVector OutputTargetOffset = (
			OutputTargetWorld.GetLocation() - FKFootTransformWorld.GetLocation())
			.GetClampedToMaxSize(FMath::Max(MaxFootTranslation, 0.0f));
		OutputTargetWorld.SetLocation(FKFootTransformWorld.GetLocation() + OutputTargetOffset);
		const float OutputTargetRotationDegrees = FMath::RadiansToDegrees(
			FKFootTransformWorld.GetRotation().AngularDistance(OutputTargetWorld.GetRotation()));
		if (OutputTargetRotationDegrees > MaxFootRotation &&
			OutputTargetRotationDegrees > UE_SMALL_NUMBER)
		{
			OutputTargetWorld.SetRotation(FQuat::Slerp(
				FKFootTransformWorld.GetRotation(),
				OutputTargetWorld.GetRotation(),
				MaxFootRotation / OutputTargetRotationDegrees).GetNormalized());
		}
		OutputTargetCS = OutputTargetWorld.GetRelativeTransform(Snapshot.ComponentToWorld);
		CommitLegCorrectionTarget(
			LegIndex,
			LegSnapshot.bLocked,
			FKFootTransformCS,
			OutputTargetCS,
			Snapshot.ComponentToWorld);

		if (!OutputTargetCS.Equals(FKFootTransformCS, UE_KINDA_SMALL_NUMBER))
		{
			FootOffsets[LegIndex] = FVector::DotProduct(
				OutputTargetWorld.GetLocation() - FKFootTransformWorld.GetLocation(),
				ComponentUpWorld);
		}

		// Stock Leg IK has one global alpha and always solves both legs. Writing the current
		// FK ankle here prevents a static or unsuitable authored IK track from pinning a swing leg.
		OutBoneTransforms.Emplace(IKFootIndex, OutputTargetCS);
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
	ResetInterpolationState();
}
