// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Backends/MoverNetworkPredictionLiaison.h"
#include "Engine/World.h"
#include "NetworkPredictionWorldManager.h"

namespace RpgMoverPredictionTests
{
	/** Fixed prediction head sampled around network dispatch, before another forward simulation may run. */
	struct FFixedPredictionHeadSnapshot
	{
		static FFixedPredictionHeadSnapshot Capture(UWorld* World, UMoverNetworkPredictionLiaisonComponent* Liaison)
		{
			FFixedPredictionHeadSnapshot Result;
			const UNetworkPredictionWorldManager* Prediction = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
			if (!Prediction || !Liaison || Prediction->GetSettings().PreferredTickingPolicy != ENetworkPredictionTickingPolicy::Fixed)
				return Result;
			const FFixedTickState& Clock = Prediction->GetFixedTickState();
			Result.LocalPendingFrame = Clock.PendingFrame;
			Result.ServerOffset = Clock.Offset;
			Result.StepMs = Clock.FixedStepMS;
			Result.ServerFrame = Liaison->GetCurrentSimFrame();
			Result.SimulationTimeMs = Liaison->GetCurrentSimTimeMs();
			Result.bValid = Clock.PendingFrame > 0 && Clock.FixedStepMS > 0;
			return Result;
		}

		bool IsSameLocalHead(const FFixedPredictionHeadSnapshot& After) const
		{
			// Every fixed forward tick advances the world's local PendingFrame; rollback restores its
			// saved head. Equality across this synchronous dispatch bracket therefore excludes forward work.
			// Liaison frame/time are cached per-instance views: rollback can refresh a stale view while
			// AP NetRecv changes the world offset independently. Keep those values for diagnostics only.
			return bValid && After.bValid && LocalPendingFrame == After.LocalPendingFrame && StepMs == After.StepMs;
		}

		int32 LocalPendingFrame = INDEX_NONE;
		int32 ServerOffset = 0;
		int32 StepMs = 0;
		int32 ServerFrame = INDEX_NONE;
		double SimulationTimeMs = 0.0;
		bool bValid = false;
	};
}
