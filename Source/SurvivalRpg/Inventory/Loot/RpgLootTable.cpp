#include "RpgLootTable.h"

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLootTable)

namespace RpgLootTable
{
	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}
}

bool URpgLootTable::HasValidConfiguration(FString* OutError) const
{
	if (Groups.IsEmpty())
	{
		RpgLootTable::SetError(OutError, TEXT("Groups must contain at least one loot group."));
		return false;
	}

	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		const FRpgLootGroup& Group = Groups[GroupIndex];
		if (static_cast<uint8>(Group.Mode) > static_cast<uint8>(ERpgLootGroupMode::WeightedPick) ||
			!FMath::IsFinite(Group.GroupChancePercent) ||
			Group.GroupChancePercent < 0.0f || Group.GroupChancePercent > 100.0f ||
			Group.Entries.IsEmpty())
		{
			RpgLootTable::SetError(
				OutError,
				FString::Printf(TEXT("Groups[%d] has an unknown mode, invalid chance, or no entries."), GroupIndex));
			return false;
		}
		if (Group.Mode == ERpgLootGroupMode::WeightedPick &&
			(Group.WeightedRollCount <= 0 || Group.WeightedRollCount > Group.Entries.Num()))
		{
			RpgLootTable::SetError(
				OutError,
				FString::Printf(
					TEXT("Groups[%d].WeightedRollCount must be between one and the number of unique entries."),
					GroupIndex));
			return false;
		}

		for (int32 EntryIndex = 0; EntryIndex < Group.Entries.Num(); ++EntryIndex)
		{
			const FRpgLootEntry& Entry = Group.Entries[EntryIndex];
			if (!Entry.ItemDefinition || Entry.MinimumQuantity <= 0 ||
				Entry.MaximumQuantity < Entry.MinimumQuantity ||
				Entry.MaximumQuantity > RpgLootMaximumQuantityPerRoll)
			{
				RpgLootTable::SetError(
					OutError,
					FString::Printf(
						TEXT("Groups[%d].Entries[%d] requires an item definition and a positive ordered quantity range."),
						GroupIndex,
						EntryIndex));
				return false;
			}
			if (Group.Mode == ERpgLootGroupMode::Independent &&
				(!FMath::IsFinite(Entry.ChancePercent) || Entry.ChancePercent < 0.0f ||
					Entry.ChancePercent > 100.0f))
			{
				RpgLootTable::SetError(
					OutError,
					FString::Printf(TEXT("Groups[%d].Entries[%d].ChancePercent must be within 0..100."), GroupIndex, EntryIndex));
				return false;
			}
			if (Group.Mode == ERpgLootGroupMode::WeightedPick &&
				(!FMath::IsFinite(Entry.Weight) || Entry.Weight <= 0.0f))
			{
				RpgLootTable::SetError(
					OutError,
					FString::Printf(TEXT("Groups[%d].Entries[%d].Weight must be positive and finite."), GroupIndex, EntryIndex));
				return false;
			}
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult URpgLootTable::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	FString Error;
	if (!HasValidConfiguration(&Error))
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
