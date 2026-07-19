#pragma once

#include "CommonTileView.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventorySpatialTypes.h"

#include "RpgPlayerInventoryLayoutViews.generated.h"

class APlayerController;
class URpgActionBarSlotWidget;
class URpgActionBarSlotViewModel;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryEntryViewModel;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventoryPanelViewModel;
class URpgInventoryDragVisualWidget;
class URpgInventoryInteractionScreenWidget;
class URpgInventorySlotGroupViewModel;
class UCanvasPanel;
class UDragDropOperation;
class UImage;
class UPanelWidget;
class USizeBox;
class UTexture2D;
class UUserWidget;
class UWidget;

/**
 * TileView specialization for 1..8 actionbar slot VMs.
 *
 * The actionbar remains a non-spatial single-slot strip, so CommonTileView is still appropriate here.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgActionBarTileView : public UCommonTileView
{
	GENERATED_BODY()

public:
	explicit URpgActionBarTileView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by generated actionbar slot entries. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slots")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Replaces list items with the supplied actionbar slot VMs. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slots")
	void SetActionBarSlotItems(const TArray<URpgActionBarSlotViewModel*>& InSlots);

	/** Current selected actionbar slot view model, or null when selection is empty/non-actionbar. */
	UFUNCTION(BlueprintPure, Category = "Action Bar|Navigation")
	URpgActionBarSlotViewModel* GetSelectedActionBarSlot() const;

	/** Selects a list item and moves controller focus to this actionbar TileView. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	bool SelectActionBarListItem(UObject* Item, APlayerController* OwningPlayer);

	/** Selects current valid selection or the first actionbar slot. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	bool SelectBestActionBarSlot(APlayerController* OwningPlayer);

	/** Selects by zero-based actionbar slot index. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	bool SelectActionBarSlotByIndex(int32 SlotIndex, APlayerController* OwningPlayer);

	/** Clears only the visible ListView selection. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	void ClearActionBarSelectionVisual();

	/** Updates mouse-drag hover feedback for the actionbar slot under a screen position. */
	bool PreviewPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition);

	/** Commits a mouse-drag payload to the actionbar slot under a screen position. */
	bool CommitPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition);

	/** Whether the screen position resolves to one concrete displayed 1-8 slot rather than only the TileView bounds. */
	bool HasActionBarSlotAtScreenPosition(FVector2D ScreenPosition) const;

	/** Clears transient mouse-drag feedback on displayed actionbar slots. */
	void ClearExternalPreviewPayloads();

	/** Marks this actionbar panel as the active controller target. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	void SetActionBarPanelActive(bool bInActionBarPanelActive);

	/** Registers this TileView with the screen-local panel navigator so selection changes update active-panel routing. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId);

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;
	virtual TOptional<EItemDropZone> HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual FReply HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual void OnSelectionChangedInternal(UObject* FirstSelectedItem) override;

private:
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;
	void ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const;
	URpgActionBarSlotWidget* FindActionBarSlotWidgetAtScreenPosition(FVector2D ScreenPosition) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationId = NAME_None;

	bool bActionBarPanelActive = true;
	bool bSuppressPanelSelectionNotify = false;
};

class URpgInventorySpatialGridWidget;

/** UI-only visual state for one designable spatial inventory cell widget. */
UENUM(BlueprintType)
enum class ERpgInventorySpatialCellVisualState : uint8
{
	/** Normal empty or background cell state. */
	Normal,

	/** Pointer is hovering this cell and no stronger state is active. */
	Hovered,

	/** Logical controller cursor is on this cell. */
	Selected,

	/** Current held or dragged payload can be placed on this cell. */
	ValidPreview,

	/** Current held or dragged payload cannot be placed on this cell. */
	InvalidPreview,

	/** A locally valid placement is awaiting an authoritative server acknowledgement. */
	PendingPreview,

	/** The authoritative server rejected the most recent placement request. */
	RejectedPreview,

	/** Cell is occupied by the origin cell of an item overlay. */
	Occupied,

	/** Cell is covered by a multi-cell item whose origin is elsewhere. */
	Covered
};

