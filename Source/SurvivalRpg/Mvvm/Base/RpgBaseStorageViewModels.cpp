#include "RpgBaseStorageViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageViewModels)

namespace
{
	constexpr float BaseStorageCapacityWarningThreshold = 0.8f;

	constexpr ETextIdenticalModeFlags BaseStorageTextIdentityFlags =
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

	ERpgInventoryItemCategory GetBaseStorageItemCategory(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetBaseStorageItemDefinitionCDO(ItemDefinition);
		const URpgInventoryFragment_ItemTraits* Traits = ItemCDO
			? Cast<URpgInventoryFragment_ItemTraits>(
				ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass()))
			: nullptr;
		return Traits ? Traits->ItemCategory : ERpgInventoryItemCategory::None;
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
	const ERpgInventoryItemCategory NewItemCategory = GetBaseStorageItemCategory(NewItemDefinition);
	const TSoftObjectPtr<UTexture2D> NewIcon = GetBaseStorageItemIcon(NewItemDefinition);
	const bool bNewIsEmpty = NewCount <= 0;
	const bool bNewIsFull = NewCapacity > 0 && NewCount >= NewCapacity;
	const bool bNewHasCapacityWarning =
		NewCapacity > 0 && NewFillRatio >= BaseStorageCapacityWarningThreshold;

	const bool bItemDefinitionChanged = ItemDefinition != NewItemDefinition;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, BaseStorageTextIdentityFlags);
	const bool bItemCategoryChanged = ItemCategory != NewItemCategory;
	const bool bIconChanged = Icon != NewIcon;
	const bool bCountChanged = Count != NewCount;
	const bool bCapacityChanged = Capacity != NewCapacity;
	const bool bFreeCapacityChanged = FreeCapacity != NewFreeCapacity;
	const bool bFillRatioChanged = FillRatio != NewFillRatio;
	const bool bSortIndexChanged = SortIndex != Entry.SortIndex;
	const bool bIsEmptyChanged = bIsEmpty != bNewIsEmpty;
	const bool bIsFullChanged = bIsFull != bNewIsFull;
	const bool bHasCapacityWarningChanged =
		bHasCapacityWarning != bNewHasCapacityWarning;

	ItemDefinition = NewItemDefinition;
	DisplayName = NewDisplayName;
	ItemCategory = NewItemCategory;
	Icon = NewIcon;
	Count = NewCount;
	Capacity = NewCapacity;
	FreeCapacity = NewFreeCapacity;
	FillRatio = NewFillRatio;
	SortIndex = Entry.SortIndex;
	bIsEmpty = bNewIsEmpty;
	bIsFull = bNewIsFull;
	bHasCapacityWarning = bNewHasCapacityWarning;

	if (bItemDefinitionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemDefinition);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bItemCategoryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemCategory);
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
	if (bHasCapacityWarningChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasCapacityWarning);
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
	CancelQueuedRefreshResources();
	const TArray<TSubclassOf<URpgInventoryItemDefinition>> EmptyAllowedResources;
	const TArray<TObjectPtr<URpgBaseResourceEntryViewModel>> EmptyResources;
	const bool bObservedBaseStorageChanged = ObservedBaseStorage != nullptr;
	const bool bAllowedResourcesChanged = !AllowedResources.IsEmpty();
	const bool bResourcesChanged = !Resources.IsEmpty();
	const bool bResourceCountChanged = ResourceCount != 0;
	const bool bSummaryChanged = Summary != FRpgBaseStorageLocalSummary();
	ObservedBaseStorage = nullptr;
	AllowedResources = EmptyAllowedResources;
	Resources = EmptyResources;
	AllResourceRows = EmptyResources;
	ResourceCount = 0;
	Summary = FRpgBaseStorageLocalSummary();

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
	if (bSummaryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Summary);
	}
	OnResourcesChanged.Broadcast();
}

