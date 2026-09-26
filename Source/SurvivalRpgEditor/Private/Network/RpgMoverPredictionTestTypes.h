// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverTypes.h"
#include "MoverSimulationTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "UObject/Object.h"

#include "RpgMoverPredictionTestTypes.generated.h"

class UAnimMontage;
class UMoverComponent;

/** Test-only receiver proving that an observed state change included the engine's actual rollback path. */
UCLASS(NotBlueprintable, Transient)
class URpgMoverRollbackTestObserver final : public UObject
{
	GENERATED_BODY()

public:
	/** Records the restored and invalidated simulation heads without changing either simulation or presentation. */
	UFUNCTION()
	void ObserveRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep);

	/** Tracks the injected Fixed frame and up to 63 subsequent real outputs; the caller retains its rollback binding. */
	void TrackPredictedFrame(UMoverComponent* Mover, int32 LocalFrame, const FMoverSyncState& InjectedSync);
	/** Starts a fresh network-dispatch bracket at the world's local prediction head. */
	void BeginDispatch(int32 LocalHead);
	/** Closes the bracket while retaining its evidence for assertions. */
	void EndDispatch();
	/** Unbinds the optional movement observer and releases copied history before PIE teardown. */
	void StopTrackingFrame();

	int32 Count = 0;
	FMoverTimeStep LastRestored;
	FMoverTimeStep LastExpunged;

	/** Local input frame restored by the latest valid rollback in this dispatch, never the liaison's cached server frame. */
	int32 RestoredLocalFrame = INDEX_NONE;
	FMoverSyncState RestoredSync;
	/** Both snapshots belong to this exact local history frame from the current rollback epoch. */
	int32 ReplacedLocalFrame = INDEX_NONE;
	FMoverSyncState PredictedSync;
	/** State replacing the recorded prediction, captured either at restore or as the exact replay output. */
	FMoverSyncState ReplacementSync;
	int32 ReplaySteps = 0;
	bool bHasReplacement = false;

private:
	UFUNCTION()
	void ObservePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& SyncState, FMoverAuxStateContext& AuxState);
	void ResetFrameEvidence();
	const FMoverSyncState* FindRecordedFrame(int32 LocalFrame) const;
	void RecordFrame(int32 LocalFrame, const FMoverSyncState& SyncState);
	void InvalidateHistoryFrom(int32 LocalFrame);

	struct FRecordedFrame
	{
		FMoverSyncState Sync;
		bool bValid = false;
	};
	static constexpr int32 MaxTrackedFrames = 64;
	TArray<FRecordedFrame> FrameHistory;

	TWeakObjectPtr<UMoverComponent> TrackedMover;
	int32 TrackedLocalFrame = INDEX_NONE;
	int32 DispatchLocalHead = INDEX_NONE;
	int32 NextReplayInputFrame = INDEX_NONE;
	bool bDispatchActive = false;
};

/** Test-only receiver for linear traversal branching events; does not alter their content or execution. */
UCLASS(NotBlueprintable, Transient)
class URpgMoverTraversalNotifyTestObserver final : public UObject
{
	GENERATED_BODY()
public:
	/** Observes one source window begin on a concrete montage instance. */
	UFUNCTION()
	void ObserveBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);
	/** Observes one source window end, including cancellation and authored blend-out. */
	UFUNCTION()
	void ObserveEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	void AddMontage(UAnimMontage* Montage);
	bool HasDuplicateCallbacks() const;
	int32 ObservedBegins() const;

private:
	struct FWindow
	{
		TWeakObjectPtr<UAnimSequenceBase> Montage;
		FName Name;
		int32 InstanceId = INDEX_NONE;
		float StartTime = 0.f, EndTime = 0.f;
		int32 Begins = 0, Ends = 0;
	};
	void Observe(FName NotifyName, const FBranchingPointNotifyPayload& Payload, bool bBegin);
	TSet<TWeakObjectPtr<UAnimSequenceBase>> Montages;
	TArray<FWindow> Windows;
};
