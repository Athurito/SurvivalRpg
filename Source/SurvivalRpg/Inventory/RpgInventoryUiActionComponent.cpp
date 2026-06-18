#include "RpgInventoryUiActionComponent.h"

#include "GameFramework/Pawn.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseConstructionSiteActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiActionComponent)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryUiActions, Log, All);

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(const URpgInventoryItemInstance* Item)
	{
		return Item ? Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
	}

	const URpgInventoryFragment_ItemTraits* GetUiActionItemTraitsForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	bool IsMaterialItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits && Traits->IsMaterial();
	}

	bool IsMaterialItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetUiActionItemTraitsForDefinition(ItemDefinition);
		return Traits && Traits->IsMaterial();
	}

	bool IsStackableItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits && Traits->GetMaxStackSize() > 1;
	}

	bool CanTargetAcceptTransferredStack(URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 TransferCount, bool bTransfersWholeEntry)
	{
		if (!TargetInventory || !Item || TransferCount <= 0)
		{
			return false;
		}

		if (IsStackableItem(Item))
		{
			return TargetInventory->CanAddItemDefinition(Item->GetItemDef(), TransferCount);
		}

		return bTransfersWholeEntry && TargetInventory->CanAddItemInstance(Item, TransferCount);
	}

	int32 GetAvailableUpgradeCostCount(
		const URpgInventoryManagerComponent* PlayerInventory,
		const URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		int32 AvailableCount = 0;

		if (ConsumeOrder != ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly && PlayerInventory)
		{
			AvailableCount += PlayerInventory->GetTotalItemCountByDefinition(ItemDefinition);
		}

		if (ConsumeOrder != ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly && BaseStorage)
		{
			AvailableCount += BaseStorage->GetResourceCount(ItemDefinition);
		}

		return AvailableCount;
	}

	bool ConsumeUpgradeCostFromPlayer(URpgInventoryManagerComponent* PlayerInventory, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 CountToConsume)
	{
		return CountToConsume <= 0 || (PlayerInventory && PlayerInventory->ConsumeItemsByDefinition(ItemDefinition, CountToConsume));
	}

	bool ConsumeUpgradeCostFromBase(URpgBaseStorageComponent* BaseStorage, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 CountToConsume)
	{
		return CountToConsume <= 0 || (BaseStorage && BaseStorage->WithdrawResource(ItemDefinition, CountToConsume));
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
			const int32 AvailableInBase = BaseStorage ? BaseStorage->GetResourceCount(ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInBase, RemainingCount);
			if (!ConsumeUpgradeCostFromBase(BaseStorage, ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		auto ConsumeFromPlayer = [&]()
		{
			const int32 AvailableInPlayer = PlayerInventory ? PlayerInventory->GetTotalItemCountByDefinition(ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInPlayer, RemainingCount);
			if (!ConsumeUpgradeCostFromPlayer(PlayerInventory, ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		switch (ConsumeOrder)
		{
		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseThenPlayer:
			return ConsumeFromBase() && ConsumeFromPlayer() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerThenBase:
			return ConsumeFromPlayer() && ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly:
			return ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly:
			return ConsumeFromPlayer() && RemainingCount <= 0;
		}

		return false;
	}
}

URpgInventoryUiActionComponent::URpgInventoryUiActionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgInventoryUiActionComponent::RequestAssignItemToQuickBar_Implementation(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (URpgQuickBarComponent* QuickBar = FindQuickBar())
	{
		QuickBar->AssignItemToLoadoutSlot(QuickBarSlotIndex, EquipmentSlot, Item);
	}
}

void URpgInventoryUiActionComponent::RequestSwapQuickBarSlots_Implementation(int32 SourceSlotIndex, ERpgEquipmentSlot SourceEquipmentSlot, int32 TargetSlotIndex, ERpgEquipmentSlot TargetEquipmentSlot)
{
	if (URpgQuickBarComponent* QuickBar = FindQuickBar())
	{
		QuickBar->SwapLoadoutSlots(SourceSlotIndex, SourceEquipmentSlot, TargetSlotIndex, TargetEquipmentSlot);
	}
}

void URpgInventoryUiActionComponent::RequestClearQuickBarSlot_Implementation(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot)
{
	if (URpgQuickBarComponent* QuickBar = FindQuickBar())
	{
		QuickBar->RemoveItemFromLoadoutSlot(QuickBarSlotIndex, EquipmentSlot);
	}
}

void URpgInventoryUiActionComponent::RequestAssignItemToEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->AssignItemToEquipmentSlot(EquipmentSlot, Item);
	}
}

void URpgInventoryUiActionComponent::RequestClearEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot)
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->ClearEquipmentSlot(EquipmentSlot);
	}
}

