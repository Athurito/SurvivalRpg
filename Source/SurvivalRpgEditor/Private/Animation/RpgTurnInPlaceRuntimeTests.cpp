#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSequence.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Components/SkeletalMeshComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceAngleAndTrajectoryTest,
	"SurvivalRpg.Animation.TurnInPlace.AngleAndTrajectory",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceAngleAndTrajectoryTest::RunTest(const FString& Parameters)
{
	struct FQuantizationCase
	{
		float Input;
		float Expected;
	};
	const FQuantizationCase QuantizationCases[] = {
		{ 29.99f, 0.0f },
		{ 30.0f, 45.0f },
		{ -30.0f, -45.0f },
		{ 67.49f, 45.0f },
		{ 67.5f, 90.0f },
		{ 112.49f, 90.0f },
		{ 112.5f, 135.0f },
		{ 157.49f, 135.0f },
		{ 157.5f, 180.0f },
		{ -200.0f, -180.0f },
	};
	for (const FQuantizationCase& TestCase : QuantizationCases)
	{
		TestTrue(
			FString::Printf(TEXT("%.2f degrees quantizes to %.0f"), TestCase.Input, TestCase.Expected),
			FMath::IsNearlyEqual(
				URpgAnimInstance::QuantizeTurnInPlaceAngle(TestCase.Input),
				TestCase.Expected));
	}

	TestTrue(
		TEXT("Positive yaw wrap is two degrees"),
		FMath::IsNearlyEqual(URpgAnimInstance::CalculateTurnInPlaceYawDelta(179.0f, -179.0f), 2.0f));
	TestTrue(
		TEXT("Negative yaw wrap is minus two degrees"),
		FMath::IsNearlyEqual(URpgAnimInstance::CalculateTurnInPlaceYawDelta(-179.0f, 179.0f), -2.0f));
	TestTrue(TEXT("45-degree duration is 0.45 seconds"), FMath::IsNearlyEqual(URpgAnimInstance::GetTurnInPlaceFacingDuration(45.0f), 0.45f));
	TestTrue(TEXT("90-degree duration is 0.65 seconds"), FMath::IsNearlyEqual(URpgAnimInstance::GetTurnInPlaceFacingDuration(90.0f), 0.65f));
	TestTrue(TEXT("135-degree duration is 0.85 seconds"), FMath::IsNearlyEqual(URpgAnimInstance::GetTurnInPlaceFacingDuration(135.0f), 0.85f));
	TestTrue(TEXT("180-degree duration is 1.0 second"), FMath::IsNearlyEqual(URpgAnimInstance::GetTurnInPlaceFacingDuration(180.0f), 1.0f));

	FTransformTrajectory SourceTrajectory;
	const FVector CurrentPosition(100.0f, 200.0f, 25.0f);
	const float SampleTimes[] = { -0.2f, 0.0f, 0.2f, 0.4f, 0.8f, 1.0f };
	for (const float SampleTime : SampleTimes)
	{
		FTransformTrajectorySample& Sample = SourceTrajectory.Samples.AddDefaulted_GetRef();
		Sample.TimeInSeconds = SampleTime;
		Sample.Position = CurrentPosition + FVector(SampleTime * 100.0f, 0.0f, 0.0f);
		Sample.Facing = FRotator(0.0f, 100.0f, 0.0f).Quaternion();
	}

	const FTransformTrajectory SyntheticTrajectory = URpgAnimInstance::MakeTurnInPlaceSyntheticTrajectory(
		SourceTrajectory,
		100.0f,
		90.0f,
		90.0f);
	TestEqual(TEXT("Synthetic trajectory preserves the exact sample count"), SyntheticTrajectory.Samples.Num(), SourceTrajectory.Samples.Num());

	float PreviousTime = -MAX_flt;
	float PreviousProgress = -MAX_flt;
	for (int32 SampleIndex = 0; SampleIndex < SyntheticTrajectory.Samples.Num(); ++SampleIndex)
	{
		const FTransformTrajectorySample& Sample = SyntheticTrajectory.Samples[SampleIndex];
		const FTransformTrajectorySample& SourceSample = SourceTrajectory.Samples[SampleIndex];
		const float FacingYaw = Sample.Facing.Rotator().Yaw;
		const float FacingProgress = FMath::FindDeltaAngleDegrees(10.0f, FacingYaw);
		TestTrue(TEXT("Synthetic sample times are strictly increasing"), Sample.TimeInSeconds > PreviousTime);
		TestTrue(TEXT("Synthetic sample time is unchanged"), FMath::IsNearlyEqual(Sample.TimeInSeconds, SourceSample.TimeInSeconds));
		TestTrue(TEXT("Synthetic sample position is unchanged"), Sample.Position.Equals(SourceSample.Position));
		TestTrue(TEXT("Synthetic facing never reverses"), FacingProgress + UE_KINDA_SMALL_NUMBER >= PreviousProgress);
		TestTrue(TEXT("Synthetic facing remains inside the requested turn"), FacingProgress >= -UE_KINDA_SMALL_NUMBER && FacingProgress <= 90.0f + UE_KINDA_SMALL_NUMBER);
		PreviousTime = Sample.TimeInSeconds;
		PreviousProgress = FacingProgress;
	}
	TestTrue(TEXT("Synthetic trajectory reaches the authored target facing"), FMath::IsNearlyEqual(PreviousProgress, 90.0f));

	const FTransformTrajectory NegativeSyntheticTrajectory = URpgAnimInstance::MakeTurnInPlaceSyntheticTrajectory(
		SourceTrajectory,
		-90.0f,
		-90.0f,
		-90.0f);
	float PreviousNegativeProgress = MAX_flt;
	for (const FTransformTrajectorySample& Sample : NegativeSyntheticTrajectory.Samples)
	{
		const float FacingProgress = FMath::FindDeltaAngleDegrees(0.0f, Sample.Facing.Rotator().Yaw);
		TestTrue(
			TEXT("Negative synthetic facing never reverses"),
			FacingProgress <= PreviousNegativeProgress + UE_KINDA_SMALL_NUMBER);
		TestTrue(
			TEXT("Negative synthetic facing remains inside the requested turn"),
			FacingProgress <= UE_KINDA_SMALL_NUMBER && FacingProgress >= -90.0f - UE_KINDA_SMALL_NUMBER);
		PreviousNegativeProgress = FacingProgress;
	}
	TestTrue(
		TEXT("Negative synthetic trajectory reaches the authored target facing"),
		FMath::IsNearlyEqual(PreviousNegativeProgress, -90.0f));

	const FTransformTrajectory DirectHalfTurnTrajectory = URpgAnimInstance::MakeTurnInPlaceSyntheticTrajectory(
		SourceTrajectory,
		180.0f,
		180.0f,
		180.0f);
	const float DirectHalfTurnProgressAtEightTenths = FMath::FindDeltaAngleDegrees(
		0.0f,
		DirectHalfTurnTrajectory.Samples[4].Facing.Rotator().Yaw);
	const float DirectHalfTurnFinalProgress = FMath::FindDeltaAngleDegrees(
		0.0f,
		DirectHalfTurnTrajectory.Samples.Last().Facing.Rotator().Yaw);
	TestTrue(
		TEXT("A direct 180-degree query advances monotonically before the quaternion half-turn boundary"),
		FMath::IsNearlyEqual(DirectHalfTurnProgressAtEightTenths, 144.0f, 0.1f));
	TestTrue(
		TEXT("A direct 180-degree query reaches the authored half-turn at one second"),
		FMath::IsNearlyEqual(FMath::Abs(DirectHalfTurnFinalProgress), 180.0f, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceStateMachineTest,
	"SurvivalRpg.Animation.TurnInPlace.StateMachine",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceStateMachineTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("The first owner snapshot cannot report a turn-in-place policy transition"),
		URpgAnimInstance::DidTurnInPlaceSupportChange(
			false,
			ERpgCharacterRotationMode::Free,
			ERpgCharacterRotationMode::CombatStrafe));
	TestTrue(
		TEXT("Free to CombatStrafe crosses the turn-in-place policy boundary"),
		URpgAnimInstance::DidTurnInPlaceSupportChange(
			true,
			ERpgCharacterRotationMode::Free,
			ERpgCharacterRotationMode::CombatStrafe));
	TestTrue(
		TEXT("Aim to Free crosses the turn-in-place policy boundary"),
		URpgAnimInstance::DidTurnInPlaceSupportChange(
			true,
			ERpgCharacterRotationMode::Aim,
			ERpgCharacterRotationMode::Free));
	TestFalse(
		TEXT("CombatStrafe to Aim preserves the shared controller-facing policy"),
		URpgAnimInstance::DidTurnInPlaceSupportChange(
			true,
			ERpgCharacterRotationMode::CombatStrafe,
			ERpgCharacterRotationMode::Aim));
	TestTrue(
		TEXT("A policy-boundary snapshot discards even a 180-degree actor-yaw jump"),
		FMath::IsNearlyZero(URpgAnimInstance::CalculateTurnInPlaceSnapshotYawDelta(
			0.0f,
			180.0f,
			false,
			true)));
	TestTrue(
		TEXT("An established controller-facing snapshot retains normal authored turn intent"),
		FMath::IsNearlyEqual(
			URpgAnimInstance::CalculateTurnInPlaceSnapshotYawDelta(0.0f, 90.0f, false, false),
			90.0f));

	USkeletalMeshComponent* AnimInstanceOuter = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(AnimInstanceOuter);
	UPoseSearchDatabase* TurnDatabase = NewObject<UPoseSearchDatabase>();
	UAnimSequence* SelectedTurn = NewObject<UAnimSequence>();
	UAnimSequence* PreviousIdle = NewObject<UAnimSequence>();
	if (!TestNotNull(TEXT("Transient RPG AnimInstance can be created"), AnimInstance) ||
		!TestNotNull(TEXT("Transient turn database can be created"), TurnDatabase) ||
		!TestNotNull(TEXT("Transient selected turn can be created"), SelectedTurn) ||
		!TestNotNull(TEXT("Transient previous idle can be created"), PreviousIdle))
	{
		return false;
	}
	AnimInstance->TurnInPlaceMotionMatchingDatabase = TurnDatabase;

	FRpgAnimInstanceProxy Proxy;
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.Gait = ERpgLocomotionGait::Idle;
	Proxy.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	Proxy.bIsMovingOnGround = true;
	Proxy.GroundSpeed = 0.0f;
	Proxy.bHasAcceleration = false;
	Proxy.bHasGroundedMoveIntent = false;
	Proxy.bTurnInPlaceHardReset = false;
	Proxy.ActorYaw = 0.0f;
	Proxy.ActorLocation = FVector::ZeroVector;
	FTransformTrajectorySample& CurrentTrajectorySample = Proxy.TransformTrajectory.Samples.AddDefaulted_GetRef();
	CurrentTrajectorySample.TimeInSeconds = 0.0f;
	CurrentTrajectorySample.Position = FVector::ZeroVector;
	CurrentTrajectorySample.Facing = FQuat::Identity;

	Proxy.RotationMode = ERpgCharacterRotationMode::Free;
	TestFalse(TEXT("Free rotation mode disables turn-in-place eligibility"), AnimInstance->IsTurnInPlaceEligible(Proxy));
	Proxy.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	TestTrue(TEXT("Combat strafe rotation mode allows turn-in-place eligibility"), AnimInstance->IsTurnInPlaceEligible(Proxy));
	Proxy.RotationMode = ERpgCharacterRotationMode::Aim;
	TestTrue(TEXT("Aim rotation mode allows turn-in-place eligibility"), AnimInstance->IsTurnInPlaceEligible(Proxy));
	Proxy.RotationMode = ERpgCharacterRotationMode::CombatStrafe;

	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.ActorYawDelta = 20.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("20 degrees enters collection without selecting a turn"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Collecting);
	TestTrue(TEXT("Collecting accumulates Offset Root rotation"), AnimInstance->OffsetRootRotationMode == EOffsetRootBoneMode::Accumulate);

	Proxy.ActorYawDelta = -10.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Cancel hysteresis keeps exactly ten degrees collecting"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Collecting);
	Proxy.ActorYawDelta = -0.1f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Dropping below ten degrees enters recovery"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);

	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.ActorYawDelta = 60.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	Proxy.ActorYawDelta = 0.0f;
	for (int32 StableFrame = 0; StableFrame < 9; ++StableFrame)
	{
		AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	}
	TestEqual(TEXT("Sixty stable degrees activates a turn after 80 ms"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Active);
	TestTrue(TEXT("Sixty degrees selects the authored 45-degree query"), FMath::IsNearlyEqual(AnimInstance->TurnInPlaceQueryAngle, 45.0f));
	TestEqual(TEXT("First active turn creates exactly one request serial"), AnimInstance->TurnInPlaceRequestSerial, 1u);
	TestTrue(
		TEXT("Active turn keeps accumulating Offset Root until a pose is selected"),
		AnimInstance->OffsetRootRotationMode == EOffsetRootBoneMode::Accumulate);

	Proxy.ActorYawDelta = 8.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestTrue(TEXT("A nearby quantization boundary does not immediately retarget"), FMath::IsNearlyEqual(AnimInstance->TurnInPlaceQueryAngle, 45.0f));
	TestEqual(TEXT("Boundary crossing below thirty additional degrees keeps one request"), AnimInstance->TurnInPlaceRequestSerial, 1u);
	Proxy.ActorYawDelta = 22.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestTrue(TEXT("Thirty additional degrees retargets to the changed bucket"), FMath::IsNearlyEqual(AnimInstance->TurnInPlaceQueryAngle, 90.0f));
	TestEqual(TEXT("Material retarget creates one new request"), AnimInstance->TurnInPlaceRequestSerial, 2u);

	Proxy.ActorYawDelta = 0.0f;
	const uint32 SelectedRequestSerial = AnimInstance->TurnInPlaceRequestSerial;
	const bool bForceSelectedRequest = AnimInstance->ConsumeTurnInPlaceForceInterruptRequest();
	TestTrue(TEXT("A request issues its TIR ForceInterrupt exactly once"), bForceSelectedRequest);
	TestTrue(
		TEXT("The one forced update searches the exclusive TIR database"),
		AnimInstance->ResolveTurnInPlaceSearchMode(bForceSelectedRequest) ==
			URpgAnimInstance::ETurnInPlaceSearchMode::SearchRequestedTurn);
	TestFalse(
		TEXT("The same request cannot issue a second TIR ForceInterrupt"),
		AnimInstance->ConsumeTurnInPlaceForceInterruptRequest());
	Proxy.ActorYawDelta = 50.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(
		TEXT("A dispatched pre-update search locks its request serial until selection or timeout"),
		AnimInstance->TurnInPlaceRequestSerial,
		SelectedRequestSerial);
	TestTrue(
		TEXT("A dispatched pre-update search cannot start a second turn clip before its result is latched"),
		FMath::IsNearlyEqual(AnimInstance->TurnInPlaceQueryAngle, 90.0f));
	TestFalse(
		TEXT("The dispatched request is no longer eligible for pre-search retargeting"),
		AnimInstance->CanRetargetTurnInPlaceRequest());
	Proxy.ActorYawDelta = 0.0f;
	TestTrue(
		TEXT("An unsuccessful one-shot request does not reopen the TIR database"),
		AnimInstance->ResolveTurnInPlaceSearchMode(false) ==
			URpgAnimInstance::ETurnInPlaceSearchMode::NormalLocomotion);
	TestFalse(
		TEXT("A stale request serial cannot latch the current SearchResult"),
		AnimInstance->TryLatchTurnInPlaceSelection(
			SelectedTurn,
			TurnDatabase,
			0.25f,
			false,
			SelectedRequestSerial - 1));
	TestTrue(
		TEXT("The first valid SearchResult latches SelectedAnim, SelectedTime, loop state, and request serial"),
		AnimInstance->TryLatchTurnInPlaceSelection(
			SelectedTurn,
			TurnDatabase,
			0.25f,
			false,
			SelectedRequestSerial));
	TestEqual(TEXT("The selected asset is latched"), AnimInstance->TurnInPlaceSelectedAsset.Get(), static_cast<UAnimationAsset*>(SelectedTurn));
	TestEqual(TEXT("The selected start time is latched"), AnimInstance->TurnInPlaceSelectedAssetStartTime, 0.25f);
	TestEqual(TEXT("The selected request serial is latched"), AnimInstance->TurnInPlaceSelectedRequestSerial, SelectedRequestSerial);
	TestFalse(TEXT("The SearchResult loop state is latched without reading the previous Blend Stack asset"), AnimInstance->bTurnInPlaceSelectedAssetLooping);
	TestFalse(
		TEXT("A request serial cannot replace its first latched selection"),
		AnimInstance->TryLatchTurnInPlaceSelection(
			PreviousIdle,
			TurnDatabase,
			0.0f,
			true,
			SelectedRequestSerial));
	TestEqual(TEXT("The first selected asset remains authoritative"), AnimInstance->TurnInPlaceSelectedAsset.Get(), static_cast<UAnimationAsset*>(SelectedTurn));
	AnimInstance->UpdateTurnInPlaceLatchedPlayback(PreviousIdle, 0.5f, 1.0f, 1.0f, 0.01f);
	TestFalse(TEXT("A transient pre-update Blend Stack mismatch is not mistaken for observed playback"), AnimInstance->bTurnInPlacePlaybackObserved);
	TestTrue(TEXT("The previous completed SearchResult remains latched through a transient mismatch"), AnimInstance->bTurnInPlacePoseSelected);
	TestTrue(
		TEXT("A latched selection closes full TIR search and keeps only its Continuing Pose"),
		AnimInstance->ResolveTurnInPlaceSearchMode(false) ==
			URpgAnimInstance::ETurnInPlaceSearchMode::ContinueSelectedTurn);
	AnimInstance->UpdateTurnInPlaceLatchedPlayback(SelectedTurn, 0.25f, 1.25f, 1.0f, 0.01f);
	TestTrue(TEXT("The Blend Stack eventually observes the exact latched asset"), AnimInstance->bTurnInPlacePlaybackObserved);
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestTrue(
		TEXT("Selected turn locks Offset Root increase while consuming authored rotation"),
		AnimInstance->OffsetRootRotationMode == EOffsetRootBoneMode::LockOffsetIncreaseAndConsumeAnimation);
	Proxy.ActorYawDelta = 50.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Continuous mouse input cannot create a mid-clip request"), AnimInstance->TurnInPlaceRequestSerial, SelectedRequestSerial);
	TestTrue(TEXT("Continuous mouse input keeps the latched authored bucket until completion"), FMath::IsNearlyEqual(AnimInstance->TurnInPlaceQueryAngle, 90.0f));
	Proxy.ActorYawDelta = 0.0f;
	AnimInstance->UpdateTurnInPlaceLatchedPlayback(SelectedTurn, 0.7f, 1.0f, 1.0f, 0.01f);
	TestFalse(TEXT("The full asset remains unfinished when the database's default indexed range has ended"), AnimInstance->bTurnInPlaceCompletionArmed);
	TestTrue(
		TEXT("An invalid SearchResult at the indexed-range end cannot reopen the TIR database"),
		AnimInstance->ResolveTurnInPlaceSearchMode(false) ==
			URpgAnimInstance::ETurnInPlaceSearchMode::ContinueSelectedTurn);
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Indexed-range exhaustion does not end the latched Blend Stack playback"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Active);
	TestFalse(TEXT("Indexed-range exhaustion does not hard-reset Offset Root"), AnimInstance->bResetOffsetRootEveryFrame);

	AnimInstance->UpdateTurnInPlaceLatchedPlayback(SelectedTurn, 0.95f, 1.0f, 1.0f, 0.01f);
	TestTrue(TEXT("Natural full-asset completion is armed one update ahead"), AnimInstance->bTurnInPlaceCompletionArmed);
	TestTrue(
		TEXT("Natural completion selects normal locomotion for the upcoming pre-update search"),
		AnimInstance->ResolveTurnInPlaceSearchMode(false) ==
			URpgAnimInstance::ETurnInPlaceSearchMode::NormalLocomotion);
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Natural completion enters recovery"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	TestFalse(TEXT("Natural completion never hard-resets Offset Root"), AnimInstance->bResetOffsetRootEveryFrame);

	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.ActorYawDelta = 30.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	Proxy.ActorYawDelta = 0.0f;
	for (int32 StableFrame = 0; StableFrame < 9; ++StableFrame)
	{
		AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	}
	bool bSelectionTimeoutPulsedReset = false;
	for (int32 SelectionFrame = 0;
		SelectionFrame < 30 && AnimInstance->TurnInPlaceState == ERpgTurnInPlaceState::Active;
		++SelectionFrame)
	{
		AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
		bSelectionTimeoutPulsedReset |= AnimInstance->bResetOffsetRootEveryFrame;
	}
	TestEqual(TEXT("Missing turn selection enters recovery after 250 ms"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	TestTrue(TEXT("Selection timeout emits a one-frame hard-reset pulse"), bSelectionTimeoutPulsedReset);

	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.ActorYawDelta = 30.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	Proxy.ActorYawDelta = 0.0f;
	for (int32 StableFrame = 0; StableFrame < 9; ++StableFrame)
	{
		AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	}
	Proxy.bIsAnyMontagePlaying = true;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Montage state hard-resets turn-in-place"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);
	TestTrue(TEXT("First montage frame resets Offset Root"), AnimInstance->bResetOffsetRootEveryFrame);
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestFalse(TEXT("Persistent montage does not repeat the reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);

	Proxy.bIsAnyMontagePlaying = false;
	Proxy.bHasGroundedMoveIntent = true;
	Proxy.ActorYawDelta = 45.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Grounded movement intent blocks turn collection"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);

	AnimInstance->ResetTurnInPlaceRuntime(false);
	AnimInstance->BeginTurnInPlaceRequest(45.0f);
	AnimInstance->TurnInPlaceAccumulatedYaw = 45.0f;
	Proxy.GroundSpeed = 300.0f;
	Proxy.Gait = ERpgLocomotionGait::Run;
	Proxy.ActorYawDelta = 0.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Movement before TIR selection exits immediately to recovery"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	TestTrue(
		TEXT("Preselection movement exit chooses the normal gait database policy"),
		AnimInstance->ResolveTurnInPlaceSearchMode(false) ==
			URpgAnimInstance::ETurnInPlaceSearchMode::NormalLocomotion);
	TestTrue(TEXT("Moving procedural nodes are allowed during movement recovery"), AnimInstance->AllowsMovingProceduralNodes());
	TestFalse(TEXT("Preselection movement exit does not hard-reset Offset Root"), AnimInstance->bResetOffsetRootEveryFrame);

	AnimInstance->ResetTurnInPlaceRuntime(false);
	AnimInstance->BeginTurnInPlaceRequest(45.0f);
	AnimInstance->TurnInPlaceAccumulatedYaw = 45.0f;
	const uint32 MidClipMovementSerial = AnimInstance->TurnInPlaceRequestSerial;
	TestTrue(
		TEXT("The mid-clip movement fixture latches a turn"),
		AnimInstance->TryLatchTurnInPlaceSelection(
			SelectedTurn,
			TurnDatabase,
			0.0f,
			false,
			MidClipMovementSerial));
	AnimInstance->UpdateTurnInPlaceLatchedPlayback(SelectedTurn, 0.25f, 1.0f, 1.0f, 0.01f);
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Mid-clip movement exits immediately to recovery"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	TestTrue(
		TEXT("Mid-clip movement exit releases the selection lock to the gait database"),
		AnimInstance->ResolveTurnInPlaceSearchMode(false) ==
			URpgAnimInstance::ETurnInPlaceSearchMode::NormalLocomotion);
	TestTrue(TEXT("Mid-clip movement recovery keeps moving procedural nodes enabled"), AnimInstance->AllowsMovingProceduralNodes());
	TestFalse(TEXT("Mid-clip movement exit does not hard-reset Offset Root"), AnimInstance->bResetOffsetRootEveryFrame);

	Proxy.bHasGroundedMoveIntent = false;
	Proxy.GroundSpeed = 0.0f;
	Proxy.Gait = ERpgLocomotionGait::Idle;
	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.TransformTrajectory.Samples.Reset();
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Missing trajectory blocks turn collection"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);

	FTransformTrajectorySample& RestoredTrajectorySample = Proxy.TransformTrajectory.Samples.AddDefaulted_GetRef();
	RestoredTrajectorySample.TimeInSeconds = 0.0f;
	RestoredTrajectorySample.Position = FVector::ZeroVector;
	RestoredTrajectorySample.Facing = FQuat::Identity;
	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.ActorYawDelta = 20.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	Proxy.ActorYawDelta = 0.0f;
	for (int32 CollectionFrame = 0; CollectionFrame < 9; ++CollectionFrame)
	{
		AnimInstance->UpdateTurnInPlaceRuntime(0.02f, Proxy);
	}
	TestEqual(TEXT("Sub-threshold collection remains active before 200 ms"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Collecting);
	AnimInstance->UpdateTurnInPlaceRuntime(0.021f, Proxy);
	TestEqual(TEXT("Sub-threshold collection recovers at 200 ms"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	AnimInstance->UpdateTurnInPlaceRuntime(0.14f, Proxy);
	TestEqual(TEXT("Recovery remains active before 150 ms"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	AnimInstance->UpdateTurnInPlaceRuntime(0.02f, Proxy);
	TestEqual(TEXT("Recovery completes after 150 ms"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);

	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.ActorYaw = 180.0f;
	Proxy.ActorYawDelta = 180.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(
		TEXT("A direct 180-degree input first enters the bounded collection phase"),
		AnimInstance->TurnInPlaceState,
		ERpgTurnInPlaceState::Collecting);
	TestTrue(
		TEXT("A direct half-turn accumulates Offset Root while collection stabilizes"),
		AnimInstance->OffsetRootRotationMode == EOffsetRootBoneMode::Accumulate);
	Proxy.ActorYawDelta = 0.0f;
	for (int32 StableFrame = 0; StableFrame < 9; ++StableFrame)
	{
		AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	}
	TestEqual(
		TEXT("A stable direct half-turn activates after the required 80 ms"),
		AnimInstance->TurnInPlaceState,
		ERpgTurnInPlaceState::Active);
	TestTrue(
		TEXT("A direct half-turn keeps the exclusive authored 180-degree query"),
		FMath::IsNearlyEqual(AnimInstance->TurnInPlaceQueryAngle, 180.0f));
	AnimInstance->TurnInPlaceStateElapsed = 1.5f;
	TestTrue(
		TEXT("The long non-looping turn fixture latches successfully"),
		AnimInstance->TryLatchTurnInPlaceSelection(
			SelectedTurn,
			TurnDatabase,
			0.0f,
			false,
			AnimInstance->TurnInPlaceRequestSerial));
	TestTrue(
		TEXT("Latching restarts the active watchdog clock"),
		FMath::IsNearlyZero(AnimInstance->TurnInPlaceStateElapsed));
	constexpr float LongTurnAssetLength = 2.1667f;
	AnimInstance->TurnInPlaceStateElapsed = 0.5f;
	AnimInstance->UpdateTurnInPlaceLatchedPlayback(
		SelectedTurn,
		0.0f,
		LongTurnAssetLength,
		0.85f,
		0.05f);
	TestTrue(
		TEXT("First Blend Stack observation restarts the playback watchdog clock"),
		FMath::IsNearlyZero(AnimInstance->TurnInPlaceStateElapsed));
	TestTrue(
		TEXT("The playback watchdog converts the 0.85x long-clip remainder to wall-clock time"),
		AnimInstance->TurnInPlacePlaybackWatchdogDuration > LongTurnAssetLength / 0.85f);
	Proxy.ActorYawDelta = 0.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(2.3f, Proxy);
	TestEqual(
		TEXT("A valid 2.1667-second turn at 0.85x remains active beyond the old raw-seconds watchdog"),
		AnimInstance->TurnInPlaceState,
		ERpgTurnInPlaceState::Active);
	TestFalse(TEXT("A long valid turn does not hard-reset before natural completion"), AnimInstance->bResetOffsetRootEveryFrame);
	AnimInstance->UpdateTurnInPlaceLatchedPlayback(
		SelectedTurn,
		LongTurnAssetLength - 0.05f,
		LongTurnAssetLength,
		0.85f,
		0.05f);
	AnimInstance->UpdateTurnInPlaceRuntime(0.05f, Proxy);
	TestEqual(TEXT("The long turn naturally recovers at full-asset completion"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	TestFalse(TEXT("Long-turn natural completion remains a soft recovery"), AnimInstance->bResetOffsetRootEveryFrame);

	AnimInstance->ResetTurnInPlaceRuntime(false);
	AnimInstance->BeginTurnInPlaceRequest(90.0f);
	AnimInstance->TurnInPlaceAccumulatedYaw = 90.0f;
	AnimInstance->TurnInPlaceStateElapsed = 0.0f;
	TestTrue(
		TEXT("A looping safety-timeout fixture latches successfully"),
		AnimInstance->TryLatchTurnInPlaceSelection(
			SelectedTurn,
			TurnDatabase,
			0.0f,
			true,
			AnimInstance->TurnInPlaceRequestSerial));
	AnimInstance->UpdateTurnInPlaceLatchedPlayback(SelectedTurn, 0.1f, 1.0f, 0.85f, 0.01f);
	TestTrue(
		TEXT("A looping turn keeps the fixed stuck-playback watchdog despite a slow play rate"),
		FMath::IsNearlyEqual(AnimInstance->TurnInPlacePlaybackWatchdogDuration, 1.75f));
	Proxy.ActorYawDelta = 0.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(1.74f, Proxy);
	TestEqual(TEXT("A selected turn remains active before 1.75 seconds"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Active);
	AnimInstance->UpdateTurnInPlaceRuntime(0.02f, Proxy);
	TestEqual(TEXT("A selected turn hard-recovers after 1.75 seconds"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Recovering);
	TestTrue(TEXT("Active timeout emits an Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);

	auto ActivateTurnForReset = [&]()
	{
		AnimInstance->bTurnInPlaceHardResetConditionLastFrame = false;
		AnimInstance->ResetTurnInPlaceRuntime(false);
		AnimInstance->BeginTurnInPlaceRequest(45.0f);
		AnimInstance->TurnInPlaceAccumulatedYaw = 45.0f;
		Proxy.ActorYawDelta = 0.0f;
		Proxy.bTurnInPlaceHardReset = false;
		Proxy.bHasTurnInPlaceBlockingGameplayTag = false;
		Proxy.bIsAnyMontagePlaying = false;
		Proxy.bIsCrouching = false;
		Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
		Proxy.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	};

	ActivateTurnForReset();
	Proxy.RotationMode = ERpgCharacterRotationMode::Free;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Switching an active turn to Free hard-resets turn-in-place"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);
	TestTrue(TEXT("The first Active-to-Free frame emits an Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestFalse(TEXT("Persistent Free mode does not repeat the Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);

	Proxy.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	Proxy.bTurnInPlaceSupportChanged = true;
	Proxy.ActorYawDelta = 180.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(
		TEXT("Free-to-combat policy entry recovers instead of selecting a turn clip"),
		AnimInstance->TurnInPlaceState,
		ERpgTurnInPlaceState::Recovering);
	TestTrue(TEXT("Rotation-mode entry hard-resets stale Offset Root state"), AnimInstance->bResetOffsetRootEveryFrame);
	const float ModeEntryAccumulatedYaw = AnimInstance->TurnInPlaceAccumulatedYaw;
	Proxy.bTurnInPlaceSupportChanged = false;
	Proxy.ActorYawDelta = 90.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.05f, Proxy);
	TestFalse(TEXT("The Offset Root reset pulse clears on the first recovery update"), AnimInstance->bResetOffsetRootEveryFrame);
	AnimInstance->UpdateTurnInPlaceRuntime(0.05f, Proxy);
	TestEqual(
		TEXT("Mode-entry recovery remains active after two 20-FPS updates"),
		AnimInstance->TurnInPlaceState,
		ERpgTurnInPlaceState::Recovering);
	TestTrue(
		TEXT("Mode-entry recovery never accumulates network-smoothed yaw"),
		FMath::IsNearlyEqual(AnimInstance->TurnInPlaceAccumulatedYaw, ModeEntryAccumulatedYaw));
	AnimInstance->UpdateTurnInPlaceRuntime(0.05f, Proxy);
	TestEqual(
		TEXT("Mode-entry recovery returns to normal combat idle on the third 20-FPS update"),
		AnimInstance->TurnInPlaceState,
		ERpgTurnInPlaceState::Inactive);
	TestTrue(
		TEXT("Mode-entry recovery clears discarded yaw before normal TIR resumes"),
		FMath::IsNearlyZero(AnimInstance->TurnInPlaceAccumulatedYaw));
	Proxy.ActorYawDelta = 20.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(
		TEXT("Normal combat turn-in-place collection resumes after the bounded mode-entry recovery"),
		AnimInstance->TurnInPlaceState,
		ERpgTurnInPlaceState::Collecting);

	ActivateTurnForReset();
	Proxy.bIsCrouching = true;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Crouching hard-resets an active turn"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);
	TestTrue(TEXT("Crouching emits an Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);

	ActivateTurnForReset();
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("Airborne movement hard-resets an active turn"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);
	TestTrue(TEXT("Airborne movement emits an Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);

	ActivateTurnForReset();
	Proxy.bHasTurnInPlaceBlockingGameplayTag = true;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("A mapped blocking gameplay tag hard-resets an active turn"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);
	TestTrue(TEXT("A mapped blocking gameplay tag emits an Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);

	ActivateTurnForReset();
	Proxy.bTurnInPlaceHardReset = true;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestEqual(TEXT("A proxy owner, role, teleport, or position reset clears an active turn"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Inactive);
	TestTrue(TEXT("A proxy hard reset emits an Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);

	ActivateTurnForReset();
	TestTrue(TEXT("A new turn request consumes one ForceInterrupt"), AnimInstance->ConsumeTurnInPlaceForceInterruptRequest());
	TestFalse(TEXT("The same turn request cannot consume ForceInterrupt twice"), AnimInstance->ConsumeTurnInPlaceForceInterruptRequest());
	AnimInstance->BeginTurnInPlaceRequest(-90.0f);
	TestTrue(TEXT("A material retarget consumes one new ForceInterrupt"), AnimInstance->ConsumeTurnInPlaceForceInterruptRequest());
	TestFalse(TEXT("The retargeted request also interrupts only once"), AnimInstance->ConsumeTurnInPlaceForceInterruptRequest());

	AnimInstance->bTurnInPlaceInitializationResetPending = true;
	AnimInstance->ResetTurnInPlaceRuntime(false);
	Proxy.bTurnInPlaceHardReset = false;
	Proxy.ActorYawDelta = 0.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestTrue(TEXT("Animation reinitialization preserves one Offset Root reset pulse"), AnimInstance->bResetOffsetRootEveryFrame);
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestFalse(TEXT("Animation reinitialization resets Offset Root for only one frame"), AnimInstance->bResetOffsetRootEveryFrame);

	const float FrameRates[] = { 20.0f, 60.0f, 120.0f };
	for (const float FrameRate : FrameRates)
	{
		USkeletalMeshComponent* FrameRateAnimInstanceOuter = NewObject<USkeletalMeshComponent>();
		URpgAnimInstance* FrameRateAnimInstance = NewObject<URpgAnimInstance>(FrameRateAnimInstanceOuter);
		FrameRateAnimInstance->TurnInPlaceMotionMatchingDatabase = TurnDatabase;
		FRpgAnimInstanceProxy FrameRateProxy;
		FrameRateProxy.MovementState = ERpgLocomotionMovementState::Grounded;
		FrameRateProxy.Gait = ERpgLocomotionGait::Idle;
		FrameRateProxy.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
		FrameRateProxy.bIsMovingOnGround = true;
		FrameRateProxy.GroundSpeed = 0.0f;
		FrameRateProxy.bTurnInPlaceHardReset = false;
		FTransformTrajectorySample& FrameRateSample = FrameRateProxy.TransformTrajectory.Samples.AddDefaulted_GetRef();
		FrameRateSample.TimeInSeconds = 0.0f;
		FrameRateSample.Facing = FQuat::Identity;

		const float FrameDelta = 1.0f / FrameRate;
		FrameRateProxy.ActorYawDelta = 30.0f;
		FrameRateAnimInstance->UpdateTurnInPlaceRuntime(FrameDelta, FrameRateProxy);
		const int32 MovingFrames = FMath::CeilToInt(0.1f / FrameDelta);
		FrameRateProxy.ActorYawDelta = 61.0f * FrameDelta;
		for (int32 Frame = 0; Frame < MovingFrames; ++Frame)
		{
			FrameRateAnimInstance->UpdateTurnInPlaceRuntime(FrameDelta, FrameRateProxy);
		}
		TestEqual(
			FString::Printf(TEXT("61 deg/s remains unstable at %.0f FPS"), FrameRate),
			FrameRateAnimInstance->TurnInPlaceState,
			ERpgTurnInPlaceState::Collecting);

		FrameRateProxy.ActorYawDelta = 0.0f;
		const int32 StableFrames = FMath::CeilToInt(0.08f / FrameDelta);
		for (int32 Frame = 0; Frame < StableFrames; ++Frame)
		{
			FrameRateAnimInstance->UpdateTurnInPlaceRuntime(FrameDelta, FrameRateProxy);
		}
		TestEqual(
			FString::Printf(TEXT("80 ms of stability activates at %.0f FPS"), FrameRate),
			FrameRateAnimInstance->TurnInPlaceState,
			ERpgTurnInPlaceState::Active);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
