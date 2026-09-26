// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/RpgMoverPredictionTestTypes.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "NetworkPredictionWorldManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgMoverPredictionTestTypes)

void URpgMoverRollbackTestObserver::ObserveRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep)
{
	++Count;
	LastRestored = CurrentTimeStep;
	LastExpunged = ExpungedTimeStep;

	if (!bDispatchActive || !TrackedMover.IsValid()) return;
	// A later restore starts another epoch, so steps or snapshots from the previous one cannot certify it.
	ResetFrameEvidence();
	const UWorld* World = TrackedMover->GetWorld();
	const UNetworkPredictionWorldManager* Prediction = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
	if (!Prediction || Prediction->GetSettings().PreferredTickingPolicy != ENetworkPredictionTickingPolicy::Fixed) return;
	const FFixedTickState& Clock = Prediction->GetFixedTickState();
	const int32 LocalFrame = Clock.PendingFrame;
	if (LocalFrame < 0 || LocalFrame >= DispatchLocalHead
		|| Clock.FixedStepMS <= 0 || CurrentTimeStep.ServerFrame != LocalFrame + Clock.Offset + 1)
	{
		InvalidateHistoryFrom(FMath::Max(0, LocalFrame));
		return;
	}

	RestoredLocalFrame = LocalFrame;
	NextReplayInputFrame = LocalFrame;
	// Mover applies the restored snapshot before broadcasting OnPostSimulationRollback.
	RestoredSync = TrackedMover->GetSyncState();
	const int32 CandidateFrame = FMath::Max(LocalFrame, TrackedLocalFrame);
	if (const FMoverSyncState* Recorded = FindRecordedFrame(CandidateFrame))
	{
		ReplacedLocalFrame = CandidateFrame;
		PredictedSync = *Recorded;
		if (LocalFrame == CandidateFrame)
		{
			ReplacementSync = RestoredSync;
			bHasReplacement = true;
		}
	}
	// Preserve this epoch's original separately, then discard invalidated predictions. A later
	// rollback must not compare against a snapshot that this rollback has already replaced.
	InvalidateHistoryFrom(LocalFrame);
}

void URpgMoverRollbackTestObserver::TrackPredictedFrame(UMoverComponent* Mover, int32 LocalFrame, const FMoverSyncState& InjectedSync)
{
	StopTrackingFrame();
	if (!IsValid(Mover) || LocalFrame < 0) return;
	TrackedMover = Mover;
	TrackedLocalFrame = LocalFrame;
	FrameHistory.SetNum(MaxTrackedFrames);
	RecordFrame(LocalFrame, InjectedSync);
	Mover->OnPostMovement.AddDynamic(this, &URpgMoverRollbackTestObserver::ObservePostMovement);
}

void URpgMoverRollbackTestObserver::BeginDispatch(int32 LocalHead)
{
	ResetFrameEvidence();
	bDispatchActive = TrackedMover.IsValid() && TrackedLocalFrame != INDEX_NONE && LocalHead >= TrackedLocalFrame;
	DispatchLocalHead = bDispatchActive ? LocalHead : INDEX_NONE;
}

void URpgMoverRollbackTestObserver::EndDispatch()
{
	bDispatchActive = false;
}

void URpgMoverRollbackTestObserver::StopTrackingFrame()
{
	if (TrackedMover.IsValid())
		TrackedMover->OnPostMovement.RemoveDynamic(this, &URpgMoverRollbackTestObserver::ObservePostMovement);
	TrackedMover.Reset();
	TrackedLocalFrame = INDEX_NONE;
	DispatchLocalHead = INDEX_NONE;
	bDispatchActive = false;
	FrameHistory.Reset();
	ResetFrameEvidence();
}

void URpgMoverRollbackTestObserver::ResetFrameEvidence()
{
	RestoredLocalFrame = INDEX_NONE;
	RestoredSync = FMoverSyncState();
	ReplacedLocalFrame = INDEX_NONE;
	PredictedSync = FMoverSyncState();
	ReplacementSync = FMoverSyncState();
	ReplaySteps = 0;
	bHasReplacement = false;
	NextReplayInputFrame = INDEX_NONE;
}

const FMoverSyncState* URpgMoverRollbackTestObserver::FindRecordedFrame(int32 LocalFrame) const
{
	const int32 Index = LocalFrame - TrackedLocalFrame;
	return FrameHistory.IsValidIndex(Index) && FrameHistory[Index].bValid ? &FrameHistory[Index].Sync : nullptr;
}

void URpgMoverRollbackTestObserver::RecordFrame(int32 LocalFrame, const FMoverSyncState& SyncState)
{
	const int32 Index = LocalFrame - TrackedLocalFrame;
	if (!FrameHistory.IsValidIndex(Index)) return;
	FrameHistory[Index].Sync = SyncState;
	FrameHistory[Index].bValid = true;
}

void URpgMoverRollbackTestObserver::InvalidateHistoryFrom(int32 LocalFrame)
{
	for (int32 Index = FMath::Max(0, LocalFrame - TrackedLocalFrame); Index < FrameHistory.Num(); ++Index)
		FrameHistory[Index] = FRecordedFrame();
}

void URpgMoverRollbackTestObserver::ObservePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& SyncState, FMoverAuxStateContext& /*AuxState*/)
{
	if (!TrackedMover.IsValid()) return;
	const UWorld* World = TrackedMover->GetWorld();
	const UNetworkPredictionWorldManager* Prediction = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
	if (!Prediction || Prediction->GetSettings().PreferredTickingPolicy != ENetworkPredictionTickingPolicy::Fixed) return;
	const FFixedTickState& Clock = Prediction->GetFixedTickState();
	if (Clock.FixedStepMS <= 0 || !FMath::IsNearlyEqual(TimeStep.StepMs, static_cast<float>(Clock.FixedStepMS)))
	{
		if (bDispatchActive && RestoredLocalFrame != INDEX_NONE)
		{
			InvalidateHistoryFrom(RestoredLocalFrame);
			ResetFrameEvidence();
		}
		return;
	}
	// Forward ticking advances PendingFrame before simulation; it already names this output.
	if (TimeStep.ServerFrame == Clock.PendingFrame + Clock.Offset)
	{
		if (bDispatchActive && RestoredLocalFrame != INDEX_NONE)
		{
			InvalidateHistoryFrom(RestoredLocalFrame);
			ResetFrameEvidence();
		}
		RecordFrame(Clock.PendingFrame, SyncState);
		return;
	}
	if (!bDispatchActive || RestoredLocalFrame == INDEX_NONE || NextReplayInputFrame == INDEX_NONE) return;
	const int32 InputLocalFrame = Clock.PendingFrame;
	// The NP liaison does not populate bIsResimulating. A contiguous replay below the saved head,
	// following a real restore in this dispatch, proves resimulation without that unset flag.
	if (InputLocalFrame != NextReplayInputFrame || InputLocalFrame >= DispatchLocalHead
		|| TimeStep.ServerFrame != InputLocalFrame + Clock.Offset + 1)
	{
		InvalidateHistoryFrom(RestoredLocalFrame);
		ResetFrameEvidence();
		return;
	}
	++ReplaySteps;
	++NextReplayInputFrame;
	if (InputLocalFrame + 1 == ReplacedLocalFrame)
	{
		// Unlike the cached component state in OnPostSimulationTick, this is the actual output.
		// The delegate permits mutation; this observer only copies it and leaves Sync/Aux untouched.
		ReplacementSync = SyncState;
		bHasReplacement = true;
	}
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
