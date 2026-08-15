// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/TrajectoryTypes.h"
#include "Engine/EngineTypes.h"

#include "RpgPoseSearchTrajectory.generated.h"

class ARpgCharacter;
class URpgCharacterMovementComponent;
struct FHitResult;

/**
 * Game-thread collision policy for the cosmetic Pose Search trajectory.
 *
 * The project resolves at most the configured number of future samples per update. None of
 * these bounded sweeps changes CharacterMovement, collision, replication, or touchdown state.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgTrajectoryCollisionSettings
{
	GENERATED_BODY()

	/** Enables game-thread gravity and world-collision correction for generated Pose Search trajectories. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	bool bEnabled = true;

	/** Height in centimeters retained between a corrected trajectory sample and the hit floor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "0.001", ClampMax = "10.0", UIMin = "0.001", UIMax = "2.0", Units = "cm"))
	float FloorOffset = 0.01f;

	/** Maximum vertical obstacle/floor search range per future sample, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "1.0", ClampMax = "1000.0", UIMin = "50.0", UIMax = "300.0", Units = "cm"))
	float MaxObstacleHeight = 150.0f;

	/** Radius of each bounded walkability sweep, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "0.1", ClampMax = "20.0", UIMin = "0.5", UIMax = "10.0", Units = "cm"))
	float SweepRadius = 2.0f;

	/** Maximum future samples swept per update; clamped to the generated 15-sample GASP horizon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "1", ClampMax = "15", UIMin = "1", UIMax = "15"))
	int32 MaxPredictionSamples = 15;

	/** Collision channel used by cosmetic prediction sweeps; defaults to Visibility like GASP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Uses complex geometry for cosmetic prediction traces; disabled by default to keep the hot path bounded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	bool bTraceComplex = false;
};

/** Pointer-free, cosmetic prediction of the next walkable landing within the trajectory horizon. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgTrajectoryLandingPrediction
{
	GENERATED_BODY()

	/** Predicted world-space contact point from the first validated walkable trajectory collision. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	FVector LandingLocation = FVector::ZeroVector;

	/** Unit world-space normal at the predicted walkable contact. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	FVector LandingNormal = FVector::UpVector;

	/** Seconds until predicted contact; -1 means there is no valid airborne prediction. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (Units = "s"))
	float TimeToLand = -1.0f;

	/** True only for a finite, walkable airborne hit inside the current prediction horizon. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	bool bIsValid = false;
};

/** Project-owned trajectory sampling contract, validation, and game-thread collision helpers. */
namespace RpgPoseSearchTrajectory
{
	/** Exact sampling arguments authored by GASP's Update_Trajectory function. */
	inline constexpr float HistorySamplingInterval = -1.0f;
	inline constexpr int32 HistorySampleCount = 30;
	inline constexpr float PredictionSamplingInterval = 0.1f;
	inline constexpr int32 PredictionSampleCount = 15;

	/** Game-thread collision output published to the AnimInstance proxy as value-only data. */
	struct SURVIVALRPG_API FCollisionResult
	{
		FTransformTrajectory CorrectedTrajectory;
		FRpgTrajectoryLandingPrediction LandingPrediction;
		int32 WorldQueryCount = 0;
	};

	/** Returns false for an empty trajectory or any non-finite or non-monotonic sample. */
	SURVIVALRPG_API bool IsTransformTrajectoryFinite(const FTransformTrajectory& Trajectory);

	/** Validates the bounded designer-authored collision policy against the generated GASP horizon. */
	SURVIVALRPG_API bool IsCollisionSettingsRuntimeValid(
		const FRpgTrajectoryCollisionSettings& Settings);

	/** Interpolates a bounded first-contact time between two ballistic samples and a floor plane. */
	SURVIVALRPG_API float CalculateLandingTime(
		float PreviousTime,
		float CurrentTime,
		const FVector& PreviousPosition,
		const FVector& CurrentPosition,
		const FVector& LandingLocation,
		const FVector& UpDirection);

	/** Sanitizes a walkable game-thread hit into the pointer-free airborne prediction contract. */
	SURVIVALRPG_API FRpgTrajectoryLandingPrediction MakeLandingPrediction(
		bool bCanPublishPrediction,
		bool bHasWalkableHit,
		bool bHardReset,
		float TimeToLand,
		float PredictionHorizon,
		const FVector& LandingLocation,
		const FVector& LandingNormal);

	/**
	 * Applies gravity and bounded walkability sweeps to the future trajectory on the game thread.
	 * CharacterMovement remains authoritative; the returned correction and landing prediction are cosmetic.
	 */
	SURVIVALRPG_API FCollisionResult ResolveWorldCollision(
		const ARpgCharacter& Character,
		const URpgCharacterMovementComponent& MovementComponent,
		const FRpgTrajectoryCollisionSettings& Settings,
		const FTransformTrajectory& RawTrajectory,
		bool bCanPublishPrediction,
		bool bHardReset);

#if WITH_DEV_AUTOMATION_TESTS
	/** Runs the production resolver against deterministic query results for bounded automation coverage. */
	SURVIVALRPG_API FRpgTrajectoryLandingPrediction ResolveCollisionForTest(
		const FRpgTrajectoryCollisionSettings& Settings,
		const FVector& GravityAcceleration,
		bool bIsFalling,
		const FTransformTrajectory& RawTrajectory,
		const TArray<FHitResult>& QueryHits,
		const TArray<bool>& QueryWalkability,
		FTransformTrajectory& OutTrajectory,
		int32& OutWorldQueryCount,
		TArray<FVector>* OutTraceStarts = nullptr,
		TArray<FVector>* OutTraceEnds = nullptr);
#endif
}
