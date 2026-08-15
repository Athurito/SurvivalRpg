// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgPoseSearchTrajectory.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPoseSearchTrajectory)

namespace
{
constexpr float TrajectoryFloorOffsetMin = 0.001f;
constexpr float TrajectoryFloorOffsetMax = 10.0f;
constexpr float TrajectoryObstacleHeightMin = 1.0f;
constexpr float TrajectoryObstacleHeightMax = 1000.0f;
constexpr float TrajectorySweepRadiusMin = 0.1f;
constexpr float TrajectorySweepRadiusMax = 20.0f;

struct FTrajectoryCollisionResolution
{
	FVector LandingLocation = FVector::ZeroVector;
	FVector LandingNormal = FVector::UpVector;
	float TimeToLand = -1.0f;
	float PredictionHorizon = 0.0f;
	int32 WorldQueryCount = 0;
	bool bHasWalkableLanding = false;
};

using FTrajectorySweepQuery = TFunctionRef<bool(
	FHitResult& OutHit,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	ECollisionChannel TraceChannel,
	const FCollisionShape& CollisionShape)>;
using FTrajectoryWalkabilityQuery = TFunctionRef<bool(const FHitResult& HitResult)>;

bool ProjectTrajectoryPointOntoHitPlane(
	const FVector& Point,
	const FVector& GravityDirection,
	const FVector& PlanePoint,
	const FVector& PlaneNormal,
	FVector& OutProjectedPoint)
{
	if (Point.ContainsNaN() || GravityDirection.ContainsNaN() ||
		PlanePoint.ContainsNaN() || PlaneNormal.ContainsNaN() ||
		GravityDirection.IsNearlyZero() || PlaneNormal.IsNearlyZero())
	{
		return false;
	}

	const FVector SafeGravityDirection = GravityDirection.GetSafeNormal();
	const FVector SafePlaneNormal = PlaneNormal.GetSafeNormal();
	const double PlaneDenominator = FVector::DotProduct(
		SafeGravityDirection,
		SafePlaneNormal);
	if (!FMath::IsFinite(PlaneDenominator) ||
		FMath::Abs(PlaneDenominator) <= UE_SMALL_NUMBER)
	{
		return false;
	}

	const double DistanceAlongGravity = FVector::DotProduct(
		PlanePoint - Point,
		SafePlaneNormal) / PlaneDenominator;
	if (!FMath::IsFinite(DistanceAlongGravity))
	{
		return false;
	}

	OutProjectedPoint = Point + SafeGravityDirection * DistanceAlongGravity;
	return !OutProjectedPoint.ContainsNaN();
}

FTrajectoryCollisionResolution ResolveCollision(
	const FRpgTrajectoryCollisionSettings& Settings,
	const FVector& GravityAcceleration,
	bool bIsFalling,
	const FTransformTrajectory& RawTrajectory,
	FTransformTrajectory& OutTrajectory,
	FTrajectorySweepQuery SweepQuery,
	FTrajectoryWalkabilityQuery WalkabilityQuery)
{
	FTrajectoryCollisionResolution Result;
	OutTrajectory = RawTrajectory;

	FVector GravityDirection = FVector::ZeroVector;
	float GravityMagnitude = 0.0f;
	GravityAcceleration.ToDirectionAndLength(GravityDirection, GravityMagnitude);
	if (!RpgPoseSearchTrajectory::IsCollisionSettingsRuntimeValid(Settings) ||
		!RpgPoseSearchTrajectory::IsTransformTrajectoryFinite(RawTrajectory) ||
		GravityAcceleration.ContainsNaN() || GravityDirection.ContainsNaN() ||
		!FMath::IsFinite(GravityMagnitude) || GravityMagnitude <= UE_SMALL_NUMBER)
	{
		return Result;
	}

	int32 CurrentSampleIndex = INDEX_NONE;
	for (int32 SampleIndex = 0; SampleIndex < RawTrajectory.Samples.Num(); ++SampleIndex)
	{
		if (RawTrajectory.Samples[SampleIndex].TimeInSeconds > 0.0f)
		{
			CurrentSampleIndex = SampleIndex;
			break;
		}
	}
	if (CurrentSampleIndex == INDEX_NONE ||
		CurrentSampleIndex + 1 >= RawTrajectory.Samples.Num())
	{
		return Result;
	}

	const FVector UpDirection = -GravityDirection;
	const float CurrentSampleTime = RawTrajectory.Samples[CurrentSampleIndex].TimeInSeconds;
	float PreviousRelativeTime = 0.0f;
	FVector PreviousPosition = RawTrajectory.Samples[CurrentSampleIndex].Position;
	float PostContactHeightAlongUp = 0.0f;
	float FreeFallAccumulatedSeconds = 0.0f;
	bool bHasPostContactHeight = false;
	bool bHasLastWalkablePosition = false;
	int32 ProcessedPredictionSamples = 0;
	const int32 MaxPredictionSamples = FMath::Min(
		Settings.MaxPredictionSamples,
		RpgPoseSearchTrajectory::PredictionSampleCount);

	for (int32 SampleIndex = CurrentSampleIndex + 1;
		 SampleIndex < RawTrajectory.Samples.Num() &&
		 ProcessedPredictionSamples < MaxPredictionSamples;
		 ++SampleIndex)
	{
		const FTransformTrajectorySample& RawSample = RawTrajectory.Samples[SampleIndex];
		FTransformTrajectorySample& CorrectedSample = OutTrajectory.Samples[SampleIndex];
		const float RelativeTime = RawSample.TimeInSeconds - CurrentSampleTime;
		if (!FMath::IsFinite(RelativeTime) ||
			RelativeTime <= PreviousRelativeTime + UE_SMALL_NUMBER)
		{
			break;
		}

		const float StepSeconds = RelativeTime - PreviousRelativeTime;
		FreeFallAccumulatedSeconds += StepSeconds;
		FVector BallisticPosition = RawSample.Position;
		if (bHasPostContactHeight)
		{
			const float RawHeightAlongUp = FVector::DotProduct(
				BallisticPosition,
				UpDirection);
			BallisticPosition +=
				UpDirection * (PostContactHeightAlongUp - RawHeightAlongUp);
		}
		BallisticPosition +=
			GravityAcceleration * (0.5f * FMath::Square(FreeFallAccumulatedSeconds));
		CorrectedSample.Position = BallisticPosition;
		Result.PredictionHorizon = RelativeTime;
		++ProcessedPredictionSamples;

		FHitResult HitResult;
		const bool bTraceBallisticSegment =
			bIsFalling && !bHasLastWalkablePosition;
		const FVector TraceStart = bTraceBallisticSegment
			? PreviousPosition
			: BallisticPosition + UpDirection * Settings.MaxObstacleHeight;
		++Result.WorldQueryCount;
		const bool bHit = SweepQuery(
			HitResult,
			TraceStart,
			BallisticPosition,
			Settings.TraceChannel.GetValue(),
			FCollisionShape::MakeSphere(Settings.SweepRadius));
		const bool bWalkableHit =
			bHit && HitResult.bBlockingHit && WalkabilityQuery(HitResult);
		if (bWalkableHit && !HitResult.ImpactPoint.ContainsNaN() &&
			!HitResult.ImpactNormal.ContainsNaN() &&
			!HitResult.ImpactNormal.IsNearlyZero())
		{
			FVector SurfacePositionAtSample = FVector::ZeroVector;
			if (ProjectTrajectoryPointOntoHitPlane(
					BallisticPosition,
					GravityDirection,
					HitResult.ImpactPoint,
					HitResult.ImpactNormal,
					SurfacePositionAtSample))
			{
				const FVector CorrectedPosition =
					SurfacePositionAtSample + UpDirection * Settings.FloorOffset;
				CorrectedSample.Position = CorrectedPosition;
				PostContactHeightAlongUp = FVector::DotProduct(
					CorrectedPosition,
					UpDirection);
				bHasPostContactHeight = true;
				bHasLastWalkablePosition = true;
				FreeFallAccumulatedSeconds = 0.0f;

				if (!Result.bHasWalkableLanding && bIsFalling)
				{
					Result.TimeToLand =
						bTraceBallisticSegment && FMath::IsFinite(HitResult.Time)
							? FMath::Lerp(
								PreviousRelativeTime,
								RelativeTime,
								FMath::Clamp(HitResult.Time, 0.0f, 1.0f))
							: RpgPoseSearchTrajectory::CalculateLandingTime(
								PreviousRelativeTime,
								RelativeTime,
								PreviousPosition,
								BallisticPosition,
								HitResult.ImpactPoint,
								UpDirection);
					Result.LandingLocation = HitResult.ImpactPoint;
					Result.LandingNormal = HitResult.ImpactNormal;
					Result.bHasWalkableLanding = true;
				}
			}
			else
			{
				bHasLastWalkablePosition = false;
			}
		}
		else
		{
			bHasLastWalkablePosition = false;
		}

		PreviousRelativeTime = RelativeTime;
		PreviousPosition = CorrectedSample.Position;
	}

	return Result;
}
}

