// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgIndicatorLayer.h"

#include "SActorCanvas.h"
#include "Widgets/Layout/SBox.h"

URpgIndicatorLayer::URpgIndicatorLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URpgIndicatorLayer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ActorCanvas.Reset();
}

TSharedRef<SWidget> URpgIndicatorLayer::RebuildWidget()
{
	if (!IsDesignTime())
	{
		if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			ActorCanvas = SNew(SActorCanvas, FLocalPlayerContext(LocalPlayer));
			return ActorCanvas.ToSharedRef();
		}
	}

	return SNew(SBox);
}
