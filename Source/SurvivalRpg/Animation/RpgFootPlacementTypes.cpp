// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgFootPlacementTypes.h"

#include "Math/UnrealMathUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgFootPlacementTypes)

FRpgFootPlacementSettings::FRpgFootPlacementSettings()
{
	LeftLeg.FKFootBone = TEXT("foot_l");
	LeftLeg.IKFootBone = TEXT("ik_foot_l");
	LeftLeg.BallBone = TEXT("ball_l");
	LeftLeg.SpeedCurveName = TEXT("contact_l");
	RightLeg.FKFootBone = TEXT("foot_r");
	RightLeg.IKFootBone = TEXT("ik_foot_r");
	RightLeg.BallBone = TEXT("ball_r");
	RightLeg.SpeedCurveName = TEXT("contact_r");
}

namespace RpgFootPlacement
{
	float ConvertContactCurveToSpeed(float ContactCurveValue)
	{
		return (1.0f - FMath::Clamp(ContactCurveValue, 0.0f, 1.0f)) * 100.0f;
	}

	float CalculateHalfLifeAlpha(float DeltaSeconds, float HalfLifeSeconds)
	{
		if (DeltaSeconds <= 0.0f)
		{
			return 0.0f;
		}
		if (HalfLifeSeconds <= UE_SMALL_NUMBER)
		{
			return 1.0f;
		}
		return 1.0f - FMath::Exp2(-DeltaSeconds / HalfLifeSeconds);
	}

	float CalculateAlignmentAlpha(float FootSpeed, const FRpgFootPlacementSettings& Settings)
	{
		if (FootSpeed <= Settings.PlantSpeedThreshold)
		{
			return 1.0f;
		}
		if (FootSpeed >= Settings.UnalignmentSpeedThreshold)
		{
			return 0.0f;
		}
		const float SpeedRange = Settings.UnalignmentSpeedThreshold - Settings.PlantSpeedThreshold;
		if (SpeedRange <= UE_SMALL_NUMBER)
		{
			return 0.0f;
		}
		return 1.0f - (FootSpeed - Settings.PlantSpeedThreshold) / SpeedRange;
	}

	bool ShouldPlantFoot(
		bool bHasWalkableGround,
		float FootSpeed,
		float DistanceToGround,
		const FRpgFootPlacementSettings& Settings)
	{
		return bHasWalkableGround &&
			FootSpeed <= Settings.PlantSpeedThreshold &&
			FMath::Abs(DistanceToGround) <= Settings.PlantDistanceThreshold;
	}

	bool ShouldUnplantFoot(
		float FootSpeed,
		float AnchorDistance,
		float GroundNormalDeltaDegrees,
		const FRpgFootPlacementSettings& Settings)
	{
		return FootSpeed > Settings.PlantSpeedThreshold ||
			AnchorDistance > Settings.UnplantRadius ||
			GroundNormalDeltaDegrees > Settings.UnplantAngle;
	}

	bool ShouldReplantFoot(
		float AnchorDistance,
		float GroundNormalDeltaDegrees,
		const FRpgFootPlacementSettings& Settings)
	{
		return AnchorDistance <= Settings.UnplantRadius * Settings.ReplantRadiusRatio &&
			GroundNormalDeltaDegrees <= Settings.UnplantAngle * Settings.ReplantAngleRatio;
	}

	FTransform RebaseTransformThroughSurface(
		const FTransform& TransformWorld,
		const FTransform& PreviousSurfaceTransform,
		const FTransform& CurrentSurfaceTransform)
	{
		return TransformWorld.GetRelativeTransform(PreviousSurfaceTransform) * CurrentSurfaceTransform;
	}

	FVector RebasePointThroughSurface(
		const FVector& PointWorld,
		const FTransform& PreviousSurfaceTransform,
		const FTransform& CurrentSurfaceTransform)
	{
		return CurrentSurfaceTransform.TransformPosition(
			PreviousSurfaceTransform.InverseTransformPosition(PointWorld));
	}

	FVector RebaseNormalThroughSurface(
		const FVector& NormalWorld,
		const FTransform& PreviousSurfaceTransform,
		const FTransform& CurrentSurfaceTransform)
	{
		return CurrentSurfaceTransform.TransformVectorNoScale(
			PreviousSurfaceTransform.InverseTransformVectorNoScale(NormalWorld))
			.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	}

