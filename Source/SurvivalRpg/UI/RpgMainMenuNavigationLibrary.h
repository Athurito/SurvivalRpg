#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "RpgMainMenuNavigationLibrary.generated.h"

class UCommonActivatableWidget;
class URpgMainMenuStackWidget;

/**
 * Local-player-safe compatibility API for the existing Main Menu assets.
 *
 * BPFL_GUI_Library inherits these functions so existing callers keep their
 * signatures while navigation resolves the registered Main Menu screen
 * instead of casting a HUD or assuming player index zero.
 */
UCLASS()
class SURVIVALRPG_API URpgMainMenuNavigationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu", meta = (WorldContext = "WorldContextObject"))
	static void MainMenu_PushToMainStack(
		const UObject* WorldContextObject,
		TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu", meta = (WorldContext = "WorldContextObject"))
	static void MainMenu_PushToOptionStack(
		const UObject* WorldContextObject,
		TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu", meta = (WorldContext = "WorldContextObject"))
	static void MainMenu_PushToPopupStack(
		const UObject* WorldContextObject,
		TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu", meta = (WorldContext = "WorldContextObject"))
	static UCommonActivatableWidget* MainMenu_PushToOption1Stack(
		const UObject* WorldContextObject,
		TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu", meta = (WorldContext = "WorldContextObject"))
	static UCommonActivatableWidget* MainMenu_PushToOption2Stack(
		const UObject* WorldContextObject,
		TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

private:
	static URpgMainMenuStackWidget* ResolveMainMenu(const UObject* WorldContextObject);
};
