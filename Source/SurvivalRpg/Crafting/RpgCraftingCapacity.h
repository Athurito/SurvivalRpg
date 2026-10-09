#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "Templates/SubclassOf.h"

class URpgCraftingRecipeDefinition;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;

/** Read-only space summary of one chest for a recipe, computed from replicated state so clients can show it. */
struct SURVIVALRPG_API FRpgCraftingStorageCapacity
{
	/** Cells of the chest's root grid. */
	int32 TotalCells = 0;

	/** Root-grid cells covered by items. */
	int32 UsedCells = 0;

	/** The recipe's first output, which the other fields describe. */
	TSubclassOf<URpgInventoryItemDefinition> OutputDefinition;

	/** Pieces of the first output already in the chest. */
	int32 PresentCount = 0;

	/** Pieces of the first output one stack holds. */
	int32 MaxStackSize = 1;

	/** Cells one piece or stack of the first output covers. */
	FRpgInventoryGridSize Footprint;

	/** Whole units (up to the wanted count) whose outputs fit now, by the same first-fit rules the server applies. */
	int32 UnitsThatFit = 0;

	int32 GetFreeCells() const { return FMath::Max(0, TotalCells - UsedCells); }
};

/** Client-safe capacity queries for crafting targets. They never mutate an inventory; the server re-checks every commit. */
namespace RpgCraftingCapacity
{
	/** Cells used and available in the root grid of an inventory. */
	SURVIVALRPG_API void GetRootCellUsage(const URpgInventoryManagerComponent& Storage, int32& OutUsedCells, int32& OutTotalCells);

	/**
	 * Evaluates how many of WantedUnits units of the recipe fit into the chest. Exact for single-output recipes; with
	 * several outputs each output is checked on its own, so the result is an upper bound.
	 */
	SURVIVALRPG_API FRpgCraftingStorageCapacity Evaluate(const URpgInventoryManagerComponent& Storage,
		const URpgCraftingRecipeDefinition& Recipe, int32 WantedUnits);
}
