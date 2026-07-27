#include "RpgCraftingIngredientEntryWidget.h"

#include "CommonLazyImage.h"
#include "CommonTextBlock.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/UI/RpgMvvmWidgetUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingIngredientEntryWidget)

const FName URpgCraftingIngredientEntryWidget::IngredientViewModelSourceName(
	TEXT("RpgCraftingIngredientViewModel"));

void URpgCraftingIngredientEntryWidget::NativeOnListItemObjectSet(
	UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetIngredientViewModel(
		Cast<URpgCraftingIngredientViewModel>(ListItemObject));
}

void URpgCraftingIngredientEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();
	SetIngredientViewModel(nullptr);
}

void URpgCraftingIngredientEntryWidget::NativeDestruct()
{
	SetIngredientViewModel(nullptr);
	Super::NativeDestruct();
}

void URpgCraftingIngredientEntryWidget::SetIngredientViewModel(
	URpgCraftingIngredientViewModel* InViewModel)
{
	IngredientViewModel = InViewModel;
	RpgMvvmWidgetUtils::SetOptionalManualViewModel(
		this,
		IngredientViewModelSourceName,
		IngredientViewModel,
		URpgCraftingIngredientViewModel::StaticClass());
}

void URpgCraftingIngredientEntryWidget::SetIngredientIcon(
	TSoftObjectPtr<UTexture2D> InIcon)
{
	if (Icon)
	{
		Icon->SetBrushFromLazyTexture(InIcon);
	}
}

void URpgCraftingIngredientEntryWidget::SetAvailableCount(int32 InCount)
{
	if (AvailableCountText)
	{
		AvailableCountText->SetText(FText::AsNumber(FMath::Max(0, InCount)));
	}
}

void URpgCraftingIngredientEntryWidget::SetRequiredCount(int32 InCount)
{
	if (RequiredCountText)
	{
		RequiredCountText->SetText(FText::AsNumber(FMath::Max(0, InCount)));
	}
}

void URpgCraftingIngredientEntryWidget::SetMissingCount(int32 InCount)
{
	if (MissingCountText)
	{
		const int32 Missing = FMath::Max(0, InCount);
		MissingCountText->SetText(
			Missing > 0
				? FText::Format(
					NSLOCTEXT("RpgCrafting", "MissingIngredientFormat", "-{0}"),
					FText::AsNumber(Missing))
				: FText::GetEmpty());
		MissingCountText->SetVisibility(
			Missing > 0
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

void URpgCraftingIngredientEntryWidget::SetHasEnough(bool bInHasEnough)
{
	if (AvailableCountText)
	{
		AvailableCountText->SetColorAndOpacity(
			bInHasEnough
				? FSlateColor(FLinearColor(0.45f, 0.85f, 0.45f))
				: FSlateColor(FLinearColor(0.95f, 0.35f, 0.25f)));
	}
}
