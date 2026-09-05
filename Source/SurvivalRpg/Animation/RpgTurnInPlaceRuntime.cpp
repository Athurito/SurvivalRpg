// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgTurnInPlaceRuntime.h"

#include "RpgAnimInstance.h"

namespace
{
enum class ETurnInPlaceHardResetReason : uint8
{
	ProxySnapshot = 1 << 0,
	UnsupportedRotationMode = 1 << 1,
	BlockingGameplayTag = 1 << 2,
	Montage = 1 << 3,
	Crouch = 1 << 4,
	UnsupportedMovementState = 1 << 5,
};

constexpr uint8 ToReasonMask(ETurnInPlaceHardResetReason Reason)
{
	return static_cast<uint8>(Reason);
}

void AddHardResetReason(
	uint8& Reasons,
	bool bCondition,
	ETurnInPlaceHardResetReason Reason)
{
	if (bCondition)
	{
		Reasons |= ToReasonMask(Reason);
	}
}

float NormalizeSignedYaw(float Yaw)
{
	// Bound the unwind first: finite malformed input must not spend unbounded time subtracting turns.
	return FMath::UnwindDegrees(FMath::Fmod(Yaw, 360.0f));
}
}

bool RpgTurnInPlaceRuntime::SupportsTurnInPlace(
	ERpgCharacterRotationMode RotationMode)
{
	return RotationMode == ERpgCharacterRotationMode::CombatStrafe ||
		RotationMode == ERpgCharacterRotationMode::Aim;
}

float RpgTurnInPlaceRuntime::QuantizeAngle(
	float SignedAngle,
	const FRpgGaspLocomotionTuning& Tuning)
{
	const float AbsoluteAngle = FMath::Min(FMath::Abs(SignedAngle), 180.0f);
	if (AbsoluteAngle < Tuning.TurnActivationThreshold)
	{
		return 0.0f;
	}

	float QuantizedMagnitude = 180.0f;
	if (AbsoluteAngle < 67.5f)
	{
		QuantizedMagnitude = 45.0f;
	}
	else if (AbsoluteAngle < 112.5f)
	{
		QuantizedMagnitude = 90.0f;
	}
	else if (AbsoluteAngle < 157.5f)
	{
		QuantizedMagnitude = 135.0f;
	}

	return SignedAngle < 0.0f ? -QuantizedMagnitude : QuantizedMagnitude;
}

float RpgTurnInPlaceRuntime::GetFacingDuration(
	float QuantizedAngle,
	const FRpgGaspLocomotionTuning& Tuning)
{
	const float AbsoluteAngle = FMath::Abs(QuantizedAngle);
	if (AbsoluteAngle <= 45.0f)
	{
		return Tuning.TurnFacingDuration45;
	}
	if (AbsoluteAngle <= 90.0f)
	{
		return Tuning.TurnFacingDuration90;
	}
	if (AbsoluteAngle <= 135.0f)
	{
		return Tuning.TurnFacingDuration135;
	}
	return Tuning.TurnFacingDuration180;
}

float RpgTurnInPlaceRuntime::CalculateYawDelta(
	float PreviousActorYaw,
	float CurrentActorYaw)
{
	return FMath::FindDeltaAngleDegrees(PreviousActorYaw, CurrentActorYaw);
}

bool RpgTurnInPlaceRuntime::CalculateRootYawGap(
	const FQuat& EvaluatedMeshRootRotation,
	const FQuat& MeshBasisRotation,
	float ActorYaw,
	float& OutRootYawGap)
{
	OutRootYawGap = 0.0f;
	if (EvaluatedMeshRootRotation.ContainsNaN() || MeshBasisRotation.ContainsNaN() ||
		!FMath::IsFinite(EvaluatedMeshRootRotation.SizeSquared()) ||
		!FMath::IsFinite(MeshBasisRotation.SizeSquared()) ||
		EvaluatedMeshRootRotation.SizeSquared() <= UE_SMALL_NUMBER ||
		MeshBasisRotation.SizeSquared() <= UE_SMALL_NUMBER || !FMath::IsFinite(ActorYaw))
	{
		return false;
	}

	const FQuat ActorSpaceRoot = (EvaluatedMeshRootRotation.GetNormalized() *
		MeshBasisRotation.GetNormalized().Inverse()).GetNormalized();
	OutRootYawGap = CalculateYawDelta(
		static_cast<float>(ActorSpaceRoot.Rotator().Yaw), NormalizeSignedYaw(ActorYaw));
	return FMath::IsFinite(OutRootYawGap);
}