void URpgBaseStorageViewModel::RefreshResources()
{
	CancelQueuedRefreshResources();
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

void URpgBaseStorageViewModel::SetSearchText(FText InSearchText)
{
	if (SearchText.IdenticalTo(InSearchText, BaseStorageTextIdentityFlags))
	{
		return;
	}

	SearchText = MoveTemp(InSearchText);
	RefreshResources();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SearchText);
}

void URpgBaseStorageViewModel::SetCategoryFilter(ERpgInventoryItemCategory InCategoryFilter)
{
	if (CategoryFilter == InCategoryFilter)
	{
		return;
	}

	CategoryFilter = InCategoryFilter;
	RefreshResources();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CategoryFilter);
}

void URpgBaseStorageViewModel::SetLocalSort(
	ERpgBaseResourceLocalSortMode InSortMode,
	bool bInSortDescending)
{
	if (LocalSortMode == InSortMode && bSortDescending == bInSortDescending)
	{
		return;
	}

	const bool bSortModeChanged = LocalSortMode != InSortMode;
	const bool bDirectionChanged = bSortDescending != bInSortDescending;
	LocalSortMode = InSortMode;
	bSortDescending = bInSortDescending;
	RefreshResources();
	if (bSortModeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(LocalSortMode);
	}
	if (bDirectionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bSortDescending);
	}
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

void URpgBaseStorageViewModel::RequestRefreshResources()
{
	UWorld* World = ObservedBaseStorage ? ObservedBaseStorage->GetWorld() : nullptr;
	if (!World)
	{
		RefreshResources();
		return;
	}

	RefreshResourcesQueue.Queue(
		World,
		this,
		&ThisClass::ExecuteQueuedRefreshResources);
}

void URpgBaseStorageViewModel::ExecuteQueuedRefreshResources()
{
	if (!RefreshResourcesQueue.Consume())
	{
		return;
	}

	RefreshResources();
}

void URpgBaseStorageViewModel::CancelQueuedRefreshResources()
{
	RefreshResourcesQueue.Cancel();
}

