// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/RpgMoverPredictionTestTypes.h"
#include "Animation/AnimMontage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgMoverPredictionTestTypes)

void URpgMoverRollbackTestObserver::ObserveRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep)
{
	++Count;
	LastRestored = CurrentTimeStep;
	LastExpunged = ExpungedTimeStep;
}

void URpgMoverTraversalNotifyTestObserver::AddMontage(UAnimMontage* Montage)
{
	Montages.Add(Montage);
}

void URpgMoverTraversalNotifyTestObserver::ObserveBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	Observe(NotifyName, Payload, true);
}

void URpgMoverTraversalNotifyTestObserver::ObserveEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	Observe(NotifyName, Payload, false);
}

void URpgMoverTraversalNotifyTestObserver::Observe(FName NotifyName, const FBranchingPointNotifyPayload& Payload, bool bBegin)
{
	if (!Montages.Contains(Payload.SequenceAsset) || !Payload.NotifyEvent || Payload.MontageInstanceID == INDEX_NONE) return;
	const float Start = Payload.NotifyEvent->GetTriggerTime(), End = Payload.NotifyEvent->GetEndTriggerTime();
	FWindow* Window = Windows.FindByPredicate([&](const FWindow& Candidate)
	{
		return Candidate.Montage.Get() == Payload.SequenceAsset && Candidate.InstanceId == Payload.MontageInstanceID
			&& Candidate.Name == NotifyName && Candidate.StartTime == Start && Candidate.EndTime == End;
	});
	if (!Window)
	{
		Window = &Windows.AddDefaulted_GetRef();
		Window->Montage = Payload.SequenceAsset; Window->Name = NotifyName;
		Window->InstanceId = Payload.MontageInstanceID; Window->StartTime = Start; Window->EndTime = End;
	}
	if (bBegin) ++Window->Begins;
	else ++Window->Ends;
}

bool URpgMoverTraversalNotifyTestObserver::HasDuplicateCallbacks() const
{
	return Windows.ContainsByPredicate([](const FWindow& Window) { return Window.Begins > 1 || Window.Ends > 1; });
}

int32 URpgMoverTraversalNotifyTestObserver::ObservedBegins() const
{
	int32 Count = 0;
	for (const FWindow& Window : Windows) Count += Window.Begins;
	return Count;
}
