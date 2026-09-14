// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/RpgMoverPredictionTestTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgMoverPredictionTestTypes)

void URpgMoverRollbackTestObserver::ObserveRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep)
{
	++Count;
	LastRestored = CurrentTimeStep;
	LastExpunged = ExpungedTimeStep;
}