	FTransform AlignFootToGroundPlane(
		const FTransform& IKFootTransformWorld,
		const FTransform& BallTransformWorld,
		const FVector& GroundPointWorld,
		const FVector& GroundNormalWorld,
		const FVector& ComponentUpWorld,
		float MaxTranslationOffset,
		float MaxRotationDegrees)
	{
		const FVector SafeNormal = GroundNormalWorld.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector SafeUp = ComponentUpWorld.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		FQuat Alignment = FQuat::FindBetweenNormals(SafeUp, SafeNormal);
		const float AlignmentAngleDegrees = FMath::RadiansToDegrees(Alignment.GetAngle());
		if (AlignmentAngleDegrees > MaxRotationDegrees && AlignmentAngleDegrees > UE_SMALL_NUMBER)
		{
			Alignment = FQuat::Slerp(FQuat::Identity, Alignment, MaxRotationDegrees / AlignmentAngleDegrees);
			Alignment.Normalize();
		}

		const FVector BallLocation = BallTransformWorld.GetLocation();
		const FVector IKLocation = IKFootTransformWorld.GetLocation();
		const float Denominator = FVector::DotProduct(-SafeUp, SafeNormal);
		FVector ProjectedBall = BallLocation;
		if (FMath::Abs(Denominator) > UE_SMALL_NUMBER)
		{
			const float DistanceAlongDown = FVector::DotProduct(GroundPointWorld - BallLocation, SafeNormal) / Denominator;
			ProjectedBall += -SafeUp * DistanceAlongDown;
		}

		FTransform Result = IKFootTransformWorld;
		Result.SetRotation((Alignment * IKFootTransformWorld.GetRotation()).GetNormalized());
		FVector TargetLocation = ProjectedBall + Alignment.RotateVector(IKLocation - BallLocation);
		const FVector TranslationOffset = (TargetLocation - IKLocation).GetClampedToMaxSize(
			FMath::Max(MaxTranslationOffset, 0.0f));
		Result.SetLocation(IKLocation + TranslationOffset);
		return Result;
	}

	FTransform DeriveIKBallTransform(
		const FTransform& FKFootTransform,
		const FTransform& BallTransform,
		const FTransform& IKFootTransform)
	{
		const FTransform FootToBall = BallTransform.GetRelativeTransform(FKFootTransform);
		return FootToBall * IKFootTransform;
	}

	FTransform PivotFootAroundBall(
		const FTransform& IKFootTransform,
		const FTransform& IKBallTransform,
		const FTransform& LockedFootTransform)
	{
		const FTransform FootToBall = IKBallTransform.GetRelativeTransform(IKFootTransform);
		const FTransform BallToFoot = IKFootTransform.GetRelativeTransform(IKBallTransform);
		const FTransform LockedBallTransform = FootToBall * LockedFootTransform;
		const FTransform PinnedBallTransform(
			IKBallTransform.GetRotation(),
			LockedBallTransform.GetLocation(),
			IKBallTransform.GetScale3D());
		return BallToFoot * PinnedBallTransform;
	}

	float CalculateGeometryWeight(
		float BallDistanceToPlane,
		float PlanarLockDrift,
		bool bLocked,
		float PlantDistanceThreshold,
		float UnplantRadius)
	{
		auto CalculateBoundWeight = [](float Value, float Bound)
		{
			const float SafeBound = FMath::Max(Bound, 0.0f);
			const float AbsoluteValue = FMath::Abs(Value);
			if (SafeBound <= UE_SMALL_NUMBER)
			{
				return AbsoluteValue <= UE_SMALL_NUMBER ? 1.0f : 0.0f;
			}
			if (AbsoluteValue >= SafeBound)
			{
				return 0.0f;
			}

			const float FadeAlpha = FMath::Clamp(
				(AbsoluteValue - SafeBound * 0.5f) / (SafeBound * 0.5f),
				0.0f,
				1.0f);
			const float SmoothFadeAlpha = FadeAlpha * FadeAlpha * (3.0f - 2.0f * FadeAlpha);
			return 1.0f - SmoothFadeAlpha;
		};

		const float PlaneWeight = CalculateBoundWeight(BallDistanceToPlane, PlantDistanceThreshold);
		const float LockWeight = bLocked
			? CalculateBoundWeight(PlanarLockDrift, UnplantRadius)
			: 1.0f;
		return PlaneWeight * LockWeight;
	}