void URpgInventoryUiActionComponent::RequestTransferItemStack_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount)
{
	if (!CanTransferItemStack(SourceInventory, TargetInventory, Item, StackCount))
	{
		return;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	const int32 TransferCount = FMath::Min(AvailableCount, RequestedCount);
	if (TransferCount <= 0)
	{
		return;
	}

	const bool bTransfersWholeEntry = TransferCount >= AvailableCount;
	const bool bTransferAsStackableDefinition = IsStackableItem(Item);

	if (TransferCount >= AvailableCount)
	{
		if (SourceInventory == FindPlayerInventory())
		{
			ClearPlayerAssignmentsForItem(Item);
		}

		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
		SourceInventory->RemoveItemInstance(Item);
		if (bTransferAsStackableDefinition)
		{
			TargetInventory->AddItemDefinition(ItemDefinition, AvailableCount);
		}
		else
		{
			TargetInventory->AddItemInstanceWithStack(Item, AvailableCount);
		}
		return;
	}

	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	if (SourceInventory->RemoveItemInstanceStack(Item, TransferCount))
	{
		TargetInventory->AddItemDefinition(ItemDefinition, TransferCount);
	}
}

void URpgInventoryUiActionComponent::RequestTransferItemStackToInventorySlot_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex)
{
	if (!CanTransferItemStackToInventorySlot(SourceInventory, TargetInventory, Item, StackCount, TargetSlotIndex))
	{
		return;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	URpgInventoryItemInstance* TargetItem = TargetInventory->GetItemInSlot(TargetSlotIndex);

	if (TargetItem && TargetItem->GetItemDef() == Item->GetItemDef())
	{
		const int32 FreeStackCapacity = TargetInventory->GetFreeStackCapacity(TargetItem);
		if (FreeStackCapacity > 0)
		{
			const int32 TransferCount = FMath::Min3(AvailableCount, RequestedCount, FreeStackCapacity);
			if (TransferCount <= 0)
			{
				return;
			}

			if (SourceInventory == FindPlayerInventory() && TransferCount >= AvailableCount)
			{
				ClearPlayerAssignmentsForItem(Item);
			}

			if (SourceInventory->RemoveItemInstanceStack(Item, TransferCount))
			{
				TargetInventory->AddStackToExistingItem(TargetItem, TransferCount);
			}
			return;
		}
	}

	if (!TargetItem)
	{
		const int32 TransferCount = FMath::Min(AvailableCount, RequestedCount);
		if (TransferCount <= 0)
		{
			return;
		}

		if (TransferCount >= AvailableCount)
		{
			if (SourceInventory == FindPlayerInventory())
			{
				ClearPlayerAssignmentsForItem(Item);
			}

			SourceInventory->RemoveItemInstance(Item);
			TargetInventory->AddItemInstanceWithStackToSlot(Item, AvailableCount, TargetSlotIndex);
			return;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
		if (SourceInventory->RemoveItemInstanceStack(Item, TransferCount))
		{
			TargetInventory->AddItemDefinitionToSlot(ItemDefinition, TransferCount, TargetSlotIndex);
		}
		return;
	}

	if (RequestedCount < AvailableCount)
	{
		return;
	}

	const int32 SourceSlotIndex = SourceInventory->GetItemSlotIndex(Item);
	const int32 TargetStackCount = TargetInventory->GetItemStackCount(TargetItem);
	if (SourceSlotIndex == INDEX_NONE || TargetStackCount <= 0)
	{
		return;
	}

	if (SourceInventory == FindPlayerInventory())
	{
		ClearPlayerAssignmentsForItem(Item);
	}

	if (TargetInventory == FindPlayerInventory())
	{
		ClearPlayerAssignmentsForItem(TargetItem);
	}

	SourceInventory->RemoveItemInstance(Item);
	TargetInventory->RemoveItemInstance(TargetItem);
	SourceInventory->AddItemInstanceWithStackToSlot(TargetItem, TargetStackCount, SourceSlotIndex);
	TargetInventory->AddItemInstanceWithStackToSlot(Item, AvailableCount, TargetSlotIndex);
}

void URpgInventoryUiActionComponent::RequestApplyInventorySort_Implementation(URpgInventoryManagerComponent* Inventory, ERpgInventorySortMode SortMode)
{
	if (!CanAccessInventory(Inventory))
	{
		return;
	}

	Inventory->ApplyInventorySort(SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntry_Implementation(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetIndex)
{
	if (!CanAccessInventory(Inventory) || !Inventory->ContainsEntry(EntryId))
	{
		return;
	}

	Inventory->MoveInventoryEntry(EntryId, TargetIndex);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntryToSlot_Implementation(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetSlotIndex)
{
	if (!CanAccessInventory(Inventory) || !Inventory->ContainsEntry(EntryId))
	{
		return;
	}

	Inventory->MoveInventoryEntryToSlot(EntryId, TargetSlotIndex);
}

void URpgInventoryUiActionComponent::RequestDepositAllMaterialsToBase_Implementation(URpgBaseStorageStationComponent* Station)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage)
	{
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries = PlayerInventory->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		URpgInventoryItemInstance* Item = Entry.Instance;
		if (!Item || Entry.StackCount <= 0 || !IsMaterialItem(Item))
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
		if (!Station->AllowsResourceDefinition(ItemDefinition))
		{
			continue;
		}

		const int32 CountToDeposit = FMath::Min(Entry.StackCount, BaseStorage->GetFreeResourceCapacity(ItemDefinition));
		if (CountToDeposit <= 0 || !BaseStorage->CanStoreResource(ItemDefinition, CountToDeposit))
		{
			continue;
		}

		if (PlayerInventory->RemoveItemInstanceStack(Item, CountToDeposit))
		{
			BaseStorage->StoreResource(ItemDefinition, CountToDeposit);
		}
	}
}

void URpgInventoryUiActionComponent::RequestDepositMaterialStackToBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage || !Item || !IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount = PlayerInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	const int32 TransferCount = FMath::Min(AvailableCount, RequestedCount);
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (TransferCount <= 0 || !BaseStorage->CanStoreResource(ItemDefinition, TransferCount))
	{
		return;
	}

	if (PlayerInventory->RemoveItemInstanceStack(Item, TransferCount))
	{
		BaseStorage->StoreResource(ItemDefinition, TransferCount);
	}
}

void URpgInventoryUiActionComponent::RequestWithdrawResourceFromBase_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage || !ItemDefinition || StackCount <= 0)
	{
		return;
	}

	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (BaseStorage->GetResourceCount(ItemDefinition) < StackCount || !PlayerInventory->CanAddItemDefinition(ItemDefinition, StackCount))
	{
		return;
	}

	if (BaseStorage->WithdrawResource(ItemDefinition, StackCount))
	{
		PlayerInventory->AddItemDefinition(ItemDefinition, StackCount);
	}
}

