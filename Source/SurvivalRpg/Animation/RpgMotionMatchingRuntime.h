// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RpgGaspLocomotionConfig.h"

enum class ERpgCharacterRotationMode : uint8;
enum class ERpgLocomotionGait : uint8;
enum class ERpgLocomotionMovementState : uint8;
enum class ERpgLocomotionStance : uint8;
enum class EPoseSearchInterruptMode : uint8;

/**
 * Pointer-free locomotion snapshot used to classify one bounded project database search.
 *
 * The snapshot deliberately contains no actor, controller, authority, network-role, or asset
 * state so identical game-thread snapshots resolve identically on local and simulated characters.
 */
struct SURVIVALRPG_API FRpgGroundMotionMatchingSelectionSnapshot
{
	ERpgLocomotionGait Gait{};
	ERpgLocomotionStance Stance{};
	ERpgLocomotionMovementState MovementState{};
	ERpgCharacterRotationMode RotationMode{};
	FVector WorldVelocity = FVector::ZeroVector;
	FVector WorldAcceleration = FVector::ZeroVector;
	FVector FutureVelocity = FVector::ZeroVector;
	float GroundSpeed = 0.0f;

	/** Role captured by the completed-search callback from the latest Motion Matching result. */
	ERpgMotionMatchingDatabaseRole CurrentDatabaseRole{};

	bool bIsMovingOnGround = false;
};

/** High-level selector state used to preserve current poses across transient role-list changes. */
struct SURVIVALRPG_API FRpgGroundMotionMatchingDomainState
{
	ERpgLocomotionMovementState PhysicalMovementState{};
	ERpgLocomotionGait Gait{};
	ERpgLocomotionStance Stance{};
	bool bChooserMoving = false;
};

/** Value-only completed-search result consumed by the Motion Matching callback bridge. */
struct SURVIVALRPG_API FRpgMotionMatchingPostSelectionState
{
	ERpgMotionMatchingDatabaseRole CurrentDatabaseRole{};
	EPoseSearchInterruptMode InterruptMode{};
	bool bIsContinuingPose = false;
	bool bShouldLatchTurnInPlace = false;
	bool bShouldLatchLanding = false;
};

/** Ordered, pointer-free database roles produced for one Motion Matching search. */
using FRpgResolvedMotionMatchingDatabaseRoles =
	TArray<ERpgMotionMatchingDatabaseRole, TInlineAllocator<4>>;

/** Deterministic value-only rules adapted from the relevant GASP Sparse chooser domains. */
namespace RpgMotionMatchingRuntime
{
	/** Mirrors GASP's logical Moving state from finite horizontal velocity and acceleration. */
	SURVIVALRPG_API bool IsChooserMoving(
		const FRpgGroundMotionMatchingSelectionSnapshot& Snapshot,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Resolves the source GASP pivot threshold for the active facing policy, in degrees. */
	SURVIVALRPG_API float GetRunPivotMinimumAngle(
		ERpgCharacterRotationMode RotationMode,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Returns true only for source-level domain changes that may interrupt a continuing pose. */
	SURVIVALRPG_API bool ShouldInterruptGroundMotionMatching(
		bool bHasPreviousState,
		const FRpgGroundMotionMatchingDomainState& PreviousState,
		const FRpgGroundMotionMatchingDomainState& CurrentState);

	/**
	 * Evaluates the pointer-free project chooser contract and returns ordered database roles.
	 * Airborne and crouching domains are resolved before grounded Idle/Walk/Run/Sprint rows.
	 */
	SURVIVALRPG_API FRpgResolvedMotionMatchingDatabaseRoles ResolveDatabaseRoles(
		const FRpgGroundMotionMatchingSelectionSnapshot& Snapshot,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Returns true for one of the six curated Idle/Walk/Run Light/Heavy landing roles. */
	SURVIVALRPG_API bool IsLandingDatabaseRole(ERpgMotionMatchingDatabaseRole Role);

	/** Central completed-search policy for role, Continuing Pose, interrupt, and exclusive latches. */
	SURVIVALRPG_API FRpgMotionMatchingPostSelectionState ResolvePostSelection(
		ERpgMotionMatchingDatabaseRole SelectedRole,
		bool bIsContinuingPose,
		EPoseSearchInterruptMode InterruptMode,
		bool bCanLatchTurnInPlace,
		bool bCanLatchLanding);
}
