#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"

#include "RpgLoadoutSlotWidgets.generated.h"

class URpgEquipmentSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryItemInstance;
class UDragDropOperation;
class UUserWidget;

/**
 * Native button base for one equipment slot such as MainHand, OffHand, Head, Chest, Hands, Legs, or Feet.
 *
 * Use this as the parent for CUI equipment slot widgets in the player inventory screen.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgEquipmentSlotWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the VM for the dedicated equipment slot represented by this widget. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	void SetEquipmentSlotViewModel(URpgEquipmentSlotViewModel* InSlotViewModel);

	/** Assigns the screen-local drag/drop coordinator shared by inventory and equipment widgets. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Current dedicated equipment slot VM represented by this widget. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	URpgEquipmentSlotViewModel* GetEquipmentSlotViewModel() const { return SlotViewModel.Get(); }

	/** Equipment slot represented by this widget. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	ERpgEquipmentSlot GetResolvedEquipmentSlot() const;

	/** Item assigned to this slot, or null when empty. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	URpgInventoryItemInstance* GetRepresentedItem() const;

	/** Controller Accept helper: pick the slot assignment up or place the currently held payload here. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	bool HandleSlotAccept();

	/** Clears this equipment slot assignment through the server-validated UI action path. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	bool HandleClearAssignment();

	/** Recomputes drag/drop presentation state and calls the Blueprint visual hook. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	void RefreshDragDropVisualState();

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnClicked() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Blueprint presentation hook called whenever the represented equipment slot changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment|Slot", meta = (DisplayName = "On Equipment Slot Updated"))
	void BP_OnEquipmentSlotUpdated(URpgEquipmentSlotViewModel* NewSlotViewModel, URpgInventoryItemInstance* ItemInstance, bool bHasItem);

	/** Blueprint presentation hook for held-item/drop-target highlights. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment|Slot", meta = (DisplayName = "On Equipment Slot DragDrop State Changed"))
	void BP_OnEquipmentSlotDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

	/** Dedicated equipment slot represented before or without a VM assignment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Slot")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::Head;

private:
	UFUNCTION()
	void HandleSlotViewModelChanged(URpgEquipmentSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	FReply HandlePointerButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);
	FRpgInventoryDragPayload MakeDragPayload() const;
	FRpgInventoryDropTarget MakeDropTarget() const;
	bool IsHeldSource() const;

	/** Optional mouse drag visual class for equipped gear slots. Leave unset to reuse this slot widget class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Slot|Drag", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> DragVisualClass;

	UPROPERTY(Transient)
	TObjectPtr<URpgEquipmentSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	bool bPendingLeftClickAccept = false;
};
