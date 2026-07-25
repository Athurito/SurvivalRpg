// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerMessageTags.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "UObject/UObjectGlobals.h"

namespace RpgInventoryManagerStoragePrivate
{
	const URpgInventoryFragment_ItemTraits* GetStorageItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	int32 GetStorageInventoryMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		return URpgInventoryManagerComponent::
			GetEffectiveMaxStackSizeForDefinition(ItemDef);
	}

	FRpgInventoryGridSize GetStorageInventoryFootprintForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, bool bRotated)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment =
			URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
				ItemDef);
		if (SpatialFragment)
		{
			return SpatialFragment->GetFootprint(bRotated);
		}

		FRpgInventoryGridSize InvalidFootprint;
		InvalidFootprint.Width = 0;
		InvalidFootprint.Height = 0;
		return InvalidFootprint;
	}

	bool CanStorageInventoryRotateDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment =
			URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
				ItemDef);
		return SpatialFragment && SpatialFragment->bAllowRotation;
	}

	FString GetStorageDisplayNameForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? ItemCDO->DisplayName.ToString() : FString();
	}

	ERpgInventoryItemCategory GetStorageCategoryForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		if (const URpgInventoryFragment_ItemTraits* Traits = GetStorageItemTraits(ItemDef))
		{
			return Traits->ItemCategory;
		}

		return ERpgInventoryItemCategory::Misc;
	}

	ERpgInventoryMutationResultCode EvaluateStorageScratchPlacement(
		const FRpgInventoryGridPlacement& Placement,
		const FRpgInventoryGridSize& GridSize,
		const TArray<FRpgInventoryGridPlacement>& Occupancy)
	{
		if (!Placement.IsValid())
		{
			return ERpgInventoryMutationResultCode::InvalidPlacement;
		}

		const FRpgInventoryGridSize OccupiedSize = Placement.GetOccupiedSize();
		if (!GridSize.IsValid() || Placement.X < 0 || Placement.Y < 0 ||
			static_cast<int64>(Placement.X) + OccupiedSize.Width >
				GridSize.Width ||
			static_cast<int64>(Placement.Y) + OccupiedSize.Height >
				GridSize.Height)
		{
			return ERpgInventoryMutationResultCode::OutOfBounds;
		}

		if (Occupancy.ContainsByPredicate(
			[&Placement](const FRpgInventoryGridPlacement& Existing)
			{
				return Existing.Overlaps(Placement);
			}))
		{
			return ERpgInventoryMutationResultCode::Occupied;
		}

		return ERpgInventoryMutationResultCode::Success;
	}

	bool FindStorageFirstFitInScratch(
		const FRpgInventoryContainerHandle& ContainerHandle,
		const FRpgInventoryGridSize& GridSize,
		const FRpgInventoryGridSize& UnrotatedFootprint,
		bool bAllowRotation,
		const TArray<FRpgInventoryGridPlacement>& Occupancy,
		FRpgInventoryGridPlacement& OutPlacement)
	{
		OutPlacement = FRpgInventoryGridPlacement();
		if (!ContainerHandle.IsValid() || !GridSize.IsValid() ||
			!UnrotatedFootprint.IsValid())
		{
			return false;
		}

		const int32 OrientationCount = bAllowRotation ? 2 : 1;
		for (int32 OrientationIndex = 0; OrientationIndex < OrientationCount; ++OrientationIndex)
		{
			FRpgInventoryGridPlacement Candidate;
			Candidate.SetContainerHandle(ContainerHandle);
			Candidate.Width = UnrotatedFootprint.Width;
			Candidate.Height = UnrotatedFootprint.Height;
			Candidate.bRotated = OrientationIndex == 1;
			const FRpgInventoryGridSize OccupiedSize = Candidate.GetOccupiedSize();
			for (int32 Y = 0; Y <= GridSize.Height - OccupiedSize.Height; ++Y)
			{
				for (int32 X = 0; X <= GridSize.Width - OccupiedSize.Width; ++X)
				{
					Candidate.X = X;
					Candidate.Y = Y;
					if (EvaluateStorageScratchPlacement(Candidate, GridSize, Occupancy) ==
						ERpgInventoryMutationResultCode::Success)
					{
						OutPlacement = Candidate;
						return true;
					}
				}
			}
		}

		return false;
	}

}

