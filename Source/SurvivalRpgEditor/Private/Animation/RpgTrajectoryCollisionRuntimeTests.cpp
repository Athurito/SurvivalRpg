// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgPoseSearchTrajectory.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTrajectoryCollisionRuntimeTest,
	"SurvivalRpg.Animation.Trajectory.CollisionAndTimeToLand",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgTrajectoryCollisionRuntimeTest::RunTest(const FString& Parameters)
{
	FRpgTrajectoryCollisionSettings DefaultSettings;
	TestTrue(TEXT("Trajectory collision is enabled by default for opted-in GASP AnimBPs"), DefaultSettings.bEnabled);
	TestTrue(TEXT("The project preserves GASP's 0.01 cm floor offset"), FMath::IsNearlyEqual(DefaultSettings.FloorOffset, 0.01f));
	TestTrue(TEXT("The project preserves GASP's bounded 150 cm floor search"), FMath::IsNearlyEqual(DefaultSettings.MaxObstacleHeight, 150.0f));
	TestEqual(TEXT("The trace budget is capped to the generated GASP future horizon"), DefaultSettings.MaxPredictionSamples, 15);
	TestEqual(TEXT("Trajectory collision uses Visibility like GASP"), DefaultSettings.TraceChannel.GetValue(), ECC_Visibility);
	TestFalse(TEXT("The cosmetic hot path defaults to simple collision"), DefaultSettings.bTraceComplex);

	const auto MakeRawTrajectory = [](int32 PredictionSampleCount)
	{
		FTransformTrajectory Trajectory;
		for (int32 SampleIndex = 0; SampleIndex <= PredictionSampleCount; ++SampleIndex)
		{
			FTransformTrajectorySample& Sample = Trajectory.Samples.AddDefaulted_GetRef();
			Sample.TimeInSeconds = 0.01f + static_cast<float>(SampleIndex) * 0.1f;
			Sample.Position = FVector(
				static_cast<double>(SampleIndex) * 60.0,
				0.0,
				100.0);
			Sample.Facing = FQuat::Identity;
		}
		return Trajectory;
	};
	const auto MakeFastDescendingTrajectory = [&MakeRawTrajectory](int32 PredictionSampleCount)
	{
		FTransformTrajectory Trajectory = MakeRawTrajectory(PredictionSampleCount);
		for (int32 SampleIndex = 0; SampleIndex < Trajectory.Samples.Num(); ++SampleIndex)
		{
			// 200 cm per 0.1-second sample models a -2000 cm/s CMC fall, deliberately
			// faster than the configured 150 cm vertical search window.
			Trajectory.Samples[SampleIndex].Position.Z =
				200.0 - static_cast<double>(SampleIndex) * 200.0;
		}
		return Trajectory;
	};
	const auto MakeBlockingHit = [](
		float HitTime,
		const FVector& ImpactPoint,
		const FVector& ImpactNormal)
	{
		FHitResult HitResult;
		HitResult.bBlockingHit = true;
		HitResult.Time = HitTime;
		HitResult.ImpactPoint = ImpactPoint;
		HitResult.ImpactNormal = ImpactNormal.GetSafeNormal();
		HitResult.Normal = HitResult.ImpactNormal;
		return HitResult;
	};

	FTransformTrajectory FiniteTrajectory;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FTransformTrajectorySample& Sample = FiniteTrajectory.Samples.AddDefaulted_GetRef();
		Sample.TimeInSeconds = static_cast<float>(Index - 1) * 0.1f;
		Sample.Position = FVector(static_cast<double>(Index) * 10.0, 0.0, 100.0);
		Sample.Facing = FQuat::Identity;
	}
	TestTrue(TEXT("A finite ordered trajectory passes the worker-snapshot contract"), RpgPoseSearchTrajectory::IsTransformTrajectoryFinite(FiniteTrajectory));
	TestFalse(TEXT("An empty trajectory fails closed"), RpgPoseSearchTrajectory::IsTransformTrajectoryFinite(FTransformTrajectory()));

	FTransformTrajectory UnsortedTrajectory = FiniteTrajectory;
	UnsortedTrajectory.Samples[2].TimeInSeconds = -1.0f;
	TestFalse(TEXT("A non-monotonic sample timeline fails closed"), RpgPoseSearchTrajectory::IsTransformTrajectoryFinite(UnsortedTrajectory));

	FTransformTrajectory NanTrajectory = FiniteTrajectory;
	NanTrajectory.Samples[1].Position.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("A non-finite sample position fails closed"), RpgPoseSearchTrajectory::IsTransformTrajectoryFinite(NanTrajectory));

	const float MidpointLandingTime = RpgPoseSearchTrajectory::CalculateLandingTime(
		0.0f,
		0.2f,
		FVector(0.0, 0.0, 100.0),
		FVector(0.0, 0.0, -100.0),
		FVector::ZeroVector,
		FVector::UpVector);
	TestTrue(TEXT("First contact is interpolated between bounding ballistic samples"), FMath::IsNearlyEqual(MidpointLandingTime, 0.1f));
	TestEqual(
		TEXT("Invalid ballistic input cannot manufacture a landing time"),
		RpgPoseSearchTrajectory::CalculateLandingTime(
			0.2f,
			0.1f,
			FVector::ZeroVector,
			FVector::ZeroVector,
			FVector::ZeroVector,
			FVector::UpVector),
		-1.0f);

	const FRpgTrajectoryLandingPrediction ValidPrediction =
		RpgPoseSearchTrajectory::MakeLandingPrediction(
			true,
			true,
			false,
			0.4f,
			1.5f,
			FVector(100.0, 25.0, 0.0),
			FVector(0.0, 0.0, 2.0));
	TestTrue(TEXT("A finite airborne walkable hit publishes a prediction"), ValidPrediction.bIsValid);
	TestTrue(TEXT("Published landing normals are normalized"), ValidPrediction.LandingNormal.IsNormalized());
	TestTrue(TEXT("Published TimeToLand remains finite"), FMath::IsFinite(ValidPrediction.TimeToLand));

	const FRpgTrajectoryLandingPrediction NoHitPrediction =
		RpgPoseSearchTrajectory::MakeLandingPrediction(
			true,
			false,
			false,
			1.5f,
			1.5f,
			FVector::ZeroVector,
			FVector::UpVector);
	TestFalse(TEXT("No collision is explicitly invalid instead of becoming the UE helper horizon"), NoHitPrediction.bIsValid);
	TestEqual(TEXT("Invalid predictions keep the -1 second sentinel"), NoHitPrediction.TimeToLand, -1.0f);

	TestFalse(
		TEXT("A teleport/owner-role hard reset rejects even a fresh-looking hit"),
		RpgPoseSearchTrajectory::MakeLandingPrediction(
			true,
			true,
			true,
			0.2f,
			1.5f,
			FVector::ZeroVector,
			FVector::UpVector).bIsValid);
	TestFalse(
		TEXT("Grounded state cannot publish predictive landing data"),
		RpgPoseSearchTrajectory::MakeLandingPrediction(
			false,
			true,
			false,
			0.0f,
			1.5f,
			FVector::ZeroVector,
			FVector::UpVector).bIsValid);
	TestFalse(
		TEXT("A hit beyond the inspected horizon fails closed"),
		RpgPoseSearchTrajectory::MakeLandingPrediction(
			true,
			true,
			false,
			1.6f,
			1.5f,
			FVector::ZeroVector,
			FVector::UpVector).bIsValid);

	const FVector GravityAcceleration(0.0, 0.0, -980.0);
	const FTransformTrajectory FlatStepRawTrajectory = MakeRawTrajectory(2);
	const TArray<FHitResult> FlatStepHits = {
		MakeBlockingHit(0.5f, FVector(30.0, 0.0, 95.55), FVector::UpVector),
		MakeBlockingHit(0.25f, FVector(120.0, 0.0, 110.0), FVector::UpVector),
	};
	const TArray<bool> FlatStepWalkability = {true, true};
	FTransformTrajectory FlatStepCorrectedTrajectory;
	int32 FlatStepQueryCount = 0;
	const FRpgTrajectoryLandingPrediction FlatStepPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			FlatStepRawTrajectory,
			FlatStepHits,
			FlatStepWalkability,
			FlatStepCorrectedTrajectory,
			FlatStepQueryCount);
	TestTrue(TEXT("The production resolver publishes its first walkable flat-floor hit"), FlatStepPrediction.bIsValid);
	TestTrue(TEXT("First-contact TimeToLand uses the sweep fraction"), FMath::IsNearlyEqual(FlatStepPrediction.TimeToLand, 0.05f));
	TestEqual(TEXT("Each inspected sample performs at most one collision query"), FlatStepQueryCount, 2);
	TestTrue(
		TEXT("A contact inside the first interval does not rewind the later sample horizontally"),
		FMath::IsNearlyEqual(
			FlatStepCorrectedTrajectory.Samples[1].Position.X,
			FlatStepRawTrajectory.Samples[1].Position.X));
	TestTrue(
		TEXT("The flat sample remains above its validated floor"),
		FlatStepCorrectedTrajectory.Samples[1].Position.Z > FlatStepHits[0].ImpactPoint.Z);
	TestTrue(
		TEXT("A later bounded vertical sweep follows a walkable step"),
		FMath::IsNearlyEqual(
			FlatStepCorrectedTrajectory.Samples[2].Position.Z,
			FlatStepHits[1].ImpactPoint.Z + DefaultSettings.FloorOffset));

	const double InverseSqrtTwo = FMath::InvSqrt(2.0);
	const FVector RampNormal(-InverseSqrtTwo, 0.0, InverseSqrtTwo);
	const FVector RampContactCenter(30.0, 0.0, 97.55);
	const FVector RampImpactPoint =
		RampContactCenter - RampNormal * DefaultSettings.SweepRadius;
	const FTransformTrajectory RampRawTrajectory = MakeRawTrajectory(1);
	const TArray<FHitResult> RampHits = {
		MakeBlockingHit(0.5f, RampImpactPoint, RampNormal),
	};
	const TArray<bool> RampWalkability = {true};
	FTransformTrajectory RampCorrectedTrajectory;
	int32 RampQueryCount = 0;
	const FRpgTrajectoryLandingPrediction RampPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			RampRawTrajectory,
			RampHits,
			RampWalkability,
			RampCorrectedTrajectory,
			RampQueryCount);
	const double RampSignedDistance = FVector::DotProduct(
		RampCorrectedTrajectory.Samples[1].Position - RampImpactPoint,
		RampNormal);
	const double ExpectedRampClearance =
		DefaultSettings.FloorOffset * FVector::DotProduct(FVector::UpVector, RampNormal);
	TestTrue(TEXT("The production resolver accepts a walkable ramp hit"), RampPrediction.bIsValid);
	TestEqual(TEXT("The ramp uses one bounded sample query"), RampQueryCount, 1);
	TestTrue(
		TEXT("Sphere contact is projected onto the ramp plane without penetration"),
		FMath::IsNearlyEqual(RampSignedDistance, ExpectedRampClearance, 0.001));
	TestTrue(
		TEXT("Ramp correction preserves the sample-end horizontal position"),
		FMath::IsNearlyEqual(
			RampCorrectedTrajectory.Samples[1].Position.X,
			RampRawTrajectory.Samples[1].Position.X));

	const FTransformTrajectory GroundedSurfaceRawTrajectory = MakeRawTrajectory(3);
	const FVector GroundedRampImpactPoint(120.0, 0.0, 5.0);
	const TArray<FHitResult> GroundedSurfaceHits = {
		MakeBlockingHit(0.4f, FVector(60.0, 0.0, 0.0), FVector::UpVector),
		MakeBlockingHit(0.5f, GroundedRampImpactPoint, RampNormal),
		MakeBlockingHit(0.6f, FVector(180.0, 0.0, 20.0), FVector::UpVector),
	};
	const TArray<bool> GroundedSurfaceWalkability = {true, true, true};
	FTransformTrajectory GroundedSurfaceCorrectedTrajectory;
	int32 GroundedSurfaceQueryCount = 0;
	TArray<FVector> GroundedTraceStarts;
	TArray<FVector> GroundedTraceEnds;
	const FRpgTrajectoryLandingPrediction GroundedSurfacePrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			false,
			GroundedSurfaceRawTrajectory,
			GroundedSurfaceHits,
			GroundedSurfaceWalkability,
			GroundedSurfaceCorrectedTrajectory,
			GroundedSurfaceQueryCount,
			&GroundedTraceStarts,
			&GroundedTraceEnds);
	TestFalse(TEXT("Grounded correction cannot publish an airborne landing prediction"), GroundedSurfacePrediction.bIsValid);
	TestEqual(TEXT("Grounded flat, ramp, and step each use one bounded query"), GroundedSurfaceQueryCount, 3);
	TestEqual(TEXT("Grounded tests capture every trace start"), GroundedTraceStarts.Num(), 3);
	TestEqual(TEXT("Grounded tests capture every trace end"), GroundedTraceEnds.Num(), 3);
	for (int32 QueryIndex = 0; QueryIndex < GroundedSurfaceQueryCount; ++QueryIndex)
	{
		TestTrue(
			*FString::Printf(TEXT("Grounded query %d preserves its endpoint XY"), QueryIndex),
			FMath::IsNearlyEqual(
				GroundedTraceStarts[QueryIndex].X,
				GroundedTraceEnds[QueryIndex].X) &&
			FMath::IsNearlyEqual(
				GroundedTraceStarts[QueryIndex].Y,
				GroundedTraceEnds[QueryIndex].Y));
		TestTrue(
			*FString::Printf(TEXT("Grounded query %d starts exactly 150 cm above its endpoint"), QueryIndex),
			FMath::IsNearlyEqual(
				GroundedTraceStarts[QueryIndex].Z - GroundedTraceEnds[QueryIndex].Z,
				DefaultSettings.MaxObstacleHeight,
				0.001));
	}
	TestTrue(
		TEXT("Grounded flat correction retains its configured floor clearance"),
		FMath::IsNearlyEqual(
			GroundedSurfaceCorrectedTrajectory.Samples[1].Position.Z,
			DefaultSettings.FloorOffset));
	const double GroundedRampSignedDistance = FVector::DotProduct(
		GroundedSurfaceCorrectedTrajectory.Samples[2].Position - GroundedRampImpactPoint,
		RampNormal);
	TestTrue(
		TEXT("Grounded ramp correction stays above its hit plane"),
		FMath::IsNearlyEqual(
			GroundedRampSignedDistance,
			ExpectedRampClearance,
			0.001));
	TestTrue(
		TEXT("Grounded step correction follows the raised floor"),
		FMath::IsNearlyEqual(
			GroundedSurfaceCorrectedTrajectory.Samples[3].Position.Z,
			20.0 + DefaultSettings.FloorOffset));

	const FHitResult FastFallContactHit =
		MakeBlockingHit(0.98f, FVector(58.8, 0.0, 0.0), FVector::UpVector);
	const FTransformTrajectory FastFloorRawTrajectory = MakeFastDescendingTrajectory(3);
	const TArray<FHitResult> FastFloorHits = {
		FastFallContactHit,
		MakeBlockingHit(0.03f, FVector(120.0, 0.0, 0.0), FVector::UpVector),
		MakeBlockingHit(0.03f, FVector(180.0, 0.0, 0.0), FVector::UpVector),
	};
	const TArray<bool> FastFloorWalkability = {true, true, true};
	FTransformTrajectory FastFloorCorrectedTrajectory;
	int32 FastFloorQueryCount = 0;
	TArray<FVector> FastFloorTraceStarts;
	TArray<FVector> FastFloorTraceEnds;
	const FRpgTrajectoryLandingPrediction FastFloorPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			FastFloorRawTrajectory,
			FastFloorHits,
			FastFloorWalkability,
			FastFloorCorrectedTrajectory,
			FastFloorQueryCount,
			&FastFloorTraceStarts,
			&FastFloorTraceEnds);
	TestTrue(TEXT("A fast fall publishes its first walkable floor contact"), FastFloorPrediction.bIsValid);
	TestEqual(TEXT("The fast-floor scenario stays within three queries"), FastFloorQueryCount, 3);
	TestTrue(
		TEXT("Post-contact correction removes the raw -2000 cm/s vertical velocity"),
		FastFloorTraceEnds[1].Z > -10.0);
	TestTrue(
		TEXT("The next 150 cm floor sweep still starts above the accepted floor"),
		FastFloorTraceStarts[1].Z > 100.0);
	for (int32 SampleIndex = 1; SampleIndex <= 3; ++SampleIndex)
	{
		TestTrue(
			*FString::Printf(TEXT("Fast falling flat-floor sample %d never tunnels underground"), SampleIndex),
			FMath::IsNearlyEqual(
				FastFloorCorrectedTrajectory.Samples[SampleIndex].Position.Z,
				DefaultSettings.FloorOffset,
				0.001));
	}

	const FTransformTrajectory EdgeRawTrajectory = MakeFastDescendingTrajectory(3);
	const TArray<FHitResult> EdgeHits = {
		FastFallContactHit,
		FHitResult(),
		FHitResult(),
	};
	const TArray<bool> EdgeWalkability = {true, false, false};
	FTransformTrajectory EdgeCorrectedTrajectory;
	int32 EdgeQueryCount = 0;
	TArray<FVector> EdgeTraceStarts;
	TArray<FVector> EdgeTraceEnds;
	const FRpgTrajectoryLandingPrediction EdgePrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			EdgeRawTrajectory,
			EdgeHits,
			EdgeWalkability,
			EdgeCorrectedTrajectory,
			EdgeQueryCount,
			&EdgeTraceStarts,
			&EdgeTraceEnds);
	TestTrue(TEXT("An airborne edge scenario retains its first valid contact prediction"), EdgePrediction.bIsValid);
	TestEqual(TEXT("The edge scenario remains within its three-sample query budget"), EdgeQueryCount, 3);
	TestTrue(
		TEXT("First miss beyond an accepted edge restarts from zero vertical speed"),
		FMath::IsNearlyEqual(
			EdgeCorrectedTrajectory.Samples[2].Position.Z,
			DefaultSettings.FloorOffset - 4.9,
			0.001));
	TestTrue(
		TEXT("Second miss preserves the post-contact freefall origin without raw-Z re-entry"),
		FMath::IsNearlyEqual(
			EdgeCorrectedTrajectory.Samples[3].Position.Z,
			DefaultSettings.FloorOffset - 19.6,
			0.001));
	TestTrue(
		TEXT("The first edge miss remains reachable by the bounded 150 cm vertical sweep"),
		EdgeTraceStarts[1].Z > 100.0 && EdgeTraceEnds[1].Z > -10.0);

	const FTransformTrajectory WalkabilityRawTrajectory = MakeRawTrajectory(2);
	const TArray<FHitResult> WalkabilityHits = {
		MakeBlockingHit(0.25f, FVector(15.0, 0.0, 98.0), FVector::ForwardVector),
		MakeBlockingHit(0.5f, FVector(90.0, 0.0, 88.0), FVector::UpVector),
	};
	const TArray<bool> WalkabilityResults = {false, true};
	FTransformTrajectory WalkabilityCorrectedTrajectory;
	int32 WalkabilityQueryCount = 0;
	const FRpgTrajectoryLandingPrediction WalkabilityPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			WalkabilityRawTrajectory,
			WalkabilityHits,
			WalkabilityResults,
			WalkabilityCorrectedTrajectory,
			WalkabilityQueryCount);
	TestEqual(TEXT("An unwalkable first blocker does not end the bounded search"), WalkabilityQueryCount, 2);
	TestTrue(TEXT("The first later walkable hit becomes the prediction"), WalkabilityPrediction.bIsValid);
	TestEqual(TEXT("The unwalkable blocker never becomes landing truth"), WalkabilityPrediction.LandingLocation, WalkabilityHits[1].ImpactPoint);
	TestTrue(TEXT("The later first-walkable hit keeps its interpolated time"), FMath::IsNearlyEqual(WalkabilityPrediction.TimeToLand, 0.15f));

	FHitResult FarContactHit = MakeBlockingHit(0.8f, FVector(48.0, 0.0, 96.0), FVector::UpVector);
	FHitResult NearContactHit = FarContactHit;
	NearContactHit.Time = 0.2f;
	FTransformTrajectory FarContactTrajectory;
	FTransformTrajectory NearContactTrajectory;
	int32 FarContactQueryCount = 0;
	int32 NearContactQueryCount = 0;
	const FRpgTrajectoryLandingPrediction FarContactPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			MakeRawTrajectory(1),
			{FarContactHit},
			{true},
			FarContactTrajectory,
			FarContactQueryCount);
	const FRpgTrajectoryLandingPrediction NearContactPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			MakeRawTrajectory(1),
			{NearContactHit},
			{true},
			NearContactTrajectory,
			NearContactQueryCount);
	TestTrue(
		TEXT("Recomputed TimeToLand trends toward zero as contact approaches"),
		NearContactPrediction.TimeToLand < FarContactPrediction.TimeToLand);

	const FTransformTrajectory BudgetRawTrajectory = MakeRawTrajectory(20);
	FTransformTrajectory BudgetCorrectedTrajectory;
	int32 BudgetQueryCount = 0;
	const FRpgTrajectoryLandingPrediction BudgetPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			DefaultSettings,
			GravityAcceleration,
			true,
			BudgetRawTrajectory,
			{},
			{},
			BudgetCorrectedTrajectory,
			BudgetQueryCount);
	TestFalse(TEXT("A bounded all-miss search publishes no landing"), BudgetPrediction.bIsValid);
	TestEqual(TEXT("The resolver never exceeds the 15-query GASP horizon"), BudgetQueryCount, 15);
	TestEqual(
		TEXT("Samples beyond the query budget remain untouched"),
		BudgetCorrectedTrajectory.Samples[16].Position,
		BudgetRawTrajectory.Samples[16].Position);

	FRpgTrajectoryCollisionSettings InvalidBoundsSettings = DefaultSettings;
	InvalidBoundsSettings.SweepRadius = 21.0f;
	FTransformTrajectory InvalidBoundsTrajectory;
	int32 InvalidBoundsQueryCount = -1;
	const FRpgTrajectoryLandingPrediction InvalidBoundsPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			InvalidBoundsSettings,
			GravityAcceleration,
			true,
			MakeRawTrajectory(1),
			{FarContactHit},
			{true},
			InvalidBoundsTrajectory,
			InvalidBoundsQueryCount);
	TestFalse(TEXT("A migrated out-of-range sweep radius fails closed"), InvalidBoundsPrediction.bIsValid);
	TestEqual(TEXT("Invalid geometry cannot issue a world query"), InvalidBoundsQueryCount, 0);

	FRpgTrajectoryCollisionSettings InvalidChannelSettings = DefaultSettings;
	InvalidChannelSettings.TraceChannel = static_cast<ECollisionChannel>(ECC_MAX);
	FTransformTrajectory InvalidChannelTrajectory;
	int32 InvalidChannelQueryCount = -1;
	const FRpgTrajectoryLandingPrediction InvalidChannelPrediction =
		RpgPoseSearchTrajectory::ResolveCollisionForTest(
			InvalidChannelSettings,
			GravityAcceleration,
			true,
			MakeRawTrajectory(1),
			{FarContactHit},
			{true},
			InvalidChannelTrajectory,
			InvalidChannelQueryCount);
	TestFalse(TEXT("A non-serialized collision channel fails closed"), InvalidChannelPrediction.bIsValid);
	TestEqual(TEXT("An invalid channel cannot alias into a world query"), InvalidChannelQueryCount, 0);

	for (TFieldIterator<FProperty> PropertyIt(FRpgTrajectoryLandingPrediction::StaticStruct()); PropertyIt; ++PropertyIt)
	{
		TestNull(
			*FString::Printf(TEXT("Prediction field %s is pointer-free"), *PropertyIt->GetName()),
			CastField<FObjectPropertyBase>(*PropertyIt));
	}

	USkeletalMeshComponent* AnimInstanceOuter = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(AnimInstanceOuter);
	const auto MakeUniqueLegacyDatabase = []()
	{
		return NewObject<UPoseSearchDatabase>();
	};
	for (TObjectPtr<UPoseSearchDatabase>& Database : AnimInstance->GroundMotionMatchingDatabaseSets.Idle)
	{
		Database = MakeUniqueLegacyDatabase();
	}
	for (TObjectPtr<UPoseSearchDatabase>& Database : AnimInstance->GroundMotionMatchingDatabaseSets.Walk)
	{
		Database = MakeUniqueLegacyDatabase();
	}
	for (TObjectPtr<UPoseSearchDatabase>& Database : AnimInstance->GroundMotionMatchingDatabaseSets.Run)
	{
		Database = MakeUniqueLegacyDatabase();
	}
	for (TObjectPtr<UPoseSearchDatabase>& Database : AnimInstance->GroundMotionMatchingDatabaseSets.Sprint)
	{
		Database = MakeUniqueLegacyDatabase();
	}
	AnimInstance->CrouchingMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->TurnInPlaceMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->AirborneMotionMatchingDatabases.Add(MakeUniqueLegacyDatabase());
	AnimInstance->LandingMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->StandHeavyLandingMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->WalkLightLandingMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->WalkHeavyLandingMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->RunLightLandingMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->RunHeavyLandingMotionMatchingDatabase = MakeUniqueLegacyDatabase();
	AnimInstance->HeavyLandingSpeedThreshold = 700.0f;
	AnimInstance->InitializeGaspRuntimeConfiguration();
	FRpgAnimInstanceProxy Proxy;
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsFalling = true;
	Proxy.bIsMovingOnGround = false;
	Proxy.bTurnInPlaceHardReset = false;
	Proxy.WorldVelocity = FVector(300.0, 0.0, -400.0);
	Proxy.VerticalVelocity = -400.0f;
	Proxy.TrajectoryLandingPrediction = ValidPrediction;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(Proxy, ERpgLocomotionGait::Run, GravityAcceleration);
	AnimInstance->ResetJumpPhaseRuntime();
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A predictive hit leaves the physical jump phase Airborne"), AnimInstance->JumpPhase, ERpgJumpPhase::Airborne);
	TestTrue(
		TEXT("Prediction can classify a sub-threshold measured fall as Heavy"),
		Proxy.LandingSelectionSnapshot.MaximumDownwardSpeed < AnimInstance->HeavyLandingSpeedThreshold &&
		Proxy.LandingSelectionSnapshot.PredictedImpactDownwardSpeed >= AnimInstance->HeavyLandingSpeedThreshold);
	TestEqual(
		TEXT("Even a predicted Heavy impact never creates an early landing request"),
		AnimInstance->LandingRequestSerial,
		0u);

	for (int32 DirectionIndex = 0; DirectionIndex < 4; ++DirectionIndex)
	{
		Proxy.WorldVelocity = FVector(
			DirectionIndex == 0 ? 300.0 : DirectionIndex == 1 ? -300.0 : 0.0,
			DirectionIndex == 2 ? 300.0 : DirectionIndex == 3 ? -300.0 : 0.0,
			-400.0);
		Proxy.VerticalVelocity = -400.0f;
		Proxy.TrajectoryLandingPrediction.TimeToLand = 0.3f - 0.05f * DirectionIndex;
		URpgAnimInstance::UpdateLandingSelectionSnapshot(Proxy, ERpgLocomotionGait::Run, GravityAcceleration);
		AnimInstance->UpdateJumpPhaseRuntime(0.05f, Proxy);
	}
	TestEqual(TEXT("Directional airborne predictions cannot restart or double-request landing"), AnimInstance->LandingRequestSerial, 0u);

	// Restore a predicted Heavy final-airborne frame. Prediction may stabilize the
	// cosmetic severity selection, but the request still waits for physical CMC touchdown.
	Proxy.WorldVelocity = FVector(300.0, 0.0, -400.0);
	Proxy.VerticalVelocity = -400.0f;
	Proxy.TrajectoryLandingPrediction = ValidPrediction;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(Proxy, ERpgLocomotionGait::Run, GravityAcceleration);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("The final predicted Heavy airborne frame still creates no request"), AnimInstance->LandingRequestSerial, 0u);

	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsFalling = false;
	Proxy.bIsMovingOnGround = true;
	Proxy.GroundSpeed = 300.0f;
	Proxy.bHasGroundedMoveIntent = true;
	Proxy.TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
	URpgAnimInstance::UpdateLandingSelectionSnapshot(Proxy, ERpgLocomotionGait::Run, GravityAcceleration);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Only physical CharacterMovement touchdown enters Landing"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(
		TEXT("Physical touchdown consumes predicted impact only as Run Heavy severity"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	TestEqual(TEXT("Physical touchdown creates exactly one request"), AnimInstance->LandingRequestSerial, 1u);
	URpgAnimInstance::UpdateLandingSelectionSnapshot(Proxy, ERpgLocomotionGait::Run, GravityAcceleration);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A repeated grounded snapshot cannot duplicate the request"), AnimInstance->LandingRequestSerial, 1u);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
