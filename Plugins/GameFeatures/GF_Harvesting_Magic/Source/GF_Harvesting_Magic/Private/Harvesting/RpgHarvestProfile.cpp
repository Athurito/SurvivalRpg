#include "Harvesting/RpgHarvestProfile.h"

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
	return Result;
}
#endif
