#include "RpgInventoryActionWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryActionWidgets)

#define LOCTEXT_NAMESPACE "RpgInventoryActionWidgets"

namespace
{
constexpr float NativePanelWidth = 320.0f;
constexpr float NativeContextMenuWidth = 220.0f;

FText GetContextActionLabel(ERpgInventoryContextAction Action)
{
	switch (Action)
	{
	case ERpgInventoryContextAction::OpenContainer:
		return LOCTEXT("OpenContainerAction", "Open Container");
	case ERpgInventoryContextAction::Inspect:
		return LOCTEXT("InspectAction", "Inspect");
	case ERpgInventoryContextAction::Unequip:
		return LOCTEXT("UnequipAction", "Unequip");
	case ERpgInventoryContextAction::Use:
		return LOCTEXT("UseAction", "Use");
	case ERpgInventoryContextAction::EquipAndActivate:
		return LOCTEXT("EquipAndActivateAction", "Equip & Activate");
	case ERpgInventoryContextAction::MoveToCarry:
		return LOCTEXT("MoveToCarryAction", "Move to Carry");
	case ERpgInventoryContextAction::Split:
		return LOCTEXT("SplitAction", "Split");
	case ERpgInventoryContextAction::Rotate:
		return LOCTEXT("RotateAction", "Rotate");
	case ERpgInventoryContextAction::QuickAccessBind:
		return LOCTEXT("QuickAccessBindAction", "Bind Quick Access");
	case ERpgInventoryContextAction::QuickAccessUnbind:
		return LOCTEXT("QuickAccessUnbindAction", "Unbind Quick Access");
	case ERpgInventoryContextAction::Transfer:
		return LOCTEXT("TransferAction", "Transfer");
	case ERpgInventoryContextAction::Drop:
		return LOCTEXT("DropAction", "Drop");
	default:
		return LOCTEXT("UnknownContextAction", "Unknown Action");
	}
}

UTextBlock* CreateNativeLabel(UWidgetTree* WidgetTree, const FText& Text, float FontSize = 16.0f)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(Text);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Label->SetFont(Font);
	return Label;
}

void ConfigureOverlaySlot(UOverlaySlot* Slot, EHorizontalAlignment HorizontalAlignment, EVerticalAlignment VerticalAlignment)
{
	if (Slot)
	{
		Slot->SetHorizontalAlignment(HorizontalAlignment);
		Slot->SetVerticalAlignment(VerticalAlignment);
	}
}
}

URpgInventoryContextActionEntryWidget::URpgInventoryContextActionEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URpgInventoryContextActionEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree)
	{
		Text_ActionLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_ActionLabel")));
		if (!Text_ActionLabel && !WidgetTree->RootWidget)
		{
			Text_ActionLabel = CreateNativeLabel(WidgetTree, FText::GetEmpty());
			WidgetTree->RootWidget = Text_ActionLabel;
		}
	}
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

URpgQuickAccessSlotPickerEntryWidget::URpgQuickAccessSlotPickerEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URpgQuickAccessSlotPickerEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree)
	{
		Text_SlotLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SlotLabel")));
		if (!Text_SlotLabel && !WidgetTree->RootWidget)
		{
			Text_SlotLabel = CreateNativeLabel(WidgetTree, FText::GetEmpty());
			WidgetTree->RootWidget = Text_SlotLabel;
		}
	}
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

URpgInventorySplitDialogWidget::URpgInventorySplitDialogWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true);
}

TOptional<FUIInputConfig> URpgInventorySplitDialogWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void URpgInventorySplitDialogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureSplitWidgetTree();
	BindSplitControls();
}

void URpgInventorySplitDialogWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RequestRefreshFocus();
}

void URpgInventorySplitDialogWidget::NativeOnDeactivated()
{
	ResetSplitState(true);
	Super::NativeOnDeactivated();
}

bool URpgInventorySplitDialogWidget::NativeOnHandleBackAction()
{
	CancelSplitDialog();
	return true;
}

FReply URpgInventorySplitDialogWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CancelSplitDialog();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UWidget* URpgInventorySplitDialogWidget::NativeGetDesiredFocusTarget() const
{
	return SpinBox_Amount ? static_cast<UWidget*>(SpinBox_Amount.Get()) : static_cast<UWidget*>(Button_Confirm.Get());
}