bool RpgTurnInPlaceRuntime::DidSupportChange(
	bool bHasPreviousSnapshot,
	ERpgCharacterRotationMode PreviousMode,
	ERpgCharacterRotationMode CurrentMode)
{
	return bHasPreviousSnapshot &&
		SupportsTurnInPlace(PreviousMode) != SupportsTurnInPlace(CurrentMode);
}

float RpgTurnInPlaceRuntime::CalculateSnapshotYawDelta(
	float PreviousActorYaw,
	float CurrentActorYaw,
	bool bHardReset,
	bool bSupportChanged)
{
	return bHardReset || bSupportChanged
		? 0.0f
		: CalculateYawDelta(PreviousActorYaw, CurrentActorYaw);
}

FTransformTrajectory RpgTurnInPlaceRuntime::MakeSyntheticTrajectory(
	const FTransformTrajectory& SourceTrajectory,
	float CurrentActorYaw,
	float AccumulatedYaw,
	float QuantizedAngle,
	const FQuat& MeshBasisRotation,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FTransformTrajectory Result;
	const float FacingDuration = GetFacingDuration(QuantizedAngle, Tuning);
	const float StartYaw = CurrentActorYaw - AccumulatedYaw;

	Result.Samples.Reserve(SourceTrajectory.Samples.Num());
	for (const FTransformTrajectorySample& SourceSample : SourceTrajectory.Samples)
	{
		FTransformTrajectorySample& Sample = Result.Samples.Add_GetRef(SourceSample);
		const float FacingAlpha = Sample.TimeInSeconds <= 0.0f
			? 0.0f
			: FMath::Clamp(Sample.TimeInSeconds / FacingDuration, 0.0f, 1.0f);
		Sample.Facing = (FRotator(
			0.0f,
			StartYaw + QuantizedAngle * FacingAlpha,
			0.0f).Quaternion() * MeshBasisRotation).GetNormalized();
	}
	return Result;
}

FTransformTrajectory RpgTurnInPlaceRuntime::MakeSyntheticTrajectory(
	const FTransformTrajectory& SourceTrajectory,
	float CurrentActorYaw,
	float AccumulatedYaw,
	float QuantizedAngle,
	const FRpgGaspLocomotionTuning& Tuning)
{
	return MakeSyntheticTrajectory(
		SourceTrajectory, CurrentActorYaw, AccumulatedYaw, QuantizedAngle, FQuat::Identity, Tuning);
}

FTransformTrajectory RpgTurnInPlaceRuntime::MakeStationaryFacingTrajectory(
	const FTransformTrajectory& SourceTrajectory,
	float ActorYaw,
	const FQuat& MeshBasisRotation,
	const FQuat& LastEvaluatedComponentRotation)
{
	FTransformTrajectory Result = SourceTrajectory;
	const FQuat EvaluatedMeshFacing = LastEvaluatedComponentRotation.GetNormalized();
	const FQuat DesiredMeshFacing =
		(FRotator(0.0f, ActorYaw, 0.0f).Quaternion() * MeshBasisRotation).GetNormalized();
	for (FTransformTrajectorySample& Sample : Result.Samples)
	{
		// Pose History time zero is the previous evaluated root in its previous component basis.
		// Replacing that basis with today's target would invent a root rotation on counter-input.
		Sample.Facing = Sample.TimeInSeconds <= 0.0f ? EvaluatedMeshFacing : DesiredMeshFacing;
	}
	return Result;
}

float RpgTurnInPlaceRuntime::CalculatePlaybackWatchdogDuration(
	float RemainingAnimationTime,
	float PlayRate,
	bool bLooping,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (bLooping || !FMath::IsFinite(PlayRate) || FMath::Abs(PlayRate) <= UE_SMALL_NUMBER)
	{
		return Tuning.TurnActiveTimeout;
	}

	return FMath::Max(
		Tuning.TurnActiveTimeout,
		FMath::Max(RemainingAnimationTime, 0.0f) / FMath::Abs(PlayRate) +
			PlaybackWatchdogSafetyMargin);
}

