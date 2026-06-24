#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "RpgInventoryPanelNavigationCoordinator.generated.h"

class APlayerController;
class URpgInventoryDragDropCoordinator;
class URpgInventoryManagerComponent;
class URpgInventoryTileView;

/** One focusable inventory panel registered for controller LB/RB navigation. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryPanelNavigationEntry
{
	GENERATED_BODY()

	/** Stable id used by screen widgets to identify this panel, for example PlayerInventory or Storage. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	FName PanelId;

	/** TileView that receives controller focus while this panel is active. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgInventoryTileView> TileView = nullptr;

	/** Inventory represented by the TileView; used for shortcut routing. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgInventoryManagerComponent> Inventory = nullptr;

	/** Last selected list item for this panel. Updated whenever focus leaves the panel. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Navigation")
	TObjectPtr<UObject> LastSelectedItem = nullptr;

	/** Last selected occupied entry id, used to restore focus after list item recycling. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Navigation")
	FGuid LastSelectedEntryId;

	/** Last selected visual slot index, used when the entry id disappeared or the selected slot was empty. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Navigation")
	int32 LastSelectedSlotIndex = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FRpgInventoryActivePanelChanged, FName, PanelId, int32, PanelIndex, URpgInventoryTileView*, TileView, URpgInventoryManagerComponent*, Inventory);

/**
 * UI-local controller navigation helper for inventory screens with multiple panels.
 *
 * It owns focus memory only. Gameplay actions still go through URpgInventoryDragDropCoordinator and
 * URpgInventoryUiActionComponent so server validation remains authoritative.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryPanelNavigationCoordinator : public UObject
{
	GENERATED_BODY()

public:
	/** Creates a UI-local navigation coordinator for one inventory screen. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation", meta = (WorldContext = "WorldContextObject"))
	static URpgInventoryPanelNavigationCoordinator* CreateInventoryPanelNavigationCoordinator(UObject* WorldContextObject, APlayerController* InPlayerController, URpgInventoryDragDropCoordinator* InDragDropCoordinator);

	/** Initializes focus routing for this screen. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void Initialize(APlayerController* InPlayerController, URpgInventoryDragDropCoordinator* InDragDropCoordinator);

	/** Removes all registered panels and clears active focus state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void ClearPanels();

	/** Registers one focusable inventory panel for LB/RB controller navigation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void RegisterInventoryPanel(FName PanelId, URpgInventoryTileView* TileView, URpgInventoryManagerComponent* Inventory);

	/** Called by registered TileViews when CommonUI selection moves into or within a panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifyPanelSelectionChanged(URpgInventoryTileView* TileView, UObject* SelectedItem);

	/** Activates a registered panel by id and focuses a sensible entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool ActivatePanelById(FName PanelId);

	/** Activates a registered panel by index and focuses a sensible entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool ActivatePanelByIndex(int32 PanelIndex);

	/** Moves controller focus to the next registered panel. Intended for RB. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool ActivateNextPanel();

	/** Moves controller focus to the previous registered panel. Intended for LB. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool ActivatePreviousPanel();

	/** Re-applies focus to the current active panel after a list refresh. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool RefreshActivePanelFocus();

	/** Active panel index, or INDEX_NONE when no panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	int32 GetActivePanelIndex() const { return ActivePanelIndex; }

	/** Active panel id, or None when no panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	FName GetActivePanelId() const;

	/** Active panel TileView, or null when no panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventoryTileView* GetActiveTileView() const;

	/** Active panel inventory, or null when no panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventoryManagerComponent* GetActiveInventory() const;

	/** Fired after active panel focus changes so Blueprints can update borders/headers. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Navigation")
	FRpgInventoryActivePanelChanged OnActivePanelChanged;

private:
	bool IsValidPanelIndex(int32 PanelIndex) const;
	void SaveActivePanelSelection();
	void UpdatePanelSelectionMemory(FRpgInventoryPanelNavigationEntry& Panel) const;
	bool RestorePanelSelection(FRpgInventoryPanelNavigationEntry& Panel) const;
	void ApplyActivePanelState();
	void UpdateShortcutRoutesForActivePanel(const FRpgInventoryPanelNavigationEntry& ActivePanel);
	int32 FindPanelIndexForTileView(const URpgInventoryTileView* TileView) const;
	void BroadcastActivePanelChanged(const FRpgInventoryPanelNavigationEntry& ActivePanel);

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TArray<FRpgInventoryPanelNavigationEntry> Panels;

	UPROPERTY(Transient)
	int32 ActivePanelIndex = INDEX_NONE;

	bool bSuppressPanelSelectionNotifications = false;
};
