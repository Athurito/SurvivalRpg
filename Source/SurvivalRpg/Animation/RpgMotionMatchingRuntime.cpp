// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgMotionMatchingRuntime.h"

#include "RpgAnimInstance.h"

bool RpgMotionMatchingRuntime::IsChooserMoving(
	const FRpgGroundMotionMatchingSelectionSnapshot& Snapshot,
	const FRpgGaspLocomotionTuning& Tuning)
{
	return Snapshot.bIsMovingOnGround &&
		!Snapshot.WorldVelocity.ContainsNaN() &&
		!Snapshot.WorldAcceleration.ContainsNaN() &&
		Snapshot.WorldVelocity.SizeSquared2D() > FMath::Square(Tuning.ChooserVelocityTolerance) &&
		Snapshot.WorldAcceleration.SizeSquared2D() > FMath::Square(Tuning.ChooserAccelerationTolerance);
}

float RpgMotionMatchingRuntime::GetRunPivotMinimumAngle(
	ERpgCharacterRotationMode RotationMode,
	const FRpgGaspLocomotionTuning& Tuning)
{
	switch (RotationMode)
	{
	case ERpgCharacterRotationMode::Free:
		return Tuning.FreeRunPivotMinimumAngle;
	case ERpgCharacterRotationMode::CombatStrafe:
		return Tuning.CombatStrafeRunPivotMinimumAngle;
	case ERpgCharacterRotationMode::Aim:
		return Tuning.AimRunPivotMinimumAngle;
	default:
		return Tuning.FreeRunPivotMinimumAngle;
	}
}

bool RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(
	bool bHasPreviousState,
	const FRpgGroundMotionMatchingDomainState& PreviousState,
	const FRpgGroundMotionMatchingDomainState& CurrentState)
{
	if (!bHasPreviousState ||
		PreviousState.PhysicalMovementState != CurrentState.PhysicalMovementState)
	{
		return true;
	}

	if (CurrentState.PhysicalMovementState != ERpgLocomotionMovementState::Grounded)
	{
		return false;
	}

	return PreviousState.bChooserMoving != CurrentState.bChooserMoving ||
		PreviousState.Stance != CurrentState.Stance ||
		(CurrentState.bChooserMoving && PreviousState.Gait != CurrentState.Gait);
}

bool RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
	ERpgMotionMatchingDatabaseRole CurrentDatabaseRole,
	bool bChooserMoving,
	float GroundSpeed,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (bChooserMoving || !FMath::IsFinite(GroundSpeed) || GroundSpeed < 0.0f ||
		!FMath::IsFinite(Tuning.ChooserVelocityTolerance) || Tuning.ChooserVelocityTolerance < 0.0f ||
		GroundSpeed > Tuning.ChooserVelocityTolerance)
	{
		return false;
	}

	switch (CurrentDatabaseRole)
	{
	case ERpgMotionMatchingDatabaseRole::StandWalkStops:
	case ERpgMotionMatchingDatabaseRole::StandRunStops:
	case ERpgMotionMatchingDatabaseRole::StandSprintStops:
		return true;
	default:
		return false;
	}
}