using RpgInventoryManagerStoragePrivate::
	CanStorageInventoryRotateDefinition;
using RpgInventoryManagerStoragePrivate::
	FindStorageFirstFitInScratch;
using RpgInventoryManagerStoragePrivate::
	GetStorageCategoryForDefinition;
using RpgInventoryManagerStoragePrivate::
	GetStorageDisplayNameForDefinition;
using RpgInventoryManagerStoragePrivate::
	GetStorageInventoryFootprintForDefinition;
using RpgInventoryManagerStoragePrivate::
	GetStorageInventoryMaxStackSizeForDefinition;

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
		*Placement.ContainerHandle.ToString(),
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

bool FRpgInventoryList::AddEntry(URpgInventoryItemInstance* Instance, int32 StackCount)
{
	if (Instance == nullptr || StackCount <= 0 || !CanInsertOwnedInstance(Instance))
	{
		return false;
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
		return false;
	}

	FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = Instance;
	NewEntry.EntryId = FGuid::NewGuid();
	NewEntry.StackCount = StackCount;
	NewEntry.Placement = NewPlacement;
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
	return true;
}

bool FRpgInventoryList::AddEntryAtPlacement(URpgInventoryItemInstance* Instance, int32 StackCount, const FRpgInventoryGridPlacement& Placement)
{
	if (Instance == nullptr || StackCount <= 0 || !CanInsertOwnedInstance(Instance) || !CanPlaceEntryAt(Placement))
	{
		return false;
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
	return true;
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

	const int32 MaxStackSize = GetStorageInventoryMaxStackSizeForDefinition(Entry->Instance->GetItemDef());
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
			View.ItemId = Entry.Instance->GetItemId();
			View.StackCount = Entry.StackCount;
			View.Placement = Entry.Placement;
		}
	}

	Results.Sort([](const FRpgInventoryEntryView& A, const FRpgInventoryEntryView& B)
	{
		if (A.Placement.GetContainerHandle() != B.Placement.GetContainerHandle())
		{
			return A.Placement.GetContainerHandle().ToString() < B.Placement.GetContainerHandle().ToString();
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

bool FRpgInventoryList::CanInsertOwnedInstance(const URpgInventoryItemInstance* Instance) const
{
	const AActor* InventoryOwner = OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
	if (!InventoryOwner || !Instance || Instance->GetOuter() != InventoryOwner ||
		!Instance->GetItemDef() || !Instance->GetItemId().IsValid())
	{
		return false;
	}

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance ||
			(Entry.Instance && Entry.Instance->GetItemId() == Instance->GetItemId()))
		{
			return false;
		}
	}

	return true;
}

URpgInventoryItemInstance* FRpgInventoryList::GetItemAtCell(const FRpgInventoryContainerHandle& ContainerHandle, int32 X, int32 Y) const
{
	if (!ContainerHandle.IsValid())
	{
		return nullptr;
	}

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance && Entry.StackCount > 0 && Entry.Placement.GetContainerHandle() == ContainerHandle && Entry.Placement.ContainsCell(X, Y))
		{
			return Entry.Instance.Get();
		}
	}

	return nullptr;
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

	const int32 MaxStackSize = GetStorageInventoryMaxStackSizeForDefinition(Entry->Instance->GetItemDef());
	return FMath::Max(0, MaxStackSize - Entry->StackCount);
}