void URpgInventoryUiActionComponent::RequestStoreItemInstanceInBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory = Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || Station->GetStationMode() != ERpgBaseStorageStationMode::Terminal || !PlayerInventory || !ArmoryInventory || !Item || IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount = PlayerInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 || RequestedCount != AvailableCount || !ArmoryInventory->CanAddItemInstance(Item, AvailableCount))
	{
		return;
	}

	ClearPlayerAssignmentsForItem(Item);
	PlayerInventory->RemoveItemInstance(Item);
	ArmoryInventory->AddItemInstanceWithStack(Item, AvailableCount);
}

void URpgInventoryUiActionComponent::RequestTakeItemInstanceFromBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory = Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || Station->GetStationMode() != ERpgBaseStorageStationMode::Terminal || !PlayerInventory || !ArmoryInventory || !Item)
	{
		return;
	}

	const int32 AvailableCount = ArmoryInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 || RequestedCount != AvailableCount || !PlayerInventory->CanAddItemInstance(Item, AvailableCount))
	{
		return;
	}

	ArmoryInventory->RemoveItemInstance(Item);
	PlayerInventory->AddItemInstanceWithStack(Item, AvailableCount);
}

void URpgInventoryUiActionComponent::RequestInstallBaseStorageUpgrade_Implementation(URpgBaseStorageStationComponent* Station, URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!Station)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: Station is null. Owner=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!CanAccessBaseStorageStation(Station))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: station access denied. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!PlayerInventory)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: player inventory missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!BaseStorage)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: base storage missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!UpgradeDefinition)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: upgrade definition is null. Owner=%s Station=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station));
		return;
	}

	if (!Station->CanInstallUpgrade(UpgradeDefinition))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: station cannot install upgrade, maybe already installed or station tags do not match. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	const ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder = Station->GetUpgradeCostConsumeOrder();
	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Install base storage upgrade requested: Owner=%s Station=%s Upgrade=%s CostCount=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		UpgradeDefinition->Costs.Num());

	for (const FRpgBaseStorageUpgradeCost& Cost : UpgradeDefinition->Costs)
	{
		if (!Cost.ItemDefinition)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: empty cost item definition. Upgrade=%s"),
				*GetNameSafe(UpgradeDefinition));
			return;
		}

		if (Cost.Count <= 0)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: invalid cost count. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}

		if (!IsMaterialItemDefinition(Cost.ItemDefinition))
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: cost item is not marked as material. Upgrade=%s ItemDef=%s"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition));
			return;
		}

		const int32 AvailableCount = GetAvailableUpgradeCostCount(PlayerInventory, BaseStorage, Cost.ItemDefinition, ConsumeOrder);
		if (AvailableCount < Cost.Count)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: not enough resources. Upgrade=%s ItemDef=%s Available=%d Required=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				AvailableCount,
				Cost.Count);
			return;
		}
	}

	for (const FRpgBaseStorageUpgradeCost& Cost : UpgradeDefinition->Costs)
	{
		if (!ConsumeUpgradeCost(PlayerInventory, BaseStorage, Cost.ItemDefinition, Cost.Count, ConsumeOrder))
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: cost consume failed after validation. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}
	}

	const bool bInstalled = Station->InstallUpgrade(UpgradeDefinition);
	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Install base storage upgrade result: Owner=%s Station=%s Upgrade=%s Installed=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		bInstalled ? TEXT("true") : TEXT("false"));
}

