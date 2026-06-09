// Copyright Epic Games, Inc. All Rights Reserved.
#include "RpgInventoryManagerComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"
#include "NativeGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Inventory_Message_StackChanged, "Rpg.Inventory.Message.StackChanged");

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	int32 GetMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		if (const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(ItemDef))
		{
			return Traits->GetMaxStackSize();
		}

		return 1;
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

//////////////////////////////////////////////////////////////////////
// FRpgInventoryEntry

FString FRpgInventoryEntry::GetDebugString() const
{
	TSubclassOf<URpgInventoryItemDefinition> ItemDef;
	if (Instance != nullptr)
	{
		ItemDef = Instance->GetItemDef();
	}

	return FString::Printf(TEXT("%s [%s] (%d x %s @ %d)"), *EntryId.ToString(), *GetNameSafe(Instance), StackCount, *GetNameSafe(ItemDef), SortIndex);
}

//////////////////////////////////////////////////////////////////////
// FRpgInventoryList

void FRpgInventoryList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FRpgInventoryEntry& Stack = Entries[Index];
		BroadcastChangeMessage(Stack, /*OldCount=*/ Stack.StackCount, /*NewCount=*/ 0);
		Stack.LastObservedCount = 0;
	}
}

void FRpgInventoryList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FRpgInventoryEntry& Stack = Entries[Index];
		BroadcastChangeMessage(Stack, /*OldCount=*/ 0, /*NewCount=*/ Stack.StackCount);
		Stack.LastObservedCount = Stack.StackCount;
	}
}

void FRpgInventoryList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		FRpgInventoryEntry& Stack = Entries[Index];
		check(Stack.LastObservedCount != INDEX_NONE);
		BroadcastChangeMessage(Stack, /*OldCount=*/ Stack.LastObservedCount, /*NewCount=*/ Stack.StackCount, /*bOrderChanged=*/ Stack.LastObservedCount == Stack.StackCount);
		Stack.LastObservedCount = Stack.StackCount;
	}
}

void FRpgInventoryList::BroadcastChangeMessage(FRpgInventoryEntry& Entry, int32 OldCount, int32 NewCount, bool bOrderChanged)
{
	FRpgInventoryChangeMessage Message;
	Message.InventoryOwner = OwnerComponent;
	Message.Instance = Entry.Instance;
	Message.EntryId = Entry.EntryId;
	Message.NewCount = NewCount;
	Message.Delta = NewCount - OldCount;
	Message.SortIndex = Entry.SortIndex;
	Message.bOrderChanged = bOrderChanged;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(OwnerComponent->GetWorld());
	MessageSystem.BroadcastMessage(TAG_Rpg_Inventory_Message_StackChanged, Message);
}

URpgInventoryItemInstance* FRpgInventoryList::AddEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, TArray<URpgInventoryItemInstance*>& OutNewInstances)
{
	URpgInventoryItemInstance* Result = nullptr;

	check(ItemDef != nullptr);
 	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());

	int32 RemainingCount = StackCount;
	if (RemainingCount <= 0)
	{
		return nullptr;
	}

	const int32 MaxStackSize = GetMaxStackSizeForDefinition(ItemDef);
	if (MaxStackSize > 1)
	{
		for (FRpgInventoryEntry& Entry : Entries)
		{
			if (RemainingCount <= 0)
			{
				break;
			}

			if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDef || Entry.StackCount >= MaxStackSize)
			{
				continue;
			}

			const int32 OldCount = Entry.StackCount;
			const int32 CountToAdd = FMath::Min(MaxStackSize - Entry.StackCount, RemainingCount);
			Entry.StackCount += CountToAdd;
			RemainingCount -= CountToAdd;
			if (!Result)
			{
				Result = Entry.Instance.Get();
			}

			MarkItemDirty(Entry);
			BroadcastChangeMessage(Entry, OldCount, Entry.StackCount);
		}
	}

	while (RemainingCount > 0)
	{
		const int32 NewEntryCount = FMath::Min(MaxStackSize, RemainingCount);
		RemainingCount -= NewEntryCount;

		FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.Instance = NewObject<URpgInventoryItemInstance>(OwnerComponent->GetOwner());  //@TODO: Using the actor instead of component as the outer due to UE-127172
		NewEntry.Instance->SetItemDef(ItemDef);
		NewEntry.EntryId = FGuid::NewGuid();
		for (URpgInventoryItemFragment* Fragment : GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
		{
			if (Fragment != nullptr)
			{
				Fragment->OnInstanceCreated(NewEntry.Instance);
			}
		}
		NewEntry.StackCount = NewEntryCount;
		NewEntry.SortIndex = GetNextSortIndex();
		if (!Result)
		{
			Result = NewEntry.Instance.Get();
		}
		OutNewInstances.Add(NewEntry.Instance);

		MarkItemDirty(NewEntry);
		BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
	}


	return Result;
}

