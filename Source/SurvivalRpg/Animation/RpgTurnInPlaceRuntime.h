// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/TrajectoryTypes.h"
#include "AnimationWarpingTypes.h"
#include "RpgGaspLocomotionConfig.h"

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
	/** First clear root-progress direction for an ambiguous half-turn (0 unknown, otherwise +/-1); reset per request. */
	float MeasuredRootYawDirection = 0.0f;
	/** World actor-facing target at request/recovery entry; unchanged goals cannot requeue overshoot. */
	float TargetYawAnchor = 0.0f;
	bool bHasTargetYawAnchor = false;
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
	/** Current authoritative actor yaw, in world degrees; consumed only with valid root feedback. */
	float ActorYaw = 0.0f;
	/** Signed shortest gap from the last evaluated visual root to ActorYaw, after removing mesh basis. */
	float RootYawGap = 0.0f;
	/** True only for a finite, relevant, previously evaluated root snapshot from this lifecycle. */
	bool bHasRootYawGap = false;
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
	inline constexpr float PlaybackWatchdogSafetyMargin = 0.1f;
	inline constexpr float FinishedTimeTolerance = 0.05f;

	/** Returns whether the replicated rotation policy permits controller-facing turn presentation. */
	SURVIVALRPG_API bool SupportsTurnInPlace(ERpgCharacterRotationMode RotationMode);

	/** Returns the signed authored turn angle nearest to the request, or zero below activation. */
	SURVIVALRPG_API float QuantizeAngle(
		float SignedAngle,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Returns the synthetic facing horizon for an authored 45/90/135/180-degree turn. */
	SURVIVALRPG_API float GetFacingDuration(
		float QuantizedAngle,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Calculates a wrap-safe signed actor-yaw delta in degrees. */
	SURVIVALRPG_API float CalculateYawDelta(float PreviousActorYaw, float CurrentActorYaw);

	/** Removes the mesh basis and calculates a world-facing gap; invalid inputs return false and zero. */
	SURVIVALRPG_API bool CalculateRootYawGap(
		const FQuat& EvaluatedMeshRootRotation,
		const FQuat& MeshBasisRotation,
		float ActorYaw,
		float& OutRootYawGap);

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

	/** Builds mesh-facing samples from actor-space yaw while preserving source times and positions. */
	SURVIVALRPG_API FTransformTrajectory MakeSyntheticTrajectory(
		const FTransformTrajectory& SourceTrajectory,
		float CurrentActorYaw,
		float AccumulatedYaw,
		float QuantizedAngle,
		const FQuat& MeshBasisRotation,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Compatibility helper for callers whose authored mesh and actor facing bases are identical. */
	SURVIVALRPG_API FTransformTrajectory MakeSyntheticTrajectory(
		const FTransformTrajectory& SourceTrajectory,
		float CurrentActorYaw,
		float AccumulatedYaw,
		float QuantizedAngle,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/**
	 * Keeps time zero/past aligned with the component basis of the last evaluated Pose History pose.
	 * Positive samples supply current world-facing intent; the trajectory interpolates between samples.
	 * The evaluated component rotation must accompany the same pose as the root feedback snapshot.
	 */
	SURVIVALRPG_API FTransformTrajectory MakeStationaryFacingTrajectory(
		const FTransformTrajectory& SourceTrajectory,
		float ActorYaw,
		const FQuat& MeshBasisRotation,
		const FQuat& LastEvaluatedComponentRotation);

	/** Resolves a finite watchdog at least as long as the configured fallback or selected clip. */
	SURVIVALRPG_API float CalculatePlaybackWatchdogDuration(
		float RemainingAnimationTime,
		float PlayRate,
		bool bLooping,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Evaluates the pointer-free stationary controller-facing eligibility contract. */
	SURVIVALRPG_API bool IsEligible(
		const FRpgTurnInPlaceEligibilitySnapshot& Snapshot,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Clears the value lifecycle while leaving monotonic request serials intact. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult Reset(
		const FRpgTurnInPlaceRuntimeState& State,
		bool bHardResetOffset,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Starts bounded interpolation recovery and releases any selected presentation asset. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult BeginRecovery(
		const FRpgTurnInPlaceRuntimeState& State,
		bool bHardResetOffset,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Starts or retargets one authored request while skipping serial zero. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult BeginRequest(
		const FRpgTurnInPlaceRuntimeState& State,
		float QuantizedAngle,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Advances accumulation, hysteresis, reset edges, timeouts, and recovery from value inputs only. */
	SURVIVALRPG_API FRpgTurnInPlaceUpdateResult Update(
		const FRpgTurnInPlaceRuntimeState& State,
		const FRpgTurnInPlaceUpdateSnapshot& Snapshot,
		float DeltaSeconds,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

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
