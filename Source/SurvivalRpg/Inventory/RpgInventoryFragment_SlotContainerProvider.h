#pragma once

#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgInventoryFragment_SlotContainerProvider.generated.h"

/**
 * Compatibility fragment for legacy backpacks, belts, pouches, and resource bags.
 *
 * Existing assets keep their ProvidedSlotGroups data while the inventory graph consumes converted item-owned
 * container definitions through GetProvidedContainers. New definitions should use URpgInventoryFragment_ItemContainer.
 */
UCLASS(BlueprintType, meta = (DeprecationMessage = "Use RpgInventoryFragment_ItemContainer for item-owned nested grids."))
class SURVIVALRPG_API URpgInventoryFragment_SlotContainerProvider : public URpgInventoryFragment_ItemContainer
{
	GENERATED_BODY()

public:
	/** Legacy equipped-layout grids retained for serialized assets and converted to item-owned containers at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout", meta = (TitleProperty = "ContainerId", DeprecatedProperty, DeprecationMessage = "Migrate this row to ProvidedContainers."))
	TArray<FRpgInventorySlotGroupDefinition> ProvidedSlotGroups;

	/** Appends native item-owned definitions followed by compatible conversions of legacy ProvidedSlotGroups rows. */
	virtual void GetProvidedContainers(TArray<FRpgInventoryItemContainerDefinition>& OutContainers) const override;

	/**
	 * Appends native rows and every converted legacy row without runtime filtering so malformed authored data is visible.
	 */
	virtual void GetAuthoredContainerDefinitions(
		TArray<FRpgInventoryItemContainerDefinition>& OutContainers) const override;
};
