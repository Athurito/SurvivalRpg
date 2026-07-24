#include "RpgInventoryViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryViewModels)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryViewModels, Log, All);

namespace
{
	constexpr ETextIdenticalModeFlags InventoryTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	template <typename ViewModelType>
	bool AreInventoryViewModelArraysEqual(
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
			InventoryTextIdentityFlags);
	const bool bDescriptionChanged =
		!Description.IdenticalTo(NewDescription, InventoryTextIdentityFlags);
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
		!DisplayName.IdenticalTo(NewDisplayName, InventoryTextIdentityFlags);
	const bool bShortDisplayNameChanged =
		!ShortDisplayName.IdenticalTo(
			NewShortDisplayName,
			InventoryTextIdentityFlags);
	const bool bDescriptionChanged =
		!Description.IdenticalTo(NewDescription, InventoryTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bItemCategoryChanged = ItemCategory != NewItemCategory;
	const bool bItemTagsChanged = ItemTags != NewItemTags;
	const bool bPresentationTagsChanged =
		PresentationTags != NewPresentationTags;
	const bool bCanDragChanged = bCanDrag != bNewCanDrag;
	const bool bIsEmptySlotChanged = bIsEmptySlot != bNewIsEmptySlot;
	const bool bFragmentViewModelsChanged =
		!AreInventoryViewModelArraysEqual(
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

URpgInventoryPanelViewModel::URpgInventoryPanelViewModel()
{
	FragmentViewModelClasses.Add(URpgInventoryFragment_UIData::StaticClass(), URpgInventoryUiDataFragmentViewModel::StaticClass());
	FragmentViewModelClasses.Add(URpgInventoryFragment_ItemTraits::StaticClass(), URpgInventoryTraitsFragmentViewModel::StaticClass());
}

TArray<URpgInventoryEntryViewModel*> URpgInventoryPanelViewModel::GetEntries() const
{
	TArray<URpgInventoryEntryViewModel*> Result;
	Result.Reserve(Entries.Num());
	for (URpgInventoryEntryViewModel* Entry : Entries)
	{
		Result.Add(Entry);
	}
	return Result;
}

void URpgInventoryPanelViewModel::BindInventory(URpgInventoryManagerComponent* InInventory)
{
	BindInventoryContainer(InInventory, FRpgInventoryContainerHandle());
}

void URpgInventoryPanelViewModel::BindInventoryContainer(
	URpgInventoryManagerComponent* InInventory,
	FRpgInventoryContainerHandle InContainerHandle)
{
	if (ObservedInventory.Get() == InInventory && ContainerFilter == InContainerHandle)
	{
		RefreshEntries();
		return;
	}

	UnregisterInventoryMessageListener();
	ObservedInventory = InInventory;
	ContainerFilter = InContainerHandle;
	RegisterInventoryMessageListener();
	RefreshEntries();
}

void URpgInventoryPanelViewModel::UnbindInventory()
{
	UnregisterInventoryMessageListener();
	CancelQueuedRefreshEntries();
	ObservedInventory.Reset();
	ContainerFilter = FRpgInventoryContainerHandle();
	const bool bEntriesChanged = !Entries.IsEmpty();
	Entries.Reset();
	RefreshCapacityFields(nullptr);
	if (bEntriesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	}
	OnEntriesChanged.Broadcast();
}

void URpgInventoryPanelViewModel::RefreshEntries()
{
	CancelQueuedRefreshEntries();

	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	if (!Inventory)
	{
		const bool bEntriesChanged = !Entries.IsEmpty();
		Entries.Reset();
		RefreshCapacityFields(nullptr);
		if (bEntriesChanged)
		{
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
		}
		OnEntriesChanged.Broadcast();
		return;
	}

	TArray<FRpgInventoryEntryView> EntryViews = Inventory->GetAllEntries();
	if (ContainerFilter.IsValid())
	{
		EntryViews.RemoveAll([this](const FRpgInventoryEntryView& Entry)
		{
			return Entry.Placement.GetContainerHandle() != ContainerFilter;
		});
	}
	EntryViews.Sort([](const FRpgInventoryEntryView& A, const FRpgInventoryEntryView& B)
	{
		const FRpgInventoryContainerHandle AHandle = A.Placement.GetContainerHandle();
		const FRpgInventoryContainerHandle BHandle = B.Placement.GetContainerHandle();
		if (AHandle != BHandle)
		{
			return AHandle.ToString() < BHandle.ToString();
		}

		if (A.Placement.Y != B.Placement.Y)
		{
			return A.Placement.Y < B.Placement.Y;
		}

		return A.Placement.X < B.Placement.X;
	});

	auto AddEntryViewModel = [this](URpgInventoryEntryViewModel* EntryViewModel, const FRpgInventoryEntryView& EntryView)
	{
		if (EntryViewModel)
		{
			EntryViewModel->InitializeFromEntry(EntryView, FragmentViewModelClasses);
			Entries.Add(EntryViewModel);
		}
	};

	TArray<TObjectPtr<URpgInventoryEntryViewModel>> PreviousEntries = MoveTemp(Entries);
	TMap<FRpgInventoryItemId, TObjectPtr<URpgInventoryEntryViewModel>> PreviousEntriesByItemId;
	for (URpgInventoryEntryViewModel* PreviousEntry : PreviousEntries)
	{
		if (PreviousEntry && PreviousEntry->GetItemId().IsValid())
		{
			PreviousEntriesByItemId.Add(PreviousEntry->GetItemId(), PreviousEntry);
		}
	}
	Entries.Reset();

	Entries.Reserve(EntryViews.Num());
	for (int32 EntryIndex = 0; EntryIndex < EntryViews.Num(); ++EntryIndex)
	{
		const FRpgInventoryEntryView& EntryView = EntryViews[EntryIndex];
		URpgInventoryEntryViewModel* EntryViewModel = nullptr;
		if (EntryView.ItemId.IsValid())
		{
			if (const TObjectPtr<URpgInventoryEntryViewModel>* ReusableEntry =
					PreviousEntriesByItemId.Find(EntryView.ItemId))
			{
				EntryViewModel = ReusableEntry->Get();
				PreviousEntriesByItemId.Remove(EntryView.ItemId);
			}
		}
		if (!EntryViewModel)
		{
			EntryViewModel = NewObject<URpgInventoryEntryViewModel>(this);
		}
		AddEntryViewModel(EntryViewModel, EntryView);
	}

	RefreshCapacityFields(Inventory);

	if (!AreInventoryViewModelArraysEqual(PreviousEntries, Entries))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	}
	OnEntriesChanged.Broadcast();
}

void URpgInventoryPanelViewModel::RefreshCapacityFields(URpgInventoryManagerComponent* Inventory)
{
	int32 NewUsedEntries = 0;
	int32 NewMaxEntries = 0;
	int32 NewFreeEntries = 0;
	bool bNewIsUnlimited = false;
	FText NewCapacityText = FText::GetEmpty();

	if (Inventory)
	{
		NewUsedEntries = Inventory->GetUsedEntryCount();
		NewMaxEntries = Inventory->GetMaxEntries();
		NewFreeEntries = Inventory->GetFreeEntryCount();
		bNewIsUnlimited = Inventory->IsCapacityUnlimited();
		NewCapacityText = bNewIsUnlimited
			? FText::FromString(TEXT("Unlimited"))
			: FText::FromString(FString::Printf(
				TEXT("%d / %d"),
				NewUsedEntries,
				NewMaxEntries));
	}

	const bool bUsedEntriesChanged = UsedEntries != NewUsedEntries;
	const bool bMaxEntriesChanged = MaxEntries != NewMaxEntries;
	const bool bFreeEntriesChanged = FreeEntries != NewFreeEntries;
	const bool bIsUnlimitedChanged = bIsUnlimited != bNewIsUnlimited;
	const bool bCapacityTextChanged =
		!CapacityText.IdenticalTo(NewCapacityText, InventoryTextIdentityFlags);

	UsedEntries = NewUsedEntries;
	MaxEntries = NewMaxEntries;
	FreeEntries = NewFreeEntries;
	bIsUnlimited = bNewIsUnlimited;
	CapacityText = NewCapacityText;

	if (bUsedEntriesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(UsedEntries);
	}
	if (bMaxEntriesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxEntries);
	}
	if (bFreeEntriesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FreeEntries);
	}
	if (bIsUnlimitedChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsUnlimited);
	}
	if (bCapacityTextChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CapacityText);
	}
}