bool RpgTurnInPlaceRuntime::IsEligible(
	const FRpgTurnInPlaceEligibilitySnapshot& Snapshot,
	const FRpgGaspLocomotionTuning& Tuning)
{
	return Snapshot.bHasTurnDatabase &&
		Snapshot.bJumpPhaseGrounded &&
		SupportsTurnInPlace(Snapshot.RotationMode) &&
		Snapshot.MovementState == ERpgLocomotionMovementState::Grounded &&
		Snapshot.bIsMovingOnGround &&
		!Snapshot.bIsCrouching &&
		!Snapshot.bIsAnyMontagePlaying &&
		!Snapshot.bHasBlockingGameplayTag &&
		Snapshot.GroundSpeed <= Tuning.StationarySpeedThreshold &&
		!Snapshot.bHasGroundedMoveIntent &&
		Snapshot.bHasTrajectory;
}

FRpgTurnInPlaceUpdateResult RpgTurnInPlaceRuntime::Reset(
	const FRpgTurnInPlaceRuntimeState& State,
	bool bHardResetOffset,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	Result.State.State = ERpgTurnInPlaceState::Inactive;
	Result.State.QueryAngle = 0.0f;
	Result.State.AccumulatedYaw = 0.0f;
	Result.State.StateElapsed = 0.0f;
	Result.State.StableElapsed = 0.0f;
	Result.State.SelectionElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = Tuning.TurnActiveTimeout;
	Result.State.RequestAccumulatedYaw = 0.0f;
	Result.State.MeasuredRootYawDirection = 0.0f;
	Result.State.TargetYawAnchor = 0.0f;
	Result.State.bHasTargetYawAnchor = false;
	Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
	Result.bResetOffsetRootEveryFrame = bHardResetOffset;
	Result.bClearSelection = true;
	return Result;
}

FRpgTurnInPlaceUpdateResult RpgTurnInPlaceRuntime::BeginRecovery(
	const FRpgTurnInPlaceRuntimeState& State,
	bool bHardResetOffset,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	Result.State.State = ERpgTurnInPlaceState::Recovering;
	Result.State.MeasuredRootYawDirection = 0.0f;
	Result.State.bHasTargetYawAnchor = false;
	Result.State.StateElapsed = 0.0f;
	Result.State.StableElapsed = 0.0f;
	Result.State.SelectionElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = Tuning.TurnActiveTimeout;
	Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
	Result.bResetOffsetRootEveryFrame = bHardResetOffset;
	Result.bClearSelection = true;
	return Result;
}

FRpgTurnInPlaceUpdateResult RpgTurnInPlaceRuntime::BeginRequest(
	const FRpgTurnInPlaceRuntimeState& State,
	float QuantizedAngle,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	Result.State.State = ERpgTurnInPlaceState::Active;
	Result.State.QueryAngle = QuantizedAngle;
	Result.State.StateElapsed = 0.0f;
	Result.State.StableElapsed = 0.0f;
	Result.State.SelectionElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = Tuning.TurnActiveTimeout;
	Result.State.RequestAccumulatedYaw = Result.State.AccumulatedYaw;
	Result.State.MeasuredRootYawDirection = 0.0f;
	Result.State.bHasTargetYawAnchor = false;
	++Result.State.RequestSerial;
	if (Result.State.RequestSerial == 0)
	{
		++Result.State.RequestSerial;
	}
	Result.OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
	Result.bClearSelection = true;
	Result.bUseSyntheticTrajectory = true;
	return Result;
}

