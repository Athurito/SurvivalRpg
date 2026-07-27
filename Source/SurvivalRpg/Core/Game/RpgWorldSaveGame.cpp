#include "RpgWorldSaveGame.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWorldSaveGame)

namespace RpgWorldSaveGame
{
bool ValidateGraphEnvelope(const FRpgInventoryGraphSaveData& Graph, FString& OutError)
{
	if (Graph.SchemaVersion != FRpgInventoryGraphSaveData::CurrentSchemaVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported inventory graph schema %d."), Graph.SchemaVersion);
		return false;
	}

	TSet<FRpgInventoryItemId> SeenItemIds;
	for (const FRpgInventorySavedItem& Item : Graph.Items)
	{
		if (!Item.ItemId.IsValid() || SeenItemIds.Contains(Item.ItemId))
		{
			OutError = TEXT("Inventory graph contains an invalid or duplicate persistent item id.");
			return false;
		}
		if (Item.ItemDefinition.IsNull() || Item.StackCount <= 0 || !Item.Container.IsValid())
		{
			OutError = TEXT("Inventory graph contains an invalid definition, stack, or container handle.");
			return false;
		}

		SeenItemIds.Add(Item.ItemId);
	}

	return true;
}
}

bool URpgWorldSaveGame::ValidateForLoad(FString& OutError) const
{
	OutError.Reset();
	if (SchemaVersion != CurrentSchemaVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported world-save schema %d."), SchemaVersion);
		return false;
	}
	if (SaveSequence < 0)
	{
		OutError = TEXT("Save sequence cannot be negative.");
		return false;
	}

	for (const TPair<FString, FRpgPlayerSaveData>& Pair : Players)
	{
		if (Pair.Key.IsEmpty() || !Pair.Value.IsSchemaSupported())
		{
			OutError = FString::Printf(TEXT("Player profile '%s' has an invalid save envelope."), *Pair.Key);
			return false;
		}
		if (Pair.Value.bHasInventoryGraph && !RpgWorldSaveGame::ValidateGraphEnvelope(Pair.Value.InventoryGraph, OutError))
		{
			OutError = FString::Printf(TEXT("Player profile '%s': %s"), *Pair.Key, *OutError);
			return false;
		}
	}

	for (const TPair<FName, FRpgWorldContainerSaveData>& Pair : WorldContainers)
	{
		if (Pair.Key.IsNone() || Pair.Value.PersistentContainerId != Pair.Key)
		{
			OutError = TEXT("World-container map contains a missing or mismatched persistent id.");
			return false;
		}
		if (!RpgWorldSaveGame::ValidateGraphEnvelope(Pair.Value.InventoryGraph, OutError))
		{
			OutError = FString::Printf(TEXT("World container '%s': %s"), *Pair.Key.ToString(), *OutError);
			return false;
		}
	}

	return true;
}
