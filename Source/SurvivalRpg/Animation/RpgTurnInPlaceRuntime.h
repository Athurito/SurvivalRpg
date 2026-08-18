// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/TrajectoryTypes.h"
#include "AnimationWarpingTypes.h"

enum class ERpgCharacterRotationMode : uint8;
enum class ERpgLocomotionMovementState : uint8;
enum class ERpgTurnInPlaceState : uint8;

/** Database policy requested by the cosmetic turn-in-place lifecycle for the next node update. */
enum class ERpgTurnInPlaceSearchMode : uint8
{
	NormalLocomotion,
	SearchRequestedTurn,
	ContinueSelectedTurn,
};

/** Pointer-free state assembled from the stable flat AnimInstance facade for one runtime decision. */
struct SURVIVALRPG_API FRpgTurnInPlaceRuntimeState
{
	ERpgTurnInPlaceState State{};
	float QueryAngle = 0.0f;
	float AccumulatedYaw = 0.0f;
	float StateElapsed = 0.0f;
	float StableElapsed = 0.0f;
	float SelectionElapsed = 0.0f;
	float PlaybackWatchdogDuration = 0.0f;
	float RequestAccumulatedYaw = 0.0f;
	uint32 RequestSerial = 0;
	uint32 InterruptedRequestSerial = 0;
	uint8 HardResetReasonsLastFrame = 0;
};

/** Immutable value inputs used to decide whether a stationary controller-facing turn is allowed. */
struct SURVIVALRPG_API FRpgTurnInPlaceEligibilitySnapshot
{
	ERpgCharacterRotationMode RotationMode{};
	ERpgLocomotionMovementState MovementState{};
	float GroundSpeed = 0.0f;
	bool bHasTurnDatabase = false;
	bool bJumpPhaseGrounded = false;
	bool bIsMovingOnGround = false;
	bool bIsCrouching = false;
	bool bIsAnyMontagePlaying = false;
	bool bHasBlockingGameplayTag = false;
	bool bHasGroundedMoveIntent = false;
	bool bHasTrajectory = false;
};

/** Immutable proxy and callback observations consumed by one value-only lifecycle update. */
struct SURVIVALRPG_API FRpgTurnInPlaceUpdateSnapshot
{
	ERpgCharacterRotationMode RotationMode{};
	ERpgLocomotionMovementState MovementState{};
	float ActorYawDelta = 0.0f;
	bool bEligible = false;
	bool bProxyHardReset = false;
	bool bSupportChanged = false;
	bool bHasBlockingGameplayTag = false;
	bool bIsAnyMontagePlaying = false;
	bool bIsCrouching = false;
	bool bSelectionLatched = false;
	bool bPlaybackObserved = false;
	bool bPoseSelected = false;
	bool bCompletionArmed = false;
};

/** Value result applied back to the AnimInstance facade after one deterministic transition. */
struct SURVIVALRPG_API FRpgTurnInPlaceUpdateResult
{
	FRpgTurnInPlaceRuntimeState State;
	EOffsetRootBoneMode OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
	bool bResetOffsetRootEveryFrame = false;
	bool bClearSelection = false;
	bool bUseSyntheticTrajectory = false;
};

/** Deterministic cosmetic turn-in-place policy adapted from GASP's ShouldTurnInPlace domain. */
namespace RpgTurnInPlaceRuntime
{
	inline constexpr float IdleSpeedThreshold = 3.0f;
	inline constexpr float CollectThreshold = 20.0f;
	inline constexpr float ActivationThreshold = 30.0f;
	inline constexpr float CancelThreshold = 10.0f;
	inline constexpr float InactiveYawRateThreshold = 6.0f;
	inline constexpr float StableYawRateThreshold = 60.0f;
	inline constexpr float StabilityDuration = 0.08f;
	inline constexpr float CollectionTimeout = 0.2f;
	inline constexpr float RecoveryDuration = 0.15f;
	inline constexpr float SelectionTimeout = 0.25f;
	inline constexpr float ActiveTimeout = 1.75f;
	inline constexpr float PlaybackWatchdogSafetyMargin = 0.1f;
	inline constexpr float InactiveAccumulatorTimeout = 0.2f;
	inline constexpr float FinishedTimeTolerance = 0.05f;
	inline constexpr float LargePositionDelta = 200.0f;

