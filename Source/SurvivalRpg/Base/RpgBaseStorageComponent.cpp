#include "RpgBaseStorageComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_BaseStorage_Message_Changed, "Rpg.BaseStorage.Message.Changed");

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	FString GetDisplayNameForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? ItemCDO->DisplayName.ToString() : FString();
	}

	ERpgInventoryItemCategory GetCategoryForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		if (const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(ItemDef))
		{
			return Traits->ItemCategory;
		}

		return ERpgInventoryItemCategory::Misc;
	}
}

TArray<FRpgBaseResourceEntryView> FRpgBaseResourceList::GetAllResources() const
{
	TArray<FRpgBaseResourceEntryView> Results;
	Results.Reserve(Entries.Num());

	for (const FRpgBaseResourceEntry& Entry : Entries)
	{
		if (!Entry.ItemDefinition)
		{
			continue;
		}

		FRpgBaseResourceEntryView& View = Results.AddDefaulted_GetRef();
		View.ItemDefinition = Entry.ItemDefinition;
		View.Count = Entry.Count;
		View.Capacity = Entry.Capacity;
		View.SortIndex = Entry.SortIndex;
	}

	Results.Sort([](const FRpgBaseResourceEntryView& A, const FRpgBaseResourceEntryView& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	return Results;
}

int32 FRpgBaseResourceList::GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	return Entry ? Entry->Count : 0;
}

int32 FRpgBaseResourceList::GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	return Entry ? Entry->Capacity : 0;
}

int32 FRpgBaseResourceList::GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	return Entry ? FMath::Max(0, Entry->Capacity - Entry->Count) : 0;
}

bool FRpgBaseResourceList::CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	return ItemDefinition && Count > 0 && GetFreeResourceCapacity(ItemDefinition) >= Count;
}

bool FRpgBaseResourceList::StoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	if (!CanStoreResource(ItemDefinition, Count))
	{
		return false;
	}

	FRpgBaseResourceEntry& Entry = FindOrAddEntry(ItemDefinition);
	const int32 OldCount = Entry.Count;
	const int32 OldCapacity = Entry.Capacity;
	Entry.Count += Count;
	MarkItemDirty(Entry);
	BroadcastChangeMessage(Entry, OldCount, OldCapacity);
	return true;
}

bool FRpgBaseResourceList::WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	if (!Entry || Count <= 0 || Entry->Count < Count)
	{
		return false;
	}

	const int32 OldCount = Entry->Count;
	const int32 OldCapacity = Entry->Capacity;
	Entry->Count -= Count;
	MarkItemDirty(*Entry);
	BroadcastChangeMessage(*Entry, OldCount, OldCapacity);
	return true;
}

void FRpgBaseResourceList::AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity)
{
	if (!ItemDefinition || DeltaCapacity == 0)
	{
		return;
	}

	FRpgBaseResourceEntry& Entry = FindOrAddEntry(ItemDefinition);
	const int32 OldCount = Entry.Count;
	const int32 OldCapacity = Entry.Capacity;
	Entry.Capacity = FMath::Max(Entry.Count, Entry.Capacity + DeltaCapacity);
	MarkItemDirty(Entry);
	BroadcastChangeMessage(Entry, OldCount, OldCapacity);
}

