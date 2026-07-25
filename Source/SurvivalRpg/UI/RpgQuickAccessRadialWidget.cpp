#include "RpgQuickAccessRadialWidget.h"

#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerGameplayInputRouterComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarSlotViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModel.h"
#include "SurvivalRpg/UI/RpgMvvmWidgetUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgQuickAccessRadialWidget)

const FName URpgQuickAccessRadialSlotWidget::ActionBarSlotViewModelSourceName(
	TEXT("RpgActionBarSlotViewModel"));

URpgQuickAccessRadialSlotWidget::URpgQuickAccessRadialSlotWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URpgQuickAccessRadialSlotWidget::SetActionBarSlotViewModel(
	URpgActionBarSlotViewModel* InSlotViewModel)
{
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(
			this,
			&ThisClass::HandleSlotViewModelChanged);
	}

	SlotViewModel = InSlotViewModel;
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleSlotViewModelChanged);
	}

	RpgMvvmWidgetUtils::SetOptionalManualViewModel(
		this,
		ActionBarSlotViewModelSourceName,
		SlotViewModel,
		URpgActionBarSlotViewModel::StaticClass());
	RefreshPresentation();
}

void URpgQuickAccessRadialSlotWidget::SetRadialSelected(bool bInSelected)
{
	if (bRadialSelected == bInSelected)
	{
		return;
	}

	bRadialSelected = bInSelected;
	RefreshPresentation();
}

void URpgQuickAccessRadialSlotWidget::SetIconSource(
	TSoftObjectPtr<UTexture2D> InIconSource)
{
	IconSource = MoveTemp(InIconSource);
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromSoftTexture(
			IconSource,
			/*bMatchSize=*/ false);
	}
}

void URpgQuickAccessRadialSlotWidget::NativeDestruct()
{
	SetActionBarSlotViewModel(nullptr);
	SetIconSource(TSoftObjectPtr<UTexture2D>());
	bRadialSelected = false;
	Super::NativeDestruct();
}

void URpgQuickAccessRadialSlotWidget::HandleSlotViewModelChanged(
	URpgActionBarSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == SlotViewModel)
	{
		RefreshPresentation();
	}
}

void URpgQuickAccessRadialSlotWidget::RefreshPresentation()
{
	const bool bHasContent = SlotViewModel && SlotViewModel->HasContent();
	const bool bAvailable = SlotViewModel && SlotViewModel->IsAvailable();
	const int32 SlotIndex = SlotViewModel
		? SlotViewModel->GetSlotIndex()
		: INDEX_NONE;

	if (SegmentBorder)
	{
		const FLinearColor SegmentColor = bRadialSelected
			? SelectedSegmentColor
			: bHasContent && !bAvailable
				? BlockedSegmentColor
				: bHasContent
					? NormalSegmentColor
					: EmptySegmentColor;
		SegmentBorder->SetBrushColor(SegmentColor);
	}

	if (SlotNumberText)
	{
		SlotNumberText->SetText(
			SlotIndex >= 0
				? FText::AsNumber(SlotIndex + 1)
				: FText::GetEmpty());
	}

	if (BlockedText)
	{
		BlockedText->SetText(
			bHasContent && !bAvailable
				? NSLOCTEXT(
					"RpgQuickAccessRadial",
					"BlockedSlot",
					"Blocked")
				: FText::GetEmpty());
		BlockedText->SetVisibility(
			bHasContent && !bAvailable
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

URpgQuickAccessRadialWidget::URpgQuickAccessRadialWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

int32 URpgQuickAccessRadialWidget::GetAuthoredSlotEntryCount() const
{
	int32 Result = 0;
	for (const URpgQuickAccessRadialSlotWidget* Entry : GetAuthoredSlotEntries())
	{
		Result += Entry ? 1 : 0;
	}
	return Result;
}

void URpgQuickAccessRadialWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ActivateExtensionPresenter();
}

void URpgQuickAccessRadialWidget::ActivateExtensionPresenter()
{
	if (bExtensionPresenterActive)
	{
		return;
	}
	bExtensionPresenterActive = true;

	if (!ActionBarViewModel)
	{
		ActionBarViewModel = NewObject<URpgActionBarViewModel>(this);
	}
	ActionBarViewModel->OnSlotsChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleActionBarSlotsChanged);
	ActionBarViewModel->BindPlayerController(GetOwningPlayer());

	const ARpgPlayerController* PlayerController =
		Cast<ARpgPlayerController>(GetOwningPlayer());
	ObservedInputRouter = PlayerController
		? PlayerController->GetGameplayInputRouterComponent()
		: nullptr;
	if (ObservedInputRouter)
	{
		ObservedInputRouter->OnQuickAccessRadialChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleRadialChanged);
		bRadialOpen = ObservedInputRouter->IsQuickAccessRadialOpen();
		SelectedSlotIndex =
			ObservedInputRouter->GetQuickAccessRadialSelection();
	}
	else
	{
		bRadialOpen = false;
		SelectedSlotIndex = INDEX_NONE;
	}

	RefreshSlotViewModels();
	SetVisibility(
		bRadialOpen
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	if (bRadialOpen)
	{
		RegisterCommonUiCancelBinding();
	}
	RefreshSelectionPresentation();
}

void URpgQuickAccessRadialWidget::NativeDestruct()
{
	DeactivateExtensionPresenter();
	Super::NativeDestruct();
}

void URpgQuickAccessRadialWidget::NativeOnExtensionAdded()
{
	ActivateExtensionPresenter();
}

void URpgQuickAccessRadialWidget::NativeOnExtensionRemoved()
{
	DeactivateExtensionPresenter();
}

void URpgQuickAccessRadialWidget::DeactivateExtensionPresenter()
{
	if (!bExtensionPresenterActive)
	{
		return;
	}
	bExtensionPresenterActive = false;

	if (ObservedInputRouter)
	{
		ObservedInputRouter->CancelQuickAccessRadial();
	}
	UnregisterCommonUiCancelBinding();

	if (ObservedInputRouter)
	{
		ObservedInputRouter->OnQuickAccessRadialChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRadialChanged);
	}
	ObservedInputRouter = nullptr;

	if (ActionBarViewModel)
	{
		ActionBarViewModel->OnSlotsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleActionBarSlotsChanged);
		ActionBarViewModel->UnbindActionBar();
	}

	for (URpgQuickAccessRadialSlotWidget* Entry : GetAuthoredSlotEntries())
	{
		if (Entry)
		{
			Entry->SetRadialSelected(false);
			Entry->SetActionBarSlotViewModel(nullptr);
		}
	}

	bRadialOpen = false;
	SelectedSlotIndex = INDEX_NONE;
	SetVisibility(ESlateVisibility::Collapsed);
}