bool URpgInventorySplitDialogWidget::InitializeSplitDialog(
	URpgInventorySpatialGridWidget* InSourceGrid,
	FGuid InEntryId,
	int32 InMinimumCount,
	int32 InMaximumCount,
	int32 InDefaultCount)
{
	if (bHasOpenSplitRequest)
	{
		ResetSplitState(true);
	}

	EnsureSplitWidgetTree();
	BindSplitControls();

	const bool bValidRange = InMinimumCount >= 1 && InMaximumCount >= InMinimumCount;
	const bool bValidSelection = InSourceGrid && InEntryId.IsValid() && InSourceGrid->GetSelectedEntryId() == InEntryId;
	if (!bValidRange || !bValidSelection || !Slider_Amount || !SpinBox_Amount || !Button_Confirm || !Button_Cancel)
	{
		if (InSourceGrid)
		{
			InSourceGrid->CancelPendingSplit();
		}
		return false;
	}

	SourceGrid = InSourceGrid;
	SourceAddressSlot = nullptr;
	SplitEntryId = InEntryId;
	SplitItemId = FRpgInventoryItemId();
	MinimumSplitCount = InMinimumCount;
	MaximumSplitCount = InMaximumCount;
	bHasOpenSplitRequest = true;

	Slider_Amount->SetMinValue(static_cast<float>(MinimumSplitCount));
	Slider_Amount->SetMaxValue(static_cast<float>(MaximumSplitCount));
	Slider_Amount->SetStepSize(1.0f);
	Slider_Amount->SetLocked(MinimumSplitCount == MaximumSplitCount);
	SpinBox_Amount->SetMinValue(static_cast<float>(MinimumSplitCount));
	SpinBox_Amount->SetMaxValue(static_cast<float>(MaximumSplitCount));
	SpinBox_Amount->SetMinSliderValue(static_cast<float>(MinimumSplitCount));
	SpinBox_Amount->SetMaxSliderValue(static_cast<float>(MaximumSplitCount));
	SpinBox_Amount->SetDelta(1.0f);
	SetSelectedSplitCount(InDefaultCount);
	RequestRefreshFocus();
	return true;
}

bool URpgInventorySplitDialogWidget::InitializeAddressSplitDialog(
	URpgInventoryAddressSlotWidget* InSourceAddressSlot,
	FRpgInventoryItemId InItemId,
	int32 InMinimumCount,
	int32 InMaximumCount,
	int32 InDefaultCount)
{
	if (bHasOpenSplitRequest)
	{
		ResetSplitState(true);
	}

	EnsureSplitWidgetTree();
	BindSplitControls();
	const URpgInventoryAddressSlotViewModel* AddressViewModel = InSourceAddressSlot
		? InSourceAddressSlot->GetAddressSlotViewModel()
		: nullptr;
	const URpgInventoryItemInstance* CurrentItem = AddressViewModel ? AddressViewModel->GetItemInstance() : nullptr;
	const bool bValidRange = InMinimumCount >= 1 && InMaximumCount >= InMinimumCount;
	if (!bValidRange || !InItemId.IsValid() || !CurrentItem || CurrentItem->GetItemId() != InItemId ||
		!Slider_Amount || !SpinBox_Amount || !Button_Confirm || !Button_Cancel)
	{
		return false;
	}

	SourceGrid = nullptr;
	SourceAddressSlot = InSourceAddressSlot;
	SplitEntryId.Invalidate();
	SplitItemId = InItemId;
	MinimumSplitCount = InMinimumCount;
	MaximumSplitCount = InMaximumCount;
	bHasOpenSplitRequest = true;
	Slider_Amount->SetMinValue(static_cast<float>(MinimumSplitCount));
	Slider_Amount->SetMaxValue(static_cast<float>(MaximumSplitCount));
	Slider_Amount->SetStepSize(1.0f);
	Slider_Amount->SetLocked(MinimumSplitCount == MaximumSplitCount);
	SpinBox_Amount->SetMinValue(static_cast<float>(MinimumSplitCount));
	SpinBox_Amount->SetMaxValue(static_cast<float>(MaximumSplitCount));
	SpinBox_Amount->SetMinSliderValue(static_cast<float>(MinimumSplitCount));
	SpinBox_Amount->SetMaxSliderValue(static_cast<float>(MaximumSplitCount));
	SpinBox_Amount->SetDelta(1.0f);
	SetSelectedSplitCount(InDefaultCount);
	RequestRefreshFocus();
	return true;
}

bool URpgInventorySplitDialogWidget::ConfirmSplitDialog()
{
	URpgInventorySpatialGridWidget* Grid = SourceGrid.Get();
	URpgInventoryAddressSlotWidget* AddressSlot = SourceAddressSlot.Get();
	const bool bGridSelectionStillMatches = bHasOpenSplitRequest && Grid && SplitEntryId.IsValid() &&
		Grid->GetSelectedEntryId() == SplitEntryId;
	const bool bAddressSelectionStillMatches = bHasOpenSplitRequest && AddressSlot && SplitItemId.IsValid();
	const bool bCountStillValid = SelectedSplitCount >= MinimumSplitCount && SelectedSplitCount <= MaximumSplitCount;
	if ((!bGridSelectionStillMatches && !bAddressSelectionStillMatches) || !bCountStillValid)
	{
		CancelSplitDialog();
		return false;
	}

	const bool bDispatched = Grid
		? Grid->ConfirmPendingSplit(SelectedSplitCount)
		: AddressSlot->ConfirmAddressSplit(SplitItemId, SelectedSplitCount);
	if (bDispatched)
	{
		// The grid clears its stable pending request after successful dispatch.
		bHasOpenSplitRequest = false;
		CloseSplitDialog();
		return true;
	}

	// A failed local dispatch must not leave a hidden pending request behind.
	CancelSplitDialog();
	return false;
}

void URpgInventorySplitDialogWidget::CancelSplitDialog()
{
	ResetSplitState(true);
	CloseSplitDialog();
}