void URpgInventoryUiActionComponent::RequestApplyBaseResourceSort_Implementation(URpgBaseStorageStationComponent* Station, ERpgInventorySortMode SortMode)
{
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage)
	{
		return;
	}

	BaseStorage->ApplyResourceSort(SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveBaseResourceEntry_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage || !Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	BaseStorage->MoveResourceEntry(ItemDefinition, TargetIndex);
}

void URpgInventoryUiActionComponent::RequestPlaceBaseBuildable_Implementation(ARpgBaseCampActor* BaseCamp, URpgBaseBuildableDefinition* BuildableDefinition, FTransform BuildTransform, bool bAutoContributeFromBase)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (!BaseCamp || !BuildableDefinition || !RequestingActor)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: missing input. Owner=%s BaseCamp=%s Buildable=%s RequestingActor=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(RequestingActor));
		return;
	}

	if (!BaseCamp->CanPlaceBuildableAtTransform(BuildableDefinition, BuildTransform, RequestingActor))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: placement validation denied. Owner=%s BaseCamp=%s Buildable=%s BuildActorClass=%s BaseDist=%.0f BuilderDist=%.0f BuildLocation=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(BuildableDefinition->BuildActorClass),
			FVector::Dist(BaseCamp->GetActorLocation(), BuildTransform.GetLocation()),
			FVector::Dist(RequestingActor->GetActorLocation(), BuildTransform.GetLocation()),
			*BuildTransform.GetLocation().ToCompactString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: world missing. Owner=%s BaseCamp=%s Buildable=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition));
		return;
	}

	TSubclassOf<ARpgBaseConstructionSiteActor> ConstructionSiteClass = BuildableDefinition->ConstructionSiteActorClass;
	if (!ConstructionSiteClass)
	{
		ConstructionSiteClass = ARpgBaseConstructionSiteActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = BaseCamp;
	SpawnParams.Instigator = Cast<APawn>(RequestingActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARpgBaseConstructionSiteActor* ConstructionSite = World->SpawnActor<ARpgBaseConstructionSiteActor>(ConstructionSiteClass, BuildTransform, SpawnParams);
	if (!ConstructionSite)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: construction site spawn failed. Owner=%s BaseCamp=%s Buildable=%s SiteClass=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(ConstructionSiteClass));
		return;
	}

	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Place buildable succeeded: Site=%s BaseCamp=%s Buildable=%s BuildActorClass=%s AutoContributeFromBase=%s Location=%s"),
		*GetNameSafe(ConstructionSite),
		*GetNameSafe(BaseCamp),
		*GetNameSafe(BuildableDefinition),
		*GetNameSafe(BuildableDefinition->BuildActorClass),
		bAutoContributeFromBase ? TEXT("true") : TEXT("false"),
		*BuildTransform.GetLocation().ToCompactString());

	ConstructionSite->InitializeConstructionSite(BaseCamp, BuildableDefinition);
	if (bAutoContributeFromBase && IsValid(ConstructionSite) && !ConstructionSite->IsConstructionComplete())
	{
		ConstructionSite->ContributeAllResources(RequestingActor, true);
	}
}

