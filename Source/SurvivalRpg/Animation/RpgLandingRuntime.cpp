// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgLandingRuntime.h"

#include "RpgAnimInstance.h"
#include "RpgMotionMatchingRuntime.h"

void RpgLandingRuntime::UpdateSelectionSnapshot(
	FRpgLandingSelectionSnapshot& SelectionSnapshot,
	FRpgLandingCaptureState& State,
	const FRpgLandingCaptureSnapshot& Snapshot,
	const FRpgGaspLocomotionTuning& Tuning)
{
	const bool bAirborne =
		Snapshot.bIsFalling ||
		Snapshot.MovementState == ERpgLocomotionMovementState::Airborne;
	const bool bGrounded =
		Snapshot.MovementState == ERpgLocomotionMovementState::Grounded &&
		Snapshot.bIsMovingOnGround;
	if (Snapshot.bHardReset || (!bAirborne && !bGrounded))
	{
		SelectionSnapshot = FRpgLandingSelectionSnapshot();
		State = FRpgLandingCaptureState();
		return;
	}

	// Capture gives supported grounded truth precedence if source flags are contradictory.
	if (bGrounded)
	{
		if (!State.bWasAirborne)
		{
			SelectionSnapshot = FRpgLandingSelectionSnapshot();
		}
		State.LastGroundedGait = Snapshot.Gait;
		State.bWasAirborne = false;
		return;
	}

	const bool bUpwardRelaunch =
		State.bWasAirborne &&
		SelectionSnapshot.bIsValid &&
		SelectionSnapshot.VerticalVelocity <= UE_KINDA_SMALL_NUMBER &&
		Snapshot.VerticalVelocity > UE_KINDA_SMALL_NUMBER;
	if (!State.bWasAirborne || bUpwardRelaunch)
	{
		State.AirborneEpoch = State.AirborneEpoch >= MAX_int32
			? 1
			: State.AirborneEpoch + 1;
		SelectionSnapshot = FRpgLandingSelectionSnapshot();
		SelectionSnapshot.AirborneEpoch = State.AirborneEpoch;
	}
	State.bWasAirborne = true;

	FVector GravityDirection = FVector::ZeroVector;
	float GravityMagnitude = 0.0f;
	Snapshot.GravityAcceleration.ToDirectionAndLength(GravityDirection, GravityMagnitude);
	const FVector HorizontalVelocity(
		Snapshot.WorldVelocity.X,
		Snapshot.WorldVelocity.Y,
		0.0f);
	const float HorizontalSpeed = HorizontalVelocity.Size();
	const bool bFiniteInputs =
		!Snapshot.WorldVelocity.ContainsNaN() &&
		!HorizontalVelocity.ContainsNaN() &&
		FMath::IsFinite(HorizontalSpeed) &&
		FMath::IsFinite(Snapshot.VerticalVelocity) &&
		FMath::IsFinite(Snapshot.InputMagnitude) && Snapshot.InputMagnitude >= 0.0f &&
		!Snapshot.GravityAcceleration.ContainsNaN() &&
		!GravityDirection.ContainsNaN() &&
		FMath::IsFinite(GravityMagnitude) && GravityMagnitude > UE_SMALL_NUMBER;
	if (!bFiniteInputs)
	{
		SelectionSnapshot = FRpgLandingSelectionSnapshot();
		SelectionSnapshot.AirborneEpoch = State.AirborneEpoch;
		return;
	}

	const bool bHasMoveIntent = Snapshot.InputMagnitude > Tuning.MoveIntentThreshold;
	ERpgLocomotionGait CapturedGait = State.LastGroundedGait;
	if (CapturedGait != ERpgLocomotionGait::Sprint && bHasMoveIntent)
	{
		CapturedGait = Snapshot.InputMagnitude < Tuning.RunInputThreshold
			? ERpgLocomotionGait::Walk
			: ERpgLocomotionGait::Run;
	}
	else if (CapturedGait == ERpgLocomotionGait::Idle &&
		HorizontalSpeed > Tuning.StationarySpeedThreshold)
	{
		CapturedGait = ERpgLocomotionGait::Run;
	}

	const float CurrentDownwardSpeed = FMath::Max(
		FVector::DotProduct(Snapshot.WorldVelocity, GravityDirection),
		0.0f);
	SelectionSnapshot.HorizontalVelocity = HorizontalVelocity;
	SelectionSnapshot.HorizontalSpeed = HorizontalSpeed;
	SelectionSnapshot.VerticalVelocity = Snapshot.VerticalVelocity;
	SelectionSnapshot.MaximumDownwardSpeed = FMath::Max(
		SelectionSnapshot.MaximumDownwardSpeed,
		CurrentDownwardSpeed);
	SelectionSnapshot.Gait = CapturedGait;
	SelectionSnapshot.bHasMoveIntent = bHasMoveIntent;
	// Prediction is current-frame evidence only; measured impact remains accumulated for the epoch.
	SelectionSnapshot.PredictedLanding = FRpgTrajectoryLandingPrediction();
	SelectionSnapshot.PredictedImpactDownwardSpeed = 0.0f;
	if (Snapshot.TrajectoryPrediction.bIsValid &&
		FMath::IsFinite(Snapshot.TrajectoryPrediction.TimeToLand) &&
		Snapshot.TrajectoryPrediction.TimeToLand >= 0.0f &&
		!Snapshot.TrajectoryPrediction.LandingLocation.ContainsNaN() &&
		!Snapshot.TrajectoryPrediction.LandingNormal.ContainsNaN())
	{
		SelectionSnapshot.PredictedLanding = Snapshot.TrajectoryPrediction;
		SelectionSnapshot.PredictedImpactDownwardSpeed = FMath::Max(
			FVector::DotProduct(Snapshot.WorldVelocity, GravityDirection) +
				GravityMagnitude * Snapshot.TrajectoryPrediction.TimeToLand,
			0.0f);
	}
	SelectionSnapshot.bIsValid =
		FMath::IsFinite(SelectionSnapshot.MaximumDownwardSpeed) &&
		FMath::IsFinite(SelectionSnapshot.PredictedImpactDownwardSpeed);
}

