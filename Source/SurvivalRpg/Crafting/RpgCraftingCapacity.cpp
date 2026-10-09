#include "RpgCraftingCapacity.h"

#include "RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

void RpgCraftingCapacity::GetRootCellUsage(const URpgInventoryManagerComponent& Storage, int32& OutUsedCells, int32& OutTotalCells)
{
	const FRpgInventoryGridSize Grid = Storage.GetDefaultGridSize();
	OutTotalCells = Grid.IsValid() ? Grid.Width * Grid.Height : 0;
	OutUsedCells = 0;
	const FRpgInventoryContainerHandle Root = FRpgInventoryContainerHandle::MakeRoot(Storage.GetDefaultContainerId());
	for (const FRpgInventoryEntryView& Entry : Storage.GetAllEntries())
	{
		if (Entry.Instance && Entry.StackCount > 0 && Entry.Placement.GetContainerHandle() == Root)
		{
			const FRpgInventoryGridSize Size = Entry.Placement.GetOccupiedSize();
			OutUsedCells += Size.Width * Size.Height;
		}
	}
	OutUsedCells = FMath::Min(OutUsedCells, OutTotalCells);
}

FRpgCraftingStorageCapacity RpgCraftingCapacity::Evaluate(const URpgInventoryManagerComponent& Storage,
	const URpgCraftingRecipeDefinition& Recipe, int32 WantedUnits)
{
	FRpgCraftingStorageCapacity Result;
	GetRootCellUsage(Storage, Result.UsedCells, Result.TotalCells);
	if (Recipe.OutputItems.IsEmpty() || !Recipe.OutputItems[0].ItemDefinition)
	{
		return Result;
	}
	Result.OutputDefinition = Recipe.OutputItems[0].ItemDefinition;
	Result.PresentCount = Storage.GetTotalItemCountByDefinition(Result.OutputDefinition);
	Result.MaxStackSize = URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(Result.OutputDefinition);
	if (const URpgInventoryFragment_SpatialItem* Spatial = URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(Result.OutputDefinition))
	{
		Result.Footprint = Spatial->Footprint;
	}

	const auto UnitsFit = [&Storage, &Recipe](int32 Units)
	{
		for (const FRpgCraftingOutputItem& Output : Recipe.OutputItems)
		{
			const int64 Pieces = static_cast<int64>(Output.Count) * Units;
			if (!Output.ItemDefinition || Output.Count <= 0 || Pieces > MAX_int32 ||
				!Storage.CanAddItemDefinition(Output.ItemDefinition, static_cast<int32>(Pieces)))
			{
				return false;
			}
		}
		return true;
	};
	int32 Low = 0;
	int32 High = FMath::Max(0, WantedUnits);
	while (Low < High)
	{
		const int32 Mid = Low + (High - Low + 1) / 2;
		if (UnitsFit(Mid)) { Low = Mid; } else { High = Mid - 1; }
	}
	Result.UnitsThatFit = Low;
	return Result;
}