void URpgInventorySplitDialogWidget::SetSelectedSplitCount(int32 InSplitCount)
{
	const int32 ClampedCount = FMath::Clamp(InSplitCount, MinimumSplitCount, MaximumSplitCount);
	SelectedSplitCount = ClampedCount;

	TGuardValue<bool> SynchronizingGuard(bSynchronizingAmountControls, true);
	if (Slider_Amount)
	{
		Slider_Amount->SetValue(static_cast<float>(SelectedSplitCount));
	}
	if (SpinBox_Amount)
	{
		SpinBox_Amount->SetValue(static_cast<float>(SelectedSplitCount));
	}
}

void URpgInventorySplitDialogWidget::EnsureSplitWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	Slider_Amount = Cast<USlider>(WidgetTree->FindWidget(TEXT("Slider_Amount")));
	SpinBox_Amount = Cast<USpinBox>(WidgetTree->FindWidget(TEXT("SpinBox_Amount")));
	Button_Confirm = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Confirm")));
	Button_Cancel = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Cancel")));

	if (!Slider_Amount || !SpinBox_Amount || !Button_Confirm || !Button_Cancel)
	{
		BuildNativeSplitWidgetTree();
	}
}

void URpgInventorySplitDialogWidget::BuildNativeSplitWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("NativeSplitRoot"));
	WidgetTree->RootWidget = RootOverlay;

	UButton* BackdropButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("NativeSplitBackdrop"));
	BackdropButton->SetBackgroundColor(FLinearColor::Transparent);
	BackdropButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	UBorder* BackdropShade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NativeSplitBackdropShade"));
	BackdropShade->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	BackdropButton->SetContent(BackdropShade);
	ConfigureOverlaySlot(RootOverlay->AddChildToOverlay(BackdropButton), HAlign_Fill, VAlign_Fill);

	UBorder* DialogBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NativeSplitDialogBorder"));
	DialogBorder->SetBrushColor(FLinearColor(0.045f, 0.045f, 0.05f, 0.98f));
	DialogBorder->SetPadding(FMargin(20.0f));
	ConfigureOverlaySlot(RootOverlay->AddChildToOverlay(DialogBorder), HAlign_Center, VAlign_Center);

	USizeBox* DialogSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NativeSplitDialogSize"));
	DialogSize->SetWidthOverride(NativePanelWidth);
	DialogBorder->SetContent(DialogSize);

	UVerticalBox* DialogContents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NativeSplitDialogContents"));
	DialogSize->SetContent(DialogContents);

	UTextBlock* Title = CreateNativeLabel(WidgetTree, LOCTEXT("SplitDialogTitle", "Split Stack"), 22.0f);
	if (UVerticalBoxSlot* TitleSlot = DialogContents->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UHorizontalBox* AmountRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NativeSplitAmountRow"));
	if (UVerticalBoxSlot* AmountRowSlot = DialogContents->AddChildToVerticalBox(AmountRow))
	{
		AmountRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	Slider_Amount = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider_Amount"));
	if (UHorizontalBoxSlot* SliderSlot = AmountRow->AddChildToHorizontalBox(Slider_Amount))
	{
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetPadding(FMargin(0.0f, 4.0f, 12.0f, 4.0f));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* SpinBoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NativeSplitSpinBoxSize"));
	SpinBoxSize->SetWidthOverride(88.0f);
	if (UHorizontalBoxSlot* SpinSizeSlot = AmountRow->AddChildToHorizontalBox(SpinBoxSize))
	{
		SpinSizeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		SpinSizeSlot->SetVerticalAlignment(VAlign_Center);
	}
	SpinBox_Amount = WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), TEXT("SpinBox_Amount"));
	SpinBox_Amount->SetMinFractionalDigits(0);
	SpinBox_Amount->SetMaxFractionalDigits(0);
	SpinBoxSize->SetContent(SpinBox_Amount);

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NativeSplitButtonRow"));
	DialogContents->AddChildToVerticalBox(ButtonRow);

	Button_Cancel = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Cancel"));
	Button_Cancel->SetContent(CreateNativeLabel(WidgetTree, LOCTEXT("CancelSplitButton", "Cancel")));
	if (UHorizontalBoxSlot* CancelSlot = ButtonRow->AddChildToHorizontalBox(Button_Cancel))
	{
		CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CancelSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}

	Button_Confirm = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Confirm"));
	Button_Confirm->SetContent(CreateNativeLabel(WidgetTree, LOCTEXT("ConfirmSplitButton", "Split")));
	if (UHorizontalBoxSlot* ConfirmSlot = ButtonRow->AddChildToHorizontalBox(Button_Confirm))
	{
		ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ConfirmSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	}
}

void URpgInventorySplitDialogWidget::BindSplitControls()
{
	if (Slider_Amount)
	{
		Slider_Amount->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
	}
	if (SpinBox_Amount)
	{
		SpinBox_Amount->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSpinBoxValueChanged);
	}
	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void URpgInventorySplitDialogWidget::CloseSplitDialog()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
	else
	{
		RemoveFromParent();
	}
}

void URpgInventorySplitDialogWidget::ResetSplitState(bool bCancelGridRequest)
{
	if (bCancelGridRequest && bHasOpenSplitRequest && SourceGrid)
	{
		SourceGrid->CancelPendingSplit();
	}

	bHasOpenSplitRequest = false;
	SourceGrid = nullptr;
	SourceAddressSlot = nullptr;
	SplitEntryId.Invalidate();
	SplitItemId = FRpgInventoryItemId();
	MinimumSplitCount = 1;
	MaximumSplitCount = 1;
	SelectedSplitCount = 1;
}

