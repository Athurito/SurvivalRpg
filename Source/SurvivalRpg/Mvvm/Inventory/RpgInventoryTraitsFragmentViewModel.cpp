#include "RpgInventoryTraitsFragmentViewModel.h"

#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryTraitsFragmentViewModel)

void URpgInventoryTraitsFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	const URpgInventoryFragment_ItemTraits* Traits = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
	const ERpgInventoryItemCategory NewItemCategory =
		Traits ? Traits->ItemCategory : ERpgInventoryItemCategory::Misc;
	const FGameplayTagContainer NewItemTags =
		Traits ? Traits->ItemTags : FGameplayTagContainer();
	const bool bNewIsMaterial = Traits ? Traits->IsMaterial() : false;
	const bool bItemCategoryChanged = ItemCategory != NewItemCategory;
	const bool bItemTagsChanged = ItemTags != NewItemTags;
	const bool bIsMaterialChanged = bIsMaterial != bNewIsMaterial;

	ItemCategory = NewItemCategory;
	ItemTags = NewItemTags;
	bIsMaterial = bNewIsMaterial;

	Super::InitializeFromEntry(Entry);

	if (bItemCategoryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemCategory);
	}
	if (bItemTagsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemTags);
	}
	if (bIsMaterialChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsMaterial);
	}
}
