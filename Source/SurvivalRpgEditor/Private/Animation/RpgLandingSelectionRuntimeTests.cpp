// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgGaspLocomotionConfig.h"
#include "SurvivalRpg/Animation/RpgLandingRuntime.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLandingSelectionRuntimeTest,
	"SurvivalRpg.Animation.Jump.Runtime.LandingSelection",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgLandingSelectionRuntimeTest::RunTest(const FString& Parameters)
{
	const FRpgGaspLocomotionTuning DefaultTuning;
	const FVector GravityAcceleration(0.0, 0.0, -1000.0);

	auto MakeSnapshot = [](
		ERpgLocomotionGait Gait,
		float HorizontalSpeed,
		bool bHasMoveIntent,
		float MaximumDownwardSpeed,
		float PredictedImpactDownwardSpeed,
		float VerticalVelocity = -100.0f)
	{
		FRpgLandingSelectionSnapshot Snapshot;
		Snapshot.HorizontalVelocity = FVector(HorizontalSpeed, 0.0, 0.0);
		Snapshot.HorizontalSpeed = HorizontalSpeed;
		Snapshot.VerticalVelocity = VerticalVelocity;
		Snapshot.MaximumDownwardSpeed = MaximumDownwardSpeed;
		Snapshot.PredictedImpactDownwardSpeed = PredictedImpactDownwardSpeed;
		Snapshot.Gait = Gait;
		Snapshot.AirborneEpoch = 1;
		Snapshot.bHasMoveIntent = bHasMoveIntent;
		Snapshot.bIsValid = true;
		if (PredictedImpactDownwardSpeed > 0.0f)
		{
			Snapshot.PredictedLanding.LandingLocation = FVector(100.0, 0.0, 0.0);
			Snapshot.PredictedLanding.LandingNormal = FVector::UpVector;
			Snapshot.PredictedLanding.TimeToLand = 0.1f;
			Snapshot.PredictedLanding.bIsValid = true;
		}
		return Snapshot;
	};
	auto TestResolvedRole = [this, &DefaultTuning](
		const TCHAR* Context,
		const FRpgLandingSelectionSnapshot& Snapshot,
		ERpgMotionMatchingDatabaseRole ExpectedRole)
	{
		TestEqual(
			Context,
			RpgLandingRuntime::ResolveDatabaseRole(Snapshot, DefaultTuning),
			ExpectedRole);
	};
	auto TestSnapshotParity = [this](
		const FString& Context,
		const FRpgLandingSelectionSnapshot& Actual,
		const FRpgLandingSelectionSnapshot& Expected)
	{
		TestTrue(
			*FString::Printf(TEXT("%s horizontal velocity"), *Context),
			Actual.HorizontalVelocity.Equals(Expected.HorizontalVelocity));
		TestTrue(
			*FString::Printf(TEXT("%s horizontal speed"), *Context),
			FMath::IsNearlyEqual(Actual.HorizontalSpeed, Expected.HorizontalSpeed));
		TestTrue(
			*FString::Printf(TEXT("%s vertical velocity"), *Context),
			FMath::IsNearlyEqual(Actual.VerticalVelocity, Expected.VerticalVelocity));
		TestTrue(
			*FString::Printf(TEXT("%s measured downward maximum"), *Context),
			FMath::IsNearlyEqual(Actual.MaximumDownwardSpeed, Expected.MaximumDownwardSpeed));
		TestTrue(
			*FString::Printf(TEXT("%s predicted impact speed"), *Context),
			FMath::IsNearlyEqual(
				Actual.PredictedImpactDownwardSpeed,
				Expected.PredictedImpactDownwardSpeed));
		TestEqual(*FString::Printf(TEXT("%s gait"), *Context), Actual.Gait, Expected.Gait);
		TestEqual(
			*FString::Printf(TEXT("%s airborne epoch"), *Context),
			Actual.AirborneEpoch,
			Expected.AirborneEpoch);
		TestEqual(
			*FString::Printf(TEXT("%s move intent"), *Context),
			Actual.bHasMoveIntent,
			Expected.bHasMoveIntent);
		TestEqual(
			*FString::Printf(TEXT("%s validity"), *Context),
			Actual.bIsValid,
			Expected.bIsValid);
		TestEqual(
			*FString::Printf(TEXT("%s prediction validity"), *Context),
			Actual.PredictedLanding.bIsValid,
			Expected.PredictedLanding.bIsValid);
		TestTrue(
			*FString::Printf(TEXT("%s predicted location"), *Context),
			Actual.PredictedLanding.LandingLocation.Equals(
				Expected.PredictedLanding.LandingLocation));
		TestTrue(
			*FString::Printf(TEXT("%s predicted normal"), *Context),
			Actual.PredictedLanding.LandingNormal.Equals(
				Expected.PredictedLanding.LandingNormal));
		TestTrue(
			*FString::Printf(TEXT("%s predicted time"), *Context),
			FMath::IsNearlyEqual(
				Actual.PredictedLanding.TimeToLand,
				Expected.PredictedLanding.TimeToLand));
	};

	// The inclusive 3 cm/s idle boundary is physical: airborne input remains useful frozen
	// context, but cannot select moving landing content before speed leaves that Idle band.
	TestResolvedRole(
		TEXT("Zero speed without intent selects stand-idle light landing"),
		MakeSnapshot(ERpgLocomotionGait::Idle, 0.0f, false, 100.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	TestResolvedRole(
		TEXT("Three cm/s without intent remains in the inclusive stand-idle domain"),
		MakeSnapshot(ERpgLocomotionGait::Run, 3.0f, false, 100.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	TestResolvedRole(
		TEXT("3.01 cm/s leaves the idle domain"),
		MakeSnapshot(ERpgLocomotionGait::Walk, 3.01f, false, 100.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::WalkLightLanding);
	TestResolvedRole(
		TEXT("Move intent alone keeps a zero-speed Light landing in the stand-idle domain"),
		MakeSnapshot(ERpgLocomotionGait::Run, 0.0f, true, 100.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	TestResolvedRole(
		TEXT("Move intent alone keeps a zero-speed Heavy landing in the stand-idle domain"),
		MakeSnapshot(ERpgLocomotionGait::Run, 0.0f, true, 700.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding);

	TestResolvedRole(
		TEXT("Walk below the heavy boundary selects Walk Light"),
		MakeSnapshot(ERpgLocomotionGait::Walk, 200.0f, true, 699.99f, 0.0f),
		ERpgMotionMatchingDatabaseRole::WalkLightLanding);
	TestResolvedRole(
		TEXT("Walk at the heavy boundary selects Walk Heavy"),
		MakeSnapshot(ERpgLocomotionGait::Walk, 200.0f, true, 700.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding);
	TestResolvedRole(
		TEXT("Run below the heavy boundary selects Run Light"),
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 699.99f, 0.0f),
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	TestResolvedRole(
		TEXT("Run at the heavy boundary selects Run Heavy"),
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	TestResolvedRole(
		TEXT("Explicit Sprint uses Run Light until issue 62 owns Sprint landing content"),
		MakeSnapshot(ERpgLocomotionGait::Sprint, 650.0f, true, 699.99f, 0.0f),
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	TestResolvedRole(
		TEXT("Explicit Sprint uses Run Heavy until issue 62 owns Sprint landing content"),
		MakeSnapshot(ERpgLocomotionGait::Sprint, 650.0f, true, 700.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);

	TestResolvedRole(
		TEXT("699.99 cm/s remains light at the inclusive heavy boundary"),
		MakeSnapshot(ERpgLocomotionGait::Idle, 0.0f, false, 699.99f, 0.0f),
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	TestResolvedRole(
		TEXT("700 cm/s selects heavy at the inclusive heavy boundary"),
		MakeSnapshot(ERpgLocomotionGait::Idle, 0.0f, false, 700.0f, 0.0f),
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding);
	TestResolvedRole(
		TEXT("Measured impact can be the stronger Heavy signal"),
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 300.0f),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	TestResolvedRole(
		TEXT("Predicted impact can be the stronger Heavy signal"),
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 300.0f, 700.0f),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	TestResolvedRole(
		TEXT("The maximum of two sub-threshold impact speeds remains Light"),
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 699.99f, 699.99f),
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	TestResolvedRole(
		TEXT("Upward speed magnitude alone never selects Heavy"),
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 0.0f, 0.0f, 900.0f),
		ERpgMotionMatchingDatabaseRole::RunLightLanding);

	FRpgGaspLocomotionTuning CustomRoleTuning = DefaultTuning;
	CustomRoleTuning.StationarySpeedThreshold = 10.0f;
	CustomRoleTuning.HeavyLandingSpeedThreshold = 900.0f;
	const FRpgLandingSelectionSnapshot CustomRoleSnapshot =
		MakeSnapshot(ERpgLocomotionGait::Run, 5.0f, true, 700.0f, 0.0f);
	TestEqual(
		TEXT("Custom stationary and Heavy thresholds change landing role selection together"),
		RpgLandingRuntime::ResolveDatabaseRole(CustomRoleSnapshot, CustomRoleTuning),
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	TestEqual(
		TEXT("Default tuning remains unchanged after a custom landing role query"),
		RpgLandingRuntime::ResolveDatabaseRole(CustomRoleSnapshot, DefaultTuning),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);

	const float QuietNaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	FRpgLandingSelectionSnapshot InvalidSnapshot =
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 0.0f);
	InvalidSnapshot.bIsValid = false;
	TestResolvedRole(TEXT("An invalid snapshot fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	InvalidSnapshot = MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 0.0f);
	InvalidSnapshot.AirborneEpoch = 0;
	TestResolvedRole(TEXT("A snapshot without an airborne epoch fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	InvalidSnapshot = MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 0.0f);
	InvalidSnapshot.HorizontalVelocity.X = QuietNaN;
	TestResolvedRole(TEXT("NaN horizontal velocity fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	InvalidSnapshot = MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 0.0f);
	InvalidSnapshot.HorizontalSpeed = Infinity;
	TestResolvedRole(TEXT("Infinite horizontal speed fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	InvalidSnapshot = MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 0.0f);
	InvalidSnapshot.VerticalVelocity = QuietNaN;
	TestResolvedRole(TEXT("NaN vertical velocity fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	InvalidSnapshot = MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 0.0f);
	InvalidSnapshot.MaximumDownwardSpeed = Infinity;
	TestResolvedRole(TEXT("Infinite measured impact speed fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	InvalidSnapshot = MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 200.0f);
	InvalidSnapshot.PredictedImpactDownwardSpeed = QuietNaN;
	TestResolvedRole(TEXT("NaN predicted impact speed fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	InvalidSnapshot = MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 500.0f, 200.0f);
	InvalidSnapshot.PredictedLanding.TimeToLand = Infinity;
	TestResolvedRole(TEXT("Infinite predicted contact time fails closed"), InvalidSnapshot, ERpgMotionMatchingDatabaseRole::None);
	FRpgGaspLocomotionTuning InvalidHeavyTuning = DefaultTuning;
	InvalidHeavyTuning.HeavyLandingSpeedThreshold = QuietNaN;
	TestEqual(
		TEXT("A NaN Heavy threshold fails closed"),
		RpgLandingRuntime::ResolveDatabaseRole(
			MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 0.0f),
			InvalidHeavyTuning),
		ERpgMotionMatchingDatabaseRole::None);
	InvalidHeavyTuning.HeavyLandingSpeedThreshold = Infinity;
	TestEqual(
		TEXT("An infinite Heavy threshold fails closed"),
		RpgLandingRuntime::ResolveDatabaseRole(
			MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 0.0f),
			InvalidHeavyTuning),
		ERpgMotionMatchingDatabaseRole::None);

	// Capture retains horizontal direction and strips vertical velocity for every cardinal input.
	const FVector HorizontalDirectionCases[] =
	{
		FVector(300.0, 0.0, 0.0),
		FVector(-300.0, 0.0, 0.0),
		FVector(0.0, 300.0, 0.0),
		FVector(0.0, -300.0, 0.0),
	};
	const TCHAR* HorizontalDirectionNames[] =
	{
		TEXT("Forward"),
		TEXT("Backward"),
		TEXT("Right"),
		TEXT("Left"),
	};
	for (int32 DirectionIndex = 0; DirectionIndex < UE_ARRAY_COUNT(HorizontalDirectionCases); ++DirectionIndex)
	{
		FRpgAnimInstanceProxy DirectionProxy;
		DirectionProxy.bTurnInPlaceHardReset = false;
		DirectionProxy.MovementState = ERpgLocomotionMovementState::Airborne;
		DirectionProxy.bIsFalling = true;
		DirectionProxy.WorldVelocity = HorizontalDirectionCases[DirectionIndex] + FVector(0.0, 0.0, -400.0);
		DirectionProxy.VerticalVelocity = -400.0f;
		URpgAnimInstance::UpdateLandingSelectionSnapshot(DirectionProxy, 0.8f, GravityAcceleration);
		TestTrue(
			*FString::Printf(TEXT("%s horizontal vector is preserved"), HorizontalDirectionNames[DirectionIndex]),
			DirectionProxy.LandingSelectionSnapshot.HorizontalVelocity.Equals(
				HorizontalDirectionCases[DirectionIndex]));
		TestTrue(
			*FString::Printf(TEXT("%s horizontal speed is direction independent"), HorizontalDirectionNames[DirectionIndex]),
			FMath::IsNearlyEqual(DirectionProxy.LandingSelectionSnapshot.HorizontalSpeed, 300.0f));
	}

	// Holding movement input before touchdown preserves raw intent and its inferred gait without
	// manufacturing horizontal speed. The physical zero-speed landing therefore remains Stand.
	FRpgAnimInstanceProxy AirborneIntentProxy;
	AirborneIntentProxy.bTurnInPlaceHardReset = false;
	AirborneIntentProxy.MovementState = ERpgLocomotionMovementState::Airborne;
	AirborneIntentProxy.bIsFalling = true;
	AirborneIntentProxy.WorldVelocity = FVector(0.0, 0.0, -500.0);
	AirborneIntentProxy.VerticalVelocity = -500.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(
		AirborneIntentProxy,
		1.0f,
		GravityAcceleration);
	TestTrue(
		TEXT("Airborne W input is retained as raw landing intent"),
		AirborneIntentProxy.LandingSelectionSnapshot.bHasMoveIntent);
	TestEqual(
		TEXT("Airborne W input retains its inferred Run gait context"),
		AirborneIntentProxy.LandingSelectionSnapshot.Gait,
		ERpgLocomotionGait::Run);
	TestTrue(
		TEXT("Airborne W input does not manufacture horizontal landing speed"),
		FMath::IsNearlyZero(AirborneIntentProxy.LandingSelectionSnapshot.HorizontalSpeed));
	TestResolvedRole(
		TEXT("Captured airborne W at zero horizontal speed resolves Stand Light"),
		AirborneIntentProxy.LandingSelectionSnapshot,
		ERpgMotionMatchingDatabaseRole::StandLightLanding);

	AirborneIntentProxy.TrajectoryLandingPrediction.LandingLocation = FVector::ZeroVector;
	AirborneIntentProxy.TrajectoryLandingPrediction.LandingNormal = FVector::UpVector;
	AirborneIntentProxy.TrajectoryLandingPrediction.TimeToLand = 0.2f;
	AirborneIntentProxy.TrajectoryLandingPrediction.bIsValid = true;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(
		AirborneIntentProxy,
		1.0f,
		GravityAcceleration);
	TestTrue(
		TEXT("Heavy airborne W capture still retains raw landing intent"),
		AirborneIntentProxy.LandingSelectionSnapshot.bHasMoveIntent);
	TestEqual(
		TEXT("Heavy airborne W capture still retains Run gait context"),
		AirborneIntentProxy.LandingSelectionSnapshot.Gait,
		ERpgLocomotionGait::Run);
	TestResolvedRole(
		TEXT("Captured airborne W at zero horizontal speed resolves Stand Heavy"),
		AirborneIntentProxy.LandingSelectionSnapshot,
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding);

	FRpgAnimInstanceProxy UpwardProxy;
	UpwardProxy.bTurnInPlaceHardReset = false;
	UpwardProxy.MovementState = ERpgLocomotionMovementState::Airborne;
	UpwardProxy.bIsFalling = true;
	UpwardProxy.WorldVelocity = FVector(300.0, 0.0, 900.0);
	UpwardProxy.VerticalVelocity = 900.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(UpwardProxy, 0.8f, GravityAcceleration);
	TestTrue(TEXT("An upward capture is valid"), UpwardProxy.LandingSelectionSnapshot.bIsValid);
	TestTrue(
		TEXT("Upward velocity captures zero measured downward speed"),
		FMath::IsNearlyZero(UpwardProxy.LandingSelectionSnapshot.MaximumDownwardSpeed));
	TestEqual(
		TEXT("An upward capture resolves Light rather than Heavy"),
		RpgLandingRuntime::ResolveDatabaseRole(
			UpwardProxy.LandingSelectionSnapshot,
			DefaultTuning),
		ERpgMotionMatchingDatabaseRole::RunLightLanding);

	FRpgAnimInstanceProxy InvalidInputProxy;
	InvalidInputProxy.bTurnInPlaceHardReset = false;
	InvalidInputProxy.MovementState = ERpgLocomotionMovementState::Airborne;
	InvalidInputProxy.bIsFalling = true;
	InvalidInputProxy.WorldVelocity = FVector(Infinity, 0.0, -400.0);
	InvalidInputProxy.VerticalVelocity = -400.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(InvalidInputProxy, 0.8f, GravityAcceleration);
	TestFalse(TEXT("Infinite capture inputs invalidate the snapshot"), InvalidInputProxy.LandingSelectionSnapshot.bIsValid);
	TestEqual(TEXT("Invalid capture retains the current airborne epoch"), InvalidInputProxy.LandingSelectionSnapshot.AirborneEpoch, 1);

	// One airborne epoch accumulates descent, relaunch resets it, and touchdown freezes one frame.
	FRpgAnimInstanceProxy LifecycleProxy;
	LifecycleProxy.bTurnInPlaceHardReset = false;
	LifecycleProxy.MovementState = ERpgLocomotionMovementState::Grounded;
	LifecycleProxy.bIsMovingOnGround = true;
	LifecycleProxy.Gait = ERpgLocomotionGait::Walk;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(LifecycleProxy, 0.0f, GravityAcceleration);
	TestFalse(TEXT("Grounded movement without prior air has no landing snapshot"), LifecycleProxy.LandingSelectionSnapshot.bIsValid);

	LifecycleProxy.MovementState = ERpgLocomotionMovementState::Airborne;
	LifecycleProxy.bIsMovingOnGround = false;
	LifecycleProxy.bIsFalling = true;
	LifecycleProxy.WorldVelocity = FVector(150.0, 0.0, 450.0);
	LifecycleProxy.VerticalVelocity = 450.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(LifecycleProxy, 0.0f, GravityAcceleration);
	const int32 FirstAirborneEpoch = LifecycleProxy.LandingSelectionSnapshot.AirborneEpoch;
	TestEqual(TEXT("The first launch opens airborne epoch one"), FirstAirborneEpoch, 1);
	TestEqual(TEXT("Grounded Walk gait survives launch"), LifecycleProxy.LandingSelectionSnapshot.Gait, ERpgLocomotionGait::Walk);

	LifecycleProxy.WorldVelocity.Z = -600.0;
	LifecycleProxy.VerticalVelocity = -600.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(LifecycleProxy, 0.0f, GravityAcceleration);
	TestEqual(TEXT("Descent remains in the same airborne epoch"), LifecycleProxy.LandingSelectionSnapshot.AirborneEpoch, FirstAirborneEpoch);
	TestTrue(
		TEXT("The current epoch accumulates measured descent"),
		FMath::IsNearlyEqual(LifecycleProxy.LandingSelectionSnapshot.MaximumDownwardSpeed, 600.0f));

	LifecycleProxy.WorldVelocity.Z = 350.0;
	LifecycleProxy.VerticalVelocity = 350.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(LifecycleProxy, 0.0f, GravityAcceleration);
	const int32 RelaunchEpoch = LifecycleProxy.LandingSelectionSnapshot.AirborneEpoch;
	TestEqual(TEXT("An upward relaunch opens a fresh airborne epoch"), RelaunchEpoch, FirstAirborneEpoch + 1);
	TestTrue(
		TEXT("Relaunch discards descent from the prior epoch"),
		FMath::IsNearlyZero(LifecycleProxy.LandingSelectionSnapshot.MaximumDownwardSpeed));

	LifecycleProxy.WorldVelocity.Z = -800.0;
	LifecycleProxy.VerticalVelocity = -800.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(LifecycleProxy, 0.0f, GravityAcceleration);
	TestEqual(TEXT("Post-relaunch descent stays in the new epoch"), LifecycleProxy.LandingSelectionSnapshot.AirborneEpoch, RelaunchEpoch);
	TestEqual(
		TEXT("The new epoch owns only its own measured descent"),
		LifecycleProxy.LandingSelectionSnapshot.MaximumDownwardSpeed,
		800.0f);
	const FRpgLandingSelectionSnapshot FinalAirborneSnapshot = LifecycleProxy.LandingSelectionSnapshot;

	LifecycleProxy.MovementState = ERpgLocomotionMovementState::Grounded;
	LifecycleProxy.bIsMovingOnGround = true;
	LifecycleProxy.bIsFalling = false;
	LifecycleProxy.WorldVelocity = FVector::ZeroVector;
	LifecycleProxy.VerticalVelocity = 0.0f;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(LifecycleProxy, 0.0f, GravityAcceleration);
	TestSnapshotParity(
		TEXT("The first physical touchdown freezes the final airborne snapshot"),
		LifecycleProxy.LandingSelectionSnapshot,
		FinalAirborneSnapshot);
	URpgAnimInstance::UpdateLandingSelectionSnapshot(LifecycleProxy, 0.0f, GravityAcceleration);
	TestFalse(TEXT("The second grounded frame clears landing validity"), LifecycleProxy.LandingSelectionSnapshot.bIsValid);
	TestEqual(TEXT("The second grounded frame clears the frozen epoch"), LifecycleProxy.LandingSelectionSnapshot.AirborneEpoch, 0);

	// Identical movement data resolves identically for authority, owner, simulated proxy, and late join.
	auto CaptureNetworkSnapshot = [&](bool bHasGroundedHistory)
	{
		FRpgAnimInstanceProxy Proxy;
		Proxy.bTurnInPlaceHardReset = false;
		if (bHasGroundedHistory)
		{
			Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
			Proxy.bIsMovingOnGround = true;
			Proxy.Gait = ERpgLocomotionGait::Run;
			URpgAnimInstance::UpdateLandingSelectionSnapshot(Proxy, 0.0f, GravityAcceleration);
		}

		Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
		Proxy.bIsMovingOnGround = false;
		Proxy.bIsFalling = true;
		Proxy.WorldVelocity = FVector(250.0, -150.0, -650.0);
		Proxy.VerticalVelocity = -650.0f;
		Proxy.TrajectoryLandingPrediction.LandingLocation = FVector(400.0, 100.0, 0.0);
		Proxy.TrajectoryLandingPrediction.LandingNormal = FVector::UpVector;
		Proxy.TrajectoryLandingPrediction.TimeToLand = 0.1f;
		Proxy.TrajectoryLandingPrediction.bIsValid = true;
		URpgAnimInstance::UpdateLandingSelectionSnapshot(Proxy, 0.0f, GravityAcceleration);
		return Proxy.LandingSelectionSnapshot;
	};
	const FRpgLandingSelectionSnapshot NetworkSnapshots[] =
	{
		CaptureNetworkSnapshot(true),
		CaptureNetworkSnapshot(true),
		CaptureNetworkSnapshot(true),
		CaptureNetworkSnapshot(false),
	};
	const TCHAR* NetworkRoleNames[] =
	{
		TEXT("Authority"),
		TEXT("Autonomous proxy"),
		TEXT("Simulated proxy"),
		TEXT("Late join"),
	};
	for (int32 RoleIndex = 0; RoleIndex < UE_ARRAY_COUNT(NetworkSnapshots); ++RoleIndex)
	{
		TestSnapshotParity(
			FString::Printf(TEXT("%s snapshot parity"), NetworkRoleNames[RoleIndex]),
			NetworkSnapshots[RoleIndex],
			NetworkSnapshots[0]);
		TestEqual(
			*FString::Printf(TEXT("%s resolves the same Run Heavy role"), NetworkRoleNames[RoleIndex]),
			RpgLandingRuntime::ResolveDatabaseRole(
				NetworkSnapshots[RoleIndex],
				DefaultTuning),
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	}

	// The extracted value runtime owns epoch wrap, current-frame prediction, request serials,
	// bounded stationary handoff, and timeout math independently of UObject/AnimNode state.
	FRpgLandingCaptureState WrappedCaptureState;
	WrappedCaptureState.LastGroundedGait = ERpgLocomotionGait::Walk;
	WrappedCaptureState.AirborneEpoch = MAX_int32;
	FRpgLandingCaptureSnapshot WrappedCaptureInput;
	WrappedCaptureInput.MovementState = ERpgLocomotionMovementState::Airborne;
	WrappedCaptureInput.Gait = ERpgLocomotionGait::Walk;
	WrappedCaptureInput.WorldVelocity = FVector(120.0, 0.0, -300.0);
	WrappedCaptureInput.VerticalVelocity = -300.0f;
	WrappedCaptureInput.GravityAcceleration = GravityAcceleration;
	WrappedCaptureInput.bIsFalling = true;
	FRpgLandingSelectionSnapshot WrappedSelectionSnapshot;
	RpgLandingRuntime::UpdateSelectionSnapshot(
		WrappedSelectionSnapshot,
		WrappedCaptureState,
		WrappedCaptureInput);
	TestEqual(
		TEXT("Airborne epoch skips zero after int32 wrap"),
		WrappedCaptureState.AirborneEpoch,
		1);
	TestEqual(
		TEXT("Wrapped epoch is published to the frozen snapshot"),
		WrappedSelectionSnapshot.AirborneEpoch,
		1);

	WrappedCaptureInput.TrajectoryPrediction.LandingLocation = FVector(50.0, 0.0, 0.0);
	WrappedCaptureInput.TrajectoryPrediction.LandingNormal = FVector::UpVector;
	WrappedCaptureInput.TrajectoryPrediction.TimeToLand = 0.2f;
	WrappedCaptureInput.TrajectoryPrediction.bIsValid = true;
	RpgLandingRuntime::UpdateSelectionSnapshot(
		WrappedSelectionSnapshot,
		WrappedCaptureState,
		WrappedCaptureInput);
	TestTrue(
		TEXT("A current valid trajectory prediction is captured"),
		WrappedSelectionSnapshot.PredictedLanding.bIsValid);
	WrappedCaptureInput.TrajectoryPrediction = FRpgTrajectoryLandingPrediction();
	RpgLandingRuntime::UpdateSelectionSnapshot(
		WrappedSelectionSnapshot,
		WrappedCaptureState,
		WrappedCaptureInput);
	TestFalse(
		TEXT("A later all-miss frame clears stale trajectory prediction"),
		WrappedSelectionSnapshot.PredictedLanding.bIsValid);
	TestTrue(
		TEXT("A later all-miss frame clears stale predicted impact speed"),
		FMath::IsNearlyZero(WrappedSelectionSnapshot.PredictedImpactDownwardSpeed));
	TestTrue(
		TEXT("Measured downward speed remains accumulated across prediction misses"),
		FMath::IsNearlyEqual(WrappedSelectionSnapshot.MaximumDownwardSpeed, 300.0f));

	const FRpgLandingSelectionSnapshot ContradictoryFinalAirborne = WrappedSelectionSnapshot;
	WrappedCaptureInput.MovementState = ERpgLocomotionMovementState::Grounded;
	WrappedCaptureInput.bIsMovingOnGround = true;
	WrappedCaptureInput.bIsFalling = true;
	RpgLandingRuntime::UpdateSelectionSnapshot(
		WrappedSelectionSnapshot,
		WrappedCaptureState,
		WrappedCaptureInput);
	TestSnapshotParity(
		TEXT("Supported grounded capture wins contradictory falling flags"),
		WrappedSelectionSnapshot,
		ContradictoryFinalAirborne);
	TestFalse(
		TEXT("Contradictory grounded capture still closes the airborne edge"),
		WrappedCaptureState.bWasAirborne);

	FRpgLandingDatabaseAvailability AllLandingDatabases;
	AllLandingDatabases.bStandLight = true;
	AllLandingDatabases.bStandHeavy = true;
	AllLandingDatabases.bWalkLight = true;
	AllLandingDatabases.bWalkHeavy = true;
	AllLandingDatabases.bRunLight = true;
	AllLandingDatabases.bRunHeavy = true;

	FRpgLandingRuntimeState WrappedRequestState;
	WrappedRequestState.RequestSerial = MAX_uint32;
	WrappedRequestState.TouchdownElapsed = 0.2f;
	FRpgLandingRuntimeResult WrappedRequest = RpgLandingRuntime::BeginRequest(
		WrappedRequestState,
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
		true);
	TestEqual(TEXT("Landing request serial skips zero after uint32 wrap"), WrappedRequest.State.RequestSerial, 1u);
	TestTrue(TEXT("An initial request resets touchdown age"), FMath::IsNearlyZero(WrappedRequest.State.TouchdownElapsed));
	TestEqual(
		TEXT("An initial request emits the reflected phase intent"),
		WrappedRequest.Transition,
		ERpgLandingRuntimeTransition::BeginLanding);
	TestTrue(
		TEXT("The initial landing ForceInterrupt is consumed exactly once"),
		RpgLandingRuntime::ConsumeForceInterrupt(true, true, WrappedRequest.State));
	TestFalse(
		TEXT("The same landing request cannot consume ForceInterrupt twice"),
		RpgLandingRuntime::ConsumeForceInterrupt(true, true, WrappedRequest.State));

	FRpgLandingRuntimeState HandoffState;
	HandoffState.ActiveRole = ERpgMotionMatchingDatabaseRole::StandHeavyLanding;
	HandoffState.TouchdownElapsed = 0.29f;
	HandoffState.StateElapsed = 0.2f;
	HandoffState.RequestSerial = 41u;
	FRpgLandingActiveSnapshot HandoffSnapshot;
	HandoffSnapshot.Eligibility.MovementState = ERpgLocomotionMovementState::Grounded;
	HandoffSnapshot.Eligibility.bIsMovingOnGround = true;
	HandoffSnapshot.Availability = AllLandingDatabases;
	HandoffSnapshot.LiveGait = ERpgLocomotionGait::Run;
	HandoffSnapshot.GroundSpeed = 450.0f;
	HandoffSnapshot.bChooserMoving = true;
	const FRpgLandingRuntimeResult HandoffResult = RpgLandingRuntime::UpdateActive(
		HandoffState,
		HandoffSnapshot,
		0.01f,
		DefaultTuning);
	TestEqual(
		TEXT("The inclusive handoff window preserves Heavy severity in the Run domain"),
		HandoffResult.State.ActiveRole,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	TestEqual(TEXT("A stationary-to-moving handoff advances the request serial"), HandoffResult.State.RequestSerial, 42u);
	TestEqual(
		TEXT("A database-change handoff pre-consumes ForceInterrupt"),
		HandoffResult.State.InterruptedRequestSerial,
		HandoffResult.State.RequestSerial);
	TestTrue(
		TEXT("A handoff preserves physical touchdown age"),
		FMath::IsNearlyEqual(HandoffResult.State.TouchdownElapsed, 0.3f, 0.0001f));

	FRpgLandingRuntimeState LateHandoffState = HandoffState;
	LateHandoffState.TouchdownElapsed = DefaultTuning.LandingMovementHandoffWindow;
	const FRpgLandingRuntimeResult LateHandoffResult = RpgLandingRuntime::UpdateActive(
		LateHandoffState,
		HandoffSnapshot,
		0.01f,
		DefaultTuning);
	TestEqual(
		TEXT("Movement after the handoff window exits to Grounded"),
		LateHandoffResult.Transition,
		ERpgLandingRuntimeTransition::ResetGrounded);

	FRpgGaspLocomotionTuning ShortHandoffTuning = DefaultTuning;
	ShortHandoffTuning.LandingMovementHandoffWindow = 0.1f;
	FRpgLandingRuntimeState CustomHandoffState = HandoffState;
	CustomHandoffState.TouchdownElapsed = 0.1f;
	TestEqual(
		TEXT("A custom landing handoff window is inclusive"),
		RpgLandingRuntime::UpdateActive(
			CustomHandoffState,
			HandoffSnapshot,
			0.0f,
			ShortHandoffTuning).Transition,
		ERpgLandingRuntimeTransition::BeginLanding);

	FRpgLandingRuntimeState FrozenMovingState;
	FrozenMovingState.ActiveRole = ERpgMotionMatchingDatabaseRole::RunHeavyLanding;
	FrozenMovingState.PlaybackWatchdogDuration = DefaultTuning.LandingActiveTimeout;
	FRpgLandingActiveSnapshot FrozenMovingSnapshot = HandoffSnapshot;
	FrozenMovingSnapshot.LiveGait = ERpgLocomotionGait::Walk;
	FrozenMovingSnapshot.GroundSpeed = 100.0f;
	const FRpgLandingRuntimeResult FrozenMovingResult = RpgLandingRuntime::UpdateActive(
		FrozenMovingState,
		FrozenMovingSnapshot,
		0.1f,
		DefaultTuning);
	TestEqual(
		TEXT("An active moving landing role remains frozen across live gait changes"),
		FrozenMovingResult.State.ActiveRole,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);

	FRpgLandingRuntimeState SelectionTimeoutState = FrozenMovingState;
	SelectionTimeoutState.StateElapsed = DefaultTuning.LandingSelectionTimeout;
	SelectionTimeoutState.bSelectionLatched = false;
	TestEqual(
		TEXT("Selection timeout is inclusive"),
		RpgLandingRuntime::UpdateActive(
			SelectionTimeoutState,
			FrozenMovingSnapshot,
			0.0f,
			DefaultTuning).Transition,
		ERpgLandingRuntimeTransition::ResetGrounded);
	FRpgLandingRuntimeState PlaybackTimeoutState = FrozenMovingState;
	PlaybackTimeoutState.StateElapsed = DefaultTuning.LandingActiveTimeout;
	PlaybackTimeoutState.PlaybackWatchdogDuration = DefaultTuning.LandingActiveTimeout;
	PlaybackTimeoutState.bSelectionLatched = true;
	TestEqual(
		TEXT("Playback watchdog timeout is inclusive"),
		RpgLandingRuntime::UpdateActive(
			PlaybackTimeoutState,
			FrozenMovingSnapshot,
			0.0f,
			DefaultTuning).Transition,
		ERpgLandingRuntimeTransition::ResetGrounded);

	TestTrue(
		TEXT("Looping landing playback uses the bounded active timeout"),
		FMath::IsNearlyEqual(
			RpgLandingRuntime::CalculatePlaybackWatchdogDuration(0.2f, 1.0f, true, DefaultTuning),
			DefaultTuning.LandingActiveTimeout));
	TestTrue(
		TEXT("Paused landing playback uses the bounded active timeout"),
		FMath::IsNearlyEqual(
			RpgLandingRuntime::CalculatePlaybackWatchdogDuration(0.2f, 0.0f, false, DefaultTuning),
			DefaultTuning.LandingActiveTimeout));
	TestTrue(
		TEXT("Negative play rate uses its absolute playback duration"),
		FMath::IsNearlyEqual(
			RpgLandingRuntime::CalculatePlaybackWatchdogDuration(0.4f, -2.0f, false, DefaultTuning),
			0.3f));
	TestTrue(
		TEXT("Landing watchdog clamps short playback to its safety margin"),
		FMath::IsNearlyEqual(
			RpgLandingRuntime::CalculatePlaybackWatchdogDuration(0.0f, 1.0f, false, DefaultTuning),
			RpgLandingRuntime::PlaybackWatchdogSafetyMargin));
	TestTrue(
		TEXT("Landing watchdog clamps long playback to its active timeout"),
		FMath::IsNearlyEqual(
			RpgLandingRuntime::CalculatePlaybackWatchdogDuration(10.0f, 1.0f, false, DefaultTuning),
			DefaultTuning.LandingActiveTimeout));
	TestTrue(
		TEXT("Non-finite play rate fails over to the bounded active timeout"),
		FMath::IsNearlyEqual(
			RpgLandingRuntime::CalculatePlaybackWatchdogDuration(0.2f, QuietNaN, false, DefaultTuning),
			DefaultTuning.LandingActiveTimeout));

	// Missing Heavy content falls back only to the same gait's Light slot, then to None.
	USkeletalMeshComponent* AnimInstanceOuter = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(AnimInstanceOuter);
	UPoseSearchDatabase* StandLightDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* StandHeavyDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkLightDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkHeavyDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunLightDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunHeavyDatabase = NewObject<UPoseSearchDatabase>();
	if (!TestNotNull(TEXT("Transient RPG AnimInstance exists"), AnimInstance) ||
		!TestNotNull(TEXT("Stand Light fallback fixture exists"), StandLightDatabase) ||
		!TestNotNull(TEXT("Stand Heavy fallback fixture exists"), StandHeavyDatabase) ||
		!TestNotNull(TEXT("Walk Light fallback fixture exists"), WalkLightDatabase) ||
		!TestNotNull(TEXT("Walk Heavy fallback fixture exists"), WalkHeavyDatabase) ||
		!TestNotNull(TEXT("Run Light fallback fixture exists"), RunLightDatabase) ||
		!TestNotNull(TEXT("Run Heavy fallback fixture exists"), RunHeavyDatabase))
	{
		return false;
	}
	AnimInstance->HeavyLandingSpeedThreshold = DefaultTuning.HeavyLandingSpeedThreshold;
	AnimInstance->LandingMotionMatchingDatabase = StandLightDatabase;
	AnimInstance->StandHeavyLandingMotionMatchingDatabase = StandHeavyDatabase;
	AnimInstance->WalkLightLandingMotionMatchingDatabase = WalkLightDatabase;
	AnimInstance->WalkHeavyLandingMotionMatchingDatabase = WalkHeavyDatabase;
	AnimInstance->RunLightLandingMotionMatchingDatabase = RunLightDatabase;
	AnimInstance->RunHeavyLandingMotionMatchingDatabase = RunHeavyDatabase;
	AnimInstance->GroundMotionMatchingDatabaseSets.Idle[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Walk[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Walk[1] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[1] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[2] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[3] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Sprint[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Sprint[1] = NewObject<UPoseSearchDatabase>();
	AnimInstance->CrouchingMotionMatchingDatabase = NewObject<UPoseSearchDatabase>();
	AnimInstance->TurnInPlaceMotionMatchingDatabase = NewObject<UPoseSearchDatabase>();
	AnimInstance->AirborneMotionMatchingDatabases.Add(NewObject<UPoseSearchDatabase>());
	AnimInstance->InitializeGaspRuntimeConfiguration();
	if (!TestNotNull(
		TEXT("The complete unique legacy facade initializes its atomic runtime cache"),
		AnimInstance->GetMotionMatchingDatabaseForRole(
			ERpgMotionMatchingDatabaseRole::StandIdle)))
	{
		return false;
	}

	FRpgLandingDatabaseAvailability CompleteAvailability;
	CompleteAvailability.bStandLight = true;
	CompleteAvailability.bStandHeavy = true;
	CompleteAvailability.bWalkLight = true;
	CompleteAvailability.bWalkHeavy = true;
	CompleteAvailability.bRunLight = true;
	CompleteAvailability.bRunHeavy = true;

	const FRpgLandingSelectionSnapshot StandHeavySnapshot =
		MakeSnapshot(ERpgLocomotionGait::Idle, 0.0f, false, 700.0f, 0.0f);
	TestEqual(
		TEXT("Configured Stand Heavy content is selected directly"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(StandHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding);
	FRpgLandingDatabaseAvailability StandFallbackAvailability = CompleteAvailability;
	StandFallbackAvailability.bStandHeavy = false;
	TestEqual(
		TEXT("Missing Stand Heavy falls back to Stand Light"),
		RpgLandingRuntime::ResolveAvailableRole(
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			StandFallbackAvailability),
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	StandFallbackAvailability.bStandLight = false;
	TestEqual(
		TEXT("Missing Stand Heavy and Stand Light falls back to None"),
		RpgLandingRuntime::ResolveAvailableRole(
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			StandFallbackAvailability),
		ERpgMotionMatchingDatabaseRole::None);

	const FRpgLandingSelectionSnapshot WalkHeavySnapshot =
		MakeSnapshot(ERpgLocomotionGait::Walk, 200.0f, true, 700.0f, 0.0f);
	TestEqual(
		TEXT("Configured Walk Heavy content is selected directly"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(WalkHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding);
	FRpgLandingDatabaseAvailability WalkFallbackAvailability = CompleteAvailability;
	WalkFallbackAvailability.bWalkHeavy = false;
	TestEqual(
		TEXT("Missing Walk Heavy falls back to Walk Light"),
		RpgLandingRuntime::ResolveAvailableRole(
			ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
			WalkFallbackAvailability),
		ERpgMotionMatchingDatabaseRole::WalkLightLanding);
	WalkFallbackAvailability.bWalkLight = false;
	TestEqual(
		TEXT("Missing Walk Heavy and Walk Light falls back to None"),
		RpgLandingRuntime::ResolveAvailableRole(
			ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
			WalkFallbackAvailability),
		ERpgMotionMatchingDatabaseRole::None);

	const FRpgLandingSelectionSnapshot RunHeavySnapshot =
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 0.0f);
	TestEqual(
		TEXT("Configured Run Heavy content is selected directly"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(RunHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	FRpgLandingDatabaseAvailability RunFallbackAvailability = CompleteAvailability;
	RunFallbackAvailability.bRunHeavy = false;
	TestEqual(
		TEXT("Missing Run Heavy falls back to Run Light"),
		RpgLandingRuntime::ResolveAvailableRole(
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
			RunFallbackAvailability),
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	RunFallbackAvailability.bRunLight = false;
	TestEqual(
		TEXT("Missing Run Heavy and Run Light falls back to None"),
		RpgLandingRuntime::ResolveAvailableRole(
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
			RunFallbackAvailability),
		ERpgMotionMatchingDatabaseRole::None);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
