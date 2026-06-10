#pragma once

#include "GameUIManagerSubsystem.h"

#include "RpgUIManagerSubsystem.generated.h"

/**
 * Concrete CommonGame UI manager for SurvivalRpg.
 *
 * CommonGame's base manager is abstract so each game can own its policy class in config.
 * Set DefaultUIPolicyClass on this subsystem to a BP derived from UGameUIPolicy.
 */
UCLASS(Config = Game)
class SURVIVALRPG_API URpgUIManagerSubsystem : public UGameUIManagerSubsystem
{
	GENERATED_BODY()
};