	float CalculateEffectivePlacementWeight(
		bool bHasWalkableGround,
		float SnapshotWeight,
		float GeometryWeight)
	{
		if (!bHasWalkableGround)
		{
			return 0.0f;
		}

		return FMath::Clamp(SnapshotWeight, 0.0f, 1.0f) *
			FMath::Clamp(GeometryWeight, 0.0f, 1.0f);
	}

	FTransform ResolveIKFootTarget(
		const FTransform& FKFootTransform,
		const FTransform& ProceduralTargetTransform,
		float EffectivePlacementWeight)
	{
		const float SafeWeight = FMath::Clamp(EffectivePlacementWeight, 0.0f, 1.0f);
		if (SafeWeight <= UE_KINDA_SMALL_NUMBER)
		{
			return FKFootTransform;
		}
		if (SafeWeight >= 1.0f - UE_KINDA_SMALL_NUMBER)
		{
			return ProceduralTargetTransform;
		}

		FTransform Result = FKFootTransform;
		Result.SetLocation(FMath::Lerp(
			FKFootTransform.GetLocation(),
			ProceduralTargetTransform.GetLocation(),
			SafeWeight));
		Result.SetRotation(FQuat::Slerp(
			FKFootTransform.GetRotation(),
			ProceduralTargetTransform.GetRotation(),
			SafeWeight).GetNormalized());
		Result.SetScale3D(FMath::Lerp(
			FKFootTransform.GetScale3D(),
			ProceduralTargetTransform.GetScale3D(),
			SafeWeight));
		return Result;
	}

	FTransform CalculateFootCorrectionOffset(
		const FTransform& FKFootTransform,
		const FTransform& ResolvedTargetTransform)
	{
		FTransform Result = FKFootTransform.GetRelativeTransformReverse(ResolvedTargetTransform);
		Result.SetScale3D(FVector::OneVector);
		return Result;
	}

	FTransform SmoothFootCorrectionOffset(
		const FTransform& CurrentOffset,
		const FTransform& DesiredOffset,
		float DeltaSeconds,
		float TranslationHalfLifeSeconds,
		float RotationHalfLifeSeconds)
	{
		const float TranslationAlpha = CalculateHalfLifeAlpha(
			DeltaSeconds,
			TranslationHalfLifeSeconds);
		const float RotationAlpha = CalculateHalfLifeAlpha(
			DeltaSeconds,
			RotationHalfLifeSeconds);
		FTransform Result = DesiredOffset;
		Result.SetLocation(FMath::Lerp(
			CurrentOffset.GetLocation(),
			DesiredOffset.GetLocation(),
			TranslationAlpha));
		Result.SetRotation(FQuat::Slerp(
			CurrentOffset.GetRotation(),
			DesiredOffset.GetRotation(),
			RotationAlpha).GetNormalized());
		return Result;
	}

	FTransform ApplyFootCorrectionOffset(
		const FTransform& FKFootTransform,
		const FTransform& CorrectionOffset)
	{
		FTransform ScaleFreeOffset = CorrectionOffset;
		ScaleFreeOffset.SetScale3D(FVector::OneVector);
		FTransform Result = FKFootTransform * ScaleFreeOffset;
		Result.SetScale3D(FKFootTransform.GetScale3D());
		return Result;
	}

	float CalculatePelvisOffset(float LeftOffset, float RightOffset, float MaxDownwardOffset)
	{
		const float RequestedOffset = FMath::Min3(LeftOffset, RightOffset, 0.0f);
		return FMath::Clamp(RequestedOffset, -FMath::Max(MaxDownwardOffset, 0.0f), 0.0f);
	}

	float SmoothPelvisOffset(
		float CurrentOffset,
		float TargetOffset,
		float DeltaSeconds,
		float HalfLifeSeconds,
		float MaxSpeed)
	{
		const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
		if (SafeDeltaSeconds <= 0.0f)
		{
			return CurrentOffset;
		}

		const float InterpolationAlpha = CalculateHalfLifeAlpha(SafeDeltaSeconds, HalfLifeSeconds);
		const float InterpolatedOffset = FMath::Lerp(CurrentOffset, TargetOffset, InterpolationAlpha);
		const float MaxStep = FMath::Max(MaxSpeed, 0.0f) * SafeDeltaSeconds;
		return CurrentOffset + FMath::Clamp(
			InterpolatedOffset - CurrentOffset,
			-MaxStep,
			MaxStep);
	}
}
