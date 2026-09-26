#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgEditorPlaytestTools.generated.h"

class APlayerController;

/** Editor-only input bridge for observing ordinary gameplay through the configured Unreal MCP tools. */
UCLASS()
class SURVIVALRPGEDITOR_API URpgEditorPlaytestTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Sends one digital key transition to a local controller in PIE. Call again to release the key
	 * after at least one game tick; pressing and releasing in one editor call can lose Enhanced Input.
	 * Uses the normal controller input path. Never activates abilities directly or changes assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Editor|Playtest")
	static bool SetPIEInputKey(APlayerController* Controller, FName KeyName, bool bPressed);
};
