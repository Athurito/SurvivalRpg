#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgInventoryContextActionSource.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViewTypes.h"

#include "RpgInventorySpatialGridWidget.generated.h"

class APlayerController;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryDragVisualWidget;
class URpgInventoryEntryViewModel;
class URpgInventoryInteractionScreenWidget;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventoryPanelViewModel;
class URpgInventorySlotGroupViewModel;
class URpgInventorySpatialCellWidget;
class URpgInventorySpatialItemWidget;
class UCanvasPanel;
class UDragDropOperation;
class USizeBox;
struct FPointerEvent;

/**
 * Native fixed-layout spatial inventory grid.
 *
 * The grid draws cells itself, owns a logical cursor, and positions one item widget per origin on a CanvasPanel.
 * It supports both player layout groups and storage inventory panels without using TileView/ListView for spatial data.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventorySpatialGridWidget
	: public UUserWidget
	, public IRpgInventoryContextActionSource
{
	GENERATED_BODY()

public:
	explicit URpgInventorySpatialGridWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Binds this grid to one player-inventory slot group such as Pockets, Backpack, Belt, or a carry slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void BindSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

	/** Binds this grid to one exact root or item-owned container in an inventory graph. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void BindInventoryContainerPanelViewModel(
		URpgInventoryPanelViewModel* InPanelViewModel,
		URpgInventoryManagerComponent* InInventory,
		FRpgInventoryContainerHandle InContainerHandle);

	/**
	 * Releases the observed view model, interaction context, transient dialogs, previews, filters, and selection.
	 *
	 * Call this when a pooled parent screen deactivates. The authored widget tree remains intact and may bind a new
	 * inventory later; no gameplay state is changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void ReleaseInventoryPresentation();

	/**
	 * Applies presentation-only dimming to the supplied replicated entry ids.
	 * Item overlays, hit targets, view models, and server-authored grid coordinates remain present and unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Filter")
	void SetDimmedEntryIds(const TArray<FGuid>& InDimmedEntryIds, float InDimmedOpacity = 0.25f);

	/** Restores full opacity for every item overlay without rebuilding inventory state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Filter")
	void ClearEntryDimming();

	/** Returns whether the presentation-only filter currently dims this replicated entry. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Filter")
	bool IsEntryDimmed(FGuid EntryId) const;

	/** Assigns the screen-local drag/drop coordinator shared by grid, item overlays, actionbar, and gear slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/**
	 * Assigns the owning inventory screen that centrally creates and owns context menus and split dialogs.
	 * The host is presentation-only and must never become an inventory gameplay authority.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	void SetInventoryPresentationHost(URpgInventoryInteractionScreenWidget* InPresentationHost);

	/** Registers this grid with the screen-local panel navigator so controller actions route to the logical cursor. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId);

	/** Marks this grid as the active controller panel. Inactive panels keep cursor memory but do not show focus. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void SetInventoryPanelActive(bool bInInventoryPanelActive);

	/** Sets fixed grid cell dimensions in Slate units. Defaults are tuned for 70x70 Tarkov-like slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void SetCellMetrics(float InCellSize, float InCellPadding);

	/** Selects current valid cursor, first occupied item, then first cell. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Navigation")
	bool SelectBestCell(APlayerController* OwningPlayer, bool bPreferOccupiedSlot = true);

	/** Selects by stable entry id first, then by visual slot index. Used after VM refreshes. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Navigation")
	bool SelectCellByIdentity(FGuid EntryId, int32 SlotIndex, APlayerController* OwningPlayer);

	/** Selects one cell and optionally gives controller focus to the grid. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Navigation")
	bool SelectCell(int32 X, int32 Y, APlayerController* OwningPlayer = nullptr);

	/** Clears visible focus state without changing cursor memory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Navigation")
	void ClearSelectionVisual();

	/** Accept helper: A/click picks an item up, or places the held payload on the selected cell. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool HandleAcceptSelectedCell();

	/** Shortcut helper for controller X on the item under the cursor. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool QuickTransferSelectedCell(URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr);

	/** Shortcut helper for controller Y. Rotates held payload, otherwise quick-splits the item under the cursor. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool QuickSplitSelectedCell(int32 SplitCount = 0);

	/** Opens the exact split UI for the selected stack (1..StackCount-1, default floor half). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool RequestSplitDialogForSelectedCell();

	/** Commits the exact value captured by the active split dialog after revalidating the same item and range. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool ConfirmPendingSplit(int32 SplitCount);

	/** Closes the current split request without changing gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	void CancelPendingSplit();

	/** Opens the context menu at an absolute Slate screen position for the current item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool RequestContextMenuForSelectedCell(FVector2D ScreenPosition);

	/**
	 * Resolves the selected item's footprint center, then the cursor-cell center, in absolute Slate coordinates.
	 * Returns false before this grid has a usable layout geometry.
	 */
	bool TryGetSelectedContextMenuScreenAnchor(FVector2D& OutAbsoluteScreenPosition) const;

	/** Returns context actions for the selected item. The server still validates every committed action. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Actions")
	TArray<ERpgInventoryContextAction> GetSelectedContextActions() const;

	/** Executes a native context action or forwards UI-only actions such as Inspect/Open/Binding to Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool ExecuteSelectedContextAction(ERpgInventoryContextAction Action, int32 SplitCount = 0, int32 QuickAccessSlotIndex = -1);

	/** Internal zero-based 0..7 Quick Access index matching the selected Carry role or consumable definition. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Actions")
	int32 GetSelectedQuickAccessSlotIndex() const;

	//~IRpgInventoryContextActionSource interface
	virtual bool QueryInventoryContextActions(
		FRpgInventoryContextActionSnapshot& OutSnapshot) const override;
	virtual bool ExecuteInventoryContextAction(
		const FRpgInventoryContextActionSnapshot& ExpectedSnapshot,
		ERpgInventoryContextAction Action,
		int32 QuickAccessSlotIndex = INDEX_NONE) override;
	//~End of IRpgInventoryContextActionSource interface

	/** Shortcut helper for use/equip on the item under the cursor. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool UseOrEquipSelectedCell(int32 StackCount = 1);

	/** Shortcut helper for dropping the item under the cursor into the world. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool DropSelectedCell(int32 StackCount = 0, bool bConfirmed = false);

	/** Toggles the target rotation used when the current held payload is dropped onto this grid. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid|Actions")
	bool ToggleHeldItemRotation();

	/** Player address currently under the logical cursor, normalized to item origin when occupied. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Navigation")
	URpgInventoryAddressSlotViewModel* GetSelectedAddressSlot() const;

	/** Storage/crafting entry currently under the logical cursor, normalized to item origin when occupied. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Navigation")
	URpgInventoryEntryViewModel* GetSelectedEntryViewModel() const;

	/** Last selected occupied entry id, or invalid when cursor is empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Navigation")
	FGuid GetSelectedEntryId() const;

	/** Persistent item id under the logical cursor, or invalid when the cursor is empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Navigation")
	FRpgInventoryItemId GetSelectedItemId() const;

	/** Visual slot index derived from cursor coordinates. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid|Navigation")
	int32 GetSelectedSlotIndex() const;

	/** Grid width in cells currently rendered by this widget. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid")
	int32 GetGridWidth() const { return GridSize.Width; }

	/** Grid height in cells currently rendered by this widget. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Grid")
	int32 GetGridHeight() const { return GridSize.Height; }

	/** Current square-cell size in Slate units, used by exact drag visuals. */
	float GetSpatialCellSize() const { return CellSize; }

	/** Current gap between spatial cells in Slate units, used by exact drag visuals. */
	float GetSpatialCellPadding() const { return CellPadding; }

	bool SelectCellFromScreenPosition(FVector2D ScreenPosition, APlayerController* OwningPlayer = nullptr);
	/** Selects the exact cell below a passive mouse hover without disturbing held, pending, or gamepad interaction. */
	bool SelectCellFromPointerHover(
		FVector2D ScreenPosition,
		const FPointerEvent& PointerEvent,
		APlayerController* OwningPlayer = nullptr);
	bool CommitPayloadToCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y);
	bool PreviewPayloadOnCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y);
	bool CommitPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition);
	bool PreviewPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition);
	/** True when the projected dragged-item center addresses this grid, even if the pointer itself is outside. */
	bool CanAddressPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition) const;
	bool ContainsScreenPosition(FVector2D ScreenPosition) const;
	bool ResolveDropTargetAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, FRpgInventoryDropTarget& OutTarget, FRpgInventoryGridPlacement& OutTargetPlacement, int32& OutAnchorX, int32& OutAnchorY) const;
	/** Resolves the one candidate consumed by target ghost, footprint cells, session, and final commit. */
	bool ResolveSpatialPreviewDescriptorAtScreenPosition(
		const FRpgInventoryDragPayload& Payload,
		FVector2D ScreenPosition,
		FRpgInventorySpatialPreviewDescriptor& OutDescriptor,
		FRpgInventoryInteractionPreviewPlan* OutPreviewPlan = nullptr) const;
	void ClearExternalPreviewPayload();
	bool IsItemWidgetFocused(const URpgInventorySpatialItemWidget* ItemWidget) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	/** Handles Inspect/Open Container/Quick-Access menu decisions that require screen-specific presentation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Grid|Context", meta = (DisplayName = "On Deferred Inventory Context Action"))
	void BP_OnDeferredInventoryContextAction(
		ERpgInventoryContextAction Action,
		URpgInventoryItemInstance* Item,
		int32 QuickAccessSlotIndex);

	/** Optional CanvasPanel in Blueprint that receives one designable cell widget per grid coordinate. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> CellCanvas = nullptr;

	/** Alternative Blueprint binding name for the designable cell layer. Used only when CellCanvas is unset. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> CellLayer = nullptr;

	/** Optional CanvasPanel in Blueprint that receives one item overlay widget per item origin. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> ItemCanvas = nullptr;

	/** Required top-most hit-test-invisible canvas that exclusively owns the snapped target ghost. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> PreviewCanvas = nullptr;

	/** Optional SizeBox root in Blueprint; used to enforce fixed grid dimensions. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSizeBox = nullptr;

	/** Widget class used for one item overlay spanning its occupied grid cells. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Grid")
	TSubclassOf<URpgInventorySpatialItemWidget> SpatialItemWidgetClass;

	/** Widget class used for one designable background cell at each grid coordinate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Grid")
	TSubclassOf<URpgInventorySpatialCellWidget> SpatialCellWidgetClass;

	/** Exact authored class for the snapped target ghost. Missing configuration fails closed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Grid|Preview")
	TSubclassOf<URpgInventoryDragVisualWidget> SpatialPreviewWidgetClass;

	/** Width and height of one cell in Slate units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Spatial Grid", meta = (ClampMin = "1", UIMin = "1"))
	float CellSize = 70.0f;

	/** Space between cells in Slate units. Does not stretch the item footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Spatial Grid", meta = (ClampMin = "0", UIMin = "0"))
	float CellPadding = 2.0f;

	/** Fraction of one cell stride retained past a snap midpoint to prevent pointer flicker at cell boundaries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Spatial Grid|Interaction", meta = (ClampMin = "0.0", ClampMax = "0.25", UIMin = "0.0", UIMax = "0.25"))
	float SnapHysteresisFraction = 0.08f;

	/** Base cell color for the native grid background. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Spatial Grid|Style")
	FLinearColor CellFillColor = FLinearColor(0.08f, 0.075f, 0.075f, 0.85f);

	/** Cell border color for the native grid background. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Spatial Grid|Style")
	FLinearColor CellBorderColor = FLinearColor(0.28f, 0.28f, 0.28f, 0.95f);

	/** Cursor border color shown only when this grid is the active controller panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Spatial Grid|Style")
	FLinearColor CursorColor = FLinearColor(0.85f, 0.78f, 0.42f, 1.0f);

	/** Draws the old native grid lines for debugging missing cell widgets. Leave disabled for designer-authored UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Spatial Grid|Debug")
	bool bUseNativeDebugPaint = false;

private:
	UFUNCTION()
	void RefreshFromPanelViewModel();

	UFUNCTION()
	void HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	UFUNCTION()
	void HandleSpatialPreviewChanged(const FRpgInventorySpatialPreviewDescriptor& Descriptor);

	void EnsureRuntimeWidgets();
	void UpdateGridSizeFromBinding();
	void UpdateDesiredGridSize();
	void RebuildCellLayer();
	void RebuildItemOverlay();
	void ApplyEntryDimming();
	void UpdateCellVisualStates();
	void UpdateSpatialPreviewGhost();
	void ClearSpatialPreviewLocal();
	void ReplanActivePointerPreview();
	URpgInventoryDragVisualWidget* EnsureSpatialPreviewGhost();
	ERpgInventorySpatialCellVisualState GetCellVisualState(
		int32 X,
		int32 Y,
		const FRpgInventorySpatialPreviewDescriptor& PreviewDescriptor) const;
	bool BuildCellPreviewDescriptor(
		FRpgInventorySpatialPreviewDescriptor& OutDescriptor) const;
	bool ResolvePayloadPreviewCellState(
		const FRpgInventorySpatialPreviewDescriptor& PreviewDescriptor,
		int32 X,
		int32 Y,
		ERpgInventorySpatialCellVisualState& OutState) const;
	bool ResolveSpatialTargetGeometryAtScreenPosition(
		const FRpgInventoryDragPayload& Payload,
		FVector2D ScreenPosition,
		FRpgInventorySpatialPreviewDescriptor& OutDescriptor) const;
	void ClearObservedSlotDelegates();
	void ObserveSlotDelegates();
	void ClearObservedEntryDelegates();
	void ObserveEntryDelegates();
	void NotifySelectionChanged();
	bool MoveCursorBy(int32 DeltaX, int32 DeltaY, APlayerController* OwningPlayer);
	URpgInventoryItemInstance* GetSelectedItemInstance() const;
	int32 GetSelectedItemStackCount() const;
	bool TryGetCellFromLocalPosition(FVector2D LocalPosition, int32& OutX, int32& OutY) const;
	bool TryGetCellFromScreenPosition(FVector2D ScreenPosition, int32& OutX, int32& OutY) const;
	FRpgInventoryDropTarget MakeDropTargetAtCursor() const;
	FRpgInventoryDropTarget MakeDropTargetForCell(int32 X, int32 Y) const;
	FRpgInventoryDropTarget MakeDropTargetForCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y) const;
	FRpgInventoryDropTarget MakeDropTargetForPlacement(const FRpgInventoryDragPayload& Payload, const FRpgInventoryGridPlacement& TargetPlacement) const;
	FRpgInventoryGridPlacement MakeTargetPlacementForCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y) const;
	FRpgInventoryDragPayload MakePayloadFromSelectedItem() const;
	URpgInventoryAddressSlotViewModel* FindAddressCell(int32 X, int32 Y) const;
	URpgInventoryAddressSlotViewModel* FindAddressItemAtCell(int32 X, int32 Y) const;
	URpgInventoryEntryViewModel* FindEntryAtCell(int32 X, int32 Y) const;
	URpgInventoryManagerComponent* ResolveGridInventory() const;
	FGeometry GetGridInteractionGeometry() const;
	FVector2D GetGridLocalSize() const;
	FVector2D GetCellPosition(int32 X, int32 Y) const;
	FVector2D GetPlacementSize(const FRpgInventoryGridPlacement& Placement) const;
	FRpgInventoryContainerHandle ResolveContainerHandle() const;
	bool IsValidCell(int32 X, int32 Y) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> GroupViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelViewModel> PanelViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> Inventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationId = NAME_None;

	/** Exact graph identity used by every cell, preview, and interaction in this grid. */
	UPROPERTY(Transient)
	FRpgInventoryContainerHandle ContainerHandle;

	UPROPERTY(Transient)
	FRpgInventoryGridSize GridSize;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventoryAddressSlotViewModel>> ObservedAddressSlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventoryEntryViewModel>> ObservedEntries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventorySpatialCellWidget>> CellWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventorySpatialItemWidget>> ItemWidgets;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragVisualWidget> SpatialPreviewGhost = nullptr;

	/** Cached ghost content; pointer movement only changes render translation and never rebuilds the icon brush. */
	TWeakObjectPtr<URpgInventoryItemInstance> SpatialPreviewConfiguredItem;
	FGuid SpatialPreviewConfiguredEntryId;
	FRpgInventoryGridSize SpatialPreviewConfiguredFootprint;
	int32 SpatialPreviewConfiguredStackCount = INDEX_NONE;
	ERpgInventoryDragSourceType SpatialPreviewConfiguredSourceType = ERpgInventoryDragSourceType::None;
	bool bSpatialPreviewGhostConfigured = false;

	/** Screen-owned presentation host; transient and cleared whenever this pooled grid releases its binding. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionScreenWidget> InventoryPresentationHost = nullptr;

	/** Replicated entry ids currently rendered with reduced opacity by a UI-only search/filter presenter. */
	TSet<FGuid> DimmedEntryIds;

	/** Render opacity used for DimmedEntryIds. It never disables item input or changes grid occupancy. */
	float DimmedEntryOpacity = 0.25f;

	UPROPERTY(Transient)
	FRpgInventorySpatialPreviewDescriptor ActiveSpatialPreview;

	int32 CursorX = 0;
	int32 CursorY = 0;
	bool bInventoryPanelActive = true;
	bool bSelectionVisualSuppressed = false;

	/** Stable replicated entry identity captured while the exact split dialog is open. */
	FGuid PendingSplitEntryId;

	/** Persistent item identity used to reject stale/recycled view-model confirmation. */
	FRpgInventoryItemId PendingSplitItemId;

	int32 PendingSplitMaximum = 0;
	bool bPendingLeftClickAccept = false;
	FVector2D LastPointerPreviewScreenPosition = FVector2D::ZeroVector;
	bool bHasLastPointerPreviewScreenPosition = false;
};