bool FRpgInventoryList::ApplySort(
	ERpgInventorySortMode SortMode,
	FRpgInventoryContainerHandle ContainerFilter,
	bool* bOutSucceeded)
{
	if (bOutSucceeded)
	{
		*bOutSucceeded = false;
	}

	TArray<FRpgInventoryEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	for (FRpgInventoryEntry& Entry : Entries)
	{
		if (!ContainerFilter.IsValid() || Entry.Placement.GetContainerHandle() == ContainerFilter)
		{
			SortedEntries.Add(&Entry);
		}
	}

	if (SortedEntries.Num() <= 1)
	{
		if (SortMode == ERpgInventorySortMode::Manual || SortedEntries.Num() == 0)
		{
			if (bOutSucceeded)
			{
				*bOutSucceeded = true;
			}
			return false;
		}

		return SetOrderFromSortedEntryPointers(SortedEntries, bOutSucceeded);
	}

	auto GetEntryDefinition = [](const FRpgInventoryEntry& Entry)
	{
		return Entry.Instance ? Entry.Instance->GetItemDef() : TSubclassOf<URpgInventoryItemDefinition>();
	};

	auto GetEntryName = [&GetEntryDefinition](const FRpgInventoryEntry& Entry)
	{
		return GetStorageDisplayNameForDefinition(GetEntryDefinition(Entry));
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
			const int32 CategoryA = static_cast<int32>(GetStorageCategoryForDefinition(GetEntryDefinition(A)));
			const int32 CategoryB = static_cast<int32>(GetStorageCategoryForDefinition(GetEntryDefinition(B)));
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

	return SetOrderFromSortedEntryPointers(SortedEntries, bOutSucceeded);
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

bool FRpgInventoryList::MoveEntryToPlacement(
	FGuid EntryId,
	const FRpgInventoryGridPlacement& TargetPlacement,
	bool bAllowStackMerge)
{
	URpgInventoryManagerComponent* Inventory =
		Cast<URpgInventoryManagerComponent>(OwnerComponent);
	FRpgInventoryEntry* MovingEntry = FindEntryByEntryId(EntryId);
	if (!Inventory || !MovingEntry || !MovingEntry->Instance ||
		!TargetPlacement.ContainerHandle.IsValid())
	{
		return false;
	}

	FRpgInventoryEntryView View;
	View.InventoryOwner = OwnerComponent;
	View.Instance = MovingEntry->Instance;
	View.EntryId = MovingEntry->EntryId;
	View.ItemId = MovingEntry->Instance->GetItemId();
	View.StackCount = MovingEntry->StackCount;
	View.Placement = MovingEntry->Placement;
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = bAllowStackMerge
		? ERpgInventoryPlacementPurpose::Move
		: ERpgInventoryPlacementPurpose::Equip;
	Query.Search = ERpgInventoryPlacementSearch::Exact;
	Query.Subject = FRpgInventoryPlacementSubject::FromOwnedEntry(
		Inventory,
		View);
	Query.TargetContainer = TargetPlacement.ContainerHandle;
	Query.ExactPlacement = TargetPlacement;
	const FRpgInventoryPlacementPlan Plan = Inventory->EvaluatePlacement(Query);
	if (!Plan.IsSuccess() || Plan.Steps.Num() != 1)
	{
		return false;
	}

	const FRpgInventoryPlacementStep& Step = Plan.Steps[0];
	if (Step.Resolution == ERpgInventoryPlacementResolution::NoOp)
	{
		return true;
	}

	if (Step.Resolution == ERpgInventoryPlacementResolution::Place)
	{
		const int32 DepthDelta =
			static_cast<int32>(Step.Placement.GetContainerHandle().Depth) -
			static_cast<int32>(MovingEntry->Placement.GetContainerHandle().Depth);
		const FRpgInventoryItemId MovingItemId =
			MovingEntry->Instance->GetItemId();
		MovingEntry->Placement = Step.Placement;
		RebaseDescendantContainerDepths(MovingItemId, DepthDelta);
		MarkItemDirty(*MovingEntry);
		BroadcastChangeMessage(
			*MovingEntry,
			MovingEntry->StackCount,
			MovingEntry->StackCount,
			true);
		SortEntriesByPlacement();
		MarkArrayDirty();
		return true;
	}

	if (Step.Resolution == ERpgInventoryPlacementResolution::Merge)
	{
		FRpgInventoryEntry* TargetEntry =
			FindEntryByEntryId(Step.TargetEntryId);
		if (!TargetEntry || !TargetEntry->Instance || Step.Quantity <= 0 ||
			!MovingEntry->Instance->IsStackCompatibleWith(TargetEntry->Instance) ||
			Step.Quantity > GetFreeStackCapacity(TargetEntry->Instance))
		{
			return false;
		}

		const int32 MovingOldCount = MovingEntry->StackCount;
		const int32 TargetOldCount = TargetEntry->StackCount;
		MovingEntry->StackCount -= Step.Quantity;
		TargetEntry->StackCount += Step.Quantity;
		MarkItemDirty(*TargetEntry);
		BroadcastChangeMessage(
			*TargetEntry,
			TargetOldCount,
			TargetEntry->StackCount);
		if (MovingEntry->StackCount <= 0)
		{
			URpgInventoryItemInstance* RemovedInstance =
				MovingEntry->Instance.Get();
			BroadcastChangeMessage(*MovingEntry, MovingOldCount, 0);
			Entries.RemoveAll(
				[EntryId](const FRpgInventoryEntry& Entry)
				{
					return Entry.EntryId == EntryId;
				});
			MarkArrayDirty();
			if (RemovedInstance && Inventory->IsUsingRegisteredSubObjectList())
			{
				Inventory->RemoveReplicatedSubObject(RemovedInstance);
			}
		}
		else
		{
			MarkItemDirty(*MovingEntry);
			BroadcastChangeMessage(
				*MovingEntry,
				MovingOldCount,
				MovingEntry->StackCount);
		}
		return true;
	}

	if (Step.Resolution != ERpgInventoryPlacementResolution::Swap)
	{
		return false;
	}
	FRpgInventoryEntry* TargetEntry =
		FindEntryByEntryId(Step.DisplacedEntryId);
	if (!TargetEntry || !TargetEntry->Instance ||
		TargetEntry->Instance->GetItemId() != Step.DisplacedItemId)
	{
		return false;
	}

	const int32 MovingDepthDelta =
		static_cast<int32>(Step.Placement.GetContainerHandle().Depth) -
		static_cast<int32>(MovingEntry->Placement.GetContainerHandle().Depth);
	const int32 TargetDepthDelta =
		static_cast<int32>(Step.DisplacedPlacement.GetContainerHandle().Depth) -
		static_cast<int32>(TargetEntry->Placement.GetContainerHandle().Depth);
	const FRpgInventoryItemId MovingItemId =
		MovingEntry->Instance->GetItemId();
	const FRpgInventoryItemId TargetItemId =
		TargetEntry->Instance->GetItemId();
	MovingEntry->Placement = Step.Placement;
	TargetEntry->Placement = Step.DisplacedPlacement;
	RebaseDescendantContainerDepths(MovingItemId, MovingDepthDelta);
	RebaseDescendantContainerDepths(TargetItemId, TargetDepthDelta);
	MarkItemDirty(*MovingEntry);
	MarkItemDirty(*TargetEntry);
	BroadcastChangeMessage(
		*MovingEntry,
		MovingEntry->StackCount,
		MovingEntry->StackCount,
		true);
	BroadcastChangeMessage(
		*TargetEntry,
		TargetEntry->StackCount,
		TargetEntry->StackCount,
		true);
	SortEntriesByPlacement();
	MarkArrayDirty();
	return true;
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

FRpgInventoryEntry* FRpgInventoryList::FindEntryByItemId(const FRpgInventoryItemId& ItemId)
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}
	return Entries.FindByPredicate([&ItemId](const FRpgInventoryEntry& Entry)
	{
		return Entry.Instance && Entry.Instance->GetItemId() == ItemId;
	});
}

const FRpgInventoryEntry* FRpgInventoryList::FindEntryByItemId(const FRpgInventoryItemId& ItemId) const
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}

	return Entries.FindByPredicate([&ItemId](const FRpgInventoryEntry& Entry)
	{
		return Entry.Instance && Entry.Instance->GetItemId() == ItemId;
	});
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

