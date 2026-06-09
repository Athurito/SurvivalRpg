#include "RpgDroppedInventoryActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgDroppedInventoryActor)

void ARpgDroppedInventoryActor::SetPickupInventory(const FInventoryPickup& NewPickupInventory)
{
	if (HasAuthority())
	{
		StaticInventory = NewPickupInventory;
	}
}
