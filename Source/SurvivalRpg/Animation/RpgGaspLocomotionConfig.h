// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RpgGaspLocomotionConfig.generated.h"

/** Stable project roles for every Pose Search database used by the curated GASP CMC runtime. */
UENUM(BlueprintType)
enum class ERpgMotionMatchingDatabaseRole : uint8
{
	None,
	StandIdle,
	StandWalk,
	StandWalkStops,
	StandRunLoops,
	StandRunPivots,
	StandRunStarts,
	StandRunStops,
	StandSprint,
	StandSprintStops,
	Crouch,
	StandTurnInPlace,
	Jump,
	StandLightLanding,
	StandHeavyLanding,
	WalkLightLanding,
	WalkHeavyLanding,
	RunLightLanding,
	RunHeavyLanding,
	Count UMETA(Hidden),
};

/**
 * Designer-owned cosmetic feel for the curated GASP locomotion profile.
 *
 * Values are copied from the static profile on the game thread during animation initialization.
 * They never own CharacterMovement state, gameplay authority, replication, or persistence.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgGaspLocomotionTuning
{
	GENERATED_BODY()

	/** Minimum horizontal speed retained as a stable locomotion direction, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared", meta = (ClampMin = "0.001", Units = "cm/s"))
	float LastMeaningfulVelocityThreshold = 5.0f;

	/** Shared stationary presentation boundary for gait, turn, and landing policies, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared", meta = (ClampMin = "0.001", Units = "cm/s"))
	float StationarySpeedThreshold = 3.0f;

	/** Normalized acceleration-input magnitude above which grounded movement intent is present. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveIntentThreshold = 0.1f;

	/** Normalized acceleration-input boundary selecting Run rather than Walk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RunInputThreshold = 0.65f;

	/** Horizontal velocity tolerance used by the logical Motion Matching moving predicate, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ChooserVelocityTolerance = 0.1f;

	/** Horizontal acceleration tolerance used by the logical Motion Matching moving predicate, in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float ChooserAccelerationTolerance = 0.0001f;

	/** Inclusive minimum speed for the overlapping Walk-stop chooser row, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.001", Units = "cm/s"))
	float WalkStopMinimumSpeed = 20.0f;

	/** Inclusive minimum speed for the overlapping Run-stop chooser row, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.001", Units = "cm/s"))
	float RunStopMinimumSpeed = 100.0f;

	/** Inclusive minimum speed for the Sprint-stop chooser row, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.001", Units = "cm/s"))
	float SprintStopMinimumSpeed = 550.0f;

	/** Minimum acceleration-versus-velocity pivot angle in free-facing locomotion, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float FreeRunPivotMinimumAngle = 45.0f;

	/** Minimum acceleration-versus-velocity pivot angle while combat strafing, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float CombatStrafeRunPivotMinimumAngle = 30.0f;

	/** Minimum acceleration-versus-velocity pivot angle while aiming, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float AimRunPivotMinimumAngle = 0.0f;

	/** Required future speed gain for the Run-start database row, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RunStartMinimumFutureSpeedGain = 100.0f;

	/** First future trajectory sample time used to estimate Run-start acceleration, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.0", ClampMax = "1.5", Units = "s"))
	float RunStartFutureVelocityBeginTime = 0.4f;

	/** Last future trajectory sample time used to estimate Run-start acceleration, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (ClampMin = "0.001", ClampMax = "1.5", Units = "s"))
	float RunStartFutureVelocityEndTime = 0.5f;

	/** Actor-yaw accumulation that enters turn collection, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", ClampMax = "180.0", Units = "deg"))
	float TurnCollectThreshold = 20.0f;

	/** Actor-yaw accumulation that may activate an authored turn, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", ClampMax = "180.0", Units = "deg"))
	float TurnActivationThreshold = 30.0f;

	/** Actor-yaw accumulation below which collection or playback recovers, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float TurnCancelThreshold = 10.0f;

	/** Yaw rate treated as inactive while retaining a small accumulator, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.0", Units = "deg/s"))
	float TurnInactiveYawRateThreshold = 6.0f;

	/** Yaw rate considered stable enough to dispatch an authored turn, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.0", Units = "deg/s"))
	float TurnStableYawRateThreshold = 60.0f;

	/** Time the yaw request must remain stable before turn dispatch, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnStabilityDuration = 0.08f;

	/** Maximum collection time before dispatch or recovery, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnCollectionTimeout = 0.2f;

	/** Offset-root recovery duration after cancelling a turn request, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnRecoveryDuration = 0.15f;

	/** Maximum time allowed to select an authored turn result, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnSelectionTimeout = 0.25f;

	/** Fallback and minimum watchdog lifetime for one active turn presentation, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnActiveTimeout = 1.75f;

	/** Time an inactive small yaw accumulator may survive before clearing, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnInactiveAccumulatorTimeout = 0.2f;

	/** Synthetic trajectory horizon paired with authored 45-degree turns, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnFacingDuration45 = 0.45f;

	/** Synthetic trajectory horizon paired with authored 90-degree turns, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnFacingDuration90 = 0.65f;

	/** Synthetic trajectory horizon paired with authored 135-degree turns, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnFacingDuration135 = 0.85f;

	/** Synthetic trajectory horizon paired with authored 180-degree turns, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.001", Units = "s"))
	float TurnFacingDuration180 = 1.0f;

	/** Independent wall-clock bound for a backward Jump Start Continuing Pose, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.001", Units = "s"))
	float BackwardJumpStartHoldTimeout = 1.25f;

	/** Playback remainder released before a backward Jump Start reaches its authored end, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.0", Units = "s"))
	float BackwardJumpStartReleaseLeadTime = 0.2f;

	/** Inclusive cosmetic impact-speed boundary between Light and Heavy landings, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "1.0", UIMin = "300.0", UIMax = "1200.0", Units = "cm/s"))
	float HeavyLandingSpeedThreshold = 700.0f;

	/** Maximum time allowed to select the initial or handed-off landing result, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.001", Units = "s"))
	float LandingSelectionTimeout = 0.25f;

	/** Maximum wall-clock lifetime of one active landing presentation, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.1", Units = "s"))
	float LandingActiveTimeout = 1.25f;

	/** Physical touchdown window for the single stationary-to-moving landing handoff, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0", Units = "s"))
	float LandingMovementHandoffWindow = 0.3f;
};

/** Stable mapping from one native database role to its project-owned asset tag. */
namespace RpgGaspLocomotionConfig
{
	SURVIVALRPG_API FName GetDatabaseRoleTag(ERpgMotionMatchingDatabaseRole Role);

	/** Resolves exactly one known project role tag; duplicate or unknown project role tags fail closed. */
	SURVIVALRPG_API ERpgMotionMatchingDatabaseRole ResolveDatabaseRoleTag(
		TConstArrayView<FName> Tags);

	/** Checks finite values, normalized ranges, ordered thresholds, and positive presentation durations. */
	SURVIVALRPG_API bool IsTuningRuntimeValid(const FRpgGaspLocomotionTuning& Tuning);
}
