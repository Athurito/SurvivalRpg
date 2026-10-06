#include "Harvesting/RpgHarvestTargetIndicatorWidget.h"

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestTargetIndicatorWidget)

void URpgHarvestTargetIndicatorWidget::BindIndicator_Implementation(UIndicatorDescriptor* Indicator)
{
	Targeting = Indicator ? Cast<URpgHarvestTargetingComponent>(Indicator->GetDataObject()) : nullptr;
	BoundIndicator = Indicator;
}

void URpgHarvestTargetIndicatorWidget::UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator)
{
	Targeting.Reset();
	BoundIndicator.Reset();
}

bool URpgHarvestTargetIndicatorWidget::GetTargetStatus(
	ERpgHarvestTargetStatus& OutStatus,
	int32& OutRemainingSections,
	int32& OutSectionCount,
	int32& OutSectionsToTake) const
{
	const URpgHarvestTargetingComponent* TargetingComponent = Targeting.Get();
	if (!TargetingComponent)
	{
		OutStatus = ERpgHarvestTargetStatus::None;
		OutRemainingSections = 0;
		OutSectionCount = 0;
		OutSectionsToTake = 0;
		return false;
	}
	return TargetingComponent->GetIndicatedTargetStatus(
		BoundIndicator.Get(),
		OutStatus,
		OutRemainingSections,
		OutSectionCount,
		OutSectionsToTake);
}

bool URpgHarvestTargetIndicatorWidget::IsAimingPreview() const
{
	const URpgHarvestTargetingComponent* TargetingComponent = Targeting.Get();
	return TargetingComponent && TargetingComponent->IsPreviewAiming();
}

bool URpgHarvestTargetIndicatorWidget::GetYieldConversion(FText& OutOutputName, int32& OutInputPerOutput) const
{
	OutOutputName = FText::GetEmpty();
	OutInputPerOutput = 0;
	const URpgHarvestTargetingComponent* TargetingComponent = Targeting.Get();
	if (!TargetingComponent)
	{
		return false;
	}
	for (const FRpgHarvestYieldConversion& Conversion : TargetingComponent->GetCurrentPreview().YieldConversions)
	{
		if (const URpgInventoryItemDefinition* Output = Conversion.IsValid()
			? GetDefault<URpgInventoryItemDefinition>(Conversion.OutputItem)
			: nullptr)
		{
			OutOutputName = Output->DisplayName;
			OutInputPerOutput = Conversion.InputPerOutput;
			return true;
		}
	}
	return false;
}

bool URpgHarvestTargetIndicatorWidget::IsWeakPointTargeted() const
{
	const URpgHarvestTargetingComponent* TargetingComponent = Targeting.Get();
	const FRpgHarvestTargetEvaluation* Target =
		TargetingComponent ? TargetingComponent->FindIndicatedTarget(BoundIndicator.Get()) : nullptr;
	return Target && Target->WouldHarvest() && Target->Result.bWeakPointHit;
}
