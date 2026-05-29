#pragma once

#include "CommonUserWidget.h"
#include "SurvivalRpg/UI/IndicatorSystem/IActorIndicatorWidget.h"

#include "RpgEnemyVitalsIndicatorWidget.generated.h"

class UEnemyVitalsViewmodel;

/**
 * MVVM-backed projected indicator widget for an enemy's replicated vitals.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgEnemyVitalsIndicatorWidget : public UCommonUserWidget, public IIndicatorWidgetInterface
{
	GENERATED_BODY()

public:
	virtual void BindIndicator_Implementation(UIndicatorDescriptor* Indicator) override;
	virtual void UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UEnemyVitalsViewmodel> EnemyVitalsViewmodel;
};
