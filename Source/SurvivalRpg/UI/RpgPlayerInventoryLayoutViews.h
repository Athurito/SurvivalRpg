#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonListView.h"
#include "CommonTileView.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "RpgPlayerInventoryLayoutViews.generated.h"

class URpgActionBarSlotViewModel;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryAddressTileView;
class URpgInventoryDragDropCoordinator;
class URpgInventorySlotGroupWidget;
class URpgInventorySlotGroupViewModel;
class UDragDropOperation;

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

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;
	virtual UDragDropOperation* HandleListEntryDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UUserWidget& EntryWidget) override;
	virtual TOptional<EItemDropZone> HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual FReply HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;

private:
	void RefreshAddressSlotItems();
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> BoundGroupViewModel = nullptr;
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

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;
	virtual TOptional<EItemDropZone> HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual FReply HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;

private:
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;
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
	/** Assigns the screen-local coordinator and forwards it to SlotTileView when present. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

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
	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> GroupViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;
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

	/** Replaces list items with the supplied slot group VMs. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups);

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;

private:
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;
};