void URpgInventorySplitDialogWidget::HandleSliderValueChanged(float NewValue)
{
	if (!bSynchronizingAmountControls)
	{
		SetSelectedSplitCount(FMath::RoundToInt(NewValue));
	}
}

void URpgInventorySplitDialogWidget::HandleSpinBoxValueChanged(float NewValue)
{
	if (!bSynchronizingAmountControls)
	{
		SetSelectedSplitCount(FMath::RoundToInt(NewValue));
	}
}

void URpgInventorySplitDialogWidget::HandleConfirmClicked()
{
	ConfirmSplitDialog();
}

void URpgInventorySplitDialogWidget::HandleCancelClicked()
{
	CancelSplitDialog();
}

URpgInventoryContextMenuWidget::URpgInventoryContextMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true);
	ActionEntryWidgetClass = URpgInventoryContextActionEntryWidget::StaticClass();
	QuickAccessSlotEntryWidgetClass = URpgQuickAccessSlotPickerEntryWidget::StaticClass();
}

TOptional<FUIInputConfig> URpgInventoryContextMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void URpgInventoryContextMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureContextWidgetTree();
	BindDismissControl();
}

void URpgInventoryContextMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	bContextPositionPending = true;
	RequestRefreshFocus();
}

void URpgInventoryContextMenuWidget::NativeOnDeactivated()
{
	ResetContextState();
	Super::NativeOnDeactivated();
}

void URpgInventoryContextMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bContextPositionPending)
	{
		UpdateContextMenuPosition();
	}
}

bool URpgInventoryContextMenuWidget::NativeOnHandleBackAction()
{
	if (bShowingQuickAccessPicker)
	{
		ShowContextActionPage();
		return true;
	}
	CloseContextMenu();
	return true;
}

FReply URpgInventoryContextMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bShowingQuickAccessPicker)
		{
			ShowContextActionPage();
		}
		else
		{
			CloseContextMenu();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UWidget* URpgInventoryContextMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (bShowingQuickAccessPicker)
	{
		return QuickAccessSlotButtons.IsEmpty() ? nullptr : QuickAccessSlotButtons[0].Get();
	}
	return ActionButtons.IsEmpty() ? nullptr : ActionButtons[0].Get();
}

int32 URpgInventoryContextMenuWidget::ToQuickAccessDisplayNumber(int32 SlotIndex)
{
	return FMath::IsWithinInclusive(SlotIndex, 0, 7) ? SlotIndex + 1 : INDEX_NONE;
}

int32 URpgInventoryContextMenuWidget::ToQuickAccessSlotIndex(int32 DisplaySlotNumber)
{
	return FMath::IsWithinInclusive(DisplaySlotNumber, 1, 8) ? DisplaySlotNumber - 1 : INDEX_NONE;
}

bool URpgInventoryContextMenuWidget::InitializeContextMenu(
	URpgInventorySpatialGridWidget* InSourceGrid,
	const TArray<ERpgInventoryContextAction>& InActions,
	FVector2D InScreenPosition)
{
	EnsureContextWidgetTree();
	BindDismissControl();

	const FGuid SelectedEntryId = InSourceGrid ? InSourceGrid->GetSelectedEntryId() : FGuid();
	if (!InSourceGrid || !SelectedEntryId.IsValid() || InActions.IsEmpty() || !ActionsBox || !ContextMenuCanvas || !ContextMenuBorder)
	{
		return false;
	}

	SourceGrid = InSourceGrid;
	SourceEquipmentSlot = nullptr;
	SourceAddressSlot = nullptr;
	ContextEntryId = SelectedEntryId;
	ContextItemId = FRpgInventoryItemId();
	RequestedScreenPosition = InScreenPosition;
	ContextActions.Reset(InActions.Num());
	for (ERpgInventoryContextAction Action : InActions)
	{
		ContextActions.AddUnique(Action);
	}

	NormalizeQuickAccessActions();
	RebuildActionButtons();
	if (ActionButtons.IsEmpty())
	{
		ResetContextState();
		return false;
	}

	bContextPositionPending = true;
	RequestRefreshFocus();
	return true;
}

bool URpgInventoryContextMenuWidget::InitializeEquipmentContextMenu(
	URpgEquipmentSlotWidget* InSourceEquipmentSlot,
	const TArray<ERpgInventoryContextAction>& InActions,
	FVector2D InScreenPosition)
{
	EnsureContextWidgetTree();
	BindDismissControl();

	const URpgInventoryItemInstance* RepresentedItem = InSourceEquipmentSlot
		? InSourceEquipmentSlot->GetRepresentedItem()
		: nullptr;
	const FRpgInventoryItemId RepresentedItemId = RepresentedItem
		? RepresentedItem->GetItemId()
		: FRpgInventoryItemId();
	if (!InSourceEquipmentSlot || !RepresentedItemId.IsValid() || InActions.IsEmpty() ||
		!ActionsBox || !ContextMenuCanvas || !ContextMenuBorder)
	{
		return false;
	}

	SourceGrid = nullptr;
	SourceEquipmentSlot = InSourceEquipmentSlot;
	SourceAddressSlot = nullptr;
	ContextEntryId.Invalidate();
	ContextItemId = RepresentedItemId;
	RequestedScreenPosition = InScreenPosition;
	ContextActions.Reset(InActions.Num());
	for (ERpgInventoryContextAction Action : InActions)
	{
		ContextActions.AddUnique(Action);
	}

	NormalizeQuickAccessActions();
	RebuildActionButtons();
	if (ActionButtons.IsEmpty())
	{
		ResetContextState();
		return false;
	}

	bContextPositionPending = true;
	RequestRefreshFocus();
	return true;
}

