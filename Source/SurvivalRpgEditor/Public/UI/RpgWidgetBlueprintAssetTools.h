#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "RpgWidgetBlueprintAssetTools.generated.h"

class UWidgetBlueprint;
class UUserWidget;

/**
 * Editor-only helpers for narrowly scoped, script-driven Widget Blueprint maintenance.
 *
 * These functions keep UMG's source-widget variable GUID bookkeeping in sync; callers
 * should still compile and save the Widget Blueprint after a successful mutation.
 */
UCLASS()
class SURVIVALRPGEDITOR_API URpgWidgetBlueprintAssetTools final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Removes one non-root source widget by stable name and marks the Blueprint structurally modified.
	 *
	 * The operation is editor-only and updates the Widget Blueprint's variable GUID map before the
	 * detached widget is moved to the transient package. Returns false for missing assets, missing
	 * widgets, root widgets, or failed WidgetTree removal; no package is saved by this function.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|UI|Editor")
	static bool RemoveSourceWidget(
		UWidgetBlueprint* WidgetBlueprint,
		FName WidgetName);

	/**
	 * Creates one initialized transient instance of the supplied Widget Blueprint in the editor world.
	 *
	 * This is a validation seam for commandlet automation: it executes UUserWidget's real initialization
	 * path so BindWidget resolution and the duplicated runtime WidgetTree can be inspected without opening UI.
	 * The returned object is transient runtime state and is never saved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|UI|Editor")
	static UUserWidget* CreateInitializedWidgetBlueprintInstance(
		UWidgetBlueprint* WidgetBlueprint);
};
