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
}

bool RpgTurnInPlaceRuntime::SupportsTurnInPlace(
	ERpgCharacterRotationMode RotationMode)
{
	return RotationMode == ERpgCharacterRotationMode::CombatStrafe ||
		RotationMode == ERpgCharacterRotationMode::Aim;
}

float RpgTurnInPlaceRuntime::QuantizeAngle(float SignedAngle)
{
	const float AbsoluteAngle = FMath::Min(FMath::Abs(SignedAngle), 180.0f);
	if (AbsoluteAngle < ActivationThreshold)
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

float RpgTurnInPlaceRuntime::GetFacingDuration(float QuantizedAngle)
{
	const float AbsoluteAngle = FMath::Abs(QuantizedAngle);
	if (AbsoluteAngle <= 45.0f)
	{
		return 0.45f;
	}
	if (AbsoluteAngle <= 90.0f)
	{
		return 0.65f;
	}
	if (AbsoluteAngle <= 135.0f)
	{
		return 0.85f;
	}
	return 1.0f;
}

float RpgTurnInPlaceRuntime::CalculateYawDelta(
	float PreviousActorYaw,
	float CurrentActorYaw)
{
	return FMath::FindDeltaAngleDegrees(PreviousActorYaw, CurrentActorYaw);
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
	float QuantizedAngle)
{
	FTransformTrajectory Result;
	const float FacingDuration = GetFacingDuration(QuantizedAngle);
	const float StartYaw = CurrentActorYaw - AccumulatedYaw;

	Result.Samples.Reserve(SourceTrajectory.Samples.Num());
	for (const FTransformTrajectorySample& SourceSample : SourceTrajectory.Samples)
	{
		FTransformTrajectorySample& Sample = Result.Samples.Add_GetRef(SourceSample);
		const float FacingAlpha = Sample.TimeInSeconds <= 0.0f
			? 0.0f
			: FMath::Clamp(Sample.TimeInSeconds / FacingDuration, 0.0f, 1.0f);
		Sample.Facing = FRotator(
			0.0f,
			StartYaw + QuantizedAngle * FacingAlpha,
			0.0f).Quaternion();
	}
	return Result;
}

float RpgTurnInPlaceRuntime::CalculatePlaybackWatchdogDuration(
	float RemainingAnimationTime,
	float PlayRate,
	bool bLooping)
{
	if (bLooping || !FMath::IsFinite(PlayRate) || FMath::Abs(PlayRate) <= UE_SMALL_NUMBER)
	{
		return ActiveTimeout;
	}

	return FMath::Max(
		ActiveTimeout,
		FMath::Max(RemainingAnimationTime, 0.0f) / FMath::Abs(PlayRate) +
			PlaybackWatchdogSafetyMargin);
}

bool RpgTurnInPlaceRuntime::IsEligible(
	const FRpgTurnInPlaceEligibilitySnapshot& Snapshot)
{
	return Snapshot.bHasTurnDatabase &&
		Snapshot.bJumpPhaseGrounded &&
		SupportsTurnInPlace(Snapshot.RotationMode) &&
		Snapshot.MovementState == ERpgLocomotionMovementState::Grounded &&
		Snapshot.bIsMovingOnGround &&
		!Snapshot.bIsCrouching &&
		!Snapshot.bIsAnyMontagePlaying &&
		!Snapshot.bHasBlockingGameplayTag &&
		Snapshot.GroundSpeed <= IdleSpeedThreshold &&
		!Snapshot.bHasGroundedMoveIntent &&
		Snapshot.bHasTrajectory;
}

FRpgTurnInPlaceUpdateResult RpgTurnInPlaceRuntime::Reset(
	const FRpgTurnInPlaceRuntimeState& State,
	bool bHardResetOffset)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	Result.State.State = ERpgTurnInPlaceState::Inactive;
	Result.State.QueryAngle = 0.0f;
	Result.State.AccumulatedYaw = 0.0f;
	Result.State.StateElapsed = 0.0f;
	Result.State.StableElapsed = 0.0f;
	Result.State.SelectionElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = ActiveTimeout;
	Result.State.RequestAccumulatedYaw = 0.0f;
	Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
	Result.bResetOffsetRootEveryFrame = bHardResetOffset;
	Result.bClearSelection = true;
	return Result;
}

FRpgTurnInPlaceUpdateResult RpgTurnInPlaceRuntime::BeginRecovery(
	const FRpgTurnInPlaceRuntimeState& State,
	bool bHardResetOffset)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	Result.State.State = ERpgTurnInPlaceState::Recovering;
	Result.State.StateElapsed = 0.0f;
	Result.State.StableElapsed = 0.0f;
	Result.State.SelectionElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = ActiveTimeout;
	Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
	Result.bResetOffsetRootEveryFrame = bHardResetOffset;
	Result.bClearSelection = true;
	return Result;
}

