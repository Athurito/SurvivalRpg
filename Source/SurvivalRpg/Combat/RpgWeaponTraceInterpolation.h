#pragma once

#include "CoreMinimal.h"

/**
 * Sub-stepping of a melee blade trace between two pose samples.
 *
 * The attack ability samples its blade about once per frame and sweeps from each sample to the next. When the pose jumps
 * several frames at once, for example a remote client's montage on the server, which advances with every received move,
 * one straight chord between two samples cuts inside the swing arc and can pass beside a target the blade swept through.
 * These helpers rebuild the arc between the two samples: each trace point's transform relative to the attacker's body
 * moves as one screw motion (the rotation the samples imply, about the axis they imply, plus the remaining translation
 * along it), so a blade turning about a fixed pivot in body space, such as an arm swung from the shoulder, stays on its
 * arc together with its grip. The body itself moves linearly with a slerped rotation.
 *
 * Pure math shared by the authority's damage trace and the owner's impact feedback trace; authority stays with the caller.
 */
namespace RpgWeaponTraceInterpolation
{
	/** Largest rotation in degrees that one sub-step may cover; a 150 cm blade then deviates at most about 1.3 cm from its arc. */
	constexpr float MaxSubstepAngleDegrees = 15.0f;

	/** Most sub-steps between two samples, which bounds the trace cost of an extreme jump (12 x 15 = 180 degrees). */
	constexpr int32 MaxSubstepCount = 12;

	/** One blade pose sample: the attacker's body transform and the world transform of every trace point, in trace order. */
	struct FBladeSample
	{
		/** World transform of the attacker's body, normally its gameplay mesh; scale is ignored. */
		FTransform BodyTransform = FTransform::Identity;

		/** World transform of each trace socket or component; each one's rotation turns with the blade. Scale is ignored. */
		TArray<FTransform> TracePointTransforms;

		bool IsEmpty() const { return TracePointTransforms.IsEmpty(); }
		void Reset();
		void GetTracePointLocations(TArray<FVector>& OutLocations) const;
	};

	/**
	 * Number of sub-steps from Previous to Current: the largest rotation of a trace point, relative to the body plus the
	 * body's own turn, divided by MaxSubstepAngleDegrees and clamped to 1..MaxSubstepCount. 1 for samples that do not match.
	 */
	SURVIVALRPG_API int32 ComputeSubstepCount(const FBladeSample& Previous, const FBladeSample& Current);

	/**
	 * Fills OutSubstepLocations with SubstepCount + 1 rows of trace point world locations from Previous to Current, one row
	 * per sub-step boundary. The first and last rows are the samples' own locations. Empty when the samples do not match.
	 */
	SURVIVALRPG_API void BuildSubstepLocations(
		const FBladeSample& Previous,
		const FBladeSample& Current,
		int32 SubstepCount,
		TArray<TArray<FVector>>& OutSubstepLocations);

	/**
	 * Location of a point that moves rigidly with From at Alpha (0 = From, 1 = To) along the screw motion from From to To.
	 * LocalOffset is the point in From's and To's local space; scale is ignored. Pure translation stays a straight line.
	 */
	SURVIVALRPG_API FVector InterpolateScrewLocation(const FTransform& From, const FTransform& To, double Alpha, const FVector& LocalOffset = FVector::ZeroVector);
}