void FRpgInventoryList::AddEntry(URpgInventoryItemInstance* Instance, int32 StackCount)
{
	//Noot implemented in lyra
	if (Instance == nullptr || StackCount <= 0)
	{
		return;
	}

	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor && OwningActor->HasAuthority());

	FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = Instance;
	NewEntry.EntryId = FGuid::NewGuid();
	NewEntry.StackCount = StackCount;
	NewEntry.SortIndex = GetNextSortIndex();
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
}

void FRpgInventoryList::RemoveEntry(URpgInventoryItemInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRpgInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			BroadcastChangeMessage(Entry, Entry.StackCount, 0);
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
			return;
		}
	}
}

bool FRpgInventoryList::RemoveEntryStack(URpgInventoryItemInstance* Instance, int32 StackCount, bool& bOutRemovedEntry)
{
	bOutRemovedEntry = false;
	if (Instance == nullptr || StackCount <= 0)
	{
		return false;
	}

	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRpgInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance != Instance)
		{
			continue;
		}

		if (Entry.StackCount < StackCount)
		{
			return false;
		}

		const int32 OldCount = Entry.StackCount;
		Entry.StackCount -= StackCount;

		if (Entry.StackCount <= 0)
		{
			BroadcastChangeMessage(Entry, OldCount, 0);
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
			bOutRemovedEntry = true;
			return true;
		}

		MarkItemDirty(Entry);
		BroadcastChangeMessage(Entry, OldCount, Entry.StackCount);
		return true;
	}

	return false;
}

TArray<URpgInventoryItemInstance*> FRpgInventoryList::GetAllItems() const
{
	TArray<URpgInventoryItemInstance*> Results;
	Results.Reserve(Entries.Num());
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr) //@TODO: Would prefer to not deal with this here and hide it further?
		{
			Results.Add(Entry.Instance);
		}
	}
	return Results;
}

TArray<FRpgInventoryEntryView> FRpgInventoryList::GetAllEntries() const
{
	TArray<FRpgInventoryEntryView> Results;
	Results.Reserve(Entries.Num());

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr)
		{
			FRpgInventoryEntryView& View = Results.AddDefaulted_GetRef();
			View.InventoryOwner = OwnerComponent;
			View.Instance = Entry.Instance;
			View.EntryId = Entry.EntryId;
			View.StackCount = Entry.StackCount;
			View.SortIndex = Entry.SortIndex;
		}
	}

	Results.Sort([](const FRpgInventoryEntryView& A, const FRpgInventoryEntryView& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	return Results;
}

int32 FRpgInventoryList::GetStackCount(URpgInventoryItemInstance* Instance) const
{
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance)
		{
			return Entry.StackCount;
		}
	}

	return 0;
}

bool FRpgInventoryList::ContainsItemInstance(URpgInventoryItemInstance* Instance) const
{
	return GetStackCount(Instance) > 0;
}

bool FRpgInventoryList::ContainsEntry(FGuid EntryId) const
{
	if (!EntryId.IsValid())
	{
		return false;
	}

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.EntryId == EntryId)
		{
			return true;
		}
	}

	return false;
}

