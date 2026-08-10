// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
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
		FMath::IsNearlyEqual(URpgAnimInstance::TrajectoryHistorySamplingInterval, -1.0f));
	TestEqual(
		TEXT("Trajectory history keeps GASP's 30 samples"),
		URpgAnimInstance::TrajectoryHistorySampleCount,
		30);
	TestTrue(
		TEXT("Trajectory prediction samples at GASP's 0.1 second interval"),
		FMath::IsNearlyEqual(URpgAnimInstance::TrajectoryPredictionSamplingInterval, 0.1f));
	TestEqual(
		TEXT("Trajectory prediction keeps GASP's 15 samples"),
		URpgAnimInstance::TrajectoryPredictionSampleCount,
		15);
	TestEqual(
		TEXT("Sprint Stops begin at GASP's inclusive 550 cm/s boundary"),
		URpgAnimInstance::SprintStopMinimumSpeed,
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
		URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot Snapshot;
		Snapshot.Gait = Gait;
		Snapshot.WorldVelocity = FVector(450.0f, 0.0f, 0.0f);
		Snapshot.WorldAcceleration = FVector(2048.0f, 0.0f, 0.0f);
		Snapshot.GroundSpeed = 450.0f;
		Snapshot.FutureGroundSpeed = 450.0f;
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

	const URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot IdleSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Idle);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases IdleResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(IdleSnapshot, DatabaseSets);
	TestEqual(TEXT("Idle resolves exactly one database"), IdleResult.Num(), 1);
	TestEqual(TEXT("Idle preserves its authored database"), IdleResult[0], Idle);

	const URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot WalkSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Walk);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkSnapshot, DatabaseSets);
	TestEqual(TEXT("Walk resolves exactly one database"), WalkResult.Num(), 1);
	TestEqual(TEXT("Moving Walk preserves its aggregate database"), WalkResult[0], WalkMoving);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot SteadyRunSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Run);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases SteadyRunResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SteadyRunSnapshot, DatabaseSets);
	TestEqual(TEXT("A source-aligned steady Free Run resolves only Loops"), SteadyRunResult.Num(), 1);
	TestEqual(TEXT("Steady Run preserves Loops"), SteadyRunResult[0], RunLoops);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot FastStartSnapshot = SteadyRunSnapshot;
	FastStartSnapshot.GroundSpeed = 350.0f;
	FastStartSnapshot.WorldVelocity = FVector(350.0f, 0.0f, 0.0f);
	FastStartSnapshot.FutureGroundSpeed =
		FastStartSnapshot.GroundSpeed + URpgAnimInstance::RunStartMinimumFutureSpeedGain;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases FastStartResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(FastStartSnapshot, DatabaseSets);
	TestEqual(TEXT("Starts remain eligible above the removed local 150 cm/s cap"), FastStartResult.Num(), 2);
	TestEqual(TEXT("Source result order places Starts before Loops"), FastStartResult[0], RunStarts);
	TestEqual(TEXT("Starts preserve Loops as the continuing fallback"), FastStartResult[1], RunLoops);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot BelowStartGainSnapshot = FastStartSnapshot;
	BelowStartGainSnapshot.FutureGroundSpeed -= 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases BelowStartGainResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(BelowStartGainSnapshot, DatabaseSets);
	TestEqual(TEXT("Below the exact +100 cm/s Start gate Run remains Loops-only"), BelowStartGainResult.Num(), 1);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot FreeBelowPivotSnapshot = SteadyRunSnapshot;
	FreeBelowPivotSnapshot.WorldAcceleration = DirectionAtDegrees(
		URpgAnimInstance::FreeRunPivotMinimumAngle - 0.01f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases FreeBelowPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(FreeBelowPivotSnapshot, DatabaseSets);
	TestFalse(TEXT("Free mode excludes a turn just below 45 degrees"), FreeBelowPivotResult.Contains(RunPivots));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot FreePivotSnapshot = SteadyRunSnapshot;
	FreePivotSnapshot.WorldAcceleration = DirectionAtDegrees(URpgAnimInstance::FreeRunPivotMinimumAngle);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases FreePivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(FreePivotSnapshot, DatabaseSets);
	TestEqual(TEXT("Free mode exposes Pivots at exactly 45 degrees"), FreePivotResult.Num(), 2);
	TestEqual(TEXT("Free Pivot keeps Loops first when no Start is eligible"), FreePivotResult[0], RunLoops);
	TestEqual(TEXT("Free Pivot appends Pivots"), FreePivotResult[1], RunPivots);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot StrafeBelowPivotSnapshot = SteadyRunSnapshot;
	StrafeBelowPivotSnapshot.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	StrafeBelowPivotSnapshot.WorldAcceleration = DirectionAtDegrees(
		URpgAnimInstance::CombatStrafeRunPivotMinimumAngle - 0.01f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases StrafeBelowPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StrafeBelowPivotSnapshot, DatabaseSets);
	TestFalse(TEXT("Combat Strafe excludes a turn just below 30 degrees"), StrafeBelowPivotResult.Contains(RunPivots));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot StrafePivotSnapshot = SteadyRunSnapshot;
	StrafePivotSnapshot.RotationMode = ERpgCharacterRotationMode::CombatStrafe;
	StrafePivotSnapshot.WorldAcceleration = DirectionAtDegrees(
		URpgAnimInstance::CombatStrafeRunPivotMinimumAngle);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases StrafePivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StrafePivotSnapshot, DatabaseSets);
	TestTrue(TEXT("Combat Strafe exposes Pivots at exactly 30 degrees"), StrafePivotResult.Contains(RunPivots));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot AimPivotSnapshot = SteadyRunSnapshot;
	AimPivotSnapshot.RotationMode = ERpgCharacterRotationMode::Aim;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AimPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AimPivotSnapshot, DatabaseSets);
	TestTrue(TEXT("Aim exposes Pivots at its exact zero-degree threshold"), AimPivotResult.Contains(RunPivots));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot AimWithoutAccelerationSnapshot = AimPivotSnapshot;
	AimWithoutAccelerationSnapshot.WorldAcceleration = FVector::ZeroVector;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AimWithoutAccelerationResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AimWithoutAccelerationSnapshot, DatabaseSets);
	TestFalse(TEXT("Aim still requires GASP logical Moving before exposing Pivots"), AimWithoutAccelerationResult.Contains(RunPivots));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot TinyAccelerationSnapshot = SteadyRunSnapshot;
	TinyAccelerationSnapshot.WorldAcceleration = FVector(0.05f, 0.0f, 0.0f);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases TinyAccelerationResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(TinyAccelerationSnapshot, DatabaseSets);
	TestEqual(
		TEXT("GASP's 1e-4 acceleration tolerance keeps a finite 0.05 cm/s2 input in Moving"),
		TinyAccelerationResult[0],
		RunLoops);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot StartAndPivotSnapshot = FreePivotSnapshot;
	StartAndPivotSnapshot.FutureGroundSpeed =
		StartAndPivotSnapshot.GroundSpeed + URpgAnimInstance::RunStartMinimumFutureSpeedGain;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases StartAndPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StartAndPivotSnapshot, DatabaseSets);
	TestEqual(TEXT("Independent GASP rows can expose Starts, Loops, and Pivots together"), StartAndPivotResult.Num(), 3);
	TestEqual(TEXT("Combined source order begins with Starts"), StartAndPivotResult[0], RunStarts);
	TestEqual(TEXT("Combined source order keeps Loops second"), StartAndPivotResult[1], RunLoops);
	TestEqual(TEXT("Combined source order ends with Pivots"), StartAndPivotResult[2], RunPivots);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot CurrentPivotSnapshot = StartAndPivotSnapshot;
	CurrentPivotSnapshot.bCurrentDatabaseIsRunPivot = true;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases CurrentPivotResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(CurrentPivotSnapshot, DatabaseSets);
	TestFalse(TEXT("A selected Pivot database suppresses the Start row"), CurrentPivotResult.Contains(RunStarts));
	TestTrue(TEXT("The selected Pivot remains searchable while its angle gate is active"), CurrentPivotResult.Contains(RunPivots));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot RunStopSnapshot = SteadyRunSnapshot;
	RunStopSnapshot.WorldAcceleration = FVector::ZeroVector;
	RunStopSnapshot.GroundSpeed = URpgAnimInstance::RunStopMinimumSpeed;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases RunStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(RunStopSnapshot, DatabaseSets);
	TestEqual(TEXT("At 100 cm/s logical Idle exposes both overlapping Stop rows"), RunStopResult.Num(), 2);
	TestEqual(TEXT("The exact 100 cm/s boundary preserves Walk Stops first"), RunStopResult[0], WalkStops);
	TestEqual(TEXT("The exact 100 cm/s boundary appends Run Stops"), RunStopResult[1], RunStops);
	TestFalse(TEXT("Run Stop never exposes Run Loops"), RunStopResult.Contains(RunLoops));
	TestFalse(TEXT("Logical Idle never exposes the moving Walk aggregate"), RunStopResult.Contains(WalkMoving));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot WalkStopSnapshot = RunStopSnapshot;
	WalkStopSnapshot.GroundSpeed = URpgAnimInstance::RunStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkStopSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 100 cm/s logical Idle exposes only Walk Stops"), WalkStopResult.Num(), 1);
	TestEqual(TEXT("Walk deceleration uses the dedicated Walk Stops database"), WalkStopResult[0], WalkStops);

	WalkStopSnapshot.GroundSpeed = URpgAnimInstance::WalkStopMinimumSpeed;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkStopBoundaryResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkStopSnapshot, DatabaseSets);
	TestEqual(TEXT("The exact 20 cm/s boundary exposes both overlapping rows"), WalkStopBoundaryResult.Num(), 2);
	TestEqual(TEXT("The exact 20 cm/s boundary preserves Idle first"), WalkStopBoundaryResult[0], Idle);
	TestEqual(TEXT("The exact 20 cm/s boundary appends Walk Stops"), WalkStopBoundaryResult[1], WalkStops);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot AboveWalkStopBoundarySnapshot = WalkStopSnapshot;
	AboveWalkStopBoundarySnapshot.GroundSpeed = URpgAnimInstance::WalkStopMinimumSpeed + 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AboveWalkStopBoundaryResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(
			AboveWalkStopBoundarySnapshot,
			DatabaseSets);
	TestEqual(TEXT("Above 20 cm/s Idle is no longer eligible"), AboveWalkStopBoundaryResult.Num(), 1);
	TestEqual(TEXT("Above 20 cm/s Walk Stops remain eligible"), AboveWalkStopBoundaryResult[0], WalkStops);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot IdleDecelerationSnapshot = WalkStopSnapshot;
	IdleDecelerationSnapshot.GroundSpeed = URpgAnimInstance::WalkStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases IdleDecelerationResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(IdleDecelerationSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 20 cm/s logical Idle resolves the Idle database"), IdleDecelerationResult.Num(), 1);
	TestEqual(TEXT("Low-speed deceleration uses Idle"), IdleDecelerationResult[0], Idle);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot BelowSprintStopSnapshot = RunStopSnapshot;
	BelowSprintStopSnapshot.GroundSpeed = URpgAnimInstance::SprintStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases BelowSprintStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(BelowSprintStopSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 550 cm/s logical Idle exposes Walk and Run Stops"), BelowSprintStopResult.Num(), 2);
	TestEqual(TEXT("Below Sprint speed Walk Stops remain first"), BelowSprintStopResult[0], WalkStops);
	TestEqual(TEXT("Below Sprint speed Run Stops remain second"), BelowSprintStopResult[1], RunStops);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot SprintStopSnapshot = RunStopSnapshot;
	SprintStopSnapshot.Gait = ERpgLocomotionGait::Sprint;
	SprintStopSnapshot.GroundSpeed = URpgAnimInstance::SprintStopMinimumSpeed;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases SprintStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SprintStopSnapshot, DatabaseSets);
	TestEqual(TEXT("A future explicit Sprint exposes all three Stop rows at 550 cm/s"), SprintStopResult.Num(), 3);
	TestEqual(TEXT("Sprint Stop source order begins with Walk Stops"), SprintStopResult[0], WalkStops);
	TestEqual(TEXT("Sprint Stop source order keeps Run Stops second"), SprintStopResult[1], RunStops);
	TestEqual(TEXT("Sprint Stop source order appends Sprint Stops"), SprintStopResult[2], SprintStops);
	TestFalse(TEXT("Sprint stopping never exposes the moving Sprint aggregate"), SprintStopResult.Contains(SprintMoving));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot FastRunStopSnapshot = RunStopSnapshot;
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

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot AboveSprintStopSnapshot = SprintStopSnapshot;
	AboveSprintStopSnapshot.GroundSpeed = URpgAnimInstance::SprintStopMinimumSpeed + 100.0f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AboveSprintStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AboveSprintStopSnapshot, DatabaseSets);
	TestEqual(TEXT("Sprint Stops have no upper speed bound"), AboveSprintStopResult.Num(), 3);
	TestEqual(TEXT("Above 550 cm/s Sprint Stops remain last in source order"), AboveSprintStopResult[2], SprintStops);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot AirborneRunSnapshot = SteadyRunSnapshot;
	AirborneRunSnapshot.bIsMovingOnGround = false;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases AirborneRunResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(AirborneRunSnapshot, DatabaseSets);
	TestTrue(TEXT("A stale airborne ground snapshot fails closed"), AirborneRunResult.IsEmpty());

	const URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot SprintSnapshot =
		MakeMovingSnapshot(ERpgLocomotionGait::Sprint);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases SprintResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(SprintSnapshot, DatabaseSets);
	TestEqual(TEXT("Sprint resolves exactly one database"), SprintResult.Num(), 1);
	TestEqual(TEXT("Moving Sprint preserves its aggregate database"), SprintResult[0], SprintMoving);

	URpgAnimInstance::FGroundMotionMatchingDomainState MovingRunDomain;
	MovingRunDomain.PhysicalMovementState = ERpgLocomotionMovementState::Grounded;
	MovingRunDomain.Gait = ERpgLocomotionGait::Run;
	MovingRunDomain.Stance = ERpgLocomotionStance::Standing;
	MovingRunDomain.bChooserMoving = true;
	TestTrue(
		TEXT("The first selector domain requests a database-change interrupt"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(false, MovingRunDomain, MovingRunDomain));
	TestFalse(
		TEXT("Transient Start or Pivot candidate changes do not interrupt a stable Moving Run domain"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, MovingRunDomain));

	URpgAnimInstance::FGroundMotionMatchingDomainState IdleRunDomain = MovingRunDomain;
	IdleRunDomain.bChooserMoving = false;
	TestTrue(
		TEXT("Moving to logical Idle interrupts exactly at input release"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, IdleRunDomain));
	TestFalse(
		TEXT("Crossing Stop speed bands does not interrupt the continuing Idle-domain pose"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(true, IdleRunDomain, IdleRunDomain));

	URpgAnimInstance::FGroundMotionMatchingDomainState MovingWalkDomain = MovingRunDomain;
	MovingWalkDomain.Gait = ERpgLocomotionGait::Walk;
	TestTrue(
		TEXT("A gait change while Moving interrupts on database change"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, MovingWalkDomain));
	URpgAnimInstance::FGroundMotionMatchingDomainState IdleWalkDomain = IdleRunDomain;
	IdleWalkDomain.Gait = ERpgLocomotionGait::Walk;
	TestFalse(
		TEXT("A gait label change inside logical Idle does not interrupt the stop continuation"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(true, IdleRunDomain, IdleWalkDomain));

	URpgAnimInstance::FGroundMotionMatchingDomainState CrouchingRunDomain = MovingRunDomain;
	CrouchingRunDomain.Stance = ERpgLocomotionStance::Crouching;
	TestTrue(
		TEXT("A grounded stance change interrupts on database change"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, CrouchingRunDomain));
	URpgAnimInstance::FGroundMotionMatchingDomainState AirborneDomain = MovingRunDomain;
	AirborneDomain.PhysicalMovementState = ERpgLocomotionMovementState::Airborne;
	TestTrue(
		TEXT("A physical movement-mode change interrupts on database change"),
		URpgAnimInstance::ShouldInterruptGroundMotionMatching(true, MovingRunDomain, AirborneDomain));

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

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot UnknownGaitSnapshot = SteadyRunSnapshot;
	UnknownGaitSnapshot.Gait = static_cast<ERpgLocomotionGait>(MAX_uint8);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases UnknownGaitResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(UnknownGaitSnapshot, DatabaseSets);
	TestTrue(TEXT("An unknown gait fails safely with no database search"), UnknownGaitResult.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
