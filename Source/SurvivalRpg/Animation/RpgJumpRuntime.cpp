// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgJumpRuntime.h"

#include "RpgAnimInstance.h"

FRpgJumpPhysicalTransitionResult RpgJumpRuntime::ResolvePhysicalTransition(
	ERpgJumpPhase CurrentPhase,
	const FRpgJumpPhysicalSnapshot& Snapshot)
{
	FRpgJumpPhysicalTransitionResult Result;
	const bool bAirborne =
		Snapshot.bIsFalling ||
		Snapshot.MovementState == ERpgLocomotionMovementState::Airborne;
	Result.bAscendingTakeoff = Snapshot.VerticalVelocity > UE_KINDA_SMALL_NUMBER;

	if (Snapshot.bHardReset)
	{
		Result.Transition = bAirborne
			? ERpgJumpPhysicalTransition::EnterAirborne
			: ERpgJumpPhysicalTransition::ResetGrounded;
		return Result;
	}

	if (bAirborne)
	{
		if (CurrentPhase != ERpgJumpPhase::Airborne)
		{
			Result.Transition = ERpgJumpPhysicalTransition::EnterAirborne;
		}
		return Result;
	}

	if (CurrentPhase == ERpgJumpPhase::Airborne)
	{
		// Landing selection consumes this physical edge before deciding whether presentation
		// enters Landing or returns directly to Grounded.
		Result.Transition = ERpgJumpPhysicalTransition::Touchdown;
	}
	else if (CurrentPhase != ERpgJumpPhase::Landing)
	{
		Result.Transition = ERpgJumpPhysicalTransition::ResetGrounded;
	}
	return Result;
}

bool RpgJumpRuntime::ShouldHoldLoopingAirborneFallPlayback(
	ERpgJumpPhase CurrentPhase,
	bool bBackwardHoldWasArmed,
	float CurrentVerticalVelocity,
	bool bCurrentAssetIsLoopingFall)
{
	return CurrentPhase == ERpgJumpPhase::Airborne &&
		bBackwardHoldWasArmed &&
		CurrentVerticalVelocity <= UE_KINDA_SMALL_NUMBER &&
		bCurrentAssetIsLoopingFall;
}

bool RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
	ERpgJumpPhase CurrentPhase,
	bool bCurrentAssetMatchesHeldSelection,
	float CurrentAssetTime,
	float CurrentAssetLength,
	float CurrentAssetPlayRate,
	float HoldElapsed,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (CurrentPhase != ERpgJumpPhase::Airborne ||
		!bCurrentAssetMatchesHeldSelection ||
		!FMath::IsFinite(CurrentAssetTime) ||
		!FMath::IsFinite(CurrentAssetLength) ||
		!FMath::IsFinite(CurrentAssetPlayRate) ||
		!FMath::IsFinite(HoldElapsed) ||
		CurrentAssetTime < 0.0f ||
		CurrentAssetLength <= 0.0f ||
		CurrentAssetTime >= CurrentAssetLength ||
		HoldElapsed < 0.0f ||
		HoldElapsed >= Tuning.BackwardJumpStartHoldTimeout)
	{
		return false;
	}

	const float AbsolutePlayRate = FMath::Abs(CurrentAssetPlayRate);
	if (AbsolutePlayRate <= UE_KINDA_SMALL_NUMBER)
	{
		// A paused cosmetic player remains bounded by HoldElapsed instead of forcing a new search.
		return true;
	}

	const float RemainingPlaybackTime =
		(CurrentAssetLength - CurrentAssetTime) / AbsolutePlayRate;
	return RemainingPlaybackTime > Tuning.BackwardJumpStartReleaseLeadTime;
}

FRpgBackwardJumpStartHoldResult RpgJumpRuntime::ResetBackwardJumpStartHold()
{
	FRpgBackwardJumpStartHoldResult Result;
	Result.bClearHeldAsset = true;
	return Result;
}

FRpgBackwardJumpStartHoldResult RpgJumpRuntime::UpdateBackwardJumpStartHold(
	const FRpgBackwardJumpStartHoldState& State,
	const FRpgBackwardJumpStartPlaybackSnapshot& Snapshot,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (Snapshot.JumpPhase != ERpgJumpPhase::Airborne)
	{
		return ResetBackwardJumpStartHold();
	}

	FRpgBackwardJumpStartHoldResult Result;
	Result.State = State;
	bool bHasHeldAsset = Snapshot.bHasHeldAsset;
	bool bCurrentAssetMatchesHeld = Snapshot.bCurrentAssetMatchesHeld;
	if (!Result.State.bOpportunityConsumed)
	{
		if (!Snapshot.bCurrentAssetIsAirborne)
		{
			// The outgoing grounded sample may remain current during the initial Ground-to-Air blend.
			return Result;
		}

		Result.State.bOpportunityConsumed = true;
		if (Result.State.bHoldEligible && Snapshot.bCurrentAssetIsBackwardStart)
		{
			Result.State.HoldElapsed = 0.0f;
			Result.State.bHoldWasArmed = true;
			Result.bCaptureCurrentAsset = true;
			bHasHeldAsset = true;
			bCurrentAssetMatchesHeld = true;
		}
	}

	if (!bHasHeldAsset)
	{
		return Result;
	}

	Result.State.HoldElapsed += FMath::Max(Snapshot.DeltaSeconds, 0.0f);
	Result.bHoldContinuingPose = ShouldHoldBackwardJumpStartPlayback(
		Snapshot.JumpPhase,
		bCurrentAssetMatchesHeld,
		Snapshot.CurrentAssetTime,
		Snapshot.CurrentAssetLength,
		Snapshot.CurrentAssetPlayRate,
		Result.State.HoldElapsed,
		Tuning);
	if (!Result.bHoldContinuingPose)
	{
		// Keep the opportunity consumed so a later search cannot re-arm the same start in this jump.
		Result.State.HoldElapsed = 0.0f;
		Result.bClearHeldAsset = true;
	}
	return Result;
}
