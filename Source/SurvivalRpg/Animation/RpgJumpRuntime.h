// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERpgJumpPhase : uint8;
enum class ERpgLocomotionMovementState : uint8;

/** Physical movement edge observed by the cosmetic jump presentation lifecycle. */
enum class ERpgJumpPhysicalTransition : uint8
{
	None,
	EnterAirborne,
	Touchdown,
	ResetGrounded,
};

/** Immutable CharacterMovement snapshot used to interpret one jump presentation edge. */
struct SURVIVALRPG_API FRpgJumpPhysicalSnapshot
{
	ERpgLocomotionMovementState MovementState{};
	float VerticalVelocity = 0.0f;
	bool bIsFalling = false;
	bool bHardReset = false;
};

/** Pointer-free result used by the AnimInstance facade to enter air, resolve touchdown, or reset. */
struct SURVIVALRPG_API FRpgJumpPhysicalTransitionResult
{
	ERpgJumpPhysicalTransition Transition = ERpgJumpPhysicalTransition::None;
	bool bAscendingTakeoff = false;
};

/** Value state for the bounded backward Jump Start Continuing-Pose policy. */
struct SURVIVALRPG_API FRpgBackwardJumpStartHoldState
{
	float HoldElapsed = 0.0f;
	bool bOpportunityConsumed = false;
	bool bHoldEligible = false;
	bool bHoldWasArmed = false;
};

/** Immutable asset-trait and playback observations supplied by the GC-safe AnimInstance bridge. */
struct SURVIVALRPG_API FRpgBackwardJumpStartPlaybackSnapshot
{
	ERpgJumpPhase JumpPhase{};
	float CurrentAssetTime = 0.0f;
	float CurrentAssetLength = 0.0f;
	float CurrentAssetPlayRate = 1.0f;
	float DeltaSeconds = 0.0f;
	bool bCurrentAssetIsAirborne = false;
	bool bCurrentAssetIsBackwardStart = false;
	bool bHasHeldAsset = false;
	bool bCurrentAssetMatchesHeld = false;
};

/** Value result applied around the AnimInstance's GC-tracked held-asset pointer. */
struct SURVIVALRPG_API FRpgBackwardJumpStartHoldResult
{
	FRpgBackwardJumpStartHoldState State;
	bool bCaptureCurrentAsset = false;
	bool bClearHeldAsset = false;
	bool bHoldContinuingPose = false;
};

/** Deterministic jump-edge and airborne Continuing-Pose policy adapted from the project GASP pilot. */
namespace RpgJumpRuntime
{
	inline constexpr float BackwardStartHoldTimeout = 1.25f;
	inline constexpr float BackwardStartReleaseLeadTime = 0.2f;

	/** Interprets CharacterMovement truth without starting or selecting a landing presentation. */
	SURVIVALRPG_API FRpgJumpPhysicalTransitionResult ResolvePhysicalTransition(
		ERpgJumpPhase CurrentPhase,
		const FRpgJumpPhysicalSnapshot& Snapshot);

	/** Retains the fall loop only after the current airborne phase actually used the backward-start path. */
	SURVIVALRPG_API bool ShouldHoldLoopingAirborneFallPlayback(
		ERpgJumpPhase CurrentPhase,
		bool bBackwardHoldWasArmed,
		float CurrentVerticalVelocity,
		bool bCurrentAssetIsLoopingFall);

	/** Bounds a continuing backward start by playback remainder and an independent wall-clock watchdog. */
	SURVIVALRPG_API bool ShouldHoldBackwardJumpStartPlayback(
		ERpgJumpPhase CurrentPhase,
		bool bCurrentAssetMatchesHeldSelection,
		float CurrentAssetTime,
		float CurrentAssetLength,
		float CurrentAssetPlayRate,
		float HoldElapsed);

	/** Resets the value hold and tells the facade to release any GC-tracked held asset. */
	SURVIVALRPG_API FRpgBackwardJumpStartHoldResult ResetBackwardJumpStartHold();

	/** Advances the one-opportunity backward-start latch from pointer-free trait and playback observations. */
	SURVIVALRPG_API FRpgBackwardJumpStartHoldResult UpdateBackwardJumpStartHold(
		const FRpgBackwardJumpStartHoldState& State,
		const FRpgBackwardJumpStartPlaybackSnapshot& Snapshot);
}
