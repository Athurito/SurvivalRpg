#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

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
	const float SampleTimes[] = { -0.2f, 0.0f, 0.2f, 0.4f, 0.8f };
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
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceStateMachineTest,
	"SurvivalRpg.Animation.TurnInPlace.StateMachine",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceStateMachineTest::RunTest(const FString& Parameters)
{
	USkeletalMeshComponent* AnimInstanceOuter = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(AnimInstanceOuter);
	UPoseSearchDatabase* TurnDatabase = NewObject<UPoseSearchDatabase>();
	if (!TestNotNull(TEXT("Transient RPG AnimInstance can be created"), AnimInstance) ||
		!TestNotNull(TEXT("Transient turn database can be created"), TurnDatabase))
	{
		return false;
	}
	AnimInstance->TurnInPlaceMotionMatchingDatabase = TurnDatabase;

	FRpgAnimInstanceProxy Proxy;
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.Gait = ERpgLocomotionGait::Idle;
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
	AnimInstance->bTurnInPlacePoseSelected = true;
	AnimInstance->TurnInPlaceSelectedAssetRemainingTime = 1.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(0.01f, Proxy);
	TestTrue(
		TEXT("Selected turn locks Offset Root increase while consuming authored rotation"),
		AnimInstance->OffsetRootRotationMode == EOffsetRootBoneMode::LockOffsetIncreaseAndConsumeAnimation);

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
	Proxy.bHasGroundedMoveIntent = false;
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
	AnimInstance->BeginTurnInPlaceRequest(90.0f);
	AnimInstance->TurnInPlaceAccumulatedYaw = 90.0f;
	AnimInstance->TurnInPlaceStateElapsed = 0.0f;
	AnimInstance->bTurnInPlacePoseSelected = true;
	AnimInstance->bTurnInPlaceHadSelection = true;
	AnimInstance->bTurnInPlaceSelectedAssetLooping = true;
	Proxy.ActorYawDelta = 0.0f;
	AnimInstance->UpdateTurnInPlaceRuntime(1.74f, Proxy);
	TestEqual(TEXT("A selected turn remains active before 1.75 seconds"), AnimInstance->TurnInPlaceState, ERpgTurnInPlaceState::Active);
	AnimInstance->bTurnInPlacePoseSelected = true;
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
	};

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
