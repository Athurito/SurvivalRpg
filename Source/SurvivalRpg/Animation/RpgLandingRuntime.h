// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RpgPoseSearchTrajectory.h"

enum class ERpgJumpPhase : uint8;
enum class ERpgLocomotionGait : uint8;
enum class ERpgLocomotionMovementState : uint8;
enum class ERpgMotionMatchingDatabaseRole : uint8;
struct FRpgLandingSelectionSnapshot;

/** Pointer-free game-thread state retained across landing-capture updates. */
struct SURVIVALRPG_API FRpgLandingCaptureState
{
	ERpgLocomotionGait LastGroundedGait{};
	int32 AirborneEpoch = 0;
	bool bWasAirborne = false;
};

/** Immutable movement and trajectory values used to freeze the final airborne landing context. */
struct SURVIVALRPG_API FRpgLandingCaptureSnapshot
{
	FVector WorldVelocity = FVector::ZeroVector;
	FVector GravityAcceleration = FVector::ZeroVector;
	FRpgTrajectoryLandingPrediction TrajectoryPrediction;
	ERpgLocomotionGait Gait{};
	ERpgLocomotionMovementState MovementState{};
	float VerticalVelocity = 0.0f;
	float InputMagnitude = 0.0f;
	bool bIsFalling = false;
	bool bIsMovingOnGround = false;
	bool bHardReset = false;
};

/** Availability of the six fixed landing database slots, assembled by the UObject facade. */
struct SURVIVALRPG_API FRpgLandingDatabaseAvailability
{
	bool bStandLight = false;
	bool bStandHeavy = false;
	bool bWalkLight = false;
	bool bWalkHeavy = false;
	bool bRunLight = false;
	bool bRunHeavy = false;
};

/** Pointer-free movement and presentation gates shared by touchdown and active landing updates. */
struct SURVIVALRPG_API FRpgLandingEligibilitySnapshot
{
	ERpgLocomotionMovementState MovementState{};
	bool bIsMovingOnGround = false;
	bool bIsCrouching = false;
	bool bIsAnyMontagePlaying = false;
	bool bHasBlockingGameplayTag = false;
};

/** Flat cosmetic landing lifecycle copied from the compatible AnimInstance facade. */
struct SURVIVALRPG_API FRpgLandingRuntimeState
{
	ERpgMotionMatchingDatabaseRole ActiveRole{};
	float StateElapsed = 0.0f;
	float TouchdownElapsed = 0.0f;
	float PlaybackWatchdogDuration = 0.0f;
	uint32 RequestSerial = 0;
	uint32 InterruptedRequestSerial = 0;
	bool bSelectionLatched = false;
	bool bCompletionArmed = false;
};

/** Value-only live inputs used to advance an already active landing request. */
struct SURVIVALRPG_API FRpgLandingActiveSnapshot
{
	FRpgLandingEligibilitySnapshot Eligibility;
	FRpgLandingDatabaseAvailability Availability;
	ERpgLocomotionGait LiveGait{};
	float GroundSpeed = 0.0f;
	bool bChooserMoving = false;
};

/** Phase intent returned to the AnimInstance, which remains the sole reflected phase facade. */
enum class ERpgLandingRuntimeTransition : uint8
{
	None,
	BeginLanding,
	ResetGrounded,
};

/** Deterministic lifecycle result plus explicit UObject-bridge cleanup actions. */
struct SURVIVALRPG_API FRpgLandingRuntimeResult
{
	FRpgLandingRuntimeState State;
	ERpgLandingRuntimeTransition Transition = ERpgLandingRuntimeTransition::None;
	bool bClearSelection = false;
	bool bClearBackwardHold = false;
};

/** Deterministic landing capture, role, fallback, serial, and timeout policy adapted from GASP. */
namespace RpgLandingRuntime
{
	inline constexpr float IdleSpeedThreshold = 3.0f;
	inline constexpr float SelectionTimeout = 0.25f;
	inline constexpr float ActiveTimeout = 1.25f;
	inline constexpr float PlaybackWatchdogSafetyMargin = 0.1f;
	inline constexpr float FinishedTimeTolerance = 0.05f;
	inline constexpr float MovementHandoffWindow = 0.3f;

