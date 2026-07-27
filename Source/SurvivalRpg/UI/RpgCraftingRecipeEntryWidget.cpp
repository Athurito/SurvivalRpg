#include "RpgCraftingRecipeEntryWidget.h"

#include "CommonLazyImage.h"
#include "CommonTextBlock.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/UI/RpgMvvmWidgetUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingRecipeEntryWidget)

const FName URpgCraftingRecipeEntryWidget::RecipeViewModelSourceName(
	TEXT("RpgCraftingRecipeViewModel"));

void URpgCraftingRecipeEntryWidget::NativeOnListItemObjectSet(
	UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetRecipeViewModel(Cast<URpgCraftingRecipeViewModel>(ListItemObject));
}

void URpgCraftingRecipeEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();
	SetRecipeViewModel(nullptr);
	SetIsSelected(false, false);
}

void URpgCraftingRecipeEntryWidget::NativeDestruct()
{
	SetRecipeViewModel(nullptr);
	Super::NativeDestruct();
}

void URpgCraftingRecipeEntryWidget::SetRecipeViewModel(
	URpgCraftingRecipeViewModel* InViewModel)
{
	RecipeViewModel = InViewModel;
	RpgMvvmWidgetUtils::SetOptionalManualViewModel(
		this,
		RecipeViewModelSourceName,
		RecipeViewModel,
		URpgCraftingRecipeViewModel::StaticClass());
}

void URpgCraftingRecipeEntryWidget::SetRecipeIcon(
	TSoftObjectPtr<UTexture2D> InIcon)
{
	if (Icon)
	{
		Icon->SetBrushFromLazyTexture(InIcon);
	}
}

void URpgCraftingRecipeEntryWidget::SetRecipeTier(int32 InRecipeTier)
{
	if (RecipeTierText)
	{
		RecipeTierText->SetText(
			InRecipeTier > 0
				? FText::Format(
					NSLOCTEXT("RpgCrafting", "RecipeTierFormat", "Tier {0}"),
					FText::AsNumber(InRecipeTier))
				: FText::GetEmpty());
	}
}
