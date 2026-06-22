#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"

#include "RpgInventorySlotEntryWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryEntryViewModel;
class URpgInventoryManagerComponent;

/**
 * Native base for TileView inventory slot entries.
 *
 * The widget stores the current entry view model assigned by UTileView and forwards controller
 * Accept input to the screen-local drag/drop coordinator. Blueprint children should keep only
 * presentation logic here: icon, stack text, hover/drop highlights, and tooltips.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventorySlotEntryWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	explicit URpgInventorySlotEntryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local drag/drop coordinator that owns controller held-item state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Current entry view model assigned by the owning TileView. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	URpgInventoryEntryViewModel* GetEntryViewModel() const { return EntryViewModel.Get(); }

	/** Current drag/drop coordinator assigned by the owning inventory TileView or screen widget. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	URpgInventoryDragDropCoordinator* GetDragDropCoordinator() const { return DragDropCoordinator.Get(); }

	/** Controller/CommonUI Accept helper: pick the item up or place the currently held item on this slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool HandleEntryAccept();

	/** Shortcut helper for Ctrl+Click or controller X: transfers the whole stack to the route's target inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool HandleEntryQuickTransfer(URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr);

	/** Shortcut helper for Shift+Click or controller Y: splits this stack into a separate stack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool HandleEntryQuickSplit(int32 SplitCount = 0, int32 TargetSlotIndex = -1);

	/** Recomputes drag/drop presentation state and calls the Blueprint visual hook. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void RefreshDragDropVisualState();

	/** Current drag/drop presentation state for this entry widget. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	ERpgInventorySlotDragVisualState GetCurrentDragDropVisualState() const { return CurrentDragDropVisualState; }

protected:
	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface

	//~IUserListEntry interface
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnClicked() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~End of IUserListEntry interface

	/** Replays the presentation hook when the assigned slot view model updates in place. */
	UFUNCTION()
	void HandleEntryViewModelChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel);

	/** Blueprint presentation hook called whenever this recycled entry receives a new view model. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Entry", meta = (DisplayName = "On Inventory Entry ViewModel Set"))
	void BP_OnInventoryEntryViewModelSet(URpgInventoryEntryViewModel* NewEntryViewModel);

	/** Blueprint presentation hook called when the TileView releases this entry for reuse. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Entry", meta = (DisplayName = "On Inventory Entry Released"))
	void BP_OnInventoryEntryReleased();

	/** Blueprint presentation hook for selected/focused list state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Entry", meta = (DisplayName = "On Inventory Entry Selection Changed"))
	void BP_OnInventoryEntrySelectionChanged(bool bIsSelected);

	/** Blueprint presentation hook for controller held-item and drop-target highlights. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|DragDrop", meta = (DisplayName = "On Inventory Entry DragDrop State Changed"))
	void BP_OnInventoryEntryDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

private:
	bool TryHandleModifiedLeftMouseButtonDown(const FPointerEvent& InMouseEvent, bool bLogFailure);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Entry", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryEntryViewModel> EntryViewModel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	ERpgInventorySlotDragVisualState CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	bool bEntrySelected = false;
};