	/** Updates the final-airborne snapshot while preserving one physical touchdown frame. */
	SURVIVALRPG_API void UpdateSelectionSnapshot(
		FRpgLandingSelectionSnapshot& SelectionSnapshot,
		FRpgLandingCaptureState& State,
		const FRpgLandingCaptureSnapshot& Snapshot);

	/** Resolves the authored Light/Heavy and Stand/Walk/Run role from frozen finite values. */
	SURVIVALRPG_API ERpgMotionMatchingDatabaseRole ResolveDatabaseRole(
		const FRpgLandingSelectionSnapshot& Snapshot,
		float HeavySpeedThreshold);

	/** Preserves severity while rebasing a Walk/Run landing into the stationary domain. */
	SURVIVALRPG_API ERpgMotionMatchingDatabaseRole ResolveStationaryRole(
		ERpgMotionMatchingDatabaseRole LandingRole);

	/** Returns whether stationary presentation must leave its inclusive physical idle band. */
	SURVIVALRPG_API bool ShouldReleaseStationary(
		ERpgMotionMatchingDatabaseRole LandingRole,
		bool bChooserMoving,
		float GroundSpeed);

	/** Preserves severity while mapping stationary presentation to the live Walk/Run gait. */
	SURVIVALRPG_API ERpgMotionMatchingDatabaseRole ResolveStationaryMovementRole(
		ERpgMotionMatchingDatabaseRole LandingRole,
		ERpgLocomotionGait LiveGait);

	/** Returns whether the exact fixed landing database slot is configured. */
	SURVIVALRPG_API bool IsRoleAvailable(
		ERpgMotionMatchingDatabaseRole Role,
		const FRpgLandingDatabaseAvailability& Availability);

	/** Applies the project-only same-gait Heavy-to-Light fallback and otherwise fails closed. */
	SURVIVALRPG_API ERpgMotionMatchingDatabaseRole ResolveAvailableRole(
		ERpgMotionMatchingDatabaseRole RequestedRole,
		const FRpgLandingDatabaseAvailability& Availability);

	/** Checks stable grounded, stance, montage, and gameplay-tag presentation gates. */
	SURVIVALRPG_API bool IsEligible(const FRpgLandingEligibilitySnapshot& Snapshot);

	/** Resolves the first physical-touchdown request without starting from trajectory prediction. */
	SURVIVALRPG_API ERpgMotionMatchingDatabaseRole ResolveTouchdownRole(
		const FRpgLandingSelectionSnapshot& SelectionSnapshot,
		const FRpgLandingEligibilitySnapshot& Eligibility,
		const FRpgLandingDatabaseAvailability& Availability,
		ERpgLocomotionGait LiveGait,
		float GroundSpeed,
		bool bChooserMoving,
		float HeavySpeedThreshold);

	/** Clears landing values while retaining the monotonic request and interrupt serials. */
	SURVIVALRPG_API FRpgLandingRuntimeResult Reset(const FRpgLandingRuntimeState& State);

	/** Starts the initial request or one database-change handoff while skipping serial zero. */
	SURVIVALRPG_API FRpgLandingRuntimeResult BeginRequest(
		const FRpgLandingRuntimeState& State,
		ERpgMotionMatchingDatabaseRole LandingRole,
		bool bForceInterrupt);

	/** Advances stationary handoff, selection timeout, playback watchdog, and completion. */
	SURVIVALRPG_API FRpgLandingRuntimeResult UpdateActive(
		const FRpgLandingRuntimeState& State,
		const FRpgLandingActiveSnapshot& Snapshot,
		float DeltaSeconds);

	/** Consumes at most one ForceInterrupt for the active request serial. */
	SURVIVALRPG_API bool ConsumeForceInterrupt(
		bool bLandingPhase,
		bool bHasActiveDatabase,
		FRpgLandingRuntimeState& State);

	/** Prevents a completed or cancelled landing database from surviving the request. */
	SURVIVALRPG_API bool ShouldInterruptDatabaseExit(
		ERpgJumpPhase CurrentJumpPhase,
		bool bCompletionArmed,
		ERpgMotionMatchingDatabaseRole CurrentDatabaseRole);

	/** Resolves the bounded wall-clock watchdog for selected landing playback. */
	SURVIVALRPG_API float CalculatePlaybackWatchdogDuration(
		float RemainingAnimationTime,
		float PlayRate,
		bool bLooping);
}