FRpgTurnInPlaceUpdateResult RpgTurnInPlaceRuntime::BeginRequest(
	const FRpgTurnInPlaceRuntimeState& State,
	float QuantizedAngle)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	Result.State.State = ERpgTurnInPlaceState::Active;
	Result.State.QueryAngle = QuantizedAngle;
	Result.State.StateElapsed = 0.0f;
	Result.State.StableElapsed = 0.0f;
	Result.State.SelectionElapsed = 0.0f;
	Result.State.PlaybackWatchdogDuration = ActiveTimeout;
	Result.State.RequestAccumulatedYaw = Result.State.AccumulatedYaw;
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
	float DeltaSeconds)
{
	FRpgTurnInPlaceUpdateResult Result;
	Result.State = State;
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
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
		Result = Reset(Result.State, bPulseHardReset);
		Result.State.HardResetReasonsLastFrame = HardResetReasons;
		return Result;
	}
	Result.State.HardResetReasonsLastFrame = 0;

	if (Snapshot.bSupportChanged)
	{
		return BeginRecovery(Result.State, true);
	}

	if (!Snapshot.bEligible)
	{
		if (Result.State.State == ERpgTurnInPlaceState::Collecting ||
			Result.State.State == ERpgTurnInPlaceState::Active)
		{
			return BeginRecovery(Result.State, false);
		}
		if (Result.State.State == ERpgTurnInPlaceState::Recovering)
		{
			Result.State.StateElapsed += SafeDeltaSeconds;
			if (Result.State.StateElapsed >= RecoveryDuration)
			{
				return Reset(Result.State, false);
			}
			Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
			return Result;
		}
		return Reset(Result.State, false);
	}

	Result.State.StateElapsed += SafeDeltaSeconds;
	if (Result.State.State != ERpgTurnInPlaceState::Recovering)
	{
		Result.State.AccumulatedYaw = FMath::Clamp(
			Result.State.AccumulatedYaw + Snapshot.ActorYawDelta,
			-180.0f,
			180.0f);
	}

	switch (Result.State.State)
	{
	case ERpgTurnInPlaceState::Inactive:
		Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
		if (AbsoluteActorYawRate <= InactiveYawRateThreshold)
		{
			Result.State.StableElapsed += SafeDeltaSeconds;
			if (Result.State.StableElapsed >= InactiveAccumulatorTimeout)
			{
				Result.State.AccumulatedYaw = 0.0f;
			}
		}
		else
		{
			Result.State.StableElapsed = 0.0f;
		}

		if (FMath::Abs(Result.State.AccumulatedYaw) >= CollectThreshold)
		{
			Result.State.State = ERpgTurnInPlaceState::Collecting;
			Result.State.StateElapsed = 0.0f;
			Result.State.StableElapsed = 0.0f;
			Result.OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
		}
		break;

	case ERpgTurnInPlaceState::Collecting:
		Result.OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
		if (FMath::Abs(Result.State.AccumulatedYaw) < CancelThreshold)
		{
			return BeginRecovery(Result.State, false);
		}

		Result.State.StableElapsed = AbsoluteActorYawRate <= StableYawRateThreshold
			? Result.State.StableElapsed + SafeDeltaSeconds
			: 0.0f;
		if (FMath::Abs(Result.State.AccumulatedYaw) >= ActivationThreshold &&
			(Result.State.StableElapsed >= StabilityDuration ||
			 Result.State.StateElapsed >= CollectionTimeout))
		{
			return BeginRequest(
				Result.State,
				QuantizeAngle(Result.State.AccumulatedYaw));
		}
		if (Result.State.StateElapsed >= CollectionTimeout)
		{
			return BeginRecovery(Result.State, false);
		}
		break;

	case ERpgTurnInPlaceState::Active:
		Result.OffsetRootRotationMode = Snapshot.bSelectionLatched
			? EOffsetRootBoneMode::LockOffsetIncreaseAndConsumeAnimation
			: EOffsetRootBoneMode::Accumulate;
		if (FMath::Abs(Result.State.AccumulatedYaw) < CancelThreshold)
		{
			return BeginRecovery(Result.State, false);
		}

		if (CanRetarget(Result.State, Snapshot.bSelectionLatched))
		{
			const float UpdatedQueryAngle = QuantizeAngle(Result.State.AccumulatedYaw);
			const float AdditionalYaw =
				Result.State.AccumulatedYaw - Result.State.RequestAccumulatedYaw;
			const bool bDirectionChanged =
				!FMath::IsNearlyZero(UpdatedQueryAngle) &&
				FMath::Sign(UpdatedQueryAngle) != FMath::Sign(Result.State.QueryAngle);
			const bool bQuantizedBucketChanged =
				!FMath::IsNearlyZero(UpdatedQueryAngle) &&
				!FMath::IsNearlyEqual(UpdatedQueryAngle, Result.State.QueryAngle);
			if (FMath::Abs(AdditionalYaw) >= ActivationThreshold &&
				(bDirectionChanged || bQuantizedBucketChanged))
			{
				return BeginRequest(Result.State, UpdatedQueryAngle);
			}
		}

		if (!Snapshot.bSelectionLatched)
		{
			Result.State.SelectionElapsed += SafeDeltaSeconds;
			if (Result.State.SelectionElapsed >= SelectionTimeout)
			{
				return BeginRecovery(Result.State, true);
			}
		}
		else if (Snapshot.bCompletionArmed)
		{
			return BeginRecovery(Result.State, false);
		}
		else if (Snapshot.bPlaybackObserved && !Snapshot.bPoseSelected)
		{
			return BeginRecovery(Result.State, true);
		}

		if (Result.State.StateElapsed >= Result.State.PlaybackWatchdogDuration)
		{
			return BeginRecovery(Result.State, true);
		}
		break;

	case ERpgTurnInPlaceState::Recovering:
		Result.OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
		if (Result.State.StateElapsed >= RecoveryDuration)
		{
			return Reset(Result.State, false);
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
