// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgGaspMoverTraversalTestFixture.h"

#if ENABLE_PIE_NETWORK_TEST
/** Grounded Hurdle uses the saved map, real input and the shared Fixed prediction/observer contracts. */
NETWORK_TEST_CLASS(GaspMoverHurdlePIE, "SurvivalRpg.GASP.Mover.Hurdle")
{
	using EGait = RpgGaspMoverTraversalTests::EGait;
	using EScenario = RpgGaspMoverTraversalTests::EScenario;
	FRpgGaspMoverTraversalTestFixture Fixture{TestRunner, Assert, TestCommandBuilder};
	BEFORE_EACH() { Fixture.Initialize(); }
	AFTER_EACH() { Fixture.Cleanup(); }
	void Queue(EGait Gait, EScenario Scenario = EScenario::Success, bool bHost = false, float Yaw = 0.0f, int32 Lane = 0)
	{
		Fixture.Queue(Gait, Scenario, bHost, Yaw, RpgGaspMoverTraversalTests::EAction::Hurdle, Lane);
	}
	TEST_METHOD(RemoteStandingHurdleNaturallyFinishesOnBackFloor) { Queue(EGait::Stand, EScenario::NaturalEnd); }
	TEST_METHOD(WideStandingHurdleResumesInputAtConditionalHandoff) { Queue(EGait::Stand, EScenario::ResumeStandingInput, false, 0.0f, 1); }
	TEST_METHOD(RemoteWalkingHurdleReturnsToWalkingBeyondRearEdge) { Queue(EGait::Walk); }
	TEST_METHOD(RemoteRunningHurdleKeepsMomentumAcrossGroundedHandoff) { Queue(EGait::Run); }
	TEST_METHOD(ListenHostRunningHurdleKeepsObserverPhaseWithDisplayedMovement) { Queue(EGait::Run, EScenario::Success, true); }
	TEST_METHOD(PositiveAngleAlignsOwnerBodyWithoutTurningView) { Queue(EGait::Run, EScenario::Success, false, 35.0f); }
	TEST_METHOD(NegativeAngleAlignsListenHostWithoutTurningView) { Queue(EGait::Run, EScenario::Success, true, -35.0f); }
	TEST_METHOD(MaximumDepthHurdleLandsBeyondRearEdge) { Queue(EGait::Run, EScenario::Success, false, 0.0f, 2); }
	TEST_METHOD(ServerBlockedBackFloorRejectsThePredictedHurdle) { Queue(EGait::Run, EScenario::BlockedExit); }
	TEST_METHOD(CancellationReleasesAllWarpTargetsAndColliderOwnership) { Queue(EGait::Stand, EScenario::Cancel); }
	TEST_METHOD(DeathDuringHurdleReleasesTraversalAndStopsMover) { Queue(EGait::Run, EScenario::Death); }
	TEST_METHOD(LostAuthoritySupportCancelsTheActiveHurdle) { Queue(EGait::Stand, EScenario::SupportLoss); }
	TEST_METHOD(LateJoinReconstructsTheCurrentHurdleState) { Queue(EGait::Stand, EScenario::LateJoin); }
	TEST_METHOD(FixedCorrectionPreservesBackFloorSupportAndWarpHistory) { Queue(EGait::Stand, EScenario::CorrectDuringWarp); }
	TEST_METHOD(FixedCorrectionAfterHandoffCannotRestoreOldHurdle) { Queue(EGait::Stand, EScenario::CorrectAfterWarp); }
};
#endif
#endif