	/** Returns whether the replicated rotation policy permits controller-facing turn presentation. */
	SURVIVALRPG_API bool SupportsTurnInPlace(ERpgCharacterRotationMode RotationMode);

	/** Returns the signed authored turn angle nearest to the request, or zero below activation. */
	SURVIVALRPG_API float QuantizeAngle(float SignedAngle);

	/** Returns the synthetic facing horizon for an authored 45/90/135/180-degree turn. */
	SURVIVALRPG_API float GetFacingDuration(float QuantizedAngle);

	/** Calculates a wrap-safe signed actor-yaw delta in degrees. */
	SURVIVALRPG_API float CalculateYawDelta(float PreviousActorYaw, float CurrentActorYaw);

	/** Detects a transition across the Free versus controller-facing presentation boundary. */
	SURVIVALRPG_API bool DidSupportChange(
		bool bHasPreviousSnapshot,
		ERpgCharacterRotationMode PreviousMode,
		ERpgCharacterRotationMode CurrentMode);

	/** Rebases actor yaw when the owner snapshot or facing policy changes. */
	SURVIVALRPG_API float CalculateSnapshotYawDelta(
		float PreviousActorYaw,
		float CurrentActorYaw,
		bool bHardReset,
		bool bSupportChanged);

	/** Builds a facing-only trajectory while preserving source sample times and positions exactly. */
	SURVIVALRPG_API FTransformTrajectory MakeSyntheticTrajectory(
		const FTransformTrajectory& SourceTrajectory,
		float CurrentActorYaw,
		float AccumulatedYaw,
		float QuantizedAngle);

	/** Resolves a bounded wall-clock watchdog for the selected non-looping animation. */
	SURVIVALRPG_API float CalculatePlaybackWatchdogDuration(
		float RemainingAnimationTime,
		float PlayRate,
		bool bLooping);

	/** Evaluates the pointer-free stationary controller-facing eligibility contract. */
	SURVIVALRPG_API bool IsEligible(const FRpgTurnInPlaceEligibilitySnapshot& Snapshot);

	/** Clears the value lifecycle while leaving monotonic request serials intact. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult Reset(
		const FRpgTurnInPlaceRuntimeState& State,
		bool bHardResetOffset);

	/** Starts bounded interpolation recovery and releases any selected presentation asset. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult BeginRecovery(
		const FRpgTurnInPlaceRuntimeState& State,
		bool bHardResetOffset);

	/** Starts or retargets one authored request while skipping serial zero. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult BeginRequest(
		const FRpgTurnInPlaceRuntimeState& State,
		float QuantizedAngle);

	/** Advances accumulation, hysteresis, reset edges, timeouts, and recovery from value inputs only. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult Update(
		const FRpgTurnInPlaceRuntimeState& State,
		const FRpgTurnInPlaceUpdateSnapshot& Snapshot,
		float DeltaSeconds);

	/** Consumes at most one ForceInterrupt for the active request serial. */
	SURVIVALRPG_API bool ConsumeForceInterrupt(
		bool bHasTurnDatabase,
		FRpgTurnInPlaceRuntimeState& State);

	/** Allows request retargeting only before the first exclusive search is dispatched. */
	SURVIVALRPG_API bool CanRetarget(
		const FRpgTurnInPlaceRuntimeState& State,
		bool bSelectionLatched);

	/** Resolves exclusive search, continuing-pose hold, or normal locomotion for the next update. */
	SURVIVALRPG_API ERpgTurnInPlaceSearchMode ResolveSearchMode(
		const FRpgTurnInPlaceRuntimeState& State,
		bool bForceNewRequest,
		bool bHasTurnDatabase,
		bool bSelectionLatched,
		bool bCompletionArmed);

	/** Allows moving procedural nodes only outside collection and active turn playback. */
	SURVIVALRPG_API bool AllowsMovingProceduralNodes(ERpgTurnInPlaceState State);
}