void URpgQuickAccessRadialWidget::HandleRadialChanged(
	bool bIsOpen,
	int32 InSelectedSlotIndex)
{
	bRadialOpen = bIsOpen;
	SelectedSlotIndex = bIsOpen ? InSelectedSlotIndex : INDEX_NONE;
	if (bRadialOpen && ActionBarViewModel)
	{
		// HUD extensions may construct before the owning PlayerState is available.
		// Rebinding on open resolves the latest owner-only actionbar projection.
		ActionBarViewModel->BindPlayerController(GetOwningPlayer());
	}
	SetVisibility(
		bRadialOpen
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	if (bRadialOpen)
	{
		RegisterCommonUiCancelBinding();
	}
	else
	{
		UnregisterCommonUiCancelBinding();
	}
	RefreshSelectionPresentation();
}

void URpgQuickAccessRadialWidget::HandleActionBarSlotsChanged()
{
	RefreshSlotViewModels();
}

TArray<URpgQuickAccessRadialSlotWidget*>
URpgQuickAccessRadialWidget::GetAuthoredSlotEntries() const
{
	return {
		RadialSlot_0.Get(),
		RadialSlot_1.Get(),
		RadialSlot_2.Get(),
		RadialSlot_3.Get(),
		RadialSlot_4.Get(),
		RadialSlot_5.Get(),
		RadialSlot_6.Get(),
		RadialSlot_7.Get()
	};
}

void URpgQuickAccessRadialWidget::HandleCommonUiCancel()
{
	if (ObservedInputRouter)
	{
		ObservedInputRouter->CancelQuickAccessRadial();
	}
}

void URpgQuickAccessRadialWidget::RegisterCommonUiCancelBinding()
{
	if (CommonUiCancelBinding.IsValid() ||
		!CommonUiCancelAction.DataTable ||
		CommonUiCancelAction.RowName.IsNone())
	{
		return;
	}

	FBindUIActionArgs BindArgs(
		CommonUiCancelAction,
		/*bShouldDisplayInActionBar=*/ false,
		FSimpleDelegate::CreateUObject(
			this,
			&ThisClass::HandleCommonUiCancel));
	BindArgs.InputMode = ECommonInputMode::Game;
	BindArgs.bConsumeInput = true;
	BindArgs.bDisplayInActionBar = false;
	CommonUiCancelBinding = RegisterUIActionBinding(BindArgs);
}

void URpgQuickAccessRadialWidget::UnregisterCommonUiCancelBinding()
{
	if (CommonUiCancelBinding.IsValid())
	{
		RemoveActionBinding(CommonUiCancelBinding);
		CommonUiCancelBinding.Unregister();
	}
	CommonUiCancelBinding = FUIActionBindingHandle();
}

void URpgQuickAccessRadialWidget::RefreshSlotViewModels()
{
	const TArray<URpgQuickAccessRadialSlotWidget*> Entries =
		GetAuthoredSlotEntries();
	for (int32 SlotIndex = 0; SlotIndex < Entries.Num(); ++SlotIndex)
	{
		if (URpgQuickAccessRadialSlotWidget* Entry = Entries[SlotIndex])
		{
			Entry->SetActionBarSlotViewModel(
				ActionBarViewModel
					? ActionBarViewModel->GetSlotAtIndex(SlotIndex)
					: nullptr);
		}
	}

	RefreshSelectionPresentation();
}

void URpgQuickAccessRadialWidget::RefreshSelectionPresentation()
{
	const TArray<URpgQuickAccessRadialSlotWidget*> Entries =
		GetAuthoredSlotEntries();
	for (int32 SlotIndex = 0; SlotIndex < Entries.Num(); ++SlotIndex)
	{
		if (URpgQuickAccessRadialSlotWidget* Entry = Entries[SlotIndex])
		{
			Entry->SetRadialSelected(
				bRadialOpen && SlotIndex == SelectedSlotIndex);
		}
	}
}