void FRpgInventoryList::FindEntriesOverlapping(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry, TArray<const FRpgInventoryEntry*>& OutEntries) const
{
	OutEntries.Reset();
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (&Entry != IgnoredEntry && Entry.Instance != nullptr && Entry.StackCount > 0 && Entry.Placement.Overlaps(Placement))
		{
			OutEntries.Add(&Entry);
		}
	}
}

bool FRpgInventoryList::IsPlacementWithinGrid(const FRpgInventoryGridPlacement& Placement) const
{
	if (!Placement.IsValid())
	{
		return false;
	}

	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	FRpgInventoryGridSize GridSize;
	if (!Inventory || !Inventory->GetGridSizeForContainerHandle(Placement.GetContainerHandle(), GridSize) || !GridSize.IsValid())
	{
		return false;
	}

	const FRpgInventoryGridSize OccupiedSize = Placement.GetOccupiedSize();
	return Placement.X >= 0 &&
		Placement.Y >= 0 &&
		static_cast<int64>(Placement.X) + OccupiedSize.Width <=
			GridSize.Width &&
		static_cast<int64>(Placement.Y) + OccupiedSize.Height <=
			GridSize.Height;
}

bool FRpgInventoryList::CanPlaceEntryAt(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry) const
{
	return IsPlacementWithinGrid(Placement) && FindEntryOverlapping(Placement, IgnoredEntry) == nullptr;
}