bool FRpgBaseResourceList::ApplySort(ERpgInventorySortMode SortMode)
{
	if (Entries.Num() <= 1)
	{
		return false;
	}

	TArray<FRpgBaseResourceEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	for (FRpgBaseResourceEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
	}

	switch (SortMode)
	{
	case ERpgInventorySortMode::Manual:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			return A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Name:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			const int32 NameCompare = GetDisplayNameForDefinition(A.ItemDefinition).Compare(GetDisplayNameForDefinition(B.ItemDefinition), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Category:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			const int32 CategoryA = static_cast<int32>(GetCategoryForDefinition(A.ItemDefinition));
			const int32 CategoryB = static_cast<int32>(GetCategoryForDefinition(B.ItemDefinition));
			if (CategoryA != CategoryB)
			{
				return CategoryA < CategoryB;
			}

			const int32 NameCompare = GetDisplayNameForDefinition(A.ItemDefinition).Compare(GetDisplayNameForDefinition(B.ItemDefinition), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::StackCount:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			if (A.Count != B.Count)
			{
				return A.Count > B.Count;
			}

			const int32 NameCompare = GetDisplayNameForDefinition(A.ItemDefinition).Compare(GetDisplayNameForDefinition(B.ItemDefinition), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Recent:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			return A.SortIndex > B.SortIndex;
		});
		break;
	}

	return SetOrderFromSortedEntryPointers(SortedEntries);
}

bool FRpgBaseResourceList::MoveResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	if (!ItemDefinition || Entries.Num() <= 0)
	{
		return false;
	}

	TArray<FRpgBaseResourceEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	FRpgBaseResourceEntry* MovingEntry = nullptr;
	for (FRpgBaseResourceEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
		if (Entry.ItemDefinition == ItemDefinition)
		{
			MovingEntry = &Entry;
		}
	}

	if (!MovingEntry)
	{
		return false;
	}

	SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	SortedEntries.Remove(MovingEntry);
	SortedEntries.Insert(MovingEntry, FMath::Clamp(TargetIndex, 0, SortedEntries.Num()));
	return SetOrderFromSortedEntryPointers(SortedEntries);
}

void FRpgBaseResourceList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FRpgBaseResourceEntry& Entry = Entries[Index];
		BroadcastChangeMessage(Entry, Entry.Count, Entry.Capacity);
		Entry.LastObservedCount = 0;
		Entry.LastObservedCapacity = 0;
	}
}

void FRpgBaseResourceList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FRpgBaseResourceEntry& Entry = Entries[Index];
		BroadcastChangeMessage(Entry, 0, 0);
		Entry.LastObservedCount = Entry.Count;
		Entry.LastObservedCapacity = Entry.Capacity;
	}
}

void FRpgBaseResourceList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		FRpgBaseResourceEntry& Entry = Entries[Index];
		check(Entry.LastObservedCount != INDEX_NONE);
		check(Entry.LastObservedCapacity != INDEX_NONE);
		BroadcastChangeMessage(Entry, Entry.LastObservedCount, Entry.LastObservedCapacity, Entry.LastObservedCount == Entry.Count && Entry.LastObservedCapacity == Entry.Capacity);
		Entry.LastObservedCount = Entry.Count;
		Entry.LastObservedCapacity = Entry.Capacity;
	}
}

FRpgBaseResourceEntry* FRpgBaseResourceList::FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	for (FRpgBaseResourceEntry& Entry : Entries)
	{
		if (Entry.ItemDefinition == ItemDefinition)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FRpgBaseResourceEntry* FRpgBaseResourceList::FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	for (const FRpgBaseResourceEntry& Entry : Entries)
	{
		if (Entry.ItemDefinition == ItemDefinition)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FRpgBaseResourceEntry& FRpgBaseResourceList::FindOrAddEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	if (FRpgBaseResourceEntry* ExistingEntry = FindEntry(ItemDefinition))
	{
		return *ExistingEntry;
	}

	FRpgBaseResourceEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.ItemDefinition = ItemDefinition;
	NewEntry.SortIndex = GetNextSortIndex();
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, 0, true);
	return NewEntry;
}

void FRpgBaseResourceList::BroadcastChangeMessage(FRpgBaseResourceEntry& Entry, int32 OldCount, int32 OldCapacity, bool bOrderChanged)
{
	if (!OwnerComponent || !OwnerComponent->GetWorld())
	{
		return;
	}

	FRpgBaseResourceChangeMessage Message;
	Message.StorageOwner = OwnerComponent;
	Message.ItemDefinition = Entry.ItemDefinition;
	Message.NewCount = Entry.Count;
	Message.Delta = Entry.Count - OldCount;
	Message.Capacity = Entry.Capacity;
	Message.SortIndex = Entry.SortIndex;
	Message.bCapacityChanged = Entry.Capacity != OldCapacity;
	Message.bOrderChanged = bOrderChanged;

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(OwnerComponent->GetWorld());
	MessageSubsystem.BroadcastMessage(TAG_Rpg_BaseStorage_Message_Changed, Message);
}

