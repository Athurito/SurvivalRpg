#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"

#include "RpgLoadoutSlotWidgets.generated.h"

class URpgEquipmentSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryItemInstance;
class URpgQuickBarSlotViewModel;
class UDragDropOperation;

/**
 * Native button base for one hand inside one weapon quickbar loadout slot.
 *
 * Use this as the parent for CUI quickbar hand-slot widgets. It owns only UI input and drag/drop routing;
 * URpgQuickBarComponent remains the replicated gameplay source of truth.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgQuickBarHandSlotWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	explicit URpgQuickBarHandSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the VM for the visual quickbar slot that contains this hand slot. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|Slot")
	void SetQuickBarSlotViewModel(URpgQuickBarSlotViewModel* InSlotViewModel);

	/** Assigns the screen-local drag/drop coordinator shared by inventory, quickbar, and equipment widgets. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|Slot")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Current quickbar slot VM represented by this widget. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|Slot")
	URpgQuickBarSlotViewModel* GetQuickBarSlotViewModel() const { return SlotViewModel.Get(); }

	/** Item assigned to this hand slot, or null when empty. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|Slot")
	URpgInventoryItemInstance* GetRepresentedItem() const;

	/** Zero-based quickbar index used by command payloads. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|Slot")
	int32 GetResolvedQuickBarSlotIndex() const;

	/** Controller Accept helper: pick the slot assignment up or place the currently held payload here. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|Slot")
	bool HandleSlotAccept();

	/** Clears this quickbar hand assignment through the server-validated UI action path. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|Slot")
	bool HandleClearAssignment();

	/** Recomputes drag/drop presentation state and calls the Blueprint visual hook. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|Slot")
	void RefreshDragDropVisualState();

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnClicked() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Blueprint presentation hook called whenever the represented hand slot changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "QuickBar|Slot", meta = (DisplayName = "On QuickBar Hand Slot Updated"))
	void BP_OnQuickBarHandSlotUpdated(URpgQuickBarSlotViewModel* NewSlotViewModel, URpgInventoryItemInstance* ItemInstance, bool bHasItem, bool bIsActiveSlot);

	/** Blueprint presentation hook for held-item/drop-target highlights. */
	UFUNCTION(BlueprintImplementableEvent, Category = "QuickBar|Slot", meta = (DisplayName = "On QuickBar Hand Slot DragDrop State Changed"))
	void BP_OnQuickBarHandSlotDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

	/** Fallback quickbar slot index for manually configured widgets before a VM is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickBar|Slot", meta = (ClampMin = "0", UIMin = "0"))
	int32 QuickBarSlotIndex = 0;

	/** Hand represented by this widget. Use MainHand for weapon cells and OffHand for shield/offhand cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickBar|Slot")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::MainHand;

private:
	UFUNCTION()
	void HandleSlotViewModelChanged(URpgQuickBarSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	FRpgInventoryDragPayload MakeDragPayload() const;
	FRpgInventoryDropTarget MakeDropTarget() const;
	bool IsHeldSource() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgQuickBarSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;
};

/**
 * Native button base for one dedicated equipment slot such as Head, Chest, Hands, Legs, or Feet.
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

	/** Assigns the screen-local drag/drop coordinator shared by inventory, quickbar, and equipment widgets. */
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
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
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

	FRpgInventoryDragPayload MakeDragPayload() const;
	FRpgInventoryDropTarget MakeDropTarget() const;
	bool IsHeldSource() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgEquipmentSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;
};
