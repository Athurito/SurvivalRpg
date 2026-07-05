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

	FRpgInventoryGridSize GetInventoryManagerFootprintForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, bool bRotated)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		const URpgInventoryFragment_SpatialItem* SpatialFragment = ItemCDO
			? Cast<URpgInventoryFragment_SpatialItem>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_SpatialItem::StaticClass()))
			: nullptr;
		return SpatialFragment ? SpatialFragment->GetFootprint(bRotated) : FRpgInventoryGridSize();
	}

	bool CanInventoryManagerRotateDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		const URpgInventoryFragment_SpatialItem* SpatialFragment = ItemCDO
			? Cast<URpgInventoryFragment_SpatialItem>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_SpatialItem::StaticClass()))
			: nullptr;
		return SpatialFragment ? SpatialFragment->bAllowRotation : true;
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

	return FString::Printf(
		TEXT("%s [%s] (%d x %s @ %s[%d,%d %dx%d%s])"),
		*EntryId.ToString(),
		*GetNameSafe(Instance),
		StackCount,
		*GetNameSafe(ItemDef),
		*Placement.ContainerId.ToString(),
		Placement.X,
		Placement.Y,
		Placement.Width,
		Placement.Height,
		Placement.bRotated ? TEXT(" R") : TEXT(""));
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
	Message.Placement = Entry.Placement;
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
		FRpgInventoryGridPlacement NewPlacement;
		if (!FindFirstFitPlacement(ItemDef, NewPlacement))
		{
			UE_LOG(LogRpgInventoryManager, Warning, TEXT("AddEntry failed: no free spatial placement. Inventory=%s ItemDef=%s RemainingCount=%d UsedEntries=%d"),
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
		NewEntry.Placement = NewPlacement;
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

URpgInventoryItemInstance* FRpgInventoryList::AddEntryAtPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, const FRpgInventoryGridPlacement& Placement, TArray<URpgInventoryItemInstance*>& OutNewInstances)
{
	check(ItemDef != nullptr);
	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor && OwningActor->HasAuthority());

	if (StackCount <= 0 || !IsPlacementWithinGrid(Placement))
	{
		return nullptr;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemDef);
	if (StackCount > MaxStackSize)
	{
		return nullptr;
	}

	if (FRpgInventoryEntry* ExistingEntry = FindEntryOverlapping(Placement))
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
	NewEntry.Placement = Placement;
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

	FRpgInventoryGridPlacement NewPlacement;
	if (!FindFirstFitPlacement(Instance, NewPlacement))
	{
		UE_LOG(LogRpgInventoryManager, Warning, TEXT("AddEntry instance failed: no free spatial placement. Inventory=%s Item=%s StackCount=%d UsedEntries=%d"),
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
	NewEntry.Placement = NewPlacement;
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
}

void FRpgInventoryList::AddEntryAtPlacement(URpgInventoryItemInstance* Instance, int32 StackCount, const FRpgInventoryGridPlacement& Placement)
{
	if (Instance == nullptr || StackCount <= 0 || !CanPlaceEntryAt(Placement))
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
	NewEntry.Placement = Placement;
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
			View.Placement = Entry.Placement;
		}
	}

	Results.Sort([](const FRpgInventoryEntryView& A, const FRpgInventoryEntryView& B)
	{
		if (A.Placement.ContainerId != B.Placement.ContainerId)
		{
			return A.Placement.ContainerId.LexicalLess(B.Placement.ContainerId);
		}

		if (A.Placement.Y != B.Placement.Y)
		{
			return A.Placement.Y < B.Placement.Y;
		}

		return A.Placement.X < B.Placement.X;
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

URpgInventoryItemInstance* FRpgInventoryList::GetItemAtCell(FName ContainerId, int32 X, int32 Y) const
{
	const FRpgInventoryEntry* Entry = FindEntryAtCell(ContainerId, X, Y);
	return Entry ? Entry->Instance.Get() : nullptr;
}

bool FRpgInventoryList::GetPlacementForItem(URpgInventoryItemInstance* Instance, FRpgInventoryGridPlacement& OutPlacement) const
{
	const FRpgInventoryEntry* Entry = FindEntryByInstance(Instance);
	if (!Entry)
	{
		OutPlacement = FRpgInventoryGridPlacement();
		return false;
	}

	OutPlacement = Entry->Placement;
	return true;
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
		SortedEntries.Sort([this](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			return GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
		});
		break;

	case ERpgInventorySortMode::Name:
		SortedEntries.Sort([this, &GetEntryName](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			const int32 NameCompare = GetEntryName(A).Compare(GetEntryName(B), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
		});
		break;

	case ERpgInventorySortMode::Category:
		SortedEntries.Sort([this, &GetEntryDefinition, &GetEntryName](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			const int32 CategoryA = static_cast<int32>(GetCategoryForDefinition(GetEntryDefinition(A)));
			const int32 CategoryB = static_cast<int32>(GetCategoryForDefinition(GetEntryDefinition(B)));
			if (CategoryA != CategoryB)
			{
				return CategoryA < CategoryB;
			}

			const int32 NameCompare = GetEntryName(A).Compare(GetEntryName(B), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
		});
		break;

	case ERpgInventorySortMode::StackCount:
		SortedEntries.Sort([this, &GetEntryName](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			if (A.StackCount != B.StackCount)
			{
				return A.StackCount > B.StackCount;
			}

			const int32 NameCompare = GetEntryName(A).Compare(GetEntryName(B), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
		});
		break;

	case ERpgInventorySortMode::Recent:
		SortedEntries.Sort([this](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			return GetLinearOrder(A.Placement) > GetLinearOrder(B.Placement);
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

	SortedEntries.Sort([this](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		return GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
	});

	SortedEntries.Remove(MovingEntry);
	const int32 ClampedTargetIndex = FMath::Clamp(TargetIndex, 0, SortedEntries.Num());
	SortedEntries.Insert(MovingEntry, ClampedTargetIndex);
	return SetOrderFromSortedEntryPointers(SortedEntries);
}

bool FRpgInventoryList::MoveEntryToPlacement(FGuid EntryId, const FRpgInventoryGridPlacement& TargetPlacement)
{
	if (!EntryId.IsValid() || !TargetPlacement.IsValid())
	{
		return false;
	}

	FRpgInventoryEntry* MovingEntry = FindEntryByEntryId(EntryId);
	if (!MovingEntry || !MovingEntry->Instance)
	{
		return false;
	}

	const TSubclassOf<URpgInventoryItemDefinition> MovingDefinition = MovingEntry->Instance->GetItemDef();
	FRpgInventoryGridPlacement NormalizedTargetPlacement = TargetPlacement;
	const FRpgInventoryGridSize MovingFootprint = GetInventoryManagerFootprintForDefinition(MovingDefinition, false);
	NormalizedTargetPlacement.Width = MovingFootprint.Width;
	NormalizedTargetPlacement.Height = MovingFootprint.Height;
	NormalizedTargetPlacement.bRotated = TargetPlacement.bRotated && CanInventoryManagerRotateDefinition(MovingDefinition);

	if (const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent))
	{
		if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = Inventory->FindOwningPlayerInventoryLayout())
		{
			FRpgInventorySlotAddress TargetAddress;
			TargetAddress.ContainerId = NormalizedTargetPlacement.ContainerId;
			TargetAddress.X = NormalizedTargetPlacement.X;
			TargetAddress.Y = NormalizedTargetPlacement.Y;
			if (!InventoryLayout->CanItemUseSlotAddress(MovingEntry->Instance, TargetAddress))
			{
				return false;
			}
		}
	}

	if (!IsPlacementWithinGrid(NormalizedTargetPlacement))
	{
		return false;
	}

	FRpgInventoryEntry* TargetEntry = FindEntryOverlapping(NormalizedTargetPlacement, MovingEntry);
	if (TargetEntry == MovingEntry)
	{
		return true;
	}

	if (!TargetEntry)
	{
		if (!CanPlaceEntryAt(NormalizedTargetPlacement, MovingEntry))
		{
			return false;
		}

		MovingEntry->Placement = NormalizedTargetPlacement;
		MarkItemDirty(*MovingEntry);
		BroadcastChangeMessage(*MovingEntry, MovingEntry->StackCount, MovingEntry->StackCount, true);
		SortEntriesByPlacement();
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

	FRpgInventoryGridPlacement TargetSwapPlacement = TargetEntry->Placement;
	TargetSwapPlacement.ContainerId = MovingEntry->Placement.ContainerId;
	TargetSwapPlacement.X = MovingEntry->Placement.X;
	TargetSwapPlacement.Y = MovingEntry->Placement.Y;

	if (!IsPlacementWithinGrid(TargetSwapPlacement) ||
		!CanPlaceEntryAt(TargetSwapPlacement, TargetEntry) ||
		!CanPlaceEntryAt(NormalizedTargetPlacement, MovingEntry))
	{
		return false;
	}

	MovingEntry->Placement = NormalizedTargetPlacement;
	TargetEntry->Placement = TargetSwapPlacement;
	MarkItemDirty(*MovingEntry);
	MarkItemDirty(*TargetEntry);
	BroadcastChangeMessage(*MovingEntry, MovingEntry->StackCount, MovingEntry->StackCount, true);
	BroadcastChangeMessage(*TargetEntry, TargetEntry->StackCount, TargetEntry->StackCount, true);
	SortEntriesByPlacement();
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

	SortedEntries.Sort([this](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		return GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
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
		SnapshotEntry.Placement = Entry->Placement;
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
		NewEntry.Placement = SnapshotEntry.Placement;
		NewEntry.LastObservedCount = INDEX_NONE;
		MarkItemDirty(NewEntry);
		BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount, true);
	}

	SortEntriesByPlacement();
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

FRpgInventoryEntry* FRpgInventoryList::FindEntryAtCell(FName ContainerId, int32 X, int32 Y)
{
	for (FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr && Entry.StackCount > 0 && Entry.Placement.ContainsCell(X, Y) && Entry.Placement.ContainerId == ContainerId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FRpgInventoryEntry* FRpgInventoryList::FindEntryAtCell(FName ContainerId, int32 X, int32 Y) const
{
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr && Entry.StackCount > 0 && Entry.Placement.ContainsCell(X, Y) && Entry.Placement.ContainerId == ContainerId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FRpgInventoryEntry* FRpgInventoryList::FindEntryOverlapping(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry)
{
	for (FRpgInventoryEntry& Entry : Entries)
	{
		if (&Entry != IgnoredEntry && Entry.Instance != nullptr && Entry.StackCount > 0 && Entry.Placement.Overlaps(Placement))
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FRpgInventoryEntry* FRpgInventoryList::FindEntryOverlapping(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry) const
{
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (&Entry != IgnoredEntry && Entry.Instance != nullptr && Entry.StackCount > 0 && Entry.Placement.Overlaps(Placement))
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool FRpgInventoryList::IsPlacementWithinGrid(const FRpgInventoryGridPlacement& Placement) const
{
	if (!Placement.IsValid())
	{
		return false;
	}

	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	FRpgInventoryGridSize GridSize;
	if (!Inventory || !Inventory->GetGridSizeForContainer(Placement.ContainerId, GridSize) || !GridSize.IsValid())
	{
		return false;
	}

	const FRpgInventoryGridSize OccupiedSize = Placement.GetOccupiedSize();
	return Placement.X >= 0 &&
		Placement.Y >= 0 &&
		Placement.X + OccupiedSize.Width <= GridSize.Width &&
		Placement.Y + OccupiedSize.Height <= GridSize.Height;
}

bool FRpgInventoryList::CanPlaceEntryAt(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry) const
{
	return IsPlacementWithinGrid(Placement) && FindEntryOverlapping(Placement, IgnoredEntry) == nullptr;
}

bool FRpgInventoryList::FindFirstFitPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!ItemDef)
	{
		return false;
	}

	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	if (!Inventory)
	{
		return false;
	}

	auto TryFindInGrid = [this, ItemDef](FName ContainerId, const FRpgInventoryGridSize& GridSize, bool bRotated, FRpgInventoryGridPlacement& CandidatePlacement)
	{
		if (!GridSize.IsValid())
		{
			return false;
		}

		const FRpgInventoryGridSize Footprint = GetInventoryManagerFootprintForDefinition(ItemDef, bRotated);
		if (!Footprint.IsValid())
		{
			return false;
		}

		CandidatePlacement.ContainerId = ContainerId;
		CandidatePlacement.Width = GetInventoryManagerFootprintForDefinition(ItemDef, false).Width;
		CandidatePlacement.Height = GetInventoryManagerFootprintForDefinition(ItemDef, false).Height;
		CandidatePlacement.bRotated = bRotated && CanInventoryManagerRotateDefinition(ItemDef);

		const FRpgInventoryGridSize OccupiedSize = CandidatePlacement.GetOccupiedSize();
		for (int32 Y = 0; Y <= GridSize.Height - OccupiedSize.Height; ++Y)
		{
			for (int32 X = 0; X <= GridSize.Width - OccupiedSize.Width; ++X)
			{
				CandidatePlacement.X = X;
				CandidatePlacement.Y = Y;
				if (CanPlaceEntryAt(CandidatePlacement))
				{
					return true;
				}
			}
		}

		return false;
	};

	if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = Inventory->FindOwningPlayerInventoryLayout())
	{
		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Content || !Group.Rule.AllowsItemDefinition(ItemDef))
			{
				continue;
			}

			if (TryFindInGrid(Group.ContainerId, Group.GridSize, false, OutPlacement))
			{
				return true;
			}

			if (CanInventoryManagerRotateDefinition(ItemDef) && TryFindInGrid(Group.ContainerId, Group.GridSize, true, OutPlacement))
			{
				return true;
			}
		}

		return false;
	}

	FRpgInventoryGridSize DefaultGridSize;
	if (!Inventory->GetGridSizeForContainer(Inventory->DefaultContainerId, DefaultGridSize))
	{
		return false;
	}

	if (TryFindInGrid(Inventory->DefaultContainerId, DefaultGridSize, false, OutPlacement))
	{
		return true;
	}

	return CanInventoryManagerRotateDefinition(ItemDef) && TryFindInGrid(Inventory->DefaultContainerId, DefaultGridSize, true, OutPlacement);
}

bool FRpgInventoryList::FindFirstFitPlacement(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement& OutPlacement) const
{
	return ItemInstance ? FindFirstFitPlacement(ItemInstance->GetItemDef(), OutPlacement) : false;
}

int32 FRpgInventoryList::GetLinearOrder(const FRpgInventoryGridPlacement& Placement) const
{
	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	FRpgInventoryGridSize GridSize;
	const int32 GridWidth = Inventory && Inventory->GetGridSizeForContainer(Placement.ContainerId, GridSize) && GridSize.Width > 0 ? GridSize.Width : 1000;
	return Placement.Y * GridWidth + Placement.X;
}

void FRpgInventoryList::SortEntriesByPlacement()
{
	Entries.Sort([this](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		if (A.Placement.ContainerId != B.Placement.ContainerId)
		{
			return A.Placement.ContainerId.LexicalLess(B.Placement.ContainerId);
		}

		return GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
	});
}

bool FRpgInventoryList::SetOrderFromSortedEntryPointers(const TArray<FRpgInventoryEntry*>& SortedEntries)
{
	bool bChanged = false;
	for (FRpgInventoryEntry* Entry : SortedEntries)
	{
		if (!Entry || !Entry->Instance)
		{
			continue;
		}

		FRpgInventoryGridPlacement NewPlacement;
		if (!FindFirstFitPlacement(Entry->Instance, NewPlacement))
		{
			continue;
		}

		if (Entry->Placement.ContainerId != NewPlacement.ContainerId ||
			Entry->Placement.X != NewPlacement.X ||
			Entry->Placement.Y != NewPlacement.Y ||
			Entry->Placement.bRotated != NewPlacement.bRotated)
		{
			Entry->Placement = NewPlacement;
			MarkItemDirty(*Entry);
			BroadcastChangeMessage(*Entry, Entry->StackCount, Entry->StackCount, true);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		SortEntriesByPlacement();
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
	DefaultGridSize.Width = 10;
	DefaultGridSize.Height = 6;
}

void URpgInventoryManagerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CapacityMode);
	DOREPLIFETIME(ThisClass, FixedMaxEntries);
	DOREPLIFETIME(ThisClass, DefaultGridSize);
	DOREPLIFETIME(ThisClass, DefaultContainerId);
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

	const int32 RequiredNewEntries = InventoryList.GetRequiredNewEntryCount(ItemDef, StackCount);
	if (RequiredNewEntries <= 0)
	{
		return true;
	}

	if (!IsCapacityUnlimited() && RequiredNewEntries > GetFreeEntryCount())
	{
		return false;
	}

	FRpgInventoryGridPlacement Placement;
	return InventoryList.FindFirstFitPlacement(ItemDef, Placement);
}

bool URpgInventoryManagerComponent::CanAddItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount) const
{
	if (ItemInstance == nullptr || StackCount <= 0)
	{
		return false;
	}

	const int32 RequiredNewEntries = GetRequiredNewEntryCountForItemInstance(ItemInstance, StackCount);
	if (RequiredNewEntries <= 0)
	{
		return true;
	}

	if (!IsCapacityUnlimited() && RequiredNewEntries > GetFreeEntryCount())
	{
		return false;
	}

	FRpgInventoryGridPlacement Placement;
	return InventoryList.FindFirstFitPlacement(ItemInstance, Placement);
}

bool URpgInventoryManagerComponent::CanAddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement) const
{
	if (!ItemDef || StackCount <= 0)
	{
		return false;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemDef);
	if (StackCount > MaxStackSize)
	{
		return false;
	}

	FRpgInventoryGridPlacement NormalizedPlacement = MakePlacementForItemDefinition(ItemDef, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated);
	if (!InventoryList.IsPlacementWithinGrid(NormalizedPlacement))
	{
		return false;
	}

	if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout())
	{
		bool bAllowedByGroup = false;
		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.ContainerId == NormalizedPlacement.ContainerId &&
				Group.GroupKind == ERpgInventorySlotGroupKind::Content &&
				Group.Rule.AllowsItemDefinition(ItemDef) &&
				Group.ContainsCell(NormalizedPlacement.X, NormalizedPlacement.Y))
			{
				bAllowedByGroup = true;
				break;
			}
		}

		if (!bAllowedByGroup)
		{
			return false;
		}
	}

	URpgInventoryItemInstance* ExistingItem = InventoryList.GetItemAtCell(NormalizedPlacement.ContainerId, NormalizedPlacement.X, NormalizedPlacement.Y);
	if (!ExistingItem)
	{
		return (IsCapacityUnlimited() || GetFreeEntryCount() > 0) && InventoryList.CanPlaceEntryAt(NormalizedPlacement);
	}

	return ExistingItem->GetItemDef() == ItemDef && InventoryList.GetFreeStackCapacity(ExistingItem) >= StackCount;
}

bool URpgInventoryManagerComponent::CanAddItemInstanceToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement) const
{
	if (!ItemInstance || StackCount <= 0)
	{
		return false;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemInstance->GetItemDef());
	if (StackCount > MaxStackSize)
	{
		return false;
	}

	FRpgInventoryGridPlacement NormalizedPlacement = MakePlacementForItemInstance(ItemInstance, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated);
	if (!InventoryList.IsPlacementWithinGrid(NormalizedPlacement))
	{
		return false;
	}

	if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout())
	{
		FRpgInventorySlotAddress Address;
		Address.ContainerId = NormalizedPlacement.ContainerId;
		Address.X = NormalizedPlacement.X;
		Address.Y = NormalizedPlacement.Y;
		if (!InventoryLayout->CanItemUseSlotAddress(ItemInstance, Address))
		{
			return false;
		}
	}

	return (IsCapacityUnlimited() || GetFreeEntryCount() > 0) && InventoryList.CanPlaceEntryAt(NormalizedPlacement);
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

URpgInventoryItemInstance* URpgInventoryManagerComponent::AddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement)
{
	URpgInventoryItemInstance* Result = nullptr;
	if (CanAddItemDefinitionToPlacement(ItemDef, StackCount, Placement))
	{
		const FRpgInventoryGridPlacement NormalizedPlacement = MakePlacementForItemDefinition(ItemDef, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated);
		TArray<URpgInventoryItemInstance*> NewInstances;
		Result = InventoryList.AddEntryAtPlacement(ItemDef, StackCount, NormalizedPlacement, NewInstances);
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

void URpgInventoryManagerComponent::AddItemInstanceWithStackToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement)
{
	if (!CanAddItemInstanceToPlacement(ItemInstance, StackCount, Placement))
	{
		return;
	}

	const FRpgInventoryGridPlacement NormalizedPlacement = MakePlacementForItemInstance(ItemInstance, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated);
	InventoryList.AddEntryAtPlacement(ItemInstance, StackCount, NormalizedPlacement);
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

URpgInventoryItemInstance* URpgInventoryManagerComponent::GetItemAtCell(FName ContainerId, int32 X, int32 Y) const
{
	return InventoryList.GetItemAtCell(ContainerId, X, Y);
}

bool URpgInventoryManagerComponent::GetItemPlacement(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement& OutPlacement) const
{
	return InventoryList.GetPlacementForItem(ItemInstance, OutPlacement);
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

bool URpgInventoryManagerComponent::MoveInventoryEntryToPlacement(FGuid EntryId, FRpgInventoryGridPlacement TargetPlacement)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	const bool bChanged = InventoryList.MoveEntryToPlacement(EntryId, TargetPlacement);
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

bool URpgInventoryManagerComponent::GetGridSizeForContainer(FName ContainerId, FRpgInventoryGridSize& OutGridSize) const
{
	OutGridSize = FRpgInventoryGridSize();
	if (ContainerId.IsNone())
	{
		return false;
	}

	if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout())
	{
		return InventoryLayout->GetGridSizeForContainer(ContainerId, OutGridSize);
	}

	if (ContainerId == DefaultContainerId)
	{
		OutGridSize = DefaultGridSize;
		return OutGridSize.IsValid();
	}

	return false;
}

FRpgInventoryGridPlacement URpgInventoryManagerComponent::MakePlacementForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, FName ContainerId, int32 X, int32 Y, bool bRotated) const
{
	FRpgInventoryGridPlacement Placement;
	Placement.ContainerId = ContainerId.IsNone() ? DefaultContainerId : ContainerId;
	Placement.X = X;
	Placement.Y = Y;
	const FRpgInventoryGridSize Footprint = GetInventoryManagerFootprintForDefinition(ItemDef, false);
	Placement.Width = Footprint.Width;
	Placement.Height = Footprint.Height;
	Placement.bRotated = bRotated && CanInventoryManagerRotateDefinition(ItemDef);
	return Placement;
}

FRpgInventoryGridPlacement URpgInventoryManagerComponent::MakePlacementForItemInstance(URpgInventoryItemInstance* ItemInstance, FName ContainerId, int32 X, int32 Y, bool bRotated) const
{
	return MakePlacementForItemDefinition(ItemInstance ? ItemInstance->GetItemDef() : nullptr, ContainerId, X, Y, bRotated);
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


