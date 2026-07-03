// Copyright Epic Games, Inc. All Rights Reserved.
#include "RpgInventoryManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Inventory_Message_StackChanged, "Rpg.Inventory.Message.StackChanged");

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryManager, Log, All);

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	int32 GetInventoryManagerMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
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

	bool IsValidInventorySlotIndex(const URpgInventoryManagerComponent* Inventory, int32 SlotIndex)
	{
		if (!Inventory || SlotIndex < 0)
		{
			return false;
		}

		return Inventory->IsCapacityUnlimited() || SlotIndex < Inventory->GetMaxEntries();
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
	Message.bCapacityChanged = false;

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

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemDef);
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
		const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
		const int32 NewSlotIndex = Inventory ? Inventory->GetNextAutoAddSlotForItemDefinition(ItemDef) : GetNextAvailableSlotIndex();
		if (NewSlotIndex == INDEX_NONE)
		{
			UE_LOG(LogRpgInventoryManager, Warning, TEXT("AddEntry failed: no free finite slot. Inventory=%s ItemDef=%s RemainingCount=%d UsedEntries=%d"),
				*GetNameSafe(OwnerComponent),
				*GetNameSafe(ItemDef),
				RemainingCount,
				GetUsedEntryCount());
			break;
		}

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
		NewEntry.SortIndex = NewSlotIndex;
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

URpgInventoryItemInstance* FRpgInventoryList::AddEntryAtSlot(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, int32 SlotIndex, TArray<URpgInventoryItemInstance*>& OutNewInstances)
{
	check(ItemDef != nullptr);
	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor && OwningActor->HasAuthority());

	if (StackCount <= 0 || !IsValidInventorySlotIndex(Cast<URpgInventoryManagerComponent>(OwnerComponent), SlotIndex))
	{
		return nullptr;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemDef);
	if (StackCount > MaxStackSize)
	{
		return nullptr;
	}

	if (FRpgInventoryEntry* ExistingEntry = FindEntryBySlotIndex(SlotIndex))
	{
		if (!ExistingEntry->Instance || ExistingEntry->Instance->GetItemDef() != ItemDef || MaxStackSize <= 1)
		{
			return nullptr;
		}

		const int32 FreeCapacity = FMath::Max(0, MaxStackSize - ExistingEntry->StackCount);
		if (StackCount > FreeCapacity)
		{
			return nullptr;
		}

		const int32 OldCount = ExistingEntry->StackCount;
		ExistingEntry->StackCount += StackCount;
		MarkItemDirty(*ExistingEntry);
		BroadcastChangeMessage(*ExistingEntry, OldCount, ExistingEntry->StackCount);
		return ExistingEntry->Instance.Get();
	}

	FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = NewObject<URpgInventoryItemInstance>(OwnerComponent->GetOwner());
	NewEntry.Instance->SetItemDef(ItemDef);
	NewEntry.EntryId = FGuid::NewGuid();
	for (URpgInventoryItemFragment* Fragment : GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment != nullptr)
		{
			Fragment->OnInstanceCreated(NewEntry.Instance);
		}
	}
	NewEntry.StackCount = StackCount;
	NewEntry.SortIndex = SlotIndex;
	OutNewInstances.Add(NewEntry.Instance);

	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
	return NewEntry.Instance.Get();
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

	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	const int32 NewSlotIndex = Inventory ? Inventory->GetNextAutoAddSlotForItemInstance(Instance) : GetNextAvailableSlotIndex();
	if (NewSlotIndex == INDEX_NONE)
	{
		UE_LOG(LogRpgInventoryManager, Warning, TEXT("AddEntry instance failed: no free finite slot. Inventory=%s Item=%s StackCount=%d UsedEntries=%d"),
			*GetNameSafe(OwnerComponent),
			*GetNameSafe(Instance),
			StackCount,
			GetUsedEntryCount());
		return;
	}

	FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = Instance;
	NewEntry.EntryId = FGuid::NewGuid();
	NewEntry.StackCount = StackCount;
	NewEntry.SortIndex = NewSlotIndex;
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
}

