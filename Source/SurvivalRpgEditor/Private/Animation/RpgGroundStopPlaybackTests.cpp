// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <cmath>
#include <limits>

#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgMotionMatchingRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGroundStopPlaybackBoundaryTest,
	"SurvivalRpg.Animation.MotionMatching.GroundStopPlayback.StationaryBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGroundStopPlaybackBoundaryTest::RunTest(const FString& Parameters)
{
	FRpgGaspLocomotionTuning Tuning;
	const ERpgMotionMatchingDatabaseRole Stops[] = {
		ERpgMotionMatchingDatabaseRole::StandWalkStops,
		ERpgMotionMatchingDatabaseRole::StandRunStops,
		ERpgMotionMatchingDatabaseRole::StandSprintStops};
	for (const ERpgMotionMatchingDatabaseRole Role : Stops)
	{
		TestTrue(TEXT("A physically stopped Stop pose can yield to Idle"),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(Role, false, 0.0f, Tuning));
		TestTrue(TEXT("The existing horizontal stillness threshold is inclusive"),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
				Role, false, Tuning.ChooserVelocityTolerance, Tuning));
		TestFalse(TEXT("The first representable speed above stillness retains normal Stop playback"),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(Role, false,
				std::nextafter(Tuning.ChooserVelocityTolerance, std::numeric_limits<float>::infinity()), Tuning));
		TestFalse(TEXT("An active logical movement request does not release through the stillness rule"),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(Role, true, 0.0f, Tuning));
		TestFalse(TEXT("Ordinary deceleration retains its Stop pose"),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(Role, false, 250.0f, Tuning));
	}

	Tuning.ChooserVelocityTolerance = 0.75f;
	TestTrue(TEXT("Designer velocity tuning controls the boundary in cm/s"),
		RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
			ERpgMotionMatchingDatabaseRole::StandRunStops, false, 0.75f, Tuning));
	TestFalse(TEXT("Speed above a custom threshold is still moving"),
		RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
			ERpgMotionMatchingDatabaseRole::StandRunStops, false, 0.8f, Tuning));
	Tuning.ChooserVelocityTolerance = 0.0f;
	TestTrue(TEXT("A zero configured tolerance accepts exact physical zero"),
		RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
			ERpgMotionMatchingDatabaseRole::StandRunStops, false, 0.0f, Tuning));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGroundStopPlaybackDomainTest,
	"SurvivalRpg.Animation.MotionMatching.GroundStopPlayback.DomainIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGroundStopPlaybackDomainTest::RunTest(const FString& Parameters)
{
	const FRpgGaspLocomotionTuning Tuning;
	for (uint8 Index = 0; Index < static_cast<uint8>(ERpgMotionMatchingDatabaseRole::Count); ++Index)
	{
		const ERpgMotionMatchingDatabaseRole Role = static_cast<ERpgMotionMatchingDatabaseRole>(Index);
		if (Role == ERpgMotionMatchingDatabaseRole::StandWalkStops ||
			Role == ERpgMotionMatchingDatabaseRole::StandRunStops ||
			Role == ERpgMotionMatchingDatabaseRole::StandSprintStops)
		{
			continue;
		}
		TestFalse(*FString::Printf(TEXT("Role %d keeps its own playback contract"), Index),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(Role, false, 0.0f, Tuning));
	}
	TestFalse(TEXT("Unknown selected roles do not create a repeated interrupt"),
		RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
			static_cast<ERpgMotionMatchingDatabaseRole>(255), false, 0.0f, Tuning));

	FRpgGroundMotionMatchingDomainState Decelerating;
	Decelerating.PhysicalMovementState = ERpgLocomotionMovementState::Grounded;
	Decelerating.Stance = ERpgLocomotionStance::Standing;
	Decelerating.Gait = ERpgLocomotionGait::Run;
	Decelerating.bChooserMoving = false;
	FRpgGroundMotionMatchingDomainState Stopped = Decelerating;
	Stopped.Gait = ERpgLocomotionGait::Idle;
	TestFalse(TEXT("The existing broad domain rule intentionally does not interrupt a decelerating gait change"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, Decelerating, Stopped));
	TestTrue(TEXT("The observed Stop-to-physical-Idle seam now has a dedicated release"),
		RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
			ERpgMotionMatchingDatabaseRole::StandRunStops, Stopped.bChooserMoving, 0.0f, Tuning));
	FRpgGroundMotionMatchingSelectionSnapshot Selection;
	Selection.MovementState = ERpgLocomotionMovementState::Grounded;
	Selection.Stance = ERpgLocomotionStance::Standing;
	Selection.Gait = ERpgLocomotionGait::Idle;
	Selection.bIsMovingOnGround = true;
	const FRpgResolvedMotionMatchingDatabaseRoles Roles = RpgMotionMatchingRuntime::ResolveDatabaseRoles(Selection, Tuning);
	if (TestEqual(TEXT("Physical Idle already requests one grounded database"), Roles.Num(), 1))
	{
		TestEqual(TEXT("The existing selector supplies Idle for the database-change blend"),
			Roles[0], ERpgMotionMatchingDatabaseRole::StandIdle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGroundStopPlaybackInvalidInputTest,
	"SurvivalRpg.Animation.MotionMatching.GroundStopPlayback.InvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGroundStopPlaybackInvalidInputTest::RunTest(const FString& Parameters)
{
	const float InvalidValues[] = {-1.0f, std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
	for (const float InvalidValue : InvalidValues)
	{
		FRpgGaspLocomotionTuning Tuning;
		TestFalse(TEXT("Invalid physical speed cannot request a Stop exit"),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
				ERpgMotionMatchingDatabaseRole::StandRunStops, false, InvalidValue, Tuning));
		Tuning.ChooserVelocityTolerance = InvalidValue;
		TestFalse(TEXT("Invalid stillness tuning cannot request a Stop exit"),
			RpgMotionMatchingRuntime::ShouldReleaseStoppedGroundPose(
				ERpgMotionMatchingDatabaseRole::StandRunStops, false, 0.0f, Tuning));
	}
	return true;
}

#endif