void URpgInventoryPanelViewModel::RequestRefreshEntries()
{
	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	UWorld* World = Inventory ? Inventory->GetWorld() : nullptr;
	if (!World)
	{
		RefreshEntries();
		return;
	}

	RefreshEntriesQueue.Queue(
		World,
		this,
		&ThisClass::ExecuteQueuedRefreshEntries);
}

void URpgInventoryPanelViewModel::ExecuteQueuedRefreshEntries()
{
	if (!RefreshEntriesQueue.Consume())
	{
		return;
	}

	RefreshEntries();
}

void URpgInventoryPanelViewModel::CancelQueuedRefreshEntries()
{
	RefreshEntriesQueue.Cancel();
}

void URpgInventoryPanelViewModel::BeginDestroy()
{
	UnbindInventory();

	Super::BeginDestroy();
}

void URpgInventoryPanelViewModel::RegisterInventoryMessageListener()
{
	UnregisterInventoryMessageListener();

	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	UWorld* World = Inventory ? Inventory->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	const FGameplayTag InventoryChangedTag = FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged"));
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		InventoryChangedTag,
		this,
		&ThisClass::HandleInventoryChanged);
}

void URpgInventoryPanelViewModel::UnregisterInventoryMessageListener()
{
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}
}

void URpgInventoryPanelViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	if (ObservedInventory.Get() == Message.InventoryOwner)
	{
		RequestRefreshEntries();
	}
}
