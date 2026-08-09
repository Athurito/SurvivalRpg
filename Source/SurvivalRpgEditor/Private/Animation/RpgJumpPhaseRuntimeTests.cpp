// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

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
	UPoseSearchDatabase* LandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* OtherDatabase = NewObject<UPoseSearchDatabase>();
	UAnimSequence* LandingClip = NewObject<UAnimSequence>();
	UAnimSequence* PreviousAirborneClip = NewObject<UAnimSequence>();
	if (!TestNotNull(TEXT("Transient RPG AnimInstance can be created"), AnimInstance) ||
		!TestNotNull(TEXT("Transient landing database can be created"), LandingDatabase) ||
		!TestNotNull(TEXT("Transient landing clip can be created"), LandingClip))
	{
		return false;
	}
	AnimInstance->LandingMotionMatchingDatabase = LandingDatabase;

	FRpgAnimInstanceProxy Proxy;
	Proxy.bTurnInPlaceHardReset = false;
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
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
	Proxy.bHasGroundedMoveIntent = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A high-speed touchdown returns directly to gait locomotion"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);
	TestEqual(TEXT("A high-speed touchdown creates no stand-idle landing request"), AnimInstance->LandingRequestSerial, 0u);

	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	Proxy.GroundSpeed = 0.0f;
	Proxy.bHasGroundedMoveIntent = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Grounded move intent bypasses the stand-idle landing latch"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);
	TestEqual(TEXT("Grounded move intent creates no landing request"), AnimInstance->LandingRequestSerial, 0u);

	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	Proxy.bHasGroundedMoveIntent = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	Proxy.GroundSpeed = 3.0f;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("An idle touchdown enters Landing at the inclusive 3 cm/s boundary"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(TEXT("The first idle touchdown creates one landing request"), AnimInstance->LandingRequestSerial, 1u);
	TestTrue(TEXT("A touchdown requests ForceInterrupt exactly once"), AnimInstance->ConsumeLandingForceInterruptRequest());
	TestFalse(TEXT("The same touchdown cannot request ForceInterrupt twice"), AnimInstance->ConsumeLandingForceInterruptRequest());

	TestFalse(
		TEXT("A result from another database cannot latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			OtherDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial));
	TestFalse(
		TEXT("A stale request serial cannot latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			LandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial - 1));
	TestTrue(
		TEXT("The first valid exclusive landing result latches"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			LandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial));
	TestTrue(TEXT("The exact landing asset is active after latching"), AnimInstance->IsActiveLandingAsset(LandingClip));
	TestFalse(
		TEXT("A second result cannot replace the latched landing"),
		AnimInstance->TryLatchLandingSelection(
			PreviousAirborneClip,
			LandingDatabase,
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
			LandingDatabase,
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
		Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
		Proxy.bIsMovingOnGround = true;
		Proxy.bIsFalling = false;
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

	const URpgAnimInstance::FGaspProceduralGates LandingGates =
		AnimInstance->ResolveGaspProceduralGates(false, 0.0f, false, false, true, 1.0f, true, true);
	TestEqual(TEXT("The exactly latched landing enables Reset Root"), LandingGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("A landing cannot inherit OW even if a curve is present"), LandingGates.OrientationWarpingAlpha, 0.0f);
	TestFalse(TEXT("The light landing never receives moving Steering"), LandingGates.bEnableSteering);

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
