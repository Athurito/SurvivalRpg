#pragma once

#include "Engine/World.h"
#include "TimerManager.h"

/**
 * Coalesces presentation-only invalidations until the next world tick.
 *
 * Each view model owns its own queue. Gameplay messages still arrive
 * synchronously; only the read-only UI rebuild triggered by those messages is
 * deferred. Explicit refresh, bind, and unbind paths should cancel the queue
 * before publishing their synchronous final state.
 */
class FRpgViewModelInvalidationQueue
{
public:
	/** Schedules Callback once; additional requests remain coalesced. */
	template <typename UserClass>
	void Queue(
		UWorld* World,
		UserClass* Target,
		void (UserClass::*Callback)())
	{
		if (bQueued || !World || !Target)
		{
			return;
		}

		const uint32 ScheduledGeneration = ++Generation;
		bQueued = true;
		QueuedWorld = World;
		FTimerDelegate TimerDelegate = FTimerDelegate::CreateWeakLambda(
			Target,
			[this, Target, Callback, ScheduledGeneration]()
			{
				if (!bQueued || Generation != ScheduledGeneration)
				{
					return;
				}

				(Target->*Callback)();
			});

		TimerHandle =
			World->GetTimerManager().SetTimerForNextTick(
				TimerDelegate);
	}

	/** Call first inside Callback so reentrant requests can queue the next tick. */
	bool Consume()
	{
		if (!bQueued)
		{
			return false;
		}

		ResetState();
		return true;
	}

	/** Cancels a pending callback. Safe during rebind, unbind, and destruction. */
	void Cancel()
	{
		++Generation;
		if (bQueued)
		{
			if (UWorld* World = QueuedWorld.Get())
			{
				World->GetTimerManager().ClearTimer(TimerHandle);
			}
		}

		ResetState();
	}

private:
	void ResetState()
	{
		bQueued = false;
		QueuedWorld.Reset();
		TimerHandle.Invalidate();
	}

	TWeakObjectPtr<UWorld> QueuedWorld;
	FTimerHandle TimerHandle;
	uint32 Generation = 0;
	bool bQueued = false;
};
