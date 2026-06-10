#pragma once

#include "Engine/DeveloperSettings.h"
#include "RpgUIScreenRegistry.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgUISettings.generated.h"

class URpgUIScreenRegistry;

/**
 * Project UI configuration that points runtime UI systems at designer-authored assets.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "RPG UI"))
class SURVIVALRPG_API URpgUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URpgUISettings();

	/** Registry used by URpgUIScreenSubsystem to resolve UI.Screen tags to widget classes and layers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Screens")
	TSoftObjectPtr<URpgUIScreenRegistry> ScreenRegistry;

	/** Config fallback mappings used when no ScreenRegistry asset is assigned or the asset lacks a requested screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Screens")
	TArray<FRpgUIScreenRegistryEntry> DefaultScreenMappings;
};
