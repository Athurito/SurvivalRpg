// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgGaspLocomotionConfig.h"

#include "RpgPoseSearchTrajectory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGaspLocomotionConfig)

FName RpgGaspLocomotionConfig::GetDatabaseRoleTag(
	ERpgMotionMatchingDatabaseRole Role)
{
	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandIdle:
		return TEXT("Rpg.MotionMatching.Role.StandIdle");
	case ERpgMotionMatchingDatabaseRole::StandWalk:
		return TEXT("Rpg.MotionMatching.Role.StandWalk");
	case ERpgMotionMatchingDatabaseRole::StandWalkStops:
		return TEXT("Rpg.MotionMatching.Role.StandWalkStops");
	case ERpgMotionMatchingDatabaseRole::StandRunLoops:
		return TEXT("Rpg.MotionMatching.Role.StandRunLoops");
	case ERpgMotionMatchingDatabaseRole::StandRunPivots:
		return TEXT("Rpg.MotionMatching.Role.StandRunPivots");
	case ERpgMotionMatchingDatabaseRole::StandRunStarts:
		return TEXT("Rpg.MotionMatching.Role.StandRunStarts");
	case ERpgMotionMatchingDatabaseRole::StandRunStops:
		return TEXT("Rpg.MotionMatching.Role.StandRunStops");
	case ERpgMotionMatchingDatabaseRole::StandSprint:
		return TEXT("Rpg.MotionMatching.Role.StandSprint");
	case ERpgMotionMatchingDatabaseRole::StandSprintStops:
		return TEXT("Rpg.MotionMatching.Role.StandSprintStops");
	case ERpgMotionMatchingDatabaseRole::Crouch:
		return TEXT("Rpg.MotionMatching.Role.Crouch");
	case ERpgMotionMatchingDatabaseRole::StandTurnInPlace:
		return TEXT("Rpg.MotionMatching.Role.StandTurnInPlace");
	case ERpgMotionMatchingDatabaseRole::Jump:
		return TEXT("Rpg.MotionMatching.Role.Jump");
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
		return TEXT("Rpg.MotionMatching.Role.StandLightLanding");
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
		return TEXT("Rpg.MotionMatching.Role.StandHeavyLanding");
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
		return TEXT("Rpg.MotionMatching.Role.WalkLightLanding");
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
		return TEXT("Rpg.MotionMatching.Role.WalkHeavyLanding");
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
		return TEXT("Rpg.MotionMatching.Role.RunLightLanding");
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return TEXT("Rpg.MotionMatching.Role.RunHeavyLanding");
	default:
		return NAME_None;
	}
}

ERpgMotionMatchingDatabaseRole RpgGaspLocomotionConfig::ResolveDatabaseRoleTag(
	TConstArrayView<FName> Tags)
{
	ERpgMotionMatchingDatabaseRole ResolvedRole = ERpgMotionMatchingDatabaseRole::None;
	int32 KnownRoleTagCount = 0;
	int32 ProjectRoleTagCount = 0;

	for (const FName Tag : Tags)
	{
		const bool bProjectRoleTag = Tag.ToString().StartsWith(
			TEXT("Rpg.MotionMatching.Role."),
			ESearchCase::CaseSensitive);
		ProjectRoleTagCount += bProjectRoleTag;
		if (!bProjectRoleTag)
		{
			continue;
		}

		for (uint8 RoleValue = static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None) + 1;
			RoleValue < static_cast<uint8>(ERpgMotionMatchingDatabaseRole::Count);
			++RoleValue)
		{
			const ERpgMotionMatchingDatabaseRole Role =
				static_cast<ERpgMotionMatchingDatabaseRole>(RoleValue);
			if (Tag == GetDatabaseRoleTag(Role))
			{
				ResolvedRole = Role;
				++KnownRoleTagCount;
				break;
			}
		}
	}

	return KnownRoleTagCount == 1 && ProjectRoleTagCount == 1
		? ResolvedRole
		: ERpgMotionMatchingDatabaseRole::None;
}

