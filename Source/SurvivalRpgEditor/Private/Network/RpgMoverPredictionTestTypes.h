// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverTypes.h"
#include "UObject/Object.h"

#include "RpgMoverPredictionTestTypes.generated.h"

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