bool URpgInventoryContextMenuWidget::InitializeAddressContextMenu(
	URpgInventoryAddressSlotWidget* InSourceAddressSlot,
	const TArray<ERpgInventoryContextAction>& InActions,
	FVector2D InScreenPosition)
{
	EnsureContextWidgetTree();
	BindDismissControl();
	const URpgInventoryAddressSlotViewModel* AddressViewModel = InSourceAddressSlot
		? InSourceAddressSlot->GetAddressSlotViewModel()
		: nullptr;
	const URpgInventoryItemInstance* Item = AddressViewModel ? AddressViewModel->GetItemInstance() : nullptr;
	if (!InSourceAddressSlot || !Item || !Item->GetItemId().IsValid() || InActions.IsEmpty() ||
		!ActionsBox || !ContextMenuCanvas || !ContextMenuBorder)
	{
		return false;
	}

	SourceGrid = nullptr;
	SourceEquipmentSlot = nullptr;
	SourceAddressSlot = InSourceAddressSlot;
	ContextEntryId.Invalidate();
	ContextItemId = Item->GetItemId();
	RequestedScreenPosition = InScreenPosition;
	ContextActions.Reset(InActions.Num());
	for (ERpgInventoryContextAction Action : InActions)
	{
		ContextActions.AddUnique(Action);
	}
	NormalizeQuickAccessActions();
	RebuildActionButtons();
	if (ActionButtons.IsEmpty())
	{
		ResetContextState();
		return false;
	}

	bContextPositionPending = true;
	RequestRefreshFocus();
	return true;
}

bool URpgInventoryContextMenuWidget::ExecuteContextAction(ERpgInventoryContextAction Action)
{
	const bool bActionWasDisplayed = ContextActions.Contains(Action);
	if (!bActionWasDisplayed)
	{
		CloseContextMenu();
		return false;
	}
	if (Action == ERpgInventoryContextAction::QuickAccessBind)
	{
		return ShowQuickAccessSlotPicker();
	}

	if (URpgInventorySpatialGridWidget* Grid = SourceGrid.Get())
	{
		if (!ContextEntryId.IsValid() || Grid->GetSelectedEntryId() != ContextEntryId)
		{
			CloseContextMenu();
			return false;
		}

		// Remove this modal before an action such as Split opens the next modal on the same CommonUI layer.
		CloseContextMenu();
		return Grid->ExecuteSelectedContextAction(Action);
	}
	if (URpgInventoryAddressSlotWidget* AddressSlot = SourceAddressSlot.Get())
	{
		const FRpgInventoryItemId ExpectedItemId = ContextItemId;
		CloseContextMenu();
		return AddressSlot->ExecuteAddressContextAction(Action, ExpectedItemId);
	}

	TWeakObjectPtr<URpgEquipmentSlotWidget> EquipmentSlot(SourceEquipmentSlot.Get());
	const FRpgInventoryItemId ExpectedItemId = ContextItemId;
	const URpgInventoryItemInstance* CurrentItem = EquipmentSlot.IsValid()
		? EquipmentSlot->GetRepresentedItem()
		: nullptr;
	if (!ExpectedItemId.IsValid() || !CurrentItem || CurrentItem->GetItemId() != ExpectedItemId)
	{
		CloseContextMenu();
		return false;
	}

	CloseContextMenu();
	return EquipmentSlot.IsValid() && EquipmentSlot->ExecuteEquipmentContextAction(Action, ExpectedItemId);
}

bool URpgInventoryContextMenuWidget::ShowQuickAccessSlotPicker()
{
	if (!ContextActions.Contains(ERpgInventoryContextAction::QuickAccessBind) ||
		(!SourceGrid && !SourceAddressSlot))
	{
		return false;
	}

	if (SourceGrid && (!ContextEntryId.IsValid() || SourceGrid->GetSelectedEntryId() != ContextEntryId))
	{
		CloseContextMenu();
		return false;
	}
	if (SourceAddressSlot)
	{
		const URpgInventoryAddressSlotViewModel* AddressViewModel = SourceAddressSlot->GetAddressSlotViewModel();
		const URpgInventoryItemInstance* CurrentItem = AddressViewModel ? AddressViewModel->GetItemInstance() : nullptr;
		if (!CurrentItem || CurrentItem->GetItemId() != ContextItemId)
		{
			CloseContextMenu();
			return false;
		}
	}

	bShowingQuickAccessPicker = true;
	RebuildQuickAccessSlotButtons();
	if (QuickAccessSlotButtons.Num() != 8)
	{
		bShowingQuickAccessPicker = false;
		ShowContextActionPage();
		return false;
	}

	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		ActionsBox->SetVisibility(ESlateVisibility::Collapsed);
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Visible);
	}

	// The picker has a different desired height than the action page. Re-clamp on the
	// following tick, after Slate has incorporated the rebuilt rows into its layout.
	bContextPositionPending = true;
	RequestRefreshFocus();
	return true;
}