void URpgInventoryUiActionComponent::RequestContributeAllToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, bool bAllowBaseStorage)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (ConstructionSite && RequestingActor)
	{
		ConstructionSite->ContributeAllResources(RequestingActor, bAllowBaseStorage);
	}
}

void URpgInventoryUiActionComponent::RequestContributeMaterialToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, bool bAllowBaseStorage)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (ConstructionSite && RequestingActor && ItemDefinition && StackCount > 0)
	{
		ConstructionSite->ContributeMaterial(RequestingActor, ItemDefinition, StackCount, bAllowBaseStorage);
	}
}

void URpgInventoryUiActionComponent::RequestCraftRecipe_Implementation(URpgCraftingStationComponent* CraftingStation, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (!CraftingStation || !RecipeDefinition || !RequestingActor || !CraftingStation->CanCraftRecipeQuantity(RequestingActor, RecipeDefinition, Quantity))
	{
		return;
	}

	CraftingStation->QueueCraftRecipe(RequestingActor, RecipeDefinition, Quantity);
}

void URpgInventoryUiActionComponent::RequestCancelCraftJob_Implementation(URpgCraftingStationComponent* CraftingStation, FGuid JobId)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->CancelCraftJob(RequestingActor, JobId);
	}
}

void URpgInventoryUiActionComponent::RequestPauseCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->PauseCraftingStation(RequestingActor);
	}
}

