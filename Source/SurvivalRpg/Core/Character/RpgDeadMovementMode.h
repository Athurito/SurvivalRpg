#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "RpgDeadMovementMode.generated.h"

/**
 * Terminal stationary state for the game-thread Mover death lifecycle. Mover replicates and reconciles
 * this mode with the capsule state; health owns the decision to die and the AnimBP owns presentation.
 * A replacement pawn receives normal authored movement modes when the player respawns.
 */
UCLASS(NotBlueprintable)
class SURVIVALRPG_API URpgDeadMovementMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	/** Stable identifier carried by FMoverSyncState; this mode has no outgoing movement transitions. */
	static const FName ModeName;

	/** Ignores movement contributions and publishes a stationary world-space state without a movement base. */
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
};
