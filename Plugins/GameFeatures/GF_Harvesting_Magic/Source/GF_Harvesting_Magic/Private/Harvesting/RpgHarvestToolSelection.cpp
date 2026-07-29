#include "Harvesting/RpgHarvestToolSelection.h"

#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

namespace
{
	FRpgSelectedHarvestTool MakeSelection(
		URpgInventoryItemInstance* Item,
		const FGameplayTag RequiredToolTag)
	{
		FRpgSelectedHarvestTool Result;
		const URpgInventoryFragment_HarvestingTool* ToolFragment = Item
			? Item->FindFragmentByClass<URpgInventoryFragment_HarvestingTool>()
			: nullptr;
		if (!ToolFragment || !ToolFragment->SupportsToolTag(RequiredToolTag) || !Item->GetItemId().IsValid())
		{
			return Result;
		}

		Result.ItemInstance = Item;
		Result.ItemId = Item->GetItemId();
		Result.HarvestPower = ToolFragment->HarvestPower;
		return Result;
	}
}

FRpgSelectedHarvestTool FRpgHarvestToolSelection::FindBestOwnedTool(
	const URpgInventoryManagerComponent* Inventory,
	const FGameplayTag RequiredToolTag)
{
	FRpgSelectedHarvestTool Best;
	if (!Inventory || !RequiredToolTag.IsValid())
	{
		return Best;
	}

	for (URpgInventoryItemInstance* Item : Inventory->GetAllItems())
	{
		FRpgSelectedHarvestTool Candidate = MakeSelection(Item, RequiredToolTag);
		if (!Candidate.IsValid())
		{
			continue;
		}

		const bool bHigherPower = Candidate.HarvestPower > Best.HarvestPower;
		const bool bPowerTie = Candidate.HarvestPower == Best.HarvestPower;
		const bool bSmallerId = !Best.ItemId.IsValid() ||
			Candidate.ItemId.ToString() < Best.ItemId.ToString();
		if (!Best.IsValid() || bHigherPower || (bPowerTie && bSmallerId))
		{
			Best = Candidate;
		}
	}
	return Best;
}

FRpgSelectedHarvestTool FRpgHarvestToolSelection::FindOwnedToolById(
	const URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryItemId ItemId,
	const FGameplayTag RequiredToolTag)
{
	if (!Inventory || !ItemId.IsValid())
	{
		return FRpgSelectedHarvestTool();
	}
	return MakeSelection(Inventory->FindItemById(ItemId), RequiredToolTag);
}
