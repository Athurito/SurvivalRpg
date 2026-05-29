#pragma once

#include "Blueprint/UserWidget.h"

#include "RpgIndicatorHostWidget.generated.h"

class SActorCanvas;

/**
 * Viewport host created by the client indicator manager to render projected indicators.
 */
UCLASS()
class SURVIVALRPG_API URpgIndicatorHostWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SActorCanvas> ActorCanvas;
};
