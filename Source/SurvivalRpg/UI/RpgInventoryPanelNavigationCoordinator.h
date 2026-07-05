#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "RpgInventoryPanelNavigationCoordinator.generated.h"

class APlayerController;
class URpgActionBarTileView;
class URpgInventoryDragDropCoordinator;
class URpgInventoryManagerComponent;
class URpgInventoryAddressTileView;
class URpgInventoryTileView;
class UWidget;

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

	/** Address TileView that receives controller focus while this player-inventory layout panel is active. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgInventoryAddressTileView> AddressTileView = nullptr;

	/** Actionbar TileView that receives controller focus while this panel is active. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Navigation")
	TObjectPtr<URpgActionBarTileView> ActionBarTileView = nullptr;

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

	/** Registers one focusable player-inventory address panel for LB/RB controller navigation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void RegisterInventoryAddressPanel(FName PanelId, URpgInventoryAddressTileView* TileView, URpgInventoryManagerComponent* Inventory);

	/** Registers one focusable actionbar panel for LB/RB controller navigation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void RegisterActionBarPanel(FName PanelId, URpgActionBarTileView* TileView);

	/** Called by registered TileViews when CommonUI selection moves into or within a panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifyPanelSelectionChanged(URpgInventoryTileView* TileView, UObject* SelectedItem);

	/** Called by registered address TileViews when CommonUI selection moves into or within a player-inventory layout panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifyAddressPanelSelectionChanged(URpgInventoryAddressTileView* TileView, UObject* SelectedItem);

	/** Called by registered actionbar TileViews when CommonUI selection moves into or within the actionbar panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void NotifyActionBarPanelSelectionChanged(URpgActionBarTileView* TileView, UObject* SelectedItem);

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

	/** Active player-inventory address TileView, or null when no address panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventoryAddressTileView* GetActiveAddressTileView() const;

	/** Active actionbar TileView, or null when no actionbar panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgActionBarTileView* GetActiveActionBarTileView() const;

	/** Active focus target widget for CommonUI, regardless of panel implementation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	UWidget* GetActiveFocusTarget() const;

	/** Active panel inventory, or null when no panel is active. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventoryManagerComponent* GetActiveInventory() const;

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
	int32 FindPanelIndexForAddressTileView(const URpgInventoryAddressTileView* TileView) const;
	int32 FindPanelIndexForActionBarTileView(const URpgActionBarTileView* TileView) const;
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
