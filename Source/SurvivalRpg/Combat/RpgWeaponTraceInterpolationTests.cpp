#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RpgWeaponTraceInterpolation.h"

namespace RpgWeaponTraceInterpolationTests
{
	using RpgWeaponTraceInterpolation::FBladeSample;

	/** Blade swung about a shoulder pivot fixed in body space, traced every 10 cm from 50 cm (base) to 150 cm (tip). */
	struct FSwingModel
	{
		FVector ShoulderPivot = FVector(10.0, 25.0, 140.0);
		FVector SwingAxis = FVector(0.25, -0.15, 1.0).GetSafeNormal();
		/** Arbitrary authored socket orientation; the blade turns it rigidly. */
		FQuat SocketRestRotation = FQuat(FRotator(80.0, 15.0, 30.0));
		TArray<double> SocketRadii = {50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0, 140.0, 150.0};
		static constexpr double TipRadius = 150.0;

		FTransform MakeTracePoint(const FTransform& Body, const double SwingDegrees, const double Radius) const
		{
			const FQuat Swing(SwingAxis, FMath::DegreesToRadians(SwingDegrees));
			const FVector ArmDirection = FVector::VectorPlaneProject(FVector::ForwardVector, SwingAxis).GetSafeNormal();
			const FTransform BodyRelative(Swing * SocketRestRotation, ShoulderPivot + Swing.RotateVector(ArmDirection * Radius));
			return BodyRelative * Body;
		}

		FBladeSample MakeSample(const FTransform& Body, const double SwingDegrees) const
		{
			FBladeSample Sample;
			Sample.BodyTransform = Body;
			for (const double Radius : SocketRadii)
			{
				Sample.TracePointTransforms.Add(MakeTracePoint(Body, SwingDegrees, Radius));
			}
			return Sample;
		}
	};

	FTransform LerpBody(const FTransform& From, const FTransform& To, const double Alpha)
	{
		return FTransform(
			FQuat::Slerp(From.GetRotation(), To.GetRotation(), Alpha),
			FMath::Lerp(From.GetLocation(), To.GetLocation(), Alpha),
			From.GetScale3D());
	}

