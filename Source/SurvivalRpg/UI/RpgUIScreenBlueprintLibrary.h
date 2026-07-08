#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"

#include "RpgUIScreenBlueprintLibrary.generated.h"

class APlayerController;
class UCommonActivatableWidget;
class URpgUIScreenSubsystem;

/**
 * Blueprint helpers for routing UI through URpgUIScreenSubsystem.
 */
UCLASS()
class SURVIVALRPG_API URpgUIScreenBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the local player's screen subsystem for the supplied player controller. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Screens")
	static URpgUIScreenSubsystem* GetRpgUIScreenSubsystem(APlayerController* PlayerController);

	/** Opens a registered UI.Screen for the supplied local player controller. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	static UCommonActivatableWidget* OpenUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag, UObject* Payload);

	/** Toggles a registered UI.Screen for the supplied local player controller. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	static UCommonActivatableWidget* ToggleUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag, UObject* Payload);

	/** Closes a registered UI.Screen for the supplied local player controller. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	static void CloseUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag);

	/** Returns the active widget currently tracked for ScreenTag. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	static UCommonActivatableWidget* GetActiveUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag);
};
