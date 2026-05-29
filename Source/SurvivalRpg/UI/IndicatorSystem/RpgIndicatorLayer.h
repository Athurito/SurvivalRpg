// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"

#include "RpgIndicatorLayer.generated.h"

class SActorCanvas;

/**
 * Full-screen HUD layer that projects active indicator descriptors into Slate.
 */
UCLASS()
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
