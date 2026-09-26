// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgGaspMoverTraversalTestFixture.h"

#if ENABLE_PIE_NETWORK_TEST
NETWORK_TEST_CLASS(GaspMoverMantlePIE, "SurvivalRpg.GASP.Mover.Mantle")
{
	using EScenario = RpgGaspMoverTraversalTests::EScenario;
	using EGait = RpgGaspMoverTraversalTests::EGait;
	FRpgGaspMoverTraversalTestFixture Fixture{TestRunner, Assert, TestCommandBuilder};
	BEFORE_EACH() { Fixture.Initialize(); }
	AFTER_EACH() { Fixture.Cleanup(); }
	TEST_METHOD(RemoteStandingMantleKeepsMomentumAfterHandoff) { Fixture.Queue(EGait::Stand); }
	TEST_METHOD(RemoteWalkingMantleUsesGroundedWalkingEntry) { Fixture.Queue(EGait::Walk); }
	TEST_METHOD(RemoteRunningMantleKeepsMomentumAfterHandoff) { Fixture.Queue(EGait::Run); }
	TEST_METHOD(PositiveAngleAlignsOwnerBodyWithoutTurningView) { Fixture.Queue(EGait::Run, EScenario::Success, false, 35.0f); }
	TEST_METHOD(NegativeAngleAlignsListenHostWithoutTurningView) { Fixture.Queue(EGait::Run, EScenario::Success, true, -35.0f); }
	TEST_METHOD(ListenHostStandingMantleUsesTheSameAbility) { Fixture.Queue(EGait::Stand, EScenario::Success, true); }
	TEST_METHOD(SpaceAwayFromAnObstacleRemainsAnOrdinaryJump) { Fixture.Queue(EGait::Run, EScenario::Jump); }
	TEST_METHOD(HeldSpaceRetriesOnlyAfterTheOrdinaryJumpLands) { Fixture.Queue(EGait::Run, EScenario::HeldRetry); }
	TEST_METHOD(ServerBlockedLandingRejectsThePredictedMantle) { Fixture.Queue(EGait::Run, EScenario::BlockedExit); }
	TEST_METHOD(CancellationReleasesMoverWarpAndColliderOwnership) { Fixture.Queue(EGait::Run, EScenario::Cancel, false, 35.0f); }
	TEST_METHOD(DeathDuringMantleReleasesTraversalAndStopsMover) { Fixture.Queue(EGait::Run, EScenario::Death); }
	TEST_METHOD(LostAuthorityColliderCancelsTheActiveMantle) { Fixture.Queue(EGait::Run, EScenario::ColliderLoss); }
	TEST_METHOD(LateJoinReconstructsTheCurrentMantleState) { Fixture.Queue(EGait::Stand, EScenario::LateJoin); }
	TEST_METHOD(FixedCorrectionPreservesActiveWarpAndCollider) { Fixture.Queue(EGait::Stand, EScenario::CorrectDuringWarp); }
	TEST_METHOD(FixedCorrectionAfterHandoffCannotRestoreOldTraversal) { Fixture.Queue(EGait::Stand, EScenario::CorrectAfterWarp); }
	TEST_METHOD(NaturalAutoBlendCompletionRetainsFinishedOnEveryRole) { Fixture.Queue(EGait::Stand, EScenario::AutoBlendNaturalEnd); }
	TEST_METHOD(AuthorityCancellationDuringAutoBlendRemainsCancelled) { Fixture.Queue(EGait::Stand, EScenario::CancelDuringAutoBlend); }

};
#endif
#endif
