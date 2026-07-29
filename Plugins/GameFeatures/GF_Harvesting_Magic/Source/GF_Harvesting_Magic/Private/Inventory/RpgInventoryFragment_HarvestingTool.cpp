#include "Inventory/RpgInventoryFragment_HarvestingTool.h"

#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_HarvestingTool)

bool URpgInventoryFragment_HarvestingTool::SupportsToolTag(const FGameplayTag RequiredToolTag) const
{
	return RequiredToolTag.IsValid() && ToolTag.IsValid() && ToolTag.MatchesTag(RequiredToolTag) &&
		FMath::IsFinite(HarvestPower) && HarvestPower > 0.0f;
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryFragment_HarvestingTool::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const FGameplayTag ToolRoot = FGameplayTag::RequestGameplayTag(TEXT("Tool.Harvesting"), false);
	if (!ToolTag.IsValid() || !ToolRoot.IsValid() || !ToolTag.MatchesTag(ToolRoot))
	{
		Context.AddError(NSLOCTEXT(
			"RpgHarvestingTool",
			"InvalidToolTag",
			"ToolTag must be a registered Tool.Harvesting.* tag."));
		Result = EDataValidationResult::Invalid;
	}
	if (!FMath::IsFinite(HarvestPower) || HarvestPower <= 0.0f)
	{
		Context.AddError(NSLOCTEXT(
			"RpgHarvestingTool",
			"InvalidHarvestPower",
			"HarvestPower must be finite and greater than zero."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
