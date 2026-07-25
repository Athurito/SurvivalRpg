#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"

#include "RpgInventorySpatialItemWidget.generated.h"

class URpgInventoryAddressSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryDragVisualWidget;
class URpgInventoryEntryViewModel;
class URpgInventorySpatialGridWidget;
class UDragDropOperation;
class UTexture2D;

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

	/** Exact authored presentation-only drag decorator. Missing configuration fails closed before a drag starts. */
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
	void BeginDragDropVisualRefreshBatch();
	void EndDragDropVisualRefreshBatch();
	void ApplyDragDropVisualState();
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
	int32 DragDropVisualRefreshBatchDepth = 0;
	bool bDragDropVisualRefreshPending = false;
};