FRpgResolvedMotionMatchingDatabaseRoles RpgMotionMatchingRuntime::ResolveDatabaseRoles(
	const FRpgGroundMotionMatchingSelectionSnapshot& Snapshot,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgResolvedMotionMatchingDatabaseRoles ResolvedRoles;
	if (Snapshot.MovementState == ERpgLocomotionMovementState::Airborne)
	{
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::Jump);
		return ResolvedRoles;
	}

	if (Snapshot.MovementState != ERpgLocomotionMovementState::Grounded ||
		!Snapshot.bIsMovingOnGround)
	{
		return ResolvedRoles;
	}

	if (Snapshot.Stance == ERpgLocomotionStance::Crouching)
	{
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::Crouch);
		return ResolvedRoles;
	}

	if (Snapshot.Stance != ERpgLocomotionStance::Standing)
	{
		return ResolvedRoles;
	}

	const float SafeGroundSpeed = FMath::IsFinite(Snapshot.GroundSpeed)
		? FMath::Max(Snapshot.GroundSpeed, 0.0f)
		: 0.0f;
	const bool bChooserMoving = IsChooserMoving(Snapshot, Tuning);
	if (!bChooserMoving)
	{
		// GASP's logical Idle rows are inclusive and intentionally overlap. Preserve their source
		// order so exact boundaries expose both adjacent roles to the Pose Search cost comparison.
		if (SafeGroundSpeed <= Tuning.WalkStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandIdle);
		}
		if (SafeGroundSpeed >= Tuning.WalkStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandWalkStops);
		}
		if (SafeGroundSpeed >= Tuning.RunStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunStops);
		}
		// Project Run reaches 600 cm/s, so source's speed-only Sprint Stop row also requires the
		// explicit cosmetic Sprint gait. This keeps ordinary Run Stops out of forward-only clips.
		if (Snapshot.Gait == ERpgLocomotionGait::Sprint &&
			SafeGroundSpeed >= Tuning.SprintStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandSprintStops);
		}
		return ResolvedRoles;
	}

	switch (Snapshot.Gait)
	{
	case ERpgLocomotionGait::Idle:
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandIdle);
		break;
	case ERpgLocomotionGait::Walk:
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandWalk);
		break;
	case ERpgLocomotionGait::Run:
	{
		float SafeFutureGroundSpeed = Snapshot.FutureVelocity.Size2D();
		if (!FMath::IsFinite(SafeFutureGroundSpeed))
		{
			SafeFutureGroundSpeed = 0.0f;
		}
		const bool bSearchStarts =
			Snapshot.CurrentDatabaseRole != ERpgMotionMatchingDatabaseRole::StandRunPivots &&
			SafeFutureGroundSpeed >= SafeGroundSpeed + Tuning.RunStartMinimumFutureSpeedGain;

		const FVector HorizontalVelocity(Snapshot.WorldVelocity.X, Snapshot.WorldVelocity.Y, 0.0f);
		const FVector HorizontalAcceleration(
			Snapshot.WorldAcceleration.X,
			Snapshot.WorldAcceleration.Y,
			0.0f);
		const float TurnAngle = FMath::Abs(FMath::FindDeltaAngleDegrees(
			HorizontalVelocity.Rotation().Yaw,
			HorizontalAcceleration.Rotation().Yaw));
		const bool bSearchPivots =
			TurnAngle >= GetRunPivotMinimumAngle(Snapshot.RotationMode, Tuning);

		ResolvedRoles.Reserve(3);
		if (bSearchStarts)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunStarts);
		}
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunLoops);
		if (bSearchPivots)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunPivots);
		}
		break;
	}
	case ERpgLocomotionGait::Sprint:
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandSprint);
		break;
	default:
		break;
	}
	return ResolvedRoles;
}

bool RpgMotionMatchingRuntime::IsLandingDatabaseRole(
	ERpgMotionMatchingDatabaseRole Role)
{
	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return true;
	default:
		return false;
	}
}

FRpgMotionMatchingPostSelectionState RpgMotionMatchingRuntime::ResolvePostSelection(
	ERpgMotionMatchingDatabaseRole SelectedRole,
	bool bIsContinuingPose,
	EPoseSearchInterruptMode InterruptMode,
	bool bCanLatchTurnInPlace,
	bool bCanLatchLanding)
{
	FRpgMotionMatchingPostSelectionState State;
	State.CurrentDatabaseRole = SelectedRole;
	State.InterruptMode = InterruptMode;
	State.bIsContinuingPose = bIsContinuingPose;
	State.bShouldLatchTurnInPlace =
		!bIsContinuingPose && bCanLatchTurnInPlace &&
		SelectedRole == ERpgMotionMatchingDatabaseRole::StandTurnInPlace;
	State.bShouldLatchLanding =
		!bIsContinuingPose && bCanLatchLanding &&
		IsLandingDatabaseRole(SelectedRole);
	return State;
}
