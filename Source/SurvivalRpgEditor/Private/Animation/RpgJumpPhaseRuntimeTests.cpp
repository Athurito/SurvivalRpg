// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgJumpPhaseRuntimeTest,
	"SurvivalRpg.Animation.Jump.Runtime.PhaseAndProceduralGates",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgJumpPhaseRuntimeTest::RunTest(const FString& Parameters)
{
	USkeletalMeshComponent* AnimInstanceOuter = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(AnimInstanceOuter);
	UPoseSearchDatabase* StandLightLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* StandHeavyLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkLightLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkHeavyLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunLightLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunHeavyLandingDatabase = NewObject<UPoseSearchDatabase>();
	UAnimSequence* LandingClip = NewObject<UAnimSequence>();
	UAnimSequence* PreviousAirborneClip = NewObject<UAnimSequence>();
	if (!TestNotNull(TEXT("Transient RPG AnimInstance can be created"), AnimInstance) ||
		!TestNotNull(TEXT("Transient Stand Light landing database can be created"), StandLightLandingDatabase) ||
		!TestNotNull(TEXT("Transient Stand Heavy landing database can be created"), StandHeavyLandingDatabase) ||
		!TestNotNull(TEXT("Transient Walk Light landing database can be created"), WalkLightLandingDatabase) ||
		!TestNotNull(TEXT("Transient Walk Heavy landing database can be created"), WalkHeavyLandingDatabase) ||
		!TestNotNull(TEXT("Transient Run Light landing database can be created"), RunLightLandingDatabase) ||
		!TestNotNull(TEXT("Transient Run Heavy landing database can be created"), RunHeavyLandingDatabase) ||
		!TestNotNull(TEXT("Transient landing clip can be created"), LandingClip))
	{
		return false;
	}
	AnimInstance->LandingMotionMatchingDatabase = StandLightLandingDatabase;
	AnimInstance->StandHeavyLandingMotionMatchingDatabase = StandHeavyLandingDatabase;
	AnimInstance->WalkLightLandingMotionMatchingDatabase = WalkLightLandingDatabase;
	AnimInstance->WalkHeavyLandingMotionMatchingDatabase = WalkHeavyLandingDatabase;
	AnimInstance->RunLightLandingMotionMatchingDatabase = RunLightLandingDatabase;
	AnimInstance->RunHeavyLandingMotionMatchingDatabase = RunHeavyLandingDatabase;

	static const ERpgMotionMatchingDatabaseRole StationaryLandingRoles[] =
	{
		ERpgMotionMatchingDatabaseRole::StandLightLanding,
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
	};
	const float QuietNaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	for (const ERpgMotionMatchingDatabaseRole LandingRole : StationaryLandingRoles)
	{
		const FName RoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(LandingRole);
		TestFalse(
			*FString::Printf(TEXT("%s stays active at the inclusive 3 cm/s Idle boundary"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldReleaseStationaryLanding(LandingRole, false, 3.0f));
		TestTrue(
			*FString::Printf(TEXT("%s releases above the Idle boundary"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldReleaseStationaryLanding(LandingRole, false, 3.01f));
		TestTrue(
			*FString::Printf(TEXT("%s releases for zero-speed grounded intent"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldReleaseStationaryLanding(LandingRole, true, 0.0f));
		TestTrue(
			*FString::Printf(TEXT("%s releases for a non-finite NaN speed"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldReleaseStationaryLanding(LandingRole, false, QuietNaN));
		TestTrue(
			*FString::Printf(TEXT("%s releases for a non-finite infinite speed"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldReleaseStationaryLanding(LandingRole, false, Infinity));
	}

	static const ERpgMotionMatchingDatabaseRole MovingLandingRoles[] =
	{
		ERpgMotionMatchingDatabaseRole::WalkLightLanding,
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
		ERpgMotionMatchingDatabaseRole::RunLightLanding,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
	};
	for (const ERpgMotionMatchingDatabaseRole LandingRole : MovingLandingRoles)
	{
		const FName RoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(LandingRole);
		TestFalse(
			*FString::Printf(TEXT("%s preserves its authored moving landing under live input"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldReleaseStationaryLanding(LandingRole, true, 450.0f));
	}

	static const ERpgMotionMatchingDatabaseRole LandingExitRoles[] =
	{
		ERpgMotionMatchingDatabaseRole::StandLightLanding,
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
		ERpgMotionMatchingDatabaseRole::WalkLightLanding,
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
		ERpgMotionMatchingDatabaseRole::RunLightLanding,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
	};
	for (const ERpgMotionMatchingDatabaseRole LandingRole : LandingExitRoles)
	{
		const FName RoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(LandingRole);
		TestFalse(
			*FString::Printf(TEXT("An active unarmed %s remains a Continuing Pose"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldInterruptLandingDatabaseExit(
				ERpgJumpPhase::Landing,
				false,
				LandingRole));
		TestTrue(
			*FString::Printf(TEXT("A completed %s interrupts its database exit"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldInterruptLandingDatabaseExit(
				ERpgJumpPhase::Landing,
				true,
				LandingRole));
		TestTrue(
			*FString::Printf(TEXT("A reset %s interrupts its handoff to gait locomotion"), *RoleTag.ToString()),
			URpgAnimInstance::ShouldInterruptLandingDatabaseExit(
				ERpgJumpPhase::Grounded,
				false,
				LandingRole));
	}
	TestFalse(
		TEXT("A normal grounded database never manufactures a landing-exit interrupt"),
		URpgAnimInstance::ShouldInterruptLandingDatabaseExit(
			ERpgJumpPhase::Grounded,
			false,
			ERpgMotionMatchingDatabaseRole::StandRunLoops));

	FRpgAnimInstanceProxy Proxy;
	Proxy.bTurnInPlaceHardReset = false;
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	int32 LandingAirborneEpoch = 0;
	auto SetValidLandingSelectionSnapshot =
		[&Proxy, &LandingAirborneEpoch](
			ERpgLocomotionGait Gait,
			float HorizontalSpeed,
			float DownwardSpeed,
			bool bHasMoveIntent)
	{
		FRpgLandingSelectionSnapshot& Snapshot = Proxy.LandingSelectionSnapshot;
		Snapshot = FRpgLandingSelectionSnapshot();
		Snapshot.HorizontalVelocity = FVector(HorizontalSpeed, 0.0f, 0.0f);
		Snapshot.HorizontalSpeed = HorizontalSpeed;
		Snapshot.VerticalVelocity = -DownwardSpeed;
		Snapshot.MaximumDownwardSpeed = DownwardSpeed;
		Snapshot.PredictedImpactDownwardSpeed = DownwardSpeed;
		Snapshot.Gait = Gait;
		Snapshot.PredictedLanding.LandingLocation = FVector(HorizontalSpeed * 0.1f, 0.0f, 0.0f);
		Snapshot.PredictedLanding.LandingNormal = FVector::UpVector;
		Snapshot.PredictedLanding.TimeToLand = 0.1f;
		Snapshot.PredictedLanding.bIsValid = true;
		Snapshot.AirborneEpoch = ++LandingAirborneEpoch;
		Snapshot.bHasMoveIntent = bHasMoveIntent;
		Snapshot.bIsValid = true;
	};
	AnimInstance->ResetJumpPhaseRuntime();
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A regular grounded snapshot stays grounded"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Falling enters the explicit airborne phase"), AnimInstance->JumpPhase, ERpgJumpPhase::Airborne);
	TestFalse(TEXT("Airborne never owns a landing selection"), AnimInstance->bLandingSelectionLatched);

	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	Proxy.GroundSpeed = 300.0f;
	Proxy.Gait = ERpgLocomotionGait::Run;
	Proxy.bHasGroundedMoveIntent = true;
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Run, 300.0f, 500.0f, true);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A moving touchdown enters the landing lifecycle"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(
		TEXT("A moving light touchdown freezes the Run Light database role"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	const uint32 RunLandingRequest = AnimInstance->LandingRequestSerial;
	TestTrue(TEXT("A moving touchdown creates a serialized landing request"), RunLandingRequest != 0u);
	TestFalse(
		TEXT("A moving landing cannot latch a result from another requested role"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			WalkLightLandingDatabase,
			0.2f,
			false,
			RunLandingRequest));
	TestTrue(
		TEXT("A moving landing latches only its exact requested Run Light role"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			RunLightLandingDatabase,
			0.2f,
			false,
			RunLandingRequest));
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(
		TEXT("A Run landing remains active under its normal grounded intent and speed"),
		AnimInstance->JumpPhase,
		ERpgJumpPhase::Landing);
	TestTrue(
		TEXT("Live moving input does not clear the Run landing latch"),
		AnimInstance->bLandingSelectionLatched);
	TestEqual(
		TEXT("Live moving input does not create a second Run landing request"),
		AnimInstance->LandingRequestSerial,
		RunLandingRequest);

	// The final airborne snapshot is consumed only at touchdown. Later grounded input and gait
	// changes must not reclassify an already running cosmetic landing request.
	Proxy.GroundSpeed = 0.0f;
	Proxy.Gait = ERpgLocomotionGait::Idle;
	Proxy.bHasGroundedMoveIntent = false;
	Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A running landing survives the grounded-input freeze"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(
		TEXT("Grounded speed, input, and gait changes cannot reclassify the active landing"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	TestTrue(
		TEXT("The exact moving landing asset remains active after grounded input is released"),
		AnimInstance->IsActiveLandingAsset(LandingClip));
	TestEqual(TEXT("Grounded-input changes do not create another request"), AnimInstance->LandingRequestSerial, RunLandingRequest);
	AnimInstance->ResetJumpPhaseRuntime();
	TestEqual(
		TEXT("Resetting the landing lifecycle clears its frozen database role"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::None);

	struct FStationaryLandingCase
	{
		const TCHAR* Name;
		ERpgMotionMatchingDatabaseRole Role;
		UPoseSearchDatabase* Database;
		float DownwardSpeed;
	};
	const FStationaryLandingCase StationaryLandingCases[] =
	{
		{
			TEXT("Stand Light"),
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			StandLightLandingDatabase,
			500.0f,
		},
		{
			TEXT("Stand Heavy"),
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			StandHeavyLandingDatabase,
			700.0f,
		},
	};
	auto BeginStationaryAirbornePhase = [&]()
	{
		AnimInstance->ResetJumpPhaseRuntime();
		Proxy.bIsAnyMontagePlaying = false;
		Proxy.bIsCrouching = false;
		Proxy.bHasTurnInPlaceBlockingGameplayTag = false;
		Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
		Proxy.bIsMovingOnGround = false;
		Proxy.bIsFalling = true;
		Proxy.GroundSpeed = 0.0f;
		Proxy.Gait = ERpgLocomotionGait::Idle;
		Proxy.bHasGroundedMoveIntent = false;
		Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			TEXT("The stationary regression fixture enters Airborne"),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Airborne);
	};
	auto TouchDownStationary = [&](const FStationaryLandingCase& LandingCase)
	{
		SetValidLandingSelectionSnapshot(
			ERpgLocomotionGait::Idle,
			0.0f,
			LandingCase.DownwardSpeed,
			false);
		Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
		Proxy.bIsMovingOnGround = true;
		Proxy.bIsFalling = false;
		Proxy.GroundSpeed = 0.0f;
		Proxy.Gait = ERpgLocomotionGait::Idle;
		Proxy.bHasGroundedMoveIntent = false;
	};
	auto EnterLatchedStationaryLanding = [&](const FStationaryLandingCase& LandingCase)
	{
		BeginStationaryAirbornePhase();
		TouchDownStationary(LandingCase);
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("%s touchdown enters Landing"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Landing);
		TestEqual(
			*FString::Printf(TEXT("%s touchdown selects its exact role"), LandingCase.Name),
			AnimInstance->ActiveLandingDatabaseRole,
			LandingCase.Role);
		const uint32 RequestSerial = AnimInstance->LandingRequestSerial;
		TestTrue(
			*FString::Printf(TEXT("%s touchdown creates a non-zero request serial"), LandingCase.Name),
			RequestSerial != 0u);
		TestTrue(
			*FString::Printf(TEXT("%s touchdown latches its exact database"), LandingCase.Name),
			AnimInstance->TryLatchLandingSelection(
				LandingClip,
				LandingCase.Database,
				0.2f,
				false,
				RequestSerial));
		AnimInstance->CurrentMotionMatchingDatabaseRole = LandingCase.Role;
		return RequestSerial;
	};

	// Input arriving on the physical touchdown frame must skip both stationary landing families.
	for (const FStationaryLandingCase& LandingCase : StationaryLandingCases)
	{
		BeginStationaryAirbornePhase();
		TouchDownStationary(LandingCase);
		Proxy.bHasGroundedMoveIntent = true;
		const uint32 RequestSerialBeforeTouchdown = AnimInstance->LandingRequestSerial;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("%s is skipped when grounded intent arrives on touchdown"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Grounded);
		TestEqual(
			*FString::Printf(TEXT("Skipped %s creates no landing request"), LandingCase.Name),
			AnimInstance->LandingRequestSerial,
			RequestSerialBeforeTouchdown);
		TestFalse(
			*FString::Printf(TEXT("Skipped %s owns no landing latch"), LandingCase.Name),
			AnimInstance->bLandingSelectionLatched);
		TestEqual(
			*FString::Printf(TEXT("Skipped %s owns no selected request serial"), LandingCase.Name),
			AnimInstance->LandingSelectedRequestSerial,
			0u);
		TestFalse(
			*FString::Printf(TEXT("Skipped %s emits no landing ForceInterrupt"), LandingCase.Name),
			AnimInstance->ConsumeLandingForceInterruptRequest());
	}

	// An already playing stationary landing releases immediately for live grounded intent.
	for (const FStationaryLandingCase& LandingCase : StationaryLandingCases)
	{
		const uint32 RequestSerial = EnterLatchedStationaryLanding(LandingCase);
		Proxy.bHasGroundedMoveIntent = true;
		Proxy.GroundSpeed = 0.0f;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("Active %s releases for zero-speed grounded intent"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Grounded);
		TestFalse(
			*FString::Printf(TEXT("Released %s clears its landing latch"), LandingCase.Name),
			AnimInstance->bLandingSelectionLatched);
		TestEqual(
			*FString::Printf(TEXT("Released %s clears its frozen role"), LandingCase.Name),
			AnimInstance->ActiveLandingDatabaseRole,
			ERpgMotionMatchingDatabaseRole::None);
		TestEqual(
			*FString::Printf(TEXT("Released %s preserves the completed request serial"), LandingCase.Name),
			AnimInstance->LandingRequestSerial,
			RequestSerial);
		TestEqual(
			*FString::Printf(TEXT("Released %s clears its selected request serial"), LandingCase.Name),
			AnimInstance->LandingSelectedRequestSerial,
			0u);
		TestFalse(
			*FString::Printf(TEXT("Released %s rejects its stale completed-search result"), LandingCase.Name),
			AnimInstance->TryLatchLandingSelection(
				LandingClip,
				LandingCase.Database,
				0.2f,
				false,
				RequestSerial));
		TestTrue(
			*FString::Printf(TEXT("Released %s interrupts its landing-database handoff"), LandingCase.Name),
			URpgAnimInstance::ShouldInterruptLandingDatabaseExit(
				AnimInstance->JumpPhase,
				AnimInstance->bLandingCompletionArmed,
				AnimInstance->CurrentMotionMatchingDatabaseRole));
	}

	// The live speed gate preserves the inclusive Idle boundary and releases immediately above it.
	for (const FStationaryLandingCase& LandingCase : StationaryLandingCases)
	{
		const uint32 RequestSerial = EnterLatchedStationaryLanding(LandingCase);
		Proxy.bHasGroundedMoveIntent = false;
		Proxy.GroundSpeed = 3.0f;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("Active %s remains at exactly 3 cm/s"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Landing);
		TestTrue(
			*FString::Printf(TEXT("Active %s keeps its latch at exactly 3 cm/s"), LandingCase.Name),
			AnimInstance->bLandingSelectionLatched);
		TestEqual(
			*FString::Printf(TEXT("Active %s keeps its selected serial at exactly 3 cm/s"), LandingCase.Name),
			AnimInstance->LandingSelectedRequestSerial,
			RequestSerial);

		Proxy.GroundSpeed = 3.01f;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("Active %s releases above 3 cm/s"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Grounded);
		TestFalse(
			*FString::Printf(TEXT("Speed-released %s clears its latch"), LandingCase.Name),
			AnimInstance->bLandingSelectionLatched);
		TestEqual(
			*FString::Printf(TEXT("Speed-released %s preserves its request serial"), LandingCase.Name),
			AnimInstance->LandingRequestSerial,
			RequestSerial);
		TestEqual(
			*FString::Printf(TEXT("Speed-released %s clears its selected serial"), LandingCase.Name),
			AnimInstance->LandingSelectedRequestSerial,
			0u);
		TestTrue(
			*FString::Printf(TEXT("Speed-released %s interrupts its landing-database handoff"), LandingCase.Name),
			URpgAnimInstance::ShouldInterruptLandingDatabaseExit(
				AnimInstance->JumpPhase,
				AnimInstance->bLandingCompletionArmed,
				AnimInstance->CurrentMotionMatchingDatabaseRole));
	}
	AnimInstance->CurrentMotionMatchingDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
	AnimInstance->ResetJumpPhaseRuntime();
	Proxy.bHasGroundedMoveIntent = false;
	Proxy.GroundSpeed = 0.0f;

	const uint32 IdleLandingRequestBeforeTouchdown = AnimInstance->LandingRequestSerial;
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	Proxy.bHasGroundedMoveIntent = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	Proxy.GroundSpeed = 3.0f;
	Proxy.Gait = ERpgLocomotionGait::Idle;
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 3.0f, 500.0f, false);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("An idle touchdown enters Landing at the inclusive 3 cm/s boundary"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(
		TEXT("The idle touchdown selects the preserved Stand Light role"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	TestEqual(
		TEXT("The idle touchdown creates exactly one new request"),
		AnimInstance->LandingRequestSerial,
		IdleLandingRequestBeforeTouchdown + 1u);
	TestTrue(TEXT("A touchdown requests ForceInterrupt exactly once"), AnimInstance->ConsumeLandingForceInterruptRequest());
	TestFalse(TEXT("The same touchdown cannot request ForceInterrupt twice"), AnimInstance->ConsumeLandingForceInterruptRequest());

	TestFalse(
		TEXT("A result from another database cannot latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			RunLightLandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial));
	TestFalse(
		TEXT("A stale request serial cannot latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			StandLightLandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial - 1));
	TestTrue(
		TEXT("The first valid exclusive landing result latches"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			StandLightLandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial));
	TestTrue(TEXT("The exact landing asset is active after latching"), AnimInstance->IsActiveLandingAsset(LandingClip));
	TestFalse(
		TEXT("A second result cannot replace the latched landing"),
		AnimInstance->TryLatchLandingSelection(
			PreviousAirborneClip,
			StandLightLandingDatabase,
			0.0f,
			false,
			AnimInstance->LandingRequestSerial));

	AnimInstance->UpdateLandingLatchedPlayback(
		PreviousAirborneClip,
		0.5f,
		1.0f,
		1.0f,
		0.01f);
	TestFalse(
		TEXT("A transient outgoing airborne pose is not mistaken for observed landing playback"),
		AnimInstance->bLandingPlaybackObserved);
	TestFalse(TEXT("The outgoing pose does not end the landing hold"), AnimInstance->bLandingCompletionArmed);

	AnimInstance->UpdateLandingLatchedPlayback(LandingClip, 0.5f, 1.0f, 1.0f, 0.01f);
	TestTrue(TEXT("The Blend Stack observes the exact landing clip"), AnimInstance->bLandingPlaybackObserved);
	TestTrue(
		TEXT("The natural watchdog is bounded to the remaining clip plus safety"),
		FMath::IsNearlyEqual(AnimInstance->LandingPlaybackWatchdogDuration, 0.6f));
	AnimInstance->UpdateJumpPhaseRuntime(0.5f, Proxy);
	TestEqual(TEXT("Landing remains latched before clip end"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);

	AnimInstance->UpdateLandingLatchedPlayback(LandingClip, 0.95f, 1.0f, 1.0f, 0.01f);
	TestTrue(TEXT("The clip-end tolerance arms landing completion"), AnimInstance->bLandingCompletionArmed);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Natural landing completion returns to grounded locomotion"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);
	TestFalse(TEXT("Natural completion clears the landing selection"), AnimInstance->bLandingSelectionLatched);

	// A missing database result must release to gait locomotion after the fixed selection timeout.
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 0.0f, 500.0f, false);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	const uint32 MissingSelectionRequest = AnimInstance->LandingRequestSerial;
	AnimInstance->UpdateJumpPhaseRuntime(0.24f, Proxy);
	TestEqual(TEXT("A missing selection remains bounded before 250 ms"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	AnimInstance->UpdateJumpPhaseRuntime(0.02f, Proxy);
	TestEqual(TEXT("A missing selection releases after 250 ms"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	// A new touchdown owns a new one-shot interrupt even after the previous request timed out.
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 0.0f, 500.0f, false);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestTrue(TEXT("A later touchdown advances the request serial"), AnimInstance->LandingRequestSerial > MissingSelectionRequest);
	TestTrue(TEXT("The later touchdown receives one new ForceInterrupt"), AnimInstance->ConsumeLandingForceInterruptRequest());
	TestFalse(TEXT("The later touchdown also interrupts only once"), AnimInstance->ConsumeLandingForceInterruptRequest());

	// Looping or stuck content cannot hold gameplay locomotion indefinitely.
	TestTrue(
		TEXT("A looping safety fixture can latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			StandLightLandingDatabase,
			0.0f,
			true,
			AnimInstance->LandingRequestSerial));
	AnimInstance->UpdateLandingLatchedPlayback(LandingClip, 0.1f, 1.0f, 1.0f, 0.01f);
	AnimInstance->UpdateJumpPhaseRuntime(1.24f, Proxy);
	TestEqual(TEXT("A stuck landing remains held before 1.25 seconds"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	AnimInstance->UpdateJumpPhaseRuntime(0.02f, Proxy);
	TestEqual(TEXT("A stuck landing releases after 1.25 seconds"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	// Overrides and a second jump cancel a cosmetic landing immediately.
	auto EnterLanding = [&]()
	{
		Proxy.bIsAnyMontagePlaying = false;
		Proxy.bIsCrouching = false;
		Proxy.bHasTurnInPlaceBlockingGameplayTag = false;
		Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
		Proxy.bIsMovingOnGround = false;
		Proxy.bIsFalling = true;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 0.0f, 500.0f, false);
		Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
		Proxy.bIsMovingOnGround = true;
		Proxy.bIsFalling = false;
		Proxy.GroundSpeed = 0.0f;
		Proxy.Gait = ERpgLocomotionGait::Idle;
		Proxy.bHasGroundedMoveIntent = false;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	};

	EnterLanding();
	Proxy.bIsAnyMontagePlaying = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A montage cancels cosmetic landing playback"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	EnterLanding();
	Proxy.bIsCrouching = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Crouch cancels cosmetic landing playback"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	EnterLanding();
	Proxy.bHasTurnInPlaceBlockingGameplayTag = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A blocking gameplay tag cancels cosmetic landing playback"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	EnterLanding();
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A second jump immediately re-enters Airborne"), AnimInstance->JumpPhase, ERpgJumpPhase::Airborne);
	TestFalse(TEXT("A second jump clears the old landing selection"), AnimInstance->bLandingSelectionLatched);

	// Immutable asset categories keep each Blend Stack sample stable while global movement phases change.
	UPackage* JumpStartPackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/RpgJumpRuntimeTest_Start"));
	UAnimSequence* JumpStart = NewObject<UAnimSequence>(
		JumpStartPackage,
		MakeUniqueObjectName(JumpStartPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Start")));
	JumpStart->bLoop = false;
	TestTrue(TEXT("A non-looping sequence in the Jump Starts contract is a jump start"), AnimInstance->IsAirborneJumpStartAsset(JumpStart));
	TestTrue(TEXT("A Jump Starts sequence belongs to the exclusive airborne reset contract"), AnimInstance->IsAirborneJumpAsset(JumpStart));
	JumpStart->bLoop = true;
	TestFalse(TEXT("A looping sequence is never treated as a jump start"), AnimInstance->IsAirborneJumpStartAsset(JumpStart));
	JumpStart->bLoop = false;

	UPackage* GroundPackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/RpgJumpRuntimeTest_Ground"));
	UAnimSequence* GroundStart = NewObject<UAnimSequence>(
		GroundPackage,
		MakeUniqueObjectName(GroundPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Ground")));
	GroundStart->bLoop = false;
	TestFalse(TEXT("An outgoing non-looping grounded pose is not a jump start"), AnimInstance->IsAirborneJumpStartAsset(GroundStart));
	TestTrue(TEXT("A Stand/Run sample keeps the moving per-sample contract"), AnimInstance->IsGroundMovingAsset(GroundStart));
	TestFalse(TEXT("A Stand/Run sample is not an airborne jump asset"), AnimInstance->IsAirborneJumpAsset(GroundStart));

	UPackage* GroundIdlePackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/RpgJumpRuntimeTest_Idle"));
	UAnimSequence* GroundIdle = NewObject<UAnimSequence>(
		GroundIdlePackage,
		MakeUniqueObjectName(GroundIdlePackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Idle")));
	TestFalse(TEXT("Stand/Idle and TIR assets remain outside moving procedural gates"), AnimInstance->IsGroundMovingAsset(GroundIdle));

	UPackage* FallPackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Jump/Airborne/RpgJumpRuntimeTest_Fall"));
	UAnimSequence* FallClip = NewObject<UAnimSequence>(
		FallPackage,
		MakeUniqueObjectName(FallPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Fall")));
	FallClip->bLoop = true;
	TestTrue(TEXT("The looping fall belongs to the exclusive airborne reset contract"), AnimInstance->IsAirborneJumpAsset(FallClip));
	TestFalse(TEXT("The looping fall never receives Jump Start moving corrections"), AnimInstance->IsAirborneJumpStartAsset(FallClip));

	UPackage* LandPackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/RpgJumpRuntimeTest_Land"));
	UAnimSequence* UnlatchedLandClip = NewObject<UAnimSequence>(
		LandPackage,
		MakeUniqueObjectName(LandPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Land")));
	TestFalse(TEXT("An unlatched landing is not an airborne database sample"), AnimInstance->IsAirborneJumpAsset(UnlatchedLandClip));

	UPackage* BackwardStartPackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_B_Start_Rfoot"));
	UAnimSequence* BackwardStart = NewObject<UAnimSequence>(
		BackwardStartPackage,
		MakeUniqueObjectName(BackwardStartPackage, UAnimSequence::StaticClass(), TEXT("M_Neutral_Jump_B_Start_Rfoot")));
	BackwardStart->bLoop = false;
	TestTrue(TEXT("Only a B Jump Start enters the bounded backward hold"), AnimInstance->IsBackwardJumpStartAsset(BackwardStart));
	TestFalse(TEXT("A lateral Jump Start never enters the backward hold"), AnimInstance->IsBackwardJumpStartAsset(JumpStart));
	TestTrue(TEXT("The looping Airborne clip is a continuing fall"), AnimInstance->IsLoopingAirborneFallAsset(FallClip));
	TestFalse(TEXT("A non-looping Jump Start is not a continuing fall"), AnimInstance->IsLoopingAirborneFallAsset(BackwardStart));
	TestTrue(
		TEXT("A descending fall continues after the bounded backward-start path"),
		AnimInstance->ShouldHoldLoopingAirborneFallPlayback(
			ERpgJumpPhase::Airborne, true, -100.0f, true));
	TestFalse(
		TEXT("A side or forward fall remains searchable when no backward hold was armed"),
		AnimInstance->ShouldHoldLoopingAirborneFallPlayback(
			ERpgJumpPhase::Airborne, false, -100.0f, true));
	TestFalse(
		TEXT("An upward relaunch releases a previously held fall loop"),
		AnimInstance->ShouldHoldLoopingAirborneFallPlayback(
			ERpgJumpPhase::Airborne, true, 100.0f, true));

	TestTrue(
		TEXT("A backward start remains continuing playback after its 0.565 second transition block"),
		AnimInstance->ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.67f, 1.97f, 1.0f, 0.67f));
	TestFalse(
		TEXT("The hold releases one authored blend interval before clip end"),
		AnimInstance->ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 1.78f, 1.97f, 1.0f, 1.0f));
	TestFalse(
		TEXT("The hold watchdog releases a genuine long fall"),
		AnimInstance->ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 1.0f, 1.97f, 1.0f, 1.25f));
	TestTrue(
		TEXT("The near-end release threshold is play-rate aware"),
		AnimInstance->ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 1.75f, 1.97f, 0.5f, 1.0f));
	TestFalse(
		TEXT("Grounded playback cannot retain an airborne start"),
		AnimInstance->ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Grounded, true, 0.67f, 1.97f, 1.0f, 0.67f));
	TestFalse(
		TEXT("An unexpected active asset fails open"),
		AnimInstance->ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, false, 0.67f, 1.97f, 1.0f, 0.67f));
	TestFalse(
		TEXT("Invalid playback timing fails open"),
		AnimInstance->ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.0f, 0.0f, 1.0f, 0.0f));

	AnimInstance->BeginAirbornePhase(true);
	TestFalse(
		TEXT("The outgoing grounded sample does not consume the initial airborne selection"),
		AnimInstance->UpdateBackwardJumpStartHold(GroundStart, 0.1f, 1.0f, 1.0f, 0.01f));
	TestFalse(
		TEXT("The outgoing sample leaves the first airborne result unresolved"),
		AnimInstance->bBackwardJumpStartHoldOpportunityConsumed);
	TestTrue(
		TEXT("The first backward result is held before its database can reselect"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	TestEqual(
		TEXT("The exact backward result owns the hold"),
		AnimInstance->BackwardJumpStartHeldAsset.Get(),
		static_cast<UAnimationAsset*>(BackwardStart));
	AnimInstance->BackwardJumpStartHoldElapsed = 1.24f;
	TestFalse(
		TEXT("The bounded watchdog releases the held start"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 1.0f, 1.97f, 1.0f, 0.02f));
	TestNull(TEXT("A released hold clears its asset"), AnimInstance->BackwardJumpStartHeldAsset.Get());
	TestFalse(
		TEXT("A later backward result cannot re-arm within the same jump"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	AnimInstance->BeginAirbornePhase(true);
	TestTrue(
		TEXT("A new airborne phase may hold a new backward result"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	AnimInstance->BeginAirbornePhase(false);
	TestFalse(
		TEXT("A descending ledge fall never arms the backward takeoff hold"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	AnimInstance->ResetJumpPhaseRuntime();

	const URpgAnimInstance::FGaspProceduralGates GroundGates =
		AnimInstance->ResolveGaspProceduralGates(true, 1.0f, false, false, false, 0.5f, true, true);
	TestEqual(TEXT("Ground movement enables Reset Root"), GroundGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("Ground Orientation Warping follows its authored curve"), GroundGates.OrientationWarpingAlpha, 0.5f);
	TestTrue(TEXT("Ground movement with an active asset and trajectory enables Steering"), GroundGates.bEnableSteering);
	const URpgAnimInstance::FGaspProceduralGates OutgoingGroundGates =
		AnimInstance->ResolveGaspProceduralGates(true, 1.0f, false, false, false, 0.5f, false, true);
	TestEqual(TEXT("An outgoing ground blend keeps Reset Root"), OutgoingGroundGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("An outgoing ground blend keeps its authored OW curve"), OutgoingGroundGates.OrientationWarpingAlpha, 0.5f);
	TestFalse(TEXT("Only the active Blend Stack branch receives Steering"), OutgoingGroundGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates StandJumpGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, true, false, 0.0f, true, true);
	TestEqual(TEXT("A stand jump start enables Reset Root without Enable_Warping"), StandJumpGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("A missing Enable_Warping curve keeps jump OW off"), StandJumpGates.OrientationWarpingAlpha, 0.0f);
	TestTrue(TEXT("A stand jump start still enables targeted Steering"), StandJumpGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates WarpedJumpGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, true, false, 0.75f, true, true);
	TestEqual(TEXT("A directional jump start curve gates OW independently"), WarpedJumpGates.OrientationWarpingAlpha, 0.75f);

	const URpgAnimInstance::FGaspProceduralGates FallGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, false, false, 1.0f, true, true);
	TestEqual(TEXT("The looped fall keeps Reset Root through airborne handoff"), FallGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("The looped fall cannot receive blanket OW"), FallGates.OrientationWarpingAlpha, 0.0f);
	TestFalse(TEXT("The looped fall cannot receive Steering"), FallGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates IdleLandingGates =
		AnimInstance->ResolveGaspProceduralGates(false, 0.0f, false, false, true, 1.0f, true, true);
	TestEqual(TEXT("The exactly latched Idle landing enables Reset Root"), IdleLandingGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("An Idle landing cannot inherit OW even if a curve is present"), IdleLandingGates.OrientationWarpingAlpha, 0.0f);
	TestFalse(TEXT("An Idle landing remains Reset Root-only without moving Steering"), IdleLandingGates.bEnableSteering);

	// The moving-pose input represents the frozen Walk/Run landing role, not current
	// post-touchdown speed, gait, or move intent. Its authored corrections remain active
	// for the exact latched request even after those live grounded values change.
	const URpgAnimInstance::FGaspProceduralGates MovingLandingGates =
		AnimInstance->ResolveGaspProceduralGates(true, 1.0f, false, false, true, 0.65f, true, true);
	TestEqual(TEXT("A latched Walk or Run landing keeps Reset Root"), MovingLandingGates.ResetRootAlpha, 1.0f);
	TestEqual(
		TEXT("A latched Walk or Run landing keeps its authored Enable_Warping curve"),
		MovingLandingGates.OrientationWarpingAlpha,
		0.65f);
	TestTrue(
		TEXT("A latched Walk or Run landing keeps Steering with an active asset and trajectory"),
		MovingLandingGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates MissingTrajectoryGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, true, false, 1.0f, true, false);
	TestEqual(TEXT("Missing trajectory does not erase the sample's Reset Root gate"), MissingTrajectoryGates.ResetRootAlpha, 1.0f);
	TestFalse(TEXT("Missing trajectory disables Steering"), MissingTrajectoryGates.bEnableSteering);
	TestTrue(
		TEXT("Ground-to-start-to-fall-to-ground keeps a continuous Reset Root contract"),
		FMath::IsNearlyEqual(GroundGates.ResetRootAlpha, StandJumpGates.ResetRootAlpha) &&
		FMath::IsNearlyEqual(StandJumpGates.ResetRootAlpha, FallGates.ResetRootAlpha) &&
		FMath::IsNearlyEqual(FallGates.ResetRootAlpha, OutgoingGroundGates.ResetRootAlpha));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