bool RpgGaspLocomotionConfig::IsTuningRuntimeValid(
	const FRpgGaspLocomotionTuning& Tuning)
{
	const float Values[] = {
		Tuning.LastMeaningfulVelocityThreshold,
		Tuning.StationarySpeedThreshold,
		Tuning.ChooserVelocityTolerance,
		Tuning.ChooserAccelerationTolerance,
		Tuning.WalkStopMinimumSpeed,
		Tuning.RunStopMinimumSpeed,
		Tuning.SprintStopMinimumSpeed,
		Tuning.FreeRunPivotMinimumAngle,
		Tuning.CombatStrafeRunPivotMinimumAngle,
		Tuning.AimRunPivotMinimumAngle,
		Tuning.RunStartMinimumFutureSpeedGain,
		Tuning.RunStartFutureVelocityBeginTime,
		Tuning.RunStartFutureVelocityEndTime,
		Tuning.TurnCollectThreshold,
		Tuning.TurnActivationThreshold,
		Tuning.TurnCancelThreshold,
		Tuning.TurnInactiveYawRateThreshold,
		Tuning.TurnStableYawRateThreshold,
		Tuning.TurnStabilityDuration,
		Tuning.TurnCollectionTimeout,
		Tuning.TurnRecoveryDuration,
		Tuning.TurnSelectionTimeout,
		Tuning.TurnActiveTimeout,
		Tuning.TurnInactiveAccumulatorTimeout,
		Tuning.TurnFacingDuration45,
		Tuning.TurnFacingDuration90,
		Tuning.TurnFacingDuration135,
		Tuning.TurnFacingDuration180,
		Tuning.BackwardJumpStartHoldTimeout,
		Tuning.BackwardJumpStartReleaseLeadTime,
		Tuning.HeavyLandingSpeedThreshold,
		Tuning.LandingSelectionTimeout,
		Tuning.LandingActiveTimeout,
		Tuning.LandingMovementHandoffWindow,
	};
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			return false;
		}
	}

	return Tuning.LastMeaningfulVelocityThreshold > 0.0f &&
		Tuning.StationarySpeedThreshold > 0.0f &&
		Tuning.ChooserVelocityTolerance >= 0.0f &&
		Tuning.ChooserAccelerationTolerance >= 0.0f &&
		Tuning.WalkStopMinimumSpeed > 0.0f &&
		Tuning.WalkStopMinimumSpeed <= Tuning.RunStopMinimumSpeed &&
		Tuning.RunStopMinimumSpeed <= Tuning.SprintStopMinimumSpeed &&
		Tuning.AimRunPivotMinimumAngle >= 0.0f &&
		Tuning.AimRunPivotMinimumAngle <= 180.0f &&
		Tuning.CombatStrafeRunPivotMinimumAngle >= 0.0f &&
		Tuning.CombatStrafeRunPivotMinimumAngle <= 180.0f &&
		Tuning.FreeRunPivotMinimumAngle >= 0.0f &&
		Tuning.FreeRunPivotMinimumAngle <= 180.0f &&
		Tuning.RunStartMinimumFutureSpeedGain >= 0.0f &&
		Tuning.RunStartFutureVelocityBeginTime >= 0.0f &&
		Tuning.RunStartFutureVelocityBeginTime < Tuning.RunStartFutureVelocityEndTime &&
		Tuning.RunStartFutureVelocityEndTime <=
			RpgPoseSearchTrajectory::PredictionSamplingInterval *
			RpgPoseSearchTrajectory::PredictionSampleCount &&
		Tuning.TurnCancelThreshold >= 0.0f &&
		Tuning.TurnCollectThreshold > 0.0f &&
		Tuning.TurnCancelThreshold <= Tuning.TurnCollectThreshold &&
		Tuning.TurnCollectThreshold <= Tuning.TurnActivationThreshold &&
		Tuning.TurnActivationThreshold <= 180.0f &&
		Tuning.TurnInactiveYawRateThreshold >= 0.0f &&
		Tuning.TurnInactiveYawRateThreshold <= Tuning.TurnStableYawRateThreshold &&
		Tuning.TurnStabilityDuration > 0.0f &&
		Tuning.TurnCollectionTimeout > 0.0f &&
		Tuning.TurnRecoveryDuration > 0.0f &&
		Tuning.TurnSelectionTimeout > 0.0f &&
		Tuning.TurnActiveTimeout > 0.0f &&
		Tuning.TurnInactiveAccumulatorTimeout > 0.0f &&
		Tuning.TurnFacingDuration45 > 0.0f &&
		Tuning.TurnFacingDuration45 <= Tuning.TurnFacingDuration90 &&
		Tuning.TurnFacingDuration90 <= Tuning.TurnFacingDuration135 &&
		Tuning.TurnFacingDuration135 <= Tuning.TurnFacingDuration180 &&
		Tuning.BackwardJumpStartHoldTimeout > 0.0f &&
		Tuning.BackwardJumpStartReleaseLeadTime >= 0.0f &&
		Tuning.BackwardJumpStartReleaseLeadTime < Tuning.BackwardJumpStartHoldTimeout &&
		Tuning.HeavyLandingSpeedThreshold > 0.0f &&
		Tuning.LandingSelectionTimeout > 0.0f &&
		// Landing watchdog math clamps to the native 0.1-second safety margin.
		Tuning.LandingActiveTimeout >= 0.1f &&
		Tuning.LandingMovementHandoffWindow >= 0.0f;
}
