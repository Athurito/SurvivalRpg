#pragma once

#include "Blueprint/UserWidget.h"
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/IActorIndicatorWidget.h"

#include "RpgHarvestTargetIndicatorWidget.generated.h"

class USceneComponent;
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

private:
	/** Read model that created the bound indicator. */
	TWeakObjectPtr<const URpgHarvestTargetingComponent> Targeting;

	/** Hit component the bound indicator is anchored to. */
	TWeakObjectPtr<const USceneComponent> TargetComponent;
};
