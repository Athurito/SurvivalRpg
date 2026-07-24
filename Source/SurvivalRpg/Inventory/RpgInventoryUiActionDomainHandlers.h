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
		, MutableActionComponent(&InActionComponent)
	{
	}

	/** Read-only construction path for planner/query methods on the const facade. */
	explicit FRpgInventoryUiActionDomainHandler(
		const URpgInventoryUiActionComponent& InActionComponent)
		: ActionComponent(InActionComponent)
	{
	}

	/** Evaluates the shared server-side authorization policy for one inventory. */
	bool EvaluateInventoryAccess(
		URpgInventoryManagerComponent* Inventory) const;

protected:
	AActor* GetOwner() const;
	UWorld* GetWorld() const;
	AActor* GetRequestingActor() const;
	const URpgInventoryUiActionComponent&
		GetReadOnlyActionComponent() const
	{
		return ActionComponent;
	}

	bool CanAccessInventory(
		URpgInventoryManagerComponent* Inventory) const;
	bool IsUiTransferDirectionAllowed(
		const URpgInventoryManagerComponent* SourceInventory,
		const URpgInventoryManagerComponent* TargetInventory) const;
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

	bool TryReplayRecentExactTransferResult(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent);
	void SendAndCacheExactTransferFeedback(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount);

	bool TryReplayRecentQuickTransferResult(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request);
	void SendAndCacheQuickTransferFeedback(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount);

	bool TryReplayRecentEquipmentIntentResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent);
	void SendAndCacheEquipmentIntentFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount);

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
	URpgInventoryUiActionComponent& GetMutableActionComponent() const
	{
		// Command bridges are invalid for a handler created by a const query.
		check(MutableActionComponent);
		return *MutableActionComponent;
	}

	const URpgInventoryUiActionComponent& ActionComponent;
	URpgInventoryUiActionComponent* MutableActionComponent = nullptr;
};

/** Read-only inventory transfer, split, and placement-planning policy. */
class FRpgInventoryTransactionQueryHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	explicit FRpgInventoryTransactionQueryHandler(
		const URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

	FRpgInventoryPlacementPlan PlanExactTransferPlacement(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent) const;
	FRpgInventoryPlacementPlan PlanQuickTransferDestination(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		FRpgInventoryContainerHandle& OutTargetContainer,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;
	bool FindQuickTransferDestination(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		FRpgInventoryContainerHandle& OutTargetContainer,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;
	bool CanTransferItemStack(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount) const;
	bool CanTransferItemStackToPlacement(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		FRpgInventoryGridPlacement TargetPlacement) const;
	bool CanSplitItemStack(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 SplitCount,
		FRpgInventoryGridPlacement TargetPlacement,
		int32& OutSplitCount,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;

private:
	FRpgInventoryPlacementPlan PlanQuickTransferInContainer(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FRpgInventoryContainerHandle& TargetContainer,
		FRpgInventoryGridPlacement& OutPlacement) const;
	void BuildDefaultQuickTransferTargets(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		TArray<FRpgInventoryContainerHandle>& OutTargets) const;
	bool FindFirstEmptyInventoryPlacement(
		URpgInventoryManagerComponent* Inventory,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		FRpgInventoryGridPlacement& OutPlacement) const;
};

/** Server-side inventory mutation, transfer, and split command handler. */
class FRpgInventoryTransactionActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	explicit FRpgInventoryTransactionActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

	void InventoryMutation(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryMutationRequest Request);
	void MoveInventoryItem(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryMoveIntent Intent);
	void TransferInventoryItem(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryTransferIntent Intent);
	void QuickTransferItem(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryQuickTransferRequest Request);
	void TransferItemStack(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount);
	void TransferItemStackToPlacement(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		FRpgInventoryGridPlacement TargetPlacement);
	void ApplyInventorySort(
		URpgInventoryManagerComponent* Inventory,
		ERpgInventorySortMode SortMode);
	void MoveInventoryEntry(
		URpgInventoryManagerComponent* Inventory,
		FGuid EntryId,
		int32 TargetIndex);
	void MoveInventoryEntryToPlacement(
		URpgInventoryManagerComponent* Inventory,
		FGuid EntryId,
		FRpgInventoryGridPlacement TargetPlacement);
	void SplitItemStack(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 SplitCount,
		FRpgInventoryGridPlacement TargetPlacement);
	void SplitItemStackById(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryItemId ItemId,
		int32 SplitCount,
		FRpgInventoryGridPlacement TargetPlacement,
		FGuid RequestId);
};

/** Read-only physical equipment placement and content-routing policy. */
class FRpgInventoryEquipmentQueryHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	explicit FRpgInventoryEquipmentQueryHandler(
		const URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

	FRpgInventoryPlacementPlan PlanEquipmentIntentPlacement(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;
	bool CanMoveItemToFirstCompatibleContentSlot(
		URpgInventoryItemInstance* Item,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;
	bool CanMoveItemOutOfGearSlot(
		const FRpgInventorySlotAddress& SourceAddress) const;
};

/** Server-side physical equipment movement and hand-selection command handler. */
class FRpgInventoryEquipmentActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	explicit FRpgInventoryEquipmentActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

	void ApplyInventoryEquipmentIntent(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryEquipmentIntent Intent);
	void AssignItemToEquipmentSlot(
		ERpgEquipmentSlot EquipmentSlot,
		URpgInventoryItemInstance* Item);
	void ClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);
	void MoveItemToInventorySlotAddress(
		URpgInventoryItemInstance* Item,
		FRpgInventorySlotAddress TargetAddress);
	void EquipSlotContainerItem(
		ERpgEquipmentSlot ContainerSlot,
		URpgInventoryItemInstance* Item);
	void UnequipSlotContainerItem(
		ERpgEquipmentSlot ContainerSlot,
		FRpgInventoryItemId ExpectedProviderItemId);
	void EquipInventoryItem(URpgInventoryItemInstance* Item);
	void UnequipInventoryItemToContentSlot(
		URpgInventoryItemInstance* Item);

private:
	bool TryBuildCurrentEquipmentIntent(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		ERpgInventoryEquipmentIntentOperation Operation,
		ERpgEquipmentSlot TargetEquipmentSlot,
		FRpgInventoryEquipmentIntent& OutIntent) const;
	bool TryAssignItemToDefaultEquipmentDestination(
		URpgInventoryItemInstance* Item);
	bool TryMoveAndActivateItemInCarry(
		URpgInventoryItemInstance* Item,
		ERpgEquipmentSlot PreferredHandSlot);
	bool TryMoveItemToGearSlot(
		ERpgEquipmentSlot EquipmentSlot,
		URpgInventoryItemInstance* Item);
	bool TryMoveItemToFirstCompatibleCarrySlot(
		URpgInventoryItemInstance* Item);
	bool TryMoveItemToFirstCompatibleContentSlot(
		URpgInventoryItemInstance* Item);
};

/** Server-side Quick Access binding and Carry activation handler. */
class FRpgInventoryQuickAccessActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	explicit FRpgInventoryQuickAccessActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

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
	explicit FRpgInventoryItemUseActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

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
	explicit FRpgInventoryManualDropActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

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
	explicit FRpgBaseStorageActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

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
	explicit FRpgBaseBuildingActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

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
	explicit FRpgCraftingActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

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
