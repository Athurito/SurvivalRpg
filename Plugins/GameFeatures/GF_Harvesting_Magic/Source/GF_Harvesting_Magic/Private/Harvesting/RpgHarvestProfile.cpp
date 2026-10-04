#include "Harvesting/RpgHarvestProfile.h"

#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Misc/DataValidation.h"

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

	if (MinimumRespawnSeconds < 0.0f || MaximumRespawnSeconds < MinimumRespawnSeconds)
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestProfile", "InvalidRespawnRange", "Respawn seconds must be non-negative and Maximum must not be below Minimum."));
	}
	if (SectionCount < 1 || SectionCount > MaxSectionCount)
	{
		MarkInvalid(FText::Format(
			NSLOCTEXT("RpgHarvestProfile", "InvalidSectionCount", "SectionCount must be between 1 and {0}."),
			FText::AsNumber(MaxSectionCount)));
	}
	if (RequiredToolTag.IsValid() &&
		!RequiredToolTag.MatchesTag(RpgHarvestingMagicGameplayTags::Tool_Harvesting))
	{
		MarkInvalid(NSLOCTEXT("RpgHarvestProfile", "InvalidRequiredToolTag", "RequiredToolTag must be empty or a registered Tool.Harvesting.* tag."));
	}
	if (WeakPointBonusSections < 0 || WeakPointBonusSections >= MaxSectionCount)
	{
		MarkInvalid(FText::Format(
			NSLOCTEXT("RpgHarvestProfile", "InvalidWeakPointBonus", "WeakPointBonusSections must be between 0 and {0}."),
			FText::AsNumber(MaxSectionCount - 1)));
	}
	else if (WeakPointBonusSections > 0 && GetClampedSectionCount() < 2)
	{
		Context.AddWarning(NSLOCTEXT(
			"RpgHarvestProfile",
			"WeakPointBonusWithoutSections",
			"WeakPointBonusSections has no effect with a single section; a weak-point hit cannot take more than the whole stock."));
	}
	return Result;
}
#endif
