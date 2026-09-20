// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "UObject/Object.h"

#include "RpgMoverPredictionTestTypes.generated.h"

class UAnimMontage;

/** Test-only receiver proving that an observed state change included the engine's actual rollback path. */
UCLASS(NotBlueprintable, Transient)
class URpgMoverRollbackTestObserver final : public UObject
{
	GENERATED_BODY()

public:
	/** Records the restored and invalidated simulation heads without changing either simulation or presentation. */
	UFUNCTION()
	void ObserveRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep);

	int32 Count = 0;
	FMoverTimeStep LastRestored;
	FMoverTimeStep LastExpunged;
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
