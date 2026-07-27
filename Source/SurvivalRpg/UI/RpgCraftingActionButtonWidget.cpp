#include "RpgCraftingActionButtonWidget.h"

#include "CommonTextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingActionButtonWidget)

void URpgCraftingActionButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Text)
	{
		Text->SetText(CraftButtonText);
	}
}

void URpgCraftingActionButtonWidget::SetCraftButtonText(FText InText)
{
	CraftButtonText = MoveTemp(InText);
	if (Text)
	{
		Text->SetText(CraftButtonText);
	}
}
