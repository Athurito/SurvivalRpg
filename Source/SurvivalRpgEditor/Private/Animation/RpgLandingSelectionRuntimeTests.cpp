// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLandingSelectionRuntimeTest,
	"SurvivalRpg.Animation.Jump.Runtime.LandingSelection",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgLandingSelectionRuntimeTest::RunTest(const FString& Parameters)
{
	constexpr float HeavySpeedThreshold = 700.0f;
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
	auto TestResolvedRole = [this, HeavySpeedThreshold](
		const TCHAR* Context,
		const FRpgLandingSelectionSnapshot& Snapshot,
		ERpgMotionMatchingDatabaseRole ExpectedRole)
	{
		TestEqual(
			Context,
			URpgAnimInstance::ResolveLandingDatabaseRole(Snapshot, HeavySpeedThreshold),
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
	TestEqual(
		TEXT("A NaN Heavy threshold fails closed"),
		URpgAnimInstance::ResolveLandingDatabaseRole(
			MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 0.0f),
			QuietNaN),
		ERpgMotionMatchingDatabaseRole::None);
	TestEqual(
		TEXT("An infinite Heavy threshold fails closed"),
		URpgAnimInstance::ResolveLandingDatabaseRole(
			MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 0.0f),
			Infinity),
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
		URpgAnimInstance::ResolveLandingDatabaseRole(
			UpwardProxy.LandingSelectionSnapshot,
			HeavySpeedThreshold),
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
			URpgAnimInstance::ResolveLandingDatabaseRole(
				NetworkSnapshots[RoleIndex],
				HeavySpeedThreshold),
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	}

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
	AnimInstance->HeavyLandingSpeedThreshold = HeavySpeedThreshold;
	AnimInstance->LandingMotionMatchingDatabase = StandLightDatabase;
	AnimInstance->StandHeavyLandingMotionMatchingDatabase = StandHeavyDatabase;
	AnimInstance->WalkLightLandingMotionMatchingDatabase = WalkLightDatabase;
	AnimInstance->WalkHeavyLandingMotionMatchingDatabase = WalkHeavyDatabase;
	AnimInstance->RunLightLandingMotionMatchingDatabase = RunLightDatabase;
	AnimInstance->RunHeavyLandingMotionMatchingDatabase = RunHeavyDatabase;

	const FRpgLandingSelectionSnapshot StandHeavySnapshot =
		MakeSnapshot(ERpgLocomotionGait::Idle, 0.0f, false, 700.0f, 0.0f);
	TestEqual(
		TEXT("Configured Stand Heavy content is selected directly"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(StandHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding);
	AnimInstance->StandHeavyLandingMotionMatchingDatabase = nullptr;
	TestEqual(
		TEXT("Missing Stand Heavy falls back to Stand Light"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(StandHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	AnimInstance->LandingMotionMatchingDatabase = nullptr;
	TestEqual(
		TEXT("Missing Stand Heavy and Stand Light falls back to None"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(StandHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::None);
	AnimInstance->LandingMotionMatchingDatabase = StandLightDatabase;
	AnimInstance->StandHeavyLandingMotionMatchingDatabase = StandHeavyDatabase;

	const FRpgLandingSelectionSnapshot WalkHeavySnapshot =
		MakeSnapshot(ERpgLocomotionGait::Walk, 200.0f, true, 700.0f, 0.0f);
	TestEqual(
		TEXT("Configured Walk Heavy content is selected directly"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(WalkHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding);
	AnimInstance->WalkHeavyLandingMotionMatchingDatabase = nullptr;
	TestEqual(
		TEXT("Missing Walk Heavy falls back to Walk Light"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(WalkHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::WalkLightLanding);
	AnimInstance->WalkLightLandingMotionMatchingDatabase = nullptr;
	TestEqual(
		TEXT("Missing Walk Heavy and Walk Light falls back to None"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(WalkHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::None);
	AnimInstance->WalkLightLandingMotionMatchingDatabase = WalkLightDatabase;
	AnimInstance->WalkHeavyLandingMotionMatchingDatabase = WalkHeavyDatabase;

	const FRpgLandingSelectionSnapshot RunHeavySnapshot =
		MakeSnapshot(ERpgLocomotionGait::Run, 450.0f, true, 700.0f, 0.0f);
	TestEqual(
		TEXT("Configured Run Heavy content is selected directly"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(RunHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	AnimInstance->RunHeavyLandingMotionMatchingDatabase = nullptr;
	TestEqual(
		TEXT("Missing Run Heavy falls back to Run Light"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(RunHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	AnimInstance->RunLightLandingMotionMatchingDatabase = nullptr;
	TestEqual(
		TEXT("Missing Run Heavy and Run Light falls back to None"),
		AnimInstance->ResolveAvailableLandingDatabaseRole(RunHeavySnapshot),
		ERpgMotionMatchingDatabaseRole::None);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
