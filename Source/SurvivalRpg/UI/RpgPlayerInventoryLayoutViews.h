#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonListView.h"
#include "CommonTileView.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"

#include "RpgPlayerInventoryLayoutViews.generated.h"

class APlayerController;
class URpgActionBarSlotViewModel;
class URpgActionBarTileView;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryAddressSlotWidget;
class URpgInventoryAddressTileView;
class URpgInventoryDragDropCoordinator;
class URpgInventoryManagerComponent;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySlotGroupWidget;
class URpgInventorySlotGroupViewModel;
class UCanvasPanel;
class UDragDropOperation;
class UOverlay;
class UUniformGridPanel;
class UUserWidget;
class UWidget;

/**
 * TileView specialization for logical player-inventory address slots.
 *
 * It assigns the screen-local drag/drop coordinator to generated URpgInventoryAddressSlotWidget entries and
 * keeps list items stable from an URpgInventorySlotGroupViewModel.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryAddressTileView : public UCommonTileView
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAddressTileView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by generated address slot entries. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Replaces list items with the supplied address slot VMs. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	void SetAddressSlotItems(const TArray<URpgInventoryAddressSlotViewModel*>& InSlots);

	/** Binds this tile view to one slot group VM and displays its Slots list. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	void BindSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

	/** Current selected address slot view model, or null when selection is empty/non-address. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Address Slots")
	URpgInventoryAddressSlotViewModel* GetSelectedAddressSlot() const;

	/** Selects a list item and moves controller focus to this address TileView. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	bool SelectAddressListItem(UObject* Item, APlayerController* OwningPlayer);

	/** Selects current valid selection, first occupied slot, then first slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	bool SelectBestAddressSlot(APlayerController* OwningPlayer, bool bPreferOccupiedSlot = true);

	/** Selects by entry id first, then global slot index. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	bool SelectAddressSlotByIdentity(FGuid EntryId, int32 GlobalSlotIndex, APlayerController* OwningPlayer);

	/** Clears only the visible ListView selection. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	void ClearAddressSelectionVisual();

	/** Mirrors selection from non-ListView spatial cells while preserving the existing controller shortcut route. */
	bool MirrorAddressSlotSelection(UObject* Item);

	/** Associates a list item with its visible spatial grid widget for controller focus redirection. */
	void RegisterAddressSlotFocusRedirect(UObject* Item, UWidget* FocusWidget);

	/** Clears spatial grid focus redirects, normally before rebuilding runtime slot widgets. */
	void ClearAddressSlotFocusRedirects();

	/** Returns the visible spatial widget that should receive focus for this address panel, if one is registered. */
	UWidget* GetAddressSlotFocusTarget() const;

	/** Marks this address panel as the active controller target. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	void SetInventoryPanelActive(bool bInInventoryPanelActive);

	/** Registers this address TileView with the screen-local panel navigator so selection changes update active-panel routing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId);

	/** Shortcut helper for quick transfer on the selected address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	bool QuickTransferSelectedAddressSlot(URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr);

	/** Shortcut helper for quick split on the selected address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	bool QuickSplitSelectedAddressSlot(int32 SplitCount = 0, int32 TargetSlotIndex = -1);

	/** Shortcut helper for use/equip/unequip on the selected address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	bool UseOrEquipSelectedAddressSlot(int32 StackCount = 1);

	/** Shortcut helper for dropping the selected address slot into the world. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slots")
	bool DropSelectedAddressSlot(int32 StackCount = 0, bool bConfirmed = false);

protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;
	virtual UDragDropOperation* HandleListEntryDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UUserWidget& EntryWidget) override;
	virtual TOptional<EItemDropZone> HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual FReply HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual void OnSelectionChangedInternal(UObject* FirstSelectedItem) override;

private:
	void RefreshAddressSlotItems();
	void ApplyBoundGridSizeToSlate();
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;
	void ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const;
	UWidget* FindAddressSlotFocusRedirect(UObject* Item) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> BoundGroupViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationId = NAME_None;

	bool bInventoryPanelActive = true;
	bool bSuppressPanelSelectionNotify = false;

	TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UWidget>> AddressSlotFocusRedirects;
};

/**
 * TileView specialization for 1..8 actionbar slot VMs.
 *
 * It turns generated URpgActionBarSlotWidget entries into SlotAddress drop targets.
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

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationId = NAME_None;

	bool bActionBarPanelActive = true;
	bool bSuppressPanelSelectionNotify = false;
};

/**
 * Native spatial item overlay used by runtime grid groups.
 *
 * It represents one item origin and spans the item's occupied cells while the slot grid below remains responsible
 * for cell selection and empty-cell drop targets. Blueprint subclasses may replace the fallback paint with richer art.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventorySpatialItemWidget : public URpgInventoryAddressSlotWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySpatialItemWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Draws a simple icon/count fallback when no Blueprint presentation is supplied for the overlay item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Item")
	bool bUseNativeFallbackPaint = true;
};

/**
 * Optional ListView entry base for one slot group row.
 *
 * If the Blueprint contains a child named SlotTileView of type URpgInventoryAddressTileView, this class binds the
 * group slots and coordinator automatically.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventorySlotGroupWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	explicit URpgInventorySlotGroupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator and forwards it to SlotTileView when present. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Assigns the screen-local panel navigator used for LB/RB focus and shortcut routing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix);

	/** Assigns the group VM manually, useful when the widget is not created by a ListView. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;

	/** Blueprint presentation hook called when this group receives or refreshes its VM. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Slot Group", meta = (DisplayName = "On Slot Group ViewModel Set"))
	void BP_OnSlotGroupViewModelSet(URpgInventorySlotGroupViewModel* NewGroupViewModel);

	/** Blueprint presentation hook called when this entry is released for reuse. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Slot Group", meta = (DisplayName = "On Slot Group Released"))
	void BP_OnSlotGroupReleased();

	/** Optional inner TileView. Name the widget SlotTileView to get automatic binding. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventoryAddressTileView> SlotTileView = nullptr;

private:
	void EnsureRuntimeSlotGridPanel();
	void RebuildRuntimeSlotGrid();
	void RebuildRuntimeItemOverlay(float SlotCellWidth, float SlotCellHeight);
	void RegisterPanelNavigationEntry();
	TSubclassOf<UUserWidget> GetAddressSlotEntryWidgetClass() const;
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

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> RuntimeSlotGridPanel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RuntimeGridOverlay = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RuntimeItemOverlayPanel = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventoryAddressSlotWidget>> RuntimeAddressSlotWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventorySpatialItemWidget>> RuntimeSpatialItemWidgets;

	/** Widget class used for one item overlay spanning its occupied grid cells. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Slot Group", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URpgInventorySpatialItemWidget> SpatialItemWidgetClass;
};

/**
 * ListView specialization for carry/backpack/belt/pocket group rows.
 *
 * It forwards the screen-local drag/drop coordinator to generated URpgInventorySlotGroupWidget entries so nested
 * address TileViews work without Blueprint loops.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventorySlotGroupListView : public UCommonListView
{
	GENERATED_BODY()

public:
	explicit URpgInventorySlotGroupListView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by generated group entries and their nested slot TileViews. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Assigns the screen-local navigator used by generated group entries and their nested slot TileViews. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix);

	/** Replaces list items with the supplied slot group VMs. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups);

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;

private:
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;
	void ApplyPanelNavigationToEntry(UUserWidget* EntryWidget) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationIdPrefix = NAME_None;
};
