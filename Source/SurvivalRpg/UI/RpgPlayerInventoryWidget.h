#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/UI/RpgInventoryControllerActionsWidget.h"

#include "RpgPlayerInventoryWidget.generated.h"

class URpgActionBarTileView;
class URpgEquipmentSlotWidget;
class URpgInventoryDragDropCoordinator;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySpatialGridWidget;
class URpgInventorySlotGroupPanelWidget;
class URpgPlayerInventoryViewModel;
class UDragDropOperation;

/**
 * Native base for the player inventory screen.
 *
 * It wires the player-inventory MVVM projection into named Blueprint widgets so the screen does not need manual
 * slot loops or per-entry coordinator setup. Gameplay state stays in inventory, layout, equipment, and actionbar
 * components; this widget only connects view models to CommonUI views.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgPlayerInventoryWidget : public URpgInventoryControllerActionsWidget
{
	GENERATED_BODY()

public:
	explicit URpgPlayerInventoryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Ensures the screen-local drag/drop coordinator exists and forwards it to all bound child widgets. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player")
	void EnsurePlayerInventoryCoordinator();

	/** Ensures the screen-local panel navigator exists and is shared with controller action routing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player")
	void EnsurePlayerInventoryPanelNavigator();

	/** Rebinds the aggregate player inventory VM to the owning player controller and refreshes all child views. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player")
	void BindPlayerInventoryViewModel();

	/** Pushes the latest slot-group VMs into CarryGroupsList and InventoryGroupsList. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player")
	void RefreshSlotGroups();

	/** Pushes the latest 1..8 actionbar slot VMs into ActionBarTileView. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player")
	void RefreshActionBar();

	/** Pushes the latest fixed armor and bag slot VMs into the optional gear widgets. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player")
	void RefreshGearSlots();

	/** Refreshes gear slots, slot groups, and actionbar preview from the aggregate VM. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player")
	void RefreshPlayerInventoryViews();

	/** Aggregate MVVM projection used by this screen. Created by the native base when missing. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player")
	URpgPlayerInventoryViewModel* GetPlayerInventoryViewModel() const { return PlayerInventoryViewModel; }

	/** Returns a compact runtime summary for debugging Blueprint widget binding and VM list counts. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player|Debug")
	FString GetPlayerInventoryWidgetDebugSummary() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Blueprint hook after the native parent creates and assigns PlayerInventoryViewModel. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Player", meta = (DisplayName = "On Player Inventory ViewModel Ready"))
	void BP_OnPlayerInventoryViewModelReady(URpgPlayerInventoryViewModel* ViewModel);

	/** Optional spatial group panel for carry groups such as WeaponSlot1, ShieldSlot, and ToolSlot1. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupPanelWidget> CarryGroupsList = nullptr;

	/** Optional spatial group panel for normal groups such as Pockets, Backpack, Belt, Pouch, and ResourceBag. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupPanelWidget> InventoryGroupsList = nullptr;

	/** Optional 1..8 actionbar preview/drop target inside the inventory screen. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgActionBarTileView> ActionBarTileView = nullptr;

	/** Optional fixed armor slot widgets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Head = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Chest = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Hands = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Legs = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Feet = nullptr;

	/** Optional bag/provider equipment slot widgets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Backpack = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Belt = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Pouch = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_ResourceBag = nullptr;

private:
	UFUNCTION()
	void HandleGearSlotsChanged();

	UFUNCTION()
	void HandleSlotGroupsChanged();

	UFUNCTION()
	void HandleActionBarSlotsChanged();

	void EnsurePlayerInventoryViewModel();
	void BindViewModelDelegates();
	void SetGearSlotViewModel(URpgEquipmentSlotWidget* GearSlotWidget, ERpgEquipmentSlot EquipmentSlot, bool bBagSlot) const;
	void ForwardCoordinatorToChildren();
	void RegisterPlayerInventoryNavigationPanels();
	void QueueDeferredPlayerInventoryRefresh();
	void ExecuteDeferredPlayerInventoryRefresh();
	bool RouteInventoryPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit);
	bool RoutePayloadToGearSlot(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit);
	bool RoutePayloadToActionBar(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit);
	bool RoutePayloadToSpatialGrid(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit);
	void CollectSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;
	void ClearExternalDragPreviews();

	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryViewModel> PlayerInventoryViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> PlayerDragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PlayerPanelNavigationCoordinator = nullptr;

	bool bViewModelDelegatesBound = false;
	bool bDeferredPlayerInventoryRefreshQueued = false;
};
