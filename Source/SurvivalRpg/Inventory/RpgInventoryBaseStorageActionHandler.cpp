#include "RpgInventoryUiActionDomainHandlers.h"

#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(
		const URpgInventoryItemInstance* Item)
	{
		return Item
			? Item->FindFragmentByClass<
				URpgInventoryFragment_ItemTraits>()
			: nullptr;
	}

	const URpgInventoryFragment_ItemTraits*
		GetUiActionItemTraitsForDefinition(
			TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO =
			ItemDefinition
				? GetDefault<URpgInventoryItemDefinition>(
					ItemDefinition)
				: nullptr;
		return ItemCDO
			? Cast<URpgInventoryFragment_ItemTraits>(
				ItemCDO->FindFragmentByClass(
					URpgInventoryFragment_ItemTraits::
						StaticClass()))
			: nullptr;
	}

	bool IsMaterialItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits =
			GetItemTraits(Item);
		return Traits && Traits->IsMaterial();
	}

	bool IsMaterialItemDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_ItemTraits* Traits =
			GetUiActionItemTraitsForDefinition(ItemDefinition);
		return Traits && Traits->IsMaterial();
	}

	bool TryGetBaseStorageEntrySnapshot(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		FRpgInventoryEntryView& OutEntry)
	{
		OutEntry = FRpgInventoryEntryView();
		if (!Inventory || !ItemId.IsValid())
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry :
			Inventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId && Entry.Instance)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	bool HasEquivalentRuntimeState(
		const TArray<FRpgInventoryFragmentStatePayload>& A,
		const TArray<FRpgInventoryFragmentStatePayload>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].FragmentId != B[Index].FragmentId ||
				A[Index].Version != B[Index].Version ||
				A[Index].Payload != B[Index].Payload)
			{
				return false;
			}
		}
		return true;
	}

	int32 GetAvailableUpgradeCostCount(
		const URpgInventoryManagerComponent* PlayerInventory,
		const URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		int32 AvailableCount = 0;

		if (ConsumeOrder !=
				ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly &&
			PlayerInventory)
		{
			AvailableCount +=
				PlayerInventory->GetTotalItemCountByDefinition(
					ItemDefinition);
		}

		if (ConsumeOrder !=
				ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly &&
			BaseStorage)
		{
			AvailableCount +=
				BaseStorage->GetResourceCount(ItemDefinition);
		}

		return AvailableCount;
	}

	bool ConsumeUpgradeCostFromPlayer(
		URpgInventoryManagerComponent* PlayerInventory,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 CountToConsume)
	{
		return CountToConsume <= 0 ||
			(PlayerInventory &&
			 PlayerInventory->ConsumeItemsByDefinition(
				 ItemDefinition,
				 CountToConsume));
	}

	bool ConsumeUpgradeCostFromBase(
		URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 CountToConsume)
	{
		return CountToConsume <= 0 ||
			(BaseStorage &&
			 BaseStorage->WithdrawResource(
				 ItemDefinition,
				 CountToConsume));
	}

	bool ConsumeUpgradeCost(
		URpgInventoryManagerComponent* PlayerInventory,
		URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 Count,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		if (!ItemDefinition || Count <= 0)
		{
			return false;
		}

		int32 RemainingCount = Count;
		auto ConsumeFromBase = [&]()
		{
			const int32 AvailableInBase =
				BaseStorage
					? BaseStorage->GetResourceCount(
						ItemDefinition)
					: 0;
			const int32 CountToConsume =
				FMath::Min(AvailableInBase, RemainingCount);
			if (!ConsumeUpgradeCostFromBase(
					BaseStorage,
					ItemDefinition,
					CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		auto ConsumeFromPlayer = [&]()
		{
			const int32 AvailableInPlayer =
				PlayerInventory
					? PlayerInventory->
						GetTotalItemCountByDefinition(
							ItemDefinition)
					: 0;
			const int32 CountToConsume =
				FMath::Min(AvailableInPlayer, RemainingCount);
			if (!ConsumeUpgradeCostFromPlayer(
					PlayerInventory,
					ItemDefinition,
					CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		switch (ConsumeOrder)
		{
		case ERpgBaseStorageUpgradeCostConsumeOrder::
			BaseThenPlayer:
			return ConsumeFromBase() &&
				ConsumeFromPlayer() &&
				RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::
			PlayerThenBase:
			return ConsumeFromPlayer() &&
				ConsumeFromBase() &&
				RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly:
			return ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly:
			return ConsumeFromPlayer() && RemainingCount <= 0;
		}

		return false;
	}
}

bool FRpgBaseStorageActionHandler::TryDepositMaterialStack(
	URpgInventoryManagerComponent* Inventory,
	URpgBaseStorageComponent* BaseStorage,
	FRpgInventoryItemId ItemId,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 AvailableCount,
	int32 CountToStore) const
{
	if (!Inventory || !BaseStorage || !ItemId.IsValid() ||
		!ItemDefinition || AvailableCount <= 0 || CountToStore <= 0 ||
		CountToStore > AvailableCount)
	{
		return false;
	}

	const TArray<FRpgInventoryEntryView> EntriesBefore =
		Inventory->GetAllEntries();
	const FRpgInventoryEntryView* EntryBefore =
		EntriesBefore.FindByPredicate(
			[&ItemId](const FRpgInventoryEntryView& Candidate)
			{
				return Candidate.ItemId == ItemId;
			});
	const FRpgInventoryGraphSaveData GraphBefore =
		Inventory->ExportInventoryGraph();
	const FRpgInventorySavedItem* SavedItemBefore =
		GraphBefore.Items.FindByPredicate(
			[&ItemId](const FRpgInventorySavedItem& Candidate)
			{
				return Candidate.ItemId == ItemId;
			});
	if (!EntryBefore || !EntryBefore->Instance ||
		EntryBefore->Instance->GetItemDef() != ItemDefinition ||
		EntryBefore->StackCount != AvailableCount ||
		GraphBefore.Items.Num() != EntriesBefore.Num() ||
		!SavedItemBefore)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Error,
			TEXT("Material deposit skipped for %s because its exact pre-consume graph could not be exported."),
			*ItemId.ToString());
		return false;
	}
	if (!BaseStorage->CanStoreResourceInstance(
			EntryBefore->Instance,
			CountToStore))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Verbose,
			TEXT("Material deposit kept runtime-state item %s in concrete inventory because BaseStorage only represents stateless definition/count credits."),
			*ItemId.ToString());
		return false;
	}

	const FRpgInventorySavedItem SavedItemSnapshot =
		*SavedItemBefore;
	URpgInventoryItemInstance* const InstanceBefore =
		EntryBefore->Instance;
	const FGuid EntryIdBefore = EntryBefore->EntryId;
	const FRpgInventoryGridPlacement PlacementBefore =
		EntryBefore->Placement;
	auto RestoreExactInventoryGraph = [&]()
	{
		FRpgInventoryMutationResult RollbackResult;
		if (!Inventory->RestoreRuntimeCheckpoint(
				GraphBefore,
				RollbackResult) ||
			!RollbackResult.IsSuccess())
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Material-deposit exact graph rollback failed for %s after storing %d x %s was rejected (code %d)."),
				*ItemId.ToString(),
				CountToStore,
				*GetNameSafe(ItemDefinition),
				static_cast<int32>(RollbackResult.Code));
			return false;
		}

		const TArray<FRpgInventoryEntryView> RestoredEntries =
			Inventory->GetAllEntries();
		const FRpgInventoryEntryView* RestoredEntry =
			RestoredEntries.FindByPredicate(
				[&ItemId](
					const FRpgInventoryEntryView& Candidate)
				{
					return Candidate.ItemId == ItemId;
				});
		const FRpgInventoryGraphSaveData RestoredGraph =
			Inventory->ExportInventoryGraph();
		const FRpgInventorySavedItem* RestoredSavedItem =
			RestoredGraph.Items.FindByPredicate(
				[&ItemId](
					const FRpgInventorySavedItem& Candidate)
				{
					return Candidate.ItemId == ItemId;
				});
		const bool bExactRollback =
			RestoredEntry &&
			RestoredEntry->Instance == InstanceBefore &&
			RestoredEntry->EntryId == EntryIdBefore &&
			RestoredEntry->StackCount == AvailableCount &&
			RestoredEntry->Placement == PlacementBefore &&
			RestoredSavedItem &&
			RestoredSavedItem->ItemDefinition ==
				SavedItemSnapshot.ItemDefinition &&
			RestoredSavedItem->Container ==
				SavedItemSnapshot.Container &&
			RestoredSavedItem->Placement ==
				SavedItemSnapshot.Placement &&
			HasEquivalentRuntimeState(
				RestoredSavedItem->RuntimeState,
				SavedItemSnapshot.RuntimeState);
		if (!bExactRollback)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Material-deposit rollback for %s completed with item identity, entry identity, placement, or runtime-state drift."),
				*ItemId.ToString());
		}
		return bExactRollback;
	};

	// Keep one concrete unit alive for a whole-stack deposit so a failed
	// storage write can restore the same UObject and replicated EntryId.
	const int32 CountToConsumeBeforeStore =
		CountToStore == AvailableCount
			? CountToStore - 1
			: CountToStore;
	if (CountToConsumeBeforeStore > 0)
	{
		const FRpgInventoryMutationResult ConsumeResult =
			Inventory->ConsumeItemById(
				ItemId,
				CountToConsumeBeforeStore);
		if (!ConsumeResult.IsSuccess() ||
			ConsumeResult.AppliedQuantity !=
				CountToConsumeBeforeStore)
		{
			return false;
		}
	}

	if (!BaseStorage->StoreResourceInstance(
			InstanceBefore,
			CountToStore))
	{
		RestoreExactInventoryGraph();
		return false;
	}

	const int32 RemainingConsumeCount =
		CountToStore - CountToConsumeBeforeStore;
	if (RemainingConsumeCount <= 0)
	{
		return true;
	}

	const FRpgInventoryMutationResult FinalConsumeResult =
		Inventory->ConsumeItemById(
			ItemId,
			RemainingConsumeCount);
	if (FinalConsumeResult.IsSuccess() &&
		FinalConsumeResult.AppliedQuantity ==
			RemainingConsumeCount)
	{
		return true;
	}

	const bool bStorageRolledBack =
		BaseStorage->WithdrawResource(
			ItemDefinition,
			CountToStore);
	const bool bInventoryRolledBack =
		RestoreExactInventoryGraph();
	if (!bStorageRolledBack || !bInventoryRolledBack)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Error,
			TEXT("Material-deposit finalization rollback failed for %s (%d x %s, Storage=%s, Inventory=%s)."),
			*ItemId.ToString(),
			CountToStore,
			*GetNameSafe(ItemDefinition),
			bStorageRolledBack
				? TEXT("restored")
				: TEXT("failed"),
			bInventoryRolledBack
				? TEXT("restored")
				: TEXT("failed"));
	}
	return false;
}

