#pragma once

#include "RpgInventoryUiActionComponent.h"
#include "RpgInventoryItemUseContext.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRpgInventoryUiActions, Log, All);

/**
 * Shared non-reflected context for server-side inventory UI action handlers.
 *
 * The controller-owned URpgInventoryUiActionComponent remains the sole RPC and
 * owning-client feedback endpoint. Domain handlers are synchronous C++ policy
 * objects and must never be captured by asynchronous gameplay callbacks.
 */
class FRpgInventoryUiActionDomainHandler
{
public:
	explicit FRpgInventoryUiActionDomainHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: ActionComponent(InActionComponent)
	{
	}

protected:
	AActor* GetOwner() const;
	UWorld* GetWorld() const;
	AActor* GetRequestingActor() const;

	bool CanAccessInventory(
		URpgInventoryManagerComponent* Inventory) const;
	bool CanAccessBaseStorageStation(
		const URpgBaseStorageStationComponent* Station) const;
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgEquipmentLoadoutComponent* FindEquipmentLoadout() const;
	URpgPlayerInventoryLayoutComponent* FindPlayerInventoryLayout() const;
	URpgActionBarComponent* FindActionBar() const;
	URpgAbilitySystemComponent* FindPlayerAbilitySystem() const;

	bool IsPlayerEquipmentPlacement(
		const FRpgInventoryGridPlacement& Placement) const;
	void SyncEquipmentLoadoutFromGearSlots() const;
	void SyncActiveHandsFromCarrySlots() const;

	void SendActionFeedback(
		FGameplayTag ActionTag,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FGuid& RequestId = FGuid(),
		FRpgInventoryItemId ItemId = FRpgInventoryItemId()) const;

	bool TryReplayRecentManualDropResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request);
	void SendAndCacheManualDropFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount,
		URpgInventoryManagerComponent* TargetInventory = nullptr);

	UObject* GetItemUseContextOuter() const;
	FRpgInventoryUseConsumePreflight MakeUseConsumePreflight(
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory,
		FRpgInventoryItemId ItemId,
		int32 ConsumeCount,
		TSharedRef<bool> RequiresEquipmentCleanup) const;
	FSimpleDelegate MakeUseConsumeSucceeded(
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory,
		FRpgInventoryItemId ItemId,
		TSharedRef<bool> RequiresEquipmentCleanup) const;

	TSubclassOf<ARpgDroppedInventoryActor>
		GetManualDropActorClass() const;
	float GetManualDropForwardDistance() const;
	float GetManualDropUpOffset() const;
	float GetManualDropMergeRadius() const;

	void RequestQuickTransferItem(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryQuickTransferRequest Request);

private:
	URpgInventoryUiActionComponent& ActionComponent;
};

/** Server-side Quick Access binding and Carry activation handler. */
class FRpgInventoryQuickAccessActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	using FRpgInventoryUiActionDomainHandler::
		FRpgInventoryUiActionDomainHandler;

	void ActivateCarrySlot(
		int32 ActionBarSlotIndex,
		FGameplayTag ExpectedCarrySemanticRole);
	void ClearActiveHands();
	void MutateBinding(FRpgQuickAccessMutationRequest Request);
	void BindInventorySlot(
		int32 ActionBarSlotIndex,
		FRpgInventorySlotAddress SlotAddress);
	void BindCarrySlot(
		int32 ActionBarSlotIndex,
		FRpgInventorySlotAddress CarrySlotAddress);
	void ClearCarryBinding(
		int32 ActionBarSlotIndex,
		FGameplayTag ExpectedCarrySemanticRole);
	void ClearConsumableBinding(
		int32 ActionBarSlotIndex,
		TSubclassOf<URpgInventoryItemDefinition>
			ExpectedConsumableDefinition);
};

/** Server-side item-use capability, GAS activation, and consume handler. */
class FRpgInventoryItemUseActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	using FRpgInventoryUiActionDomainHandler::
		FRpgInventoryUiActionDomainHandler;

	void ExecuteItemAction(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryItemActionRequest Request);
	void UseInventoryItem(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FGuid& RequestId = FGuid());
};

/** Server-side manual-drop validation, world-spawn, and transfer handler. */
class FRpgInventoryManualDropActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	using FRpgInventoryUiActionDomainHandler::
		FRpgInventoryUiActionDomainHandler;

	void DropInventoryItem(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		bool bConfirmed);
	void DropInventoryItemById(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryManualDropRequest Request);

private:
	bool TryTransferManualDrop(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryItemInstance* Item,
		const FRpgInventoryTransferIntent& Intent,
		URpgInventoryManagerComponent*& OutTargetInventory);
	FTransform GetManualDropTransform() const;
};

/** Server-side base resource, armory, and upgrade command handler. */
class FRpgBaseStorageActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	using FRpgInventoryUiActionDomainHandler::
		FRpgInventoryUiActionDomainHandler;

	void DepositAllMaterials(URpgBaseStorageStationComponent* Station);
	void DepositMaterialStack(
		URpgBaseStorageStationComponent* Station,
		URpgInventoryItemInstance* Item,
		int32 StackCount);
	void WithdrawResource(
		URpgBaseStorageStationComponent* Station,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 StackCount);
	void StoreItemInstance(
		URpgBaseStorageStationComponent* Station,
		URpgInventoryItemInstance* Item,
		int32 StackCount);
	void TakeItemInstance(
		URpgBaseStorageStationComponent* Station,
		URpgInventoryItemInstance* Item,
		int32 StackCount);
	void InstallUpgrade(
		URpgBaseStorageStationComponent* Station,
		URpgBaseStorageUpgradeDefinition* UpgradeDefinition);
	void ApplyResourceSort(
		URpgBaseStorageStationComponent* Station,
		ERpgInventorySortMode SortMode);
	void MoveResourceEntry(
		URpgBaseStorageStationComponent* Station,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 TargetIndex);

private:
	bool TryDepositMaterialStack(
		URpgInventoryManagerComponent* Inventory,
		URpgBaseStorageComponent* BaseStorage,
		FRpgInventoryItemId ItemId,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 AvailableCount,
		int32 CountToStore) const;
};

/** Server-side base placement and construction contribution handler. */
class FRpgBaseBuildingActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	using FRpgInventoryUiActionDomainHandler::
		FRpgInventoryUiActionDomainHandler;

	void PlaceBuildable(
		ARpgBaseCampActor* BaseCamp,
		URpgBaseBuildableDefinition* BuildableDefinition,
		FTransform BuildTransform,
		bool bAutoContributeFromBase);
	void ContributeAll(
		ARpgBaseConstructionSiteActor* ConstructionSite,
		bool bAllowBaseStorage);
	void ContributeMaterial(
		ARpgBaseConstructionSiteActor* ConstructionSite,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 StackCount,
		bool bAllowBaseStorage);
};

/** Server-side crafting queue and station-state command handler. */
class FRpgCraftingActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	using FRpgInventoryUiActionDomainHandler::
		FRpgInventoryUiActionDomainHandler;

	void CraftRecipe(
		URpgCraftingStationComponent* CraftingStation,
		URpgCraftingRecipeDefinition* RecipeDefinition,
		int32 Quantity);
	void CancelCraftJob(
		URpgCraftingStationComponent* CraftingStation,
		FGuid JobId);
	void PauseStation(URpgCraftingStationComponent* CraftingStation);
	void ResumeStation(URpgCraftingStationComponent* CraftingStation);
	void SetOutputAutoDepositEnabled(
		URpgCraftingStationComponent* CraftingStation,
		bool bEnabled);
};
