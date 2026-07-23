#include "RpgBaseStorageViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageViewModels)

namespace
{
	constexpr ETextIdenticalModeFlags FieldNotifyTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	const URpgInventoryItemDefinition* GetBaseStorageItemDefinitionCDO(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		return ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
	}

	FText GetBaseStorageItemDisplayName(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetBaseStorageItemDefinitionCDO(ItemDefinition);
		return ItemCDO ? ItemCDO->DisplayName : FText::GetEmpty();
	}

	TSoftObjectPtr<UTexture2D> GetBaseStorageItemIcon(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetBaseStorageItemDefinitionCDO(ItemDefinition);
		const URpgInventoryFragment_UIData* UIData = ItemCDO ? Cast<URpgInventoryFragment_UIData>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_UIData::StaticClass())) : nullptr;
		return UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
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
	const TSubclassOf<URpgInventoryItemDefinition> NewItemDefinition = Entry.ItemDefinition;
	const int32 NewCount = FMath::Max(0, Entry.Count);
	const int32 NewCapacity = FMath::Max(0, Entry.Capacity);
	const int32 NewFreeCapacity = FMath::Max(0, NewCapacity - NewCount);
	const float NewFillRatio = NewCapacity > 0
		? FMath::Clamp(static_cast<float>(NewCount) / static_cast<float>(NewCapacity), 0.0f, 1.0f)
		: 0.0f;
	const FText NewDisplayName = GetBaseStorageItemDisplayName(NewItemDefinition);
	const TSoftObjectPtr<UTexture2D> NewIcon = GetBaseStorageItemIcon(NewItemDefinition);
	const bool bNewIsEmpty = NewCount <= 0;
	const bool bNewIsFull = NewCapacity > 0 && NewCount >= NewCapacity;

	const bool bItemDefinitionChanged = ItemDefinition != NewItemDefinition;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, FieldNotifyTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bCountChanged = Count != NewCount;
	const bool bCapacityChanged = Capacity != NewCapacity;
	const bool bFreeCapacityChanged = FreeCapacity != NewFreeCapacity;
	const bool bFillRatioChanged = FillRatio != NewFillRatio;
	const bool bSortIndexChanged = SortIndex != Entry.SortIndex;
	const bool bIsEmptyChanged = bIsEmpty != bNewIsEmpty;
	const bool bIsFullChanged = bIsFull != bNewIsFull;

	ItemDefinition = NewItemDefinition;
	DisplayName = NewDisplayName;
	Icon = NewIcon;
	Count = NewCount;
	Capacity = NewCapacity;
	FreeCapacity = NewFreeCapacity;
	FillRatio = NewFillRatio;
	SortIndex = Entry.SortIndex;
	bIsEmpty = bNewIsEmpty;
	bIsFull = bNewIsFull;

	if (bItemDefinitionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemDefinition);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Count);
	}
	if (bCapacityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Capacity);
	}
	if (bFreeCapacityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FreeCapacity);
	}
	if (bFillRatioChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FillRatio);
	}
	if (bSortIndexChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SortIndex);
	}
	if (bIsEmptyChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsEmpty);
	}
	if (bIsFullChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsFull);
	}
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

	UnregisterBaseStorageMessageListener();
	const bool bObservedBaseStorageChanged = ObservedBaseStorage != InBaseStorage;
	const bool bAllowedResourcesChanged = !AreDefinitionArraysEqual(AllowedResources, InAllowedResources);
	ObservedBaseStorage = InBaseStorage;
	AllowedResources = InAllowedResources;
	RegisterBaseStorageMessageListener();
	RefreshResources();
	if (bObservedBaseStorageChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedBaseStorage);
	}
	if (bAllowedResourcesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AllowedResources);
	}
}

void URpgBaseStorageViewModel::BindBaseStorageStation(URpgBaseStorageStationComponent* Station)
{
	BindBaseStorage(Station ? Station->GetBaseStorage() : nullptr, Station ? Station->GetAllowedResourceDefinitions() : TArray<TSubclassOf<URpgInventoryItemDefinition>>());
}

void URpgBaseStorageViewModel::UnbindBaseStorage()
{
	UnregisterBaseStorageMessageListener();
	const TArray<TSubclassOf<URpgInventoryItemDefinition>> EmptyAllowedResources;
	const TArray<TObjectPtr<URpgBaseResourceEntryViewModel>> EmptyResources;
	const bool bObservedBaseStorageChanged = ObservedBaseStorage != nullptr;
	const bool bAllowedResourcesChanged = !AllowedResources.IsEmpty();
	const bool bResourcesChanged = !Resources.IsEmpty();
	const bool bResourceCountChanged = ResourceCount != 0;
	ObservedBaseStorage = nullptr;
	AllowedResources = EmptyAllowedResources;
	Resources = EmptyResources;
	ResourceCount = 0;

	if (bObservedBaseStorageChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedBaseStorage);
	}
	if (bAllowedResourcesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AllowedResources);
	}
	if (bResourcesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Resources);
	}
	if (bResourceCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ResourceCount);
	}
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
	RefreshResources();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AllowedResources);
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
	TMap<TSubclassOf<URpgInventoryItemDefinition>, URpgBaseResourceEntryViewModel*> PreviousViewModelsByDefinition;
	for (URpgBaseResourceEntryViewModel* ExistingViewModel : Resources)
	{
		const TSubclassOf<URpgInventoryItemDefinition> ExistingDefinition = ExistingViewModel ? ExistingViewModel->GetItemDefinition() : nullptr;
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

	const int32 NewResourceCount = NewResources.Num();
	const bool bResourcesChanged = Resources != NewResources;
	const bool bResourceCountChanged = ResourceCount != NewResourceCount;
	Resources = MoveTemp(NewResources);
	ResourceCount = NewResourceCount;
	if (bResourcesChanged)
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