void FRpgBaseStorageActionHandler::DepositAllMaterials(
	URpgBaseStorageStationComponent* Station)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!PlayerInventory ||
		!BaseStorage)
	{
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries =
		PlayerInventory->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		URpgInventoryItemInstance* Item = Entry.Instance;
		if (!Item || Entry.StackCount <= 0 ||
			!IsMaterialItem(Item))
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition>
			ItemDefinition = Item->GetItemDef();
		if (!Station->AllowsResourceDefinition(ItemDefinition))
		{
			continue;
		}

		const int32 CountToDeposit = FMath::Min(
			Entry.StackCount,
			BaseStorage->GetFreeResourceCapacity(
				ItemDefinition));
		if (CountToDeposit <= 0 ||
			!BaseStorage->CanStoreResource(
				ItemDefinition,
				CountToDeposit))
		{
			continue;
		}

		TryDepositMaterialStack(
			PlayerInventory,
			BaseStorage,
			Entry.ItemId,
			ItemDefinition,
			Entry.StackCount,
			CountToDeposit);
	}
}

void FRpgBaseStorageActionHandler::DepositMaterialStack(
	URpgBaseStorageStationComponent* Station,
	URpgInventoryItemInstance* Item,
	int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!PlayerInventory ||
		!BaseStorage ||
		!Item ||
		!IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount =
		PlayerInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return;
	}

	const int32 RequestedCount =
		StackCount <= 0 ? AvailableCount : StackCount;
	const int32 TransferCount =
		FMath::Min(AvailableCount, RequestedCount);
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Item->GetItemDef();
	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (TransferCount <= 0 ||
		!BaseStorage->CanStoreResource(
			ItemDefinition,
			TransferCount))
	{
		return;
	}

	TryDepositMaterialStack(
		PlayerInventory,
		BaseStorage,
		Item->GetItemId(),
		ItemDefinition,
		AvailableCount,
		TransferCount);
}

