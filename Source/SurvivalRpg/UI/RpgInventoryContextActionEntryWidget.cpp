#include "RpgInventoryContextActionEntryWidget.h"

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
	if (Text_ActionLabel)
	{
		Text_ActionLabel->SetText(ActionLabel);
	}
	BP_OnContextActionConfigured(ContextAction, ActionLabel);
}
