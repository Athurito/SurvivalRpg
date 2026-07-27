#include "RpgInventoryStackFragmentViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryStackFragmentViewModel)

void URpgInventoryStackFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	const int32 NewStackCount = Entry.StackCount;
	const bool bStackCountChanged = StackCount != NewStackCount;
	StackCount = NewStackCount;

	Super::InitializeFromEntry(Entry);

	if (bStackCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	}
}
