#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"

#include "RpgInventoryAddressSlotWidget.generated.h"

class UDragDropOperation;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryDragDropCoordinator;

/**
 * Native CommonUI slot entry for one logical player-inventory address such as Belt[0] or WeaponSlot1[0].
 *
 * Use this as the EntryWidgetClass inside group TileViews. The widget owns no gameplay state; it forwards
 * drag/drop and controller Accept to URpgInventoryDragDropCoordinator.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventoryAddressSlotWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAddressSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local drag/drop coordinator shared by player inventory, gear slots, and actionbar. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Current logical slot VM assigned by the owning list/tile view. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Address Slot")
	URpgInventoryAddressSlotViewModel* GetAddressSlotViewModel() const { return SlotViewModel.Get(); }

	/** Controller/CommonUI Accept: pick this item up or place the held item onto this address. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	bool HandleSlotAccept();

	/** Recomputes held-item/drop-target visuals and calls the Blueprint visual hook. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	void RefreshDragDropVisualState();

	/** Current drag/drop presentation state for this slot widget. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Address Slot")
	ERpgInventorySlotDragVisualState GetCurrentDragDropVisualState() const { return CurrentDragDropVisualState; }

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnClicked() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Blueprint presentation hook called when this recycled entry receives a new address slot VM. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot ViewModel Set"))
	void BP_OnAddressSlotViewModelSet(URpgInventoryAddressSlotViewModel* NewSlotViewModel);

	/** Blueprint presentation hook called when this entry is released for reuse. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot Released"))
	void BP_OnAddressSlotReleased();

	/** Blueprint presentation hook for CommonUI selection/focus. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot Selection Changed"))
	void BP_OnAddressSlotSelectionChanged(bool bIsSelected);

	/** Blueprint presentation hook for held-item/drop-target highlights. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot DragDrop State Changed"))
	void BP_OnAddressSlotDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

private:
	UFUNCTION()
	void HandleSlotViewModelChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	FRpgInventoryDragPayload MakeDragPayload(bool bAllowEmptyAddressPayload) const;
	FRpgInventoryDropTarget MakeDropTarget() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	ERpgInventorySlotDragVisualState CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	bool bSlotSelected = false;
};
