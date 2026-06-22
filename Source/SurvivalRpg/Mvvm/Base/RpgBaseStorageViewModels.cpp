#include "RpgBaseStorageViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageViewModels)

namespace
{
	const URpgInventoryItemDefinition* GetItemDefinitionCDO(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		return ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
	}

	FText GetItemDisplayName(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		return ItemCDO ? ItemCDO->DisplayName : FText::GetEmpty();
	}

	TSoftObjectPtr<UTexture2D> GetItemIcon(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		const URpgInventoryFragment_UIData* UIData = ItemCDO ? Cast<URpgInventoryFragment_UIData>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_UIData::StaticClass())) : nullptr;
		return UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
	}

	bool AreResourceOrdersEqual(
		const TArray<TSubclassOf<URpgInventoryItemDefinition>>& PreviousOrder,
		const TArray<TObjectPtr<URpgBaseResourceEntryViewModel>>& NewResources)
	{
		if (PreviousOrder.Num() != NewResources.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < PreviousOrder.Num(); ++Index)
		{
			const TSubclassOf<URpgInventoryItemDefinition> NewItemDefinition = NewResources[Index] ? NewResources[Index]->GetItemDefinition() : nullptr;
			if (PreviousOrder[Index] != NewItemDefinition)
			{
				return false;
			}
		}

		return true;
	}

	bool AreDefinitionArraysEqual(
		const TArray<TSubclassOf<URpgInventoryItemDefinition>>& A,
		const TArray<TSubclassOf<URpgInventoryItemDefinition>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index] != B[Index])
			{
				return false;
			}
		}

		return true;
	}
}

void URpgBaseResourceEntryViewModel::InitializeFromResourceEntry(const FRpgBaseResourceEntryView& Entry)
{
	ItemDefinition = Entry.ItemDefinition;
	DisplayName = GetItemDisplayName(ItemDefinition);
	Icon = GetItemIcon(ItemDefinition);
	Count = FMath::Max(0, Entry.Count);
	Capacity = FMath::Max(0, Entry.Capacity);
	FreeCapacity = FMath::Max(0, Capacity - Count);
	FillRatio = Capacity > 0 ? FMath::Clamp(static_cast<float>(Count) / static_cast<float>(Capacity), 0.0f, 1.0f) : 0.0f;
	SortIndex = Entry.SortIndex;
	bIsEmpty = Count <= 0;
	bIsFull = Capacity > 0 && Count >= Capacity;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemDefinition);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Count);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Capacity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FreeCapacity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FillRatio);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SortIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsEmpty);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsFull);
}

void URpgBaseStorageViewModel::BeginDestroy()
{
	UnbindBaseStorage();
	Super::BeginDestroy();
}

void URpgBaseStorageViewModel::BindBaseStorage(URpgBaseStorageComponent* InBaseStorage, const TArray<TSubclassOf<URpgInventoryItemDefinition>>& InAllowedResources)
{
	if (ObservedBaseStorage == InBaseStorage && AreDefinitionArraysEqual(AllowedResources, InAllowedResources))
	{
		RefreshResources();
		return;
	}

	UnbindBaseStorage();
	ObservedBaseStorage = InBaseStorage;
	AllowedResources = InAllowedResources;
	RegisterBaseStorageMessageListener();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedBaseStorage);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AllowedResources);
	RefreshResources();
}

void URpgBaseStorageViewModel::BindBaseStorageStation(URpgBaseStorageStationComponent* Station)
{
	BindBaseStorage(Station ? Station->GetBaseStorage() : nullptr, Station ? Station->GetAllowedResourceDefinitions() : TArray<TSubclassOf<URpgInventoryItemDefinition>>());
}

void URpgBaseStorageViewModel::UnbindBaseStorage()
{
	UnregisterBaseStorageMessageListener();
	ObservedBaseStorage = nullptr;
	AllowedResources.Reset();
	Resources.Reset();
	ResourceCount = 0;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedBaseStorage);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AllowedResources);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Resources);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ResourceCount);
	OnResourcesChanged.Broadcast();
}

void URpgBaseStorageViewModel::RefreshResources()
{
	RebuildResources();
}

void URpgBaseStorageViewModel::SetAllowedResources(const TArray<TSubclassOf<URpgInventoryItemDefinition>>& InAllowedResources)
{
	if (AreDefinitionArraysEqual(AllowedResources, InAllowedResources))
	{
		return;
	}

	AllowedResources = InAllowedResources;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AllowedResources);
	RefreshResources();
}

TArray<URpgBaseResourceEntryViewModel*> URpgBaseStorageViewModel::GetResources() const
{
	TArray<URpgBaseResourceEntryViewModel*> Result;
	Result.Reserve(Resources.Num());
	for (URpgBaseResourceEntryViewModel* Resource : Resources)
	{
		Result.Add(Resource);
	}
	return Result;
}

