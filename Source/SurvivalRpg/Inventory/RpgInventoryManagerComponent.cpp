// Copyright Epic Games, Inc. All Rights Reserved.
#include "RpgInventoryManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Templates/Greater.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Inventory_Message_StackChanged, "Rpg.Inventory.Message.StackChanged");

DEFINE_LOG_CATEGORY(LogRpgInventoryManager);

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	int32 GetInventoryManagerMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		return URpgInventoryManagerComponent::
			GetEffectiveMaxStackSizeForDefinition(ItemDef);
	}

	FRpgInventoryGridSize GetInventoryManagerFootprintForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, bool bRotated)
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

	bool CanInventoryManagerRotateDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment =
			URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
				ItemDef);
		return SpatialFragment && SpatialFragment->bAllowRotation;
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

	bool IsCompletelyUnsetPlacement(
		const FRpgInventoryGridPlacement& Placement)
	{
		return Placement.ContainerHandle.Root.IsNone() &&
			!Placement.ContainerHandle.ItemOwnerId.IsValid() &&
			Placement.ContainerHandle.ContainerId.IsNone() &&
			Placement.ContainerHandle.Depth == 0 &&
			Placement.X == INDEX_NONE &&
			Placement.Y == INDEX_NONE &&
			Placement.Width == 1 &&
			Placement.Height == 1 &&
			!Placement.bRotated;
	}

	bool ArePlacementSnapshotsExactlyEqual(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool IsItemOwnedHandleDepthOverflow(
		const FRpgInventoryContainerHandle& Handle)
	{
		return Handle.Root.IsNone() && Handle.ItemOwnerId.IsValid() &&
			!Handle.ContainerId.IsNone() &&
			Handle.Depth > RpgInventoryMaxItemOwnedDepth;
	}

	bool AreInventoryRuntimeStatesExactlyEqual(
		const TArray<FRpgInventoryFragmentStatePayload>& A,
		const TArray<FRpgInventoryFragmentStatePayload>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].FragmentId != B[Index].FragmentId ||
				A[Index].Version != B[Index].Version ||
				A[Index].Payload != B[Index].Payload)
			{
				return false;
			}
		}
		return true;
	}

	ERpgInventoryMutationResultCode EvaluateScratchPlacement(
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

	bool FindFirstFitInScratch(
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
					if (EvaluateScratchPlacement(Candidate, GridSize, Occupancy) ==
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

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(Entry->Instance->GetItemDef());
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

	if (TargetPlacement.bRotated && !CanInventoryManagerRotateDefinition(ItemDefinition))
	{
		return false;
	}

	const FRpgInventoryGridSize UnrotatedFootprint = GetInventoryManagerFootprintForDefinition(ItemDefinition, false);
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
		return FindFirstFitInScratch(
			ContainerHandle,
			GridSize,
			GetInventoryManagerFootprintForDefinition(ItemDef, false),
			CanInventoryManagerRotateDefinition(ItemDef),
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
		GetInventoryManagerFootprintForDefinition(ItemDef, false);
	if (!Footprint.IsValid())
	{
		return false;
	}
	bool bAllowRotation = CanInventoryManagerRotateDefinition(ItemDef);
	if (Inventory->ShouldUseSingleCellPlacementForContainer(ContainerHandle))
	{
		Footprint.Width = 1;
		Footprint.Height = 1;
		bAllowRotation = false;
	}
	return FindFirstFitInScratch(
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
			 !GetInventoryManagerFootprintForDefinition(
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
	FDoRepLifetimeParams InventoryStateParams;
	InventoryStateParams.Condition = COND_Dynamic;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InventoryList, InventoryStateParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InventoryRevision, InventoryStateParams);
}

void URpgInventoryManagerComponent::GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const
{
	Super::GetReplicatedCustomConditionState(OutActiveState);
	const ELifetimeCondition InventoryCondition = ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly
		? COND_OwnerOnly
		: COND_None;
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(ThisClass, InventoryList, InventoryCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(ThisClass, InventoryRevision, InventoryCondition);
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

void URpgInventoryManagerComponent::SetCapacityMode(ERpgInventoryCapacityMode NewCapacityMode)
{
	if (IsInventoryMutationLocked())
	{
		return;
	}
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
	if (IsInventoryMutationLocked())
	{
		return;
	}
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
	if (IsInventoryMutationLocked())
	{
		return;
	}
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

bool URpgInventoryManagerComponent::ExpandDefaultGridToMinimum(
	FRpgInventoryGridSize MinimumSize)
{
	if (IsInventoryMutationLocked())
	{
		return false;
	}
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld =
		World && World->IsGameWorld() && IsRegistered() &&
		!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (!MinimumSize.IsValid() ||
		(bIsRuntimeGameWorld &&
			(!OwningActor || !OwningActor->HasAuthority())))
	{
		return false;
	}

	FRpgInventoryGridSize ExpandedSize;
	ExpandedSize.Width =
		FMath::Max(DefaultGridSize.Width, MinimumSize.Width);
	ExpandedSize.Height =
		FMath::Max(DefaultGridSize.Height, MinimumSize.Height);
	if (ExpandedSize.Width == DefaultGridSize.Width &&
		ExpandedSize.Height == DefaultGridSize.Height)
	{
		return true;
	}

	DefaultGridSize = ExpandedSize;
	if (bIsRuntimeGameWorld)
	{
		BroadcastCapacityChanged();
		OwningActor->ForceNetUpdate();
	}
	return true;
}

TArray<URpgInventoryItemInstance*> URpgInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

TArray<FRpgInventoryEntryView> URpgInventoryManagerComponent::GetAllEntries() const
{
	return InventoryList.GetAllEntries();
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::GetItemAtContainerCell(FRpgInventoryContainerHandle ContainerHandle, int32 X, int32 Y) const
{
	return InventoryList.GetItemAtCell(ContainerHandle, X, Y);
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

URpgInventoryItemInstance* URpgInventoryManagerComponent::FindItemById(FRpgInventoryItemId ItemId) const
{
	const FRpgInventoryEntry* Entry = InventoryList.FindEntryByItemId(ItemId);
	return Entry ? Entry->Instance.Get() : nullptr;
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

FRpgInventoryMutationResult URpgInventoryManagerComponent::ExecuteCrossInventoryTransfer(
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryMutationRequest Request,
	bool bAllowPartialStackPickup)
{
	Request.EnsureRequestId();
	FRpgInventoryMutationResult Result;
	if (TryReplayRecentMutation(
			Request,
			TargetInventory,
			bAllowPartialStackPickup,
			Result))
	{
		return Result;
	}
	auto CacheResult =
		[this, &Request, TargetInventory, bAllowPartialStackPickup](
			FRpgInventoryMutationResult ResultToCache)
		{
			return CacheRecentMutationResult(
				Request,
				TargetInventory,
				bAllowPartialStackPickup,
				MoveTemp(ResultToCache));
		};

	Result.RequestId = Request.RequestId;
	Result.Operation = Request.Operation;
	Result.RequestedQuantity = Request.Quantity;
	auto Reject = [&Result, &CacheResult](ERpgInventoryMutationResultCode Code)
	{
		Result.Code = Code;
		Result.AppliedQuantity = 0;
		Result.Deltas.Reset();
		return CacheResult(MoveTemp(Result));
	};
	if (IsInventoryMutationLocked() ||
		(TargetInventory && TargetInventory->IsInventoryMutationLocked()))
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidRequest);
	}

	AActor* const SourceOwner = GetOwner();
	AActor* const TargetOwner = TargetInventory ? TargetInventory->GetOwner() : nullptr;
	if (!SourceOwner || !TargetOwner || !SourceOwner->HasAuthority() || !TargetOwner->HasAuthority())
	{
		return Reject(ERpgInventoryMutationResultCode::AuthorityRequired);
	}
	if (IsItemOwnedHandleDepthOverflow(Request.Source) ||
		IsItemOwnedHandleDepthOverflow(Request.Target) ||
		IsItemOwnedHandleDepthOverflow(
			Request.ExpectedSourcePlacement.GetContainerHandle()) ||
		IsItemOwnedHandleDepthOverflow(
			Request.TargetPlacement.GetContainerHandle()))
	{
		return Reject(
			ERpgInventoryMutationResultCode::MaxDepthExceeded);
	}
	if (!TargetInventory || TargetInventory == this ||
		(Request.Operation != ERpgInventoryMutationOperation::Transfer &&
			Request.Operation != ERpgInventoryMutationOperation::Pickup &&
			Request.Operation != ERpgInventoryMutationOperation::Drop) ||
		!Request.Source.IsValid() || !Request.Target.IsValid() ||
		SourceOwner->GetWorld() != TargetOwner->GetWorld())
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidRequest);
	}
	TGuardValue<bool> SourceTransferGuard(
		bIsApplyingCrossInventoryTransfer,
		true);
	TGuardValue<bool> TargetTransferGuard(
		TargetInventory->bIsApplyingCrossInventoryTransfer,
		true);
	const bool bMayApplyPartial =
		bAllowPartialStackPickup &&
		Request.Operation == ERpgInventoryMutationOperation::Pickup;

	// Compatibility imports can still contain a legacy stacked container provider.
	// Classify an exact partial request as the operation-contract violation it is before
	// whole-graph validation reports the already known legacy stack shape as corrupt.
	const FRpgInventoryEntry* PreliminarySourceEntry =
		InventoryList.FindEntryByItemId(Request.ItemId);
	if (PreliminarySourceEntry && PreliminarySourceEntry->Instance &&
		PreliminarySourceEntry->Placement.GetContainerHandle() == Request.Source &&
		Request.ExpectedEntryId.IsValid() &&
		Request.ExpectedEntryId == PreliminarySourceEntry->EntryId &&
		Request.ExpectedSourcePlacement.IsValid() &&
		ArePlacementSnapshotsExactlyEqual(
			Request.ExpectedSourcePlacement,
			PreliminarySourceEntry->Placement) &&
		Request.ExpectedSourceQuantity == PreliminarySourceEntry->StackCount &&
		Request.Quantity > 0 &&
		Request.Quantity < PreliminarySourceEntry->StackCount &&
		PreliminarySourceEntry->Instance->FindFragmentByClass<
			URpgInventoryFragment_ItemContainer>() != nullptr)
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidRequest);
	}

	const int32 SourceRevisionBefore = InventoryRevision;
	const int32 TargetRevisionBefore = TargetInventory->InventoryRevision;

	// Runtime transfer validates complete live graphs directly. Persistence serialization is deliberately absent here
	// so an unrelated item's save payload can never block or churn a gameplay transfer. Capacity is target-only: a
	// source that became over capacity after a runtime shrink must remain able to reduce its deficit through egress.
	FValidatedInventoryGraph SourceGraph;
	FValidatedInventoryGraph TargetGraph;
	ERpgInventoryMutationResultCode LiveGraphCode =
		ERpgInventoryMutationResultCode::Success;
	if (!ValidateLiveInventoryGraph(false, SourceGraph, LiveGraphCode) ||
		!TargetInventory->ValidateLiveInventoryGraph(
			true,
			TargetGraph,
			LiveGraphCode))
	{
		return Reject(LiveGraphCode);
	}
	const TMap<FRpgInventoryItemId, int32>& SourceIndexByItemId =
		SourceGraph.IndexByItemId;
	const TMap<FRpgInventoryItemId, int32>& TargetIndexByItemId =
		TargetGraph.IndexByItemId;

	const int32* SourceEntryIndex =
		SourceIndexByItemId.Find(Request.ItemId);
	const FRpgInventoryEntry* SourceEntry = SourceEntryIndex
		? &InventoryList.Entries[*SourceEntryIndex]
		: nullptr;
	if (!SourceEntry || !SourceEntry->Instance)
	{
		return Reject(ERpgInventoryMutationResultCode::ItemNotFound);
	}
	if (SourceEntry->Placement.GetContainerHandle() != Request.Source)
	{
		return Reject(ERpgInventoryMutationResultCode::SourceMismatch);
	}
	if ((Request.ExpectedEntryId.IsValid() &&
			Request.ExpectedEntryId != SourceEntry->EntryId) ||
		(Request.ExpectedSourcePlacement.IsValid() &&
			!ArePlacementSnapshotsExactlyEqual(
				Request.ExpectedSourcePlacement,
				SourceEntry->Placement)) ||
		(Request.ExpectedSourceQuantity > 0 &&
			Request.ExpectedSourceQuantity != SourceEntry->StackCount))
	{
		return Reject(ERpgInventoryMutationResultCode::SourceMismatch);
	}

	if (!Request.ExpectedEntryId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.ExpectedSourceQuantity <= 0)
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidRequest);
	}
	const int32 RequestedQuantity = Request.Quantity;
	if (RequestedQuantity <= 0 || RequestedQuantity > SourceEntry->StackCount)
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidRequest);
	}
	Result.RequestedQuantity = RequestedQuantity;

	struct FPreparedSourceEntry
	{
		int32 SourceIndex = INDEX_NONE;
		FRpgInventoryEntry Before;
		FRpgInventoryItemId ExpectedItemId;
		TSubclassOf<URpgInventoryItemDefinition> ExpectedItemDefinition;
		TObjectPtr<UObject> ExpectedOuter = nullptr;
		TArray<FRpgInventoryFragmentStatePayload> ExpectedRuntimeState;
	};

	TArray<FPreparedSourceEntry> TransferSubtree;
	TransferSubtree.Reserve(InventoryList.Entries.Num());
	TArray<int32> TransferSubtreeIndices;
	if (!SourceGraph.GatherSubtreeIndices(
			*SourceEntryIndex,
			TransferSubtreeIndices))
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidContainer);
	}
	TransferSubtreeIndices.RemoveSingle(*SourceEntryIndex);
	TransferSubtreeIndices.Insert(*SourceEntryIndex, 0);
	for (const int32 CandidateIndex : TransferSubtreeIndices)
	{
		FPreparedSourceEntry& Prepared =
			TransferSubtree.AddDefaulted_GetRef();
		Prepared.SourceIndex = CandidateIndex;
		Prepared.Before = InventoryList.Entries[CandidateIndex];
		Prepared.ExpectedItemId = Prepared.Before.Instance->GetItemId();
		Prepared.ExpectedItemDefinition = Prepared.Before.Instance->GetItemDef();
		Prepared.ExpectedOuter = Prepared.Before.Instance->GetOuter();
		if (!Prepared.Before.Instance->ExportRuntimeState(
				Prepared.ExpectedRuntimeState))
		{
			return Reject(ERpgInventoryMutationResultCode::InternalError);
		}
	}

	const FRpgInventoryEntry SourceRootBefore =
		TransferSubtree[0].Before;
	const bool bTransfersSubtree =
		TransferSubtree.Num() > 1 ||
		SourceRootBefore.Instance->FindFragmentByClass<
			URpgInventoryFragment_ItemContainer>() != nullptr;
	if (bTransfersSubtree &&
		RequestedQuantity != SourceRootBefore.StackCount)
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidRequest);
	}

	TArray<URpgInventoryManagerComponent*> TargetOwnerInventories;
	TargetOwner->GetComponents(TargetOwnerInventories);
	for (const FPreparedSourceEntry& Incoming : TransferSubtree)
	{
		const FRpgInventoryItemId IncomingItemId =
			Incoming.Before.Instance->GetItemId();
		const bool bConflictsWithTargetSibling =
			TargetOwnerInventories.ContainsByPredicate(
				[this, TargetInventory, &IncomingItemId](
					const URpgInventoryManagerComponent* Candidate)
				{
					return Candidate &&
						Candidate != TargetInventory &&
						Candidate != this &&
						Candidate->FindItemById(IncomingItemId) !=
							nullptr;
				});
		if (bConflictsWithTargetSibling)
		{
			return Reject(ERpgInventoryMutationResultCode::DuplicateItemId);
		}
	}

	FRpgInventoryEntryView SourceView;
	SourceView.InventoryOwner = this;
	SourceView.Instance = SourceRootBefore.Instance;
	SourceView.EntryId = SourceRootBefore.EntryId;
	SourceView.ItemId = SourceRootBefore.Instance->GetItemId();
	SourceView.StackCount = SourceRootBefore.StackCount;
	SourceView.Placement = SourceRootBefore.Placement;
	FRpgInventoryPlacementQuery PlacementQuery;
	PlacementQuery.Purpose = ERpgInventoryPlacementPurpose::Transfer;
	PlacementQuery.Search = IsCompletelyUnsetPlacement(
		Request.TargetPlacement)
		? ERpgInventoryPlacementSearch::FirstFit
		: ERpgInventoryPlacementSearch::Exact;
	PlacementQuery.Subject = FRpgInventoryPlacementSubject::FromIncomingInstance(
		this,
		SourceView,
		RequestedQuantity);
	PlacementQuery.TargetContainer = Request.Target;
	PlacementQuery.ExactPlacement = Request.TargetPlacement;
	const FRpgInventoryPlacementPlan PlacementPlan =
		TargetInventory->EvaluatePlacement(PlacementQuery);
	if (!PlacementPlan.IsSuccess() || PlacementPlan.AppliedQuantity <= 0 ||
		(!bMayApplyPartial &&
		 PlacementPlan.AppliedQuantity != RequestedQuantity) ||
		(bTransfersSubtree &&
		 PlacementPlan.AppliedQuantity != RequestedQuantity))
	{
		return Reject(
			PlacementPlan.Code == ERpgInventoryMutationResultCode::PartiallyApplied
			? ERpgInventoryMutationResultCode::NoSpace
			: PlacementPlan.Code);
	}
	if (PlacementPlan.SourceRevision != SourceRevisionBefore ||
		PlacementPlan.TargetRevision != TargetRevisionBefore)
	{
		return Reject(ERpgInventoryMutationResultCode::SourceMismatch);
	}

	struct FPreparedTargetMerge
	{
		int32 TargetIndex = INDEX_NONE;
		FRpgInventoryEntry Before;
		int32 NewCount = 0;
	};
	struct FPreparedTargetAddition
	{
		TObjectPtr<URpgInventoryItemInstance> Instance;
		FGuid EntryId;
		int32 StackCount = 0;
		FRpgInventoryGridPlacement Placement;
	};

	TArray<FPreparedTargetMerge> TargetMerges;
	TArray<FPreparedTargetAddition> TargetAdditions;
	TArray<FPreparedSourceEntry> SourceRemovals;
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>> StagedTargetInstanceRoots;
	TMap<int32, int32> MergePlanIndexByTargetIndex;
	TSet<FRpgInventoryItemId> StagedTargetItemIds;
	TSet<FGuid> StagedTargetEntryIds;
	int32 AppliedQuantity = 0;
	bool bSourceStackChanges = false;
	int32 SourceNewCount = SourceRootBefore.StackCount;

	auto MakeUniqueTargetEntryId = [&TargetGraph, &StagedTargetEntryIds]()
	{
		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const FGuid Candidate = FGuid::NewGuid();
			if (Candidate.IsValid() &&
				!TargetGraph.IndexByEntryId.Contains(Candidate) &&
				!StagedTargetEntryIds.Contains(Candidate))
			{
				return Candidate;
			}
		}
		return FGuid();
	};

	auto MakeUniqueTargetItemId =
		[&SourceIndexByItemId,
		 &TargetOwnerInventories,
		 &StagedTargetItemIds]()
	{
		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const FRpgInventoryItemId Candidate =
				FRpgInventoryItemId::NewId();
			if (!Candidate.IsValid() ||
				SourceIndexByItemId.Contains(Candidate) ||
				StagedTargetItemIds.Contains(Candidate))
			{
				continue;
			}
			const bool bAlreadyOwned =
				TargetOwnerInventories.ContainsByPredicate(
					[&Candidate](
						const URpgInventoryManagerComponent* Inventory)
					{
						return Inventory &&
							Inventory->FindItemById(Candidate) != nullptr;
					});
			if (!bAlreadyOwned)
			{
				return Candidate;
			}
		}
		return FRpgInventoryItemId();
	};

	auto StageTargetInstance =
		[SourceOwner, TargetOwner, &StagedTargetInstanceRoots](
			const FRpgInventoryEntry& Source,
			const FRpgInventoryItemId& TargetItemId,
			bool bReuseSourceInstance)
			-> URpgInventoryItemInstance*
	{
		if (!Source.Instance || !TargetItemId.IsValid())
		{
			return nullptr;
		}
		if (bReuseSourceInstance)
		{
			return SourceOwner == TargetOwner &&
				Source.Instance->GetItemId() == TargetItemId
					? Source.Instance.Get()
					: nullptr;
		}

		TArray<FRpgInventoryFragmentStatePayload> RuntimeState;
		if (!Source.Instance->ExportRuntimeState(RuntimeState))
		{
			return nullptr;
		}

		URpgInventoryItemInstance* StagedInstance =
			NewObject<URpgInventoryItemInstance>(TargetOwner);
		if (!StagedInstance)
		{
			return nullptr;
		}
		// Fragment initialization and runtime-state import are extension points and may
		// synchronously collect garbage. Keep every detached staged instance alive until
		// the transaction either rejects or publishes it through the target FastArray.
		StagedTargetInstanceRoots.Emplace(StagedInstance);
		StagedInstance->SetItemDef(Source.Instance->GetItemDef());
		if (!StagedInstance->RestoreItemId(TargetItemId))
		{
			return nullptr;
		}
		const URpgInventoryItemDefinition* ItemCDO =
			GetDefault<URpgInventoryItemDefinition>(
				Source.Instance->GetItemDef());
		if (!ItemCDO)
		{
			return nullptr;
		}
		for (URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnInstanceCreated(StagedInstance);
			}
		}
		return StagedInstance->ImportRuntimeState(RuntimeState)
			? StagedInstance
			: nullptr;
	};

	auto AddPreparedTargetEntry =
		[&TargetAdditions,
		 &StagedTargetItemIds,
		 &StagedTargetEntryIds,
		 &MakeUniqueTargetEntryId](
			URpgInventoryItemInstance* Instance,
			int32 StackCount,
			const FRpgInventoryGridPlacement& Placement)
	{
		if (!Instance || StackCount <= 0 || !Placement.IsValid() ||
			!Instance->GetItemId().IsValid() ||
			StagedTargetItemIds.Contains(Instance->GetItemId()))
		{
			return false;
		}
		const FGuid NewEntryId = MakeUniqueTargetEntryId();
		if (!NewEntryId.IsValid())
		{
			return false;
		}

		FPreparedTargetAddition& Addition =
			TargetAdditions.AddDefaulted_GetRef();
		Addition.Instance = Instance;
		Addition.EntryId = NewEntryId;
		Addition.StackCount = StackCount;
		Addition.Placement = Placement;
		StagedTargetItemIds.Add(Instance->GetItemId());
		StagedTargetEntryIds.Add(NewEntryId);
		return true;
	};

	if (bTransfersSubtree)
	{
		if (PlacementPlan.Steps.Num() != 1 ||
			PlacementPlan.Steps[0].Resolution !=
				ERpgInventoryPlacementResolution::Place)
		{
			return Reject(ERpgInventoryMutationResultCode::InternalError);
		}

		const FRpgInventoryGridPlacement& TargetRootPlacement =
			PlacementPlan.Steps[0].Placement;
		const int32 DepthDelta =
			static_cast<int32>(
				TargetRootPlacement.GetContainerHandle().Depth) -
			static_cast<int32>(
				SourceRootBefore.Placement.GetContainerHandle().Depth);
		for (const FPreparedSourceEntry& Moving : TransferSubtree)
		{
			FRpgInventoryGridPlacement TargetPlacement =
				Moving.Before.Placement;
			if (Moving.Before.Instance->GetItemId() == Request.ItemId)
			{
				TargetPlacement = TargetRootPlacement;
			}
			else
			{
				FRpgInventoryContainerHandle TargetContainer =
					TargetPlacement.GetContainerHandle();
				const int32 NewDepth =
					static_cast<int32>(TargetContainer.Depth) +
					DepthDelta;
				if (NewDepth <= 0 ||
					NewDepth > RpgInventoryMaxItemOwnedDepth)
				{
					return Reject(
						ERpgInventoryMutationResultCode::MaxDepthExceeded);
				}
				TargetContainer.Depth = static_cast<uint8>(NewDepth);
				TargetPlacement.SetContainerHandle(TargetContainer);
			}

			const FRpgInventoryItemId TargetItemId =
				Moving.Before.Instance->GetItemId();
			const bool bReuseSourceInstance = SourceOwner == TargetOwner;
			URpgInventoryItemInstance* TargetInstance =
				StageTargetInstance(
					Moving.Before,
					TargetItemId,
					bReuseSourceInstance);
			if (!TargetInstance ||
				!AddPreparedTargetEntry(
					TargetInstance,
					Moving.Before.StackCount,
					TargetPlacement))
			{
				return Reject(ERpgInventoryMutationResultCode::InternalError);
			}

			SourceRemovals.Add(Moving);
			FRpgInventoryMutationDelta& Delta =
				Result.Deltas.AddDefaulted_GetRef();
			Delta.Kind = ERpgInventoryMutationDeltaKind::Moved;
			Delta.ItemId = TargetItemId;
			Delta.BeforeContainer =
				Moving.Before.Placement.GetContainerHandle();
			Delta.AfterContainer = TargetPlacement.GetContainerHandle();
			Delta.BeforePlacement = Moving.Before.Placement;
			Delta.AfterPlacement = TargetPlacement;
			Delta.PreviousQuantity = Moving.Before.StackCount;
			Delta.NewQuantity = Moving.Before.StackCount;
		}
		AppliedQuantity = RequestedQuantity;
	}
	else
	{
		const bool bHasMerge = PlacementPlan.Steps.ContainsByPredicate(
			[](const FRpgInventoryPlacementStep& Step)
			{
				return Step.Resolution ==
					ERpgInventoryPlacementResolution::Merge;
			});

		for (const FRpgInventoryPlacementStep& Step : PlacementPlan.Steps)
		{
			if (Step.Quantity <= 0)
			{
				return Reject(ERpgInventoryMutationResultCode::InternalError);
			}

			if (Step.Resolution ==
				ERpgInventoryPlacementResolution::Merge)
			{
				const int32* TargetIndex =
					TargetIndexByItemId.Find(Step.TargetItemId);
				if (!TargetIndex ||
					!TargetInventory->InventoryList.Entries.IsValidIndex(
						*TargetIndex))
				{
					return Reject(
						ERpgInventoryMutationResultCode::InternalError);
				}

				const FRpgInventoryEntry& MergeTarget =
					TargetInventory->InventoryList.Entries[*TargetIndex];
				if (!MergeTarget.Instance ||
					MergeTarget.EntryId != Step.TargetEntryId ||
					!ArePlacementSnapshotsExactlyEqual(
						MergeTarget.Placement,
						Step.Placement) ||
					!SourceRootBefore.Instance->IsStackCompatibleWith(
						MergeTarget.Instance))
				{
					return Reject(
						ERpgInventoryMutationResultCode::StackIncompatible);
				}

				int32 PreparedMergeIndex = INDEX_NONE;
				if (const int32* ExistingPlanIndex =
						MergePlanIndexByTargetIndex.Find(*TargetIndex))
				{
					PreparedMergeIndex = *ExistingPlanIndex;
				}
				else
				{
					PreparedMergeIndex = TargetMerges.AddDefaulted();
					FPreparedTargetMerge& NewMerge =
						TargetMerges[PreparedMergeIndex];
					NewMerge.TargetIndex = *TargetIndex;
					NewMerge.Before = MergeTarget;
					NewMerge.NewCount = MergeTarget.StackCount;
					MergePlanIndexByTargetIndex.Add(
						*TargetIndex,
						PreparedMergeIndex);
				}

				FPreparedTargetMerge& PreparedMerge =
					TargetMerges[PreparedMergeIndex];
				const int32 MaxStackSize =
					GetInventoryManagerMaxStackSizeForDefinition(
						MergeTarget.Instance->GetItemDef());
				if (PreparedMerge.NewCount >
					MaxStackSize - Step.Quantity)
				{
					return Reject(
						ERpgInventoryMutationResultCode::StackLimitReached);
				}
				PreparedMerge.NewCount += Step.Quantity;
				AppliedQuantity += Step.Quantity;
			}
			else if (Step.Resolution ==
				ERpgInventoryPlacementResolution::Place)
			{
				const bool bPreserveSourceIdentity =
					!bHasMerge &&
					Step.Quantity == SourceRootBefore.StackCount;
				const FRpgInventoryItemId TargetItemId =
					bPreserveSourceIdentity
						? Request.ItemId
						: MakeUniqueTargetItemId();
				if (!TargetItemId.IsValid())
				{
					return Reject(
						ERpgInventoryMutationResultCode::DuplicateItemId);
				}
				URpgInventoryItemInstance* TargetInstance =
					StageTargetInstance(
						SourceRootBefore,
						TargetItemId,
						bPreserveSourceIdentity &&
							SourceOwner == TargetOwner);
				if (!TargetInstance ||
					!AddPreparedTargetEntry(
						TargetInstance,
						Step.Quantity,
						Step.Placement))
				{
					return Reject(
						ERpgInventoryMutationResultCode::InternalError);
				}

				FRpgInventoryMutationDelta& Delta =
					Result.Deltas.AddDefaulted_GetRef();
				Delta.Kind = ERpgInventoryMutationDeltaKind::Added;
				Delta.ItemId = TargetItemId;
				Delta.AfterContainer =
					Step.Placement.GetContainerHandle();
				Delta.AfterPlacement = Step.Placement;
				Delta.NewQuantity = Step.Quantity;
				AppliedQuantity += Step.Quantity;
			}
			else
			{
				return Reject(ERpgInventoryMutationResultCode::InternalError);
			}
		}

		for (const FPreparedTargetMerge& Merge : TargetMerges)
		{
			FRpgInventoryMutationDelta& Delta =
				Result.Deltas.AddDefaulted_GetRef();
			Delta.Kind = ERpgInventoryMutationDeltaKind::StackChanged;
			Delta.ItemId = Merge.Before.Instance->GetItemId();
			Delta.BeforeContainer =
				Merge.Before.Placement.GetContainerHandle();
			Delta.AfterContainer = Delta.BeforeContainer;
			Delta.BeforePlacement = Merge.Before.Placement;
			Delta.AfterPlacement = Merge.Before.Placement;
			Delta.PreviousQuantity = Merge.Before.StackCount;
			Delta.NewQuantity = Merge.NewCount;
		}

		if (AppliedQuantity <= 0 ||
			AppliedQuantity != PlacementPlan.AppliedQuantity ||
			AppliedQuantity > RequestedQuantity)
		{
			return Reject(ERpgInventoryMutationResultCode::InternalError);
		}

		SourceNewCount = SourceRootBefore.StackCount - AppliedQuantity;
		if (SourceNewCount < 0)
		{
			return Reject(ERpgInventoryMutationResultCode::InternalError);
		}
		if (SourceNewCount == 0)
		{
			SourceRemovals.Add(TransferSubtree[0]);
		}
		else
		{
			bSourceStackChanges = true;
		}

		FRpgInventoryMutationDelta& SourceDelta =
			Result.Deltas.AddDefaulted_GetRef();
		SourceDelta.Kind = SourceNewCount == 0
			? ERpgInventoryMutationDeltaKind::Removed
			: ERpgInventoryMutationDeltaKind::StackChanged;
		SourceDelta.ItemId = Request.ItemId;
		SourceDelta.BeforeContainer = Request.Source;
		SourceDelta.BeforePlacement = SourceRootBefore.Placement;
		SourceDelta.PreviousQuantity = SourceRootBefore.StackCount;
		SourceDelta.NewQuantity = SourceNewCount;
		if (SourceNewCount > 0)
		{
			SourceDelta.AfterContainer = Request.Source;
			SourceDelta.AfterPlacement = SourceRootBefore.Placement;
		}
	}

	if (AppliedQuantity <= 0 ||
		AppliedQuantity != PlacementPlan.AppliedQuantity ||
		(!bMayApplyPartial && AppliedQuantity != RequestedQuantity) ||
		(bTransfersSubtree && AppliedQuantity != RequestedQuantity))
	{
		return Reject(ERpgInventoryMutationResultCode::InternalError);
	}

	// Validate the exact post-commit graphs before entering the no-fail region. This single contract proves capacity,
	// identity uniqueness, subtree closure, rebased depth, provider rules, bounds, and occupancy for both sides.
	TSet<int32> SourceRemovalIndices;
	for (const FPreparedSourceEntry& Removal : SourceRemovals)
	{
		SourceRemovalIndices.Add(Removal.SourceIndex);
	}
	TArray<FRpgInventoryEntry> ProjectedSourceEntries;
	ProjectedSourceEntries.Reserve(
		InventoryList.Entries.Num() - SourceRemovalIndices.Num());
	for (int32 EntryIndex = 0;
		 EntryIndex < InventoryList.Entries.Num();
		 ++EntryIndex)
	{
		if (SourceRemovalIndices.Contains(EntryIndex))
		{
			continue;
		}
		FRpgInventoryEntry& Projected =
			ProjectedSourceEntries.Add_GetRef(InventoryList.Entries[EntryIndex]);
		if (bSourceStackChanges && EntryIndex == *SourceEntryIndex)
		{
			Projected.StackCount = SourceNewCount;
		}
	}

	TArray<FRpgInventoryEntry> ProjectedTargetEntries =
		TargetInventory->InventoryList.Entries;
	for (const FPreparedTargetMerge& Merge : TargetMerges)
	{
		if (!ProjectedTargetEntries.IsValidIndex(Merge.TargetIndex))
		{
			return Reject(ERpgInventoryMutationResultCode::InternalError);
		}
		ProjectedTargetEntries[Merge.TargetIndex].StackCount = Merge.NewCount;
	}
	for (const FPreparedTargetAddition& Addition : TargetAdditions)
	{
		FRpgInventoryEntry& Projected =
			ProjectedTargetEntries.AddDefaulted_GetRef();
		Projected.Instance = Addition.Instance;
		Projected.EntryId = Addition.EntryId;
		Projected.StackCount = Addition.StackCount;
		Projected.Placement = Addition.Placement;
	}

	FValidatedInventoryGraph ProjectedSourceGraph;
	FValidatedInventoryGraph ProjectedTargetGraph;
	ERpgInventoryMutationResultCode ProjectedGraphCode =
		ERpgInventoryMutationResultCode::Success;
	if (!ValidateInventoryGraph(
			ProjectedSourceEntries,
			SourceOwner,
			false,
			ProjectedSourceGraph,
			ProjectedGraphCode) ||
		!TargetInventory->ValidateInventoryGraph(
			ProjectedTargetEntries,
			TargetOwner,
			true,
			ProjectedTargetGraph,
			ProjectedGraphCode))
	{
		return Reject(ProjectedGraphCode);
	}

	auto ArePlansEquivalent = [](
		const FRpgInventoryPlacementPlan& A,
		const FRpgInventoryPlacementPlan& B)
	{
		if (A.Code != B.Code || A.SourceRevision != B.SourceRevision ||
			A.TargetRevision != B.TargetRevision ||
			A.RequestedQuantity != B.RequestedQuantity ||
			A.AppliedQuantity != B.AppliedQuantity ||
			A.Steps.Num() != B.Steps.Num())
		{
			return false;
		}
		for (int32 StepIndex = 0; StepIndex < A.Steps.Num(); ++StepIndex)
		{
			const FRpgInventoryPlacementStep& Left = A.Steps[StepIndex];
			const FRpgInventoryPlacementStep& Right = B.Steps[StepIndex];
			if (Left.Resolution != Right.Resolution ||
				!ArePlacementSnapshotsExactlyEqual(
					Left.Placement,
					Right.Placement) ||
				Left.Quantity != Right.Quantity ||
				Left.TargetItemId != Right.TargetItemId ||
				Left.TargetEntryId != Right.TargetEntryId ||
				Left.DisplacedItemId != Right.DisplacedItemId ||
				Left.DisplacedEntryId != Right.DisplacedEntryId ||
				!ArePlacementSnapshotsExactlyEqual(
					Left.DisplacedPlacement,
					Right.DisplacedPlacement))
			{
				return false;
			}
		}
		return true;
	};

	// Staging may execute fragment hooks. Re-evaluate and compare the complete operation before
	// entering the no-fail commit region so any unexpected side effect still rejects atomically.
	const FRpgInventoryPlacementPlan RevalidatedPlan =
		TargetInventory->EvaluatePlacement(PlacementQuery);
	if (!ArePlansEquivalent(PlacementPlan, RevalidatedPlan))
	{
		const ERpgInventoryMutationResultCode RevalidationCode =
			!RevalidatedPlan.IsSuccess()
				? RevalidatedPlan.Code
				: ERpgInventoryMutationResultCode::SourceMismatch;
		return Reject(
			RevalidationCode ==
				ERpgInventoryMutationResultCode::PartiallyApplied
					? ERpgInventoryMutationResultCode::NoSpace
					: RevalidationCode);
	}

	auto EntryStillMatches = [](
		const FRpgInventoryEntry& Live,
		const FRpgInventoryEntry& Before)
	{
		return Live.Instance == Before.Instance &&
			Live.EntryId == Before.EntryId &&
			Live.StackCount == Before.StackCount &&
			ArePlacementSnapshotsExactlyEqual(
				Live.Placement,
				Before.Placement);
	};
	if (InventoryRevision != SourceRevisionBefore ||
		TargetInventory->InventoryRevision != TargetRevisionBefore)
	{
		return Reject(ERpgInventoryMutationResultCode::SourceMismatch);
	}
	for (const FPreparedSourceEntry& Moving : TransferSubtree)
	{
		TArray<FRpgInventoryFragmentStatePayload> CurrentRuntimeState;
		if (!InventoryList.Entries.IsValidIndex(Moving.SourceIndex) ||
			!EntryStillMatches(
				InventoryList.Entries[Moving.SourceIndex],
				Moving.Before) ||
			!InventoryList.Entries[Moving.SourceIndex].Instance ||
			InventoryList.Entries[Moving.SourceIndex].Instance->GetItemId() !=
				Moving.ExpectedItemId ||
			InventoryList.Entries[Moving.SourceIndex].Instance->GetItemDef() !=
				Moving.ExpectedItemDefinition ||
			InventoryList.Entries[Moving.SourceIndex].Instance->GetOuter() !=
				Moving.ExpectedOuter ||
			!InventoryList.Entries[Moving.SourceIndex].Instance->ExportRuntimeState(
				CurrentRuntimeState) ||
			!AreInventoryRuntimeStatesExactlyEqual(
				Moving.ExpectedRuntimeState,
				CurrentRuntimeState))
		{
			return Reject(ERpgInventoryMutationResultCode::SourceMismatch);
		}
	}
	for (const FPreparedTargetMerge& Merge : TargetMerges)
	{
		if (!TargetInventory->InventoryList.Entries.IsValidIndex(
				Merge.TargetIndex) ||
			!EntryStillMatches(
				TargetInventory->InventoryList.Entries[Merge.TargetIndex],
				Merge.Before))
		{
			return Reject(ERpgInventoryMutationResultCode::SourceMismatch);
		}
	}

	TSet<FRpgInventoryItemId> RemovedSourceItemIds;
	for (const FPreparedSourceEntry& Removal : SourceRemovals)
	{
		RemovedSourceItemIds.Add(Removal.Before.Instance->GetItemId());
	}
	TargetOwnerInventories.Reset();
	TargetOwner->GetComponents(TargetOwnerInventories);
	for (const FPreparedTargetAddition& Addition : TargetAdditions)
	{
		if (TargetInventory->FindItemById(Addition.Instance->GetItemId()))
		{
			return Reject(ERpgInventoryMutationResultCode::DuplicateItemId);
		}
		for (URpgInventoryManagerComponent* Sibling : TargetOwnerInventories)
		{
			if (!Sibling || Sibling == TargetInventory)
			{
				continue;
			}
			URpgInventoryItemInstance* Existing =
				Sibling->FindItemById(Addition.Instance->GetItemId());
			if (!Existing)
			{
				continue;
			}
			const bool bMovesSameActorInstance =
				Sibling == this && SourceOwner == TargetOwner &&
				Existing == Addition.Instance &&
				RemovedSourceItemIds.Contains(
					Addition.Instance->GetItemId());
			if (!bMovesSameActorInstance)
			{
				return Reject(
					ERpgInventoryMutationResultCode::DuplicateItemId);
			}
		}
	}

	// No fallible gameplay work occurs below this line. Both lists become final before revisions,
	// replay-cache state, or synchronous observers can see the transaction.
	TargetInventory->InventoryList.Entries.Reserve(
		TargetInventory->InventoryList.Entries.Num() +
		TargetAdditions.Num());
	TArray<FRpgInventoryEntry> TargetChangedNotifications;
	TArray<FRpgInventoryEntry> TargetAddedNotifications;
	TArray<FRpgInventoryEntry> SourceChangedNotifications;
	TArray<FRpgInventoryEntry> SourceRemovedNotifications;
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>> SourceRemovalInstanceRoots;
	TargetChangedNotifications.Reserve(TargetMerges.Num());
	TargetAddedNotifications.Reserve(TargetAdditions.Num());
	SourceChangedNotifications.Reserve(bSourceStackChanges ? 1 : 0);
	SourceRemovedNotifications.Reserve(SourceRemovals.Num());
	SourceRemovalInstanceRoots.Reserve(SourceRemovals.Num());
	for (const FPreparedSourceEntry& Removal : SourceRemovals)
	{
		if (Removal.Before.Instance)
		{
			// Removed entries are no longer referenced by the source FastArray while
			// synchronous change listeners still receive their final removal messages.
			SourceRemovalInstanceRoots.Emplace(Removal.Before.Instance.Get());
		}
	}

	for (const FPreparedTargetMerge& Merge : TargetMerges)
	{
		FRpgInventoryEntry& Entry =
			TargetInventory->InventoryList.Entries[Merge.TargetIndex];
		Entry.StackCount = Merge.NewCount;
		TargetInventory->InventoryList.MarkItemDirty(Entry);
		TargetChangedNotifications.Add(Entry);
	}
	for (const FPreparedTargetAddition& Addition : TargetAdditions)
	{
		FRpgInventoryEntry& NewEntry =
			TargetInventory->InventoryList.Entries.AddDefaulted_GetRef();
		NewEntry.Instance = Addition.Instance;
		NewEntry.EntryId = Addition.EntryId;
		NewEntry.StackCount = Addition.StackCount;
		NewEntry.Placement = Addition.Placement;
		TargetInventory->InventoryList.MarkItemDirty(NewEntry);
		TargetAddedNotifications.Add(NewEntry);
	}
	if (!TargetAdditions.IsEmpty())
	{
		TargetInventory->InventoryList.MarkArrayDirty();
	}

	if (bSourceStackChanges)
	{
		FRpgInventoryEntry& Entry =
			InventoryList.Entries[*SourceEntryIndex];
		Entry.StackCount = SourceNewCount;
		InventoryList.MarkItemDirty(Entry);
		SourceChangedNotifications.Add(Entry);
	}
	else
	{
		TArray<int32> RemovalIndices;
		RemovalIndices.Reserve(SourceRemovals.Num());
		for (const FPreparedSourceEntry& Removal : SourceRemovals)
		{
			RemovalIndices.Add(Removal.SourceIndex);
			SourceRemovedNotifications.Add(Removal.Before);
		}
		RemovalIndices.Sort(TGreater<int32>());
		for (const int32 RemovalIndex : RemovalIndices)
		{
			InventoryList.Entries.RemoveAt(
				RemovalIndex,
				1,
				EAllowShrinking::No);
		}
		if (!RemovalIndices.IsEmpty())
		{
			InventoryList.MarkArrayDirty();
		}
	}

	if (IsUsingRegisteredSubObjectList())
	{
		for (const FPreparedSourceEntry& Removal : SourceRemovals)
		{
			RemoveReplicatedSubObject(Removal.Before.Instance);
		}
	}
	if (TargetInventory->IsUsingRegisteredSubObjectList() &&
		TargetInventory->IsReadyForReplication())
	{
		for (const FPreparedTargetAddition& Addition : TargetAdditions)
		{
			TargetInventory->AddReplicatedSubObject(
				Addition.Instance,
				TargetInventory->ReplicationPolicy ==
					ERpgInventoryReplicationPolicy::OwnerOnly
						? COND_OwnerOnly
						: COND_None);
		}
	}

	MarkInventoryStateDirty();
	TargetInventory->MarkInventoryStateDirty();
	Result.AppliedQuantity = AppliedQuantity;
	Result.Code = AppliedQuantity == RequestedQuantity
		? ERpgInventoryMutationResultCode::Success
		: ERpgInventoryMutationResultCode::PartiallyApplied;
	Result = CacheResult(MoveTemp(Result));

	SourceRemovedNotifications.Sort(
		[](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			return A.Placement.GetContainerHandle().Depth >
				B.Placement.GetContainerHandle().Depth;
		});
	TargetAddedNotifications.Sort(
		[](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
		{
			return A.Placement.GetContainerHandle().Depth <
				B.Placement.GetContainerHandle().Depth;
		});

	for (FRpgInventoryEntry& Changed : SourceChangedNotifications)
	{
		InventoryList.BroadcastChangeMessage(
			Changed,
			SourceRootBefore.StackCount,
			Changed.StackCount);
	}
	for (FRpgInventoryEntry& Removed : SourceRemovedNotifications)
	{
		InventoryList.BroadcastChangeMessage(
			Removed,
			Removed.StackCount,
			0);
	}
	for (int32 MergeIndex = 0;
		 MergeIndex < TargetChangedNotifications.Num();
		 ++MergeIndex)
	{
		TargetInventory->InventoryList.BroadcastChangeMessage(
			TargetChangedNotifications[MergeIndex],
			TargetMerges[MergeIndex].Before.StackCount,
			TargetChangedNotifications[MergeIndex].StackCount);
	}
	for (FRpgInventoryEntry& Added : TargetAddedNotifications)
	{
		TargetInventory->InventoryList.BroadcastChangeMessage(
			Added,
			0,
			Added.StackCount);
	}

	return Result;
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
				AddReplicatedSubObject(Instance, ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
			}
		}
	}
}

bool URpgInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly && (!RepFlags || !RepFlags->bNetOwner))
	{
		return WroteSomething;
	}

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


bool URpgInventoryManagerComponent::IsInventoryMutationLocked() const
{
	return bIsApplyingPickupBatch || bIsPlanningPickupBatch ||
		bIsApplyingCollectBatch || bIsApplyingCrossInventoryTransfer ||
		bIsRestoringInventoryGraph;
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


