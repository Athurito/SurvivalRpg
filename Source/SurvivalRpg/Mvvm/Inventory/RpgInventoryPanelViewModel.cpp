#include "RpgInventoryPanelViewModel.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryTraitsFragmentViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryUiDataFragmentViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryPanelViewModel)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryViewModels, Log, All);

namespace
{
	constexpr ETextIdenticalModeFlags InventoryPanelTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	template <typename ViewModelType>
	bool AreInventoryPanelViewModelArraysEqual(
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

	if (!AreInventoryPanelViewModelArraysEqual(PreviousEntries, Entries))
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
		!CapacityText.IdenticalTo(NewCapacityText, InventoryPanelTextIdentityFlags);

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
