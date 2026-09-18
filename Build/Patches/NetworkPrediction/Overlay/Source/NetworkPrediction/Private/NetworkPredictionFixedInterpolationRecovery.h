#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionTickState.h"

// Project-maintained UE 5.8.2 patch. These helpers own only the presentation clock.
// Simulation, input, rollback and cue histories retain the engine's ownership.
namespace UE::NetworkPrediction::FixedInterpolationRecovery
{

// The pinned engine's TInstanceFrameState constructor allocates 64 frames.
// Leave a valid FromFrame and ToFrame inside that ring even for invalid tuning.
inline constexpr int32 HistoryCapacity = 64;

inline int32 DesiredFrames(const FFixedTickState& State, int32 DesiredBufferedMS)
{
	return FMath::Clamp(DesiredBufferedMS / FMath::Max(1, State.FixedStepMS), 1, HistoryCapacity - 2);
}

inline int32 MaxFrames(const FFixedTickState& State, int32 DesiredBufferedMS, int32 MaxBufferedMS)
{
	return FMath::Clamp(FMath::Max(MaxBufferedMS / FMath::Max(1, State.FixedStepMS),
		DesiredFrames(State, DesiredBufferedMS) + 1), 2, HistoryCapacity - 2);
}

inline void UpdateTime(FFixedTickState& State)
{
	auto& Clock = State.Interpolation;
	Clock.PCT = FMath::Clamp(Clock.AccumulatedTimeMS / State.FixedStepRealTimeMS, 0.f, 1.f);
	Clock.InterpolatedTimeMS = (Clock.ToFrame - 1) * State.FixedStepMS +
		static_cast<int32>(Clock.PCT * State.FixedStepMS);
}

/** Repair an overrun before services read their bounded history; never rewind presentation. */
inline bool Recover(FFixedTickState& State, int32 DesiredBufferedMS, int32 MaxBufferedMS)
{
	auto& Clock = State.Interpolation;
	const int32 Latest = FMath::Max(Clock.LatestRecvFrameAP, Clock.LatestRecvFrameSP);
	if (Clock.ToFrame == INDEX_NONE || Latest == INDEX_NONE || State.FixedStepMS <= 0 ||
		State.FixedStepRealTimeMS <= 0.f || Latest - Clock.ToFrame <= MaxFrames(State, DesiredBufferedMS, MaxBufferedMS))
	{
		return false;
	}
	Clock.ToFrame = FMath::Max(1, Latest - DesiredFrames(State, DesiredBufferedMS));
	Clock.AccumulatedTimeMS = 0.f;
	UpdateTime(State);
	return true;
}

/** Advance once per rendered frame without replaying an entire hitch or outrunning received history. */
inline void Advance(FFixedTickState& State, float DeltaTimeMS, bool bRecoveredThisFrame = false)
{
	auto& Clock = State.Interpolation;
	const int32 Latest = FMath::Max(Clock.LatestRecvFrameAP, Clock.LatestRecvFrameSP);
	if (Clock.ToFrame == INDEX_NONE || Latest == INDEX_NONE || State.FixedStepRealTimeMS <= 0.f)
	{
		return;
	}

	// Reconcile may already have recentered the clock this frame. Do not immediately
	// consume the new target buffer with the old wall-clock delta from the same hitch.
	if (bRecoveredThisFrame || !FMath::IsFinite(DeltaTimeMS))
	{
		return;
	}

	Clock.AccumulatedTimeMS += FMath::Max(0.f, DeltaTimeMS);
	const int32 AvailableFrames = FMath::Max(0, Latest - Clock.ToFrame);
	const int32 AdvanceFrames = FMath::FloorToInt(Clock.AccumulatedTimeMS / State.FixedStepRealTimeMS);
	if (AdvanceFrames > AvailableFrames)
	{
		Clock.ToFrame += AvailableFrames;
		Clock.AccumulatedTimeMS = State.FixedStepRealTimeMS;
	}
	else
	{
		Clock.ToFrame += AdvanceFrames;
		Clock.AccumulatedTimeMS -= AdvanceFrames * State.FixedStepRealTimeMS;
	}
	UpdateTime(State);
}

}