int32 FRpgBaseResourceList::GetNextSortIndex() const
{
	int32 MaxSortIndex = INDEX_NONE;
	for (const FRpgBaseResourceEntry& Entry : Entries)
	{
		MaxSortIndex = FMath::Max(MaxSortIndex, Entry.SortIndex);
	}

	return MaxSortIndex + 1;
}

void FRpgBaseResourceList::SortEntriesBySortIndex()
{
	Entries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});
}

bool FRpgBaseResourceList::SetOrderFromSortedEntryPointers(const TArray<FRpgBaseResourceEntry*>& SortedEntries)
{
	bool bChanged = false;
	for (int32 Index = 0; Index < SortedEntries.Num(); ++Index)
	{
		FRpgBaseResourceEntry* Entry = SortedEntries[Index];
		if (!Entry || Entry->SortIndex == Index)
		{
			continue;
		}

		const int32 OldCount = Entry->Count;
		const int32 OldCapacity = Entry->Capacity;
		Entry->SortIndex = Index;
		MarkItemDirty(*Entry);
		BroadcastChangeMessage(*Entry, OldCount, OldCapacity, true);
		bChanged = true;
	}

	if (bChanged)
	{
		SortEntriesBySortIndex();
		MarkArrayDirty();
	}

	return bChanged;
}

URpgBaseStorageComponent::URpgBaseStorageComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ResourceList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgBaseStorageComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	for (const FRpgBaseResourceCapacity& DefaultCapacity : DefaultResourceCapacities)
	{
		AddResourceCapacity(DefaultCapacity.ItemDefinition, DefaultCapacity.Capacity);
	}
}

void URpgBaseStorageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ResourceList);
}

TArray<FRpgBaseResourceEntryView> URpgBaseStorageComponent::GetAllResources() const
{
	return ResourceList.GetAllResources();
}

int32 URpgBaseStorageComponent::GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	return ResourceList.GetResourceCount(ItemDefinition);
}

int32 URpgBaseStorageComponent::GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	return ResourceList.GetResourceCapacity(ItemDefinition);
}

int32 URpgBaseStorageComponent::GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	return ResourceList.GetFreeResourceCapacity(ItemDefinition);
}

bool URpgBaseStorageComponent::CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	return ResourceList.CanStoreResource(ItemDefinition, Count);
}

bool URpgBaseStorageComponent::CanStoreResourceInstance(
	const URpgInventoryItemInstance* Item,
	int32 Count) const
{
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Item ? Item->GetItemDef() : nullptr;
	const URpgInventoryFragment_ItemTraits* Traits =
		GetItemTraits(ItemDefinition);
	return Item && Traits && Traits->IsMaterial() &&
		Item->CanCollapseIntoDefinitionCount() &&
		ResourceList.CanStoreResource(ItemDefinition, Count);
}

bool URpgBaseStorageComponent::StoreDefinitionResource(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 Count)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	return ResourceList.StoreResource(ItemDefinition, Count);
}

bool URpgBaseStorageComponent::StoreResourceInstance(
	const URpgInventoryItemInstance* Item,
	int32 Count)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!CanStoreResourceInstance(Item, Count))
	{
		return false;
	}

	return ResourceList.StoreResource(Item->GetItemDef(), Count);
}

bool URpgBaseStorageComponent::WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	return ResourceList.WithdrawResource(ItemDefinition, Count);
}

void URpgBaseStorageComponent::AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	ResourceList.AddResourceCapacity(ItemDefinition, DeltaCapacity);
}

bool URpgBaseStorageComponent::ApplyResourceSort(ERpgInventorySortMode SortMode)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	return ResourceList.ApplySort(SortMode);
}

bool URpgBaseStorageComponent::MoveResourceEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	return ResourceList.MoveResource(ItemDefinition, TargetIndex);
}
