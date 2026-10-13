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

	/**
	 * Sends one raw value for an analog key, such as Gamepad_LeftY, to a local controller in PIE. The mapping's
	 * modifiers (dead zone, swizzle) still apply. Resend the value every tick while the stick is held and send 0
	 * to release it. Uses the normal controller input path; never changes movement state or assets directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Editor|Playtest")
	static bool SetPIEInputAxis(APlayerController* Controller, FName KeyName, float Value);
};
