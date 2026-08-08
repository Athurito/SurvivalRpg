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

	float CalculatePelvisOffset(float LeftOffset, float RightOffset, float MaxDownwardOffset)
	{
		const float RequestedOffset = FMath::Min3(LeftOffset, RightOffset, 0.0f);
		return FMath::Clamp(RequestedOffset, -FMath::Max(MaxDownwardOffset, 0.0f), 0.0f);
	}
}
