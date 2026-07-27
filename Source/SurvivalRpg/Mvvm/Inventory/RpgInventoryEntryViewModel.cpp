#include "RpgInventoryEntryViewModel.h"

#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryStackFragmentViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryEntryViewModel)

namespace
{
	constexpr ETextIdenticalModeFlags InventoryEntryTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	template <typename ViewModelType>
	bool AreInventoryEntryViewModelArraysEqual(
		const TArray<TObjectPtr<ViewModelType>>& A,
		const TArray<TObjectPtr<ViewModelType>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Get() != B[Index].Get())
			{
				return false;
			}
		}

		return true;
	}
}

void URpgInventoryEntryViewModel::InitializeFromEntry(
	const FRpgInventoryEntryView& Entry,
	const TMap<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>>& FragmentViewModelClasses)
{
	const TObjectPtr<UActorComponent> NewInventoryOwner = Entry.InventoryOwner;
	const TObjectPtr<URpgInventoryItemInstance> NewItemInstance = Entry.Instance;
	const FRpgInventoryItemId NewItemId = Entry.ItemId;
	const FGuid NewEntryId = Entry.EntryId;
	const int32 NewStackCount = Entry.StackCount;
	const FRpgInventoryGridPlacement NewPlacement = Entry.Placement;
	FText NewDisplayName = FText::GetEmpty();
	FText NewShortDisplayName = FText::GetEmpty();
	FText NewDescription = FText::GetEmpty();
	TSoftObjectPtr<UTexture2D> NewIcon;
	ERpgInventoryItemCategory NewItemCategory = ERpgInventoryItemCategory::Misc;
	FGameplayTagContainer NewItemTags;
	FGameplayTagContainer NewPresentationTags;
	const bool bNewCanDrag = NewItemInstance != nullptr && NewStackCount > 0;
	const bool bNewIsEmptySlot = NewItemInstance == nullptr;
	TArray<TObjectPtr<URpgInventoryFragmentViewModel>> NewFragmentViewModels;

	if (NewItemInstance)
	{
		if (const TSubclassOf<URpgInventoryItemDefinition> ItemDef =
				NewItemInstance->GetItemDef())
		{
			if (const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDef))
			{
				NewDisplayName = ItemCDO->DisplayName;
			}
		}

		if (const URpgInventoryFragment_UIData* UIData =
				NewItemInstance->FindFragmentByClass<URpgInventoryFragment_UIData>())
		{
			NewIcon = UIData->Icon;
			NewShortDisplayName = UIData->ShortDisplayName.IsEmpty()
				? NewDisplayName
				: UIData->ShortDisplayName;
			NewDescription = UIData->Description;
			NewPresentationTags = UIData->PresentationTags;
		}
		else
		{
			NewShortDisplayName = NewDisplayName;
		}

		if (const URpgInventoryFragment_ItemTraits* Traits =
				NewItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>())
		{
			NewItemCategory = Traits->ItemCategory;
			NewItemTags = Traits->ItemTags;
		}
	}

	auto AddFragmentViewModel =
		[this, &Entry, &NewFragmentViewModels](
			TSubclassOf<URpgInventoryFragmentViewModel> ViewModelClass)
	{
		if (!ViewModelClass)
		{
			return;
		}

		URpgInventoryFragmentViewModel* FragmentViewModel = NewObject<URpgInventoryFragmentViewModel>(this, ViewModelClass);
		if (FragmentViewModel)
		{
			FragmentViewModel->InitializeFromEntry(Entry);
			NewFragmentViewModels.Add(FragmentViewModel);
		}
	};

	if (NewItemInstance)
	{
		AddFragmentViewModel(URpgInventoryStackFragmentViewModel::StaticClass());

		for (const TPair<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>>& Mapping : FragmentViewModelClasses)
		{
			if (Mapping.Key && Mapping.Value &&
				NewItemInstance->FindFragmentByClass(Mapping.Key) != nullptr)
			{
				AddFragmentViewModel(Mapping.Value);
			}
		}
	}

	const bool bInventoryOwnerChanged = InventoryOwner != NewInventoryOwner;
	const bool bItemInstanceChanged = ItemInstance != NewItemInstance;
	const bool bItemIdChanged = ItemId != NewItemId;
	const bool bEntryIdChanged = EntryId != NewEntryId;
	const bool bStackCountChanged = StackCount != NewStackCount;
	const bool bPlacementChanged = !(Placement == NewPlacement);
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, InventoryEntryTextIdentityFlags);
	const bool bShortDisplayNameChanged =
		!ShortDisplayName.IdenticalTo(
			NewShortDisplayName,
			InventoryEntryTextIdentityFlags);
	const bool bDescriptionChanged =
		!Description.IdenticalTo(NewDescription, InventoryEntryTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bItemCategoryChanged = ItemCategory != NewItemCategory;
	const bool bItemTagsChanged = ItemTags != NewItemTags;
	const bool bPresentationTagsChanged =
		PresentationTags != NewPresentationTags;
	const bool bCanDragChanged = bCanDrag != bNewCanDrag;
	const bool bIsEmptySlotChanged = bIsEmptySlot != bNewIsEmptySlot;
	const bool bFragmentViewModelsChanged =
		!AreInventoryEntryViewModelArraysEqual(
			FragmentViewModels,
			NewFragmentViewModels);
	const bool bWasChanged =
		bInventoryOwnerChanged ||
		bItemInstanceChanged ||
		bItemIdChanged ||
		bEntryIdChanged ||
		bStackCountChanged ||
		bPlacementChanged ||
		bDisplayNameChanged ||
		bShortDisplayNameChanged ||
		bDescriptionChanged ||
		bIconChanged ||
		bItemCategoryChanged ||
		bItemTagsChanged ||
		bPresentationTagsChanged ||
		bCanDragChanged ||
		bIsEmptySlotChanged;

	InventoryOwner = NewInventoryOwner;
	ItemInstance = NewItemInstance;
	ItemId = NewItemId;
	EntryId = NewEntryId;
	StackCount = NewStackCount;
	Placement = NewPlacement;
	DisplayName = NewDisplayName;
	ShortDisplayName = NewShortDisplayName;
	Description = NewDescription;
	Icon = NewIcon;
	ItemCategory = NewItemCategory;
	ItemTags = NewItemTags;
	PresentationTags = NewPresentationTags;
	bCanDrag = bNewCanDrag;
	bIsEmptySlot = bNewIsEmptySlot;
	FragmentViewModels = MoveTemp(NewFragmentViewModels);

	if (bInventoryOwnerChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InventoryOwner);
	}
	if (bItemInstanceChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	}
	if (bItemIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemId);
	}
	if (bEntryIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
	}
	if (bStackCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	}
	if (bPlacementChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Placement);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bShortDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	}
	if (bDescriptionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bItemCategoryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemCategory);
	}
	if (bItemTagsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemTags);
	}
	if (bPresentationTagsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PresentationTags);
	}
	if (bCanDragChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanDrag);
	}
	if (bIsEmptySlotChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsEmptySlot);
	}
	if (bFragmentViewModelsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FragmentViewModels);
	}
	if (bWasChanged)
	{
		OnEntryChanged.Broadcast(this);
	}
}

void URpgInventoryEntryViewModel::InitializeEmptySlot(UActorComponent* InInventoryOwner, FRpgInventoryGridPlacement InPlacement)
{
	FRpgInventoryEntryView EmptyEntry;
	EmptyEntry.InventoryOwner = InInventoryOwner;
	EmptyEntry.StackCount = 0;
	EmptyEntry.Placement = InPlacement;

	const TMap<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>> EmptyFragmentViewModelClasses;
	InitializeFromEntry(EmptyEntry, EmptyFragmentViewModelClasses);
}
