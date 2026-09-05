// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgTurnInPlaceRuntime.h"

#include <limits>

namespace
{
/** Evaluated facing is supplied explicitly: these tests exercise policy, not a rendered MM winner. */
struct FRootFeedbackTurnFixture
{
	FRpgTurnInPlaceRuntimeState State;
	FRpgTurnInPlaceUpdateSnapshot Snapshot;
	float DeltaSeconds;

	explicit FRootFeedbackTurnFixture(int32 FramesPerSecond)
		: DeltaSeconds(1.0f / FramesPerSecond)
	{
		Snapshot.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
		Snapshot.MovementState = ERpgLocomotionMovementState::Grounded;
		Snapshot.bEligible = true;
		Snapshot.bHasRootYawGap = true;
	}

	FRpgTurnInPlaceUpdateResult Advance(float ActorYaw, float EvaluatedRootYaw, bool bCompleted = false)
	{
		Snapshot.ActorYawDelta = RpgTurnInPlaceRuntime::CalculateYawDelta(Snapshot.ActorYaw, ActorYaw);
		Snapshot.ActorYaw = ActorYaw;
		Snapshot.RootYawGap = RpgTurnInPlaceRuntime::CalculateYawDelta(EvaluatedRootYaw, ActorYaw);
		Snapshot.bCompletionArmed = bCompleted;
		FRpgTurnInPlaceUpdateResult Result = RpgTurnInPlaceRuntime::Update(State, Snapshot, DeltaSeconds);
		State = Result.State;
		if (Result.bClearSelection)
		{
			Snapshot.bSelectionLatched = false;
			Snapshot.bPlaybackObserved = false;
			Snapshot.bPoseSelected = false;
			Snapshot.bCompletionArmed = false;
		}
		return Result;
	}

	void Start(float ActorYaw = 90.0f, float EvaluatedRootYaw = 0.0f)
	{
		for (int32 Frame = 0; Frame < 120 && State.State != ERpgTurnInPlaceState::Active; ++Frame)
		{
			Advance(ActorYaw, EvaluatedRootYaw);
		}
	}

