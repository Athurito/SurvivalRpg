#pragma once

#include "RpgInventoryItemDefinition.h"
#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgInventoryFragment_SlotContainerProvider.generated.h"

/**
 * Item fragment for backpacks, belts, pouches, and resource bags that expand the player's inventory layout.
 *
 * When assigned to a supported equipment/container slot, its grid definitions are exposed as spatial content
 * containers on the owning inventory while the concrete item instance remains the identity that can be moved,
 * dropped, looted, and equipped again.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_SlotContainerProvider : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Spatial grid containers contributed while this item is equipped as a bag, belt, pouch, or resource bag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout", meta = (TitleProperty = "ContainerId"))
	TArray<FRpgInventorySlotGroupDefinition> ProvidedSlotGroups;
};
