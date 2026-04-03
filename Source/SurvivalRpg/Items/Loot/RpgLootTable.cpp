#include "RpgLootTable.h"

#include "SurvivalRpg/Items/RpgItemDefinition.h"

TArray<TObjectPtr<URpgItemDefinition>> URpgLootTable::RollItemDefinitions(FRandomStream& RandomStream) const
{
	TArray<TObjectPtr<URpgItemDefinition>> Result;
	if (Entries.IsEmpty())
	{
		return Result;
	}

	const int32 NumRolls = RandomStream.RandRange(MinRolls, FMath::Max(MinRolls, MaxRolls));
	for (int32 RollIndex = 0; RollIndex < NumRolls; ++RollIndex)
	{
		float TotalWeight = 0.0f;
		for (const FRpgLootTableEntry& Entry : Entries)
		{
			if (Entry.ItemDefinition != nullptr && Entry.Weight > 0.0f)
			{
				TotalWeight += Entry.Weight;
			}
		}

		if (TotalWeight <= 0.0f)
		{
			break;
		}

		float Threshold = RandomStream.FRandRange(0.0f, TotalWeight);
		for (const FRpgLootTableEntry& Entry : Entries)
		{
			if (Entry.ItemDefinition == nullptr || Entry.Weight <= 0.0f)
			{
				continue;
			}

			Threshold -= Entry.Weight;
			if (Threshold > 0.0f)
			{
				continue;
			}

			const int32 DropCount = RandomStream.RandRange(Entry.MinDropCount, FMath::Max(Entry.MinDropCount, Entry.MaxDropCount));
			for (int32 CountIndex = 0; CountIndex < DropCount; ++CountIndex)
			{
				Result.Add(Entry.ItemDefinition);
			}
			break;
		}
	}

	return Result;
}
