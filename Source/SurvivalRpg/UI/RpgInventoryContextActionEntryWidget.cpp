#include "RpgInventoryContextActionEntryWidget.h"

#include "CommonLazyImage.h"
#include "Components/TextBlock.h"
#include "SurvivalRpg/UI/RpgInventoryContextMenuWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryContextActionEntryWidget)

URpgInventoryContextActionEntryWidget::URpgInventoryContextActionEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URpgInventoryContextActionEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RefreshActionPresentation();
}

void URpgInventoryContextActionEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Building the label applies its text style, which resets the destructive colour set before construction.
	RefreshActionPresentation();
}

void URpgInventoryContextActionEntryWidget::InitializeContextAction(
	URpgInventoryContextMenuWidget* InOwningMenu,
	ERpgInventoryContextAction InAction,
	const FText& InLabel)
{
	OwningMenu = InOwningMenu;
	ContextAction = InAction;
	ActionLabel = InLabel;
	RefreshActionPresentation();
}

void URpgInventoryContextActionEntryWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	if (OwningMenu)
	{
		OwningMenu->HandleContextActionClicked(ContextAction);
	}
}

void URpgInventoryContextActionEntryWidget::RefreshActionPresentation()
{
	if (!bCapturedDefaultColors)
	{
		bCapturedDefaultColors = true;
		DefaultLabelColor = Text_ActionLabel ? Text_ActionLabel->GetColorAndOpacity() : FSlateColor();
		DefaultIconColor = Image_ActionIcon ? Image_ActionIcon->GetColorAndOpacity() : FLinearColor::White;
	}

	const bool bDestructive = IsDestructiveAction();
	if (Text_ActionLabel)
	{
		Text_ActionLabel->SetText(ActionLabel);
		Text_ActionLabel->SetColorAndOpacity(bDestructive ? DestructiveColor : DefaultLabelColor);
	}
	if (Image_ActionIcon)
	{
		const TSoftObjectPtr<UTexture2D>* ActionIcon = ActionIcons.Find(ContextAction);
		if (ActionIcon && !ActionIcon->IsNull())
		{
			Image_ActionIcon->SetBrushFromLazyTexture(*ActionIcon);
			Image_ActionIcon->SetColorAndOpacity(bDestructive ? DestructiveColor.GetSpecifiedColor() : DefaultIconColor);
			Image_ActionIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_ActionIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	if (DestructiveDivider)
	{
		DestructiveDivider->SetVisibility(bDestructive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	BP_OnContextActionConfigured(ContextAction, ActionLabel);
}
