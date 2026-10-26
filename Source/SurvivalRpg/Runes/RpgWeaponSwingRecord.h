#pragma once

#include "CoreMinimal.h"
#include "RpgWeaponSwingRecord.generated.h"

/** Shape of one recorded weapon trace query. */
UENUM()
enum class ERpgRecordedTraceShape : uint8
{
	Line,
	Sphere,
	Capsule,
	Box,
};

/** One trace query an authority attack window ran, kept so the swing can be replayed. Server-only runtime data. */
USTRUCT()
struct SURVIVALRPG_API FRpgRecordedWeaponTrace
{
	GENERATED_BODY()

	/** Seconds after the attack window opened. */
	float TimeOffset = 0.0f;

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	ERpgRecordedTraceShape Shape = ERpgRecordedTraceShape::Line;

	/** Sphere radius in X; capsule radius in X and half height in Y; box half extents. Unused for lines. */
	FVector ShapeExtent = FVector::ZeroVector;
};

/** The recorded trace queries of one authority attack window, in time order. Server-only runtime data. */
USTRUCT()
struct SURVIVALRPG_API FRpgWeaponSwingRecord
{
	GENERATED_BODY()

	/** World time in seconds at which the window opened. */
	float WindowStartTime = 0.0f;

	/** World time in seconds at which the window closed. */
	float WindowEndTime = 0.0f;

	TArray<FRpgRecordedWeaponTrace> Traces;

	float GetDuration() const { return FMath::Max(0.0f, WindowEndTime - WindowStartTime); }

	/**
	 * Index one past the last trace that is due after Elapsed seconds of replay, scanning from FirstIndex. A replay
	 * runs the traces in [FirstIndex, result) and continues from there.
	 */
	int32 FindDueEnd(int32 FirstIndex, float Elapsed) const;
};

/**
 * The Echo charge rule, kept apart from the weapon so it can be tested: swings that land a hit add one, swings into the
 * air add nothing, the trigger count fires and empties the charge, and a pause longer than the timeout expires it.
 * Server-side runtime state.
 */
struct SURVIVALRPG_API FRpgEchoStrikeCharge
{
	int32 Charge = 0;
	float LastChargeTime = 0.0f;

	/** Applies one swing at Now and returns true when it triggers the strike, which leaves the charge empty. */
	bool RegisterSwing(bool bLandedHit, int32 HitsToTrigger, float ChargeTimeout, float Now);

	/** Empties a charge whose last hit is older than ChargeTimeout at Now; zero never expires. True when it changed. */
	bool Expire(float ChargeTimeout, float Now);

	void Reset() { Charge = 0; }
};
