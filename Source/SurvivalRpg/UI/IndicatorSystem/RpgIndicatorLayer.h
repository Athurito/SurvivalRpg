// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"

#include "RpgIndicatorLayer.generated.h"

class SActorCanvas;

/**
 * Designer-placeable full-screen HUD layer for projected world indicators.
 *
 * Author this widget inside the CommonUI HUD layout. The controller component
 * owns descriptor state only and never creates a separate viewport widget.
 */
UCLASS(meta = (DisplayName = "RPG Indicator Layer"))
class SURVIVALRPG_API URpgIndicatorLayer : public UWidget
{
	GENERATED_BODY()

public:
	URpgIndicatorLayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SActorCanvas> ActorCanvas;
};
