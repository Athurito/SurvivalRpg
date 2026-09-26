#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "RpgMoverRagdollMovementMode.generated.h"

/** Holds the approved stationary capsule anchor while Blueprint simulates the living ragdoll mesh. */
UCLASS()
class SURVIVALRPG_API URpgMoverRagdollMovementMode : public UBaseMovementMode
{
	GENERATED_BODY()
public:
	static const FName ModeName;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
};
