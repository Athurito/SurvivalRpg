#include "RpgQuickAccessSlotPickerEntryWidget.h"

#include "Components/TextBlock.h"
#include "SurvivalRpg/UI/RpgInventoryContextMenuWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgQuickAccessSlotPickerEntryWidget)

#define LOCTEXT_NAMESPACE "RpgInventoryActionWidgets"

URpgQuickAccessSlotPickerEntryWidget::URpgQuickAccessSlotPickerEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URpgQuickAccessSlotPickerEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RefreshSlotPresentation();
}

void URpgQuickAccessSlotPickerEntryWidget::InitializeQuickAccessSlot(
	URpgInventoryContextMenuWidget* InOwningMenu,
	int32 InSlotIndex,
	const FText& InBindingLabel,
	bool bInOccupied,
	bool bInCurrentBinding)
{
	OwningMenu = InOwningMenu;
	SlotIndex = FMath::IsWithinInclusive(InSlotIndex, 0, 7) ? InSlotIndex : INDEX_NONE;
	BindingLabel = InBindingLabel;
	bOccupied = bInOccupied;
	bCurrentBinding = bInCurrentBinding;
	SetIsEnabled(SlotIndex != INDEX_NONE);
	RefreshSlotPresentation();
}

void URpgQuickAccessSlotPickerEntryWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	if (OwningMenu && SlotIndex != INDEX_NONE)
	{
		OwningMenu->SelectQuickAccessSlot(SlotIndex);
	}
}

void URpgQuickAccessSlotPickerEntryWidget::RefreshSlotPresentation()
{
	const int32 DisplaySlotNumber = SlotIndex == INDEX_NONE ? INDEX_NONE : SlotIndex + 1;
	if (Text_SlotLabel)
	{
		FText StatusText = BindingLabel;
		if (bCurrentBinding)
		{
			StatusText = FText::Format(LOCTEXT("CurrentQuickAccessSlotFormat", "{0} (Current)"), BindingLabel);
		}
		Text_SlotLabel->SetText(FText::Format(
			LOCTEXT("QuickAccessPickerSlotFormat", "{0}: {1}"),
			FText::AsNumber(DisplaySlotNumber),
			StatusText));
	}
	BP_OnQuickAccessSlotConfigured(DisplaySlotNumber, BindingLabel, bOccupied, bCurrentBinding);
}

#undef LOCTEXT_NAMESPACE