/**
 * Designable background cell for one spatial inventory grid coordinate.
 *
 * Cells are presentation and hit-test widgets only. They never own item truth; placement validation still routes
 * through the owning grid and server-authoritative inventory actions.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventorySpatialCellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySpatialCellWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the owning grid and fixed grid coordinate represented by this cell. */
	void SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid, int32 InCellX, int32 InCellY);

	/** Assigns optional player-layout or storage state represented underneath this cell. */
	void SetCellViewModels(URpgInventoryAddressSlotViewModel* InAddressSlotViewModel, URpgInventoryEntryViewModel* InEntryViewModel);

	/** Updates the visual state sent to the Blueprint styling hook. */
	void SetCellVisualState(ERpgInventorySpatialCellVisualState InVisualState);

	/** X coordinate represented by this cell. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	int32 GetCellX() const { return CellX; }

	/** Y coordinate represented by this cell. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	int32 GetCellY() const { return CellY; }

	/** Player address VM represented by this cell, if this grid is bound to the player layout. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	URpgInventoryAddressSlotViewModel* GetAddressSlotViewModel() const { return AddressSlotViewModel.Get(); }

	/** Storage entry VM occupying this cell, if any. Empty storage cells have no entry VM. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	URpgInventoryEntryViewModel* GetEntryViewModel() const { return EntryViewModel.Get(); }

	/** Current visual state after hover/selection/preview resolution. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	ERpgInventorySpatialCellVisualState GetCurrentCellVisualState() const { return CurrentVisualState; }

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Called after this cell receives its grid coordinate and optional backing VM references. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Cell", meta = (DisplayName = "On Spatial Cell Set"))
	void BP_OnSpatialCellSet(int32 NewCellX, int32 NewCellY, URpgInventoryAddressSlotViewModel* NewAddressSlotViewModel, URpgInventoryEntryViewModel* NewEntryViewModel);

	/** Called when selection, hover, occupancy, or drop preview changes how this cell should look. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Cell", meta = (DisplayName = "On Spatial Cell State Changed"))
	void BP_OnSpatialCellStateChanged(ERpgInventorySpatialCellVisualState NewState);

	/** Optional background styled natively before Blueprint animation runs, guaranteeing stale tint reset. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Spatial Cell|Style")
	TObjectPtr<UImage> Image_Background = nullptr;

	/** Neutral tint used for Normal, Occupied, and Covered states. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor NeutralTint = FLinearColor::White;

	/** Subtle mouse-hover tint when no stronger state is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor HoveredTint = FLinearColor(0.36f, 0.36f, 0.36f, 1.0f);

	/** Logical controller-selection tint; pointer hover never writes this state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor SelectedTint = FLinearColor(0.85f, 0.78f, 0.42f, 1.0f);

	/** Tint shared by locally valid Move, Merge, Swap, and Equip footprints. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor ValidPreviewTint = FLinearColor(0.08f, 0.82f, 0.18f, 0.82f);

	/** Tint for blocked and out-of-bounds footprints. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor InvalidPreviewTint = FLinearColor(0.82f, 0.06f, 0.05f, 0.88f);

	/** Tint while waiting for a server acknowledgement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor PendingPreviewTint = FLinearColor(1.0f, 0.48f, 0.04f, 0.9f);

	/** Tint used briefly after authoritative rejection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor RejectedPreviewTint = FLinearColor(1.0f, 0.02f, 0.02f, 1.0f);

private:
	void ApplyResolvedVisualState();
	void ApplyNativeVisualStyle(ERpgInventorySpatialCellVisualState NewState);
	ERpgInventorySpatialCellVisualState ResolveHoveredVisualState() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySpatialGridWidget> OwningGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotViewModel> AddressSlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> EntryViewModel = nullptr;

	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	bool bHovered = false;
	bool bPendingLeftClickAccept = false;
	bool bHasAppliedVisualState = false;
	ERpgInventorySpatialCellVisualState BaseVisualState = ERpgInventorySpatialCellVisualState::Normal;
	ERpgInventorySpatialCellVisualState CurrentVisualState = ERpgInventorySpatialCellVisualState::Normal;
};

/**
 * Item overlay used by spatial grids.
 *
 * One widget is created per item origin. It spans the replicated item footprint on the grid CanvasPanel and forwards
 * mouse drag/drop to the UI-local coordinator while the server remains authoritative for final placement.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventorySpatialItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySpatialItemWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the owning grid so pointer input can update the logical cursor. */
	void SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid);

	/** Assigns the screen-local drag/drop coordinator used for payload preview and commits. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Item")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Binds this overlay to one player-inventory address origin. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Item")
	void SetAddressSlotViewModel(URpgInventoryAddressSlotViewModel* InSlotViewModel);

	/** Binds this overlay to one storage/crafting inventory entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Item")
	void SetEntryViewModel(URpgInventoryEntryViewModel* InEntryViewModel);

	/** Marks whether the parent spatial panel is active for controller highlight purposes. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Item")
	void SetInventoryPanelActive(bool bInInventoryPanelActive);

	/** Recomputes held-item/drop-target visuals and calls the Blueprint hook. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Item")
	void RefreshDragDropVisualState();

	/** Player address VM represented by this item overlay, if any. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Item")
	URpgInventoryAddressSlotViewModel* GetAddressSlotViewModel() const { return AddressSlotViewModel.Get(); }

	/** Storage/crafting entry VM represented by this item overlay, if any. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Item")
	URpgInventoryEntryViewModel* GetEntryViewModel() const { return EntryViewModel.Get(); }

	/** Current presentation state for hover/focus/held feedback. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Item")
	ERpgInventorySlotDragVisualState GetCurrentDragDropVisualState() const { return CurrentDragDropVisualState; }

	/** Stable replicated entry id used by the owning grid to reuse this widget across placement refreshes. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Item")
	FGuid GetRepresentedEntryId() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Called when this overlay receives or refreshes a player address VM. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Item", meta = (DisplayName = "On Spatial Address Item Set"))
	void BP_OnSpatialAddressItemSet(URpgInventoryAddressSlotViewModel* NewSlotViewModel);

	/** Called when this overlay receives or refreshes a storage entry VM. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Item", meta = (DisplayName = "On Spatial Entry Item Set"))
	void BP_OnSpatialEntryItemSet(URpgInventoryEntryViewModel* NewEntryViewModel);

	/** Called whenever held payload or focus changes the overlay visual state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Item", meta = (DisplayName = "On Spatial Item DragDrop State Changed"))
	void BP_OnSpatialItemDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

	/** Canonical presentation-only drag decorator. Unset uses the native emergency fallback until Step 11. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Item|Drag")
	TSubclassOf<URpgInventoryDragVisualWidget> DragVisualClass;

	/**
	 * Optional canonical item presentation shared with the drag ghost.
	 * Name a child widget "ItemVisual"; it receives the authoritative placement rotation and grid cell metrics while
	 * this outer widget remains the hit target. When bound, native fallback icon/count painting is suppressed.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Spatial Item|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<URpgInventoryDragVisualWidget> ItemVisual = nullptr;

	/** Draws a minimal icon/count fallback when no Blueprint presentation is supplied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Item")
	bool bUseNativeFallbackPaint = true;

private:
	friend class URpgInventorySpatialGridWidget;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgSpatialItemPresentationLifecycleTest;
#endif

	UFUNCTION()
	void HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	FRpgInventoryDragPayload MakeDragPayload() const;
	void ReleaseSpatialItemState();
	void RefreshPlacedItemVisual();
	bool IsPlacedItemRotated() const;
	TSoftObjectPtr<UTexture2D> GetIcon() const;
	int32 GetStackCount() const;
	bool IsFocusedItem() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySpatialGridWidget> OwningGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotViewModel> AddressSlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> EntryViewModel = nullptr;

	UPROPERTY(Transient)
	ERpgInventorySlotDragVisualState CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	bool bInventoryPanelActive = true;
	bool bPendingLeftClickAccept = false;
	FRpgInventoryDragAnchor PendingPointerDragAnchor;
	bool bHasPendingPointerDragAnchor = false;
	bool bSpatialItemStateReleased = false;
};

/**
 * Native fixed-layout spatial inventory grid.
 *
 * The grid draws cells itself, owns a logical cursor, and positions one item widget per origin on a CanvasPanel.
 * It supports both player layout groups and storage inventory panels without using TileView/ListView for spatial data.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventorySpatialGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySpatialGridWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Binds this grid to one player-inventory slot group such as Pockets, Backpack, Belt, or a carry slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void BindSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

	/** Binds this grid to one storage/crafting inventory panel and concrete container id. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Grid")
	void BindInventoryPanelViewModel(URpgInventoryPanelViewModel* InPanelViewModel, URpgInventoryManagerComponent* InInventory, FName InContainerId);

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
		FRpgInventorySpatialPreviewDescriptor& OutDescriptor) const;
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

	/** Optional top-most hit-test-invisible canvas for the snapped target ghost; ItemCanvas is the runtime fallback. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
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

	/** Designer subclass for the snapped target ghost. Native drag visual is used when unset. */
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
	URpgInventoryDragVisualWidget* EnsureSpatialPreviewGhost();
	ERpgInventorySpatialCellVisualState GetCellVisualState(int32 X, int32 Y) const;
	bool ResolvePayloadPreviewCellState(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y, ERpgInventorySpatialCellVisualState& OutState) const;
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
	FName ResolveContainerId() const;
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

	UPROPERTY(Transient)
	FName ContainerId = NAME_None;

	/** Exact graph identity; ContainerId remains as a legacy root-container adapter. */
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