ERpgMotionMatchingDatabaseRole RpgLandingRuntime::ResolveDatabaseRole(
	const FRpgLandingSelectionSnapshot& Snapshot,
	const FRpgGaspLocomotionTuning& Tuning)
{
	const bool bFiniteSnapshot =
		Snapshot.bIsValid &&
		Snapshot.AirborneEpoch != 0 &&
		!Snapshot.HorizontalVelocity.ContainsNaN() &&
		FMath::IsFinite(Snapshot.HorizontalSpeed) && Snapshot.HorizontalSpeed >= 0.0f &&
		FMath::IsFinite(Snapshot.VerticalVelocity) &&
		FMath::IsFinite(Snapshot.MaximumDownwardSpeed) &&
		Snapshot.MaximumDownwardSpeed >= 0.0f &&
		FMath::IsFinite(Snapshot.PredictedImpactDownwardSpeed) &&
		Snapshot.PredictedImpactDownwardSpeed >= 0.0f &&
		FMath::IsFinite(Tuning.HeavyLandingSpeedThreshold) &&
		Tuning.HeavyLandingSpeedThreshold > 0.0f;
	if (!bFiniteSnapshot)
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	if (Snapshot.PredictedLanding.bIsValid &&
		(!FMath::IsFinite(Snapshot.PredictedLanding.TimeToLand) ||
		 Snapshot.PredictedLanding.TimeToLand < 0.0f ||
		 Snapshot.PredictedLanding.LandingLocation.ContainsNaN() ||
		 Snapshot.PredictedLanding.LandingNormal.ContainsNaN()))
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	const bool bHeavy = FMath::Max(
		Snapshot.MaximumDownwardSpeed,
		Snapshot.PredictedImpactDownwardSpeed) >= Tuning.HeavyLandingSpeedThreshold;
	if (Snapshot.HorizontalSpeed <= Tuning.StationarySpeedThreshold)
	{
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::StandHeavyLanding
			: ERpgMotionMatchingDatabaseRole::StandLightLanding;
	}

	switch (Snapshot.Gait)
	{
	case ERpgLocomotionGait::Walk:
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::WalkHeavyLanding
			: ERpgMotionMatchingDatabaseRole::WalkLightLanding;
	case ERpgLocomotionGait::Run:
	case ERpgLocomotionGait::Sprint:
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::RunHeavyLanding
			: ERpgMotionMatchingDatabaseRole::RunLightLanding;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
}

ERpgMotionMatchingDatabaseRole RpgLandingRuntime::ResolveStationaryRole(
	ERpgMotionMatchingDatabaseRole LandingRole)
{
	switch (LandingRole)
	{
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
		return ERpgMotionMatchingDatabaseRole::StandLightLanding;
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return ERpgMotionMatchingDatabaseRole::StandHeavyLanding;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
}

bool RpgLandingRuntime::ShouldReleaseStationary(
	ERpgMotionMatchingDatabaseRole LandingRole,
	bool bChooserMoving,
	float GroundSpeed,
	const FRpgGaspLocomotionTuning& Tuning)
{
	const bool bStationaryLanding =
		LandingRole == ERpgMotionMatchingDatabaseRole::StandLightLanding ||
		LandingRole == ERpgMotionMatchingDatabaseRole::StandHeavyLanding;
	return bStationaryLanding &&
		(bChooserMoving ||
		 !FMath::IsFinite(GroundSpeed) ||
		 GroundSpeed > Tuning.StationarySpeedThreshold);
}

ERpgMotionMatchingDatabaseRole RpgLandingRuntime::ResolveStationaryMovementRole(
	ERpgMotionMatchingDatabaseRole LandingRole,
	ERpgLocomotionGait LiveGait)
{
	const bool bLight = LandingRole == ERpgMotionMatchingDatabaseRole::StandLightLanding;
	const bool bHeavy = LandingRole == ERpgMotionMatchingDatabaseRole::StandHeavyLanding;
	if (!bLight && !bHeavy)
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	switch (LiveGait)
	{
	case ERpgLocomotionGait::Walk:
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::WalkHeavyLanding
			: ERpgMotionMatchingDatabaseRole::WalkLightLanding;
	case ERpgLocomotionGait::Run:
	case ERpgLocomotionGait::Sprint:
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::RunHeavyLanding
			: ERpgMotionMatchingDatabaseRole::RunLightLanding;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
}

bool RpgLandingRuntime::IsRoleAvailable(
	ERpgMotionMatchingDatabaseRole Role,
	const FRpgLandingDatabaseAvailability& Availability)
{
	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
		return Availability.bStandLight;
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
		return Availability.bStandHeavy;
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
		return Availability.bWalkLight;
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
		return Availability.bWalkHeavy;
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
		return Availability.bRunLight;
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return Availability.bRunHeavy;
	default:
		return false;
	}
}

ERpgMotionMatchingDatabaseRole RpgLandingRuntime::ResolveAvailableRole(
	ERpgMotionMatchingDatabaseRole RequestedRole,
	const FRpgLandingDatabaseAvailability& Availability)
{
	if (IsRoleAvailable(RequestedRole, Availability))
	{
		return RequestedRole;
	}

	switch (RequestedRole)
	{
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
		RequestedRole = ERpgMotionMatchingDatabaseRole::StandLightLanding;
		break;
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
		RequestedRole = ERpgMotionMatchingDatabaseRole::WalkLightLanding;
		break;
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		RequestedRole = ERpgMotionMatchingDatabaseRole::RunLightLanding;
		break;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
	return IsRoleAvailable(RequestedRole, Availability)
		? RequestedRole
		: ERpgMotionMatchingDatabaseRole::None;
}

bool RpgLandingRuntime::IsEligible(const FRpgLandingEligibilitySnapshot& Snapshot)
{
	return Snapshot.MovementState == ERpgLocomotionMovementState::Grounded &&
		Snapshot.bIsMovingOnGround &&
		!Snapshot.bIsCrouching &&
		!Snapshot.bIsAnyMontagePlaying &&
		!Snapshot.bHasBlockingGameplayTag;
}

ERpgMotionMatchingDatabaseRole RpgLandingRuntime::ResolveTouchdownRole(
	const FRpgLandingSelectionSnapshot& SelectionSnapshot,
	const FRpgLandingEligibilitySnapshot& Eligibility,
	const FRpgLandingDatabaseAvailability& Availability,
	ERpgLocomotionGait LiveGait,
	float GroundSpeed,
	bool bChooserMoving,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (!IsEligible(Eligibility))
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	ERpgMotionMatchingDatabaseRole LandingRole = ResolveDatabaseRole(
		SelectionSnapshot,
		Tuning);
	if (FMath::IsFinite(GroundSpeed) && GroundSpeed <= Tuning.StationarySpeedThreshold)
	{
		LandingRole = ResolveStationaryRole(LandingRole);
	}
	LandingRole = ResolveAvailableRole(LandingRole, Availability);
	if (ShouldReleaseStationary(LandingRole, bChooserMoving, GroundSpeed, Tuning))
	{
		LandingRole = bChooserMoving
			? ResolveAvailableRole(
				ResolveStationaryMovementRole(LandingRole, LiveGait),
				Availability)
			: ERpgMotionMatchingDatabaseRole::None;
	}
	return LandingRole;
}

FRpgLandingRuntimeResult RpgLandingRuntime::Reset(
	const FRpgLandingRuntimeState& State,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgLandingRuntimeResult Result;
	Result.State = State;
	Result.State.ActiveRole = ERpgMotionMatchingDatabaseRole::None;
	Result.State.StateElapsed = 0.0f;
	Result.State.TouchdownElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = Tuning.LandingActiveTimeout;
	Result.State.bSelectionLatched = false;
	Result.State.bCompletionArmed = false;
	Result.Transition = ERpgLandingRuntimeTransition::ResetGrounded;
	Result.bClearSelection = true;
	Result.bClearBackwardHold = true;
	return Result;
}

FRpgLandingRuntimeResult RpgLandingRuntime::BeginRequest(
	const FRpgLandingRuntimeState& State,
	ERpgMotionMatchingDatabaseRole LandingRole,
	bool bForceInterrupt,
	const FRpgGaspLocomotionTuning& Tuning)
{
	check(RpgMotionMatchingRuntime::IsLandingDatabaseRole(LandingRole));
	FRpgLandingRuntimeResult Result;
	Result.State = State;
	Result.State.ActiveRole = LandingRole;
	Result.State.StateElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = Tuning.LandingActiveTimeout;
	Result.State.bSelectionLatched = false;
	Result.State.bCompletionArmed = false;
	++Result.State.RequestSerial;
	if (Result.State.RequestSerial == 0)
	{
		++Result.State.RequestSerial;
	}
	if (bForceInterrupt)
	{
		Result.State.TouchdownElapsed = 0.0f;
	}
	else
	{
		Result.State.InterruptedRequestSerial = Result.State.RequestSerial;
	}
	Result.Transition = ERpgLandingRuntimeTransition::BeginLanding;
	Result.bClearSelection = true;
	Result.bClearBackwardHold = true;
	return Result;
}

FRpgLandingRuntimeResult RpgLandingRuntime::UpdateActive(
	const FRpgLandingRuntimeState& State,
	const FRpgLandingActiveSnapshot& Snapshot,
	float DeltaSeconds,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgLandingRuntimeResult Result;
	Result.State = State;
	if (!IsEligible(Snapshot.Eligibility) ||
		!RpgMotionMatchingRuntime::IsLandingDatabaseRole(Result.State.ActiveRole) ||
		!IsRoleAvailable(Result.State.ActiveRole, Snapshot.Availability))
	{
		return Reset(Result.State, Tuning);
	}

	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	Result.State.TouchdownElapsed += SafeDeltaSeconds;
	if (ShouldReleaseStationary(
		Result.State.ActiveRole,
		Snapshot.bChooserMoving,
		Snapshot.GroundSpeed,
		Tuning))
	{
		const ERpgMotionMatchingDatabaseRole HandoffRole =
			Snapshot.bChooserMoving &&
			Result.State.TouchdownElapsed <= Tuning.LandingMovementHandoffWindow
				? ResolveAvailableRole(
					ResolveStationaryMovementRole(Result.State.ActiveRole, Snapshot.LiveGait),
					Snapshot.Availability)
				: ERpgMotionMatchingDatabaseRole::None;
		return HandoffRole != ERpgMotionMatchingDatabaseRole::None
			? BeginRequest(Result.State, HandoffRole, false, Tuning)
			: Reset(Result.State, Tuning);
	}

	Result.State.StateElapsed += SafeDeltaSeconds;
	const bool bSelectionTimedOut =
		!Result.State.bSelectionLatched &&
		Result.State.StateElapsed >= Tuning.LandingSelectionTimeout;
	const bool bPlaybackTimedOut =
		Result.State.bSelectionLatched &&
		Result.State.StateElapsed >= Result.State.PlaybackWatchdogDuration;
	if (Result.State.bCompletionArmed || bSelectionTimedOut || bPlaybackTimedOut)
	{
		return Reset(Result.State, Tuning);
	}
	return Result;
}

bool RpgLandingRuntime::ConsumeForceInterrupt(
	bool bLandingPhase,
	bool bHasActiveDatabase,
	FRpgLandingRuntimeState& State)
{
	if (!bLandingPhase ||
		!bHasActiveDatabase ||
		State.InterruptedRequestSerial == State.RequestSerial)
	{
		return false;
	}

	State.InterruptedRequestSerial = State.RequestSerial;
	return true;
}

bool RpgLandingRuntime::ShouldInterruptDatabaseExit(
	ERpgJumpPhase CurrentJumpPhase,
	bool bCompletionArmed,
	ERpgMotionMatchingDatabaseRole CurrentDatabaseRole)
{
	return RpgMotionMatchingRuntime::IsLandingDatabaseRole(CurrentDatabaseRole) &&
		(CurrentJumpPhase != ERpgJumpPhase::Landing || bCompletionArmed);
}

float RpgLandingRuntime::CalculatePlaybackWatchdogDuration(
	float RemainingAnimationTime,
	float PlayRate,
	bool bLooping,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (bLooping || !FMath::IsFinite(PlayRate) || FMath::Abs(PlayRate) <= UE_SMALL_NUMBER)
	{
		return Tuning.LandingActiveTimeout;
	}

	return FMath::Clamp(
		FMath::Max(RemainingAnimationTime, 0.0f) / FMath::Abs(PlayRate) +
			PlaybackWatchdogSafetyMargin,
		PlaybackWatchdogSafetyMargin,
		Tuning.LandingActiveTimeout);
}