namespace RpgPoseSearchTrajectory
{
bool IsTransformTrajectoryFinite(const FTransformTrajectory& Trajectory)
{
	if (Trajectory.Samples.IsEmpty())
	{
		return false;
	}

	float PreviousTime = -MAX_flt;
	for (const FTransformTrajectorySample& Sample : Trajectory.Samples)
	{
		if (!FMath::IsFinite(Sample.TimeInSeconds) ||
			Sample.TimeInSeconds + UE_SMALL_NUMBER < PreviousTime ||
			Sample.Position.ContainsNaN() || Sample.Facing.ContainsNaN())
		{
			return false;
		}
		PreviousTime = Sample.TimeInSeconds;
	}
	return true;
}

bool IsCollisionSettingsRuntimeValid(
	const FRpgTrajectoryCollisionSettings& Settings)
{
	const int32 TraceChannel = static_cast<int32>(Settings.TraceChannel.GetValue());
	return Settings.bEnabled &&
		FMath::IsFinite(Settings.FloorOffset) &&
		Settings.FloorOffset >= TrajectoryFloorOffsetMin &&
		Settings.FloorOffset <= TrajectoryFloorOffsetMax &&
		FMath::IsFinite(Settings.MaxObstacleHeight) &&
		Settings.MaxObstacleHeight >= TrajectoryObstacleHeightMin &&
		Settings.MaxObstacleHeight <= TrajectoryObstacleHeightMax &&
		FMath::IsFinite(Settings.SweepRadius) &&
		Settings.SweepRadius >= TrajectorySweepRadiusMin &&
		Settings.SweepRadius <= TrajectorySweepRadiusMax &&
		Settings.MaxPredictionSamples >= 1 &&
		Settings.MaxPredictionSamples <= PredictionSampleCount &&
		TraceChannel >= static_cast<int32>(ECC_WorldStatic) &&
		TraceChannel < static_cast<int32>(ECC_OverlapAll_Deprecated);
}

float CalculateLandingTime(
	float PreviousTime,
	float CurrentTime,
	const FVector& PreviousPosition,
	const FVector& CurrentPosition,
	const FVector& LandingLocation,
	const FVector& UpDirection)
{
	if (!FMath::IsFinite(PreviousTime) || !FMath::IsFinite(CurrentTime) ||
		CurrentTime < PreviousTime || PreviousPosition.ContainsNaN() ||
		CurrentPosition.ContainsNaN() || LandingLocation.ContainsNaN() ||
		UpDirection.ContainsNaN() || UpDirection.IsNearlyZero())
	{
		return -1.0f;
	}

	const FVector SafeUpDirection = UpDirection.GetSafeNormal();
	const float PreviousHeight = FVector::DotProduct(
		PreviousPosition - LandingLocation,
		SafeUpDirection);
	const float CurrentHeight = FVector::DotProduct(
		CurrentPosition - LandingLocation,
		SafeUpDirection);
	const float HeightDelta = PreviousHeight - CurrentHeight;
	const float Alpha = HeightDelta > UE_SMALL_NUMBER
		? FMath::Clamp(PreviousHeight / HeightDelta, 0.0f, 1.0f)
		: 1.0f;
	return FMath::Lerp(PreviousTime, CurrentTime, Alpha);
}

FRpgTrajectoryLandingPrediction MakeLandingPrediction(
	bool bCanPublishPrediction,
	bool bHasWalkableHit,
	bool bHardReset,
	float TimeToLand,
	float PredictionHorizon,
	const FVector& LandingLocation,
	const FVector& LandingNormal)
{
	FRpgTrajectoryLandingPrediction Result;
	if (!bCanPublishPrediction || !bHasWalkableHit || bHardReset ||
		!FMath::IsFinite(TimeToLand) || !FMath::IsFinite(PredictionHorizon) ||
		TimeToLand < 0.0f || PredictionHorizon <= 0.0f ||
		TimeToLand > PredictionHorizon + UE_KINDA_SMALL_NUMBER ||
		LandingLocation.ContainsNaN() || LandingNormal.ContainsNaN() ||
		LandingNormal.IsNearlyZero())
	{
		return Result;
	}

	Result.LandingLocation = LandingLocation;
	Result.LandingNormal = LandingNormal.GetSafeNormal(
		UE_SMALL_NUMBER,
		FVector::UpVector);
	Result.TimeToLand = FMath::Clamp(TimeToLand, 0.0f, PredictionHorizon);
	Result.bIsValid = true;
	return Result;
}

FCollisionResult ResolveWorldCollision(
	const ARpgCharacter& Character,
	const URpgCharacterMovementComponent& MovementComponent,
	const FRpgTrajectoryCollisionSettings& Settings,
	const FTransformTrajectory& RawTrajectory,
	bool bCanPublishPrediction,
	bool bHardReset)
{
	check(IsInGameThread());
	FCollisionResult Result;
	Result.CorrectedTrajectory = RawTrajectory;
	UWorld* World = Character.GetWorld();
	if (!World)
	{
		return Result;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(RpgPoseSearchTrajectory),
		Settings.bTraceComplex,
		&Character);
	QueryParams.AddIgnoredActor(&Character);
	const FVector GravityAcceleration =
		MovementComponent.GetGravityDirection() * -MovementComponent.GetGravityZ();
	const FTrajectoryCollisionResolution Resolution = ResolveCollision(
		Settings,
		GravityAcceleration,
		MovementComponent.IsFalling(),
		RawTrajectory,
		Result.CorrectedTrajectory,
		[World, &QueryParams](
			FHitResult& OutHit,
			const FVector& TraceStart,
			const FVector& TraceEnd,
			ECollisionChannel TraceChannel,
			const FCollisionShape& CollisionShape)
		{
			return World->SweepSingleByChannel(
				OutHit,
				TraceStart,
				TraceEnd,
				FQuat::Identity,
				TraceChannel,
				CollisionShape,
				QueryParams);
		},
		[&MovementComponent](const FHitResult& HitResult)
		{
			return MovementComponent.IsWalkable(HitResult);
		});
	Result.WorldQueryCount = Resolution.WorldQueryCount;
	Result.LandingPrediction = MakeLandingPrediction(
		bCanPublishPrediction,
		Resolution.bHasWalkableLanding,
		bHardReset,
		Resolution.TimeToLand,
		Resolution.PredictionHorizon,
		Resolution.LandingLocation,
		Resolution.LandingNormal);
	return Result;
}

#if WITH_DEV_AUTOMATION_TESTS
FRpgTrajectoryLandingPrediction ResolveCollisionForTest(
	const FRpgTrajectoryCollisionSettings& Settings,
	const FVector& GravityAcceleration,
	bool bIsFalling,
	const FTransformTrajectory& RawTrajectory,
	const TArray<FHitResult>& QueryHits,
	const TArray<bool>& QueryWalkability,
	FTransformTrajectory& OutTrajectory,
	int32& OutWorldQueryCount,
	TArray<FVector>* OutTraceStarts,
	TArray<FVector>* OutTraceEnds)
{
	int32 QueryIndex = 0;
	int32 LastQueryIndex = INDEX_NONE;
	const FTrajectoryCollisionResolution Resolution = ResolveCollision(
		Settings,
		GravityAcceleration,
		bIsFalling,
		RawTrajectory,
		OutTrajectory,
		[&QueryHits, &QueryIndex, &LastQueryIndex, OutTraceStarts, OutTraceEnds](
			FHitResult& OutHit,
			const FVector& TraceStart,
			const FVector& TraceEnd,
			ECollisionChannel,
			const FCollisionShape&)
		{
			if (OutTraceStarts)
			{
				OutTraceStarts->Add(TraceStart);
			}
			if (OutTraceEnds)
			{
				OutTraceEnds->Add(TraceEnd);
			}
			LastQueryIndex = QueryIndex++;
			if (!QueryHits.IsValidIndex(LastQueryIndex))
			{
				OutHit = FHitResult();
				return false;
			}

			OutHit = QueryHits[LastQueryIndex];
			return OutHit.bBlockingHit != 0;
		},
		[&QueryWalkability, &LastQueryIndex](const FHitResult&)
		{
			return QueryWalkability.IsValidIndex(LastQueryIndex) &&
				QueryWalkability[LastQueryIndex];
		});
	OutWorldQueryCount = Resolution.WorldQueryCount;
	return MakeLandingPrediction(
		bIsFalling,
		Resolution.bHasWalkableLanding,
		false,
		Resolution.TimeToLand,
		Resolution.PredictionHorizon,
		Resolution.LandingLocation,
		Resolution.LandingNormal);
}
#endif
}