bool FRpgInventoryList::CanPlaceEntryAt(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntryA, const FRpgInventoryEntry* IgnoredEntryB) const
{
	if (!IsPlacementWithinGrid(Placement))
	{
		return false;
	}

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (&Entry != IgnoredEntryA &&
			&Entry != IgnoredEntryB &&
			Entry.Instance != nullptr &&
			Entry.StackCount > 0 &&
			Entry.Placement.Overlaps(Placement))
		{
			return false;
		}
	}

	return true;
}

bool FRpgInventoryList::CanEntryUsePlacement(const FRpgInventoryEntry& Entry, const FRpgInventoryGridPlacement& Placement) const
{
	if (!Entry.Instance || !Placement.IsValid())
	{
		return false;
	}

	if (const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent))
	{
		ERpgInventoryMutationResultCode GraphRuleResult = ERpgInventoryMutationResultCode::Success;
		if (!Inventory->ValidatePlacementGraphRules(Entry, Placement, GraphRuleResult))
		{
			return false;
		}

		if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = Inventory->FindOwningPlayerInventoryLayout())
		{
			if (Placement.GetContainerHandle().IsItemOwned())
			{
				return true;
			}

			FRpgInventorySlotAddress Address;
			Address.SetContainerHandle(Placement.GetContainerHandle());
			Address.X = Placement.X;
			Address.Y = Placement.Y;
			return InventoryLayout->CanItemUseSlotAddress(Entry.Instance, Address);
		}
	}

	return true;
}

bool FRpgInventoryList::NormalizePlacementForEntry(const FRpgInventoryEntry& Entry, const FRpgInventoryGridPlacement& TargetPlacement, FRpgInventoryGridPlacement& OutNormalizedPlacement) const
{
	OutNormalizedPlacement = FRpgInventoryGridPlacement();
	if (!Entry.Instance || !TargetPlacement.IsValid())
	{
		return false;
	}

	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Entry.Instance->GetItemDef();
	if (!ItemDefinition)
	{
		return false;
	}

	if (const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent))
	{
		return Inventory->TryNormalizePlacementForDefinition(
			ItemDefinition,
			TargetPlacement.GetContainerHandle(),
			TargetPlacement.X,
			TargetPlacement.Y,
			TargetPlacement.bRotated,
			OutNormalizedPlacement);
	}

	if (TargetPlacement.bRotated && !CanStorageInventoryRotateDefinition(ItemDefinition))
	{
		return false;
	}

	const FRpgInventoryGridSize UnrotatedFootprint = GetStorageInventoryFootprintForDefinition(ItemDefinition, false);
	if (!UnrotatedFootprint.IsValid())
	{
		return false;
	}

	OutNormalizedPlacement = TargetPlacement;
	OutNormalizedPlacement.Width = UnrotatedFootprint.Width;
	OutNormalizedPlacement.Height = UnrotatedFootprint.Height;
	OutNormalizedPlacement.bRotated = TargetPlacement.bRotated;
	return true;
}