void FRpgBaseStorageActionHandler::WithdrawResource(
	URpgBaseStorageStationComponent* Station,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!PlayerInventory ||
		!BaseStorage ||
		!ItemDefinition ||
		StackCount <= 0)
	{
		return;
	}

	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (BaseStorage->GetResourceCount(ItemDefinition) <
			StackCount ||
		!PlayerInventory->CanAddItemDefinition(
			ItemDefinition,
			StackCount))
	{
		return;
	}

	if (BaseStorage->WithdrawResource(
			ItemDefinition,
			StackCount))
	{
		if (!PlayerInventory->GrantItemDefinition(
				ItemDefinition,
				StackCount) &&
			!BaseStorage->StoreDefinitionResource(
				ItemDefinition,
				StackCount))
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Resource-withdraw rollback failed after the player inventory rejected %d x %s."),
				StackCount,
				*GetNameSafe(ItemDefinition));
		}
	}
}

void FRpgBaseStorageActionHandler::StoreItemInstance(
	URpgBaseStorageStationComponent* Station,
	URpgInventoryItemInstance* Item,
	int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory =
		Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		Station->GetStationMode() !=
			ERpgBaseStorageStationMode::Terminal ||
		!PlayerInventory ||
		!ArmoryInventory ||
		!Item ||
		IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount =
		PlayerInventory->GetItemStackCount(Item);
	const int32 RequestedCount =
		StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 ||
		RequestedCount != AvailableCount)
	{
		return;
	}
	FRpgInventoryEntryView SourceEntry;
	if (!TryGetBaseStorageEntrySnapshot(
			PlayerInventory,
			Item->GetItemId(),
			SourceEntry))
	{
		return;
	}

	FRpgInventoryQuickTransferRequest TransferRequest;
	TransferRequest.RequestId = FGuid::NewGuid();
	TransferRequest.ItemId = Item->GetItemId();
	TransferRequest.ExpectedEntryId = SourceEntry.EntryId;
	TransferRequest.ExpectedSourcePlacement =
		SourceEntry.Placement;
	TransferRequest.ExpectedSourceQuantity =
		SourceEntry.StackCount;
	TransferRequest.StackCount = AvailableCount;
	RequestQuickTransferItem(
		PlayerInventory,
		ArmoryInventory,
		MoveTemp(TransferRequest));
}

