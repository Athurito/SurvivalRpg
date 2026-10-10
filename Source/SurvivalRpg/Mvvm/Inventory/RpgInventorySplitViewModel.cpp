#include "RpgInventorySplitViewModel.h"

#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySplitViewModel)

#define LOCTEXT_NAMESPACE "RpgInventorySplit"

namespace RpgInventorySplitViewModel
{
	void SetText(URpgInventorySplitViewModel& ViewModel, FText& Field, const FText& NewValue, UE::FieldNotification::FFieldId FieldId)
	{
		if (!Field.IdenticalTo(NewValue, ETextIdenticalModeFlags::DeepCompare | ETextIdenticalModeFlags::LexicalCompareInvariants))
		{
			Field = NewValue;
			ViewModel.BroadcastFieldValueChanged(FieldId);
		}
	}
}

void URpgInventorySplitViewModel::SetStack(
	const FText& InDisplayName,
	const TSoftObjectPtr<UTexture2D>& InIcon,
	int32 InTotalCount)
{
	TotalCount = FMath::Max(0, InTotalCount);
	RpgInventorySplitViewModel::SetText(*this, DisplayName, InDisplayName, FFieldNotificationClassDescriptor::DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, InIcon);
	RpgInventorySplitViewModel::SetText(
		*this,
		TotalText,
		FText::Format(LOCTEXT("Total", "{0} total"), FText::AsNumber(TotalCount)),
		FFieldNotificationClassDescriptor::TotalText);
	RefreshAmounts();
}

void URpgInventorySplitViewModel::SetRange(int32 InMinimumCount, int32 InMaximumCount)
{
	RpgInventorySplitViewModel::SetText(*this, MinimumText, FText::AsNumber(InMinimumCount), FFieldNotificationClassDescriptor::MinimumText);
	RpgInventorySplitViewModel::SetText(*this, MaximumText, FText::AsNumber(InMaximumCount), FFieldNotificationClassDescriptor::MaximumText);
}

void URpgInventorySplitViewModel::SetSelectedCount(int32 InSelectedCount)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedCount, FMath::Max(0, InSelectedCount));
	RefreshAmounts();
}

void URpgInventorySplitViewModel::Clear()
{
	SetStack(FText::GetEmpty(), TSoftObjectPtr<UTexture2D>(), 0);
	SetRange(0, 0);
	SetSelectedCount(0);
}

void URpgInventorySplitViewModel::RefreshAmounts()
{
	UE_MVVM_SET_PROPERTY_VALUE(RemainingCount, FMath::Max(0, TotalCount - SelectedCount));
	RpgInventorySplitViewModel::SetText(
		*this,
		SummaryText,
		FText::Format(
			LOCTEXT("Summary", "{0} split · {1} remain"),
			FText::AsNumber(SelectedCount),
			FText::AsNumber(RemainingCount)),
		FFieldNotificationClassDescriptor::SummaryText);
	RpgInventorySplitViewModel::SetText(
		*this,
		ConfirmText,
		FText::Format(LOCTEXT("Confirm", "Split {0}"), FText::AsNumber(SelectedCount)),
		FFieldNotificationClassDescriptor::ConfirmText);
}

#undef LOCTEXT_NAMESPACE
