#include "RpgInventoryUiActionComponent.h"

#include "GameFramework/Pawn.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiActionComponent)

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(const URpgInventoryItemInstance* Item)
	{
		return Item ? Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
	}

	bool IsMaterialItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
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
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !ArmoryInventory || !Item || IsMaterialItem(Item))
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
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !ArmoryInventory || !Item)
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
