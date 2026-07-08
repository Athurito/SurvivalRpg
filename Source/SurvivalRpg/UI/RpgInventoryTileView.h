#pragma once

#include "CommonTileView.h"
#include "CoreMinimal.h"

#include "RpgInventoryTileView.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryEntryViewModel;
class URpgInventoryManagerComponent;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventoryPanelViewModel;

/**
 * CommonTileView specialization for inventory panels.
 *
 * This class centralizes ListView/TileView mouse drag/drop and controller item-accept routing so
 * entry widgets stay presentation-focused and gameplay mutations still flow through the
 * server-validated inventory UI action component.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryTileView : public UCommonTileView
{
	GENERATED_BODY()

public:
	explicit URpgInventoryTileView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by all generated entry widgets and drop commits. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Replaces the TileView items with the latest panel VM entries and requests an immediate visual refresh. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void SetInventoryEntryItems(const TArray<URpgInventoryEntryViewModel*>& InEntries);

	/** Binds this TileView directly to a panel VM and keeps list items refreshed without the MVVM TileView extension. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void BindInventoryPanelViewModel(URpgInventoryPanelViewModel* InPanelViewModel);

	/** Pulls the latest entries from the bound panel VM and applies them to this TileView. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void RefreshInventoryEntryItems();

	/** Current selected inventory entry view model, or null when selection is empty/non-inventory. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	URpgInventoryEntryViewModel* GetSelectedInventoryEntry() const;

	/** Selects and navigates to a specific list item if it belongs to this TileView. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool SelectInventoryListItem(UObject* Item, APlayerController* OwningPlayer);

	/** Selects a sensible slot for controller focus: current valid selection, first occupied slot, then first slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool SelectBestInventoryEntry(APlayerController* OwningPlayer, bool bPreferOccupiedSlot = true);

	/** Selects by stable entry id first, then by visual slot index. Used to restore focus after ViewModel refreshes. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	bool SelectInventoryEntryByIdentity(FGuid EntryId, int32 SlotIndex, APlayerController* OwningPlayer);

	/** Marks this inventory panel as the active controller target so only one panel shows focused slot visuals. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void SetInventoryPanelActive(bool bInInventoryPanelActive);

	/** Registers this TileView with the screen-local panel navigator so selection changes can update active-panel routing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId);

	/** Clears only the visible ListView selection. The panel navigator keeps the slot memory separately. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
	void ClearInventorySelectionVisual();

	/** True when this panel should show selected slots as focused. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Navigation")
	bool IsInventoryPanelActive() const { return bInventoryPanelActive; }

	/** Shortcut helper for controller X on the selected slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickTransferSelectedEntry(URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr);

	/** Shortcut helper for controller Y on the selected slot. SplitCount <= 0 performs quick 50% split. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickSplitSelectedEntry(int32 SplitCount = 0, int32 TargetSlotIndex = -1);

	/** Shortcut helper for use/equip on the selected slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool UseOrEquipSelectedEntry(int32 StackCount = 1);

	/** Shortcut helper for dropping the selected slot into the world. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool DropSelectedEntry(int32 StackCount = 0, bool bConfirmed = false);

	/** Current coordinator used by this inventory TileView. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	URpgInventoryDragDropCoordinator* GetDragDropCoordinator() const { return DragDropCoordinator.Get(); }

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;
	virtual UDragDropOperation* HandleListEntryDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UUserWidget& EntryWidget) override;
	virtual TOptional<EItemDropZone> HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual FReply HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual void OnItemClickedInternal(UObject* Item) override;
	virtual void OnSelectionChangedInternal(UObject* FirstSelectedItem) override;

private:
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;
	void ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryPanelViewModel> BoundPanelViewModel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Navigation", meta = (AllowPrivateAccess = "true"))
	FName PanelNavigationId = NAME_None;

	/** If true, TileView item click/confirm uses pick/place. Leave false when entries inherit URpgInventorySlotEntryWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	bool bUseItemClickAsAccept = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Navigation", meta = (AllowPrivateAccess = "true"))
	bool bInventoryPanelActive = true;

	bool bSuppressPanelSelectionNotify = false;
};
