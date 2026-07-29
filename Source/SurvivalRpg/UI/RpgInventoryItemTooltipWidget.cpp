#include "RpgInventoryItemTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryEntryViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryItemizationFragmentViewModel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryItemTooltipWidget)

namespace RpgInventoryItemTooltipWidget
{
	FText FormatNumber(float Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 2;
		return FText::AsNumber(Value, &Options);
	}

	FText FormatStatRow(const FRpgItemizationDisplayRow& Row)
	{
		const FText ValueText = FormatNumber(Row.Value);
		return Row.bAffix
			? FText::Format(
				NSLOCTEXT("RpgInventoryTooltip", "AffixRow", "+{0} {1}"),
				ValueText,
				Row.Label)
			: FText::Format(
				NSLOCTEXT("RpgInventoryTooltip", "BaseStatRow", "{0}: {1}"),
				Row.Label,
				ValueText);
	}
}

URpgInventoryItemTooltipWidget::URpgInventoryItemTooltipWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

URpgInventoryItemTooltipWidget* URpgInventoryItemTooltipWidget::CreateForHost(
	UUserWidget* Host,
	TSubclassOf<URpgInventoryItemTooltipWidget> TooltipClass)
{
	if (!Host || !TooltipClass)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = Host->GetOwningPlayer())
	{
		return CreateWidget<URpgInventoryItemTooltipWidget>(OwningPlayer, TooltipClass);
	}

	return Host->GetWorld()
		? CreateWidget<URpgInventoryItemTooltipWidget>(Host->GetWorld(), TooltipClass)
		: nullptr;
}

void URpgInventoryItemTooltipWidget::SetEntryViewModel(
	URpgInventoryEntryViewModel* InEntryViewModel)
{
	UnbindPresentationDelegates();
	if (InEntryViewModel != OwnedEntryViewModel)
	{
		OwnedEntryViewModel = nullptr;
	}
	EntryViewModel = InEntryViewModel;
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleEntryChanged);
	}

	RefreshBoundItemizationViewModel();
	RefreshPresentation();
}

void URpgInventoryItemTooltipWidget::SetItemInstance(
	URpgInventoryItemInstance* InItemInstance,
	int32 InStackCount)
{
	if (!InItemInstance)
	{
		ClearItem();
		return;
	}

	if (!OwnedEntryViewModel)
	{
		OwnedEntryViewModel = NewObject<URpgInventoryEntryViewModel>(this);
	}

	FRpgInventoryEntryView Entry;
	Entry.Instance = InItemInstance;
	Entry.ItemId = InItemInstance->GetItemId();
	Entry.StackCount = FMath::Max(1, InStackCount);
	const TMap<
		TSubclassOf<URpgInventoryItemFragment>,
		TSubclassOf<URpgInventoryFragmentViewModel>> NoAdditionalPresenters;
	OwnedEntryViewModel->InitializeFromEntry(Entry, NoAdditionalPresenters);
	SetEntryViewModel(OwnedEntryViewModel);
}

void URpgInventoryItemTooltipWidget::ClearItem()
{
	UnbindPresentationDelegates();
	EntryViewModel = nullptr;
	OwnedEntryViewModel = nullptr;
	RefreshPresentation();
}

FText URpgInventoryItemTooltipWidget::GetDisplayName() const
{
	return EntryViewModel ? EntryViewModel->GetDisplayName() : FText::GetEmpty();
}

FText URpgInventoryItemTooltipWidget::GetDescription() const
{
	return EntryViewModel ? EntryViewModel->GetDescription() : FText::GetEmpty();
}

int32 URpgInventoryItemTooltipWidget::GetStackCount() const
{
	return EntryViewModel ? EntryViewModel->GetStackCount() : 0;
}

bool URpgInventoryItemTooltipWidget::HasItem() const
{
	return EntryViewModel && EntryViewModel->GetItemInstance() != nullptr;
}

TSharedRef<SWidget> URpgInventoryItemTooltipWidget::RebuildWidget()
{
	const bool bHasDesignerRoot = WidgetTree && WidgetTree->RootWidget != nullptr;
	const TSharedRef<SWidget> DesignerContent = Super::RebuildWidget();
	if (bHasDesignerRoot)
	{
		return DesignerContent;
	}

	const TSharedRef<SWidget> NativeContent =
		SNew(SBox)
		.MinDesiredWidth(NativeMinimumWidth)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(NativeBackgroundColor)
			.Padding(FMargin(NativePadding))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 3.0f)
				[
					SAssignNew(NativeNameText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), NativeHeaderFontSize))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					SAssignNew(NativeRarityAndLevelText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), NativeBodyFontSize))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(NativeStackText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), NativeBodyFontSize))
					.ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f, 1.0f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 4.0f)
				[
					SAssignNew(NativeDescriptionText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), NativeBodyFontSize))
					.ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f, 1.0f))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SAssignNew(NativeStatRows, SVerticalBox)
				]
			]
		];

	RefreshNativePresentation();
	return NativeContent;
}

void URpgInventoryItemTooltipWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	NativeNameText.Reset();
	NativeRarityAndLevelText.Reset();
	NativeStackText.Reset();
	NativeDescriptionText.Reset();
	NativeStatRows.Reset();
}

void URpgInventoryItemTooltipWidget::NativeDestruct()
{
	UnbindPresentationDelegates();
	EntryViewModel = nullptr;
	Super::NativeDestruct();
}

void URpgInventoryItemTooltipWidget::HandleEntryChanged(
	URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	if (ChangedEntryViewModel != EntryViewModel)
	{
		return;
	}

	RefreshBoundItemizationViewModel();
	RefreshPresentation();
}

void URpgInventoryItemTooltipWidget::HandleItemizationPresentationChanged(
	URpgInventoryItemizationFragmentViewModel* ChangedViewModel)
{
	if (ChangedViewModel == ItemizationViewModel)
	{
		RefreshPresentation();
	}
}

void URpgInventoryItemTooltipWidget::RefreshBoundItemizationViewModel()
{
	if (ItemizationViewModel)
	{
		ItemizationViewModel->OnPresentationChanged.RemoveDynamic(
			this,
			&ThisClass::HandleItemizationPresentationChanged);
	}

	ItemizationViewModel = EntryViewModel
		? EntryViewModel->GetItemizationViewModel()
		: nullptr;
	if (ItemizationViewModel)
	{
		ItemizationViewModel->OnPresentationChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleItemizationPresentationChanged);
	}
}

void URpgInventoryItemTooltipWidget::RefreshPresentation()
{
	RefreshNativePresentation();
	BP_OnTooltipPresentationChanged(EntryViewModel, ItemizationViewModel);
	OnTooltipPresentationChanged.Broadcast(this);
}

void URpgInventoryItemTooltipWidget::RefreshNativePresentation()
{
	const bool bHasItem = HasItem();
	const bool bGenerated = bHasItem && ItemizationViewModel && ItemizationViewModel->IsGenerated();
	const FLinearColor HeaderColor = bGenerated
		? ItemizationViewModel->GetRarityColor()
		: FLinearColor::White;

	if (NativeNameText)
	{
		NativeNameText->SetText(GetDisplayName());
		NativeNameText->SetColorAndOpacity(HeaderColor);
		NativeNameText->SetVisibility(bHasItem ? EVisibility::Visible : EVisibility::Collapsed);
	}

	if (NativeRarityAndLevelText)
	{
		NativeRarityAndLevelText->SetText(bGenerated
			? FText::Format(
				NSLOCTEXT("RpgInventoryTooltip", "RarityAndItemLevel", "{0}  -  Item Level {1}"),
				ItemizationViewModel->GetRarityLabel(),
				FText::AsNumber(ItemizationViewModel->GetItemLevel()))
			: FText::GetEmpty());
		NativeRarityAndLevelText->SetColorAndOpacity(HeaderColor);
		NativeRarityAndLevelText->SetVisibility(bGenerated ? EVisibility::Visible : EVisibility::Collapsed);
	}

	if (NativeStackText)
	{
		const int32 StackCount = GetStackCount();
		NativeStackText->SetText(StackCount > 1
			? FText::Format(
				NSLOCTEXT("RpgInventoryTooltip", "StackCount", "Quantity: {0}"),
				FText::AsNumber(StackCount))
			: FText::GetEmpty());
		NativeStackText->SetVisibility(StackCount > 1 ? EVisibility::Visible : EVisibility::Collapsed);
	}

	if (NativeDescriptionText)
	{
		const FText Description = GetDescription();
		NativeDescriptionText->SetText(Description);
		NativeDescriptionText->SetVisibility(!Description.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed);
	}

	if (NativeStatRows)
	{
		NativeStatRows->ClearChildren();
		if (bGenerated)
		{
			for (const FRpgItemizationDisplayRow& Row : ItemizationViewModel->GetStatRows())
			{
				NativeStatRows->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(RpgInventoryItemTooltipWidget::FormatStatRow(Row))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), NativeBodyFontSize))
					.ColorAndOpacity(Row.bAffix ? NativeAffixColor : NativeBaseStatColor)
				];
			}
		}
		NativeStatRows->SetVisibility(
			bGenerated && !ItemizationViewModel->GetStatRows().IsEmpty()
				? EVisibility::Visible
				: EVisibility::Collapsed);
	}
}

void URpgInventoryItemTooltipWidget::UnbindPresentationDelegates()
{
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(
			this,
			&ThisClass::HandleEntryChanged);
	}
	if (ItemizationViewModel)
	{
		ItemizationViewModel->OnPresentationChanged.RemoveDynamic(
			this,
			&ThisClass::HandleItemizationPresentationChanged);
	}
	ItemizationViewModel = nullptr;
}