void FRpgBaseStorageActionHandler::TakeItemInstance(
	URpgBaseStorageStationComponent* Station,
	URpgInventoryItemInstance* Item,
	int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory =
		Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		Station->GetStationMode() !=
			ERpgBaseStorageStationMode::Terminal ||
		!PlayerInventory ||
		!ArmoryInventory ||
		!Item)
	{
		return;
	}

	const int32 AvailableCount =
		ArmoryInventory->GetItemStackCount(Item);
	const int32 RequestedCount =
		StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 ||
		RequestedCount != AvailableCount)
	{
		return;
	}
	FRpgInventoryEntryView SourceEntry;
	if (!TryGetBaseStorageEntrySnapshot(
			ArmoryInventory,
			Item->GetItemId(),
			SourceEntry))
	{
		return;
	}

	FRpgInventoryQuickTransferRequest TransferRequest;
	TransferRequest.RequestId = FGuid::NewGuid();
	TransferRequest.ItemId = Item->GetItemId();
	TransferRequest.ExpectedEntryId = SourceEntry.EntryId;
	TransferRequest.ExpectedSourcePlacement =
		SourceEntry.Placement;
	TransferRequest.ExpectedSourceQuantity =
		SourceEntry.StackCount;
	TransferRequest.StackCount = AvailableCount;
	RequestQuickTransferItem(
		ArmoryInventory,
		PlayerInventory,
		MoveTemp(TransferRequest));
}