void URpgInventoryContextMenuWidget::ShowContextActionPage()
{
	bShowingQuickAccessPicker = false;
	QuickAccessSlotButtons.Reset();
	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		QuickAccessSlotsBox->ClearChildren();
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
		ActionsBox->SetVisibility(ESlateVisibility::Visible);
	}
	RebuildActionButtons();
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Returning to the shorter action page changes the menu's desired size as well.
	// Defer the viewport clamp until the rebuilt page has completed a layout pass.
	bContextPositionPending = true;
	RequestRefreshFocus();
}

bool URpgInventoryContextMenuWidget::SelectQuickAccessSlot(int32 SlotIndex)
{
	if (!bShowingQuickAccessPicker || !FMath::IsWithinInclusive(SlotIndex, 0, 7))
	{
		return false;
	}

	bool bDispatched = false;
	if (URpgInventorySpatialGridWidget* Grid = SourceGrid.Get())
	{
		bDispatched = ContextEntryId.IsValid() && Grid->GetSelectedEntryId() == ContextEntryId &&
			Grid->ExecuteSelectedContextAction(ERpgInventoryContextAction::QuickAccessBind, 0, SlotIndex);
	}
	else if (URpgInventoryAddressSlotWidget* AddressSlot = SourceAddressSlot.Get())
	{
		bDispatched = AddressSlot->ExecuteAddressContextAction(
			ERpgInventoryContextAction::QuickAccessBind,
			ContextItemId,
			SlotIndex);
	}

	if (bDispatched)
	{
		CloseContextMenu();
	}
	return bDispatched;
}

void URpgInventoryContextMenuWidget::CloseContextMenu()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
	else
	{
		ResetContextState();
		RemoveFromParent();
	}
}

void URpgInventoryContextMenuWidget::EnsureContextWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	Button_Dismiss = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Dismiss")));
	ContextMenuCanvas = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("ContextMenuCanvas")));
	ContextMenuBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("ContextMenuBorder")));
	ActionsBox = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("ActionsBox")));
	QuickAccessSlotsBox = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("QuickAccessSlotsBox")));
	Button_QuickAccessBack = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_QuickAccessBack")));

	if (!Button_Dismiss || !ContextMenuCanvas || !ContextMenuBorder || !ActionsBox)
	{
		BuildNativeContextWidgetTree();
	}
	if (QuickAccessSlotsBox && !bShowingQuickAccessPicker)
	{
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_QuickAccessBack && !bShowingQuickAccessPicker)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URpgInventoryContextMenuWidget::BuildNativeContextWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("NativeContextMenuRoot"));
	WidgetTree->RootWidget = RootOverlay;

	Button_Dismiss = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Dismiss"));
	Button_Dismiss->SetBackgroundColor(FLinearColor::Transparent);
	ConfigureOverlaySlot(RootOverlay->AddChildToOverlay(Button_Dismiss), HAlign_Fill, VAlign_Fill);

	ContextMenuCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ContextMenuCanvas"));
	ContextMenuCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ConfigureOverlaySlot(RootOverlay->AddChildToOverlay(ContextMenuCanvas), HAlign_Fill, VAlign_Fill);

	ContextMenuBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContextMenuBorder"));
	ContextMenuBorder->SetBrushColor(FLinearColor(0.035f, 0.035f, 0.04f, 0.98f));
	ContextMenuBorder->SetPadding(FMargin(8.0f));
	if (UCanvasPanelSlot* MenuSlot = ContextMenuCanvas->AddChildToCanvas(ContextMenuBorder))
	{
		MenuSlot->SetAutoSize(true);
		MenuSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		MenuSlot->SetAlignment(FVector2D::ZeroVector);
	}

	USizeBox* MenuSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NativeContextMenuSize"));
	MenuSize->SetWidthOverride(NativeContextMenuWidth);
	ContextMenuBorder->SetContent(MenuSize);

	UVerticalBox* PageContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NativeContextPageContainer"));
	MenuSize->SetContent(PageContainer);

	ActionsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ActionsBox"));
	PageContainer->AddChildToVerticalBox(ActionsBox);

	QuickAccessSlotsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuickAccessSlotsBox"));
	QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	PageContainer->AddChildToVerticalBox(QuickAccessSlotsBox);

	Button_QuickAccessBack = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_QuickAccessBack"));
	Button_QuickAccessBack->SetContent(CreateNativeLabel(WidgetTree, LOCTEXT("QuickAccessBackButton", "Back")));
	Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* BackSlot = PageContainer->AddChildToVerticalBox(Button_QuickAccessBack))
	{
		BackSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void URpgInventoryContextMenuWidget::BindDismissControl()
{
	if (Button_Dismiss)
	{
		Button_Dismiss->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDismissClicked);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuickAccessBackClicked);
	}
}

