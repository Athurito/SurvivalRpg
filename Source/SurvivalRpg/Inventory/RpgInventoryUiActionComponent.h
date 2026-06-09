#pragma once

#include "Components/ControllerComponent.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgInventoryUiActionComponent.generated.h"

class URpgEquipmentLoadoutComponent;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgQuickBarComponent;

/**
 * Owned controller component that turns UI drag-and-drop intents into server-validated inventory actions.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgInventoryUiActionComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgInventoryUiActionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns an owned inventory item to a quickbar hand slot without removing it from inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestAssignItemToQuickBar(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Swaps or moves two quickbar hand-slot assignments. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestSwapQuickBarSlots(int32 SourceSlotIndex, ERpgEquipmentSlot SourceEquipmentSlot, int32 TargetSlotIndex, ERpgEquipmentSlot TargetEquipmentSlot);

	/** Clears one quickbar hand slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearQuickBarSlot(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot);

	/** Assigns an owned inventory item to a dedicated equipment slot such as Head or Chest. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Clears one dedicated equipment slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Transfers a whole item entry or partial stack between two accessible inventories. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Applies a shared server-side sort to an accessible inventory such as storage or loot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestApplyInventorySort(URpgInventoryManagerComponent* Inventory, ERpgInventorySortMode SortMode);

	/** Moves one accessible inventory entry to a shared replicated index for manual storage ordering. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveInventoryEntry(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|UI Actions")
	bool CanAccessInventory(URpgInventoryManagerComponent* Inventory) const;

private:
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgQuickBarComponent* FindQuickBar() const;
	URpgEquipmentLoadoutComponent* FindEquipmentLoadout() const;
	bool CanTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount) const;
	void ClearPlayerAssignmentsForItem(URpgInventoryItemInstance* Item) const;
};
