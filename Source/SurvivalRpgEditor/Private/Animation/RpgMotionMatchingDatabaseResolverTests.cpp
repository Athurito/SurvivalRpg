// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgMotionMatchingRuntime.h"
#include "SurvivalRpg/Animation/RpgPoseSearchTrajectory.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgMotionMatchingDatabaseResolverTest,
	"SurvivalRpg.Animation.MotionMatching.GroundDatabaseResolver",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgMotionMatchingDatabaseResolverTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Trajectory history uses GASP's adaptive sampling interval"),
		FMath::IsNearlyEqual(RpgPoseSearchTrajectory::HistorySamplingInterval, -1.0f));
	TestEqual(
		TEXT("Trajectory history keeps GASP's 30 samples"),
		RpgPoseSearchTrajectory::HistorySampleCount,
		30);
	TestTrue(
		TEXT("Trajectory prediction samples at GASP's 0.1 second interval"),
		FMath::IsNearlyEqual(RpgPoseSearchTrajectory::PredictionSamplingInterval, 0.1f));
	TestEqual(
		TEXT("Trajectory prediction keeps GASP's 15 samples"),
		RpgPoseSearchTrajectory::PredictionSampleCount,
		15);
	TestEqual(
		TEXT("Sprint Stops begin at GASP's inclusive 550 cm/s boundary"),
		RpgMotionMatchingRuntime::SprintStopMinimumSpeed,
		550.0f);

	FRpgGroundMotionMatchingDatabaseSets DatabaseSets;
	TestEqual(TEXT("Idle has one fixed database slot"), DatabaseSets.Idle.Num(), 1);
	TestEqual(TEXT("Walk has Moving Aggregate and Stops slots"), DatabaseSets.Walk.Num(), 2);
	TestEqual(TEXT("Run has four fixed database slots"), DatabaseSets.Run.Num(), 4);
	TestEqual(TEXT("Sprint has Moving Aggregate and Stops slots"), DatabaseSets.Sprint.Num(), 2);

	UPoseSearchDatabase* Idle = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkMoving = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkStops = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunLoops = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunPivots = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunStarts = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunStops = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* SprintMoving = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* SprintStops = NewObject<UPoseSearchDatabase>();
	if (!TestNotNull(TEXT("Idle database fixture exists"), Idle) ||
		!TestNotNull(TEXT("Walk Moving database fixture exists"), WalkMoving) ||
		!TestNotNull(TEXT("Walk Stops database fixture exists"), WalkStops) ||
		!TestNotNull(TEXT("Run Loops database fixture exists"), RunLoops) ||
		!TestNotNull(TEXT("Run Pivots database fixture exists"), RunPivots) ||
		!TestNotNull(TEXT("Run Starts database fixture exists"), RunStarts) ||
		!TestNotNull(TEXT("Run Stops database fixture exists"), RunStops) ||
		!TestNotNull(TEXT("Sprint Moving database fixture exists"), SprintMoving) ||
		!TestNotNull(TEXT("Sprint Stops database fixture exists"), SprintStops))
	{
		return false;
	}

	DatabaseSets.Idle[0] = Idle;
	DatabaseSets.Walk[0] = WalkMoving;
	DatabaseSets.Walk[1] = WalkStops;
	DatabaseSets.Run[0] = RunLoops;
	DatabaseSets.Run[1] = RunPivots;
	DatabaseSets.Run[2] = RunStarts;
	DatabaseSets.Run[3] = RunStops;
	DatabaseSets.Sprint[0] = SprintMoving;
	DatabaseSets.Sprint[1] = SprintStops;

	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation ValidContract =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(DatabaseSets);
	TestTrue(TEXT("The complete 1/2/4/2 database contract is valid"), ValidContract.IsValid());

	auto MakeMovingSnapshot = [](ERpgLocomotionGait Gait)
	{
		FRpgGroundMotionMatchingSelectionSnapshot Snapshot;
		Snapshot.Gait = Gait;
		Snapshot.Stance = ERpgLocomotionStance::Standing;
		Snapshot.MovementState = ERpgLocomotionMovementState::Grounded;
		Snapshot.WorldVelocity = FVector(450.0f, 0.0f, 0.0f);
		Snapshot.WorldAcceleration = FVector(2048.0f, 0.0f, 0.0f);
		Snapshot.GroundSpeed = 450.0f;
		Snapshot.FutureVelocity = FVector(450.0f, 0.0f, 0.0f);
		Snapshot.CurrentDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
		Snapshot.bIsMovingOnGround = true;
		return Snapshot;
	};
	auto DirectionAtDegrees = [](float Degrees)
	{
		return FVector(
			FMath::Cos(FMath::DegreesToRadians(Degrees)),
			FMath::Sin(FMath::DegreesToRadians(Degrees)),
			0.0f) * 2048.0f;
	};
	auto TestRoleSequence = [this](
		const TCHAR* Context,
		const FRpgResolvedMotionMatchingDatabaseRoles& ActualRoles,
		std::initializer_list<ERpgMotionMatchingDatabaseRole> ExpectedRoles)
	{
		TestEqual(
			*FString::Printf(TEXT("%s role count"), Context),
			ActualRoles.Num(),
			static_cast<int32>(ExpectedRoles.size()));
		int32 RoleIndex = 0;
		for (const ERpgMotionMatchingDatabaseRole ExpectedRole : ExpectedRoles)
		{
			if (!ActualRoles.IsValidIndex(RoleIndex))
			{
				break;
			}
			TestEqual(
				*FString::Printf(TEXT("%s role %d"), Context, RoleIndex),
				static_cast<uint8>(ActualRoles[RoleIndex]),
				static_cast<uint8>(ExpectedRole));
			++RoleIndex;
		}
	};

	const FRpgGroundMotionMatchingSelectionSnapshot IdleSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Idle);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases IdleResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(IdleSnapshot, DatabaseSets);
	TestEqual(TEXT("Idle resolves exactly one database"), IdleResult.Num(), 1);
	TestEqual(TEXT("Idle preserves its authored database"), IdleResult[0], Idle);
	TestRoleSequence(
		TEXT("Moving Idle"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(IdleSnapshot),
		{ERpgMotionMatchingDatabaseRole::StandIdle});

	const FRpgGroundMotionMatchingSelectionSnapshot WalkSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Walk);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkSnapshot, DatabaseSets);
	TestEqual(TEXT("Walk resolves exactly one database"), WalkResult.Num(), 1);
	TestEqual(TEXT("Moving Walk preserves its aggregate database"), WalkResult[0], WalkMoving);
	TestRoleSequence(
		TEXT("Moving Walk"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(WalkSnapshot),
		{ERpgMotionMatchingDatabaseRole::StandWalk});

	FRpgGroundMotionMatchingSelectionSnapshot SteadyRunSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Run);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases SteadyRunResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SteadyRunSnapshot, DatabaseSets);
	TestEqual(TEXT("A source-aligned steady Free Run resolves only Loops"), SteadyRunResult.Num(), 1);
	TestEqual(TEXT("Steady Run preserves Loops"), SteadyRunResult[0], RunLoops);
	TestRoleSequence(
		TEXT("Steady Run"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(SteadyRunSnapshot),
		{ERpgMotionMatchingDatabaseRole::StandRunLoops});

	FRpgGroundMotionMatchingSelectionSnapshot FastStartSnapshot = SteadyRunSnapshot;
	FastStartSnapshot.GroundSpeed = 350.0f;
	FastStartSnapshot.WorldVelocity = FVector(350.0f, 0.0f, 0.0f);
	FastStartSnapshot.FutureVelocity = FVector(
		FastStartSnapshot.GroundSpeed + RpgMotionMatchingRuntime::RunStartMinimumFutureSpeedGain,
		0.0f,
		0.0f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases FastStartResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(FastStartSnapshot, DatabaseSets);
	TestEqual(TEXT("Starts remain eligible above the removed local 150 cm/s cap"), FastStartResult.Num(), 2);
	TestEqual(TEXT("Source result order places Starts before Loops"), FastStartResult[0], RunStarts);
	TestEqual(TEXT("Starts preserve Loops as the continuing fallback"), FastStartResult[1], RunLoops);
	TestRoleSequence(
		TEXT("Starting Run"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(FastStartSnapshot),
		{
			ERpgMotionMatchingDatabaseRole::StandRunStarts,
			ERpgMotionMatchingDatabaseRole::StandRunLoops,
		});

	FRpgGroundMotionMatchingSelectionSnapshot BelowStartGainSnapshot = FastStartSnapshot;
	BelowStartGainSnapshot.FutureVelocity.X -= 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases BelowStartGainResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(BelowStartGainSnapshot, DatabaseSets);
	TestEqual(TEXT("Below the exact +100 cm/s Start gate Run remains Loops-only"), BelowStartGainResult.Num(), 1);

	FRpgGroundMotionMatchingSelectionSnapshot FreeBelowPivotSnapshot = SteadyRunSnapshot;
	FreeBelowPivotSnapshot.WorldAcceleration = DirectionAtDegrees(
		RpgMotionMatchingRuntime::FreeRunPivotMinimumAngle - 0.01f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases FreeBelowPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(FreeBelowPivotSnapshot, DatabaseSets);
	TestFalse(TEXT("Free mode excludes a turn just below 45 degrees"), FreeBelowPivotResult.Contains(RunPivots));

	FRpgGroundMotionMatchingSelectionSnapshot FreePivotSnapshot = SteadyRunSnapshot;
	FreePivotSnapshot.WorldAcceleration = DirectionAtDegrees(RpgMotionMatchingRuntime::FreeRunPivotMinimumAngle);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases FreePivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(FreePivotSnapshot, DatabaseSets);
	TestEqual(TEXT("Free mode exposes Pivots at exactly 45 degrees"), FreePivotResult.Num(), 2);
	TestEqual(TEXT("Free Pivot keeps Loops first when no Start is eligible"), FreePivotResult[0], RunLoops);
	TestEqual(TEXT("Free Pivot appends Pivots"), FreePivotResult[1], RunPivots);

	FRpgGroundMotionMatchingSelectionSnapshot StrafeBelowPivotSnapshot = SteadyRunSnapshot;
	StrafeBelowPivotSnapshot.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	StrafeBelowPivotSnapshot.WorldAcceleration = DirectionAtDegrees(
		RpgMotionMatchingRuntime::CombatStrafeRunPivotMinimumAngle - 0.01f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases StrafeBelowPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StrafeBelowPivotSnapshot, DatabaseSets);
	TestFalse(TEXT("Combat Strafe excludes a turn just below 30 degrees"), StrafeBelowPivotResult.Contains(RunPivots));

	FRpgGroundMotionMatchingSelectionSnapshot StrafePivotSnapshot = SteadyRunSnapshot;
	StrafePivotSnapshot.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	StrafePivotSnapshot.WorldAcceleration = DirectionAtDegrees(
		RpgMotionMatchingRuntime::CombatStrafeRunPivotMinimumAngle);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases StrafePivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StrafePivotSnapshot, DatabaseSets);
	TestTrue(TEXT("Combat Strafe exposes Pivots at exactly 30 degrees"), StrafePivotResult.Contains(RunPivots));

	FRpgGroundMotionMatchingSelectionSnapshot AimPivotSnapshot = SteadyRunSnapshot;
	AimPivotSnapshot.RotationMode = ERpgCharacterRotationMode::Aim;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AimPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AimPivotSnapshot, DatabaseSets);
	TestTrue(TEXT("Aim exposes Pivots at its exact zero-degree threshold"), AimPivotResult.Contains(RunPivots));

	FRpgGroundMotionMatchingSelectionSnapshot AimWithoutAccelerationSnapshot = AimPivotSnapshot;
	AimWithoutAccelerationSnapshot.WorldAcceleration = FVector::ZeroVector;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AimWithoutAccelerationResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AimWithoutAccelerationSnapshot, DatabaseSets);
	TestFalse(TEXT("Aim still requires GASP logical Moving before exposing Pivots"), AimWithoutAccelerationResult.Contains(RunPivots));

	FRpgGroundMotionMatchingSelectionSnapshot TinyAccelerationSnapshot = SteadyRunSnapshot;
	TinyAccelerationSnapshot.WorldAcceleration = FVector(0.05f, 0.0f, 0.0f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases TinyAccelerationResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(TinyAccelerationSnapshot, DatabaseSets);
	TestEqual(
		TEXT("GASP's 1e-4 acceleration tolerance keeps a finite 0.05 cm/s2 input in Moving"),
		TinyAccelerationResult[0],
		RunLoops);

	// A stationary landing can release on the first grounded input frame while CharacterMovement still
	// carries a small floor-projected Z remainder. Only horizontal velocity may open the moving Run domain.
	constexpr double MaxAcceleration = 2400.0;
	const FVector FullForwardAcceleration(MaxAcceleration, 0.0, 0.0);
	FRpgReplicatedAcceleration PackedForwardAcceleration;
	PackedForwardAcceleration.SetFromAcceleration(FullForwardAcceleration, MaxAcceleration);
	const FVector ReplicatedForwardAcceleration =
		PackedForwardAcceleration.ToAcceleration(MaxAcceleration);
	const FVector NetworkAccelerations[] =
	{
		FullForwardAcceleration,
		FullForwardAcceleration,
		ReplicatedForwardAcceleration,
		ReplicatedForwardAcceleration,
	};
	static const TCHAR* const LandingHandoffNetworkViews[] =
	{
		TEXT("Authority"),
		TEXT("Autonomous Proxy"),
		TEXT("Simulated Proxy"),
		TEXT("Late Join Simulated Proxy"),
	};

	for (int32 NetworkViewIndex = 0;
		 NetworkViewIndex < UE_ARRAY_COUNT(LandingHandoffNetworkViews);
		 ++NetworkViewIndex)
	{
		const TCHAR* NetworkView = LandingHandoffNetworkViews[NetworkViewIndex];
		FRpgGroundMotionMatchingSelectionSnapshot VerticalOnlySnapshot;
		VerticalOnlySnapshot.Gait = ERpgLocomotionGait::Run;
		VerticalOnlySnapshot.Stance = ERpgLocomotionStance::Standing;
		VerticalOnlySnapshot.MovementState = ERpgLocomotionMovementState::Grounded;
		VerticalOnlySnapshot.WorldVelocity = FVector(
			0.0f,
			0.0f,
			RpgMotionMatchingRuntime::ChooserVelocityTolerance + 0.01f);
		VerticalOnlySnapshot.WorldAcceleration = NetworkAccelerations[NetworkViewIndex];
		VerticalOnlySnapshot.GroundSpeed = VerticalOnlySnapshot.WorldVelocity.Size2D();
		VerticalOnlySnapshot.FutureVelocity = FVector(
			RpgMotionMatchingRuntime::RunStartMinimumFutureSpeedGain,
			0.0f,
			0.0f);
		VerticalOnlySnapshot.CurrentDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
		VerticalOnlySnapshot.bIsMovingOnGround = true;

		TestFalse(
			*FString::Printf(TEXT("%s ignores vertical-only grounded velocity for logical Moving"), NetworkView),
			RpgMotionMatchingRuntime::IsChooserMoving(VerticalOnlySnapshot));
		TestRoleSequence(
			*FString::Printf(TEXT("%s vertical-only landing handoff"), NetworkView),
			RpgMotionMatchingRuntime::ResolveDatabaseRoles(VerticalOnlySnapshot),
			{ERpgMotionMatchingDatabaseRole::StandIdle});
		const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases VerticalOnlyResult =
			URpgAnimInstance::ResolveGroundMotionMatchingDatabases(VerticalOnlySnapshot, DatabaseSets);
		TestEqual(
			*FString::Printf(TEXT("%s vertical-only landing handoff resolves one database"), NetworkView),
			VerticalOnlyResult.Num(),
			1);
		if (VerticalOnlyResult.Num() == 1)
		{
			TestEqual(
				*FString::Printf(TEXT("%s vertical-only landing handoff remains Idle"), NetworkView),
				VerticalOnlyResult[0],
				Idle);
		}

		FRpgGroundMotionMatchingSelectionSnapshot HorizontalMovingSnapshot =
			VerticalOnlySnapshot;
		HorizontalMovingSnapshot.WorldVelocity = FVector(
			RpgMotionMatchingRuntime::ChooserVelocityTolerance + 0.01f,
			0.0f,
			0.0f);
		HorizontalMovingSnapshot.GroundSpeed = HorizontalMovingSnapshot.WorldVelocity.Size2D();
		HorizontalMovingSnapshot.FutureVelocity = FVector(
			HorizontalMovingSnapshot.GroundSpeed + RpgMotionMatchingRuntime::RunStartMinimumFutureSpeedGain,
			0.0f,
			0.0f);

		TestTrue(
			*FString::Printf(TEXT("%s opens logical Moving above the horizontal velocity tolerance"), NetworkView),
			RpgMotionMatchingRuntime::IsChooserMoving(HorizontalMovingSnapshot));
		TestRoleSequence(
			*FString::Printf(TEXT("%s first horizontal Run frame"), NetworkView),
			RpgMotionMatchingRuntime::ResolveDatabaseRoles(HorizontalMovingSnapshot),
			{
				ERpgMotionMatchingDatabaseRole::StandRunStarts,
				ERpgMotionMatchingDatabaseRole::StandRunLoops,
			});
		const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases HorizontalMovingResult =
			URpgAnimInstance::ResolveGroundMotionMatchingDatabases(HorizontalMovingSnapshot, DatabaseSets);
		TestEqual(
			*FString::Printf(TEXT("%s first horizontal Run frame resolves Starts and Loops"), NetworkView),
			HorizontalMovingResult.Num(),
			2);
		if (HorizontalMovingResult.Num() == 2)
		{
			TestEqual(
				*FString::Printf(TEXT("%s first horizontal Run frame keeps Starts first"), NetworkView),
				HorizontalMovingResult[0],
				RunStarts);
			TestEqual(
				*FString::Printf(TEXT("%s first horizontal Run frame keeps Loops second"), NetworkView),
				HorizontalMovingResult[1],
				RunLoops);
		}
	}

	FRpgGroundMotionMatchingSelectionSnapshot StartAndPivotSnapshot = FreePivotSnapshot;
	StartAndPivotSnapshot.FutureVelocity = FVector(
		StartAndPivotSnapshot.GroundSpeed + RpgMotionMatchingRuntime::RunStartMinimumFutureSpeedGain,
		0.0f,
		0.0f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases StartAndPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StartAndPivotSnapshot, DatabaseSets);
	TestEqual(TEXT("Independent GASP rows can expose Starts, Loops, and Pivots together"), StartAndPivotResult.Num(), 3);
	TestEqual(TEXT("Combined source order begins with Starts"), StartAndPivotResult[0], RunStarts);
	TestEqual(TEXT("Combined source order keeps Loops second"), StartAndPivotResult[1], RunLoops);
	TestEqual(TEXT("Combined source order ends with Pivots"), StartAndPivotResult[2], RunPivots);
	TestRoleSequence(
		TEXT("Starting and pivoting Run"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(StartAndPivotSnapshot),
		{
			ERpgMotionMatchingDatabaseRole::StandRunStarts,
			ERpgMotionMatchingDatabaseRole::StandRunLoops,
			ERpgMotionMatchingDatabaseRole::StandRunPivots,
		});

	static const TCHAR* const SyntheticNetworkViews[] = {
		TEXT("Authority"),
		TEXT("Autonomous Proxy"),
		TEXT("Simulated Proxy"),
		TEXT("Late Join Simulated Proxy"),
	};
	for (const TCHAR* NetworkView : SyntheticNetworkViews)
	{
		const FRpgGroundMotionMatchingSelectionSnapshot IdenticalSnapshot =
			StartAndPivotSnapshot;
		TestRoleSequence(
			*FString::Printf(TEXT("%s identical snapshot parity"), NetworkView),
			RpgMotionMatchingRuntime::ResolveDatabaseRoles(IdenticalSnapshot),
			{
				ERpgMotionMatchingDatabaseRole::StandRunStarts,
				ERpgMotionMatchingDatabaseRole::StandRunLoops,
				ERpgMotionMatchingDatabaseRole::StandRunPivots,
			});
	}

	FRpgGroundMotionMatchingSelectionSnapshot CurrentPivotSnapshot = StartAndPivotSnapshot;
	CurrentPivotSnapshot.CurrentDatabaseRole = ERpgMotionMatchingDatabaseRole::StandRunPivots;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases CurrentPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(CurrentPivotSnapshot, DatabaseSets);
	TestFalse(TEXT("A selected Pivot database suppresses the Start row"), CurrentPivotResult.Contains(RunStarts));
	TestTrue(TEXT("The selected Pivot remains searchable while its angle gate is active"), CurrentPivotResult.Contains(RunPivots));
	TestRoleSequence(
		TEXT("Continuing Run Pivot"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(CurrentPivotSnapshot),
		{
			ERpgMotionMatchingDatabaseRole::StandRunLoops,
			ERpgMotionMatchingDatabaseRole::StandRunPivots,
		});

	FGraphTraversalCounter AnimInstanceUpdateCounter;
	FGraphTraversalCounter MotionMatchingUpdateCounter;
	AnimInstanceUpdateCounter.Increment();
	TestFalse(
		TEXT("The first Motion Matching update is initialization rather than re-entry"),
		URpgAnimInstance::SynchronizeMotionMatchingNodeUpdateCounter(
			MotionMatchingUpdateCounter,
			AnimInstanceUpdateCounter));
	AnimInstanceUpdateCounter.Increment();
	TestFalse(
		TEXT("Consecutive Motion Matching updates remain relevant"),
		URpgAnimInstance::SynchronizeMotionMatchingNodeUpdateCounter(
			MotionMatchingUpdateCounter,
			AnimInstanceUpdateCounter));
	AnimInstanceUpdateCounter.Increment();
	AnimInstanceUpdateCounter.Increment();
	const bool bBecameRelevant =
		URpgAnimInstance::SynchronizeMotionMatchingNodeUpdateCounter(
			MotionMatchingUpdateCounter,
			AnimInstanceUpdateCounter);
	TestTrue(TEXT("A missed node update detects Motion Matching re-entry"), bBecameRelevant);

	FRpgGroundMotionMatchingSelectionSnapshot RelevancyResetSnapshot =
		CurrentPivotSnapshot;
	RelevancyResetSnapshot.CurrentDatabaseRole = bBecameRelevant
		? ERpgMotionMatchingDatabaseRole::None
		: ERpgMotionMatchingDatabaseRole::StandRunPivots;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases RelevancyResetResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(RelevancyResetSnapshot, DatabaseSets);
	TestTrue(
		TEXT("Clearing a stale Pivot role after relevancy restores Run Starts"),
		RelevancyResetResult.Contains(RunStarts));
	TestRoleSequence(
		TEXT("Run after relevancy reset"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(RelevancyResetSnapshot),
		{
			ERpgMotionMatchingDatabaseRole::StandRunStarts,
			ERpgMotionMatchingDatabaseRole::StandRunLoops,
			ERpgMotionMatchingDatabaseRole::StandRunPivots,
		});

	FRpgGroundMotionMatchingSelectionSnapshot RunStopSnapshot = SteadyRunSnapshot;
	RunStopSnapshot.WorldAcceleration = FVector::ZeroVector;
	RunStopSnapshot.GroundSpeed = RpgMotionMatchingRuntime::RunStopMinimumSpeed;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases RunStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(RunStopSnapshot, DatabaseSets);
	TestEqual(TEXT("At 100 cm/s logical Idle exposes both overlapping Stop rows"), RunStopResult.Num(), 2);
	TestEqual(TEXT("The exact 100 cm/s boundary preserves Walk Stops first"), RunStopResult[0], WalkStops);
	TestEqual(TEXT("The exact 100 cm/s boundary appends Run Stops"), RunStopResult[1], RunStops);
	TestFalse(TEXT("Run Stop never exposes Run Loops"), RunStopResult.Contains(RunLoops));
	TestFalse(TEXT("Logical Idle never exposes the moving Walk aggregate"), RunStopResult.Contains(WalkMoving));
	TestRoleSequence(
		TEXT("Overlapping Walk and Run Stops"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(RunStopSnapshot),
		{
			ERpgMotionMatchingDatabaseRole::StandWalkStops,
			ERpgMotionMatchingDatabaseRole::StandRunStops,
		});

	FRpgGroundMotionMatchingSelectionSnapshot WalkStopSnapshot = RunStopSnapshot;
	WalkStopSnapshot.GroundSpeed = RpgMotionMatchingRuntime::RunStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkStopSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 100 cm/s logical Idle exposes only Walk Stops"), WalkStopResult.Num(), 1);
	TestEqual(TEXT("Walk deceleration uses the dedicated Walk Stops database"), WalkStopResult[0], WalkStops);

	WalkStopSnapshot.GroundSpeed = RpgMotionMatchingRuntime::WalkStopMinimumSpeed;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkStopBoundaryResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkStopSnapshot, DatabaseSets);
	TestEqual(TEXT("The exact 20 cm/s boundary exposes both overlapping rows"), WalkStopBoundaryResult.Num(), 2);
	TestEqual(TEXT("The exact 20 cm/s boundary preserves Idle first"), WalkStopBoundaryResult[0], Idle);
	TestEqual(TEXT("The exact 20 cm/s boundary appends Walk Stops"), WalkStopBoundaryResult[1], WalkStops);

	FRpgGroundMotionMatchingSelectionSnapshot AboveWalkStopBoundarySnapshot = WalkStopSnapshot;
	AboveWalkStopBoundarySnapshot.GroundSpeed = RpgMotionMatchingRuntime::WalkStopMinimumSpeed + 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AboveWalkStopBoundaryResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(
			AboveWalkStopBoundarySnapshot,
			DatabaseSets);
	TestEqual(TEXT("Above 20 cm/s Idle is no longer eligible"), AboveWalkStopBoundaryResult.Num(), 1);
	TestEqual(TEXT("Above 20 cm/s Walk Stops remain eligible"), AboveWalkStopBoundaryResult[0], WalkStops);

	FRpgGroundMotionMatchingSelectionSnapshot IdleDecelerationSnapshot = WalkStopSnapshot;
	IdleDecelerationSnapshot.GroundSpeed = RpgMotionMatchingRuntime::WalkStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases IdleDecelerationResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(IdleDecelerationSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 20 cm/s logical Idle resolves the Idle database"), IdleDecelerationResult.Num(), 1);
	TestEqual(TEXT("Low-speed deceleration uses Idle"), IdleDecelerationResult[0], Idle);

	FRpgGroundMotionMatchingSelectionSnapshot BelowSprintStopSnapshot = RunStopSnapshot;
	BelowSprintStopSnapshot.GroundSpeed = RpgMotionMatchingRuntime::SprintStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases BelowSprintStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(BelowSprintStopSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 550 cm/s logical Idle exposes Walk and Run Stops"), BelowSprintStopResult.Num(), 2);
	TestEqual(TEXT("Below Sprint speed Walk Stops remain first"), BelowSprintStopResult[0], WalkStops);
	TestEqual(TEXT("Below Sprint speed Run Stops remain second"), BelowSprintStopResult[1], RunStops);

	FRpgGroundMotionMatchingSelectionSnapshot SprintStopSnapshot = RunStopSnapshot;
	SprintStopSnapshot.Gait = ERpgLocomotionGait::Sprint;
	SprintStopSnapshot.GroundSpeed = RpgMotionMatchingRuntime::SprintStopMinimumSpeed;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases SprintStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SprintStopSnapshot, DatabaseSets);
	TestEqual(TEXT("A future explicit Sprint exposes all three Stop rows at 550 cm/s"), SprintStopResult.Num(), 3);
	TestEqual(TEXT("Sprint Stop source order begins with Walk Stops"), SprintStopResult[0], WalkStops);
	TestEqual(TEXT("Sprint Stop source order keeps Run Stops second"), SprintStopResult[1], RunStops);
	TestEqual(TEXT("Sprint Stop source order appends Sprint Stops"), SprintStopResult[2], SprintStops);
	TestFalse(TEXT("Sprint stopping never exposes the moving Sprint aggregate"), SprintStopResult.Contains(SprintMoving));
	TestRoleSequence(
		TEXT("Overlapping Sprint Stops"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(SprintStopSnapshot),
		{
			ERpgMotionMatchingDatabaseRole::StandWalkStops,
			ERpgMotionMatchingDatabaseRole::StandRunStops,
			ERpgMotionMatchingDatabaseRole::StandSprintStops,
		});

	FRpgGroundMotionMatchingSelectionSnapshot FastRunStopSnapshot = RunStopSnapshot;
	FastRunStopSnapshot.GroundSpeed = 600.0f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases FastRunStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(FastRunStopSnapshot, DatabaseSets);
	TestEqual(
		TEXT("A project-tuned 600 cm/s Run does not masquerade as Sprint while stopping"),
		FastRunStopResult.Num(),
		2);
	TestEqual(TEXT("Fast Run keeps Walk Stops first"), FastRunStopResult[0], WalkStops);
	TestEqual(TEXT("Fast Run keeps Run Stops second"), FastRunStopResult[1], RunStops);
	TestFalse(TEXT("Fast Run excludes the forward-only Sprint Stops"), FastRunStopResult.Contains(SprintStops));

	FRpgGroundMotionMatchingSelectionSnapshot AboveSprintStopSnapshot = SprintStopSnapshot;
	AboveSprintStopSnapshot.GroundSpeed = RpgMotionMatchingRuntime::SprintStopMinimumSpeed + 100.0f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AboveSprintStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AboveSprintStopSnapshot, DatabaseSets);
	TestEqual(TEXT("Sprint Stops have no upper speed bound"), AboveSprintStopResult.Num(), 3);
	TestEqual(TEXT("Above 550 cm/s Sprint Stops remain last in source order"), AboveSprintStopResult[2], SprintStops);

	FRpgGroundMotionMatchingSelectionSnapshot StaleNonGroundSnapshot = SteadyRunSnapshot;
	StaleNonGroundSnapshot.bIsMovingOnGround = false;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases StaleNonGroundResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StaleNonGroundSnapshot, DatabaseSets);
	TestTrue(TEXT("A stale non-grounded pointer snapshot fails closed"), StaleNonGroundResult.IsEmpty());

	FRpgGroundMotionMatchingSelectionSnapshot AirborneRunSnapshot = StaleNonGroundSnapshot;
	AirborneRunSnapshot.MovementState = ERpgLocomotionMovementState::Airborne;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AirborneRunResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AirborneRunSnapshot, DatabaseSets);
	TestTrue(TEXT("The grounded pointer resolver ignores the Airborne role"), AirborneRunResult.IsEmpty());
	TestRoleSequence(
		TEXT("Airborne"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(AirborneRunSnapshot),
		{ERpgMotionMatchingDatabaseRole::Jump});

	FRpgGroundMotionMatchingSelectionSnapshot CrouchingSnapshot = WalkSnapshot;
	CrouchingSnapshot.Stance = ERpgLocomotionStance::Crouching;
	TestRoleSequence(
		TEXT("Grounded Crouch"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(CrouchingSnapshot),
		{ERpgMotionMatchingDatabaseRole::Crouch});
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases CrouchingGroundResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(CrouchingSnapshot, DatabaseSets);
	TestTrue(TEXT("The grounded pointer resolver ignores the dedicated Crouch role"), CrouchingGroundResult.IsEmpty());

	const FRpgGroundMotionMatchingSelectionSnapshot SprintSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Sprint);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases SprintResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SprintSnapshot, DatabaseSets);
	TestEqual(TEXT("Sprint resolves exactly one database"), SprintResult.Num(), 1);
	TestEqual(TEXT("Moving Sprint preserves its aggregate database"), SprintResult[0], SprintMoving);
	TestRoleSequence(
		TEXT("Moving Sprint"),
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(SprintSnapshot),
		{ERpgMotionMatchingDatabaseRole::StandSprint});

	FRpgGroundMotionMatchingDomainState MovingRunDomain;
	MovingRunDomain.PhysicalMovementState = ERpgLocomotionMovementState::Grounded;
	MovingRunDomain.Gait = ERpgLocomotionGait::Run;
	MovingRunDomain.Stance = ERpgLocomotionStance::Standing;
	MovingRunDomain.bChooserMoving = true;
	TestTrue(
		TEXT("The first selector domain requests a database-change interrupt"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(false, MovingRunDomain, MovingRunDomain));
	TestFalse(
		TEXT("Transient Start or Pivot candidate changes do not interrupt a stable Moving Run domain"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, MovingRunDomain));

	FRpgGroundMotionMatchingDomainState IdleRunDomain = MovingRunDomain;
	IdleRunDomain.bChooserMoving = false;
	TestTrue(
		TEXT("Moving to logical Idle interrupts exactly at input release"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, IdleRunDomain));
	TestFalse(
		TEXT("Crossing Stop speed bands does not interrupt the continuing Idle-domain pose"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, IdleRunDomain, IdleRunDomain));

	FRpgGroundMotionMatchingDomainState MovingWalkDomain = MovingRunDomain;
	MovingWalkDomain.Gait = ERpgLocomotionGait::Walk;
	TestTrue(
		TEXT("A gait change while Moving interrupts on database change"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, MovingWalkDomain));
	FRpgGroundMotionMatchingDomainState IdleWalkDomain = IdleRunDomain;
	IdleWalkDomain.Gait = ERpgLocomotionGait::Walk;
	TestFalse(
		TEXT("A gait label change inside logical Idle does not interrupt the stop continuation"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, IdleRunDomain, IdleWalkDomain));

	FRpgGroundMotionMatchingDomainState CrouchingRunDomain = MovingRunDomain;
	CrouchingRunDomain.Stance = ERpgLocomotionStance::Crouching;
	TestTrue(
		TEXT("A grounded stance change interrupts on database change"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, CrouchingRunDomain));
	FRpgGroundMotionMatchingDomainState AirborneDomain = MovingRunDomain;
	AirborneDomain.PhysicalMovementState = ERpgLocomotionMovementState::Airborne;
	TestTrue(
		TEXT("A physical movement-mode change interrupts on database change"),
		RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, AirborneDomain));

	FRpgGroundMotionMatchingDatabaseSets NullEntrySets = DatabaseSets;
	NullEntrySets.Run[1] = nullptr;
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation NullValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(NullEntrySets);
	TestTrue(TEXT("A null database is rejected by validation"), NullValidation.bHasNullDatabase);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases NullSafeResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StartAndPivotSnapshot, NullEntrySets);
	TestEqual(TEXT("Runtime omits a null Pivot while preserving Starts and Loops"), NullSafeResult.Num(), 2);
	TestFalse(TEXT("The omitted null cannot appear in results"), NullSafeResult.Contains(nullptr));

	FRpgGroundMotionMatchingDatabaseSets NullWalkStopSets = DatabaseSets;
	NullWalkStopSets.Walk[1] = nullptr;
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation NullWalkStopValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(NullWalkStopSets);
	TestTrue(TEXT("A null Walk Stops slot is rejected by validation"), NullWalkStopValidation.bHasNullDatabase);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases NullWalkStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SprintStopSnapshot, NullWalkStopSets);
	TestEqual(TEXT("A null Walk Stops slot safely preserves Run and Sprint Stops"), NullWalkStopResult.Num(), 2);
	TestEqual(TEXT("Run Stops become first when null Walk Stops are omitted"), NullWalkStopResult[0], RunStops);
	TestEqual(TEXT("Sprint Stops remain second when null Walk Stops are omitted"), NullWalkStopResult[1], SprintStops);

	FRpgGroundMotionMatchingDatabaseSets DuplicateRunSets = DatabaseSets;
	DuplicateRunSets.Run[1] = RunLoops;
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation DuplicateRunValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(DuplicateRunSets);
	TestTrue(TEXT("A duplicate inside Run is rejected"), DuplicateRunValidation.bHasDuplicateDatabase);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases DuplicateSafeResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StartAndPivotSnapshot, DuplicateRunSets);
	TestEqual(TEXT("Runtime searches a selected duplicate only once"), DuplicateSafeResult.Num(), 2);

	FRpgGroundMotionMatchingDatabaseSets CrossSetDuplicateSets = DatabaseSets;
	CrossSetDuplicateSets.Sprint[1] = WalkStops;
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation CrossSetValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(CrossSetDuplicateSets);
	TestTrue(TEXT("A Stop database reused across gait groups is rejected"), CrossSetValidation.bHasDuplicateDatabase);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases CrossSetDuplicateResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SprintStopSnapshot, CrossSetDuplicateSets);
	TestEqual(TEXT("Overlapping Stop rows search a duplicate database only once"), CrossSetDuplicateResult.Num(), 2);
	TestEqual(TEXT("Deduplication preserves the first Walk Stops row"), CrossSetDuplicateResult[0], WalkStops);
	TestEqual(TEXT("Deduplication preserves the distinct Run Stops row"), CrossSetDuplicateResult[1], RunStops);

	FRpgGroundMotionMatchingDatabaseSets InvalidWalkShapeSets = DatabaseSets;
	InvalidWalkShapeSets.Walk.RemoveAt(1);
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation WalkShapeValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(InvalidWalkShapeSets);
	TestTrue(TEXT("A Walk set without its Stops slot is rejected"), WalkShapeValidation.bHasInvalidShape);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases InvalidWalkShapeResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SprintStopSnapshot, InvalidWalkShapeSets);
	TestEqual(TEXT("An undersized Walk set safely preserves Run and Sprint Stops"), InvalidWalkShapeResult.Num(), 2);

	FRpgGroundMotionMatchingDatabaseSets InvalidRunShapeSets = DatabaseSets;
	InvalidRunShapeSets.Run.RemoveAt(3);
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation RunShapeValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(InvalidRunShapeSets);
	TestTrue(TEXT("A Run set without four entries is rejected"), RunShapeValidation.bHasInvalidShape);

	FRpgGroundMotionMatchingDatabaseSets InvalidSprintShapeSets = DatabaseSets;
	InvalidSprintShapeSets.Sprint.RemoveAt(1);
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation SprintShapeValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(InvalidSprintShapeSets);
	TestTrue(TEXT("A Sprint set without its Stops slot is rejected"), SprintShapeValidation.bHasInvalidShape);

	FRpgGroundMotionMatchingDatabaseSets OversizedSets = DatabaseSets;
	UPoseSearchDatabase* UnexpectedFifthRunDatabase = NewObject<UPoseSearchDatabase>();
	OversizedSets.Run.Add(UnexpectedFifthRunDatabase);
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation OversizedValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(OversizedSets);
	TestTrue(TEXT("An oversized Run set is rejected"), OversizedValidation.bHasInvalidShape);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases BoundedRunResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StartAndPivotSnapshot, OversizedSets);
	TestEqual(TEXT("Runtime remains bounded to Starts, Loops, and Pivots"), BoundedRunResult.Num(), 3);
	TestFalse(TEXT("An invalid fifth Run entry is ignored"), BoundedRunResult.Contains(UnexpectedFifthRunDatabase));

	auto MakeTaggedDatabase = [](ERpgMotionMatchingDatabaseRole Role)
	{
		UPoseSearchDatabase* Database = NewObject<UPoseSearchDatabase>();
		Database->Tags.Add(URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(Role));
		Database->Tags.Add(URpgAnimInstance::GetMotionMatchingDatabaseStateTag(Role));
		return Database;
	};
	static const struct
	{
		ERpgMotionMatchingDatabaseRole Role;
		const TCHAR* RoleTag;
		const TCHAR* StateTag;
	} ExpectedRoleContracts[] = {
		{ERpgMotionMatchingDatabaseRole::StandIdle, TEXT("Rpg.MotionMatching.Role.StandIdle"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandWalk, TEXT("Rpg.MotionMatching.Role.StandWalk"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandWalkStops, TEXT("Rpg.MotionMatching.Role.StandWalkStops"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandRunLoops, TEXT("Rpg.MotionMatching.Role.StandRunLoops"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandRunPivots, TEXT("Rpg.MotionMatching.Role.StandRunPivots"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandRunStarts, TEXT("Rpg.MotionMatching.Role.StandRunStarts"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandRunStops, TEXT("Rpg.MotionMatching.Role.StandRunStops"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandSprint, TEXT("Rpg.MotionMatching.Role.StandSprint"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::StandSprintStops, TEXT("Rpg.MotionMatching.Role.StandSprintStops"), TEXT("Rpg.MotionMatching.State.Grounded")},
		{ERpgMotionMatchingDatabaseRole::Crouch, TEXT("Rpg.MotionMatching.Role.Crouch"), TEXT("Rpg.MotionMatching.State.Crouching")},
		{ERpgMotionMatchingDatabaseRole::StandTurnInPlace, TEXT("Rpg.MotionMatching.Role.StandTurnInPlace"), TEXT("Rpg.MotionMatching.State.TurnInPlace")},
		{ERpgMotionMatchingDatabaseRole::Jump, TEXT("Rpg.MotionMatching.Role.Jump"), TEXT("Rpg.MotionMatching.State.Airborne")},
		{ERpgMotionMatchingDatabaseRole::StandLightLanding, TEXT("Rpg.MotionMatching.Role.StandLightLanding"), TEXT("Rpg.MotionMatching.State.Landing")},
		{ERpgMotionMatchingDatabaseRole::StandHeavyLanding, TEXT("Rpg.MotionMatching.Role.StandHeavyLanding"), TEXT("Rpg.MotionMatching.State.Landing")},
		{ERpgMotionMatchingDatabaseRole::WalkLightLanding, TEXT("Rpg.MotionMatching.Role.WalkLightLanding"), TEXT("Rpg.MotionMatching.State.Landing")},
		{ERpgMotionMatchingDatabaseRole::WalkHeavyLanding, TEXT("Rpg.MotionMatching.Role.WalkHeavyLanding"), TEXT("Rpg.MotionMatching.State.Landing")},
		{ERpgMotionMatchingDatabaseRole::RunLightLanding, TEXT("Rpg.MotionMatching.Role.RunLightLanding"), TEXT("Rpg.MotionMatching.State.Landing")},
		{ERpgMotionMatchingDatabaseRole::RunHeavyLanding, TEXT("Rpg.MotionMatching.Role.RunHeavyLanding"), TEXT("Rpg.MotionMatching.State.Landing")},
	};
	TestEqual(
		TEXT("The curated runtime has exactly eighteen non-None database roles"),
		static_cast<int32>(ERpgMotionMatchingDatabaseRole::Count) - 1,
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedRoleContracts)));

	URpgAnimInstance::FMotionMatchingDatabaseRoleContracts ValidRoleContracts;
	for (const auto& ExpectedContract : ExpectedRoleContracts)
	{
		TestEqual(
			*FString::Printf(TEXT("%s keeps its exact immutable role tag"), ExpectedContract.RoleTag),
			URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(ExpectedContract.Role),
			FName(ExpectedContract.RoleTag));
		TestEqual(
			*FString::Printf(TEXT("%s keeps its exact immutable state tag"), ExpectedContract.RoleTag),
			URpgAnimInstance::GetMotionMatchingDatabaseStateTag(ExpectedContract.Role),
			FName(ExpectedContract.StateTag));
		UPoseSearchDatabase* Database = MakeTaggedDatabase(ExpectedContract.Role);
		ValidRoleContracts.Add({ExpectedContract.Role, Database});
		TestEqual(
			*FString::Printf(TEXT("%s resolves from its completed tagged database"), ExpectedContract.RoleTag),
			static_cast<uint8>(URpgAnimInstance::ResolveMotionMatchingDatabaseRole(Database)),
			static_cast<uint8>(ExpectedContract.Role));
	}
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation ValidRoleValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("Every runtime role has one unique database plus exact role/state tags"), ValidRoleValidation.IsValid());

	auto FindRoleDatabase = [&ValidRoleContracts](ERpgMotionMatchingDatabaseRole Role)
	{
		for (const URpgAnimInstance::FMotionMatchingDatabaseRoleContract& Contract : ValidRoleContracts)
		{
			if (Contract.Role == Role)
			{
				return Contract.Database;
			}
		}
		return static_cast<UPoseSearchDatabase*>(nullptr);
	};
	USkeletalMeshComponent* RoleDatabaseOwnerOuter = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* RoleDatabaseOwner =
		NewObject<URpgAnimInstance>(RoleDatabaseOwnerOuter);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Idle[0] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandIdle);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Walk[0] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandWalk);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Walk[1] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandWalkStops);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Run[0] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandRunLoops);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Run[1] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandRunPivots);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Run[2] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandRunStarts);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Run[3] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandRunStops);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Sprint[0] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandSprint);
	RoleDatabaseOwner->GroundMotionMatchingDatabaseSets.Sprint[1] = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandSprintStops);
	RoleDatabaseOwner->CrouchingMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::Crouch);
	RoleDatabaseOwner->TurnInPlaceMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandTurnInPlace);
	RoleDatabaseOwner->AirborneMotionMatchingDatabases.Add(FindRoleDatabase(ERpgMotionMatchingDatabaseRole::Jump));
	RoleDatabaseOwner->LandingMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandLightLanding);
	RoleDatabaseOwner->StandHeavyLandingMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::StandHeavyLanding);
	RoleDatabaseOwner->WalkLightLandingMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::WalkLightLanding);
	RoleDatabaseOwner->WalkHeavyLandingMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::WalkHeavyLanding);
	RoleDatabaseOwner->RunLightLandingMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::RunLightLanding);
	RoleDatabaseOwner->RunHeavyLandingMotionMatchingDatabase = FindRoleDatabase(ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	for (const auto& ExpectedContract : ExpectedRoleContracts)
	{
		TestEqual(
			*FString::Printf(TEXT("%s maps to its configured current database"), ExpectedContract.RoleTag),
			RoleDatabaseOwner->GetMotionMatchingDatabaseForRole(ExpectedContract.Role),
			FindRoleDatabase(ExpectedContract.Role));
	}
	const URpgAnimInstance::FMotionMatchingDatabaseRoleContracts BuiltRoleContracts =
		RoleDatabaseOwner->BuildMotionMatchingDatabaseRoleContracts();
	TestEqual(
		TEXT("The live AnimInstance contract builder covers all eighteen database roles"),
		BuiltRoleContracts.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedRoleContracts)));
	TestTrue(
		TEXT("The live AnimInstance role mapping passes exact database/tag validation"),
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(BuiltRoleContracts).IsValid());

	UPoseSearchDatabase* StandIdleRoleDatabase = ValidRoleContracts[0].Database;
	const FName StandIdleRoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(
		ERpgMotionMatchingDatabaseRole::StandIdle);
	const FName StandIdleStateTag = URpgAnimInstance::GetMotionMatchingDatabaseStateTag(
		ERpgMotionMatchingDatabaseRole::StandIdle);
	TestEqual(
		TEXT("An exact immutable role tag resolves Stand Idle"),
		static_cast<uint8>(URpgAnimInstance::ResolveMotionMatchingDatabaseRole(StandIdleRoleDatabase)),
		static_cast<uint8>(ERpgMotionMatchingDatabaseRole::StandIdle));
	TestEqual(
		TEXT("A null completed database resolves no runtime role"),
		static_cast<uint8>(URpgAnimInstance::ResolveMotionMatchingDatabaseRole(nullptr)),
		static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None));

	StandIdleRoleDatabase->Tags.Remove(StandIdleRoleTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation MissingRoleTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("A missing project role tag fails validation"), MissingRoleTagValidation.bHasMissingRoleTag);
	TestEqual(
		TEXT("A database without a project role tag resolves no runtime role"),
		static_cast<uint8>(URpgAnimInstance::ResolveMotionMatchingDatabaseRole(StandIdleRoleDatabase)),
		static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None));
	StandIdleRoleDatabase->Tags.Add(StandIdleRoleTag);

	StandIdleRoleDatabase->Tags.Add(StandIdleRoleTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation DuplicateRoleTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("A duplicate project role tag fails validation"), DuplicateRoleTagValidation.bHasDuplicateRoleTag);
	TestEqual(
		TEXT("Duplicate role tags fail closed during completed-database resolution"),
		static_cast<uint8>(URpgAnimInstance::ResolveMotionMatchingDatabaseRole(StandIdleRoleDatabase)),
		static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None));
	StandIdleRoleDatabase->Tags.Pop();

	const FName WrongRoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(
		ERpgMotionMatchingDatabaseRole::StandWalk);
	StandIdleRoleDatabase->Tags.Remove(StandIdleRoleTag);
	StandIdleRoleDatabase->Tags.Add(WrongRoleTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation WrongRoleTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("A recognized tag for the wrong configured role fails validation"), WrongRoleTagValidation.bHasWrongRoleTag);
	StandIdleRoleDatabase->Tags.Remove(WrongRoleTag);
	StandIdleRoleDatabase->Tags.Add(StandIdleRoleTag);
	const FName MisspelledRoleTag(TEXT("Rpg.MotionMatching.Role.StandIdel"));
	StandIdleRoleDatabase->Tags.Add(MisspelledRoleTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation MisspelledRoleTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("An unknown project-prefixed role tag fails validation"), MisspelledRoleTagValidation.bHasWrongRoleTag);
	StandIdleRoleDatabase->Tags.Remove(MisspelledRoleTag);

	StandIdleRoleDatabase->Tags.Remove(StandIdleStateTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation MissingStateTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("A missing project state tag fails validation"), MissingStateTagValidation.bHasMissingStateTag);
	StandIdleRoleDatabase->Tags.Add(StandIdleStateTag);

	StandIdleRoleDatabase->Tags.Add(StandIdleStateTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation DuplicateStateTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("A duplicate project state tag fails validation"), DuplicateStateTagValidation.bHasDuplicateStateTag);
	StandIdleRoleDatabase->Tags.Pop();

	const FName WrongStateTag = URpgAnimInstance::GetMotionMatchingDatabaseStateTag(
		ERpgMotionMatchingDatabaseRole::Jump);
	StandIdleRoleDatabase->Tags.Remove(StandIdleStateTag);
	StandIdleRoleDatabase->Tags.Add(WrongStateTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation WrongStateTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("A recognized tag for the wrong configured state fails validation"), WrongStateTagValidation.bHasWrongStateTag);
	StandIdleRoleDatabase->Tags.Remove(WrongStateTag);
	StandIdleRoleDatabase->Tags.Add(StandIdleStateTag);
	const FName MisspelledStateTag(TEXT("Rpg.MotionMatching.State.Groundd"));
	StandIdleRoleDatabase->Tags.Add(MisspelledStateTag);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation MisspelledStateTagValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(ValidRoleContracts);
	TestTrue(TEXT("An unknown project-prefixed state tag fails validation"), MisspelledStateTagValidation.bHasWrongStateTag);
	StandIdleRoleDatabase->Tags.Remove(MisspelledStateTag);

	URpgAnimInstance::FMotionMatchingDatabaseRoleContracts MissingRoleContracts = ValidRoleContracts;
	MissingRoleContracts.RemoveAt(MissingRoleContracts.Num() - 1);
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation MissingRoleValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(MissingRoleContracts);
	TestTrue(TEXT("A missing runtime role contract fails validation"), MissingRoleValidation.bHasMissingRole);

	URpgAnimInstance::FMotionMatchingDatabaseRoleContracts DuplicateRoleContracts = ValidRoleContracts;
	DuplicateRoleContracts.Add({
		ERpgMotionMatchingDatabaseRole::StandIdle,
		MakeTaggedDatabase(ERpgMotionMatchingDatabaseRole::StandIdle),
	});
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation DuplicateRoleValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(DuplicateRoleContracts);
	TestTrue(TEXT("A duplicate runtime role contract fails validation"), DuplicateRoleValidation.bHasDuplicateRole);

	URpgAnimInstance::FMotionMatchingDatabaseRoleContracts DuplicateRoleDatabaseContracts = ValidRoleContracts;
	DuplicateRoleDatabaseContracts[1].Database = DuplicateRoleDatabaseContracts[0].Database;
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation DuplicateRoleDatabaseValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(DuplicateRoleDatabaseContracts);
	TestTrue(TEXT("One database cannot own two runtime role contracts"), DuplicateRoleDatabaseValidation.bHasDuplicateDatabase);

	URpgAnimInstance::FMotionMatchingDatabaseRoleContracts NullRoleDatabaseContracts = ValidRoleContracts;
	NullRoleDatabaseContracts.Last().Database = nullptr;
	const URpgAnimInstance::FMotionMatchingDatabaseRoleValidation NullRoleDatabaseValidation =
		URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(NullRoleDatabaseContracts);
	TestTrue(TEXT("A null runtime role database fails validation"), NullRoleDatabaseValidation.bHasNullDatabase);

	const FRpgMotionMatchingPostSelectionState ContinuingRunPostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::StandRunLoops,
			true,
			EPoseSearchInterruptMode::DoNotInterrupt,
			true,
			true);
	TestEqual(
		TEXT("PostSelection preserves the completed Run role"),
		static_cast<uint8>(ContinuingRunPostSelection.CurrentDatabaseRole),
		static_cast<uint8>(ERpgMotionMatchingDatabaseRole::StandRunLoops));
	TestTrue(TEXT("PostSelection preserves Continuing Pose"), ContinuingRunPostSelection.bIsContinuingPose);
	TestEqual(
		TEXT("PostSelection preserves DoNotInterrupt"),
		static_cast<uint8>(ContinuingRunPostSelection.InterruptMode),
		static_cast<uint8>(EPoseSearchInterruptMode::DoNotInterrupt));
	TestFalse(TEXT("A Continuing Pose never relatches turn-in-place"), ContinuingRunPostSelection.bShouldLatchTurnInPlace);
	TestFalse(TEXT("A Continuing Pose never relatches landing"), ContinuingRunPostSelection.bShouldLatchLanding);

	const FRpgMotionMatchingPostSelectionState FreshTurnPostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::StandTurnInPlace,
			false,
			EPoseSearchInterruptMode::ForceInterrupt,
			true,
			true);
	TestEqual(
		TEXT("PostSelection preserves ForceInterrupt for a requested turn"),
		static_cast<uint8>(FreshTurnPostSelection.InterruptMode),
		static_cast<uint8>(EPoseSearchInterruptMode::ForceInterrupt));
	TestTrue(TEXT("A fresh selected TurnInPlace role latches its playback"), FreshTurnPostSelection.bShouldLatchTurnInPlace);
	TestFalse(TEXT("A TurnInPlace role cannot also latch landing"), FreshTurnPostSelection.bShouldLatchLanding);

	const FRpgMotionMatchingPostSelectionState ContinuingTurnPostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::StandTurnInPlace,
			true,
			EPoseSearchInterruptMode::DoNotInterrupt,
			true,
			false);
	TestFalse(TEXT("A continuing TurnInPlace result is not restarted"), ContinuingTurnPostSelection.bShouldLatchTurnInPlace);
	const FRpgMotionMatchingPostSelectionState UnrequestedTurnPostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::StandTurnInPlace,
			false,
			EPoseSearchInterruptMode::DoNotInterrupt,
			false,
			false);
	TestFalse(TEXT("A TurnInPlace role cannot latch without an active request"), UnrequestedTurnPostSelection.bShouldLatchTurnInPlace);

	const FRpgMotionMatchingPostSelectionState FreshLandingPostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			false,
			EPoseSearchInterruptMode::InterruptOnDatabaseChange,
			true,
			true);
	TestEqual(
		TEXT("PostSelection preserves InterruptOnDatabaseChange for landing"),
		static_cast<uint8>(FreshLandingPostSelection.InterruptMode),
		static_cast<uint8>(EPoseSearchInterruptMode::InterruptOnDatabaseChange));
	TestTrue(TEXT("A fresh selected Landing role latches its bounded playback"), FreshLandingPostSelection.bShouldLatchLanding);
	TestFalse(TEXT("A Landing role cannot also latch turn-in-place"), FreshLandingPostSelection.bShouldLatchTurnInPlace);
	const FRpgMotionMatchingPostSelectionState ContinuingLandingPostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			true,
			EPoseSearchInterruptMode::DoNotInterrupt,
			false,
			true);
	TestFalse(
		TEXT("A continuing Landing result is not restarted"),
		ContinuingLandingPostSelection.bShouldLatchLanding);
	const FRpgMotionMatchingPostSelectionState UnrequestedLandingPostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			false,
			EPoseSearchInterruptMode::DoNotInterrupt,
			false,
			false);
	TestFalse(TEXT("A Landing role cannot latch outside the landing window"), UnrequestedLandingPostSelection.bShouldLatchLanding);

	static const ERpgMotionMatchingDatabaseRole LandingRoles[] = {
		ERpgMotionMatchingDatabaseRole::StandLightLanding,
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
		ERpgMotionMatchingDatabaseRole::WalkLightLanding,
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
		ERpgMotionMatchingDatabaseRole::RunLightLanding,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
	};
	for (const ERpgMotionMatchingDatabaseRole LandingRole : LandingRoles)
	{
		const FName RoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(LandingRole);
		TestTrue(
			*FString::Printf(TEXT("%s is recognized as one of the six landing roles"), *RoleTag.ToString()),
			RpgMotionMatchingRuntime::IsLandingDatabaseRole(LandingRole));
		const FRpgMotionMatchingPostSelectionState FreshRolePostSelection =
			RpgMotionMatchingRuntime::ResolvePostSelection(
				LandingRole,
				false,
				EPoseSearchInterruptMode::InterruptOnDatabaseChange,
				true,
				true);
		TestEqual(
			*FString::Printf(TEXT("PostSelection preserves completed %s"), *RoleTag.ToString()),
			static_cast<uint8>(FreshRolePostSelection.CurrentDatabaseRole),
			static_cast<uint8>(LandingRole));
		TestTrue(
			*FString::Printf(TEXT("A fresh %s result latches its bounded landing playback"), *RoleTag.ToString()),
			FreshRolePostSelection.bShouldLatchLanding);
		TestFalse(
			*FString::Printf(TEXT("A %s result cannot latch turn-in-place"), *RoleTag.ToString()),
			FreshRolePostSelection.bShouldLatchTurnInPlace);
		const FRpgMotionMatchingPostSelectionState ContinuingRolePostSelection =
			RpgMotionMatchingRuntime::ResolvePostSelection(
				LandingRole,
				true,
				EPoseSearchInterruptMode::DoNotInterrupt,
				false,
				true);
		TestFalse(
			*FString::Printf(TEXT("A continuing %s result is not restarted"), *RoleTag.ToString()),
			ContinuingRolePostSelection.bShouldLatchLanding);
	}

	const FRpgMotionMatchingPostSelectionState InvalidRolePostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			ERpgMotionMatchingDatabaseRole::None,
			false,
			EPoseSearchInterruptMode::InterruptOnDatabaseChange,
			true,
			true);
	TestEqual(
		TEXT("An invalid completed role remains explicitly None"),
		static_cast<uint8>(InvalidRolePostSelection.CurrentDatabaseRole),
		static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None));
	TestFalse(TEXT("An invalid completed role cannot latch turn-in-place"), InvalidRolePostSelection.bShouldLatchTurnInPlace);
	TestFalse(TEXT("An invalid completed role cannot latch landing"), InvalidRolePostSelection.bShouldLatchLanding);

	FRpgGroundMotionMatchingSelectionSnapshot UnknownGaitSnapshot = SteadyRunSnapshot;
	UnknownGaitSnapshot.Gait = static_cast<ERpgLocomotionGait>(MAX_uint8);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases UnknownGaitResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(UnknownGaitSnapshot, DatabaseSets);
	TestTrue(TEXT("An unknown gait fails safely with no database search"), UnknownGaitResult.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