FRpgTurnInPlaceUpdateResult RpgTurnInPlaceRuntime::Update(
	const FRpgTurnInPlaceRuntimeState& State,
	const FRpgTurnInPlaceUpdateSnapshot& Snapshot,
	float DeltaSeconds,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	const bool bUseRootYawGap = Snapshot.bHasRootYawGap &&
		FMath::IsFinite(Snapshot.RootYawGap) && FMath::IsFinite(Snapshot.ActorYaw);
	auto WithTargetAnchor = [&](FRpgTurnInPlaceUpdateResult Transition)
	{
		if (bUseRootYawGap)
		{
			Transition.State.TargetYawAnchor = NormalizeSignedYaw(Snapshot.ActorYaw);
			Transition.State.bHasTargetYawAnchor = true;
		}
		return Transition;
	};
	auto Recover = [&](const FRpgTurnInPlaceRuntimeState& Current, bool bHardResetOffset)
	{
		return WithTargetAnchor(BeginRecovery(Current, bHardResetOffset, Tuning));
	};
	auto Request = [&](const FRpgTurnInPlaceRuntimeState& Current, float Angle)
	{
		return WithTargetAnchor(BeginRequest(Current, Angle, Tuning));
	};
	const float AbsoluteActorYawRate = SafeDeltaSeconds > UE_SMALL_NUMBER
		? FMath::Abs(Snapshot.ActorYawDelta) / SafeDeltaSeconds
		: (FMath::IsNearlyZero(Snapshot.ActorYawDelta) ? 0.0f : MAX_flt);

	const bool bAirborneCancelsTurnInPlace =
		Snapshot.MovementState == ERpgLocomotionMovementState::Airborne;
	uint8 HardResetReasons = 0;
	AddHardResetReason(
		HardResetReasons,
		Snapshot.bProxyHardReset,
		ETurnInPlaceHardResetReason::ProxySnapshot);
	AddHardResetReason(
		HardResetReasons,
		!SupportsTurnInPlace(Snapshot.RotationMode),
		ETurnInPlaceHardResetReason::UnsupportedRotationMode);
	AddHardResetReason(
		HardResetReasons,
		Snapshot.bHasBlockingGameplayTag,
		ETurnInPlaceHardResetReason::BlockingGameplayTag);
	AddHardResetReason(
		HardResetReasons,
		Snapshot.bIsAnyMontagePlaying,
		ETurnInPlaceHardResetReason::Montage);
	AddHardResetReason(
		HardResetReasons,
		Snapshot.bIsCrouching,
		ETurnInPlaceHardResetReason::Crouch);
	AddHardResetReason(
		HardResetReasons,
		!bAirborneCancelsTurnInPlace &&
			Snapshot.MovementState != ERpgLocomotionMovementState::Grounded,
		ETurnInPlaceHardResetReason::UnsupportedMovementState);
	if (HardResetReasons != 0 || bAirborneCancelsTurnInPlace)
	{
		const bool bHasTurnStateToClear =
			Result.State.State != ERpgTurnInPlaceState::Inactive ||
			FMath::Abs(Result.State.AccumulatedYaw) > UE_KINDA_SMALL_NUMBER;
		const bool bHasNewHardResetReason =
			(HardResetReasons & ~Result.State.HardResetReasonsLastFrame) != 0;
		// Airborne movement releases TIR without disturbing ordinary locomotion interpolation.
		// Real retained turn state and each newly added teleport, policy, tag, montage, or stance
		// reason still receive one reset edge, even while another reason remains active.
		const bool bPulseHardReset =
			bHasTurnStateToClear ||
			bHasNewHardResetReason ||
			Snapshot.bSupportChanged;
		Result = Reset(Result.State, bPulseHardReset, Tuning);
		Result.State.HardResetReasonsLastFrame = HardResetReasons;
		return Result;
	}
	Result.State.HardResetReasonsLastFrame = 0;

	if (Snapshot.bSupportChanged)
	{
		return Recover(Result.State, true);
	}

	if (!Snapshot.bEligible)
	{
		if (Result.State.State == ERpgTurnInPlaceState::Collecting ||
			Result.State.State == ERpgTurnInPlaceState::Active)
		{
			return Recover(Result.State, false);
		}
		if (Result.State.State == ERpgTurnInPlaceState::Recovering)
		{
			Result.State.StateElapsed += SafeDeltaSeconds;
			if (Result.State.StateElapsed >= Tuning.TurnRecoveryDuration)
			{
				return WithTargetAnchor(Reset(Result.State, false, Tuning));
			}
			Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
			return Result;
		}
		return Reset(Result.State, false, Tuning);
	}

	if (bUseRootYawGap)
	{
		Result.State.AccumulatedYaw = NormalizeSignedYaw(Snapshot.RootYawGap);
		if (Result.State.State == ERpgTurnInPlaceState::Active && !Result.State.bHasTargetYawAnchor)
		{
			// Feedback can first become relevant after a legacy request has already selected its clip.
			Result.State.TargetYawAnchor = NormalizeSignedYaw(Snapshot.ActorYaw);
			Result.State.bHasTargetYawAnchor = true;
			Result.State.RequestAccumulatedYaw = Result.State.AccumulatedYaw;
			Result.State.MeasuredRootYawDirection = 0.0f;
		}
	}
	else if (Result.State.State != ERpgTurnInPlaceState::Recovering)
	{
		Result.State.AccumulatedYaw = FMath::Clamp(
			Result.State.AccumulatedYaw + Snapshot.ActorYawDelta,
			-180.0f,
			180.0f);
	}
	const float TargetYawChange = bUseRootYawGap && Result.State.bHasTargetYawAnchor
		? CalculateYawDelta(Result.State.TargetYawAnchor, NormalizeSignedYaw(Snapshot.ActorYaw))
		: 0.0f;
	const bool bFreshRootTarget = !Result.State.bHasTargetYawAnchor ||
		FMath::Abs(TargetYawChange) >= Tuning.TurnCollectThreshold;
	const float RootProgress = bUseRootYawGap && Result.State.bHasTargetYawAnchor
		? CalculateYawDelta(
			NormalizeSignedYaw(Result.State.TargetYawAnchor - Result.State.RequestAccumulatedYaw),
			NormalizeSignedYaw(Snapshot.ActorYaw - Result.State.AccumulatedYaw))
		: 0.0f;
	const bool bAmbiguousHalfTurn = FMath::IsNearlyEqual(FMath::Abs(Result.State.QueryAngle), 180.0f, 1.e-3f);
	if (bUseRootYawGap && Result.State.State == ERpgTurnInPlaceState::Active && bAmbiguousHalfTurn &&
		FMath::IsNearlyZero(Result.State.MeasuredRootYawDirection) &&
		FMath::Abs(RootProgress) >= FMath::Max(Tuning.TurnCancelThreshold, UE_KINDA_SMALL_NUMBER) &&
		FMath::Abs(RootProgress) < 180.0f - UE_KINDA_SMALL_NUMBER)
	{
		// +/-180 describe the same target; Motion Matching may select the same clip for either.
		// Freeze its observed direction before endpoint yaw wraps at 180. Recomputing this sign
		// after overshoot would mistake the wrapped progress for an authored reverse turn.
		Result.State.MeasuredRootYawDirection = FMath::Sign(RootProgress);
	}
	const float ActiveTurnDirection = bAmbiguousHalfTurn && bUseRootYawGap
		? Result.State.MeasuredRootYawDirection : FMath::Sign(Result.State.QueryAngle);
	if (bUseRootYawGap && Result.State.State == ERpgTurnInPlaceState::Recovering &&
		bFreshRootTarget && FMath::Abs(Result.State.AccumulatedYaw) >= Tuning.TurnCollectThreshold)
	{
		// New intent may interrupt recovery; an unchanged residual/overshoot must interpolate away.
		Result.State.State = ERpgTurnInPlaceState::Inactive;
		Result.State.StateElapsed = 0.0f;
		Result.State.StableElapsed = 0.0f;
		Result.State.bHasTargetYawAnchor = false;
	}
	Result.State.StateElapsed += SafeDeltaSeconds;

	float CollectionDeltaSeconds = SafeDeltaSeconds;
	switch (Result.State.State)
	{
	case ERpgTurnInPlaceState::Inactive:
		Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
		if (bUseRootYawGap && !bFreshRootTarget)
		{
			break;
		}
		if (AbsoluteActorYawRate <= Tuning.TurnInactiveYawRateThreshold)
		{
			Result.State.StableElapsed += SafeDeltaSeconds;
			if (Result.State.StableElapsed >= Tuning.TurnInactiveAccumulatorTimeout)
			{
				if (!bUseRootYawGap)
				{
					Result.State.AccumulatedYaw = 0.0f;
				}
			}
		}
		else
		{
			Result.State.StableElapsed = 0.0f;
		}

		if (FMath::Abs(Result.State.AccumulatedYaw) >= Tuning.TurnCollectThreshold)
		{
			Result.State.State = ERpgTurnInPlaceState::Collecting;
			// Yaw describes the interval ending at this update. Preserve its portion after the
			// collection threshold crossing instead of losing up to one whole low-FPS frame.
			const float YawPastThreshold =
				FMath::Abs(State.AccumulatedYaw + Snapshot.ActorYawDelta) - Tuning.TurnCollectThreshold;
			const float FrameYaw = FMath::Abs(Snapshot.ActorYawDelta);
			CollectionDeltaSeconds = FrameYaw > UE_SMALL_NUMBER
				? SafeDeltaSeconds * FMath::Clamp(YawPastThreshold / FrameYaw, 0.0f, 1.0f)
				: 0.0f;
			Result.State.StateElapsed = CollectionDeltaSeconds;
			Result.State.StableElapsed = 0.0f;
			Result.OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
		}
		else
		{
			break;
		}
		[[fallthrough]];

	case ERpgTurnInPlaceState::Collecting:
		Result.OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
		if (FMath::Abs(Result.State.AccumulatedYaw) < Tuning.TurnCancelThreshold)
		{
			return Recover(Result.State, false);
		}

		Result.State.StableElapsed = AbsoluteActorYawRate <= Tuning.TurnStableYawRateThreshold
			? Result.State.StableElapsed + CollectionDeltaSeconds
			: 0.0f;
		if (FMath::Abs(Result.State.AccumulatedYaw) >= Tuning.TurnActivationThreshold &&
			(Result.State.StableElapsed >= Tuning.TurnStabilityDuration ||
			 Result.State.StateElapsed >= Tuning.TurnCollectionTimeout))
		{
			return Request(
				Result.State,
				QuantizeAngle(Result.State.AccumulatedYaw, Tuning));
		}
		if (Result.State.StateElapsed >= Tuning.TurnCollectionTimeout)
		{
			return Recover(Result.State, false);
		}
		break;

	case ERpgTurnInPlaceState::Active:
		Result.OffsetRootRotationMode = Snapshot.bSelectionLatched && !bUseRootYawGap
			? EOffsetRootBoneMode::LockOffsetIncreaseAndConsumeAnimation
			: EOffsetRootBoneMode::Accumulate;
		if (!bUseRootYawGap && FMath::Abs(Result.State.AccumulatedYaw) < Tuning.TurnCancelThreshold)
		{
			return Recover(Result.State, false);
		}
		if (bUseRootYawGap && Result.State.bHasTargetYawAnchor && !FMath::IsNearlyZero(ActiveTurnDirection) &&
			FMath::Abs(TargetYawChange) >= Tuning.TurnActivationThreshold &&
			(TargetYawChange * ActiveTurnDirection < 0.0f ||
			 FMath::IsNearlyEqual(FMath::Abs(TargetYawChange), 180.0f, 1.e-3f)) &&
			Result.State.AccumulatedYaw * ActiveTurnDirection <= 0.0f)
		{
			// Half-turn input has no unique sign; the actual root gap resolves its direction.
			// Otherwise retain the target-direction guard so later overshoot is not new counter-input.
			return FMath::Abs(Result.State.AccumulatedYaw) >= Tuning.TurnActivationThreshold
				? Request(Result.State, QuantizeAngle(Result.State.AccumulatedYaw, Tuning))
				: Recover(Result.State, false);
		}

		if (CanRetarget(Result.State, Snapshot.bSelectionLatched))
		{
			const float UpdatedQueryAngle = QuantizeAngle(Result.State.AccumulatedYaw, Tuning);
			const float AdditionalYaw = bUseRootYawGap ? TargetYawChange :
				Result.State.AccumulatedYaw - Result.State.RequestAccumulatedYaw;
			const bool bDirectionChanged =
				!FMath::IsNearlyZero(UpdatedQueryAngle) &&
				FMath::Sign(UpdatedQueryAngle) != FMath::Sign(Result.State.QueryAngle);
			const bool bQuantizedBucketChanged =
				!FMath::IsNearlyZero(UpdatedQueryAngle) &&
				!FMath::IsNearlyEqual(UpdatedQueryAngle, Result.State.QueryAngle);
			if (FMath::Abs(AdditionalYaw) >= Tuning.TurnActivationThreshold &&
				(bDirectionChanged || bQuantizedBucketChanged))
			{
				return Request(Result.State, UpdatedQueryAngle);
			}
		}

		if (!Snapshot.bSelectionLatched)
		{
			Result.State.SelectionElapsed += SafeDeltaSeconds;
			if (Result.State.SelectionElapsed >= Tuning.TurnSelectionTimeout)
			{
				return Recover(Result.State, true);
			}
		}
		else if (Snapshot.bCompletionArmed)
		{
			if (bUseRootYawGap &&
				FMath::Abs(Result.State.AccumulatedYaw) >= Tuning.TurnActivationThreshold &&
				Result.State.AccumulatedYaw * ActiveTurnDirection > 0.0f &&
				FMath::Abs(TargetYawChange) >= Tuning.TurnActivationThreshold)
			{
				// Only input added during this request can need another authored turn. The
				// unchanged goal's blended root remainder belongs to ordinary recovery.
				return Request(Result.State, QuantizeAngle(Result.State.AccumulatedYaw, Tuning));
			}
			return Recover(Result.State, false);
		}
		else if (Snapshot.bPlaybackObserved && !Snapshot.bPoseSelected)
		{
			return Recover(Result.State, true);
		}

		if (Result.State.StateElapsed >= Result.State.PlaybackWatchdogDuration)
		{
			return Recover(Result.State, true);
		}
		break;

	case ERpgTurnInPlaceState::Recovering:
		Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
		if (Result.State.StateElapsed >= Tuning.TurnRecoveryDuration)
		{
			return WithTargetAnchor(Reset(Result.State, false, Tuning));
		}
		break;
	}

	Result.bUseSyntheticTrajectory =
		Result.State.State == ERpgTurnInPlaceState::Active;
	return Result;
}

