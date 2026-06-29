#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryLayoutTypes)

bool FRpgInventorySlotRule::AllowsItem(const URpgInventoryItemInstance* Item) const
{
	if (!Item)
	{
		return false;
	}

	const URpgInventoryFragment_ItemTraits* Traits = Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>();
	if (!Traits)
	{
		return AllowedCategories.IsEmpty() && RequiredItemTags.IsEmpty() && BlockedItemTags.IsEmpty();
	}

	if (!AllowedCategories.IsEmpty() && !AllowedCategories.Contains(Traits->ItemCategory))
	{
		return false;
	}

	if (!RequiredItemTags.IsEmpty() && !Traits->ItemTags.HasAll(RequiredItemTags))
	{
		return false;
	}

	if (!BlockedItemTags.IsEmpty() && Traits->ItemTags.HasAny(BlockedItemTags))
	{
		return false;
	}

	return true;
}