bool FRpgInventoryList::ApplySort(ERpgInventorySortMode SortMode)
{
	if (Entries.Num() <= 1)
	{
		NormalizeSortIndices();
		return false;
	}

	TArray<FRpgInventoryEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	for (FRpgInventoryEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
	}

	auto GetEntryDefinition = [](const FRpgInventoryEntry& Entry)
	{
		return Entry.Instance ? Entry.Instance->GetItemDef() : TSubclassOf<URpgInventoryItemDefinition>();
	};

	auto GetEntryName = [&GetEntryDefinition](const FRpgInventoryEntry& Entry)
	{
		return GetDisplayNameForDefinition(GetEntryDefinition(Entry));
	};

	switch (SortMode)
	{
	case ERpgInventorySortMode::Manual:
		SortedEntries.Sort([](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			return A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Name:
		SortedEntries.Sort([&GetEntryName](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			const int32 NameCompare = GetEntryName(A).Compare(GetEntryName(B), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Category:
		SortedEntries.Sort([&GetEntryDefinition, &GetEntryName](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			const int32 CategoryA = static_cast<int32>(GetCategoryForDefinition(GetEntryDefinition(A)));
			const int32 CategoryB = static_cast<int32>(GetCategoryForDefinition(GetEntryDefinition(B)));
			if (CategoryA != CategoryB)
			{
				return CategoryA < CategoryB;
			}

			const int32 NameCompare = GetEntryName(A).Compare(GetEntryName(B), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::StackCount:
		SortedEntries.Sort([&GetEntryName](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			if (A.StackCount != B.StackCount)
			{
				return A.StackCount > B.StackCount;
			}

			const int32 NameCompare = GetEntryName(A).Compare(GetEntryName(B), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Recent:
		SortedEntries.Sort([](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			return A.SortIndex > B.SortIndex;
		});
		break;
	}

	return SetOrderFromSortedEntryPointers(SortedEntries);
}

bool FRpgInventoryList::MoveEntry(FGuid EntryId, int32 TargetIndex)
{
	if (!EntryId.IsValid() || Entries.Num() <= 0)
	{
		return false;
	}

	TArray<FRpgInventoryEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	FRpgInventoryEntry* MovingEntry = nullptr;
	for (FRpgInventoryEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
		if (Entry.EntryId == EntryId)
		{
			MovingEntry = &Entry;
		}
	}

	if (!MovingEntry)
	{
		return false;
	}

	SortedEntries.Sort([](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	SortedEntries.Remove(MovingEntry);
	const int32 ClampedTargetIndex = FMath::Clamp(TargetIndex, 0, SortedEntries.Num());
	SortedEntries.Insert(MovingEntry, ClampedTargetIndex);
	return SetOrderFromSortedEntryPointers(SortedEntries);
}

FRpgInventorySnapshot FRpgInventoryList::ExportSnapshot(FName ContainerId) const
{
	FRpgInventorySnapshot Snapshot;
	Snapshot.ContainerId = ContainerId;
	Snapshot.Entries.Reserve(Entries.Num());

	TArray<const FRpgInventoryEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
	}

	SortedEntries.Sort([](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	for (const FRpgInventoryEntry* Entry : SortedEntries)
	{
		if (!Entry || !Entry->Instance || Entry->StackCount <= 0)
		{
			continue;
		}

		FRpgInventorySnapshotEntry& SnapshotEntry = Snapshot.Entries.AddDefaulted_GetRef();
		SnapshotEntry.EntryId = Entry->EntryId;
		SnapshotEntry.ItemDefinition = Entry->Instance->GetItemDef();
		SnapshotEntry.StackCount = Entry->StackCount;
		SnapshotEntry.SortIndex = Entry->SortIndex;
	}

	return Snapshot;
}

void FRpgInventoryList::ImportSnapshot(const FRpgInventorySnapshot& Snapshot)
{
	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor && OwningActor->HasAuthority());

	for (FRpgInventoryEntry& Entry : Entries)
	{
		BroadcastChangeMessage(Entry, Entry.StackCount, 0);
	}

	Entries.Reset();
	MarkArrayDirty();

	for (const FRpgInventorySnapshotEntry& SnapshotEntry : Snapshot.Entries)
	{
		if (!SnapshotEntry.ItemDefinition || SnapshotEntry.StackCount <= 0)
		{
			continue;
		}

		FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.Instance = NewObject<URpgInventoryItemInstance>(OwnerComponent->GetOwner());
		NewEntry.Instance->SetItemDef(SnapshotEntry.ItemDefinition);
		for (URpgInventoryItemFragment* Fragment : GetDefault<URpgInventoryItemDefinition>(SnapshotEntry.ItemDefinition)->Fragments)
		{
			if (Fragment != nullptr)
			{
				Fragment->OnInstanceCreated(NewEntry.Instance);
			}
		}
		NewEntry.EntryId = SnapshotEntry.EntryId.IsValid() ? SnapshotEntry.EntryId : FGuid::NewGuid();
		NewEntry.StackCount = SnapshotEntry.StackCount;
		NewEntry.SortIndex = SnapshotEntry.SortIndex;
		NewEntry.LastObservedCount = INDEX_NONE;
		MarkItemDirty(NewEntry);
		BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount, true);
	}

	NormalizeSortIndices();
}

int32 FRpgInventoryList::GetNextSortIndex() const
{
	int32 MaxSortIndex = INDEX_NONE;
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		MaxSortIndex = FMath::Max(MaxSortIndex, Entry.SortIndex);
	}

	return MaxSortIndex + 1;
}

void FRpgInventoryList::NormalizeSortIndices()
{
	TArray<FRpgInventoryEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	for (FRpgInventoryEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
	}

	SortedEntries.Sort([](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	SetOrderFromSortedEntryPointers(SortedEntries);
}

void FRpgInventoryList::SortEntriesBySortIndex()
{
	Entries.Sort([](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});
}

bool FRpgInventoryList::SetOrderFromSortedEntryPointers(const TArray<FRpgInventoryEntry*>& SortedEntries)
{
	bool bChanged = false;
	for (int32 Index = 0; Index < SortedEntries.Num(); ++Index)
	{
		FRpgInventoryEntry* Entry = SortedEntries[Index];
		if (!Entry)
		{
			continue;
		}

		const int32 OldSortIndex = Entry->SortIndex;
		if (OldSortIndex != Index)
		{
			Entry->SortIndex = Index;
			MarkItemDirty(*Entry);
			BroadcastChangeMessage(*Entry, Entry->StackCount, Entry->StackCount, true);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		SortEntriesBySortIndex();
		MarkArrayDirty();
	}

	return bChanged;
}

//////////////////////////////////////////////////////////////////////
// URpgInventoryManagerComponent

URpgInventoryManagerComponent::URpgInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InventoryList(this)
{
	SetIsReplicatedByDefault(true);
}

void URpgInventoryManagerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

bool URpgInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount)
{
	return ItemDef != nullptr && StackCount > 0;
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::AddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount)
{
	URpgInventoryItemInstance* Result = nullptr;
	if (ItemDef != nullptr)
	{
		TArray<URpgInventoryItemInstance*> NewInstances;
		Result = InventoryList.AddEntry(ItemDef, StackCount, NewInstances);
		
		if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
		{
			for (URpgInventoryItemInstance* NewInstance : NewInstances)
			{
				if (NewInstance)
				{
					AddReplicatedSubObject(NewInstance);
				}
			}
		}
	}
	return Result;
}

void URpgInventoryManagerComponent::AddItemInstance(URpgInventoryItemInstance* ItemInstance)
{
	AddItemInstanceWithStack(ItemInstance, 1);
}

void URpgInventoryManagerComponent::AddItemInstanceWithStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	InventoryList.AddEntry(ItemInstance, StackCount);
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
	{
		AddReplicatedSubObject(ItemInstance);
	}
}

void URpgInventoryManagerComponent::RemoveItemInstance(URpgInventoryItemInstance* ItemInstance)
{
	InventoryList.RemoveEntry(ItemInstance);

	if (ItemInstance && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}
}

bool URpgInventoryManagerComponent::RemoveItemInstanceStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	bool bRemovedEntry = false;
	const bool bRemovedStack = InventoryList.RemoveEntryStack(ItemInstance, StackCount, bRemovedEntry);

	if (bRemovedStack && bRemovedEntry && ItemInstance && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}

	return bRemovedStack;
}

TArray<URpgInventoryItemInstance*> URpgInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

TArray<FRpgInventoryEntryView> URpgInventoryManagerComponent::GetAllEntries() const
{
	return InventoryList.GetAllEntries();
}

bool URpgInventoryManagerComponent::ContainsItemInstance(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.ContainsItemInstance(ItemInstance);
}

bool URpgInventoryManagerComponent::ContainsEntry(FGuid EntryId) const
{
	return InventoryList.ContainsEntry(EntryId);
}

int32 URpgInventoryManagerComponent::GetItemStackCount(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.GetStackCount(ItemInstance);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::FindFirstItemStackByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

int32 URpgInventoryManagerComponent::GetTotalItemCountByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	int32 TotalCount = 0;
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				TotalCount += Entry.StackCount;
			}
		}
	}

	return TotalCount;
}

bool URpgInventoryManagerComponent::ConsumeItemsByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 NumToConsume)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	if (GetTotalItemCountByDefinition(ItemDef) < NumToConsume)
	{
		return false;
	}

	int32 RemainingToConsume = NumToConsume;
	while (RemainingToConsume > 0)
	{
		URpgInventoryItemInstance* Instance = FindFirstItemStackByDefinition(ItemDef);
		if (Instance == nullptr)
		{
			return false;
		}

		const int32 CountToConsume = FMath::Min(RemainingToConsume, GetItemStackCount(Instance));
		if (!RemoveItemInstanceStack(Instance, CountToConsume))
		{
			return false;
		}

		RemainingToConsume -= CountToConsume;
	}

	return true;
}

bool URpgInventoryManagerComponent::ApplyInventorySort(ERpgInventorySortMode SortMode)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	return InventoryList.ApplySort(SortMode);
}

bool URpgInventoryManagerComponent::MoveInventoryEntry(FGuid EntryId, int32 TargetIndex)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	return InventoryList.MoveEntry(EntryId, TargetIndex);
}

FRpgInventorySnapshot URpgInventoryManagerComponent::ExportInventorySnapshot(FName ContainerId) const
{
	return InventoryList.ExportSnapshot(ContainerId);
}

void URpgInventoryManagerComponent::ImportInventorySnapshot(const FRpgInventorySnapshot& Snapshot)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
		{
			if (Entry.Instance)
			{
				RemoveReplicatedSubObject(Entry.Instance);
			}
		}
	}

	InventoryList.ImportSnapshot(Snapshot);

	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
		{
			if (Entry.Instance)
			{
				AddReplicatedSubObject(Entry.Instance);
			}
		}
	}
}

void URpgInventoryManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Register existing URpgInventoryItemInstance
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
		{
			URpgInventoryItemInstance* Instance = Entry.Instance;

			if (IsValid(Instance))
			{
				AddReplicatedSubObject(Instance);
			}
		}
	}
}

bool URpgInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (Instance && IsValid(Instance))
		{
			WroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

//////////////////////////////////////////////////////////////////////
//

// UCLASS(Abstract)
// class URpgInventoryFilter : public UObject
// {
// public:
// 	virtual bool PassesFilter(URpgInventoryItemInstance* Instance) const { return true; }
// };

// UCLASS()
// class URpgInventoryFilter_HasTag : public URpgInventoryFilter
// {
// public:
// 	virtual bool PassesFilter(URpgInventoryItemInstance* Instance) const { return true; }
// };


