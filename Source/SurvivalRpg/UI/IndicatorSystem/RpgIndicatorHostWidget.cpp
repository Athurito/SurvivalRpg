#include "RpgIndicatorHostWidget.h"

#include "SActorCanvas.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> URpgIndicatorHostWidget::RebuildWidget()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		ActorCanvas = SNew(SActorCanvas, FLocalPlayerContext(LocalPlayer));
		return ActorCanvas.ToSharedRef();
	}

	return SNew(SBox);
}

void URpgIndicatorHostWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ActorCanvas.Reset();
}
