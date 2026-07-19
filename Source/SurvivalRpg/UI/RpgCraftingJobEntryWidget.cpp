#include "RpgCraftingJobEntryWidget.h"

#include "CommonLazyImage.h"
#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/UI/RpgCraftingActionButtonWidget.h"
#include "SurvivalRpg/UI/RpgCraftingStationWidget.h"
#include "SurvivalRpg/UI/RpgMvvmWidgetUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingJobEntryWidget)

const FName URpgCraftingJobEntryWidget::JobViewModelSourceName(
	TEXT("RpgCraftingJobViewModel"));

void URpgCraftingJobEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Cancel)
	{
		Button_Cancel->SetCraftButtonText(
			NSLOCTEXT("RpgCrafting", "CancelCraftJobButton", "Cancel"));
	}
	RefreshCancelAvailability();
}

void URpgCraftingJobEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Cancel)
	{
		Button_Cancel->OnClicked().RemoveAll(this);
		Button_Cancel->OnClicked().AddUObject(
			this,
			&ThisClass::HandleCancelClicked);
	}
	RefreshCancelAvailability();
}

void URpgCraftingJobEntryWidget::NativeOnListItemObjectSet(
	UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetJobViewModel(Cast<URpgCraftingJobViewModel>(ListItemObject));
}

void URpgCraftingJobEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();
	SetCommandOwner(nullptr);
	SetJobViewModel(nullptr);
}

void URpgCraftingJobEntryWidget::NativeDestruct()
{
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked().RemoveAll(this);
	}
	SetCommandOwner(nullptr);
	SetJobViewModel(nullptr);
	Super::NativeDestruct();
}

void URpgCraftingJobEntryWidget::SetCommandOwner(
	URpgCraftingStationWidget* InCommandOwner)
{
	CommandOwner = InCommandOwner;
	RefreshCancelAvailability();
}

void URpgCraftingJobEntryWidget::SetJobViewModel(
	URpgCraftingJobViewModel* InViewModel)
{
	JobViewModel = InViewModel;
	bCanCancelFromViewModel =
		JobViewModel && JobViewModel->CanCancelJob();
	RpgMvvmWidgetUtils::SetOptionalManualViewModel(
		this,
		JobViewModelSourceName,
		JobViewModel,
		URpgCraftingJobViewModel::StaticClass());
	RefreshCancelAvailability();
}

void URpgCraftingJobEntryWidget::SetJobIcon(
	TSoftObjectPtr<UTexture2D> InIcon)
{
	if (Icon)
	{
		Icon->SetBrushFromLazyTexture(InIcon);
	}
}

void URpgCraftingJobEntryWidget::SetQuantityCompleted(int32 InQuantity)
{
	if (QuantityCompletedText)
	{
		QuantityCompletedText->SetText(
			FText::AsNumber(FMath::Max(0, InQuantity)));
	}
}

void URpgCraftingJobEntryWidget::SetQuantityTotal(int32 InQuantity)
{
	if (QuantityTotalText)
	{
		QuantityTotalText->SetText(
			FText::AsNumber(FMath::Max(0, InQuantity)));
	}
}

void URpgCraftingJobEntryWidget::SetJobProgress(float InProgress)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(InProgress, 0.0f, 1.0f));
	}
}

void URpgCraftingJobEntryWidget::SetCanCancelFromViewModel(
	bool bInCanCancel)
{
	bCanCancelFromViewModel = bInCanCancel;
	RefreshCancelAvailability();
}

void URpgCraftingJobEntryWidget::HandleCancelClicked()
{
	if (CommandOwner && JobViewModel && JobViewModel->CanCancelJob())
	{
		CommandOwner->RequestCancelCraftJob(JobViewModel);
	}
}

void URpgCraftingJobEntryWidget::RefreshCancelAvailability()
{
	if (Button_Cancel)
	{
		Button_Cancel->SetIsEnabled(
			CommandOwner &&
			JobViewModel &&
			bCanCancelFromViewModel);
	}
}
