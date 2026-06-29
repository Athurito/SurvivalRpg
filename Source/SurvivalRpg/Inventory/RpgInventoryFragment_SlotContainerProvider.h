#pragma once

#include "RpgInventoryItemDefinition.h"
#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgInventoryFragment_SlotContainerProvider.generated.h"

/**
 * Item fragment for backpacks, belts, pouches, and resource bags that expand the player's inventory layout.
 *
 * The item does not own a second inventory. When assigned to a supported equipment/container slot, its groups are
 * appended to the owning player's single URpgInventoryManagerComponent as extra logical slot ranges.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_SlotContainerProvider : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Slot groups contributed while this item is equipped as a bag, belt, pouch, or resource bag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout", meta = (TitleProperty = "GroupId"))
	TArray<FRpgInventorySlotGroupDefinition> ProvidedSlotGroups;
};
