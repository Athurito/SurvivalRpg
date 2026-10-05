#pragma once

#include "GameFramework/GameModeBase.h"

#include "RpgSkillTreeNetworkTestTypes.generated.h"

/** Plain game mode whose players get the real RPG player state, so client requests run through their own connection. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationProgressionGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationProgressionGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