void URpgInventoryUiActionComponent::RequestResumeCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->ResumeCraftingStation(RequestingActor);
	}
}

bool URpgInventoryUiActionComponent::CanAccessInventory(URpgInventoryManagerComponent* Inventory) const
{
	if (Inventory == nullptr)
	{
		return false;
	}

	if (Inventory == FindPlayerInventory())
	{
		return true;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();

	const URpgInventoryManagerComponent* BaseArmoryInventory = Inventory;
	const AActor* InventoryOwner = Inventory->GetOwner();
	const URpgBaseStorageStationComponent* Station = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgBaseStorageStationComponent>() : nullptr;
	if (Station && Station->GetArmoryInventory() == BaseArmoryInventory)
	{
		return Station->CanActorAccess(RequestingActor);
	}

	const URpgCraftingStationComponent* CraftingStation = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgCraftingStationComponent>() : nullptr;
	if (CraftingStation && CraftingStation->GetOutputInventory() == Inventory)
	{
		return CraftingStation->CanActorAccess(RequestingActor);
	}

	const URpgInventoryContainerComponent* Container = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
	if (!Container)
	{
		return false;
	}

	return Container->CanActorAccess(RequestingActor);
}

URpgInventoryManagerComponent* URpgInventoryUiActionComponent::FindPlayerInventory() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetInventoryManagerComponent();
		}
	}

	return nullptr;
}

URpgQuickBarComponent* URpgInventoryUiActionComponent::FindQuickBar() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetQuickBarComponent();
	}

	return nullptr;
}

URpgEquipmentLoadoutComponent* URpgInventoryUiActionComponent::FindEquipmentLoadout() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetEquipmentLoadoutComponent();
	}

	return nullptr;
}

bool URpgInventoryUiActionComponent::CanTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount) const
{
	if (!SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !Item)
	{
		return false;
	}

	if (!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
	{
		return false;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (RequestedCount <= 0 || RequestedCount > AvailableCount)
	{
		return false;
	}

	return CanTargetAcceptTransferredStack(TargetInventory, Item, RequestedCount, RequestedCount >= AvailableCount);
}

bool URpgInventoryUiActionComponent::CanTransferItemStackToInventorySlot(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex) const
{
	if (!SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !Item || TargetSlotIndex < 0)
	{
		return false;
	}

	if (!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
	{
		return false;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (RequestedCount <= 0 || RequestedCount > AvailableCount)
	{
		return false;
	}

	URpgInventoryItemInstance* TargetItem = TargetInventory->GetItemInSlot(TargetSlotIndex);
	if (!TargetItem)
	{
		if (RequestedCount >= AvailableCount)
		{
			return TargetInventory->CanAddItemInstanceToSlot(Item, AvailableCount, TargetSlotIndex);
		}

		return TargetInventory->CanAddItemDefinitionToSlot(Item->GetItemDef(), RequestedCount, TargetSlotIndex);
	}

	if (TargetItem->GetItemDef() == Item->GetItemDef() && TargetInventory->GetFreeStackCapacity(TargetItem) > 0)
	{
		return true;
	}

	return RequestedCount >= AvailableCount && SourceInventory->GetItemSlotIndex(Item) != INDEX_NONE;
}

bool URpgInventoryUiActionComponent::CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const
{
	if (!Station)
	{
		return false;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	return Station->CanActorAccess(RequestingActor);
}

void URpgInventoryUiActionComponent::ClearPlayerAssignmentsForItem(URpgInventoryItemInstance* Item) const
{
	if (URpgQuickBarComponent* QuickBar = FindQuickBar())
	{
		QuickBar->ClearItemFromAllLoadoutSlots(Item);
	}

	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->ClearItemFromAllEquipmentSlots(Item);
	}
}
