#include "RpgInventoryViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryViewModels)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryViewModels, Log, All);

void URpgInventoryFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	ItemInstance = Entry.Instance;
	EntryId = Entry.EntryId;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
}

void URpgInventoryStackFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	Super::InitializeFromEntry(Entry);

	StackCount = Entry.StackCount;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
}

void URpgInventoryTraitsFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	Super::InitializeFromEntry(Entry);

	const URpgInventoryFragment_ItemTraits* Traits = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
	ItemCategory = Traits ? Traits->ItemCategory : ERpgInventoryItemCategory::Misc;
	ItemTags = Traits ? Traits->ItemTags : FGameplayTagContainer();
	bIsMaterial = Traits ? Traits->IsMaterial() : false;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemCategory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemTags);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsMaterial);
}

void URpgInventoryUiDataFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	Super::InitializeFromEntry(Entry);

	const URpgInventoryFragment_UIData* UIData = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_UIData>() : nullptr;
	Icon = UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
	ShortDisplayName = UIData ? UIData->ShortDisplayName : FText::GetEmpty();
	Description = UIData ? UIData->Description : FText::GetEmpty();
	PresentationTags = UIData ? UIData->PresentationTags : FGameplayTagContainer();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PresentationTags);
}

void URpgInventoryEntryViewModel::InitializeFromEntry(
	const FRpgInventoryEntryView& Entry,
	const TMap<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>>& FragmentViewModelClasses)
{
	const bool bWasChanged =
		InventoryOwner != Entry.InventoryOwner ||
		ItemInstance != Entry.Instance ||
		ItemId != Entry.ItemId ||
		EntryId != Entry.EntryId ||
		StackCount != Entry.StackCount ||
		Placement.GetContainerHandle() != Entry.Placement.GetContainerHandle() ||
		Placement.X != Entry.Placement.X ||
		Placement.Y != Entry.Placement.Y ||
		Placement.bRotated != Entry.Placement.bRotated ||
		bIsEmptySlot != (Entry.Instance == nullptr);

	InventoryOwner = Entry.InventoryOwner;
	ItemInstance = Entry.Instance;
	ItemId = Entry.ItemId;
	EntryId = Entry.EntryId;
	StackCount = Entry.StackCount;
	Placement = Entry.Placement;
	DisplayName = FText::GetEmpty();
	ShortDisplayName = FText::GetEmpty();
	Description = FText::GetEmpty();
	Icon.Reset();
	ItemCategory = ERpgInventoryItemCategory::Misc;
	ItemTags.Reset();
	PresentationTags.Reset();
	bCanDrag = ItemInstance != nullptr && StackCount > 0;
	bIsEmptySlot = ItemInstance == nullptr;
	FragmentViewModels.Reset();

	if (ItemInstance)
	{
		if (const TSubclassOf<URpgInventoryItemDefinition> ItemDef = ItemInstance->GetItemDef())
		{
			if (const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDef))
			{
				DisplayName = ItemCDO->DisplayName;
			}
		}

		if (const URpgInventoryFragment_UIData* UIData = ItemInstance->FindFragmentByClass<URpgInventoryFragment_UIData>())
		{
			Icon = UIData->Icon;
			ShortDisplayName = UIData->ShortDisplayName.IsEmpty() ? DisplayName : UIData->ShortDisplayName;
			Description = UIData->Description;
			PresentationTags = UIData->PresentationTags;
		}
		else
		{
			ShortDisplayName = DisplayName;
		}

		if (const URpgInventoryFragment_ItemTraits* Traits = ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>())
		{
			ItemCategory = Traits->ItemCategory;
			ItemTags = Traits->ItemTags;
		}
	}

	auto AddFragmentViewModel = [this, &Entry](TSubclassOf<URpgInventoryFragmentViewModel> ViewModelClass)
	{
		if (!ViewModelClass)
		{
			return;
		}

		URpgInventoryFragmentViewModel* FragmentViewModel = NewObject<URpgInventoryFragmentViewModel>(this, ViewModelClass);
		if (FragmentViewModel)
		{
			FragmentViewModel->InitializeFromEntry(Entry);
			FragmentViewModels.Add(FragmentViewModel);
		}
	};

	if (ItemInstance)
	{
		AddFragmentViewModel(URpgInventoryStackFragmentViewModel::StaticClass());

		for (const TPair<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>>& Mapping : FragmentViewModelClasses)
		{
			if (Mapping.Key && Mapping.Value && ItemInstance->FindFragmentByClass(Mapping.Key) != nullptr)
			{
				AddFragmentViewModel(Mapping.Value);
			}
		}
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InventoryOwner);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Placement);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemCategory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemTags);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PresentationTags);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanDrag);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsEmptySlot);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FragmentViewModels);
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

	UnbindInventory();
	ObservedInventory = InInventory;
	ContainerFilter = InContainerHandle;
	RegisterInventoryMessageListener();
	RefreshEntries();
}

void URpgInventoryPanelViewModel::UnbindInventory()
{
	UnregisterInventoryMessageListener();
	ObservedInventory.Reset();
	ContainerFilter = FRpgInventoryContainerHandle();
	Entries.Reset();
	bRefreshEntriesQueued = false;
	RefreshCapacityFields(nullptr);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	OnEntriesChanged.Broadcast();
}

void URpgInventoryPanelViewModel::RefreshEntries()
{
	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	RefreshCapacityFields(Inventory);
	if (!Inventory)
	{
		Entries.Reset();
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
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

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	OnEntriesChanged.Broadcast();
}

void URpgInventoryPanelViewModel::RefreshCapacityFields(URpgInventoryManagerComponent* Inventory)
{
	if (!Inventory)
	{
		UsedEntries = 0;
		MaxEntries = 0;
		FreeEntries = 0;
		bIsUnlimited = false;
		CapacityText = FText::GetEmpty();
	}
	else
	{
		UsedEntries = Inventory->GetUsedEntryCount();
		MaxEntries = Inventory->GetMaxEntries();
		FreeEntries = Inventory->GetFreeEntryCount();
		bIsUnlimited = Inventory->IsCapacityUnlimited();
		CapacityText = bIsUnlimited
			? FText::FromString(TEXT("Unlimited"))
			: FText::FromString(FString::Printf(TEXT("%d / %d"), UsedEntries, MaxEntries));
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(UsedEntries);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxEntries);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FreeEntries);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsUnlimited);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CapacityText);
}

void URpgInventoryPanelViewModel::RequestRefreshEntries()
{
	if (bRefreshEntriesQueued)
	{
		return;
	}

	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	UWorld* World = Inventory ? Inventory->GetWorld() : nullptr;
	if (!World)
	{
		RefreshEntries();
		return;
	}

	bRefreshEntriesQueued = true;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::ExecuteQueuedRefreshEntries));
}

void URpgInventoryPanelViewModel::ExecuteQueuedRefreshEntries()
{
	bRefreshEntriesQueued = false;
	RefreshEntries();
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
