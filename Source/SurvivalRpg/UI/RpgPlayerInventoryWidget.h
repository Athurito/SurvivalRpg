#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/UI/RpgInventoryControllerActionsWidget.h"

#include "RpgPlayerInventoryWidget.generated.h"

class URpgActionBarTileView;
class URpgEquipmentSlotWidget;
class URpgInventoryDragDropCoordinator;
class URpgInventoryDragVisualWidget;
class URpgInventoryFeedbackToastWidget;
class URpgInventoryItemInstance;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySpatialGridWidget;
class URpgInventorySlotGroupWidget;
class URpgInventorySlotGroupPanelWidget;
class URpgInventorySlotGroupViewModel;
class URpgPlayerInventoryViewModel;
class UDragDropOperation;
class UWidget;

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
	virtual void NativeDestruct() override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Lets dual-inventory screens add their spatial panels inside the same focus-preserving navigator refresh. */
	virtual void RegisterAdditionalInventoryNavigationPanels(URpgInventoryPanelNavigationCoordinator* Navigator);

	/** Appends spatial grids owned by a derived screen so mouse routing and preview clearing use the shared session. */
	virtual void AppendAdditionalSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;

	/** Blueprint hook after the native parent creates and assigns PlayerInventoryViewModel. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Player", meta = (DisplayName = "On Player Inventory ViewModel Ready"))
	void BP_OnPlayerInventoryViewModelReady(URpgPlayerInventoryViewModel* ViewModel);

	/**
	 * Presentation hook for Move/Merge/Swap/Equip/Blocked/OOB/Pending/Rejected indicators.
	 * Read the coordinator's InteractionSession for the current payload, target, rotation, and request id.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Interaction", meta = (DisplayName = "On Inventory Interaction State Changed"))
	void BP_OnInventoryInteractionStateChanged(ERpgInventoryInteractionPreviewState PreviewState, bool bHasPayload, bool bPendingRequest);

	/** Optional styled owner-local result toast. A fully functional native widget is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Feedback")
	TSubclassOf<URpgInventoryFeedbackToastWidget> FeedbackToastWidgetClass;

	/**
	 * Optional free pointer-ghost style. The screen-owned instance bypasses UMG's drag-start interpolation while the
	 * underlying invisible drag operation continues to carry events; the native canonical visual is used when unset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Interaction")
	TSubclassOf<URpgInventoryDragVisualWidget> FreeDragVisualWidgetClass;

	/** Optional spatial group panel for the two ready-weapon roles and the offhand/shield role. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupPanelWidget> CarryGroupsList = nullptr;

	/** Optional spatial group panel for normal groups such as Pockets, Backpack, Belt, Pouch, and ResourceBag. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupPanelWidget> InventoryGroupsList = nullptr;

	/** Optional freely placed Pockets host. When present, Pockets are removed from InventoryGroupsList. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Pockets = nullptr;

	/** Optional freely placed first ready-weapon host. When present, WeaponSlot1 is removed from CarryGroupsList. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Carry_Weapon1 = nullptr;

	/** Optional freely placed second ready-weapon host. When present, WeaponSlot2 is removed from CarryGroupsList. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Carry_Weapon2 = nullptr;

	/** Optional freely placed offhand/shield host. When present, ShieldSlot is removed from CarryGroupsList. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Carry_Offhand = nullptr;

	/** Optional freely placed content host for the currently equipped backpack. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Backpack = nullptr;

	/** Optional freely placed content host for the currently equipped belt. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Belt = nullptr;

	/** Optional freely placed content host for the currently equipped pouch. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Pouch = nullptr;

	/** Optional freely placed content host for the currently equipped resource bag. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_ResourceBag = nullptr;

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

	UFUNCTION()
	void HandleInventoryInteractionStateChanged(ERpgInventoryInteractionPreviewState PreviewState, bool bHasPayload, bool bPendingRequest);
	void HandleInventoryActionFeedback(FGameplayTag Channel, const FRpgInventoryActionFeedbackMessage& Message);

	void EnsurePlayerInventoryViewModel();
	void BindViewModelDelegates();
	void SetGearSlotViewModel(URpgEquipmentSlotWidget* GearSlotWidget, ERpgEquipmentSlot EquipmentSlot, bool bBagSlot) const;
	void ForwardCoordinatorToChildren();
	void RegisterPlayerInventoryNavigationPanels();
	void QueueDeferredPlayerInventoryRefresh();
	void ExecuteDeferredPlayerInventoryRefresh();
	bool RouteInventoryPayloadAtScreenPosition(
		const FRpgInventoryDragPayload& Payload,
		FVector2D ScreenPosition,
		bool bCommit,
		const URpgInventoryDragDropOperation* DragOperation);
	bool RoutePayloadToGearSlot(const FRpgInventoryDragPayload& Payload, FVector2D GhostCenterScreenPosition, bool bCommit);
	bool RoutePayloadToActionBar(const FRpgInventoryDragPayload& Payload, FVector2D GhostCenterScreenPosition, bool bCommit);
	bool RoutePayloadToSpatialGrid(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit);
	void SwitchActivePointerDropTarget(UWidget* NewTarget);
	void CollectSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;
	void ClearExternalDragPreviews();
	URpgInventorySlotGroupViewModel* FindEquipmentProvidedContentGroup(FName SourceEquipmentSlotName) const;
	void CollectStandaloneGroupWidgets(TArray<URpgInventorySlotGroupWidget*>& OutWidgets) const;
	void RegisterInventoryFeedbackListener();
	void UnregisterInventoryFeedbackListener();
	URpgInventoryFeedbackToastWidget* EnsureInventoryFeedbackToast();
	void UpdateFreePointerDragVisual(
		const FRpgInventoryDragPayload& Payload,
		FVector2D PointerScreenPosition,
		URpgInventoryDragDropOperation* DragOperation);
	void ClearFreePointerDragVisual();

	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryViewModel> PlayerInventoryViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> PlayerDragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PlayerPanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryFeedbackToastWidget> InventoryFeedbackToast = nullptr;

	/** Screen-owned free ghost; target-local spatial ghosts remain owned by their grids. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragVisualWidget> FreePointerDragVisual = nullptr;

	/** Active invisible UMG operation retained so rotation can refresh its canonical payload without pointer motion. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropOperation> ActivePointerDragOperation = nullptr;

	FGameplayMessageListenerHandle InventoryActionFeedbackHandle;

	bool bViewModelDelegatesBound = false;
	bool bDeferredPlayerInventoryRefreshQueued = false;
	bool bHasLastPointerDragScreenPosition = false;
	bool bRoutingPointerPreview = false;
	FVector2D LastPointerDragScreenPosition = FVector2D::ZeroVector;
	FVector2D FreePointerDragVisualSize = FVector2D::ZeroVector;
	float FreePointerDragCellSize = 70.0f;
	float FreePointerDragCellPadding = 2.0f;
	TWeakObjectPtr<URpgInventoryItemInstance> FreePointerDragConfiguredItem;
	FGuid FreePointerDragConfiguredEntryId;
	FRpgInventoryGridSize FreePointerDragConfiguredFootprint;
	int32 FreePointerDragConfiguredStackCount = INDEX_NONE;
	ERpgInventoryDragSourceType FreePointerDragConfiguredSourceType = ERpgInventoryDragSourceType::None;
	bool bFreePointerDragVisualConfigured = false;
	TWeakObjectPtr<UWidget> ActivePointerDropTarget;
};
