#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "RpgInventoryPanelNavigationCoordinator.generated.h"

class APlayerController;
class URpgActionBarTileView;
class URpgEquipmentSlotWidget;
class URpgInventoryCarrySlotWidget;
class URpgInventoryDragDropCoordinator;
class URpgInventoryManagerComponent;
class URpgInventorySpatialGridWidget;
class UWidget;

/** One focusable inventory panel registered for controller LB/RB navigation. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryPanelNavigationEntry
{
	GENERATED_BODY()

	/** Stable id used by screen widgets to identify this panel, for example PlayerInventory or Storage. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	FName PanelId;

	/** Spatial grid that receives controller focus while this fixed-layout inventory panel is active. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgInventorySpatialGridWidget> SpatialGridWidget = nullptr;

	/** Actionbar TileView that receives controller focus while this panel is active. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgActionBarTileView> ActionBarTileView = nullptr;

	/** Equipment slot widget that receives controller focus while this gear panel is active. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgEquipmentSlotWidget> EquipmentSlotWidget = nullptr;

	/** Gear-like single-address carry slot that receives focus without exposing a fake 1x1 spatial grid. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgInventoryCarrySlotWidget> CarrySlotWidget = nullptr;

	/** Inventory represented by this panel; used for shortcut routing. */
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRpgInventoryActivePanelChanged,
	FName,
	PanelId,
	int32,
	PanelIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgInventoryActiveSelectionChanged);

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

	/** Removes all registered panels, clears active focus state, and publishes an empty selection outside refresh transactions. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void ClearPanels();

	/** Begins an in-place panel registration pass while retaining active panel and per-panel selection memory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void BeginPanelRefresh();

	/**
	 * Finishes a registration pass, restoring the previous panel or using PreferredInitialPanelId before index zero.
	 * The preference applies only when no previously active panel survives the refresh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void EndPanelRefresh(FName PreferredInitialPanelId = NAME_None);

	/** Registers one focusable spatial inventory grid for LB/RB controller navigation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void RegisterSpatialInventoryPanel(FName PanelId, URpgInventorySpatialGridWidget* SpatialGridWidget, URpgInventoryManagerComponent* Inventory);

	/** Registers one focusable actionbar panel for LB/RB controller navigation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void RegisterActionBarPanel(FName PanelId, URpgActionBarTileView* TileView);

	/** Registers one focusable gear/equipment slot panel for LB/RB controller navigation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void RegisterEquipmentPanel(FName PanelId, URpgEquipmentSlotWidget* EquipmentSlotWidget);

	/** Registers one gear-like Weapon1, Weapon2, or Offhand carry address as a single controller panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void RegisterCarrySlotPanel(FName PanelId, URpgInventoryCarrySlotWidget* CarrySlotWidget);

	/** Called by registered spatial grids when their logical cursor moves into or within a panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifySpatialPanelSelectionChanged(URpgInventorySpatialGridWidget* SpatialGridWidget, UObject* SelectedItem);

	/** Called by registered actionbar TileViews when CommonUI selection moves into or within the actionbar panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifyActionBarPanelSelectionChanged(URpgActionBarTileView* TileView, UObject* SelectedItem);

	/** Called when a registered single-address carry slot receives CommonUI or pointer focus. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifyCarrySlotFocused(URpgInventoryCarrySlotWidget* CarrySlotWidget);

	/** Called when a registered equipment slot receives CommonUI or pointer focus. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifyEquipmentSlotFocused(URpgEquipmentSlotWidget* EquipmentSlotWidget);

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

	/** Active spatial grid, or null when no spatial panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventorySpatialGridWidget* GetActiveSpatialGridWidget() const;

	/** Active actionbar TileView, or null when no actionbar panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgActionBarTileView* GetActiveActionBarTileView() const;

	/** Active equipment slot widget, or null when no gear panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgEquipmentSlotWidget* GetActiveEquipmentSlotWidget() const;

	/** Active gear-like carry slot, or null when another panel type is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventoryCarrySlotWidget* GetActiveCarrySlotWidget() const;

	/** Active focus target widget for CommonUI, regardless of panel implementation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	UWidget* GetActiveFocusTarget() const;

	/** Active panel inventory, or null when no panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventoryManagerComponent* GetActiveInventory() const;

	/** Whether the current selection can expose the full-stack transfer shortcut in the action bar. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation|Availability")
	bool CanQuickTransferActiveSelection() const;

	/** Whether the current selection can split, or the held payload can use the shared rotate shortcut. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation|Availability")
	bool CanQuickSplitActiveSelection() const;

	/** Whether the current selection has a usable, equippable, carry, or unequip action. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation|Availability")
	bool CanUseOrEquipActiveSelection() const;

	/** Whether the current selection represents an item that may submit a drop request. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation|Availability")
	bool CanDropActiveSelection() const;

	/** Runs quick transfer on the active panel selection when supported. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool QuickTransferActiveSelection();

	/** Runs quick split on the active panel selection when supported. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool QuickSplitActiveSelection(int32 SplitCount = 0);

	/** Runs use/equip/unequip on the active panel selection when supported. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool UseOrEquipActiveSelection(int32 StackCount = 1);

	/** Runs manual drop on the active panel selection when supported. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool DropActiveSelection(int32 StackCount = 0, bool bConfirmed = false);

	/**
	 * Opens the context menu for the active spatial, gear, or carry selection.
	 *
	 * The source leaf resolves its absolute Slate anchor. Player-screen center is used only when that geometry is
	 * not yet usable, so callers cannot accidentally pass viewport-local or sentinel coordinates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool RequestContextMenuForActiveSelection();

	/** Fired after active panel focus changes so Blueprints can update borders/headers. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Navigation")
	FRpgInventoryActivePanelChanged OnActivePanelChanged;

	/** Fired for selection changes inside the active panel so CommonUI hints can update without rebuilding bindings. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Navigation")
	FRpgInventoryActiveSelectionChanged OnActiveSelectionChanged;

private:
	bool IsValidPanelIndex(int32 PanelIndex) const;
	void SaveActivePanelSelection();
	void UpdatePanelSelectionMemory(FRpgInventoryPanelNavigationEntry& Panel) const;
	bool RestorePanelSelection(FRpgInventoryPanelNavigationEntry& Panel) const;
	void ApplyActivePanelState();
	void UpdateFocusedInventoryForActivePanel(const FRpgInventoryPanelNavigationEntry& ActivePanel);
	int32 FindPanelIndexForSpatialGridWidget(const URpgInventorySpatialGridWidget* SpatialGridWidget) const;
	int32 FindPanelIndexForActionBarTileView(const URpgActionBarTileView* TileView) const;
	int32 FindPanelIndexForEquipmentSlotWidget(const URpgEquipmentSlotWidget* EquipmentSlotWidget) const;
	int32 FindPanelIndexForCarrySlotWidget(const URpgInventoryCarrySlotWidget* CarrySlotWidget) const;
	void BroadcastActivePanelChanged(const FRpgInventoryPanelNavigationEntry& ActivePanel);
	void ApplyRetainedPanelMemory(FRpgInventoryPanelNavigationEntry& Panel) const;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TArray<FRpgInventoryPanelNavigationEntry> Panels;

	UPROPERTY(Transient)
	int32 ActivePanelIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TMap<FName, FRpgInventoryPanelNavigationEntry> RetainedPanelMemories;

	UPROPERTY(Transient)
	FName RetainedActivePanelId = NAME_None;

	bool bSuppressPanelSelectionNotifications = false;
	bool bPanelRefreshInProgress = false;
};
