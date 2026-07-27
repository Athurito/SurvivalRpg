#include "RpgInventoryDropConfirmationDialogWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "PrimaryGameLayout.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDropConfirmationDialogWidget)

#define LOCTEXT_NAMESPACE "RpgInventoryActionWidgets"

namespace
{
bool RemoveDropConfirmationFromOwningLayer(
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
		if (!RemoveDropConfirmationFromOwningLayer(*this))
		{
			RemoveFromParent();
		}
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

#undef LOCTEXT_NAMESPACE
