#include "RpgLazyImage.h"

#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLazyImage)

void URpgLazyImage::SetLazyTexture(TSoftObjectPtr<UTexture2D> InTexture)
{
	if (InTexture.IsNull())
	{
		SetBrushFromTexture(nullptr);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetBrushFromLazyTexture(InTexture, /*bMatchSize=*/ false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

#if WITH_EDITOR

const FText URpgLazyImage::GetPaletteCategory()
{
	return NSLOCTEXT("RpgLazyImage", "PaletteCategory", "RPG HUD");
}

#endif
