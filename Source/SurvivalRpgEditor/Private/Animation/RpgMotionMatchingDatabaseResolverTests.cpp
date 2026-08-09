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

	FRpgGroundMotionMatchingDatabaseSets DatabaseSets;
	TestEqual(TEXT("Idle has one fixed database slot"), DatabaseSets.Idle.Num(), 1);
	TestEqual(TEXT("Walk has one fixed database slot"), DatabaseSets.Walk.Num(), 1);
	TestEqual(TEXT("Run has four fixed database slots"), DatabaseSets.Run.Num(), 4);
	TestEqual(TEXT("Sprint has one fixed database slot"), DatabaseSets.Sprint.Num(), 1);

	UPoseSearchDatabase* Idle = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* Walk = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunLoops = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunPivots = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunStarts = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunStops = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* Sprint = NewObject<UPoseSearchDatabase>();
	if (!TestNotNull(TEXT("Idle database fixture exists"), Idle) ||
		!TestNotNull(TEXT("Walk database fixture exists"), Walk) ||
		!TestNotNull(TEXT("Run Loops database fixture exists"), RunLoops) ||
		!TestNotNull(TEXT("Run Pivots database fixture exists"), RunPivots) ||
		!TestNotNull(TEXT("Run Starts database fixture exists"), RunStarts) ||
		!TestNotNull(TEXT("Run Stops database fixture exists"), RunStops) ||
		!TestNotNull(TEXT("Sprint database fixture exists"), Sprint))
	{
		return false;
	}

	DatabaseSets.Idle[0] = Idle;
	DatabaseSets.Walk[0] = Walk;
	DatabaseSets.Run[0] = RunLoops;
	DatabaseSets.Run[1] = RunPivots;
	DatabaseSets.Run[2] = RunStarts;
	DatabaseSets.Run[3] = RunStops;
	DatabaseSets.Sprint[0] = Sprint;

	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation ValidContract =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(DatabaseSets);
	TestTrue(TEXT("The complete 1/1/4/1 database contract is valid"), ValidContract.IsValid());

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
	TestEqual(TEXT("Walk preserves its authored database"), WalkResult[0], Walk);

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
	TestEqual(TEXT("At 100 cm/s logical Idle resolves only Run Stops"), RunStopResult.Num(), 1);
	TestEqual(TEXT("Run Stop removes the current Run Loops database"), RunStopResult[0], RunStops);
	TestFalse(TEXT("Run Stop never exposes Run Loops"), RunStopResult.Contains(RunLoops));

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot WalkStopSnapshot = RunStopSnapshot;
	WalkStopSnapshot.GroundSpeed = URpgAnimInstance::RunStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkStopResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkStopSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 100 cm/s logical Idle uses the bounded Walk fallback"), WalkStopResult.Num(), 1);
	TestEqual(TEXT("The bounded Walk stop fallback uses the configured Walk database"), WalkStopResult[0], Walk);

	WalkStopSnapshot.GroundSpeed = URpgAnimInstance::WalkStopMinimumSpeed;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases WalkStopBoundaryResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(WalkStopSnapshot, DatabaseSets);
	TestEqual(TEXT("The exact 20 cm/s Walk stop boundary stays on Walk"), WalkStopBoundaryResult[0], Walk);

	URpgAnimInstance::FGroundMotionMatchingSelectionSnapshot IdleDecelerationSnapshot = WalkStopSnapshot;
	IdleDecelerationSnapshot.GroundSpeed = URpgAnimInstance::WalkStopMinimumSpeed - 0.01f;
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases IdleDecelerationResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(IdleDecelerationSnapshot, DatabaseSets);
	TestEqual(TEXT("Below 20 cm/s logical Idle resolves the Idle database"), IdleDecelerationResult.Num(), 1);
	TestEqual(TEXT("Low-speed deceleration uses Idle"), IdleDecelerationResult[0], Idle);

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
	TestEqual(TEXT("Sprint preserves its authored database"), SprintResult[0], Sprint);

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

	FRpgGroundMotionMatchingDatabaseSets DuplicateRunSets = DatabaseSets;
	DuplicateRunSets.Run[1] = RunLoops;
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation DuplicateRunValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(DuplicateRunSets);
	TestTrue(TEXT("A duplicate inside Run is rejected"), DuplicateRunValidation.bHasDuplicateDatabase);
	const URpgAnimInstance::FResolvedGroundMotionMatchingDatabases DuplicateSafeResult =
		URpgAnimInstance::ResolveGroundMotionMatchingDatabases(StartAndPivotSnapshot, DuplicateRunSets);
	TestEqual(TEXT("Runtime searches a selected duplicate only once"), DuplicateSafeResult.Num(), 2);

	FRpgGroundMotionMatchingDatabaseSets CrossSetDuplicateSets = DatabaseSets;
	CrossSetDuplicateSets.Sprint[0] = Walk;
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation CrossSetValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(CrossSetDuplicateSets);
	TestTrue(TEXT("A duplicate reused across gait groups is rejected"), CrossSetValidation.bHasDuplicateDatabase);

	FRpgGroundMotionMatchingDatabaseSets InvalidShapeSets = DatabaseSets;
	InvalidShapeSets.Run.RemoveAt(3);
	const URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation ShapeValidation =
		URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(InvalidShapeSets);
	TestTrue(TEXT("A Run set without four entries is rejected"), ShapeValidation.bHasInvalidShape);

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
