#include "RpgInventoryUiActionComponent.h"

#include "GameFramework/Pawn.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiActionComponent)

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

	if (TransferCount >= AvailableCount)
	{
		if (SourceInventory == FindPlayerInventory())
		{
			ClearPlayerAssignmentsForItem(Item);
		}

		SourceInventory->RemoveItemInstance(Item);
		TargetInventory->AddItemInstanceWithStack(Item, AvailableCount);
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

	const AActor* InventoryOwner = Inventory->GetOwner();
	const URpgInventoryContainerComponent* Container = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
	if (!Container)
	{
		return false;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
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
	return RequestedCount > 0 && RequestedCount <= AvailableCount;
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
