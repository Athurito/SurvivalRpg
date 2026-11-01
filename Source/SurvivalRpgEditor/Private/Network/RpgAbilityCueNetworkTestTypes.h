#pragma once

#include "GameplayEffect.h"

#include "RpgAbilityCueNetworkTestTypes.generated.h"

/**
 * Infinite effect for network cue tests. It survives death and respawn, because it carries no lifecycle tag. Tests set
 * its GameplayCues on the class default before applying it and clear them afterwards, since configured tags are not
 * loaded when an editor-module class default is built. Being native, the effect replicates by its class default.
 */
UCLASS(NotBlueprintable)
class URpgNetworkAutomationLoopingCueEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URpgNetworkAutomationLoopingCueEffect();
};
