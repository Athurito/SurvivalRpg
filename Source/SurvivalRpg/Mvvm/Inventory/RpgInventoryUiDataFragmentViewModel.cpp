#include "RpgInventoryUiDataFragmentViewModel.h"

#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiDataFragmentViewModel)

namespace
{
	constexpr ETextIdenticalModeFlags InventoryUiDataTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;
}

void URpgInventoryUiDataFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	const URpgInventoryFragment_UIData* UIData = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_UIData>() : nullptr;
	const TSoftObjectPtr<UTexture2D> NewIcon =
		UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
	const FText NewShortDisplayName =
		UIData ? UIData->ShortDisplayName : FText::GetEmpty();
	const FText NewDescription =
		UIData ? UIData->Description : FText::GetEmpty();
	const FGameplayTagContainer NewPresentationTags =
		UIData ? UIData->PresentationTags : FGameplayTagContainer();
	const bool bIconChanged = Icon != NewIcon;
	const bool bShortDisplayNameChanged =
		!ShortDisplayName.IdenticalTo(
			NewShortDisplayName,
			InventoryUiDataTextIdentityFlags);
	const bool bDescriptionChanged =
		!Description.IdenticalTo(NewDescription, InventoryUiDataTextIdentityFlags);
	const bool bPresentationTagsChanged =
		PresentationTags != NewPresentationTags;

	Icon = NewIcon;
	ShortDisplayName = NewShortDisplayName;
	Description = NewDescription;
	PresentationTags = NewPresentationTags;

	Super::InitializeFromEntry(Entry);

	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bShortDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	}
	if (bDescriptionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	}
	if (bPresentationTagsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PresentationTags);
	}
}