void FRpgInventoryList::AddEntryAtSlot(URpgInventoryItemInstance* Instance, int32 StackCount, int32 SlotIndex)
{
	if (Instance == nullptr || StackCount <= 0 || !IsValidInventorySlotIndex(Cast<URpgInventoryManagerComponent>(OwnerComponent), SlotIndex) || FindEntryBySlotIndex(SlotIndex) != nullptr)
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
	NewEntry.SortIndex = SlotIndex;
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
}

bool FRpgInventoryList::AddStackToEntry(URpgInventoryItemInstance* Instance, int32 StackCount)
{
	if (!Instance || StackCount <= 0)
	{
		return false;
	}

	FRpgInventoryEntry* Entry = FindEntryByInstance(Instance);
	if (!Entry || !Entry->Instance)
	{
		return false;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(Entry->Instance->GetItemDef());
	const int32 FreeCapacity = FMath::Max(0, MaxStackSize - Entry->StackCount);
	if (StackCount > FreeCapacity)
	{
		return false;
	}

	const int32 OldCount = Entry->StackCount;
	Entry->StackCount += StackCount;
	MarkItemDirty(*Entry);
	BroadcastChangeMessage(*Entry, OldCount, Entry->StackCount);
	return true;
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

int32 FRpgInventoryList::GetUsedEntryCount() const
{
	int32 UsedCount = 0;
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr && Entry.StackCount > 0)
		{
			++UsedCount;
		}
	}

	return UsedCount;
}

int32 FRpgInventoryList::GetRequiredNewEntryCount(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const
{
	if (!ItemDef || StackCount <= 0)
	{
		return 0;
	}

	int32 RemainingCount = StackCount;
	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemDef);
	if (MaxStackSize > 1)
	{
		for (const FRpgInventoryEntry& Entry : Entries)
		{
			if (RemainingCount <= 0)
			{
				break;
			}

			if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDef || Entry.StackCount >= MaxStackSize)
			{
				continue;
			}

			RemainingCount -= FMath::Min(MaxStackSize - Entry.StackCount, RemainingCount);
		}
	}

	return RemainingCount > 0 ? FMath::DivideAndRoundUp(RemainingCount, MaxStackSize) : 0;
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

URpgInventoryItemInstance* FRpgInventoryList::GetItemInSlot(int32 SlotIndex) const
{
	const FRpgInventoryEntry* Entry = FindEntryBySlotIndex(SlotIndex);
	return Entry ? Entry->Instance.Get() : nullptr;
}

int32 FRpgInventoryList::GetSlotIndex(URpgInventoryItemInstance* Instance) const
{
	const FRpgInventoryEntry* Entry = FindEntryByInstance(Instance);
	return Entry ? Entry->SortIndex : INDEX_NONE;
}

