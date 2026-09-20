// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgGaspMoverTraversalTestFixture.h"

#if ENABLE_PIE_NETWORK_TEST
/** Vault shares the actual Mover input/rollback harness; success requires crossing, falling and lower-floor landing. */
NETWORK_TEST_CLASS(GaspMoverVaultPIE, "SurvivalRpg.GASP.Mover.Vault")
{
	using EGait = RpgGaspMoverTraversalTests::EGait;
	using EScenario = RpgGaspMoverTraversalTests::EScenario;
	FRpgGaspMoverTraversalTestFixture Fixture{TestRunner, Assert, TestCommandBuilder};
	BEFORE_EACH() { Fixture.Initialize(); }
	AFTER_EACH() { Fixture.Cleanup(); }
	void Queue(EGait Gait, EScenario Scenario = EScenario::Success, bool bHost = false, float Yaw = 0.0f, int32 Lane = 0)
	{
		Fixture.Queue(Gait, Scenario, bHost, Yaw, RpgGaspMoverTraversalTests::EAction::Vault, Lane);
	}
	TEST_METHOD(RemoteStandingVaultCrossesRearEdgeAndFallsToLowerFloor) { Queue(EGait::Stand); }
	TEST_METHOD(RemoteWalkingVaultCrossesRearEdgeAndKeepsMoving) { Queue(EGait::Walk); }
	TEST_METHOD(RemoteRunningVaultCrossesRearEdgeAndKeepsMoving) { Queue(EGait::Run); }
	TEST_METHOD(ListenHostRunningVaultKeepsObserverMontageWithDisplayedMovement) { Queue(EGait::Run, EScenario::Success, true); }
	TEST_METHOD(EquipmentMontageReplacesPresentedVaultThenSameAssetCanReplay) { Queue(EGait::Stand, EScenario::ReplaceActiveAndReplay, true); }
	TEST_METHOD(EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection) { Queue(EGait::Stand, EScenario::ReplacePendingAndReplay, true); }
	TEST_METHOD(PositiveAngleAlignsOwnerBodyWithoutTurningView) { Queue(EGait::Run, EScenario::Success, false, 35.0f); }
	TEST_METHOD(NegativeAngleAlignsListenHostWithoutTurningView) { Queue(EGait::Run, EScenario::Success, true, -35.0f); }
	TEST_METHOD(ListenHostStandingVaultCrossesTheThinnerBarrier) { Queue(EGait::Stand, EScenario::Success, true, 0.0f, 1); }
	TEST_METHOD(ServerBlockedExitRejectsThePredictedVault) { Queue(EGait::Run, EScenario::BlockedExit); }
	TEST_METHOD(CancellationReleasesBothWarpTargetsAndColliderOwnership) { Queue(EGait::Stand, EScenario::Cancel); }
	TEST_METHOD(DeathDuringVaultReleasesTraversalAndStopsMover) { Queue(EGait::Run, EScenario::Death); }
	TEST_METHOD(LostAuthorityColliderCancelsTheActiveVault) { Queue(EGait::Stand, EScenario::ColliderLoss); }
	TEST_METHOD(LateJoinReconstructsTheCurrentVaultState) { Queue(EGait::Stand, EScenario::LateJoin); }
	TEST_METHOD(FixedCorrectionPreservesBothWarpTargetsAndCollider) { Queue(EGait::Stand, EScenario::CorrectDuringWarp); }
	TEST_METHOD(FixedCorrectionAfterHandoffCannotRestoreOldVault) { Queue(EGait::Stand, EScenario::CorrectAfterWarp); }
};
#endif
#endif
