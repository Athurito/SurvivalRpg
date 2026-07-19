#include "RpgInventoryActionWidgets.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryUiGeometry.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryActionWidgets)

#define LOCTEXT_NAMESPACE "RpgInventoryActionWidgets"

namespace
{
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
}

URpgInventoryDropConfirmationDialogWidget::URpgInventoryDropConfirmationDialogWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true);
}

TOptional<FUIInputConfig> URpgInventoryDropConfirmationDialogWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void URpgInventoryDropConfirmationDialogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindDropConfirmationControls();
}

void URpgInventoryDropConfirmationDialogWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RequestRefreshFocus();
}

void URpgInventoryDropConfirmationDialogWidget::NativeOnDeactivated()
{
	ResetDropConfirmationState(true);
	Super::NativeOnDeactivated();
}

void URpgInventoryDropConfirmationDialogWidget::NativeDestruct()
{
	ResetDropConfirmationState(true);
	Super::NativeDestruct();
}

bool URpgInventoryDropConfirmationDialogWidget::NativeOnHandleBackAction()
{
	CancelDropConfirmation();
	return true;
}

FReply URpgInventoryDropConfirmationDialogWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		CancelDropConfirmation();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter ||
		Key == EKeys::SpaceBar ||
		Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		ConfirmDropConfirmation();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UWidget* URpgInventoryDropConfirmationDialogWidget::NativeGetDesiredFocusTarget() const
{
	return Button_Confirm;
}

bool URpgInventoryDropConfirmationDialogWidget::InitializeDropConfirmation(
	URpgInventoryInteractionScreenWidget* InInteractionScreen,
	FGuid InInitialRequestId,
	const FText& InItemName,
	int32 InStackCount)
{
	ResetDropConfirmationState(true);
	BindDropConfirmationControls();

	const bool bHasCompleteAuthoredControls =
		Button_Backdrop && Text_Message && Button_Confirm && Button_Cancel;
	if (!InInteractionScreen ||
		!InInitialRequestId.IsValid() ||
		InStackCount <= 0 ||
		!bHasCompleteAuthoredControls)
	{
		if (InInteractionScreen && InInitialRequestId.IsValid())
		{
			InInteractionScreen->CancelPendingInventoryDrop(InInitialRequestId);
		}
		return false;
	}

	InteractionScreenHost = InInteractionScreen;
	InitialRequestId = InInitialRequestId;
	bHasOpenDropConfirmation = true;
	Text_Message->SetText(BuildDropConfirmationMessage(InItemName, InStackCount));
	Button_Confirm->SetIsEnabled(true);
	RequestRefreshFocus();
	return true;
}

bool URpgInventoryDropConfirmationDialogWidget::ConfirmDropConfirmation()
{
	URpgInventoryInteractionScreenWidget* Host = InteractionScreenHost.Get();
	const FGuid RequestId = InitialRequestId;
	if (!bHasOpenDropConfirmation || !Host || !RequestId.IsValid())
	{
		CancelDropConfirmation();
		return false;
	}

	// Consume the modal before calling out. A double click, key repeat, re-entrant screen close, or pooled
	// deactivation can therefore never dispatch the same confirmed request more than once.
	bHasOpenDropConfirmation = false;
	InteractionScreenHost.Reset();
	InitialRequestId.Invalidate();
	if (Button_Confirm)
	{
		Button_Confirm->SetIsEnabled(false);
	}

	const bool bDispatched = Host->ConfirmPendingInventoryDrop(RequestId);
	CloseDropConfirmation();
	return bDispatched;
}

void URpgInventoryDropConfirmationDialogWidget::CancelDropConfirmation()
{
	ResetDropConfirmationState(true);
	CloseDropConfirmation();
}