void URpgBaseStorageViewModel::RebuildResources()
{
	TMap<TSubclassOf<URpgInventoryItemDefinition>, URpgBaseResourceEntryViewModel*> PreviousViewModelsByDefinition;
	for (URpgBaseResourceEntryViewModel* ExistingViewModel : AllResourceRows)
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

	TArray<TObjectPtr<URpgBaseResourceEntryViewModel>> NewAllResourceRows;
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
			NewAllResourceRows.Add(ResourceViewModel);
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
			Entry.SortIndex = NewAllResourceRows.Num();

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
			NewAllResourceRows.Add(ResourceViewModel);
		}
	}

	FRpgBaseStorageLocalSummary NewSummary;
	NewSummary.TotalResourceCount = NewAllResourceRows.Num();
	if (ObservedBaseStorage)
	{
		NewSummary.UsedCapacityPoints =
			ObservedBaseStorage->GetUsedMaterialCapacityPoints();
		NewSummary.MaterialCapacityPoints =
			ObservedBaseStorage->GetMaterialCapacityPoints();
	}
	for (const URpgBaseResourceEntryViewModel* Resource : NewAllResourceRows)
	{
		if (!Resource)
		{
			continue;
		}

		NewSummary.TotalStoredUnits += Resource->GetCount();
		NewSummary.TotalCapacity += Resource->GetCapacity();
		NewSummary.CapacityWarningCount += Resource->HasCapacityWarning() ? 1 : 0;
		NewSummary.FullResourceCount +=
			Resource->GetCapacity() > 0 && Resource->GetCount() >= Resource->GetCapacity()
				? 1
				: 0;
	}
	if (NewSummary.MaterialCapacityPoints > 0)
	{
		NewSummary.AggregateFillRatio = FMath::Clamp(
			static_cast<float>(NewSummary.UsedCapacityPoints) /
				static_cast<float>(NewSummary.MaterialCapacityPoints),
			0.0f,
			1.0f);
	}
	else if (NewSummary.TotalCapacity > 0)
	{
		NewSummary.AggregateFillRatio = FMath::Clamp(
			static_cast<float>(NewSummary.TotalStoredUnits) /
				static_cast<float>(NewSummary.TotalCapacity),
			0.0f,
			1.0f);
	}
	NewSummary.bHasCapacityWarning =
		NewSummary.AggregateFillRatio >= BaseStorageCapacityWarningThreshold ||
		NewSummary.CapacityWarningCount > 0;

	const FString SearchQuery = SearchText.ToString().TrimStartAndEnd();
	TArray<TObjectPtr<URpgBaseResourceEntryViewModel>> NewResources;
	NewResources.Reserve(NewAllResourceRows.Num());
	for (URpgBaseResourceEntryViewModel* Resource : NewAllResourceRows)
	{
		if (!Resource)
		{
			continue;
		}

		if (CategoryFilter != ERpgInventoryItemCategory::None &&
			Resource->GetItemCategory() != CategoryFilter)
		{
			continue;
		}
		if (!SearchQuery.IsEmpty() &&
			!Resource->GetDisplayName().ToString().Contains(
				SearchQuery,
				ESearchCase::IgnoreCase))
		{
			continue;
		}

		NewResources.Add(Resource);
	}

	if (LocalSortMode != ERpgBaseResourceLocalSortMode::ReplicatedOrder)
	{
		NewResources.StableSort(
			[this](const URpgBaseResourceEntryViewModel& Left,
				const URpgBaseResourceEntryViewModel& Right)
			{
				int32 Comparison = 0;
				switch (LocalSortMode)
				{
				case ERpgBaseResourceLocalSortMode::Name:
					Comparison = Left.GetDisplayName().ToString().Compare(
						Right.GetDisplayName().ToString(),
						ESearchCase::IgnoreCase);
					break;
				case ERpgBaseResourceLocalSortMode::Category:
					Comparison = FMath::Sign(
						static_cast<int32>(Left.GetItemCategory()) -
						static_cast<int32>(Right.GetItemCategory()));
					break;
				case ERpgBaseResourceLocalSortMode::StoredCount:
					Comparison = FMath::Sign(Left.GetCount() - Right.GetCount());
					break;
				case ERpgBaseResourceLocalSortMode::FreeCapacity:
					Comparison = FMath::Sign(
						Left.GetFreeCapacity() - Right.GetFreeCapacity());
					break;
				case ERpgBaseResourceLocalSortMode::FillRatio:
					Comparison = Left.GetFillRatio() < Right.GetFillRatio()
						? -1
						: (Left.GetFillRatio() > Right.GetFillRatio() ? 1 : 0);
					break;
				case ERpgBaseResourceLocalSortMode::ReplicatedOrder:
				default:
					break;
				}

				if (Comparison != 0)
				{
					return bSortDescending ? Comparison > 0 : Comparison < 0;
				}
				if (Left.GetSortIndex() != Right.GetSortIndex())
				{
					return Left.GetSortIndex() < Right.GetSortIndex();
				}
				return Left.GetItemDefinition()->GetPathName() <
					Right.GetItemDefinition()->GetPathName();
			});
	}

	const int32 NewResourceCount = NewResources.Num();
	NewSummary.VisibleResourceCount = NewResourceCount;
	const bool bResourcesChanged = Resources != NewResources;
	const bool bResourceCountChanged = ResourceCount != NewResourceCount;
	const bool bSummaryChanged = Summary != NewSummary;
	AllResourceRows = MoveTemp(NewAllResourceRows);
	Resources = MoveTemp(NewResources);
	ResourceCount = NewResourceCount;
	Summary = NewSummary;
	if (bResourcesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Resources);
	}
	if (bResourceCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ResourceCount);
	}
	if (bSummaryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Summary);
	}

	OnResourcesChanged.Broadcast();
}

void URpgBaseStorageViewModel::HandleBaseStorageChanged(FGameplayTag Channel, const FRpgBaseResourceChangeMessage& Message)
{
	if (ObservedBaseStorage && Message.StorageOwner.Get() == ObservedBaseStorage)
	{
		RequestRefreshResources();
	}
}
