#include "RpgInventoryFragmentViewModel.h"

#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragmentViewModel)

void URpgInventoryFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	const TObjectPtr<URpgInventoryItemInstance> NewItemInstance = Entry.Instance;
	const FGuid NewEntryId = Entry.EntryId;
	const bool bItemInstanceChanged = ItemInstance != NewItemInstance;
	const bool bEntryIdChanged = EntryId != NewEntryId;

	ItemInstance = NewItemInstance;
	EntryId = NewEntryId;

	if (bItemInstanceChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	}
	if (bEntryIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
	}
}