void URpgInventoryDropConfirmationDialogWidget::BindDropConfirmationControls()
{
	if (Button_Backdrop)
	{
		Button_Backdrop->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
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

void URpgInventoryDropConfirmationDialogWidget::CloseDropConfirmation()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
	else
	{
		ResetDropConfirmationState(true);
		RemoveFromParent();
	}
}

void URpgInventoryDropConfirmationDialogWidget::ResetDropConfirmationState(bool bNotifyHost)
{
	if (bResettingDropConfirmationState)
	{
		return;
	}

	TGuardValue<bool> ResetGuard(bResettingDropConfirmationState, true);
	TWeakObjectPtr<URpgInventoryInteractionScreenWidget> Host = InteractionScreenHost;
	const FGuid RequestId = InitialRequestId;

	// Clear local state before notifying the host because cancel can synchronously close or pool this modal.
	bHasOpenDropConfirmation = false;
	InteractionScreenHost.Reset();
	InitialRequestId.Invalidate();
	if (Text_Message)
	{
		Text_Message->SetText(FText::GetEmpty());
	}
	if (Button_Confirm)
	{
		Button_Confirm->SetIsEnabled(true);
	}

	if (bNotifyHost && Host.IsValid() && RequestId.IsValid())
	{
		Host->CancelPendingInventoryDrop(RequestId);
	}
}

FText URpgInventoryDropConfirmationDialogWidget::BuildDropConfirmationMessage(
	const FText& InItemName,
	int32 InStackCount) const
{
	const FText DisplayName = InItemName.IsEmpty()
		? LOCTEXT("UnknownDropItemName", "this item")
		: InItemName;
	if (InStackCount == 1)
	{
		return FText::Format(
			LOCTEXT("ConfirmSingleDropMessage", "Drop {0}?"),
			DisplayName);
	}

	return FText::Format(
		LOCTEXT("ConfirmStackDropMessage", "Drop {0} x {1}?"),
		FText::AsNumber(InStackCount),
		DisplayName);
}

void URpgInventoryDropConfirmationDialogWidget::HandleConfirmClicked()
{
	ConfirmDropConfirmation();
}

void URpgInventoryDropConfirmationDialogWidget::HandleCancelClicked()
{
	CancelDropConfirmation();
}

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
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		CancelSplitDialog();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter ||
		Key == EKeys::SpaceBar ||
		Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		ConfirmSplitDialog();
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

	BindSplitControls();

	const bool bValidRange = InMinimumCount >= 1 && InMaximumCount >= InMinimumCount;
	const bool bValidSelection = InSourceGrid && InEntryId.IsValid() && InSourceGrid->GetSelectedEntryId() == InEntryId;
	if (!bValidRange || !bValidSelection || !Button_Backdrop || !Slider_Amount ||
		!SpinBox_Amount || !Button_Confirm || !Button_Cancel)
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

	BindSplitControls();
	const URpgInventoryAddressSlotViewModel* AddressViewModel = InSourceAddressSlot
		? InSourceAddressSlot->GetAddressSlotViewModel()
		: nullptr;
	const URpgInventoryItemInstance* CurrentItem = AddressViewModel ? AddressViewModel->GetItemInstance() : nullptr;
	const bool bValidRange = InMinimumCount >= 1 && InMaximumCount >= InMinimumCount;
	if (!bValidRange || !InItemId.IsValid() || !CurrentItem || CurrentItem->GetItemId() != InItemId ||
		!Button_Backdrop || !Slider_Amount || !SpinBox_Amount || !Button_Confirm ||
		!Button_Cancel)
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

void URpgInventorySplitDialogWidget::BindSplitControls()
{
	if (Button_Backdrop)
	{
		Button_Backdrop->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
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
}

TOptional<FUIInputConfig> URpgInventoryContextMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void URpgInventoryContextMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BindDismissControl();
	if (QuickAccessSlotsBox)
	{
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}
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
	BindDismissControl();

	const FGuid SelectedEntryId = InSourceGrid ? InSourceGrid->GetSelectedEntryId() : FGuid();
	const FRpgInventoryItemId SelectedItemId = InSourceGrid
		? InSourceGrid->GetSelectedItemId()
		: FRpgInventoryItemId();
	if (!InSourceGrid || !SelectedEntryId.IsValid() || !SelectedItemId.IsValid() ||
		InActions.IsEmpty() ||
		!Button_Dismiss || !ContextMenuCanvas || !ContextMenuBorder || !ActionsBox ||
		!QuickAccessSlotsBox || !Button_QuickAccessBack || !ActionEntryWidgetClass ||
		!QuickAccessSlotEntryWidgetClass)
	{
		return false;
	}

	SourceGrid = InSourceGrid;
	SourceEquipmentSlot = nullptr;
	SourceAddressSlot = nullptr;
	ContextEntryId = SelectedEntryId;
	ContextItemId = SelectedItemId;
	RequestedScreenPosition = InScreenPosition;
	ContextActions.Reset(InActions.Num());
	for (ERpgInventoryContextAction Action : InActions)
	{
		ContextActions.AddUnique(Action);
	}

	ContextQuickAccessSlotIndex = ResolveCurrentQuickAccessSlotIndex();
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
	BindDismissControl();

	const URpgInventoryItemInstance* RepresentedItem = InSourceEquipmentSlot
		? InSourceEquipmentSlot->GetRepresentedItem()
		: nullptr;
	const FRpgInventoryItemId RepresentedItemId = RepresentedItem
		? RepresentedItem->GetItemId()
		: FRpgInventoryItemId();
	if (!InSourceEquipmentSlot || !RepresentedItemId.IsValid() || InActions.IsEmpty() ||
		!Button_Dismiss || !ActionsBox || !QuickAccessSlotsBox ||
		!Button_QuickAccessBack || !ContextMenuCanvas || !ContextMenuBorder ||
		!ActionEntryWidgetClass || !QuickAccessSlotEntryWidgetClass)
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

	ContextQuickAccessSlotIndex = INDEX_NONE;
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
	BindDismissControl();
	const URpgInventoryAddressSlotViewModel* AddressViewModel = InSourceAddressSlot
		? InSourceAddressSlot->GetAddressSlotViewModel()
		: nullptr;
	const URpgInventoryItemInstance* Item = AddressViewModel ? AddressViewModel->GetItemInstance() : nullptr;
	if (!InSourceAddressSlot || !Item || !Item->GetItemId().IsValid() || InActions.IsEmpty() ||
		!Button_Dismiss || !ActionsBox || !QuickAccessSlotsBox ||
		!Button_QuickAccessBack || !ContextMenuCanvas || !ContextMenuBorder ||
		!ActionEntryWidgetClass || !QuickAccessSlotEntryWidgetClass)
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
	ContextQuickAccessSlotIndex = ResolveCurrentQuickAccessSlotIndex();
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
	if (Action == ERpgInventoryContextAction::QuickAccessUnbind &&
		(ContextQuickAccessSlotIndex == INDEX_NONE ||
			ResolveCurrentQuickAccessSlotIndex() != ContextQuickAccessSlotIndex))
	{
		CloseContextMenu();
		return false;
	}

	if (URpgInventorySpatialGridWidget* Grid = SourceGrid.Get())
	{
		if (!ContextEntryId.IsValid() || Grid->GetSelectedEntryId() != ContextEntryId ||
			Grid->GetSelectedItemId() != ContextItemId)
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

	if (SourceGrid)
	{
		if (!ContextEntryId.IsValid() || SourceGrid->GetSelectedEntryId() != ContextEntryId ||
			SourceGrid->GetSelectedItemId() != ContextItemId ||
			!SourceGrid->GetSelectedContextActions().Contains(
				ERpgInventoryContextAction::QuickAccessBind))
		{
			CloseContextMenu();
			return false;
		}
	}
	if (SourceAddressSlot)
	{
		const URpgInventoryAddressSlotViewModel* AddressViewModel = SourceAddressSlot->GetAddressSlotViewModel();
		const URpgInventoryItemInstance* CurrentItem = AddressViewModel ? AddressViewModel->GetItemInstance() : nullptr;
		if (!CurrentItem || CurrentItem->GetItemId() != ContextItemId ||
			!SourceAddressSlot->GetAddressContextActions().Contains(
				ERpgInventoryContextAction::QuickAccessBind))
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
			Grid->GetSelectedItemId() == ContextItemId &&
			Grid->ExecuteSelectedContextAction(ERpgInventoryContextAction::QuickAccessBind, 0, SlotIndex);
	}
	else if (URpgInventoryAddressSlotWidget* AddressSlot = SourceAddressSlot.Get())
	{
		bDispatched = AddressSlot->ExecuteAddressContextAction(
			ERpgInventoryContextAction::QuickAccessBind,
			ContextItemId,
			SlotIndex);
	}

	// A picker click consumes this modal even when the source became stale while the page was open.
	CloseContextMenu();
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
	if (!ActionsBox || !WidgetTree || !ActionEntryWidgetClass)
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
		URpgInventoryContextActionEntryWidget* ActionButton =
			WidgetTree->ConstructWidget<URpgInventoryContextActionEntryWidget>(
				ActionEntryWidgetClass);
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
	UVerticalBox* PickerHost = QuickAccessSlotsBox.Get();
	if (!PickerHost || !WidgetTree || !QuickAccessSlotEntryWidgetClass)
	{
		return;
	}

	PickerHost->ClearChildren();
	const int32 CurrentSlotIndex = ResolveCurrentQuickAccessSlotIndex();
	for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
	{
		bool bOccupied = false;
		const FText BindingLabel = ResolveQuickAccessBindingLabel(SlotIndex, bOccupied);
		URpgQuickAccessSlotPickerEntryWidget* SlotButton =
			WidgetTree->ConstructWidget<URpgQuickAccessSlotPickerEntryWidget>(
				QuickAccessSlotEntryWidgetClass);
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

	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuickAccessBackClicked);
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Visible);
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
		if (const URpgInventoryItemDefinition* Definition =
			ActionBarSlot.ConsumableDefinition.GetDefaultObject())
		{
			return Definition->DisplayName;
		}
		return LOCTEXT("MissingConsumableQuickAccessSlot", "Missing Consumable");
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

	ForceLayoutPrepass();
	const FVector2D MenuSize = ContextMenuBorder->GetDesiredSize();
	FVector2D LocalPosition;
	if (!RpgInventoryUiGeometry::TryResolveClampedMenuCanvasPosition(
		ContextMenuCanvas->GetCachedGeometry(),
		RequestedScreenPosition,
		MenuSize,
		LocalPosition))
	{
		return;
	}

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
	ContextQuickAccessSlotIndex = INDEX_NONE;
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

void URpgInventoryContextMenuWidget::HandleQuickAccessBackClicked()
{
	ShowContextActionPage();
}

#undef LOCTEXT_NAMESPACE
