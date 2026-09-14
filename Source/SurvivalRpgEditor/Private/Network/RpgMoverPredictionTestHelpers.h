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
			// Fixed AP NetRecv can change the server-frame offset without advancing the local input head.
			// Rollback restores that same local head, but its liaison frame/time then include the new offset.
			// Verify both clocks after removing exactly that offset; any genuine forward tick still fails.
			return bValid && After.bValid && LocalPendingFrame == After.LocalPendingFrame && StepMs == After.StepMs
				&& ServerFrame - ServerOffset == After.ServerFrame - After.ServerOffset
				&& SimulationTimeMs - static_cast<double>(ServerOffset) * StepMs
					== After.SimulationTimeMs - static_cast<double>(After.ServerOffset) * After.StepMs;
		}

		int32 LocalPendingFrame = INDEX_NONE;
		int32 ServerOffset = 0;
		int32 StepMs = 0;
		int32 ServerFrame = INDEX_NONE;
		double SimulationTimeMs = 0.0;
		bool bValid = false;
	};
}
