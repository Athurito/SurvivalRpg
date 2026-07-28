#include "Harvesting/RpgHarvestProfile.h"

#include "Misc/DataValidation.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestProfile)

FPrimaryAssetId URpgHarvestProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("RpgHarvestProfile"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult URpgHarvestProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto MarkInvalid = [&Result, &Context](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (!LootTable)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestProfile", "MissingLootTable", "A harvest profile requires a loot table."));
	}
	else
	{
		FString LootTableError;
		if (!LootTable->HasValidConfiguration(&LootTableError))
		{
			MarkInvalid(FText::Format(
				NSLOCTEXT("RpgHarvestProfile", "InvalidLootTable", "The harvest loot table is invalid: {0}"),
				FText::FromString(LootTableError)));
		}
	}
	const FGameplayTag GatheringRoot = FGameplayTag::RequestGameplayTag(TEXT("Skill.Gathering"), false);
	if (!SkillTag.IsValid() || !GatheringRoot.IsValid() || !SkillTag.MatchesTag(GatheringRoot))
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestProfile", "InvalidSkillTag", "SkillTag must be a registered Skill.Gathering.* tag."));
	}
	if (MinimumSkillLevel < 1 || MinimumSkillLevel > 100)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestProfile", "InvalidMinimumSkillLevel", "MinimumSkillLevel must be between 1 and 100."));
	}
	if (SkillExperience < 0)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestProfile", "NegativeSkillExperience", "SkillExperience cannot be negative."));
	}
	if (MinimumRespawnSeconds < 0.0f || MaximumRespawnSeconds < MinimumRespawnSeconds)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestProfile", "InvalidRespawnRange", "Respawn seconds must be non-negative and Maximum must not be below Minimum."));
	}
	if (!OverflowDropClass)
	{
		Context.AddWarning(NSLOCTEXT("RpgHarvestProfile", "MissingOverflowDrop", "No overflow drop class is set; the native dropped-inventory actor will be used."));
	}

	return Result;
}
#endif
