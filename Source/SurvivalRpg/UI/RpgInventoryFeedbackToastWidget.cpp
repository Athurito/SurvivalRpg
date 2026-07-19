#include "RpgInventoryFeedbackToastWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFeedbackToastWidget)

#define LOCTEXT_NAMESPACE "RpgInventoryFeedbackToast"

URpgInventoryFeedbackToastWidget::URpgInventoryFeedbackToastWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void URpgInventoryFeedbackToastWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	HideInventoryActionFeedback();
}

void URpgInventoryFeedbackToastWidget::NativeDestruct()
{
	HideInventoryActionFeedback();
	Super::NativeDestruct();
}

void URpgInventoryFeedbackToastWidget::ShowInventoryActionFeedback(const FRpgInventoryActionFeedbackMessage& Message)
{
	if (!FeedbackBorder || !FeedbackText)
	{
		return;
	}

	const bool bSucceeded = Message.Result == ERpgInventoryActionFeedbackResult::Success;
	FeedbackBorder->SetBrushColor(bSucceeded ? SuccessColor : FailureColor);
	FeedbackText->SetText(BuildFeedbackText(Message));
	SetVisibility(ESlateVisibility::HitTestInvisible);
	BP_OnInventoryActionFeedbackShown(Message);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
		World->GetTimerManager().SetTimer(
			HideTimerHandle,
			this,
			&ThisClass::HideInventoryActionFeedback,
			FMath::Max(0.25f, DisplayDuration),
			false);
	}
}

void URpgInventoryFeedbackToastWidget::HideInventoryActionFeedback()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

FText URpgInventoryFeedbackToastWidget::BuildFeedbackText(const FRpgInventoryActionFeedbackMessage& Message)
{
	switch (Message.Result)
	{
	case ERpgInventoryActionFeedbackResult::Success:
		return LOCTEXT("Success", "Action completed");
	case ERpgInventoryActionFeedbackResult::InventoryFull:
		return LOCTEXT("InventoryFull", "No complete placement available");
	case ERpgInventoryActionFeedbackResult::InvalidSlot:
		return LOCTEXT("InvalidSlot", "Item does not fit this slot");
	case ERpgInventoryActionFeedbackResult::NotStackable:
		return LOCTEXT("NotStackable", "Stack cannot be split or merged");
	case ERpgInventoryActionFeedbackResult::CannotUse:
		return LOCTEXT("CannotUse", "Item cannot be used now");
	case ERpgInventoryActionFeedbackResult::CannotDrop:
		return LOCTEXT("CannotDrop", "This item cannot be dropped");
	case ERpgInventoryActionFeedbackResult::RequiresConfirmation:
		return LOCTEXT("RequiresConfirmation", "This drop requires confirmation");
	case ERpgInventoryActionFeedbackResult::WrongInventory:
		return LOCTEXT("WrongInventory", "Action is not available from this inventory");
	case ERpgInventoryActionFeedbackResult::NotEquippable:
		return LOCTEXT("NotEquippable", "Item cannot be equipped");
	case ERpgInventoryActionFeedbackResult::NoValidSlot:
		return LOCTEXT("NoValidSlot", "No compatible slot is available");
	case ERpgInventoryActionFeedbackResult::AbilityRejected:
		return LOCTEXT("AbilityRejected", "Item ability was rejected");
	case ERpgInventoryActionFeedbackResult::NoAccess:
		return LOCTEXT("NoAccess", "Inventory is no longer accessible");
	case ERpgInventoryActionFeedbackResult::MissingItem:
		return LOCTEXT("MissingItem", "Item is no longer available");
	case ERpgInventoryActionFeedbackResult::InvalidRequest:
		return LOCTEXT("InvalidRequest", "Inventory changed before the action completed");
	case ERpgInventoryActionFeedbackResult::ServerRejected:
	default:
		return LOCTEXT("ServerRejected", "Action rejected by server");
	}
}

#undef LOCTEXT_NAMESPACE
