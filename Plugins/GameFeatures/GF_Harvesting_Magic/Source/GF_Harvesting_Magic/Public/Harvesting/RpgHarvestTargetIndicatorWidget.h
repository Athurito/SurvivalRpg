#pragma once

#include "Blueprint/UserWidget.h"
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/IActorIndicatorWidget.h"

#include "RpgHarvestTargetIndicatorWidget.generated.h"

class UIndicatorDescriptor;

/**
 * Native base for projected harvest target indicators.
 *
 * URpgHarvestTargetingComponent creates one indicator per indicated target. This base binds to that indicator and
 * exposes the summary of exactly the target it marks; the Widget Blueprint child owns layout, text and styling.
 * Local, cosmetic and UI-read-only.
 */
UCLASS(Abstract, Blueprintable)
class GF_HARVESTING_MAGIC_API URpgHarvestTargetIndicatorWidget : public UUserWidget, public IIndicatorWidgetInterface
{
	GENERATED_BODY()

public:
	//~ IIndicatorWidgetInterface
	virtual void BindIndicator_Implementation(UIndicatorDescriptor* Indicator) override;
	virtual void UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator) override;
	//~ End IIndicatorWidgetInterface

	/**
	 * Summarizes the target this indicator marks. Returns false while unbound or once the target left the preview.
	 * OutRemainingSections is the current stock; OutSectionsToTake is what the previewed ability would extract.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Indicator")
	bool GetTargetStatus(
		ERpgHarvestTargetStatus& OutStatus,
		int32& OutRemainingSections,
		int32& OutSectionCount,
		int32& OutSectionsToTake) const;

	/** True while the indicator belongs to a held aim ability rather than the primary swing. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Indicator")
	bool IsAimingPreview() const;

	/**
	 * True while the previewed swing would strike the active weak point of the target this indicator marks; the
	 * sections to take then include the weak-point bonus.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Indicator")
	bool IsWeakPointTargeted() const;

	/**
	 * Returns the first yield conversion of the previewed ability, for example two wood into one charcoal: the output
	 * item's display name and the input items per output item. False when the preview converts nothing.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Indicator")
	bool GetYieldConversion(FText& OutOutputName, int32& OutInputPerOutput) const;

private:
	/** Read model that created the bound indicator. */
	TWeakObjectPtr<const URpgHarvestTargetingComponent> Targeting;

	/** Bound indicator; identifies the marked target, including its instance for instanced targets. */
	TWeakObjectPtr<const UIndicatorDescriptor> BoundIndicator;
};
