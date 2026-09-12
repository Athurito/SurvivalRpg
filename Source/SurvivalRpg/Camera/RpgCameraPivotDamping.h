#pragma once

#include "CoreMinimal.h"

/** Cosmetic position history for a critically damped camera pivot; contains no gameplay or replicated state. */
struct FRpgCameraPivotDamping
{
	/** Forget history when a camera mode activates or its view target changes. */
	void Reset() { bInitialized = false; }

	/** Track a linearly moving target in cm. Factor is angular frequency in rad/s; zero disables damping. */
	FVector Update(const FVector& Target, float DeltaTime, float Factor, float ResetDistance, bool bReset = false)
	{
		if (!bInitialized || bReset || Factor <= UE_SMALL_NUMBER
			|| (ResetDistance > 0.0f && FVector::DistSquared(Target, PreviousTarget) > FMath::Square(ResetDistance)))
		{
			bInitialized = true;
			PreviousTarget = Position = Target;
			Lag = LagVelocity = FVector::ZeroVector;
			return Position;
		}
		if (DeltaTime <= 0.0f)
		{
			return Position;
		}

		// Solve the driven critical spring over this frame, including the target's movement.
		// This is the moving-base solution used by GameplayCameras' FCriticalDamper in GASP.
		// Keeping the solver value-only avoids adopting the sample camera runtime/composition.
		const FVector Movement = Target - PreviousTarget;
		const FVector TargetVelocity = Movement / DeltaTime;
		const FVector P0 = 2.0 * TargetVelocity / Factor;
		const FVector A = Lag - P0;
		const FVector B = LagVelocity + Factor * A + TargetVelocity;
		const double Decay = FMath::Exp(-static_cast<double>(Factor) * DeltaTime);
		Lag = (A + B * DeltaTime) * Decay + P0;
		LagVelocity = (B - Factor * A - Factor * B * DeltaTime) * Decay - TargetVelocity;
		if (Lag.IsNearlyZero(0.001) && LagVelocity.IsNearlyZero(0.001))
		{
			Lag = LagVelocity = FVector::ZeroVector;
		}
		PreviousTarget = Target;
		Position = Target - Lag;
		return Position;
	}

private:
	FVector PreviousTarget = FVector::ZeroVector;
	FVector Position = FVector::ZeroVector;
	FVector Lag = FVector::ZeroVector;
	FVector LagVelocity = FVector::ZeroVector;
	bool bInitialized = false;
};
