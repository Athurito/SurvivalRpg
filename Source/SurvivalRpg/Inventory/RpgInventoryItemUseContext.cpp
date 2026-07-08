#include "RpgInventoryItemUseContext.h"

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryItemUseContext)

void URpgInventoryItemUseContext::Initialize(
	URpgInventoryManagerComponent* InInventory,
	URpgInventoryItemInstance* InItemInstance,
	int32 InRequestedUseCount,
	int32 InConsumeCount)
{
	Inventory = InInventory;
	ItemInstance = InItemInstance;
	RequestedUseCount = FMath::Max(1, InRequestedUseCount);
	ConsumeCount = FMath::Max(0, InConsumeCount);
	bConsumed = false;
}

bool URpgInventoryItemUseContext::TryConsume()
{
	if (bConsumed)
	{
		return true;
	}

	if (ConsumeCount <= 0)
	{
		bConsumed = true;
		return true;
	}

	URpgInventoryManagerComponent* OwningInventory = Inventory.Get();
	URpgInventoryItemInstance* UsedItem = ItemInstance.Get();
	if (!OwningInventory || !UsedItem || OwningInventory->GetItemStackCount(UsedItem) < ConsumeCount)
	{
		return false;
	}

	bConsumed = OwningInventory->RemoveItemInstanceStack(UsedItem, ConsumeCount);
	return bConsumed;
}
