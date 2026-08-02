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
	void SendBaseStorageCommandFeedback(
		FGameplayTag ActionTag,
		const FRpgBaseStorageCommandResult& Result,
		FRpgInventoryItemId ItemId = FRpgInventoryItemId(),
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = nullptr) const
	{
		GetMutableActionComponent().SendBaseStorageCommandFeedback(
			ActionTag,
			Result,
			ItemId,
			ItemDefinition);
	}

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

	bool TryReplayRecentSplitResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventorySplitRequest& Request);
	void SendAndCacheSplitFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventorySplitRequest& Request,
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
	void SplitItemStackById(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventorySplitRequest Request);
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
private:
	bool TryAssignItemToDefaultEquipmentDestination(
		URpgInventoryItemInstance* Item);
	bool TryMoveAndActivateItemInCarry(
		URpgInventoryItemInstance* Item,
		ERpgEquipmentSlot PreferredHandSlot,
		const FRpgInventoryGridPlacement& ExactTargetPlacement =
			FRpgInventoryGridPlacement());
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
};

/** Synchronous outcome returned before the owning facade finalizes replay state and emits feedback. */
struct FRpgInventoryItemUseExecutionResult
{
	ERpgInventoryActionFeedbackResult Result =
		ERpgInventoryActionFeedbackResult::InvalidRequest;
	URpgInventoryItemInstance* Item = nullptr;
	int32 FeedbackUseCount = 0;
};

/** Server-side stable-ID resolution, item-use capability, GAS activation, and consume handler. */
class FRpgInventoryItemUseActionHandler final
	: public FRpgInventoryUiActionDomainHandler
{
public:
	explicit FRpgInventoryItemUseActionHandler(
		URpgInventoryUiActionComponent& InActionComponent)
		: FRpgInventoryUiActionDomainHandler(InActionComponent)
	{
	}

	FRpgInventoryItemUseExecutionResult ExecuteUseRequest(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryUseRequest& Request);
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
	void SmartDeposit(FRpgBaseStorageSmartDepositRequest Request);
	void DepositExact(FRpgBaseStorageDepositRequest Request);
	void WithdrawExact(FRpgBaseStorageWithdrawRequest Request);
	void InstallUpgradeById(FRpgBaseStorageUpgradeRequest Request);
	void DecommissionUpgrade(FRpgBaseStorageUpgradeRequest Request);
	void StabilizeContainedItem(FRpgBaseStorageRiftItemRequest Request);
	void ExtractContainedItem(FRpgBaseStorageRiftItemRequest Request);
	void CleanseRiftStrain(FRpgBaseStorageCleanseRequest Request);
	void DepositMaterialStack(
		URpgBaseStorageStationComponent* Station,
		URpgInventoryItemInstance* Item,
		int32 StackCount);
	void WithdrawResource(
		URpgBaseStorageStationComponent* Station,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
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
	/** Typed result keeps a safely rolled-back rejection distinct from an unrecoverable graph/ledger rollback failure. */
	enum class EMaterialDepositResult : uint8
	{
		Success,
		Rejected,
		RolledBack,
		RollbackFailed
	};

	EMaterialDepositResult TryDepositMaterialStack(
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