	void Latch()
	{
		RpgTurnInPlaceRuntime::ConsumeForceInterrupt(true, State);
		Snapshot.bSelectionLatched = true;
		Snapshot.bPlaybackObserved = true;
		Snapshot.bPoseSelected = true;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceRootGapBasisTest,
	"SurvivalRpg.Animation.TurnInPlace.RootFeedback.BasisAndWrapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceRootGapBasisTest::RunTest(const FString& Parameters)
{
	const FQuat Bases[] = {
		FQuat::Identity, FRotator(0.0f, -90.0f, 0.0f).Quaternion(),
		FRotator(23.0f, 47.0f, -18.0f).Quaternion()};
	struct FGapCase { float RootYaw; float ActorYaw; float Expected; };
	const FGapCase Cases[] = {
		{0.0f, 90.0f, 90.0f}, {75.0f, 30.0f, -45.0f},
		{179.0f, -179.0f, 2.0f}, {-179.0f, 179.0f, -2.0f},
		{0.0f, 180.0f, 180.0f}, {0.0f, -180.0f, -180.0f},
		{90.0f, 450.0f, 0.0f}};
	for (const FQuat& Basis : Bases)
	{
		for (const FGapCase& Case : Cases)
		{
			float Gap = MAX_flt;
			TestTrue(TEXT("A finite evaluated root can be converted to actor facing"),
				RpgTurnInPlaceRuntime::CalculateRootYawGap(
					FRotator(0.0f, Case.RootYaw, 0.0f).Quaternion() * Basis, Basis, Case.ActorYaw, Gap));
			TestTrue(TEXT("Root feedback removes arbitrary mesh basis and keeps shortest signed yaw"),
				FMath::IsNearlyEqual(Gap, Case.Expected, 1.e-3f));
		}
	}
	float InvalidGap = 42.0f;
	TestFalse(TEXT("A zero quaternion cannot masquerade as evaluated feedback"),
		RpgTurnInPlaceRuntime::CalculateRootYawGap(FQuat(0, 0, 0, 0), FQuat::Identity, 0.0f, InvalidGap));
	TestEqual(TEXT("Invalid feedback clears its output"), InvalidGap, 0.0f);
	TestFalse(TEXT("Non-finite target yaw rejects feedback"),
		RpgTurnInPlaceRuntime::CalculateRootYawGap(FQuat::Identity, FQuat::Identity,
			std::numeric_limits<float>::quiet_NaN(), InvalidGap));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceRootHistoryTrajectoryTest,
	"SurvivalRpg.Animation.TurnInPlace.RootFeedback.HistoryAlignedTrajectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceRootHistoryTrajectoryTest::RunTest(const FString& Parameters)
{
	const FQuat Bases[] = {
		FQuat::Identity, FRotator(0.0f, -90.0f, 0.0f).Quaternion(),
		FRotator(23.0f, 47.0f, -18.0f).Quaternion()};
	struct FHistoryCase { float ComponentYaw; float RootYaw; float TargetYaw; float ExpectedHeading; };
	const FHistoryCase Cases[] = {
		{135.0f, 99.382459f, 0.0f, -99.382459f},
		{135.0f, 99.382459f, 135.0f, 35.617541f},
		{179.0f, 177.0f, -179.0f, 4.0f},
		{-179.0f, -177.0f, 179.0f, -4.0f},
		{90.0f, 0.0f, 180.0f, 180.0f},
		{-90.0f, 0.0f, -180.0f, -180.0f}};
	FTransformTrajectory Source;
	for (const float Time : {-0.2f, 0.0f, 1.0f / 120.0f, 0.15f, 0.35f, 0.7f})
	{
		FTransformTrajectorySample& Sample = Source.Samples.AddDefaulted_GetRef();
		Sample.TimeInSeconds = Time;
		Sample.Position = FVector(10.0f + Time, 20.0f, 30.0f);
		Sample.Facing = FRotator(0.0f, -70.0f, 0.0f).Quaternion();
	}
	for (const FQuat& Basis : Bases)
	{
		for (const FHistoryCase& Case : Cases)
		{
			const FQuat EvaluatedComponent = (FRotator(0.0f, Case.ComponentYaw, 0.0f).Quaternion() * Basis).GetNormalized();
			const FQuat EvaluatedRoot = (FRotator(0.0f, Case.RootYaw, 0.0f).Quaternion() * Basis).GetNormalized();
			const FQuat ComponentSpaceRoot = (EvaluatedComponent.Inverse() * EvaluatedRoot).GetNormalized();
			const FQuat DesiredFacing = (FRotator(0.0f, Case.TargetYaw, 0.0f).Quaternion() * Basis).GetNormalized();
			const FTransformTrajectory Target = RpgTurnInPlaceRuntime::MakeStationaryFacingTrajectory(
				Source, Case.TargetYaw, Basis, EvaluatedComponent);
			TestEqual(TEXT("World-facing trajectory retains the source sample count"), Target.Samples.Num(), Source.Samples.Num());
			for (int32 Index = 0; Index < Source.Samples.Num(); ++Index)
			{
				TestEqual(TEXT("World-facing intent preserves sample times"), Target.Samples[Index].TimeInSeconds, Source.Samples[Index].TimeInSeconds);
				TestEqual(TEXT("World-facing intent preserves sample positions"), Target.Samples[Index].Position, Source.Samples[Index].Position);
				TestTrue(TEXT("Time zero and past keep evaluated basis; every positive sample contains current target intent once"),
					Target.Samples[Index].Facing.Equals(Source.Samples[Index].TimeInSeconds <= 0.0f
						? EvaluatedComponent : DesiredFacing, 1.e-6));
				TestTrue(TEXT("Building target intent leaves the source facing unchanged"),
					Source.Samples[Index].Facing.Equals(FRotator(0.0f, -70.0f, 0.0f).Quaternion(), 1.e-6));
			}

			// Pose History reconstructs world root as Trajectory(0) * the last evaluated root CS.
			const FQuat OriginRoot = (Target.Samples[1].Facing * ComponentSpaceRoot).GetNormalized();
			TestTrue(TEXT("The history origin reconstructs the actual evaluated world root, including arbitrary mesh basis"),
				OriginRoot.Equals(EvaluatedRoot, 1.e-6));
			for (const int32 FutureIndex : {4, 5})
			{
				// At schema heading times .35/.7, the configured .3-second root recovery has reached reference root.
				const FQuat FutureRoot = Target.Samples[FutureIndex].Facing;
				const double OriginActorYaw = (OriginRoot * Basis.Inverse()).Rotator().Yaw;
				const double FutureActorYaw = (FutureRoot * Basis.Inverse()).Rotator().Yaw;
				const float Heading = FMath::FindDeltaAngleDegrees(OriginActorYaw, FutureActorYaw);
				TestTrue(TEXT("Recovered future heading contains the actual remaining root gap once"),
					FMath::IsNearlyEqual(FMath::Abs(Case.ExpectedHeading), 180.0f)
						? FMath::IsNearlyEqual(FMath::Abs(Heading), 180.0f, 1.e-3f)
						: FMath::IsNearlyEqual(Heading, Case.ExpectedHeading, 1.e-3f));
			}
		}
	}

	const FQuat OldComponent = FRotator(0.0f, 135.0f, 0.0f).Quaternion();
	const FQuat OldRoot = FRotator(0.0f, 99.382459f, 0.0f).Quaternion();
	const FQuat OldRootCS = OldComponent.Inverse() * OldRoot;
	const float ContaminatedHeading = FMath::FindDeltaAngleDegrees(
		static_cast<float>(OldRootCS.Rotator().Yaw), 0.0f);
	TestTrue(TEXT("Using the new zero-degree component at the origin reproduces the wrong +35.617541-degree turn"),
		FMath::IsNearlyEqual(ContaminatedHeading, 35.617541f, 1.e-3f));
	for (const int32 FramesPerSecond : {15, 30, 60, 120})
	{
		Source.Samples[2].TimeInSeconds = 1.0f / FramesPerSecond;
		for (const float NewTarget : {0.0f, 30.0f, -60.0f, 0.0f, 0.0f})
		{
			// Several animation updates may occur before evaluation; never pair the old root with a new basis.
			const FTransformTrajectory Target = RpgTurnInPlaceRuntime::MakeStationaryFacingTrajectory(
				Source, NewTarget, FQuat::Identity, OldComponent);
			TestTrue(TEXT("Repeated updates without evaluation cannot advance the history origin"),
				(Target.Samples[1].Facing * OldRootCS).Equals(OldRoot, 1.e-6));
			TestTrue(TEXT("The first future sample responds immediately even when there has been no evaluation"),
				Target.Samples[2].Facing.Equals(FRotator(0.0f, NewTarget, 0.0f).Quaternion(), 1.e-6));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceRootFeedbackLifecycleTest,
	"SurvivalRpg.Animation.TurnInPlace.RootFeedback.ActiveInputAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceRootFeedbackLifecycleTest::RunTest(const FString& Parameters)
{
	for (const int32 FramesPerSecond : {15, 30, 60, 120})
	{
		AddInfo(FString::Printf(TEXT("Evaluated root feedback at %d FPS"), FramesPerSecond));
		FRootFeedbackTurnFixture SameDirection(FramesPerSecond);
		SameDirection.Start();
		TestEqual(TEXT("Root gap dispatches the first right turn"), SameDirection.State.QueryAngle, 90.0f);
		SameDirection.Latch();
		const uint32 FirstRequest = SameDirection.State.RequestSerial;
		SameDirection.Advance(90.0f, 30.0f);
		const FRpgTurnInPlaceUpdateResult Continued = SameDirection.Advance(150.0f, 30.0f);
		TestEqual(TEXT("Additional target yaw becomes actual outstanding root gap"), SameDirection.State.AccumulatedYaw, 120.0f);
		TestEqual(TEXT("Same-direction input preserves the selected clip before its marker"), SameDirection.State.RequestSerial, FirstRequest);
		TestEqual(TEXT("Active root feedback accumulates component rotation instead of rotating planted feet with it"),
			Continued.OffsetRootRotationMode, EOffsetRootBoneMode::Accumulate);
		SameDirection.Advance(150.0f, 90.0f);
		TestEqual(TEXT("Releasing mouse input retains the unconsumed 60-degree goal"), SameDirection.State.AccumulatedYaw, 60.0f);
		const FRpgTurnInPlaceUpdateResult Reentry = SameDirection.Advance(150.0f, 90.0f, true);
		TestEqual(TEXT("The authored marker directly dispatches the remaining turn"), Reentry.State.RequestSerial, FirstRequest + 1);
		TestEqual(TEXT("The followup queries remaining 60 degrees rather than historical 150 degrees"), Reentry.State.QueryAngle, 45.0f);
		TestTrue(TEXT("A new request clears the old selection"), Reentry.bClearSelection);
		TestFalse(TEXT("A natural followup never resets the visual root"), Reentry.bResetOffsetRootEveryFrame);
		TestTrue(TEXT("The followup has exactly one new forced search"), RpgTurnInPlaceRuntime::ConsumeForceInterrupt(true, SameDirection.State));
		TestFalse(TEXT("The same followup cannot force a second search"), RpgTurnInPlaceRuntime::ConsumeForceInterrupt(true, SameDirection.State));
		SameDirection.Latch();
		SameDirection.Advance(150.0f, 90.0f); // Repeated previous-evaluation feedback is permitted for one frame.
		TestEqual(TEXT("One-frame feedback latency cannot create another request"), SameDirection.State.RequestSerial, FirstRequest + 1);
		SameDirection.Advance(150.0f, 150.0f);
		TestEqual(TEXT("Reaching an unchanged goal does not cut off authored foot recovery"), SameDirection.State.State, ERpgTurnInPlaceState::Active);
		SameDirection.Advance(150.0f, 150.0f, true);
		TestEqual(TEXT("A completed goal enters interpolation recovery"), SameDirection.State.State, ERpgTurnInPlaceState::Recovering);
		SameDirection.Advance(90.0f, 150.0f);
		TestEqual(TEXT("New counter-input interrupts recovery without waiting 150 ms"), SameDirection.State.State, ERpgTurnInPlaceState::Collecting);
		SameDirection.Start(90.0f, 150.0f);
		TestEqual(TEXT("Fresh recovery input selects a left remaining-angle request"), SameDirection.State.QueryAngle, -45.0f);

		FRootFeedbackTurnFixture Counter(FramesPerSecond);
		Counter.Start();
		Counter.Latch();
		Counter.Advance(90.0f, 60.0f);
		const uint32 CounterFirst = Counter.State.RequestSerial;
		const FRpgTurnInPlaceUpdateResult Reversed = Counter.Advance(0.0f, 60.0f);
		TestEqual(TEXT("Returning input to its starting yaw still requires a left turn from the actual root"), Reversed.State.QueryAngle, -45.0f);
		TestEqual(TEXT("A material opposite gap interrupts the active clip before its marker"), Reversed.State.RequestSerial, CounterFirst + 1);
		TestFalse(TEXT("Counter-turn selection preserves the evaluated root"), Reversed.bResetOffsetRootEveryFrame);
		Counter.Latch();
		Counter.Advance(0.0f, 60.0f);
		TestEqual(TEXT("An unchanged stale root sample cannot replay the counter request"), Counter.State.RequestSerial, CounterFirst + 1);

		for (const float Direction : {-1.0f, 1.0f})
		{
			FRootFeedbackTurnFixture HalfTurn(FramesPerSecond);
			HalfTurn.Start(-135.0f * Direction, -180.0f * Direction);
			HalfTurn.Latch();
			TestEqual(TEXT("The half-turn fixture starts a signed 45-degree clip"), HalfTurn.State.QueryAngle, 45.0f * Direction);
			HalfTurn.Advance(-135.0f * Direction, -138.0f * Direction);
			const FRpgTurnInPlaceUpdateResult HalfTurnResult = HalfTurn.Advance(45.0f * Direction, -138.0f * Direction);
			TestEqual(TEXT("Exact half-turn input uses the actual opposite 177-degree root gap despite ambiguous target-delta sign"),
				HalfTurnResult.State.QueryAngle, -180.0f * Direction);
			TestEqual(TEXT("An ambiguous half-turn interrupts the selected clip exactly once"), HalfTurnResult.State.RequestSerial, 2u);
			TestFalse(TEXT("Half-turn replacement preserves the evaluated root"), HalfTurnResult.bResetOffsetRootEveryFrame);
			HalfTurn.Latch();
			HalfTurn.Advance(45.0f * Direction, -138.0f * Direction);
			TestEqual(TEXT("Repeated half-turn feedback cannot dispatch another request"), HalfTurn.State.RequestSerial, 2u);
		}

		FRootFeedbackTurnFixture AddedInputOvershoot(FramesPerSecond);
		AddedInputOvershoot.Start();
		AddedInputOvershoot.Latch();
		AddedInputOvershoot.Advance(150.0f, 30.0f);
		AddedInputOvershoot.Advance(150.0f, -170.0f);
		TestEqual(TEXT("Overshoot after earlier same-direction input is not fresh counter-input"),
			AddedInputOvershoot.State.RequestSerial, 1u);
		TestEqual(TEXT("Earlier extra input does not let no-input overshoot cut off authored foot recovery"),
			AddedInputOvershoot.State.State, ERpgTurnInPlaceState::Active);

		FRootFeedbackTurnFixture CounterStop(FramesPerSecond);
		CounterStop.Start();
		CounterStop.Latch();
		CounterStop.Advance(90.0f, 60.0f);
		CounterStop.Advance(60.0f, 60.0f);
		TestEqual(TEXT("A changed counter-goal already at the root stops the obsolete clip"), CounterStop.State.State, ERpgTurnInPlaceState::Recovering);
		TestEqual(TEXT("Stopping at the evaluated root needs no opposite clip"), CounterStop.State.RequestSerial, 1u);

		for (const bool bOvershot : {false, true})
		{
			FRootFeedbackTurnFixture NoInput(FramesPerSecond);
			NoInput.Start();
			NoInput.Latch();
			const float RootYaw = bOvershot ? 150.0f : 0.0f;
			NoInput.Advance(90.0f, RootYaw);
			TestEqual(TEXT("No-input root overshoot cannot synthesize an early counter-turn"), NoInput.State.RequestSerial, 1u);
			NoInput.Advance(90.0f, RootYaw, true);
			TestEqual(TEXT("Overshoot or no visual progress exits at the marker"), NoInput.State.State, ERpgTurnInPlaceState::Recovering);
			for (int32 Frame = 0; Frame < FramesPerSecond; ++Frame)
			{
				NoInput.Advance(90.0f, Frame % 2 == 0 ? RootYaw : 30.0f);
			}
			TestEqual(TEXT("Even changing residual root feedback cannot ping-pong an unchanged goal"), NoInput.State.RequestSerial, 1u);
			TestEqual(TEXT("No-input recovery finishes without requeuing the old target"), NoInput.State.State, ERpgTurnInPlaceState::Inactive);
		}

		FRootFeedbackTurnFixture Undershot(FramesPerSecond);
		Undershot.Start(180.0f);
		Undershot.Latch();
		Undershot.Advance(180.0f, 90.0f, true);
		TestEqual(TEXT("The authored marker releases an unchanged goal's remaining root gap to recovery"),
			Undershot.State.State, ERpgTurnInPlaceState::Recovering);
		TestEqual(TEXT("A partially consumed initial goal does not invent additional turn input"), Undershot.State.RequestSerial, 1u);
		Undershot.Advance(180.0f, 90.0f, true);
		TestEqual(TEXT("Repeated marker feedback cannot requeue an unchanged target"), Undershot.State.State, ERpgTurnInPlaceState::Recovering);
		TestEqual(TEXT("No-input recovery keeps its serial"), Undershot.State.RequestSerial, 1u);

		FRootFeedbackTurnFixture Wrapped(FramesPerSecond);
		Wrapped.Start(-100.0f, 170.0f);
		Wrapped.Latch();
		TestEqual(TEXT("The initial +90 gap crosses the world-yaw wrap without reversing"), Wrapped.State.QueryAngle, 90.0f);
		Wrapped.Advance(-100.0f, -150.0f);
		Wrapped.Advance(170.0f, -150.0f);
		TestEqual(TEXT("A counter-goal crossing +180 still queries the short left gap"), Wrapped.State.QueryAngle, -45.0f);
		TestEqual(TEXT("World-yaw wrapping produces only one new counter request"), Wrapped.State.RequestSerial, 2u);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceRootHalfTurnEquivalenceTest,
	"SurvivalRpg.Animation.TurnInPlace.RootFeedback.HalfTurnSignEquivalence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceRootHalfTurnEquivalenceTest::RunTest(const FString& Parameters)
{
	for (const int32 FramesPerSecond : {15, 30, 60, 120})
	{
		for (const float QuerySign : {-1.0f, 1.0f})
		{
			for (const float RootDirection : {-1.0f, 1.0f})
			{
				AddInfo(FString::Printf(TEXT("Half-turn query %.0f, physical direction %.0f at %d FPS"),
					180.0f * QuerySign, RootDirection, FramesPerSecond));
				const float InitialTarget = 180.0f * QuerySign;
				auto AdvanceRoot = [&](FRootFeedbackTurnFixture& Turn, float ActorYaw, float StartRoot, float EndRoot)
				{
					const int32 Steps = FMath::CeilToInt(0.4f * FramesPerSecond);
					for (int32 Step = 1; Step <= Steps; ++Step)
					{
						Turn.Advance(ActorYaw, FMath::Lerp(StartRoot, EndRoot, static_cast<float>(Step) / Steps));
					}
				};

				// Reproduce the measured 180_R path for either +/-180 query, and its left-turn mirror.
				// The curve/marker belongs to the selected pose; QueryAngle does not identify its physical direction.
				for (const float RootMagnitude : {0.0f, 45.0f, 123.45f, 180.0f, 240.0f})
				{
					FRootFeedbackTurnFixture Unchanged(FramesPerSecond);
					Unchanged.Start(InitialTarget);
					Unchanged.Latch();
					TestEqual(TEXT("Equivalent target representation retains its diagnostic query sign"),
						Unchanged.State.QueryAngle, InitialTarget);
					const float RootYaw = RootMagnitude * RootDirection;
					AdvanceRoot(Unchanged, InitialTarget, 0.0f, RootYaw);
					TestEqual(TEXT("Unchanged half-turn intent keeps its authored clip until the marker"),
						Unchanged.State.State, ERpgTurnInPlaceState::Active);
					TestEqual(TEXT("Root progress or overshoot cannot cause an early request"), Unchanged.State.RequestSerial, 1u);
					TestEqual(TEXT("Half-turn direction follows physical progress and remains stable past 180 degrees"),
						Unchanged.State.MeasuredRootYawDirection, RootMagnitude > 0.0f ? RootDirection : 0.0f);
					const FRpgTurnInPlaceUpdateResult Finished = Unchanged.Advance(InitialTarget, RootYaw, true);
					TestEqual(TEXT("Any unchanged goal releases to recovery at the authored marker, independently of query sign or root remainder"),
						Finished.State.State, ERpgTurnInPlaceState::Recovering);
					TestEqual(TEXT("Root remainder never substitutes for additional user input"), Finished.State.RequestSerial, 1u);
					TestEqual(TEXT("Recovery clears the completed request's observed direction"), Finished.State.MeasuredRootYawDirection, 0.0f);
					TestFalse(TEXT("Natural half-turn completion preserves the visual root for interpolation"), Finished.bResetOffsetRootEveryFrame);
					for (int32 Frame = 0; Frame < FramesPerSecond; ++Frame)
					{
						Unchanged.Advance(InitialTarget, RootYaw);
					}
					TestEqual(TEXT("An unchanged stalled or overshot goal cannot create a recovery loop"), Unchanged.State.RequestSerial, 1u);
					TestEqual(TEXT("Unchanged half-turn recovery finishes normally"), Unchanged.State.State, ERpgTurnInPlaceState::Inactive);
				}

				for (const float AddedYaw : {60.0f, 90.0f})
				{
					FRootFeedbackTurnFixture Additional(FramesPerSecond);
					Additional.Start(InitialTarget);
					Additional.Latch();
					AdvanceRoot(Additional, InitialTarget, 0.0f, 110.0f * RootDirection);
					const float ChangedTarget = InitialTarget + AddedYaw * RootDirection;
					const FRpgTurnInPlaceUpdateResult Continued = Additional.Advance(ChangedTarget, 110.0f * RootDirection);
					TestEqual(TEXT("Same-direction input during an active half-turn does not reverse a clip whose query has the other sign"),
						Continued.State.RequestSerial, 1u);
					TestEqual(TEXT("Additional input retains the active clip before its foot marker"),
						Continued.State.State, ERpgTurnInPlaceState::Active);
					AdvanceRoot(Additional, ChangedTarget, 110.0f * RootDirection, 180.0f * RootDirection);
					const FRpgTurnInPlaceUpdateResult Followup = Additional.Advance(ChangedTarget, 180.0f * RootDirection, true);
					TestEqual(TEXT("Actual additional input dispatches one remaining turn at the marker"), Followup.State.RequestSerial, 2u);
					TestTrue(TEXT("Followup uses the evaluated outstanding root gap, not the original half-turn sign"),
						FMath::IsNearlyEqual(Followup.State.AccumulatedYaw, AddedYaw * RootDirection, 1.e-3f));
					TestEqual(TEXT("Additional 60/90 degree intent selects the matching signed remaining-angle bucket"),
						Followup.State.QueryAngle, RpgTurnInPlaceRuntime::QuantizeAngle(AddedYaw * RootDirection));
					TestEqual(TEXT("A new request clears its predecessor's measured direction"), Followup.State.MeasuredRootYawDirection, 0.0f);
					TestFalse(TEXT("Additional half-turn intent does not reset evaluated root motion"), Followup.bResetOffsetRootEveryFrame);
					Additional.Latch();
					Additional.Advance(ChangedTarget, 180.0f * RootDirection, true);
					TestEqual(TEXT("Repeated stale completion without further input cannot chain another request"),
						Additional.State.State, ERpgTurnInPlaceState::Recovering);
					TestEqual(TEXT("The stalled followup retains its request serial"), Additional.State.RequestSerial, 2u);
				}

				for (const float CounterTargetMagnitude : {0.0f, 30.0f})
				{
					FRootFeedbackTurnFixture Counter(FramesPerSecond);
					Counter.Start(InitialTarget);
					Counter.Latch();
					AdvanceRoot(Counter, InitialTarget, 0.0f, 70.0f * RootDirection);
					const float CounterTarget = CounterTargetMagnitude * RootDirection;
					const FRpgTurnInPlaceUpdateResult Reversed = Counter.Advance(CounterTarget, 70.0f * RootDirection);
					TestEqual(TEXT("Counter-input reverses the measured half-turn direction regardless of its +/-180 query representation"),
						Reversed.State.RequestSerial, 2u);
					TestEqual(TEXT("Counter-input selects the actual outstanding opposite gap"), Reversed.State.QueryAngle,
						RpgTurnInPlaceRuntime::QuantizeAngle((CounterTargetMagnitude - 70.0f) * RootDirection));
					TestFalse(TEXT("Counter-input preserves the root instead of snapping to the new target"), Reversed.bResetOffsetRootEveryFrame);
					Counter.Latch();
					Counter.Advance(CounterTarget, 70.0f * RootDirection);
					TestEqual(TEXT("One-frame root feedback latency cannot repeat a counter request"), Counter.State.RequestSerial, 2u);
				}

				FRootFeedbackTurnFixture CounterStop(FramesPerSecond);
				CounterStop.Start(InitialTarget);
				CounterStop.Latch();
				AdvanceRoot(CounterStop, InitialTarget, 0.0f, 70.0f * RootDirection);
				CounterStop.Advance(70.0f * RootDirection, 70.0f * RootDirection);
				TestEqual(TEXT("Counter-input to the evaluated root cancels either half-turn query without another clip"),
					CounterStop.State.State, ERpgTurnInPlaceState::Recovering);
				TestEqual(TEXT("An already reached counter-goal needs no new search"), CounterStop.State.RequestSerial, 1u);

				FRootFeedbackTurnFixture AddedOvershoot(FramesPerSecond);
				AddedOvershoot.Start(InitialTarget);
				AddedOvershoot.Latch();
				AdvanceRoot(AddedOvershoot, InitialTarget, 0.0f, 110.0f * RootDirection);
				const float OvershootTarget = InitialTarget + 60.0f * RootDirection;
				AddedOvershoot.Advance(OvershootTarget, 110.0f * RootDirection);
				AdvanceRoot(AddedOvershoot, OvershootTarget, 110.0f * RootDirection, 290.0f * RootDirection);
				TestEqual(TEXT("Wrapped endpoint progress cannot replace the initially observed half-turn direction"),
					AddedOvershoot.State.MeasuredRootYawDirection, RootDirection);
				TestEqual(TEXT("Overshoot after earlier additional input is not new counter-input"), AddedOvershoot.State.RequestSerial, 1u);
				AddedOvershoot.Advance(OvershootTarget, 290.0f * RootDirection, true);
				TestEqual(TEXT("An overshot additional goal interpolates instead of ping-ponging into a reverse turn"),
					AddedOvershoot.State.State, ERpgTurnInPlaceState::Recovering);
				TestEqual(TEXT("Overshoot suppression preserves its request serial"), AddedOvershoot.State.RequestSerial, 1u);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTurnInPlaceRootFeedbackInvalidationTest,
	"SurvivalRpg.Animation.TurnInPlace.RootFeedback.InvalidationAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceRootFeedbackInvalidationTest::RunTest(const FString& Parameters)
{
	FRootFeedbackTurnFixture LateFeedback(60);
	LateFeedback.Snapshot.bHasRootYawGap = false;
	LateFeedback.Start();
	LateFeedback.Latch();
	LateFeedback.Snapshot.bHasRootYawGap = true;
	LateFeedback.Advance(90.0f, 30.0f);
	TestTrue(TEXT("Feedback becoming relevant midclip anchors the current world target"), LateFeedback.State.bHasTargetYawAnchor);
	LateFeedback.Advance(0.0f, 60.0f);
	TestEqual(TEXT("Subsequent counter-input is recognized after a legacy request gains feedback"), LateFeedback.State.QueryAngle, -45.0f);

	FRootFeedbackTurnFixture Turn(60);
	Turn.Start();
	Turn.Latch();
	Turn.Snapshot.bHasRootYawGap = false;
	const FRpgTurnInPlaceUpdateResult Legacy = Turn.Advance(150.0f, 30.0f);
	TestEqual(TEXT("Missing feedback retains the existing latched-turn policy"),
		Legacy.OffsetRootRotationMode, EOffsetRootBoneMode::LockOffsetIncreaseAndConsumeAnimation);
	TestEqual(TEXT("Missing feedback preserves the actor-yaw accumulator fallback"), Legacy.State.AccumulatedYaw, 150.0f);
	TestEqual(TEXT("Fallback does not introduce midclip retargeting"), Legacy.State.RequestSerial, 1u);
	Turn.Snapshot.bHasRootYawGap = true;
	Turn.Snapshot.RootYawGap = std::numeric_limits<float>::quiet_NaN();
	Turn.Snapshot.ActorYawDelta = 0.0f;
	const FRpgTurnInPlaceUpdateResult Invalid = RpgTurnInPlaceRuntime::Update(Turn.State, Turn.Snapshot, 1.0f / 60.0f);
	TestEqual(TEXT("Non-finite feedback takes the safe legacy fallback"),
		Invalid.OffsetRootRotationMode, EOffsetRootBoneMode::LockOffsetIncreaseAndConsumeAnimation);
	Turn.Snapshot.bProxyHardReset = true;
	const FRpgTurnInPlaceUpdateResult Reset = Turn.Advance(150.0f, 0.0f);
	TestEqual(TEXT("Hard reset clears the active root-feedback request"), Reset.State.State, ERpgTurnInPlaceState::Inactive);
	TestFalse(TEXT("Hard reset invalidates target history"), Reset.State.bHasTargetYawAnchor);
	TestTrue(TEXT("Real lifecycle reset still resets the root node"), Reset.bResetOffsetRootEveryFrame);
	return true;
}

#endif
