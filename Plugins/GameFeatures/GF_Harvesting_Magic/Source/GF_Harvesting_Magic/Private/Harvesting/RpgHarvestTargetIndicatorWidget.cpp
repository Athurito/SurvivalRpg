#include "Harvesting/RpgHarvestTargetIndicatorWidget.h"

#include "Components/SceneComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestTargetIndicatorWidget)

void URpgHarvestTargetIndicatorWidget::BindIndicator_Implementation(UIndicatorDescriptor* Indicator)
{
	Targeting = Indicator ? Cast<URpgHarvestTargetingComponent>(Indicator->GetDataObject()) : nullptr;
	TargetComponent = Indicator ? Indicator->GetSceneComponent() : nullptr;
}

void URpgHarvestTargetIndicatorWidget::UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator)
{
	Targeting.Reset();
	TargetComponent.Reset();
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
	return TargetingComponent->GetTargetStatus(
		TargetComponent.Get(),
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

bool URpgHarvestTargetIndicatorWidget::IsWeakPointTargeted() const
{
	const URpgHarvestTargetingComponent* TargetingComponent = Targeting.Get();
	const FRpgHarvestTargetEvaluation* Target =
		TargetingComponent ? TargetingComponent->FindTargetEvaluation(TargetComponent.Get()) : nullptr;
	return Target && Target->WouldHarvest() && Target->Result.bWeakPointHit;
}
