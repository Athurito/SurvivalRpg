#pragma once

#include "RpgInventoryUiActionComponent.h"

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

	void RequestQuickTransferItem(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryQuickTransferRequest Request);

private:
	URpgInventoryUiActionComponent& ActionComponent;
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