void URpgInventoryContextMenuWidget::RebuildActionButtons()
{
	ActionButtons.Reset();
	if (!ActionsBox || !WidgetTree)
	{
		return;
	}

	bShowingQuickAccessPicker = false;
	ActionsBox->SetVisibility(ESlateVisibility::Visible);
	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		QuickAccessSlotsBox->ClearChildren();
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActionsBox->ClearChildren();
	for (ERpgInventoryContextAction Action : ContextActions)
	{
		TSubclassOf<URpgInventoryContextActionEntryWidget> EntryClass = ActionEntryWidgetClass;
		if (!EntryClass)
		{
			EntryClass = URpgInventoryContextActionEntryWidget::StaticClass();
		}
		URpgInventoryContextActionEntryWidget* ActionButton = WidgetTree->ConstructWidget<URpgInventoryContextActionEntryWidget>(EntryClass);
		if (!ActionButton)
		{
			continue;
		}
		ActionButton->InitializeContextAction(this, Action, ResolveContextActionLabel(Action));
		if (UVerticalBoxSlot* ActionSlot = ActionsBox->AddChildToVerticalBox(ActionButton))
		{
			ActionSlot->SetPadding(FMargin(0.0f, 2.0f));
			ActionSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		ActionButtons.Add(ActionButton);
	}
}

void URpgInventoryContextMenuWidget::RebuildQuickAccessSlotButtons()
{
	QuickAccessSlotButtons.Reset();
	UVerticalBox* PickerHost = QuickAccessSlotsBox ? QuickAccessSlotsBox.Get() : ActionsBox.Get();
	if (!PickerHost || !WidgetTree)
	{
		return;
	}

	PickerHost->ClearChildren();
	const int32 CurrentSlotIndex = ResolveCurrentQuickAccessSlotIndex();
	for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
	{
		bool bOccupied = false;
		const FText BindingLabel = ResolveQuickAccessBindingLabel(SlotIndex, bOccupied);
		TSubclassOf<URpgQuickAccessSlotPickerEntryWidget> EntryClass = QuickAccessSlotEntryWidgetClass;
		if (!EntryClass)
		{
			EntryClass = URpgQuickAccessSlotPickerEntryWidget::StaticClass();
		}
		URpgQuickAccessSlotPickerEntryWidget* SlotButton =
			WidgetTree->ConstructWidget<URpgQuickAccessSlotPickerEntryWidget>(EntryClass);
		if (!SlotButton)
		{
			continue;
		}
		SlotButton->InitializeQuickAccessSlot(this, SlotIndex, BindingLabel, bOccupied, SlotIndex == CurrentSlotIndex);
		if (UVerticalBoxSlot* PickerSlot = PickerHost->AddChildToVerticalBox(SlotButton))
		{
			PickerSlot->SetPadding(FMargin(0.0f, 2.0f));
			PickerSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		QuickAccessSlotButtons.Add(SlotButton);
	}

	if (!Button_QuickAccessBack)
	{
		Button_QuickAccessBack = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("NativeQuickAccessBackButton"));
		Button_QuickAccessBack->SetContent(CreateNativeLabel(WidgetTree, LOCTEXT("QuickAccessBackButton", "Back")));
	}
	if (Button_QuickAccessBack && !Button_QuickAccessBack->GetParent())
	{
		if (UVerticalBoxSlot* BackSlot = PickerHost->AddChildToVerticalBox(Button_QuickAccessBack))
		{
			BackSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
			BackSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuickAccessBackClicked);
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Visible);
	}
}

void URpgInventoryContextMenuWidget::NormalizeQuickAccessActions()
{
	const bool bHasBindableSource = SourceGrid != nullptr || SourceAddressSlot != nullptr;
	if (!bHasBindableSource)
	{
		ContextActions.Remove(ERpgInventoryContextAction::QuickAccessBind);
		ContextActions.Remove(ERpgInventoryContextAction::QuickAccessUnbind);
		return;
	}

	if (ResolveCurrentQuickAccessSlotIndex() == INDEX_NONE)
	{
		ContextActions.Remove(ERpgInventoryContextAction::QuickAccessUnbind);
	}
}

FText URpgInventoryContextMenuWidget::ResolveContextActionLabel(ERpgInventoryContextAction Action) const
{
	const int32 CurrentSlotIndex = ResolveCurrentQuickAccessSlotIndex();
	if (Action == ERpgInventoryContextAction::QuickAccessBind && CurrentSlotIndex != INDEX_NONE)
	{
		return LOCTEXT("QuickAccessChangeAction", "Change Quick Access Slot");
	}
	if (Action == ERpgInventoryContextAction::QuickAccessUnbind && CurrentSlotIndex != INDEX_NONE)
	{
		return FText::Format(
			LOCTEXT("QuickAccessUnbindSlotAction", "Unbind Quick Access ({0})"),
			FText::AsNumber(ToQuickAccessDisplayNumber(CurrentSlotIndex)));
	}
	return GetContextActionLabel(Action);
}

int32 URpgInventoryContextMenuWidget::ResolveCurrentQuickAccessSlotIndex() const
{
	if (const URpgInventorySpatialGridWidget* Grid = SourceGrid.Get())
	{
		return Grid->GetSelectedQuickAccessSlotIndex();
	}
	if (const URpgInventoryAddressSlotWidget* AddressSlot = SourceAddressSlot.Get())
	{
		return AddressSlot->GetQuickAccessSlotIndex();
	}
	return INDEX_NONE;
}

