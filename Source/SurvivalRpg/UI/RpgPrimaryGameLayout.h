#pragma once

#include "PrimaryGameLayout.h"

#include "RpgPrimaryGameLayout.generated.h"

class UCommonActivatableWidgetContainerBase;

/**
 * Project root layout that registers the standard Lyra/CommonGame UI layers.
 *
 * If no Blueprint widget tree exists, the native class creates four full-screen stacks.
 * Blueprint children can override the layout by binding widgets with these exact property names.
 */
UCLASS(Blueprintable, BlueprintType)
class SURVIVALRPG_API URpgPrimaryGameLayout : public UPrimaryGameLayout
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	/** Persistent gameplay/HUD layer. Bind to a CommonActivatableWidgetStack in the root layout widget. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> GameLayer = nullptr;

	/** In-game menu layer for inventory, storage, loot, crafting, and character panels. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> GameMenuLayer = nullptr;

	/** Full-screen menu layer above in-game menus. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> MenuLayer = nullptr;

	/** Blocking modal layer above all other registered UI layers. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> ModalLayer = nullptr;

private:
	void CreateNativeFallbackLayers();
};