int32 FRpgInventoryList::GetFreeStackCapacity(URpgInventoryItemInstance* Instance) const
{
	const FRpgInventoryEntry* Entry = FindEntryByInstance(Instance);
	if (!Entry || !Entry->Instance)
	{
		return 0;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(Entry->Instance->GetItemDef());
	return FMath::Max(0, MaxStackSize - Entry->StackCount);
}

bool FRpgInventoryList::ApplySort(ERpgInventorySortMode SortMode)
{
	if (Entries.Num() <= 1)
	{
		if (SortMode == ERpgInventorySortMode::Manual || Entries.Num() == 0)
		{
			return false;
		}

		TArray<FRpgInventoryEntry*> SingleEntry;
		SingleEntry.Add(&Entries[0]);
		return SetOrderFromSortedEntryPointers(SingleEntry);
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

bool FRpgInventoryList::MoveEntryToSlot(FGuid EntryId, int32 TargetSlotIndex)
{
	if (!EntryId.IsValid() || TargetSlotIndex < 0)
	{
		return false;
	}

	FRpgInventoryEntry* MovingEntry = FindEntryByEntryId(EntryId);
	if (!MovingEntry || !MovingEntry->Instance)
	{
		return false;
	}

	FRpgInventoryEntry* TargetEntry = FindEntryBySlotIndex(TargetSlotIndex);
	if (TargetEntry == MovingEntry)
	{
		return true;
	}

	const int32 MovingOldSortIndex = MovingEntry->SortIndex;
	if (!TargetEntry)
	{
		MovingEntry->SortIndex = TargetSlotIndex;
		MarkItemDirty(*MovingEntry);
		BroadcastChangeMessage(*MovingEntry, MovingEntry->StackCount, MovingEntry->StackCount, true);
		SortEntriesBySortIndex();
		MarkArrayDirty();
		return true;
	}

	if (TargetEntry->Instance &&
		MovingEntry->Instance->GetItemDef() == TargetEntry->Instance->GetItemDef())
	{
		const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(TargetEntry->Instance->GetItemDef());
		const int32 FreeCapacity = FMath::Max(0, MaxStackSize - TargetEntry->StackCount);
		if (FreeCapacity > 0)
		{
			const int32 CountToMove = FMath::Min(FreeCapacity, MovingEntry->StackCount);
			const int32 MovingOldCount = MovingEntry->StackCount;
			const int32 TargetOldCount = TargetEntry->StackCount;

			MovingEntry->StackCount -= CountToMove;
			TargetEntry->StackCount += CountToMove;

			MarkItemDirty(*TargetEntry);
			BroadcastChangeMessage(*TargetEntry, TargetOldCount, TargetEntry->StackCount);

			if (MovingEntry->StackCount <= 0)
			{
				URpgInventoryItemInstance* RemovedInstance = MovingEntry->Instance.Get();
				BroadcastChangeMessage(*MovingEntry, MovingOldCount, 0);
				Entries.RemoveAll([EntryId](const FRpgInventoryEntry& Entry)
				{
					return Entry.EntryId == EntryId;
				});
				MarkArrayDirty();
				if (URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent))
				{
					if (RemovedInstance && Inventory->IsUsingRegisteredSubObjectList())
					{
						Inventory->RemoveReplicatedSubObject(RemovedInstance);
					}
				}
			}
			else
			{
				MarkItemDirty(*MovingEntry);
				BroadcastChangeMessage(*MovingEntry, MovingOldCount, MovingEntry->StackCount);
			}

			return true;
		}
	}

	MovingEntry->SortIndex = TargetEntry->SortIndex;
	TargetEntry->SortIndex = MovingOldSortIndex;
	MarkItemDirty(*MovingEntry);
	MarkItemDirty(*TargetEntry);
	BroadcastChangeMessage(*MovingEntry, MovingEntry->StackCount, MovingEntry->StackCount, true);
	BroadcastChangeMessage(*TargetEntry, TargetEntry->StackCount, TargetEntry->StackCount, true);
	SortEntriesBySortIndex();
	MarkArrayDirty();
	return true;
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

	SortEntriesBySortIndex();
}

FRpgInventoryEntry* FRpgInventoryList::FindEntryByInstance(URpgInventoryItemInstance* Instance)
{
	if (!Instance)
	{
		return nullptr;
	}

	for (FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FRpgInventoryEntry* FRpgInventoryList::FindEntryByInstance(URpgInventoryItemInstance* Instance) const
{
	if (!Instance)
	{
		return nullptr;
	}

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FRpgInventoryEntry* FRpgInventoryList::FindEntryByEntryId(FGuid EntryId)
{
	if (!EntryId.IsValid())
	{
		return nullptr;
	}

	for (FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.EntryId == EntryId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FRpgInventoryEntry* FRpgInventoryList::FindEntryByEntryId(FGuid EntryId) const
{
	if (!EntryId.IsValid())
	{
		return nullptr;
	}

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.EntryId == EntryId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FRpgInventoryEntry* FRpgInventoryList::FindEntryBySlotIndex(int32 SlotIndex)
{
	for (FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.SortIndex == SlotIndex && Entry.Instance != nullptr && Entry.StackCount > 0)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FRpgInventoryEntry* FRpgInventoryList::FindEntryBySlotIndex(int32 SlotIndex) const
{
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.SortIndex == SlotIndex && Entry.Instance != nullptr && Entry.StackCount > 0)
		{
			return &Entry;
		}
	}

	return nullptr;
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

int32 FRpgInventoryList::GetNextAvailableSlotIndex() const
{
	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	if (Inventory && !Inventory->IsCapacityUnlimited())
	{
		const int32 MaxEntries = Inventory->GetMaxEntries();
		for (int32 SlotIndex = 0; SlotIndex < MaxEntries; ++SlotIndex)
		{
			if (!FindEntryBySlotIndex(SlotIndex))
			{
				return SlotIndex;
			}
		}

		return INDEX_NONE;
	}

	return GetNextSortIndex();
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

	DOREPLIFETIME(ThisClass, CapacityMode);
	DOREPLIFETIME(ThisClass, FixedMaxEntries);
	DOREPLIFETIME(ThisClass, InventoryList);
	DOREPLIFETIME(ThisClass, InventoryRevision);
}

void URpgInventoryManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCapacityAttributeBinding();
}

void URpgInventoryManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCapacityAttributeBinding();

	Super::EndPlay(EndPlayReason);
}

bool URpgInventoryManagerComponent::IsCapacityUnlimited() const
{
	return CapacityMode == ERpgInventoryCapacityMode::Unlimited;
}

int32 URpgInventoryManagerComponent::GetMaxEntries() const
{
	if (CapacityMode == ERpgInventoryCapacityMode::Unlimited)
	{
		return INDEX_NONE;
	}

	if (CapacityMode == ERpgInventoryCapacityMode::AbilitySystemAttribute && CapacityAttribute.IsValid())
	{
		if (const UAbilitySystemComponent* ASC = FindCapacityAbilitySystem())
		{
			return FMath::Max(0, FMath::RoundToInt(ASC->GetNumericAttribute(CapacityAttribute)));
		}
	}

	return FMath::Max(0, FixedMaxEntries);
}

int32 URpgInventoryManagerComponent::GetUsedEntryCount() const
{
	return InventoryList.GetUsedEntryCount();
}

int32 URpgInventoryManagerComponent::GetFreeEntryCount() const
{
	if (IsCapacityUnlimited())
	{
		return INDEX_NONE;
	}

	return FMath::Max(0, GetMaxEntries() - GetUsedEntryCount());
}

int32 URpgInventoryManagerComponent::GetRequiredNewEntryCountForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const
{
	return InventoryList.GetRequiredNewEntryCount(ItemDef, StackCount);
}

int32 URpgInventoryManagerComponent::GetRequiredNewEntryCountForItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount) const
{
	return ItemInstance && StackCount > 0 ? 1 : 0;
}

void URpgInventoryManagerComponent::SetCapacityMode(ERpgInventoryCapacityMode NewCapacityMode)
{
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld = World && World->IsGameWorld() && IsRegistered() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (bIsRuntimeGameWorld && !OwningActor->HasAuthority())
	{
		return;
	}

	if (CapacityMode == NewCapacityMode)
	{
		return;
	}

	CapacityMode = NewCapacityMode;
	if (bIsRuntimeGameWorld)
	{
		RefreshCapacityAttributeBinding();
		BroadcastCapacityChanged();
	}
}

void URpgInventoryManagerComponent::SetFixedMaxEntries(int32 NewFixedMaxEntries)
{
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld = World && World->IsGameWorld() && IsRegistered() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (bIsRuntimeGameWorld && !OwningActor->HasAuthority())
	{
		return;
	}

	const int32 ClampedValue = FMath::Max(0, NewFixedMaxEntries);
	if (FixedMaxEntries == ClampedValue)
	{
		return;
	}

	FixedMaxEntries = ClampedValue;
	if (bIsRuntimeGameWorld)
	{
		BroadcastCapacityChanged();
	}
}

void URpgInventoryManagerComponent::SetCapacityAttribute(FGameplayAttribute NewCapacityAttribute)
{
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld = World && World->IsGameWorld() && IsRegistered() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (bIsRuntimeGameWorld && !OwningActor->HasAuthority())
	{
		return;
	}

	if (CapacityAttribute == NewCapacityAttribute)
	{
		return;
	}

	CapacityAttribute = NewCapacityAttribute;
	if (bIsRuntimeGameWorld)
	{
		RefreshCapacityAttributeBinding();
		BroadcastCapacityChanged();
	}
}

bool URpgInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const
{
	if (ItemDef == nullptr || StackCount <= 0)
	{
		return false;
	}

	if (IsCapacityUnlimited())
	{
		return true;
	}

	const int32 RequiredNewEntries = InventoryList.GetRequiredNewEntryCount(ItemDef, StackCount);
	if (RequiredNewEntries <= 0)
	{
		return true;
	}

	if (FindOwningPlayerInventoryLayout())
	{
		return CountAvailableAutoAddSlotsForItemDefinition(ItemDef) >= RequiredNewEntries;
	}

	return RequiredNewEntries <= GetFreeEntryCount();
}

bool URpgInventoryManagerComponent::CanAddItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount) const
{
	if (ItemInstance == nullptr || StackCount <= 0)
	{
		return false;
	}

	if (IsCapacityUnlimited())
	{
		return true;
	}

	const int32 RequiredNewEntries = GetRequiredNewEntryCountForItemInstance(ItemInstance, StackCount);
	if (RequiredNewEntries <= 0)
	{
		return true;
	}

	if (FindOwningPlayerInventoryLayout())
	{
		return CountAvailableAutoAddSlotsForItemInstance(ItemInstance) >= RequiredNewEntries;
	}

	return RequiredNewEntries <= GetFreeEntryCount();
}

bool URpgInventoryManagerComponent::CanAddItemDefinitionToSlot(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, int32 SlotIndex) const
{
	if (!ItemDef || StackCount <= 0 || !IsValidInventorySlotIndex(this, SlotIndex))
	{
		return false;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemDef);
	if (StackCount > MaxStackSize)
	{
		return false;
	}

	URpgInventoryItemInstance* ExistingItem = InventoryList.GetItemInSlot(SlotIndex);
	if (!ExistingItem)
	{
		return IsCapacityUnlimited() || GetFreeEntryCount() > 0;
	}

	return ExistingItem->GetItemDef() == ItemDef && InventoryList.GetFreeStackCapacity(ExistingItem) >= StackCount;
}

bool URpgInventoryManagerComponent::CanAddItemInstanceToSlot(URpgInventoryItemInstance* ItemInstance, int32 StackCount, int32 SlotIndex) const
{
	if (!ItemInstance || StackCount <= 0 || !IsValidInventorySlotIndex(this, SlotIndex))
	{
		return false;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemInstance->GetItemDef());
	if (StackCount > MaxStackSize)
	{
		return false;
	}

	return InventoryList.GetItemInSlot(SlotIndex) == nullptr && (IsCapacityUnlimited() || GetFreeEntryCount() > 0);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::AddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount)
{
	URpgInventoryItemInstance* Result = nullptr;
	if (CanAddItemDefinition(ItemDef, StackCount))
	{
		TArray<URpgInventoryItemInstance*> NewInstances;
		Result = InventoryList.AddEntry(ItemDef, StackCount, NewInstances);
		MarkInventoryStateDirty();
		
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

URpgInventoryItemInstance* URpgInventoryManagerComponent::AddItemDefinitionToSlot(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, int32 SlotIndex)
{
	URpgInventoryItemInstance* Result = nullptr;
	if (CanAddItemDefinitionToSlot(ItemDef, StackCount, SlotIndex))
	{
		TArray<URpgInventoryItemInstance*> NewInstances;
		Result = InventoryList.AddEntryAtSlot(ItemDef, StackCount, SlotIndex, NewInstances);
		MarkInventoryStateDirty();

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
	if (!CanAddItemInstance(ItemInstance, StackCount))
	{
		return;
	}

	InventoryList.AddEntry(ItemInstance, StackCount);
	MarkInventoryStateDirty();
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
	{
		AddReplicatedSubObject(ItemInstance);
	}
}

void URpgInventoryManagerComponent::AddItemInstanceWithStackToSlot(URpgInventoryItemInstance* ItemInstance, int32 StackCount, int32 SlotIndex)
{
	if (!CanAddItemInstanceToSlot(ItemInstance, StackCount, SlotIndex))
	{
		return;
	}

	InventoryList.AddEntryAtSlot(ItemInstance, StackCount, SlotIndex);
	MarkInventoryStateDirty();
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
	{
		AddReplicatedSubObject(ItemInstance);
	}
}

bool URpgInventoryManagerComponent::AddStackToExistingItem(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	const bool bAddedStack = InventoryList.AddStackToEntry(ItemInstance, StackCount);
	if (bAddedStack)
	{
		MarkInventoryStateDirty();
	}
	return bAddedStack;
}

void URpgInventoryManagerComponent::RemoveItemInstance(URpgInventoryItemInstance* ItemInstance)
{
	const bool bHadItem = ContainsItemInstance(ItemInstance);
	InventoryList.RemoveEntry(ItemInstance);
	if (bHadItem)
	{
		MarkInventoryStateDirty();
	}

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

	if (bRemovedStack)
	{
		MarkInventoryStateDirty();
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

URpgInventoryItemInstance* URpgInventoryManagerComponent::GetItemInSlot(int32 SlotIndex) const
{
	return InventoryList.GetItemInSlot(SlotIndex);
}

int32 URpgInventoryManagerComponent::GetItemSlotIndex(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.GetSlotIndex(ItemInstance);
}

int32 URpgInventoryManagerComponent::GetFreeStackCapacity(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.GetFreeStackCapacity(ItemInstance);
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

	const bool bChanged = InventoryList.ApplySort(SortMode);
	if (bChanged)
	{
		MarkInventoryStateDirty();
	}
	return bChanged;
}

bool URpgInventoryManagerComponent::MoveInventoryEntry(FGuid EntryId, int32 TargetIndex)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	const bool bChanged = InventoryList.MoveEntry(EntryId, TargetIndex);
	if (bChanged)
	{
		MarkInventoryStateDirty();
	}
	return bChanged;
}

bool URpgInventoryManagerComponent::MoveInventoryEntryToSlot(FGuid EntryId, int32 TargetSlotIndex)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || !IsValidInventorySlotIndex(this, TargetSlotIndex))
	{
		return false;
	}

	const bool bChanged = InventoryList.MoveEntryToSlot(EntryId, TargetSlotIndex);
	if (bChanged)
	{
		MarkInventoryStateDirty();
	}
	return bChanged;
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
	MarkInventoryStateDirty();

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

void URpgInventoryManagerComponent::OnRep_InventoryRevision()
{
	BroadcastInventoryStateChanged();
}

void URpgInventoryManagerComponent::OnRep_CapacitySettings()
{
	RefreshCapacityAttributeBinding();
	BroadcastCapacityChanged();
}

void URpgInventoryManagerComponent::MarkInventoryStateDirty()
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	++InventoryRevision;
	OwningActor->ForceNetUpdate();
}

void URpgInventoryManagerComponent::BroadcastInventoryStateChanged() const
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	FRpgInventoryChangeMessage Message;
	Message.InventoryOwner = const_cast<URpgInventoryManagerComponent*>(this);
	Message.bOrderChanged = true;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(TAG_Rpg_Inventory_Message_StackChanged, Message);
}

const URpgPlayerInventoryLayoutComponent* URpgInventoryManagerComponent::FindOwningPlayerInventoryLayout() const
{
	const ARpgPlayerState* RpgPlayerState = Cast<ARpgPlayerState>(GetOwner());
	const ARpgPlayerController* RpgPlayerController = RpgPlayerState ? RpgPlayerState->GetRpgPlayerController() : nullptr;
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPlayerController ? RpgPlayerController->GetPlayerInventoryLayoutComponent() : nullptr;
	return InventoryLayout && RpgPlayerState && RpgPlayerState->GetInventoryManagerComponent() == this ? InventoryLayout : nullptr;
}

int32 URpgInventoryManagerComponent::GetNextAutoAddSlotForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout();
	if (!InventoryLayout)
	{
		return InventoryList.GetNextAvailableSlotIndex();
	}

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (!Group.Rule.AllowsItemDefinition(ItemDef))
		{
			continue;
		}

		for (int32 LocalSlotIndex = 0; LocalSlotIndex < Group.SlotCount; ++LocalSlotIndex)
		{
			const int32 GlobalSlotIndex = Group.FirstGlobalSlotIndex + LocalSlotIndex;
			if (!InventoryList.FindEntryBySlotIndex(GlobalSlotIndex))
			{
				return GlobalSlotIndex;
			}
		}
	}

	return INDEX_NONE;
}

int32 URpgInventoryManagerComponent::GetNextAutoAddSlotForItemInstance(URpgInventoryItemInstance* ItemInstance) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout();
	if (!InventoryLayout)
	{
		return InventoryList.GetNextAvailableSlotIndex();
	}

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (!Group.Rule.AllowsItem(ItemInstance))
		{
			continue;
		}

		for (int32 LocalSlotIndex = 0; LocalSlotIndex < Group.SlotCount; ++LocalSlotIndex)
		{
			const int32 GlobalSlotIndex = Group.FirstGlobalSlotIndex + LocalSlotIndex;
			if (!InventoryList.FindEntryBySlotIndex(GlobalSlotIndex))
			{
				return GlobalSlotIndex;
			}
		}
	}

	return INDEX_NONE;
}

int32 URpgInventoryManagerComponent::CountAvailableAutoAddSlotsForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout();
	if (!InventoryLayout)
	{
		return GetFreeEntryCount();
	}

	int32 MatchingFreeSlots = 0;
	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (!Group.Rule.AllowsItemDefinition(ItemDef))
		{
			continue;
		}

		for (int32 LocalSlotIndex = 0; LocalSlotIndex < Group.SlotCount; ++LocalSlotIndex)
		{
			const int32 GlobalSlotIndex = Group.FirstGlobalSlotIndex + LocalSlotIndex;
			if (!InventoryList.FindEntryBySlotIndex(GlobalSlotIndex))
			{
				++MatchingFreeSlots;
			}
		}
	}

	return MatchingFreeSlots;
}

