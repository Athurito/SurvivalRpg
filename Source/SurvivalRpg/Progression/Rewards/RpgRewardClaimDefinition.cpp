#include "RpgRewardClaimDefinition.h"

#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRewardClaimDefinition)

bool URpgRewardClaimDefinition::HasValidConfiguration(FString* OutError) const
{
	auto Fail = [OutError](const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	};

	if (Options.IsEmpty() == (LootTable == nullptr))
	{
		return Fail(TEXT("A reward needs either options to choose from or a loot table, not both."));
	}
	for (const FRpgRewardClaimOption& Option : Options)
	{
		if (!Option.ItemDefinition || Option.Count < 1)
		{
			return Fail(TEXT("Every reward option needs an item definition and a count of at least 1."));
		}
	}
	if (LootTable && !LootTable->HasValidConfiguration(OutError))
	{
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult URpgRewardClaimDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	FString Error;
	if (!HasValidConfiguration(&Error))
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
