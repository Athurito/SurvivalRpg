#pragma once

#include "PrimaryGameLayout.h"

#include "RpgPrimaryGameLayout.generated.h"

class UCommonActivatableWidgetContainerBase;

/**
 * Project root layout that registers the standard Lyra/CommonGame UI layers.
 *
 * The configured Blueprint owns the visible static hierarchy. Its four named CommonUI stacks are required and are
 * registered here so screen lifecycle stays native and centralized.
 */
UCLASS(Blueprintable, BlueprintType)
class SURVIVALRPG_API URpgPrimaryGameLayout : public UPrimaryGameLayout
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	/** Persistent gameplay/HUD layer authored as a CommonActivatableWidgetStack in the root layout asset. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> GameLayer = nullptr;

	/** In-game menu layer for inventory, storage, loot, crafting, and character panels. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> GameMenuLayer = nullptr;

	/** Full-screen menu layer above in-game menus. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> MenuLayer = nullptr;

	/** Blocking modal layer above all other registered UI layers. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> ModalLayer = nullptr;
};
