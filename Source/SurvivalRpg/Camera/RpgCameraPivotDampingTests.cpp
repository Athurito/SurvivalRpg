#include "RpgCameraPivotDamping.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace RpgCameraPivotDampingTests
{
	constexpr float Factor = 20.0f;
	constexpr float ResetDistance = 500.0f;
	constexpr float FrameTime = 1.0f / 60.0f;

	FVector RunRamp(TConstArrayView<float> FrameTimes)
	{
		FRpgCameraPivotDamping Damping;
		const FVector Origin(300.0, -200.0, 70.0);
		const FVector Velocity(240.0, -80.0, 100.0);
		FVector Position = Damping.Update(Origin, 0.0f, Factor, ResetDistance);
		double Time = 0.0;
		for (float DeltaTime : FrameTimes)
		{
			Time += DeltaTime;
			Position = Damping.Update(Origin + Velocity * Time, DeltaTime, Factor, ResetDistance);
		}
		return Position;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCameraPivotDampingLifecycleTest,
	"SurvivalRpg.Camera.PivotDamping.DisabledAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCameraPivotDampingLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgCameraPivotDampingTests;
	FRpgCameraPivotDamping Damping;
	const FVector Initial(120.0, -35.0, 180.0);
	const FVector Raised = Initial + FVector(40.0, 0.0, 100.0);
	TestTrue(TEXT("A new camera starts at its target rather than travelling from the world origin"),
		Damping.Update(Initial, FrameTime, Factor, ResetDistance) == Initial);
	TestFalse(TEXT("An ordinary movement creates a visible lag before a reset"),
		Damping.Update(Raised, FrameTime, Factor, ResetDistance).Equals(Raised, 0.01));
	TestTrue(TEXT("Disabling damping immediately restores direct following despite prior lag"),
		Damping.Update(Raised, FrameTime, 0.0f, ResetDistance) == Raised);
	TestTrue(TEXT("Disabled following tracks subsequent changes exactly"),
		Damping.Update(Initial, FrameTime, 0.0f, ResetDistance) == Initial);
	TestTrue(TEXT("Enabling a stationary camera has no stale position or velocity"),
		Damping.Update(Initial, FrameTime, Factor, ResetDistance) == Initial);

	Damping.Update(Raised, FrameTime, Factor, ResetDistance);
	Damping.Reset();
	TestTrue(TEXT("Activation or a changed target discards the preceding camera's movement history"),
		Damping.Update(Raised, FrameTime, Factor, ResetDistance) == Raised);
	TestTrue(TEXT("Reset also discards velocity, so a stationary target stays still"),
		Damping.Update(Raised, FrameTime, Factor, ResetDistance) == Raised);

	Damping.Update(Initial, FrameTime, Factor, ResetDistance);
	TestTrue(TEXT("An explicit camera cut snaps even during a zero-time update"),
		Damping.Update(Raised, 0.0f, Factor, ResetDistance, true) == Raised);
	const FVector Teleported = Raised + FVector(2000.0, -700.0, 900.0);
	TestTrue(TEXT("A teleport bypasses interpolation instead of dragging the camera across the map"),
		Damping.Update(Teleported, FrameTime, Factor, ResetDistance) == Teleported);
	TestTrue(TEXT("Teleport recovery does not retain a spring velocity"),
		Damping.Update(Teleported, FrameTime, Factor, ResetDistance) == Teleported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCameraPivotDampingConvergenceTest,
	"SurvivalRpg.Camera.PivotDamping.SmoothConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCameraPivotDampingConvergenceTest::RunTest(const FString& Parameters)
{
	using namespace RpgCameraPivotDampingTests;
	FRpgCameraPivotDamping Damping;
	const FVector Initial(150.0, -260.0, 25.0);
	const FVector Target = Initial + FVector(0.0, 0.0, 100.0);
	Damping.Update(Initial, FrameTime, Factor, ResetDistance);
	FVector Position = Damping.Update(Target, FrameTime, Factor, ResetDistance);
	TestTrue(TEXT("A traversal-sized rise moves the camera partway without snapping to the new height"),
		Position.Z > Initial.Z && Position.Z < Target.Z);
	bool bMonotonic = true;
	bool bNoOvershoot = true;
	bool bOtherAxesStayFixed = true;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		const FVector Previous = Position;
		Position = Damping.Update(Target, FrameTime, Factor, ResetDistance);
		bMonotonic &= Position.Z >= Previous.Z - 0.00001;
		bNoOvershoot &= Position.Z <= Target.Z + 0.00001;
		bOtherAxesStayFixed &= Position.X == Initial.X && Position.Y == Initial.Y;
	}
	TestTrue(TEXT("A stationary ledge is approached continuously without reversing the camera"), bMonotonic);
	TestTrue(TEXT("The stationary camera target is never overshot"), bNoOvershoot);
	TestTrue(TEXT("Vertical damping adds no lateral drift"), bOtherAxesStayFixed);
	TestTrue(TEXT("The camera settles onto the stationary target"), Position.Equals(Target, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCameraPivotDampingTimingTest,
	"SurvivalRpg.Camera.PivotDamping.FrameRateHitchAndPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCameraPivotDampingTimingTest::RunTest(const FString& Parameters)
{
	using namespace RpgCameraPivotDampingTests;
	TArray<float> Frames30;
	TArray<float> Frames60;
	TArray<float> Frames120;
	Frames30.Init(1.0f / 30.0f, 30);
	Frames60.Init(FrameTime, 60);
	Frames120.Init(1.0f / 120.0f, 120);
	const FVector At30 = RunRamp(Frames30);
	const FVector At60 = RunRamp(Frames60);
	const FVector At120 = RunRamp(Frames120);
	TestTrue(TEXT("The same one-second movement has the same camera response at 30 and 60 fps"), At30.Equals(At60, 0.01));
	TestTrue(TEXT("The same one-second movement has the same camera response at 60 and 120 fps"), At60.Equals(At120, 0.01));
	const FVector RampEnd(540.0, -280.0, 170.0);
	TestTrue(TEXT("Frame-rate agreement still includes damping rather than immediate following"),
		At60.X > 300.0 && At60.X < RampEnd.X && At60.Y < -200.0 && At60.Y > RampEnd.Y
		&& At60.Z > 70.0 && At60.Z < RampEnd.Z);
	TArray<float> HitchFrames;
	HitchFrames.Init(FrameTime, 30);
	HitchFrames.Add(0.2f);
	for (int32 Frame = 0; Frame < 18; ++Frame) HitchFrames.Add(FrameTime);
	TestTrue(TEXT("A 200 ms hitch on the same linear path does not change the final camera response"),
		RunRamp(HitchFrames).Equals(At60, 0.01));

	FRpgCameraPivotDamping Paused;
	FRpgCameraPivotDamping Unpaused;
	const FVector Start(10.0, 20.0, 30.0);
	const FVector Target(10.0, 20.0, 130.0);
	Paused.Update(Start, FrameTime, Factor, ResetDistance);
	Unpaused.Update(Start, FrameTime, Factor, ResetDistance);
	const FVector BeforePause = Paused.Update(Target, FrameTime, Factor, ResetDistance);
	Unpaused.Update(Target, FrameTime, Factor, ResetDistance);
	TestTrue(TEXT("A paused camera does not consume movement or advance its spring"),
		Paused.Update(Target + FVector(0.0, 0.0, 10.0), 0.0f, Factor, ResetDistance) == BeforePause);
	TestTrue(TEXT("Resuming after a zero-time observation has the same state as the uninterrupted camera"),
		Paused.Update(Target, FrameTime, Factor, ResetDistance).Equals(
			Unpaused.Update(Target, FrameTime, Factor, ResetDistance), 0.00001));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