bool FRpgInventoryList::TryResolveDisplacedEntryPlacement(
	const FRpgInventoryEntry& MovingEntry,
	const FRpgInventoryGridPlacement& MovingTargetPlacement,
	const FRpgInventoryEntry& DisplacedEntry,
	FRpgInventoryGridPlacement& OutDisplacedPlacement) const
{
	OutDisplacedPlacement = FRpgInventoryGridPlacement();
	if (!MovingEntry.Instance || !DisplacedEntry.Instance || !MovingTargetPlacement.IsValid() ||
		!MovingEntry.Placement.IsValid())
	{
		return false;
	}

	const FRpgInventoryContainerHandle SourceContainer = MovingEntry.Placement.GetContainerHandle();
	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	FRpgInventoryGridSize SourceGridSize;
	if (!Inventory || !Inventory->GetGridSizeForContainerHandle(SourceContainer, SourceGridSize) || !SourceGridSize.IsValid())
	{
		return false;
	}
	if (!IsPlacementWithinGrid(MovingEntry.Placement))
	{
		return false;
	}

	TSet<FIntPoint> TestedOrigins;
	auto TryOrigin = [this, &MovingEntry, &MovingTargetPlacement, &DisplacedEntry, &SourceContainer, &TestedOrigins, &OutDisplacedPlacement](int32 X, int32 Y)
	{
		const FIntPoint Origin(X, Y);
		if (TestedOrigins.Contains(Origin))
		{
			return false;
		}
		TestedOrigins.Add(Origin);

		FRpgInventoryGridPlacement Candidate = DisplacedEntry.Placement;
		Candidate.SetContainerHandle(SourceContainer);
		Candidate.X = X;
		Candidate.Y = Y;

		FRpgInventoryGridPlacement NormalizedCandidate;
		if (!NormalizePlacementForEntry(DisplacedEntry, Candidate, NormalizedCandidate) ||
			!CanEntryUsePlacement(DisplacedEntry, NormalizedCandidate) ||
			!IsPlacementWithinGrid(NormalizedCandidate) ||
			NormalizedCandidate.Overlaps(MovingTargetPlacement) ||
			!CanPlaceEntryAt(NormalizedCandidate, &MovingEntry, &DisplacedEntry))
		{
			return false;
		}

		OutDisplacedPlacement = NormalizedCandidate;
		return true;
	};

	// Preserve the classic swap result when both final footprints fit without touching.
	if (TryOrigin(MovingEntry.Placement.X, MovingEntry.Placement.Y))
	{
		return true;
	}

	// Prefer cells released by the moving item so a small displaced item stays visually near the drop.
	const FRpgInventoryGridSize ReleasedFootprint = MovingEntry.Placement.GetOccupiedSize();
	for (int32 Y = MovingEntry.Placement.Y; Y < MovingEntry.Placement.Y + ReleasedFootprint.Height; ++Y)
	{
		for (int32 X = MovingEntry.Placement.X; X < MovingEntry.Placement.X + ReleasedFootprint.Width; ++X)
		{
			if (TryOrigin(X, Y))
			{
				return true;
			}
		}
	}

	// If the released footprint is too small, fall back to a deterministic first fit in the source container.
	for (int32 Y = 0; Y < SourceGridSize.Height; ++Y)
	{
		for (int32 X = 0; X < SourceGridSize.Width; ++X)
		{
			if (TryOrigin(X, Y))
			{
				return true;
			}
		}
	}

	return false;
}

bool FRpgInventoryList::FindFirstFitPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, FRpgInventoryGridPlacement& OutPlacement) const
{
	const TArray<FRpgInventoryGridPlacement> EmptyAdditionalOccupancy;
	return FindFirstFitPlacement(ItemDef, OutPlacement, EmptyAdditionalOccupancy);
}

bool FRpgInventoryList::FindFirstFitPlacement(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef,
	FRpgInventoryGridPlacement& OutPlacement,
	const TArray<FRpgInventoryGridPlacement>& AdditionalOccupancy) const
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

	TArray<FRpgInventoryGridPlacement> ScratchOccupancy = AdditionalOccupancy;
	ScratchOccupancy.Reserve(ScratchOccupancy.Num() + Entries.Num());
	for (const FRpgInventoryEntry& Existing : Entries)
	{
		if (Existing.Instance && Existing.StackCount > 0)
		{
			ScratchOccupancy.Add(Existing.Placement);
		}
	}

	auto TryFindInGrid =
		[ItemDef, &ScratchOccupancy](
			const FRpgInventoryContainerHandle& ContainerHandle,
			const FRpgInventoryGridSize& GridSize,
			FRpgInventoryGridPlacement& CandidatePlacement)
	{
		return FindStorageFirstFitInScratch(
			ContainerHandle,
			GridSize,
			GetStorageInventoryFootprintForDefinition(ItemDef, false),
			CanStorageInventoryRotateDefinition(ItemDef),
			ScratchOccupancy,
			CandidatePlacement);
	};

	if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = Inventory->FindOwningPlayerInventoryLayout())
	{
		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Content || !Group.Rule.AllowsItemDefinition(ItemDef))
			{
				continue;
			}

			if (TryFindInGrid(Group.ContainerHandle, Group.GridSize, OutPlacement))
			{
				return true;
			}
		}

		return false;
	}

	const FRpgInventoryContainerHandle DefaultHandle =
		FRpgInventoryContainerHandle::MakeRoot(Inventory->DefaultContainerId);
	FRpgInventoryGridSize DefaultGridSize;
	if (!Inventory->GetGridSizeForContainerHandle(DefaultHandle, DefaultGridSize))
	{
		return false;
	}

	return TryFindInGrid(DefaultHandle, DefaultGridSize, OutPlacement);
}

