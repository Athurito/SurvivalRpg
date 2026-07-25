#include "RpgInventorySplitDialogWidget.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "PrimaryGameLayout.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryAddressSlotViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySplitDialogWidget)

namespace
{
bool RemoveSplitDialogFromOwningLayer(
	UCommonActivatableWidget& Modal)
{
	ULocalPlayer* LocalPlayer = Modal.GetOwningLocalPlayer();
	UPrimaryGameLayout* RootLayout = LocalPlayer
		? UPrimaryGameLayout::GetPrimaryGameLayout(LocalPlayer)
		: nullptr;
	UCommonActivatableWidgetContainerBase* ModalLayer = RootLayout
		? RootLayout->GetLayerWidget(RpgGameplayTags::UI_Layer_Modal)
		: nullptr;
	if (!ModalLayer || !ModalLayer->GetWidgetList().Contains(&Modal))
	{
		return false;
	}

	ModalLayer->RemoveWidget(Modal);
	return true;
}
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

void URpgInventorySplitDialogWidget::NativeDestruct()
{
	ResetSplitState(true);
	Super::NativeDestruct();
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
		ResetSplitState(false);
		if (!RemoveSplitDialogFromOwningLayer(*this))
		{
			RemoveFromParent();
		}
	}
}

void URpgInventorySplitDialogWidget::ResetSplitState(bool bCancelGridRequest)
{
	if (URpgInventorySpatialGridWidget* Grid = SourceGrid.Get();
		bCancelGridRequest && bHasOpenSplitRequest && Grid)
	{
		Grid->CancelPendingSplit();
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