	/** Closest distance from Target to the trace segments the ability sweeps between two rows: each point's path and the blade. */
	double GetClosestTraceDistance(const TArray<TArray<FVector>>& Rows, const FVector& Target)
	{
		double Closest = TNumericLimits<double>::Max();
		for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
		{
			const TArray<FVector>& Row = Rows[RowIndex];
			for (int32 PointIndex = 0; PointIndex < Row.Num(); ++PointIndex)
			{
				if (RowIndex + 1 < Rows.Num())
				{
					Closest = FMath::Min(Closest, static_cast<double>(FMath::PointDistToSegment(Target, Row[PointIndex], Rows[RowIndex + 1][PointIndex])));
				}
				if (PointIndex + 1 < Row.Num())
				{
					Closest = FMath::Min(Closest, static_cast<double>(FMath::PointDistToSegment(Target, Row[PointIndex], Row[PointIndex + 1])));
				}
			}
		}
		return Closest;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponTraceMultiFrameJumpTest,
	"SurvivalRpg.Combat.WeaponTrace.MultiFrameJumpFollowsSwingArc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponTraceMultiFrameJumpTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponTraceInterpolationTests;

	// A remote client's montage that stood still for a frame and then advanced several frames at once: between two
	// server samples the blade turned 75 degrees about the shoulder while root motion moved and turned the body.
	const FSwingModel Swing;
	const FTransform PreviousBody(FRotator(0.0, -60.0, 0.0), FVector(1200.0, -350.0, 90.0), FVector(1.1));
	const FTransform CurrentBody(FRotator(0.0, -56.0, 0.0), FVector(1206.0, -341.0, 90.0), FVector(1.1));
	constexpr double PreviousSwingDegrees = -40.0;
	constexpr double CurrentSwingDegrees = 35.0;
	const FBladeSample Previous = Swing.MakeSample(PreviousBody, PreviousSwingDegrees);
	const FBladeSample Current = Swing.MakeSample(CurrentBody, CurrentSwingDegrees);

	const int32 SubstepCount = RpgWeaponTraceInterpolation::ComputeSubstepCount(Previous, Current);
	const int32 ExpectedSubsteps = FMath::CeilToInt32((75.0 + 4.0) / RpgWeaponTraceInterpolation::MaxSubstepAngleDegrees);
	TestEqual(TEXT("The blade's and the body's turn split the jump into sub-steps of at most the sub-step angle"), SubstepCount, ExpectedSubsteps);

	TArray<TArray<FVector>> Rows;
	RpgWeaponTraceInterpolation::BuildSubstepLocations(Previous, Current, SubstepCount, Rows);
	if (!TestEqual(TEXT("One row per sub-step boundary"), Rows.Num(), SubstepCount + 1))
	{
		return false;
	}

	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const double Alpha = static_cast<double>(RowIndex) / static_cast<double>(SubstepCount);
		const FTransform Body = LerpBody(PreviousBody, CurrentBody, Alpha);
		const double SwingDegrees = FMath::Lerp(PreviousSwingDegrees, CurrentSwingDegrees, Alpha);
		for (int32 PointIndex = 0; PointIndex < Swing.SocketRadii.Num(); ++PointIndex)
		{
			const FVector OnArc = Swing.MakeTracePoint(Body, SwingDegrees, Swing.SocketRadii[PointIndex]).GetLocation();
			TestTrue(
				FString::Printf(TEXT("Sub-step %d keeps trace point %d on its swing arc"), RowIndex, PointIndex),
				Rows[RowIndex][PointIndex].Equals(OnArc, 0.01));
		}
	}
	TestTrue(TEXT("The first row is the previous sample"), Rows[0].Last().Equals(Previous.TracePointTransforms.Last().GetLocation(), UE_DOUBLE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("The last row is the current sample"), Rows.Last().Last().Equals(Current.TracePointTransforms.Last().GetLocation(), UE_DOUBLE_KINDA_SMALL_NUMBER));

	// A thin target the blade's outer part sweeps through between two sub-step rows, as the PIE target on the arc.
	constexpr double TraceRadius = 12.0;
	constexpr double TargetAlpha = 0.42;
	const FVector Target = Swing.MakeTracePoint(
		LerpBody(PreviousBody, CurrentBody, TargetAlpha),
		FMath::Lerp(PreviousSwingDegrees, CurrentSwingDegrees, TargetAlpha),
		145.0).GetLocation();

	TArray<TArray<FVector>> SingleChord;
	RpgWeaponTraceInterpolation::BuildSubstepLocations(Previous, Current, 1, SingleChord);
	const double ChordDistance = GetClosestTraceDistance(SingleChord, Target);
	const double SubstepDistance = GetClosestTraceDistance(Rows, Target);
	AddInfo(FString::Printf(TEXT("Closest trace to the target: single chord %.1f cm, %d sub-steps %.1f cm"), ChordDistance, SubstepCount, SubstepDistance));
	TestTrue(TEXT("One straight chord across the jump passes beside the target"), ChordDistance > TraceRadius);
	TestTrue(TEXT("The sub-stepped arc sweeps through the target"), SubstepDistance <= TraceRadius);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponTraceSubstepCountTest,
	"SurvivalRpg.Combat.WeaponTrace.SubstepCountFollowsTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponTraceSubstepCountTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponTraceInterpolationTests;
	using RpgWeaponTraceInterpolation::ComputeSubstepCount;
	using RpgWeaponTraceInterpolation::MaxSubstepCount;

	const FSwingModel Swing;
	const FTransform Body(FRotator(0.0, 20.0, 0.0), FVector(100.0, 200.0, 90.0));
	const FBladeSample Rest = Swing.MakeSample(Body, 0.0);

	TestEqual(TEXT("An unchanged pose traces once"), ComputeSubstepCount(Rest, Rest), 1);
	TestEqual(TEXT("A frame's usual turn keeps one straight sweep"), ComputeSubstepCount(Rest, Swing.MakeSample(Body, 12.0)), 1);
	TestEqual(TEXT("A 70 degree jump splits into five sub-steps of at most 15 degrees"), ComputeSubstepCount(Rest, Swing.MakeSample(Body, 70.0)), 5);
	TestEqual(TEXT("An extreme jump stays bounded"), ComputeSubstepCount(Rest, Swing.MakeSample(Body * FTransform(FRotator(0.0, 40.0, 0.0)), 175.0)), MaxSubstepCount);

	// Walking without turning stays one straight sweep.
	const FTransform MovedBody(Body.GetRotation(), Body.GetLocation() + FVector(50.0, -20.0, 0.0));
	const FBladeSample Moved = Swing.MakeSample(MovedBody, 0.0);
	TestEqual(TEXT("Pure translation traces once"), ComputeSubstepCount(Rest, Moved), 1);

	// The body turning with a held blade carries the blade around the body.
	const FTransform TurnedBody(FRotator(0.0, 45.0, 0.0), Body.GetLocation());
	const FBladeSample Turned = Swing.MakeSample(TurnedBody, 0.0);
	TestEqual(TEXT("A 25 degree body turn splits into two sub-steps"), ComputeSubstepCount(Rest, Turned), 2);
	TArray<TArray<FVector>> Rows;
	RpgWeaponTraceInterpolation::BuildSubstepLocations(Rest, Turned, 2, Rows);
	const FVector HalfTurnTip = Swing.MakeTracePoint(LerpBody(Body, TurnedBody, 0.5), 0.0, FSwingModel::TipRadius).GetLocation();
	TestTrue(TEXT("The held blade's tip follows the body's turn"), Rows.Num() == 3 && Rows[1].Last().Equals(HalfTurnTip, 0.01));

	// Samples with different trace points, for example across a weapon swap, never interpolate.
	FBladeSample Shorter = Rest;
	Shorter.TracePointTransforms.Pop();
	TestEqual(TEXT("Mismatched samples trace once"), ComputeSubstepCount(Rest, Shorter), 1);
	RpgWeaponTraceInterpolation::BuildSubstepLocations(Rest, Shorter, 3, Rows);
	TestTrue(TEXT("Mismatched samples build no rows"), Rows.IsEmpty());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponTraceScrewMotionTest,
	"SurvivalRpg.Combat.WeaponTrace.ScrewMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponTraceScrewMotionTest::RunTest(const FString& Parameters)
{
	using RpgWeaponTraceInterpolation::InterpolateScrewLocation;

	// A quarter turn about a vertical line through (100, 0, 0) that also slides 20 cm up: a helix.
	const FVector Pivot(100.0, 0.0, 0.0);
	const FQuat QuarterTurn(FVector::UpVector, UE_DOUBLE_HALF_PI);
	const FTransform From(FQuat::Identity, FVector(0.0, 0.0, 0.0));
	const FTransform To(QuarterTurn, Pivot + QuarterTurn.RotateVector(-Pivot) + FVector(0.0, 0.0, 20.0));
	const FQuat EighthTurn(FVector::UpVector, UE_DOUBLE_HALF_PI * 0.5);
	const FVector ExpectedHalfway = Pivot + EighthTurn.RotateVector(-Pivot) + FVector(0.0, 0.0, 10.0);
	TestTrue(TEXT("The frame origin follows the helix"), InterpolateScrewLocation(From, To, 0.5).Equals(ExpectedHalfway, 0.001));

	const FVector Offset(30.0, 10.0, 5.0);
	const FVector ExpectedOffsetHalfway = Pivot + EighthTurn.RotateVector(Offset - Pivot) + FVector(0.0, 0.0, 10.0);
	TestTrue(TEXT("A point fixed in the frame follows the same helix"), InterpolateScrewLocation(From, To, 0.5, Offset).Equals(ExpectedOffsetHalfway, 0.001));
	TestTrue(TEXT("Alpha 0 is the start"), InterpolateScrewLocation(From, To, 0.0, Offset).Equals(Offset, 0.001));
	TestTrue(TEXT("Alpha 1 is the end"), InterpolateScrewLocation(From, To, 1.0, Offset).Equals(To.TransformPosition(Offset), 0.001));

	const FTransform Shifted(FQuat::Identity, FVector(40.0, -30.0, 12.0));
	TestTrue(TEXT("Pure translation stays a straight line"), InterpolateScrewLocation(From, Shifted, 0.25).Equals(FVector(10.0, -7.5, 3.0), 0.001));
	return !HasAnyErrors();
}

#endif