void FRpgBaseStorageActionHandler::InstallUpgrade(
	URpgBaseStorageStationComponent* Station,
	URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!Station)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: Station is null. Owner=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!CanAccessBaseStorageStation(Station))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: station access denied. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!PlayerInventory)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: player inventory missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!BaseStorage)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: base storage missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!UpgradeDefinition)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: upgrade definition is null. Owner=%s Station=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station));
		return;
	}

	if (!Station->CanInstallUpgrade(UpgradeDefinition))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: station cannot install upgrade, maybe already installed or station tags do not match. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	const ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder =
		Station->GetUpgradeCostConsumeOrder();
	UE_LOG(
		LogRpgInventoryUiActions,
		Log,
		TEXT("Install base storage upgrade requested: Owner=%s Station=%s Upgrade=%s CostCount=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		UpgradeDefinition->Costs.Num());

	for (const FRpgBaseStorageUpgradeCost& Cost :
		UpgradeDefinition->Costs)
	{
		if (!Cost.ItemDefinition)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: empty cost item definition. Upgrade=%s"),
				*GetNameSafe(UpgradeDefinition));
			return;
		}

		if (Cost.Count <= 0)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: invalid cost count. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}

		if (!IsMaterialItemDefinition(Cost.ItemDefinition))
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: cost item is not marked as material. Upgrade=%s ItemDef=%s"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition));
			return;
		}

		const int32 AvailableCount =
			GetAvailableUpgradeCostCount(
				PlayerInventory,
				BaseStorage,
				Cost.ItemDefinition,
				ConsumeOrder);
		if (AvailableCount < Cost.Count)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: not enough resources. Upgrade=%s ItemDef=%s Available=%d Required=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				AvailableCount,
				Cost.Count);
			return;
		}
	}

	for (const FRpgBaseStorageUpgradeCost& Cost :
		UpgradeDefinition->Costs)
	{
		if (!ConsumeUpgradeCost(
				PlayerInventory,
				BaseStorage,
				Cost.ItemDefinition,
				Cost.Count,
				ConsumeOrder))
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: cost consume failed after validation. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}
	}

	const bool bInstalled =
		Station->InstallUpgrade(UpgradeDefinition);
	UE_LOG(
		LogRpgInventoryUiActions,
		Log,
		TEXT("Install base storage upgrade result: Owner=%s Station=%s Upgrade=%s Installed=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		bInstalled ? TEXT("true") : TEXT("false"));
}

void FRpgBaseStorageActionHandler::ApplyResourceSort(
	URpgBaseStorageStationComponent* Station,
	ERpgInventorySortMode SortMode)
{
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage)
	{
		return;
	}

	BaseStorage->ApplyResourceSort(SortMode);
}

void FRpgBaseStorageActionHandler::MoveResourceEntry(
	URpgBaseStorageStationComponent* Station,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 TargetIndex)
{
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!BaseStorage ||
		!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	BaseStorage->MoveResourceEntry(ItemDefinition, TargetIndex);
}
