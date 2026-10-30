#include "RpgWeaponTraceInterpolation.h"

namespace RpgWeaponTraceInterpolation
{
	namespace
	{
		/** Below this turn in radians a screw degenerates to a straight line; the arc it skips is far below a millimeter. */
		constexpr double MinScrewAngleRadians = 1.0e-4;

		/** Transform of Point in the space of Body, without scale. */
		FTransform MakeBodyRelative(const FTransform& Body, const FTransform& Point)
		{
			const FQuat BodyRotation = Body.GetRotation().GetNormalized();
			return FTransform(
				BodyRotation.Inverse() * Point.GetRotation().GetNormalized(),
				BodyRotation.UnrotateVector(Point.GetLocation() - Body.GetLocation()));
		}

		/** Shortest turn from From to To, as unit axis and angle in radians (0..pi); stable where acos is not. */
		double GetShortestTurn(const FQuat& From, const FQuat& To, FVector& OutAxis)
		{
			const FQuat Turn = To * From.Inverse();
			const FVector TurnVector(Turn.X, Turn.Y, Turn.Z);
			const double SinHalfAngle = TurnVector.Size();
			OutAxis = SinHalfAngle > UE_DOUBLE_SMALL_NUMBER ? TurnVector / SinHalfAngle : FVector::XAxisVector;
			if (Turn.W < 0.0)
			{
				OutAxis = -OutAxis;
			}
			return 2.0 * FMath::Atan2(SinHalfAngle, FMath::Abs(Turn.W));
		}

		bool DoSamplesMatch(const FBladeSample& Previous, const FBladeSample& Current)
		{
			return !Previous.IsEmpty() && Previous.TracePointTransforms.Num() == Current.TracePointTransforms.Num();
		}
	}

	void FBladeSample::Reset()
	{
		BodyTransform = FTransform::Identity;
		TracePointTransforms.Reset();
	}

	void FBladeSample::GetTracePointLocations(TArray<FVector>& OutLocations) const
	{
		OutLocations.Reset(TracePointTransforms.Num());
		for (const FTransform& TracePointTransform : TracePointTransforms)
		{
			OutLocations.Add(TracePointTransform.GetLocation());
		}
	}

	int32 ComputeSubstepCount(const FBladeSample& Previous, const FBladeSample& Current)
	{
		if (!DoSamplesMatch(Previous, Current))
		{
			return 1;
		}

		double LargestRelativeTurn = 0.0;
		for (int32 PointIndex = 0; PointIndex < Previous.TracePointTransforms.Num(); ++PointIndex)
		{
			const FQuat PreviousRotation = MakeBodyRelative(Previous.BodyTransform, Previous.TracePointTransforms[PointIndex]).GetRotation();
			const FQuat CurrentRotation = MakeBodyRelative(Current.BodyTransform, Current.TracePointTransforms[PointIndex]).GetRotation();
			FVector UnusedAxis;
			LargestRelativeTurn = FMath::Max(LargestRelativeTurn, GetShortestTurn(PreviousRotation, CurrentRotation, UnusedAxis));
		}

		FVector UnusedAxis;
		const double BodyTurn = GetShortestTurn(
			Previous.BodyTransform.GetRotation().GetNormalized(),
			Current.BodyTransform.GetRotation().GetNormalized(),
			UnusedAxis);
		const double TotalTurnDegrees = FMath::RadiansToDegrees(LargestRelativeTurn + BodyTurn);
		return FMath::Clamp(FMath::CeilToInt32(TotalTurnDegrees / MaxSubstepAngleDegrees), 1, MaxSubstepCount);
	}

	void BuildSubstepLocations(
		const FBladeSample& Previous,
		const FBladeSample& Current,
		const int32 SubstepCount,
		TArray<TArray<FVector>>& OutSubstepLocations)
	{
		OutSubstepLocations.Reset();
		if (!DoSamplesMatch(Previous, Current))
		{
			return;
		}

		const int32 StepCount = FMath::Clamp(SubstepCount, 1, MaxSubstepCount);
		const int32 PointCount = Previous.TracePointTransforms.Num();
		OutSubstepLocations.SetNum(StepCount + 1);
		Previous.GetTracePointLocations(OutSubstepLocations[0]);
		Current.GetTracePointLocations(OutSubstepLocations[StepCount]);
		if (StepCount == 1)
		{
			return;
		}

		TArray<FTransform> PreviousRelative;
		TArray<FTransform> CurrentRelative;
		PreviousRelative.Reserve(PointCount);
		CurrentRelative.Reserve(PointCount);
		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			PreviousRelative.Add(MakeBodyRelative(Previous.BodyTransform, Previous.TracePointTransforms[PointIndex]));
			CurrentRelative.Add(MakeBodyRelative(Current.BodyTransform, Current.TracePointTransforms[PointIndex]));
		}

		const FQuat PreviousBodyRotation = Previous.BodyTransform.GetRotation().GetNormalized();
		const FQuat CurrentBodyRotation = Current.BodyTransform.GetRotation().GetNormalized();
		for (int32 StepIndex = 1; StepIndex < StepCount; ++StepIndex)
		{
			const double Alpha = static_cast<double>(StepIndex) / static_cast<double>(StepCount);
			const FVector BodyLocation = FMath::Lerp(Previous.BodyTransform.GetLocation(), Current.BodyTransform.GetLocation(), Alpha);
			const FQuat BodyRotation = FQuat::Slerp(PreviousBodyRotation, CurrentBodyRotation, Alpha);

			TArray<FVector>& StepLocations = OutSubstepLocations[StepIndex];
			StepLocations.Reserve(PointCount);
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				const FVector RelativeLocation = InterpolateScrewLocation(PreviousRelative[PointIndex], CurrentRelative[PointIndex], Alpha);
				StepLocations.Add(BodyLocation + BodyRotation.RotateVector(RelativeLocation));
			}
		}
	}

	FVector InterpolateScrewLocation(const FTransform& From, const FTransform& To, const double Alpha, const FVector& LocalOffset)
	{
		const FQuat FromRotation = From.GetRotation().GetNormalized();
		const FQuat ToRotation = To.GetRotation().GetNormalized();
		const FVector FromPoint = From.GetLocation() + FromRotation.RotateVector(LocalOffset);
		const FVector ToPoint = To.GetLocation() + ToRotation.RotateVector(LocalOffset);

		// The rigid motion x' = Turn * x + Shift carries From onto To; take its shortest turn.
		FVector Axis;
		const double Angle = GetShortestTurn(FromRotation, ToRotation, Axis);
		if (Angle < MinScrewAngleRadians)
		{
			return FMath::Lerp(FromPoint, ToPoint, Alpha);
		}

		// Chasles: the motion turns about a line along Axis through Pivot and slides Slide along it. Pivot solves
		// (I - Turn) * Pivot = the shift's part across the axis.
		const FQuat Turn(Axis, Angle);
		const FVector Shift = To.GetLocation() - Turn.RotateVector(From.GetLocation());
		const double Slide = FVector::DotProduct(Shift, Axis);
		const FVector CrossShift = Shift - (Slide * Axis);
		const FVector Pivot = 0.5 * (CrossShift + (FVector::CrossProduct(Axis, CrossShift) / FMath::Tan(0.5 * Angle)));

		const FQuat PartialTurn(Axis, Alpha * Angle);
		return Pivot + PartialTurn.RotateVector(FromPoint - Pivot) + (Alpha * Slide * Axis);
	}
}