bool FRpgInventoryList::FindFirstFitPlacementInContainer(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef,
	const FRpgInventoryContainerHandle& ContainerHandle,
	const TArray<FRpgInventoryGridPlacement>& ScratchOccupancy,
	FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	FRpgInventoryGridSize GridSize;
	if (!ItemDef || !Inventory || !Inventory->GetGridSizeForContainerHandle(ContainerHandle, GridSize) || !GridSize.IsValid())
	{
		return false;
	}

	if (const URpgPlayerInventoryLayoutComponent* Layout = Inventory->FindOwningPlayerInventoryLayout())
	{
		const TArray<FRpgInventorySlotGroupView> SlotGroups = Layout->GetSlotGroups();
		const FRpgInventorySlotGroupView* Group = SlotGroups.FindByPredicate(
			[&ContainerHandle](const FRpgInventorySlotGroupView& Candidate)
			{
				return Candidate.ContainerHandle == ContainerHandle;
			});
		if (ContainerHandle.IsRoot() && (!Group || Group->GroupKind != ERpgInventorySlotGroupKind::Content || !Group->Rule.AllowsItemDefinition(ItemDef)))
		{
			return false;
		}
	}

	if (ContainerHandle.IsItemOwned())
	{
		FRpgInventoryItemContainerDefinition Definition;
		if (!Inventory->GetItemContainerDefinition(ContainerHandle, Definition) || !Definition.AllowsItemDefinition(ItemDef, ContainerHandle.Depth))
		{
			return false;
		}
	}

	FRpgInventoryGridSize Footprint =
		GetStorageInventoryFootprintForDefinition(ItemDef, false);
	if (!Footprint.IsValid())
	{
		return false;
	}
	bool bAllowRotation = CanStorageInventoryRotateDefinition(ItemDef);
	if (Inventory->ShouldUseSingleCellPlacementForContainer(ContainerHandle))
	{
		Footprint.Width = 1;
		Footprint.Height = 1;
		bAllowRotation = false;
	}
	return FindStorageFirstFitInScratch(
		ContainerHandle,
		GridSize,
		Footprint,
		bAllowRotation,
		ScratchOccupancy,
		OutPlacement);
}

bool FRpgInventoryList::FindFirstFitPlacement(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement& OutPlacement) const
{
	return ItemInstance ? FindFirstFitPlacement(ItemInstance->GetItemDef(), OutPlacement) : false;
}

int32 FRpgInventoryList::GetLinearOrder(const FRpgInventoryGridPlacement& Placement) const
{
	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	FRpgInventoryGridSize GridSize;
	const int32 GridWidth = Inventory && Inventory->GetGridSizeForContainerHandle(Placement.GetContainerHandle(), GridSize) && GridSize.Width > 0 ? GridSize.Width : 1000;
	return Placement.Y * GridWidth + Placement.X;
}

