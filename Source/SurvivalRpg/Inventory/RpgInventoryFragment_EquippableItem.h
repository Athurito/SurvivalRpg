#pragma once

#include "CoreMinimal.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryFragment_EquippableItem.generated.h"

class URpgEquipmentDefinition;

/**
 * Marks an inventory item as explicitly assignable to equipment-managed Gear or Carry roles.
 *
 * The referenced definition is required and must author at least one usable AllowedSlot. The inventory remains the
 * physical authority; equipment and UI only activate or present that placement.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_EquippableItem : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Returns the immutable equipment contract referenced by this fragment. */
	TSubclassOf<URpgEquipmentDefinition> GetEquipmentDefinition() const { return EquipmentDefinition; }

	/** Required static slot, instance, ability, load, and presentation contract used when this item is equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition;
};