/**
 * Widget for one visible spatial slot group.
 *
 * It owns or binds a URpgInventorySpatialGridWidget directly; no ListView entry recycling is used for spatial groups.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventorySlotGroupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySlotGroupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact manual MVVM source name authored by the canonical spatial slot-group widget. */
	static const FName SlotGroupViewModelSourceName;

	/** Assigns the screen-local coordinator and forwards it to the spatial grid. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Assigns the screen-local panel navigator used for LB/RB focus and shortcut routing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix);

	/**
	 * Assigns the group VM to both the native spatial grid and the authored read-only MVVM leaf source.
	 * Spatial groups are created by a panel builder, not a ListView.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

	/** Spatial grid owned by this group widget, if the Blueprint supplied one. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Slot Group")
	URpgInventorySpatialGridWidget* GetSpatialGridWidget() const { return SpatialGrid.Get(); }

	/** Stable container/group id used by the parent panel to retain this widget across VM refreshes. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Slot Group")
	FName GetSlotGroupId() const;

	/** Full root or item-owned identity used to distinguish equal local container ids. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Slot Group")
	FRpgInventoryContainerHandle GetSlotGroupHandle() const;

protected:
	virtual void NativeDestruct() override;

	/** Optional inner spatial grid. Name the widget SpatialGrid for automatic binding. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialGridWidget> SpatialGrid = nullptr;

	/** Deprecated fallback setting. The Blueprint should provide a child named SpatialGrid for designer-owned layout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Slot Group")
	TSubclassOf<URpgInventorySpatialGridWidget> SpatialGridWidgetClass;

private:
	bool InjectSlotGroupViewModelIntoMvvm();
	void EnsureSpatialGrid();
	void RegisterPanelNavigationEntry();
	URpgInventoryManagerComponent* ResolveGroupInventory() const;
	FName MakePanelNavigationId() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> GroupViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationIdPrefix = NAME_None;
};

/**
 * Plain panel builder for carry/content spatial groups.
 *
 * It creates URpgInventorySlotGroupWidget children directly, avoiding CommonListView/ListView recycling for spatial
 * inventory containers whose layout must stay fixed regardless of resolution.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventorySlotGroupPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySlotGroupPanelWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by generated group widgets and their spatial grids. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Assigns the screen-local navigator used by generated group widgets and their spatial grids. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix);

	/** Reconciles children by stable group id so existing grids retain cursor, focus, and interaction state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups);

	/** Appends all currently generated spatial grids to OutGrids for screen-level drag/drop routing. */
	void GetSpatialGridWidgets(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;

protected:
	/** Optional Blueprint panel that receives group widgets. If unset, an existing root panel is used. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> GroupsPanel = nullptr;

	/** Widget class used for one spatial group. Blueprint should usually point this at CUI_InventorySlotGroupEntry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Slot Group")
	TSubclassOf<URpgInventorySlotGroupWidget> GroupWidgetClass;

private:
	void EnsureGroupsPanel();
	void RebuildGroupWidgets();
	void ApplyCoordinatorToGroup(URpgInventorySlotGroupWidget* GroupWidget) const;
	void ApplyNavigationToGroup(URpgInventorySlotGroupWidget* GroupWidget) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationIdPrefix = NAME_None;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventorySlotGroupViewModel>> GroupItems;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventorySlotGroupWidget>> GroupWidgets;
};