bool RpgTurnInPlaceRuntime::ConsumeForceInterrupt(
	bool bHasTurnDatabase,
	FRpgTurnInPlaceRuntimeState& State)
{
	if (State.State != ERpgTurnInPlaceState::Active ||
		!bHasTurnDatabase ||
		State.InterruptedRequestSerial == State.RequestSerial)
	{
		return false;
	}

	State.InterruptedRequestSerial = State.RequestSerial;
	return true;
}

bool RpgTurnInPlaceRuntime::CanRetarget(
	const FRpgTurnInPlaceRuntimeState& State,
	bool bSelectionLatched)
{
	return State.State == ERpgTurnInPlaceState::Active &&
		!bSelectionLatched &&
		State.InterruptedRequestSerial != State.RequestSerial;
}

ERpgTurnInPlaceSearchMode RpgTurnInPlaceRuntime::ResolveSearchMode(
	const FRpgTurnInPlaceRuntimeState& State,
	bool bForceNewRequest,
	bool bHasTurnDatabase,
	bool bSelectionLatched,
	bool bCompletionArmed)
{
	if (bForceNewRequest &&
		State.State == ERpgTurnInPlaceState::Active &&
		bHasTurnDatabase)
	{
		return ERpgTurnInPlaceSearchMode::SearchRequestedTurn;
	}

	if (State.State == ERpgTurnInPlaceState::Active &&
		bSelectionLatched &&
		!bCompletionArmed)
	{
		return ERpgTurnInPlaceSearchMode::ContinueSelectedTurn;
	}

	return ERpgTurnInPlaceSearchMode::NormalLocomotion;
}

bool RpgTurnInPlaceRuntime::AllowsMovingProceduralNodes(
	ERpgTurnInPlaceState State)
{
	return State == ERpgTurnInPlaceState::Inactive ||
		State == ERpgTurnInPlaceState::Recovering;
}
