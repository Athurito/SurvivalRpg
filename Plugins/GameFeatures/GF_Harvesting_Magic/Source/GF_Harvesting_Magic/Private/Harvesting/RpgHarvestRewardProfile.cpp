#include "Harvesting/RpgHarvestRewardProfile.h"

#include "Misc/DataValidation.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestRewardProfile)

#if WITH_EDITOR
EDataValidationResult URpgHarvestRewardProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto MarkInvalid = [&Result, &Context](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (!LootTable)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestRewardProfile", "MissingLootTable", "A harvest reward profile requires a loot table."));
	}
	else
	{
		FString LootTableError;
		if (!LootTable->HasValidConfiguration(&LootTableError))
		{
			MarkInvalid(FText::Format(
				NSLOCTEXT("RpgHarvestRewardProfile", "InvalidLootTable", "The harvest loot table is invalid: {0}"),
				FText::FromString(LootTableError)));
		}
	}

	const FGameplayTag GatheringRoot = FGameplayTag::RequestGameplayTag(TEXT("Skill.Gathering"), false);
	if (!SkillTag.IsValid() || !GatheringRoot.IsValid() || !SkillTag.MatchesTag(GatheringRoot))
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestRewardProfile", "InvalidSkillTag", "SkillTag must be a registered Skill.Gathering.* tag."));
	}
	if (MinimumSkillLevel < 1 || MinimumSkillLevel > 100)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestRewardProfile", "InvalidMinimumSkillLevel", "MinimumSkillLevel must be between 1 and 100."));
	}
	if (SkillExperience < 0)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestRewardProfile", "NegativeSkillExperience", "SkillExperience cannot be negative."));
	}
	if (!OverflowDropClass)
	{
		Context.AddWarning(NSLOCTEXT("RpgHarvestRewardProfile", "MissingOverflowDrop", "No overflow drop class is set; the native dropped-inventory actor will be used."));
	}

	return Result;
}
#endif
