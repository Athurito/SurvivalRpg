#pragma once

#include "Components/ControllerComponent.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgInventoryUiActionComponent.generated.h"

class ARpgBaseCampActor;
class ARpgBaseConstructionSiteActor;
class URpgEquipmentLoadoutComponent;
class URpgBaseBuildableDefinition;
class URpgBaseStorageStationComponent;
class URpgBaseStorageUpgradeDefinition;
class URpgCraftingRecipeDefinition;
class URpgCraftingStationComponent;
class URpgInventoryItemDefinition;
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

	/** Transfers a stack into one exact target slot. Explicit drag/drop uses this instead of auto-stacking. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestTransferItemStackToInventorySlot(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex);

	/** Applies a shared server-side sort to an accessible inventory such as storage or loot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestApplyInventorySort(URpgInventoryManagerComponent* Inventory, ERpgInventorySortMode SortMode);

	/** Moves one accessible inventory entry to a shared replicated index for manual storage ordering. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveInventoryEntry(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetIndex);

	/** Moves, swaps, or stack-merges an accessible inventory entry into one exact slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveInventoryEntryToSlot(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetSlotIndex);

	/** Deposits all material stacks from the player inventory into the linked base storage station. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestDepositAllMaterialsToBase(URpgBaseStorageStationComponent* Station);

	/** Deposits one material stack from the player inventory into the linked base storage station. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestDepositMaterialStackToBase(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Withdraws resources from the linked base storage station into the player inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestWithdrawResourceFromBase(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount);

	/** Stores an instance-based player item in the linked base armory inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestStoreItemInstanceInBase(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Takes an instance-based item from the linked base armory inventory into the player inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestTakeItemInstanceFromBase(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Installs a data-driven upgrade on a base storage station after paying material costs. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestInstallBaseStorageUpgrade(URpgBaseStorageStationComponent* Station, URpgBaseStorageUpgradeDefinition* UpgradeDefinition);

	/** Applies a shared server-side sort to the linked base resource rows. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestApplyBaseResourceSort(URpgBaseStorageStationComponent* Station, ERpgInventorySortMode SortMode);

	/** Moves one linked base resource row to a shared replicated index. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestMoveBaseResourceEntry(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex);

	/** Places a replicated construction site for a buildable near a base camp. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Building")
	void RequestPlaceBaseBuildable(ARpgBaseCampActor* BaseCamp, URpgBaseBuildableDefinition* BuildableDefinition, FTransform BuildTransform, bool bAutoContributeFromBase);

	/** Contributes all available matching resources to a construction site. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Building")
	void RequestContributeAllToBaseConstructionSite(ARpgBaseConstructionSiteActor* ConstructionSite, bool bAllowBaseStorage);

	/** Contributes one resource type to a construction site. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Building")
	void RequestContributeMaterialToBaseConstructionSite(ARpgBaseConstructionSiteActor* ConstructionSite, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, bool bAllowBaseStorage);

	/** Queues one or more units of a recipe through an accessible crafting station. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestCraftRecipe(URpgCraftingStationComponent* CraftingStation, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity = 1);

	/** Cancels one queued or active crafting job and refunds the unfinished resource credits. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestCancelCraftJob(URpgCraftingStationComponent* CraftingStation, FGuid JobId);

	/** Pauses a whole crafting station queue. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestPauseCraftingStation(URpgCraftingStationComponent* CraftingStation);

	/** Resumes a paused crafting station queue. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestResumeCraftingStation(URpgCraftingStationComponent* CraftingStation);

	/** Toggles whether an accessible crafting station auto-deposits finished outputs into linked base storage. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestSetCraftingOutputAutoDepositEnabled(URpgCraftingStationComponent* CraftingStation, bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|UI Actions")
	bool CanAccessInventory(URpgInventoryManagerComponent* Inventory) const;

private:
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgQuickBarComponent* FindQuickBar() const;
	URpgEquipmentLoadoutComponent* FindEquipmentLoadout() const;
	bool CanTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount) const;
	bool CanTransferItemStackToInventorySlot(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex) const;
	bool CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const;
	void ClearPlayerAssignmentsForItem(URpgInventoryItemInstance* Item) const;
};