void FRpgInventoryList::SortEntriesByPlacement()
{
	Entries.Sort([this](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
	{
		if (A.Placement.GetContainerHandle() != B.Placement.GetContainerHandle())
		{
			return A.Placement.GetContainerHandle().ToString() < B.Placement.GetContainerHandle().ToString();
		}

		return GetLinearOrder(A.Placement) < GetLinearOrder(B.Placement);
	});
}

void FRpgInventoryList::RebaseDescendantContainerDepths(const FRpgInventoryItemId& AncestorItemId, int32 DepthDelta)
{
	if (!AncestorItemId.IsValid() || DepthDelta == 0)
	{
		return;
	}

	for (FRpgInventoryEntry& Candidate : Entries)
	{
		if (!Candidate.Instance || Candidate.Instance->GetItemId() == AncestorItemId)
		{
			continue;
		}

		FRpgInventoryContainerHandle CandidateHandle = Candidate.Placement.GetContainerHandle();
		FRpgInventoryItemId ParentItemId = CandidateHandle.IsItemOwned() ? CandidateHandle.ItemOwnerId : FRpgInventoryItemId();
		bool bIsDescendant = false;
		for (int32 Guard = 0; Guard <= Entries.Num() && ParentItemId.IsValid(); ++Guard)
		{
			if (ParentItemId == AncestorItemId)
			{
				bIsDescendant = true;
				break;
			}

			const FRpgInventoryEntry* ParentEntry = FindEntryByItemId(ParentItemId);
			const FRpgInventoryContainerHandle ParentHandle = ParentEntry
				? ParentEntry->Placement.GetContainerHandle()
				: FRpgInventoryContainerHandle();
			ParentItemId = ParentHandle.IsItemOwned() ? ParentHandle.ItemOwnerId : FRpgInventoryItemId();
		}

		if (!bIsDescendant)
		{
			continue;
		}

		const int32 RebasingDepth = static_cast<int32>(CandidateHandle.Depth) + DepthDelta;
		if (!ensureMsgf(RebasingDepth > 0 && RebasingDepth <= RpgInventoryMaxItemOwnedDepth,
			TEXT("Validated inventory subtree depth became invalid while rebasing %s"), *AncestorItemId.ToString()))
		{
			continue;
		}

		CandidateHandle.Depth = static_cast<uint8>(RebasingDepth);
		Candidate.Placement.SetContainerHandle(CandidateHandle);
		MarkItemDirty(Candidate);
		BroadcastChangeMessage(Candidate, Candidate.StackCount, Candidate.StackCount, true);
	}
}

bool FRpgInventoryList::SetOrderFromSortedEntryPointers(
	const TArray<FRpgInventoryEntry*>& SortedEntries,
	bool* bOutSucceeded)
{
	if (bOutSucceeded)
	{
		*bOutSucceeded = false;
	}

	const URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(OwnerComponent);
	if (!Inventory)
	{
		return false;
	}

	TMap<FRpgInventoryContainerHandle, TArray<FRpgInventoryGridPlacement>> ScratchOccupancyByContainer;
	TMap<FRpgInventoryEntry*, FRpgInventoryGridPlacement> PlannedPlacements;
	PlannedPlacements.Reserve(SortedEntries.Num());

	for (FRpgInventoryEntry* Entry : SortedEntries)
	{
		if (!Entry || !Entry->Instance)
		{
			return false;
		}

		FRpgInventoryGridPlacement NewPlacement = Entry->Placement;
		const FRpgInventoryContainerHandle EntryContainer = Entry->Placement.GetContainerHandle();
		TArray<FRpgInventoryGridPlacement>& ContainerScratch = ScratchOccupancyByContainer.FindOrAdd(EntryContainer);
		const bool bSingleCellContainer =
			Inventory->ShouldUseSingleCellPlacementForContainer(
				EntryContainer);
		if ((bSingleCellContainer &&
			 !GetStorageInventoryFootprintForDefinition(
				 Entry->Instance->GetItemDef(),
				 false).IsValid()) ||
			(!bSingleCellContainer &&
			 !FindFirstFitPlacementInContainer(
				 Entry->Instance->GetItemDef(),
				 EntryContainer,
				 ContainerScratch,
				 NewPlacement)))
		{
			// Sort is a single mutation. Never leave a fragmented container half repacked.
			return false;
		}

		ContainerScratch.Add(NewPlacement);
		PlannedPlacements.Add(Entry, NewPlacement);
	}

	bool bChanged = false;
	for (const TPair<FRpgInventoryEntry*, FRpgInventoryGridPlacement>& PlannedPair : PlannedPlacements)
	{
		FRpgInventoryEntry* Entry = PlannedPair.Key;
		const FRpgInventoryGridPlacement& NewPlacement = PlannedPair.Value;
		if (Entry->Placement.GetContainerHandle() != NewPlacement.GetContainerHandle() ||
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

	if (bOutSucceeded)
	{
		*bOutSucceeded = true;
	}
	return bChanged;
}