FText URpgInventoryContextMenuWidget::ResolveQuickAccessBindingLabel(int32 SlotIndex, bool& bOutOccupied) const
{
	bOutOccupied = false;
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(GetOwningPlayer());
	const URpgActionBarComponent* ActionBar = RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr;
	if (!ActionBar || !FMath::IsWithinInclusive(SlotIndex, 0, 7))
	{
		return LOCTEXT("EmptyQuickAccessSlot", "Empty");
	}

	const FRpgActionBarSlot ActionBarSlot = ActionBar->GetSlot(SlotIndex);
	bOutOccupied = !ActionBarSlot.IsEmpty();
	switch (ActionBarSlot.SlotType)
	{
	case ERpgActionBarSlotType::Consumable:
	case ERpgActionBarSlotType::InventorySlotBinding:
		return ActionBarSlot.ConsumableDefinition
			? ActionBarSlot.ConsumableDefinition->GetDisplayNameText()
			: LOCTEXT("MissingConsumableQuickAccessSlot", "Missing Consumable");
	case ERpgActionBarSlotType::CarrySlot:
	case ERpgActionBarSlotType::CarrySlotBinding:
		return ActionBarSlot.CarryRole.IsNone()
			? LOCTEXT("MissingCarryQuickAccessSlot", "Missing Carry Role")
			: FText::FromName(ActionBarSlot.CarryRole);
	case ERpgActionBarSlotType::Ability:
		return ActionBarSlot.AbilityId.IsValid()
			? FText::FromString(ActionBarSlot.AbilityId.ToString())
			: LOCTEXT("MissingAbilityQuickAccessSlot", "Missing Ability");
	case ERpgActionBarSlotType::Empty:
	default:
		bOutOccupied = false;
		return LOCTEXT("EmptyQuickAccessSlot", "Empty");
	}
}

void URpgInventoryContextMenuWidget::UpdateContextMenuPosition()
{
	if (!ContextMenuCanvas || !ContextMenuBorder)
	{
		bContextPositionPending = false;
		return;
	}

	const FGeometry& RootGeometry = GetCachedGeometry();
	const FVector2D RootSize = RootGeometry.GetLocalSize();
	if (RootSize.X <= 1.0f || RootSize.Y <= 1.0f)
	{
		return;
	}

	ForceLayoutPrepass();
	const FVector2D MenuSize = ContextMenuBorder->GetDesiredSize();
	FVector2D LocalPosition = RootGeometry.AbsoluteToLocal(RequestedScreenPosition);
	LocalPosition.X = FMath::Clamp(LocalPosition.X, 0.0f, FMath::Max(0.0f, RootSize.X - MenuSize.X));
	LocalPosition.Y = FMath::Clamp(LocalPosition.Y, 0.0f, FMath::Max(0.0f, RootSize.Y - MenuSize.Y));

	if (UCanvasPanelSlot* MenuSlot = Cast<UCanvasPanelSlot>(ContextMenuBorder->Slot))
	{
		MenuSlot->SetPosition(LocalPosition);
		bContextPositionPending = false;
	}
}

void URpgInventoryContextMenuWidget::ResetContextState()
{
	if (ActionsBox)
	{
		ActionsBox->SetVisibility(ESlateVisibility::Visible);
		ActionsBox->ClearChildren();
	}
	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		QuickAccessSlotsBox->ClearChildren();
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}
	SourceGrid = nullptr;
	SourceEquipmentSlot = nullptr;
	SourceAddressSlot = nullptr;
	ContextEntryId.Invalidate();
	ContextItemId = FRpgInventoryItemId();
	ContextActions.Reset();
	ActionButtons.Reset();
	QuickAccessSlotButtons.Reset();
	RequestedScreenPosition = FVector2D::ZeroVector;
	bContextPositionPending = false;
	bShowingQuickAccessPicker = false;
}

void URpgInventoryContextMenuWidget::HandleContextActionClicked(ERpgInventoryContextAction Action)
{
	ExecuteContextAction(Action);
}

void URpgInventoryContextMenuWidget::HandleDismissClicked()
{
	CloseContextMenu();
}

void URpgInventoryContextMenuWidget::HandleOpenContainerClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::OpenContainer);
}

void URpgInventoryContextMenuWidget::HandleInspectClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::Inspect);
}

void URpgInventoryContextMenuWidget::HandleUnequipClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::Unequip);
}

void URpgInventoryContextMenuWidget::HandleUseClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::Use);
}

void URpgInventoryContextMenuWidget::HandleEquipAndActivateClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::EquipAndActivate);
}

void URpgInventoryContextMenuWidget::HandleMoveToCarryClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::MoveToCarry);
}

void URpgInventoryContextMenuWidget::HandleSplitClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::Split);
}

void URpgInventoryContextMenuWidget::HandleRotateClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::Rotate);
}

void URpgInventoryContextMenuWidget::HandleQuickAccessBindClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::QuickAccessBind);
}

void URpgInventoryContextMenuWidget::HandleQuickAccessUnbindClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::QuickAccessUnbind);
}

void URpgInventoryContextMenuWidget::HandleQuickAccessBackClicked()
{
	ShowContextActionPage();
}

void URpgInventoryContextMenuWidget::HandleTransferClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::Transfer);
}

void URpgInventoryContextMenuWidget::HandleDropClicked()
{
	HandleContextActionClicked(ERpgInventoryContextAction::Drop);
}

#undef LOCTEXT_NAMESPACE