void URpgBaseStorageViewModel::RegisterBaseStorageMessageListener()
{
	UnregisterBaseStorageMessageListener();

	UWorld* World = ObservedBaseStorage ? ObservedBaseStorage->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	BaseStorageChangedHandle = MessageSubsystem.RegisterListener<FRpgBaseResourceChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.BaseStorage.Message.Changed")),
		this,
		&ThisClass::HandleBaseStorageChanged);
}

void URpgBaseStorageViewModel::UnregisterBaseStorageMessageListener()
{
	if (BaseStorageChangedHandle.IsValid())
	{
		BaseStorageChangedHandle.Unregister();
	}
}

void URpgBaseStorageViewModel::RebuildResources()
{
	TArray<TSubclassOf<URpgInventoryItemDefinition>> PreviousOrder;
	PreviousOrder.Reserve(Resources.Num());

	TMap<TSubclassOf<URpgInventoryItemDefinition>, URpgBaseResourceEntryViewModel*> PreviousViewModelsByDefinition;
	for (URpgBaseResourceEntryViewModel* ExistingViewModel : Resources)
	{
		const TSubclassOf<URpgInventoryItemDefinition> ExistingDefinition = ExistingViewModel ? ExistingViewModel->GetItemDefinition() : nullptr;
		PreviousOrder.Add(ExistingDefinition);
		if (ExistingDefinition && ExistingViewModel)
		{
			PreviousViewModelsByDefinition.Add(ExistingDefinition, ExistingViewModel);
		}
	}

	TSet<TSubclassOf<URpgInventoryItemDefinition>> AllowedSet;
	for (TSubclassOf<URpgInventoryItemDefinition> AllowedDefinition : AllowedResources)
	{
		if (AllowedDefinition)
		{
			AllowedSet.Add(AllowedDefinition);
		}
	}

	auto IsAllowed = [&AllowedSet](TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		return AllowedSet.Num() <= 0 || AllowedSet.Contains(ItemDefinition);
	};

	TArray<TObjectPtr<URpgBaseResourceEntryViewModel>> NewResources;
	TSet<TSubclassOf<URpgInventoryItemDefinition>> AddedDefinitions;
	if (ObservedBaseStorage)
	{
		for (const FRpgBaseResourceEntryView& Entry : ObservedBaseStorage->GetAllResources())
		{
			if (!Entry.ItemDefinition || !IsAllowed(Entry.ItemDefinition))
			{
				continue;
			}

			URpgBaseResourceEntryViewModel* ResourceViewModel = nullptr;
			if (URpgBaseResourceEntryViewModel** ExistingViewModel = PreviousViewModelsByDefinition.Find(Entry.ItemDefinition))
			{
				ResourceViewModel = *ExistingViewModel;
			}

			if (!ResourceViewModel)
			{
				ResourceViewModel = NewObject<URpgBaseResourceEntryViewModel>(this);
			}

			ResourceViewModel->InitializeFromResourceEntry(Entry);
			NewResources.Add(ResourceViewModel);
			AddedDefinitions.Add(Entry.ItemDefinition);
		}

		for (TSubclassOf<URpgInventoryItemDefinition> AllowedDefinition : AllowedResources)
		{
			if (!AllowedDefinition || AddedDefinitions.Contains(AllowedDefinition))
			{
				continue;
			}

			const int32 Count = ObservedBaseStorage->GetResourceCount(AllowedDefinition);
			const int32 Capacity = ObservedBaseStorage->GetResourceCapacity(AllowedDefinition);
			if (Count <= 0 && Capacity <= 0)
			{
				continue;
			}

			FRpgBaseResourceEntryView Entry;
			Entry.ItemDefinition = AllowedDefinition;
			Entry.Count = Count;
			Entry.Capacity = Capacity;
			Entry.SortIndex = NewResources.Num();

			URpgBaseResourceEntryViewModel* ResourceViewModel = nullptr;
			if (URpgBaseResourceEntryViewModel** ExistingViewModel = PreviousViewModelsByDefinition.Find(AllowedDefinition))
			{
				ResourceViewModel = *ExistingViewModel;
			}

			if (!ResourceViewModel)
			{
				ResourceViewModel = NewObject<URpgBaseResourceEntryViewModel>(this);
			}

			ResourceViewModel->InitializeFromResourceEntry(Entry);
			NewResources.Add(ResourceViewModel);
		}
	}

	const bool bResourceListChanged = !AreResourceOrdersEqual(PreviousOrder, NewResources);
	Resources = MoveTemp(NewResources);
	const int32 NewResourceCount = Resources.Num();
	const bool bResourceCountChanged = ResourceCount != NewResourceCount;
	ResourceCount = NewResourceCount;

	if (bResourceListChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Resources);
	}

	if (bResourceCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ResourceCount);
	}

	OnResourcesChanged.Broadcast();
}

void URpgBaseStorageViewModel::HandleBaseStorageChanged(FGameplayTag Channel, const FRpgBaseResourceChangeMessage& Message)
{
	if (ObservedBaseStorage && Message.StorageOwner.Get() == ObservedBaseStorage)
	{
		RefreshResources();
	}
}