int32 URpgInventoryManagerComponent::CountAvailableAutoAddSlotsForItemInstance(URpgInventoryItemInstance* ItemInstance) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout();
	if (!InventoryLayout)
	{
		return GetFreeEntryCount();
	}

	int32 MatchingFreeSlots = 0;
	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (!Group.Rule.AllowsItem(ItemInstance))
		{
			continue;
		}

		for (int32 LocalSlotIndex = 0; LocalSlotIndex < Group.SlotCount; ++LocalSlotIndex)
		{
			const int32 GlobalSlotIndex = Group.FirstGlobalSlotIndex + LocalSlotIndex;
			if (!InventoryList.FindEntryBySlotIndex(GlobalSlotIndex))
			{
				++MatchingFreeSlots;
			}
		}
	}

	return MatchingFreeSlots;
}

UAbilitySystemComponent* URpgInventoryManagerComponent::FindCapacityAbilitySystem() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(OwnerActor))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	return OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
}

void URpgInventoryManagerComponent::RefreshCapacityAttributeBinding()
{
	ClearCapacityAttributeBinding();

	if (CapacityMode != ERpgInventoryCapacityMode::AbilitySystemAttribute || !CapacityAttribute.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = FindCapacityAbilitySystem())
	{
		BoundCapacityAbilitySystem = ASC;
		CapacityAttributeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(CapacityAttribute)
			.AddUObject(this, &ThisClass::HandleCapacityAttributeChanged);
	}
}

void URpgInventoryManagerComponent::ClearCapacityAttributeBinding()
{
	if (UAbilitySystemComponent* ASC = BoundCapacityAbilitySystem.Get())
	{
		if (CapacityAttributeChangedHandle.IsValid() && CapacityAttribute.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(CapacityAttribute).Remove(CapacityAttributeChangedHandle);
		}
	}

	CapacityAttributeChangedHandle.Reset();
	BoundCapacityAbilitySystem.Reset();
}

void URpgInventoryManagerComponent::HandleCapacityAttributeChanged(const FOnAttributeChangeData& Data)
{
	BroadcastCapacityChanged();
}

void URpgInventoryManagerComponent::BroadcastCapacityChanged() const
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	FRpgInventoryChangeMessage Message;
	Message.InventoryOwner = const_cast<URpgInventoryManagerComponent*>(this);
	Message.bCapacityChanged = true;
	Message.bOrderChanged = true;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(TAG_Rpg_Inventory_Message_StackChanged, Message);
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


