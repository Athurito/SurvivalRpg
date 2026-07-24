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
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Inventory_Message_StackChanged, "Rpg.Inventory.Message.StackChanged");

DEFINE_LOG_CATEGORY(LogRpgInventoryManager);

namespace
{
	constexpr int32 MaxRecentMutationResults = 64;

	const URpgInventoryFragment_ItemTraits* GetItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	int32 GetInventoryManagerMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO =
			ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		if (ItemCDO &&
			ItemCDO->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()))
		{
			// One container provider owns one concrete set of item-owned grids. Multiple physical
			// providers can therefore never share one persistent item identity or one stack entry.
			return 1;
		}

		if (const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(ItemDef))
		{
			return Traits->GetMaxStackSize();
		}

		return 1;
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

	ERpgInventoryMutationResultCode GetItemContainerRuleFailureCode(
		const FRpgInventoryItemContainerDefinition& ContainerDefinition,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		uint8 TargetDepth)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition
			? GetDefault<URpgInventoryItemDefinition>(ItemDefinition)
			: nullptr;
		const bool bProvidesContainer = ItemCDO &&
			ItemCDO->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()) != nullptr;
		if (bProvidesContainer && !ContainerDefinition.bAllowNestedContainers)
		{
			return ERpgInventoryMutationResultCode::NestedContainersNotAllowed;
		}
		if (bProvidesContainer &&
			TargetDepth >= ContainerDefinition.GetEffectiveMaxNestingDepth())
		{
			return ERpgInventoryMutationResultCode::MaxDepthExceeded;
		}
		return ERpgInventoryMutationResultCode::ItemNotAllowed;
	}

	FRpgInventoryMutationResult MakeRejectedIntentResult(
		const FGuid& RequestId,
		ERpgInventoryMutationOperation Operation,
		int32 RequestedQuantity)
	{
		FRpgInventoryMutationResult Result;
		Result.RequestId = RequestId;
		Result.Operation = Operation;
		Result.RequestedQuantity = RequestedQuantity;
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Result;
	}

	bool IsCompleteSourceSnapshot(
		const FRpgInventoryItemId& ItemId,
		const FGuid& ExpectedEntryId,
		const FRpgInventoryGridPlacement& ExpectedSourcePlacement)
	{
		return ItemId.IsValid() &&
			ExpectedEntryId.IsValid() &&
			ExpectedSourcePlacement.ContainerHandle.IsValid() &&
			ExpectedSourcePlacement.IsValid();
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

	bool FindManagedInventoryEntry(
		const URpgInventoryItemInstance* ItemInstance,
		const URpgInventoryManagerComponent*& OutInventory,
		FRpgInventoryEntryView& OutEntry)
	{
		OutInventory = nullptr;
		OutEntry = FRpgInventoryEntryView();
		const AActor* ItemOwner = ItemInstance
			? ItemInstance->GetTypedOuter<AActor>()
			: nullptr;
		if (!ItemOwner)
		{
			return false;
		}

		TArray<URpgInventoryManagerComponent*> Inventories;
		ItemOwner->GetComponents(Inventories);
		for (const URpgInventoryManagerComponent* Inventory : Inventories)
		{
			if (!Inventory)
			{
				continue;
			}
			for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
			{
				if (Entry.Instance == ItemInstance)
				{
					OutInventory = Inventory;
					OutEntry = Entry;
					return true;
				}
			}
		}
		return false;
	}

}

FRpgInventoryPlacementSubject FRpgInventoryPlacementSubject::FromOwnedEntry(
	const URpgInventoryManagerComponent* SourceInventory,
	const FRpgInventoryEntryView& Entry,
	int32 Quantity)
{
	FRpgInventoryPlacementSubject Subject;
	Subject.Kind = ERpgInventoryPlacementSubjectKind::OwnedEntry;
	Subject.SourceInventory = SourceInventory;
	Subject.ItemInstance = Entry.Instance;
	Subject.ItemDefinition = Entry.Instance ? Entry.Instance->GetItemDef() : nullptr;
	Subject.ItemId = Entry.ItemId;
	Subject.ExpectedEntryId = Entry.EntryId;
	Subject.ExpectedSourcePlacement = Entry.Placement;
	Subject.ExpectedSourceQuantity = Entry.StackCount;
	Subject.Quantity = Quantity > 0 ? Quantity : Entry.StackCount;
	return Subject;
}

FRpgInventoryPlacementSubject FRpgInventoryPlacementSubject::FromIncomingInstance(
	const URpgInventoryManagerComponent* SourceInventory,
	const FRpgInventoryEntryView& Entry,
	int32 Quantity)
{
	FRpgInventoryPlacementSubject Subject = FromOwnedEntry(SourceInventory, Entry, Quantity);
	Subject.Kind = ERpgInventoryPlacementSubjectKind::IncomingEntry;
	return Subject;
}

FRpgInventoryPlacementSubject FRpgInventoryPlacementSubject::FromDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 Quantity)
{
	FRpgInventoryPlacementSubject Subject;
	Subject.Kind = ERpgInventoryPlacementSubjectKind::DefinitionGrant;
	Subject.ItemDefinition = ItemDefinition;
	Subject.Quantity = Quantity;
	return Subject;
}

FRpgInventoryPlacementSubject FRpgInventoryPlacementSubject::FromGeneratedGrant(
	const URpgInventoryItemInstance* ItemInstance,
	int32 Quantity)
{
	FRpgInventoryPlacementSubject Subject = FromDetachedInstance(ItemInstance, Quantity);
	Subject.Kind = ERpgInventoryPlacementSubjectKind::GeneratedGrant;
	return Subject;
}

FRpgInventoryPlacementSubject FRpgInventoryPlacementSubject::FromDetachedInstance(
	const URpgInventoryItemInstance* ItemInstance,
	int32 Quantity)
{
	FRpgInventoryPlacementSubject Subject;
	Subject.Kind = ERpgInventoryPlacementSubjectKind::DetachedInstance;
	Subject.ItemInstance = ItemInstance;
	Subject.ItemDefinition = ItemInstance ? ItemInstance->GetItemDef() : nullptr;
	Subject.ItemId = ItemInstance ? ItemInstance->GetItemId() : FRpgInventoryItemId();
	Subject.Quantity = Quantity;
	return Subject;
}

FRpgInventoryPlacementSubject FRpgInventoryPlacementSubject::FromStagedRestore(
	const URpgInventoryItemInstance* ItemInstance,
	FRpgInventoryItemId ItemId,
	int32 Quantity)
{
	FRpgInventoryPlacementSubject Subject = FromDetachedInstance(ItemInstance, Quantity);
	Subject.Kind = ERpgInventoryPlacementSubjectKind::StagedRestore;
	Subject.ItemId = ItemId;
	return Subject;
}

//////////////////////////////////////////////////////////////////////
// FRpgInventoryEntry

int32 URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef)
{
	return GetInventoryManagerMaxStackSizeForDefinition(ItemDef);
}

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
	if (!ItemDef || StackCount <= 0)
	{
		return 0;
	}

	URpgInventoryItemInstance* StagedInstance =
		NewObject<URpgInventoryItemInstance>(GetTransientPackage());
	StagedInstance->SetItemDef(ItemDef);
	for (URpgInventoryItemFragment* Fragment :
		GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment)
		{
			Fragment->OnInstanceCreated(StagedInstance);
		}
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	Query.Subject = FRpgInventoryPlacementSubject::FromGeneratedGrant(
		StagedInstance,
		StackCount);
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	int32 MergeQuantity = 0;
	for (const FRpgInventoryPlacementStep& Step : Plan.Steps)
	{
		if (Step.Resolution == ERpgInventoryPlacementResolution::Merge)
		{
			MergeQuantity += Step.Quantity;
		}
	}
	const int32 RemainingQuantity = FMath::Max(0, StackCount - MergeQuantity);
	return RemainingQuantity > 0
		? FMath::DivideAndRoundUp(
			RemainingQuantity,
			GetInventoryManagerMaxStackSizeForDefinition(ItemDef))
		: 0;
}

int32 URpgInventoryManagerComponent::GetRequiredNewEntryCountForItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount) const
{
	if (!ItemInstance || StackCount <= 0)
	{
		return 0;
	}

	return ItemInstance && StackCount > 0 ? 1 : 0;
}

bool URpgInventoryManagerComponent::TryNormalizePlacementForDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef,
	const FRpgInventoryContainerHandle& ContainerHandle,
	int32 X,
	int32 Y,
	bool bRotated,
	FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!ItemDef || !ContainerHandle.IsValid())
	{
		return false;
	}

	const FRpgInventoryGridSize Footprint =
		GetInventoryManagerFootprintForDefinition(ItemDef, false);
	if (!Footprint.IsValid())
	{
		return false;
	}

	OutPlacement.SetContainerHandle(ContainerHandle);
	OutPlacement.X = X;
	OutPlacement.Y = Y;

	// Gear/Carry single-cell semantics are a property of a root layout group. An item-owned
	// container may legitimately reuse the same local name and must retain the item's real footprint.
	if (ShouldUseSingleCellPlacementForContainer(ContainerHandle))
	{
		OutPlacement.Width = 1;
		OutPlacement.Height = 1;
		OutPlacement.bRotated = false;
		return true;
	}

	if (bRotated && !CanInventoryManagerRotateDefinition(ItemDef))
	{
		return false;
	}

	OutPlacement.Width = Footprint.Width;
	OutPlacement.Height = Footprint.Height;
	OutPlacement.bRotated = bRotated;
	return true;
}

FRpgInventoryPlacementPlan URpgInventoryManagerComponent::EvaluatePlacement(
	const FRpgInventoryPlacementQuery& Query) const
{
	return EvaluatePlacementInternal(Query, nullptr, nullptr);
}

FRpgInventoryPlacementPlan URpgInventoryManagerComponent::EvaluatePlacementInternal(
	const FRpgInventoryPlacementQuery& Query,
	const FRpgInventoryGridSize* StagedRestoreGridSize,
	const TArray<FRpgInventoryGridPlacement>* StagedRestoreOccupancy) const
{
	FRpgInventoryPlacementPlan Plan;
	Plan.TargetRevision = InventoryRevision;
	Plan.RequestedQuantity = Query.Subject.Quantity;
	const bool bUsesStagedRestoreScratch =
		Query.Purpose == ERpgInventoryPlacementPurpose::Restore &&
		StagedRestoreGridSize != nullptr &&
		StagedRestoreOccupancy != nullptr;
	if ((StagedRestoreGridSize == nullptr) !=
			(StagedRestoreOccupancy == nullptr) ||
		((StagedRestoreGridSize || StagedRestoreOccupancy) &&
			Query.Purpose != ERpgInventoryPlacementPurpose::Restore))
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Plan;
	}

	const FRpgInventoryPlacementSubject& Subject = Query.Subject;
	if (Subject.Kind == ERpgInventoryPlacementSubjectKind::Invalid ||
		!Subject.ItemDefinition || Subject.Quantity <= 0 ||
		(Subject.ItemInstance && Subject.ItemInstance->GetItemDef() != Subject.ItemDefinition))
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Plan;
	}

	const bool bOwnedPurpose =
		Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
		Query.Purpose == ERpgInventoryPlacementPurpose::Equip ||
		Query.Purpose == ERpgInventoryPlacementPurpose::Split ||
		Query.Purpose == ERpgInventoryPlacementPurpose::Transfer;
	const FRpgInventoryEntry* SourceEntry = nullptr;
	const bool bKindMatchesPurpose =
		((Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
		  Query.Purpose == ERpgInventoryPlacementPurpose::Equip ||
		  Query.Purpose == ERpgInventoryPlacementPurpose::Split) &&
		 Subject.Kind == ERpgInventoryPlacementSubjectKind::OwnedEntry) ||
		(Query.Purpose == ERpgInventoryPlacementPurpose::Transfer &&
		 Subject.Kind == ERpgInventoryPlacementSubjectKind::IncomingEntry) ||
		(Query.Purpose == ERpgInventoryPlacementPurpose::Add &&
		 (Subject.Kind == ERpgInventoryPlacementSubjectKind::DefinitionGrant ||
		  Subject.Kind == ERpgInventoryPlacementSubjectKind::GeneratedGrant ||
		  Subject.Kind == ERpgInventoryPlacementSubjectKind::DetachedInstance)) ||
		(Query.Purpose == ERpgInventoryPlacementPurpose::Restore &&
		 Subject.Kind == ERpgInventoryPlacementSubjectKind::StagedRestore);
	if (!bKindMatchesPurpose)
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Plan;
	}
	if ((Subject.Kind == ERpgInventoryPlacementSubjectKind::GeneratedGrant ||
		 Subject.Kind == ERpgInventoryPlacementSubjectKind::DetachedInstance ||
		 Subject.Kind == ERpgInventoryPlacementSubjectKind::StagedRestore) &&
		(!Subject.ItemInstance || !Subject.ItemId.IsValid() ||
		 Subject.ItemInstance->GetItemId() != Subject.ItemId))
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Plan;
	}
	if (bOwnedPurpose)
	{
		if (!Subject.SourceInventory || !Subject.ItemInstance ||
			!IsCompleteSourceSnapshot(
				Subject.ItemId,
				Subject.ExpectedEntryId,
				Subject.ExpectedSourcePlacement) ||
			Subject.ExpectedSourceQuantity <= 0)
		{
			Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return Plan;
		}

		Plan.SourceRevision = Subject.SourceInventory->InventoryRevision;
		SourceEntry = Subject.SourceInventory->InventoryList.FindEntryByItemId(
			Subject.ItemId);
		if (!SourceEntry || !SourceEntry->Instance ||
			SourceEntry->Instance != Subject.ItemInstance ||
			SourceEntry->EntryId != Subject.ExpectedEntryId ||
			!ArePlacementSnapshotsExactlyEqual(
				SourceEntry->Placement,
				Subject.ExpectedSourcePlacement) ||
			SourceEntry->StackCount != Subject.ExpectedSourceQuantity)
		{
			Plan.Code = ERpgInventoryMutationResultCode::SourceMismatch;
			return Plan;
		}

		if (Subject.Quantity > Subject.ExpectedSourceQuantity)
		{
			Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return Plan;
		}
		if ((Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
			 Query.Purpose == ERpgInventoryPlacementPurpose::Equip) &&
			Subject.Quantity != Subject.ExpectedSourceQuantity)
		{
			Plan.Code = ERpgInventoryMutationResultCode::SourceMismatch;
			return Plan;
		}
		if ((Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
			 Query.Purpose == ERpgInventoryPlacementPurpose::Equip ||
			 Query.Purpose == ERpgInventoryPlacementPurpose::Split) &&
			Subject.SourceInventory != this)
		{
			Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return Plan;
		}
		if (Query.Purpose == ERpgInventoryPlacementPurpose::Transfer &&
			Subject.SourceInventory == this)
		{
			Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return Plan;
		}
	}
	else if (Subject.SourceInventory)
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Plan;
	}

	const FRpgInventoryGridSize DefinitionFootprint =
		GetInventoryManagerFootprintForDefinition(
			Subject.ItemDefinition,
			false);
	if (!DefinitionFootprint.IsValid())
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidPlacement;
		return Plan;
	}
	const bool bDefinitionAllowsRotation =
		CanInventoryManagerRotateDefinition(Subject.ItemDefinition);

	const int32 MaxStackSize =
		GetInventoryManagerMaxStackSizeForDefinition(Subject.ItemDefinition);
	const bool bMayFanOutGeneratedGrant =
		Query.Purpose == ERpgInventoryPlacementPurpose::Add &&
		(Subject.Kind == ERpgInventoryPlacementSubjectKind::DefinitionGrant ||
		 Subject.Kind == ERpgInventoryPlacementSubjectKind::GeneratedGrant);
	if (MaxStackSize <= 0 ||
		(!bMayFanOutGeneratedGrant && Subject.Quantity > MaxStackSize))
	{
		Plan.Code = ERpgInventoryMutationResultCode::StackLimitReached;
		return Plan;
	}
	if (Query.Purpose == ERpgInventoryPlacementPurpose::Split &&
		(!SourceEntry || Subject.Quantity >= SourceEntry->StackCount ||
		 MaxStackSize <= 1 ||
		 Subject.ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()))
	{
		Plan.Code = ERpgInventoryMutationResultCode::StackLimitReached;
		return Plan;
	}

	if (Query.Purpose == ERpgInventoryPlacementPurpose::Add &&
		Subject.ItemId.IsValid() && FindItemById(Subject.ItemId))
	{
		Plan.Code = ERpgInventoryMutationResultCode::DuplicateItemId;
		return Plan;
	}

	TArray<const FRpgInventoryEntry*> TransferSubtree;
	uint8 DeepestRelativeDepth = 0;
	if (Query.Purpose == ERpgInventoryPlacementPurpose::Transfer)
	{
		if (FindItemById(Subject.ItemId))
		{
			Plan.Code = ERpgInventoryMutationResultCode::DuplicateItemId;
			return Plan;
		}

		TransferSubtree.Add(SourceEntry);
		const uint8 SourceDepth = SourceEntry->Placement.GetContainerHandle().Depth;
		for (const FRpgInventoryEntry& Candidate : Subject.SourceInventory->InventoryList.Entries)
		{
			if (&Candidate == SourceEntry || !Candidate.Instance)
			{
				continue;
			}

			FRpgInventoryContainerHandle Handle = Candidate.Placement.GetContainerHandle();
			for (int32 Guard = 0;
				 Guard <= Subject.SourceInventory->InventoryList.Entries.Num() && Handle.IsItemOwned();
				 ++Guard)
			{
				if (Handle.ItemOwnerId == Subject.ItemId)
				{
					TransferSubtree.Add(&Candidate);
					DeepestRelativeDepth = FMath::Max<uint8>(
						DeepestRelativeDepth,
						Candidate.Placement.GetContainerHandle().Depth > SourceDepth
							? Candidate.Placement.GetContainerHandle().Depth - SourceDepth
							: 1);
					break;
				}
				const FRpgInventoryEntry* Parent =
					Subject.SourceInventory->InventoryList.FindEntryByItemId(Handle.ItemOwnerId);
				Handle = Parent
					? Parent->Placement.GetContainerHandle()
					: FRpgInventoryContainerHandle();
			}
		}

		const bool bHasContainerContract =
			Subject.ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;
		if ((bHasContainerContract || TransferSubtree.Num() > 1) &&
			Subject.Quantity != Subject.ExpectedSourceQuantity)
		{
			Plan.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return Plan;
		}

		for (const FRpgInventoryEntry* Incoming : TransferSubtree)
		{
			if (Incoming && Incoming->Instance &&
				FindItemById(Incoming->Instance->GetItemId()))
			{
				Plan.Code = ERpgInventoryMutationResultCode::DuplicateItemId;
				return Plan;
			}
		}
	}

	TArray<FRpgInventoryContainerHandle> SearchContainers;
	if (Query.TargetContainer.IsValid())
	{
		SearchContainers.Add(Query.TargetContainer);
	}
	else if (Query.Search == ERpgInventoryPlacementSearch::FirstFit)
	{
		if (const URpgPlayerInventoryLayoutComponent* Layout = FindOwningPlayerInventoryLayout())
		{
			for (const FRpgInventorySlotGroupView& Group : Layout->GetSlotGroups())
			{
				if (Group.GroupKind == ERpgInventorySlotGroupKind::Content &&
					Group.Rule.AllowsItemDefinition(Subject.ItemDefinition))
				{
					SearchContainers.AddUnique(Group.ContainerHandle);
				}
			}
		}
		else if (!DefaultContainerId.IsNone())
		{
			SearchContainers.Add(FRpgInventoryContainerHandle::MakeRoot(DefaultContainerId));
		}
	}

	const bool bPurposeRequiresExact =
		Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
		Query.Purpose == ERpgInventoryPlacementPurpose::Equip ||
		Query.Purpose == ERpgInventoryPlacementPurpose::Split ||
		Query.Purpose == ERpgInventoryPlacementPurpose::Restore;
	if ((bPurposeRequiresExact && Query.Search != ERpgInventoryPlacementSearch::Exact) ||
		SearchContainers.IsEmpty() ||
		(Query.Search == ERpgInventoryPlacementSearch::Exact &&
		 (!Query.TargetContainer.IsValid() || !Query.ExactPlacement.IsValid())))
	{
		Plan.Code = Query.Search == ERpgInventoryPlacementSearch::Exact
			? ERpgInventoryMutationResultCode::InvalidPlacement
			: ERpgInventoryMutationResultCode::InvalidContainer;
		return Plan;
	}
	if (Query.Search == ERpgInventoryPlacementSearch::Exact &&
		(!Query.ExactPlacement.ContainerHandle.IsValid() ||
		 Query.ExactPlacement.ContainerHandle != Query.TargetContainer))
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidContainer;
		return Plan;
	}

	auto FindContainerRank = [&SearchContainers](const FRpgInventoryContainerHandle& Handle)
	{
		return SearchContainers.IndexOfByKey(Handle);
	};

	auto ValidateNormalizedPlacement =
		[this,
		 &Query,
		 &Subject,
		 SourceEntry,
		 DeepestRelativeDepth,
		 bUsesStagedRestoreScratch,
		 StagedRestoreGridSize,
		 StagedRestoreOccupancy](
			const FRpgInventoryGridPlacement& Placement,
			ERpgInventoryMutationResultCode& OutCode)
	{
		OutCode = ERpgInventoryMutationResultCode::Success;
		FRpgInventoryGridSize GridSize = bUsesStagedRestoreScratch
			? *StagedRestoreGridSize
			: FRpgInventoryGridSize();
		if ((!bUsesStagedRestoreScratch &&
			 !GetGridSizeForContainerHandle(
				 Placement.GetContainerHandle(),
				 GridSize)) ||
			!GridSize.IsValid())
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}

		if (Placement.GetContainerHandle().IsItemOwned() &&
			!bUsesStagedRestoreScratch)
		{
			FRpgInventoryItemContainerDefinition Definition;
			if (!GetItemContainerDefinition(Placement.GetContainerHandle(), Definition))
			{
				OutCode = Placement.GetContainerHandle().Depth > RpgInventoryMaxItemOwnedDepth
					? ERpgInventoryMutationResultCode::MaxDepthExceeded
					: ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}
			if (!Definition.AllowsItemDefinition(
				Subject.ItemDefinition,
				Placement.GetContainerHandle().Depth))
			{
				OutCode = GetItemContainerRuleFailureCode(
					Definition,
					Subject.ItemDefinition,
					Placement.GetContainerHandle().Depth);
				return false;
			}
		}

		FRpgInventoryEntry ProposedEntry;
		ProposedEntry.Instance = const_cast<URpgInventoryItemInstance*>(Subject.ItemInstance);
		ProposedEntry.EntryId = Subject.ExpectedEntryId;
		ProposedEntry.StackCount = Subject.ExpectedSourceQuantity;
		ProposedEntry.Placement = SourceEntry
			? Subject.ExpectedSourcePlacement
			: Placement;
		if (Subject.ItemInstance && !bUsesStagedRestoreScratch)
		{
			if (!ValidatePlacementGraphRules(ProposedEntry, Placement, OutCode))
			{
				return false;
			}
		}

		if (Placement.GetContainerHandle().Depth + DeepestRelativeDepth >
			RpgInventoryMaxItemOwnedDepth)
		{
			OutCode = ERpgInventoryMutationResultCode::MaxDepthExceeded;
			return false;
		}

		// Moving a provider rebases every descendant handle. Re-evaluate the owning
		// container's designer depth contract for every child, not only the global cap.
		if (SourceEntry && Subject.SourceInventory)
		{
			const int32 DepthDelta =
				static_cast<int32>(Placement.GetContainerHandle().Depth) -
				static_cast<int32>(SourceEntry->Placement.GetContainerHandle().Depth);
			for (const FRpgInventoryEntry& Candidate :
				Subject.SourceInventory->InventoryList.Entries)
			{
				if (&Candidate == SourceEntry || !Candidate.Instance ||
					!Candidate.Placement.GetContainerHandle().IsItemOwned())
				{
					continue;
				}

				FRpgInventoryContainerHandle AncestorHandle =
					Candidate.Placement.GetContainerHandle();
				bool bDescendant = false;
				for (int32 Guard = 0;
					 Guard <= Subject.SourceInventory->InventoryList.Entries.Num() &&
						 AncestorHandle.IsItemOwned();
					 ++Guard)
				{
					if (AncestorHandle.ItemOwnerId == Subject.ItemId)
					{
						bDescendant = true;
						break;
					}
					const FRpgInventoryEntry* Parent =
						Subject.SourceInventory->InventoryList.FindEntryByItemId(
							AncestorHandle.ItemOwnerId);
					AncestorHandle = Parent
						? Parent->Placement.GetContainerHandle()
						: FRpgInventoryContainerHandle();
				}
				if (!bDescendant)
				{
					continue;
				}

				const int32 NewDepth =
					static_cast<int32>(Candidate.Placement.GetContainerHandle().Depth) +
					DepthDelta;
				if (NewDepth <= 0 || NewDepth > RpgInventoryMaxItemOwnedDepth)
				{
					OutCode = ERpgInventoryMutationResultCode::MaxDepthExceeded;
					return false;
				}

				FRpgInventoryItemContainerDefinition Definition;
				if (!Subject.SourceInventory->GetItemContainerDefinition(
						Candidate.Placement.GetContainerHandle(),
						Definition))
				{
					OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
					return false;
				}
				if (!Definition.AllowsItemDefinition(
					Candidate.Instance->GetItemDef(),
					static_cast<uint8>(NewDepth)))
				{
					OutCode = GetItemContainerRuleFailureCode(
						Definition,
						Candidate.Instance->GetItemDef(),
						static_cast<uint8>(NewDepth));
					return false;
				}
			}
		}

		if (Placement.GetContainerHandle().IsRoot())
		{
			if (const URpgPlayerInventoryLayoutComponent* Layout = FindOwningPlayerInventoryLayout())
			{
				const TArray<FRpgInventorySlotGroupView> Groups =
					Layout->GetSlotGroups();
				const FRpgInventorySlotGroupView* Group = Groups.FindByPredicate(
					[&Placement](const FRpgInventorySlotGroupView& Candidate)
					{
						return Candidate.ContainerHandle == Placement.GetContainerHandle() &&
							Candidate.ContainsCell(Placement.X, Placement.Y);
					});
				if (!Group ||
					(Query.Purpose == ERpgInventoryPlacementPurpose::Add &&
					 Group->GroupKind != ERpgInventorySlotGroupKind::Content))
				{
					OutCode = ERpgInventoryMutationResultCode::ItemNotAllowed;
					return false;
				}
				FRpgInventorySlotAddress Address;
				Address.SetContainerHandle(Placement.GetContainerHandle());
				Address.X = Placement.X;
				Address.Y = Placement.Y;
				if (Subject.ItemInstance)
				{
					if (!Layout->CanItemUseSlotAddress(
						const_cast<URpgInventoryItemInstance*>(Subject.ItemInstance),
						Address))
					{
						OutCode = ERpgInventoryMutationResultCode::ItemNotAllowed;
						return false;
					}
				}
				else
				{
					if (!Group || Group->GroupKind != ERpgInventorySlotGroupKind::Content ||
						!Group->Rule.AllowsItemDefinition(Subject.ItemDefinition))
					{
						OutCode = ERpgInventoryMutationResultCode::ItemNotAllowed;
						return false;
					}
				}
			}
		}

		const TArray<FRpgInventoryGridPlacement> EmptyOccupancy;
		const TArray<FRpgInventoryGridPlacement>& ValidationOccupancy =
			bUsesStagedRestoreScratch
				? *StagedRestoreOccupancy
				: EmptyOccupancy;
		OutCode = EvaluateScratchPlacement(
			Placement,
			GridSize,
			ValidationOccupancy);
		return OutCode == ERpgInventoryMutationResultCode::Success;
	};

	auto ValidateRebasedDescendantRules = [](
		const URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryEntry& RootEntry,
		const FRpgInventoryGridPlacement& NewRootPlacement,
		ERpgInventoryMutationResultCode& OutCode)
	{
		if (!SourceInventory || !RootEntry.Instance)
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		const FRpgInventoryItemId RootItemId = RootEntry.Instance->GetItemId();
		const int32 DepthDelta =
			static_cast<int32>(NewRootPlacement.GetContainerHandle().Depth) -
			static_cast<int32>(RootEntry.Placement.GetContainerHandle().Depth);
		for (const FRpgInventoryEntry& Candidate : SourceInventory->InventoryList.Entries)
		{
			if (&Candidate == &RootEntry || !Candidate.Instance ||
				!Candidate.Placement.GetContainerHandle().IsItemOwned())
			{
				continue;
			}

			FRpgInventoryContainerHandle Handle =
				Candidate.Placement.GetContainerHandle();
			bool bDescendant = false;
			for (int32 Guard = 0;
				 Guard <= SourceInventory->InventoryList.Entries.Num() && Handle.IsItemOwned();
				 ++Guard)
			{
				if (Handle.ItemOwnerId == RootItemId)
				{
					bDescendant = true;
					break;
				}
				const FRpgInventoryEntry* Parent =
					SourceInventory->InventoryList.FindEntryByItemId(Handle.ItemOwnerId);
				Handle = Parent
					? Parent->Placement.GetContainerHandle()
					: FRpgInventoryContainerHandle();
			}
			if (!bDescendant)
			{
				continue;
			}

			const int32 NewDepth =
				static_cast<int32>(Candidate.Placement.GetContainerHandle().Depth) +
				DepthDelta;
			if (NewDepth <= 0 || NewDepth > RpgInventoryMaxItemOwnedDepth)
			{
				OutCode = ERpgInventoryMutationResultCode::MaxDepthExceeded;
				return false;
			}
			FRpgInventoryItemContainerDefinition Definition;
			if (!SourceInventory->GetItemContainerDefinition(
					Candidate.Placement.GetContainerHandle(),
					Definition))
			{
				OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}
			if (!Definition.AllowsItemDefinition(
				Candidate.Instance->GetItemDef(),
				static_cast<uint8>(NewDepth)))
			{
				OutCode = Candidate.Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()
					? ERpgInventoryMutationResultCode::NestedContainersNotAllowed
					: ERpgInventoryMutationResultCode::ItemNotAllowed;
				return false;
			}
		}
		OutCode = ERpgInventoryMutationResultCode::Success;
		return true;
	};

	TArray<FRpgInventoryGridPlacement> ScratchOccupancy;
	if (bUsesStagedRestoreScratch)
	{
		ScratchOccupancy = *StagedRestoreOccupancy;
	}
	ScratchOccupancy.Reserve(
		ScratchOccupancy.Num() + InventoryList.Entries.Num() + 4);
	if (!bUsesStagedRestoreScratch)
	{
		for (const FRpgInventoryEntry& Existing : InventoryList.Entries)
		{
			const bool bIgnoreMovingEntry =
				SourceEntry && Subject.SourceInventory == this &&
				(&Existing == SourceEntry) &&
				(Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
				 Query.Purpose == ERpgInventoryPlacementPurpose::Equip);
			if (!bIgnoreMovingEntry && Existing.Instance && Existing.StackCount > 0)
			{
				ScratchOccupancy.Add(Existing.Placement);
			}
		}
	}

	auto MakeMergeStep = [&Plan](const FRpgInventoryEntry& Target, int32 Quantity)
	{
		FRpgInventoryPlacementStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.Resolution = ERpgInventoryPlacementResolution::Merge;
		Step.Placement = Target.Placement;
		Step.Quantity = Quantity;
		Step.TargetItemId = Target.Instance->GetItemId();
		Step.TargetEntryId = Target.EntryId;
	};

	auto HasEntryCapacity = [this](int32 NewEntryCount)
	{
		return NewEntryCount <= 0 || IsCapacityUnlimited() ||
			GetFreeEntryCount() >= NewEntryCount;
	};

	int32 RemainingQuantity = Subject.Quantity;
	int32 PlannedNewEntries = 0;
	const bool bCanMerge =
		(Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
		 (Query.Purpose == ERpgInventoryPlacementPurpose::Add &&
		  Subject.Kind == ERpgInventoryPlacementSubjectKind::GeneratedGrant) ||
		 Query.Purpose == ERpgInventoryPlacementPurpose::Transfer) &&
		Subject.ItemInstance &&
		!Subject.ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>();

	if (Query.Search == ERpgInventoryPlacementSearch::FirstFit && bCanMerge)
	{
		TArray<const FRpgInventoryEntry*> StableMergeTargets;
		for (const FRpgInventoryEntry& Existing : InventoryList.Entries)
		{
			if (!Existing.Instance || Existing.StackCount <= 0 ||
				FindContainerRank(Existing.Placement.GetContainerHandle()) == INDEX_NONE ||
				!Subject.ItemInstance->IsStackCompatibleWith(Existing.Instance) ||
				InventoryList.GetFreeStackCapacity(Existing.Instance) <= 0)
			{
				continue;
			}
			StableMergeTargets.Add(&Existing);
		}
		StableMergeTargets.Sort(
			[&FindContainerRank](const FRpgInventoryEntry& A, const FRpgInventoryEntry& B)
			{
				const int32 RankA = FindContainerRank(A.Placement.GetContainerHandle());
				const int32 RankB = FindContainerRank(B.Placement.GetContainerHandle());
				if (RankA != RankB)
				{
					return RankA < RankB;
				}
				if (A.Placement.Y != B.Placement.Y)
				{
					return A.Placement.Y < B.Placement.Y;
				}
				if (A.Placement.X != B.Placement.X)
				{
					return A.Placement.X < B.Placement.X;
				}
				return A.EntryId.ToString() < B.EntryId.ToString();
			});

		for (const FRpgInventoryEntry* Target : StableMergeTargets)
		{
			if (RemainingQuantity <= 0)
			{
				break;
			}
			const int32 MergeQuantity = FMath::Min(
				RemainingQuantity,
				InventoryList.GetFreeStackCapacity(Target->Instance));
			if (MergeQuantity > 0)
			{
				MakeMergeStep(*Target, MergeQuantity);
				RemainingQuantity -= MergeQuantity;
				Plan.AppliedQuantity += MergeQuantity;
			}
		}
	}

	if (Query.Search == ERpgInventoryPlacementSearch::Exact)
	{
		FRpgInventoryGridPlacement Candidate;
		if (!TryNormalizePlacementForDefinition(
			Subject.ItemDefinition,
			Query.TargetContainer,
			Query.ExactPlacement.X,
			Query.ExactPlacement.Y,
			Query.ExactPlacement.bRotated,
			Candidate))
		{
			Plan.Code = ERpgInventoryMutationResultCode::InvalidPlacement;
			return Plan;
		}
		if (Query.Purpose == ERpgInventoryPlacementPurpose::Restore &&
			!ArePlacementSnapshotsExactlyEqual(
				Candidate,
				Query.ExactPlacement))
		{
			Plan.Code = ERpgInventoryMutationResultCode::InvalidPlacement;
			return Plan;
		}

		ERpgInventoryMutationResultCode PlacementCode;
		if (!ValidateNormalizedPlacement(Candidate, PlacementCode))
		{
			Plan.Code = PlacementCode;
			return Plan;
		}

		if (SourceEntry && Subject.SourceInventory == this &&
			(Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
			 Query.Purpose == ERpgInventoryPlacementPurpose::Equip) &&
			Candidate == SourceEntry->Placement)
		{
			FRpgInventoryPlacementStep& Step = Plan.Steps.AddDefaulted_GetRef();
			Step.Resolution = ERpgInventoryPlacementResolution::NoOp;
			Step.Placement = Candidate;
			Step.Quantity = Subject.Quantity;
			Step.TargetItemId = Subject.ItemId;
			Step.TargetEntryId = Subject.ExpectedEntryId;
			Plan.AppliedQuantity = Subject.Quantity;
			Plan.Code = ERpgInventoryMutationResultCode::Success;
			return Plan;
		}

		TArray<const FRpgInventoryEntry*> Overlaps;
		if (!bUsesStagedRestoreScratch)
		{
			InventoryList.FindEntriesOverlapping(
				Candidate,
				SourceEntry && Subject.SourceInventory == this &&
					(Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
					 Query.Purpose == ERpgInventoryPlacementPurpose::Equip)
						? SourceEntry
						: nullptr,
				Overlaps);
		}
		if (Overlaps.Num() > 1)
		{
			Plan.Code = ERpgInventoryMutationResultCode::Occupied;
			return Plan;
		}

		const FRpgInventoryEntry* Target = Overlaps.Num() == 1 ? Overlaps[0] : nullptr;
		if (Target)
		{
			if (bCanMerge && Target->Instance &&
				Subject.ItemInstance->IsStackCompatibleWith(Target->Instance) &&
				InventoryList.GetFreeStackCapacity(Target->Instance) > 0)
			{
				const int32 MergeQuantity = FMath::Min(
					RemainingQuantity,
					InventoryList.GetFreeStackCapacity(Target->Instance));
				MakeMergeStep(*Target, MergeQuantity);
				Plan.AppliedQuantity = MergeQuantity;
				Plan.Code = MergeQuantity == Subject.Quantity
					? ERpgInventoryMutationResultCode::Success
					: ERpgInventoryMutationResultCode::PartiallyApplied;
				return Plan;
			}

			if (Query.Purpose == ERpgInventoryPlacementPurpose::Move ||
				Query.Purpose == ERpgInventoryPlacementPurpose::Equip)
			{
				FRpgInventoryGridPlacement DisplacedPlacement;
				if (!SourceEntry ||
					!InventoryList.TryResolveDisplacedEntryPlacement(
						*SourceEntry,
						Candidate,
						*Target,
						DisplacedPlacement))
				{
					Plan.Code = ERpgInventoryMutationResultCode::NoSpace;
					return Plan;
				}
				ERpgInventoryMutationResultCode DisplacedGraphCode;
				if (!ValidateRebasedDescendantRules(
						this,
						*Target,
						DisplacedPlacement,
						DisplacedGraphCode))
				{
					Plan.Code = DisplacedGraphCode;
					return Plan;
				}

				FRpgInventoryPlacementStep& Step = Plan.Steps.AddDefaulted_GetRef();
				Step.Resolution = ERpgInventoryPlacementResolution::Swap;
				Step.Placement = Candidate;
				Step.Quantity = Subject.Quantity;
				Step.TargetItemId = Subject.ItemId;
				Step.TargetEntryId = Subject.ExpectedEntryId;
				Step.DisplacedItemId = Target->Instance->GetItemId();
				Step.DisplacedEntryId = Target->EntryId;
				Step.DisplacedPlacement = DisplacedPlacement;
				Plan.AppliedQuantity = Subject.Quantity;
				Plan.Code = ERpgInventoryMutationResultCode::Success;
				return Plan;
			}

			Plan.Code = bCanMerge
				? ERpgInventoryMutationResultCode::StackIncompatible
				: ERpgInventoryMutationResultCode::Occupied;
			return Plan;
		}

		const int32 NewEntryCost =
			Query.Purpose == ERpgInventoryPlacementPurpose::Split ? 1 :
			Query.Purpose == ERpgInventoryPlacementPurpose::Transfer ? TransferSubtree.Num() :
			Query.Purpose == ERpgInventoryPlacementPurpose::Add ||
			Query.Purpose == ERpgInventoryPlacementPurpose::Restore ? 1 : 0;
		if (!bUsesStagedRestoreScratch && !HasEntryCapacity(NewEntryCost))
		{
			Plan.Code = ERpgInventoryMutationResultCode::NoSpace;
			return Plan;
		}
		if ((Query.Purpose == ERpgInventoryPlacementPurpose::Add ||
			 Query.Purpose == ERpgInventoryPlacementPurpose::Restore) &&
			Subject.Quantity > MaxStackSize)
		{
			Plan.Code = ERpgInventoryMutationResultCode::StackLimitReached;
			return Plan;
		}

		FRpgInventoryPlacementStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.Resolution = ERpgInventoryPlacementResolution::Place;
		Step.Placement = Candidate;
		Step.Quantity = Subject.Quantity;
		Step.TargetItemId = Subject.ItemId;
		Step.TargetEntryId = Subject.ExpectedEntryId;
		Plan.AppliedQuantity = Subject.Quantity;
		Plan.Code = ERpgInventoryMutationResultCode::Success;
		return Plan;
	}

	while (RemainingQuantity > 0)
	{
		const int32 StackQuantity = FMath::Min(MaxStackSize, RemainingQuantity);
		FRpgInventoryGridPlacement Placement;
		bool bFoundPlacement = false;
		ERpgInventoryMutationResultCode LastPlacementCode =
			ERpgInventoryMutationResultCode::NoSpace;
		for (const FRpgInventoryContainerHandle& Container : SearchContainers)
		{
			FRpgInventoryGridSize GridSize;
			FRpgInventoryGridSize Footprint = DefinitionFootprint;
			bool bAllowRotation = bDefinitionAllowsRotation;
			if (ShouldUseSingleCellPlacementForContainer(Container))
			{
				Footprint.Width = 1;
				Footprint.Height = 1;
				bAllowRotation = false;
			}
			if (!GetGridSizeForContainerHandle(Container, GridSize) ||
				!FindFirstFitInScratch(
					Container,
					GridSize,
					Footprint,
					bAllowRotation,
					ScratchOccupancy,
					Placement))
			{
				continue;
			}

			if (ValidateNormalizedPlacement(Placement, LastPlacementCode))
			{
				bFoundPlacement = true;
				break;
			}
		}

		const int32 EntryCost = Query.Purpose == ERpgInventoryPlacementPurpose::Transfer
			? FMath::Max(1, TransferSubtree.Num())
			: 1;
		if (!bFoundPlacement || !HasEntryCapacity(PlannedNewEntries + EntryCost))
		{
			Plan.Code = Plan.AppliedQuantity > 0
				? ERpgInventoryMutationResultCode::PartiallyApplied
				: (bFoundPlacement
					? ERpgInventoryMutationResultCode::NoSpace
					: LastPlacementCode);
			if (Plan.Code == ERpgInventoryMutationResultCode::Success)
			{
				Plan.Code = ERpgInventoryMutationResultCode::NoSpace;
			}
			return Plan;
		}

		FRpgInventoryPlacementStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.Resolution = ERpgInventoryPlacementResolution::Place;
		Step.Placement = Placement;
		Step.Quantity = StackQuantity;
		Step.TargetItemId = Subject.ItemId;
		Step.TargetEntryId = Subject.ExpectedEntryId;
		ScratchOccupancy.Add(Placement);
		PlannedNewEntries += EntryCost;
		RemainingQuantity -= StackQuantity;
		Plan.AppliedQuantity += StackQuantity;
	}

	Plan.Code = Plan.AppliedQuantity == Subject.Quantity
		? ERpgInventoryMutationResultCode::Success
		: ERpgInventoryMutationResultCode::PartiallyApplied;
	return Plan;
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

bool URpgInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const
{
	if (ItemDef == nullptr || StackCount <= 0)
	{
		return false;
	}

	URpgInventoryItemInstance* StagedInstance =
		NewObject<URpgInventoryItemInstance>(GetTransientPackage());
	StagedInstance->SetItemDef(ItemDef);
	for (URpgInventoryItemFragment* Fragment :
		GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment)
		{
			Fragment->OnInstanceCreated(StagedInstance);
		}
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	Query.Subject = FRpgInventoryPlacementSubject::FromGeneratedGrant(
		StagedInstance,
		StackCount);
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	return Plan.Code == ERpgInventoryMutationResultCode::Success &&
		Plan.AppliedQuantity == StackCount;
}

bool URpgInventoryManagerComponent::CanAddItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount) const
{
	if (!InventoryList.CanInsertOwnedInstance(ItemInstance) ||
		IsItemManagedByAnyInventory(ItemInstance) ||
		HasItemIdentityConflictInAnyInventory(ItemInstance))
	{
		return false;
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	Query.Subject = FRpgInventoryPlacementSubject::FromDetachedInstance(
		ItemInstance,
		StackCount);
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	return Plan.Code == ERpgInventoryMutationResultCode::Success &&
		Plan.AppliedQuantity == StackCount;
}

bool URpgInventoryManagerComponent::CanReceiveTransferredItemInstance(
	URpgInventoryItemInstance* ItemInstance,
	int32 StackCount) const
{
	const URpgInventoryManagerComponent* SourceInventory = nullptr;
	FRpgInventoryEntryView SourceEntry;
	if (StackCount <= 0 ||
		!FindManagedInventoryEntry(ItemInstance, SourceInventory, SourceEntry) ||
		SourceInventory == this)
	{
		return false;
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Transfer;
	Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	Query.Subject = FRpgInventoryPlacementSubject::FromIncomingInstance(
		SourceInventory,
		SourceEntry,
		StackCount);
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	return Plan.Code == ERpgInventoryMutationResultCode::Success &&
		Plan.AppliedQuantity == StackCount;
}

bool URpgInventoryManagerComponent::CanAddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement) const
{
	if (!ItemDef || StackCount <= 0)
	{
		return false;
	}

	if (!Placement.ContainerHandle.IsValid())
	{
		return false;
	}

	URpgInventoryItemInstance* StagedInstance =
		NewObject<URpgInventoryItemInstance>(GetTransientPackage());
	StagedInstance->SetItemDef(ItemDef);
	for (URpgInventoryItemFragment* Fragment :
		GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment)
		{
			Fragment->OnInstanceCreated(StagedInstance);
		}
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::Exact;
	Query.Subject = FRpgInventoryPlacementSubject::FromGeneratedGrant(
		StagedInstance,
		StackCount);
	Query.TargetContainer = Placement.ContainerHandle;
	Query.ExactPlacement = Placement;
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	return Plan.Code == ERpgInventoryMutationResultCode::Success &&
		Plan.AppliedQuantity == StackCount;
}

bool URpgInventoryManagerComponent::CanAddItemInstanceToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement) const
{
	if (!InventoryList.CanInsertOwnedInstance(ItemInstance) ||
		IsItemManagedByAnyInventory(ItemInstance) ||
		HasItemIdentityConflictInAnyInventory(ItemInstance))
	{
		return false;
	}
	if (!Placement.ContainerHandle.IsValid())
	{
		return false;
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::Exact;
	Query.Subject = FRpgInventoryPlacementSubject::FromDetachedInstance(
		ItemInstance,
		StackCount);
	Query.TargetContainer = Placement.ContainerHandle;
	Query.ExactPlacement = Placement;
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	return Plan.Code == ERpgInventoryMutationResultCode::Success &&
		Plan.AppliedQuantity == StackCount;
}

bool URpgInventoryManagerComponent::CanReceiveTransferredItemInstanceToPlacement(
	URpgInventoryItemInstance* ItemInstance,
	int32 StackCount,
	FRpgInventoryGridPlacement Placement) const
{
	const URpgInventoryManagerComponent* SourceInventory = nullptr;
	FRpgInventoryEntryView SourceEntry;
	if (StackCount <= 0 ||
		!FindManagedInventoryEntry(ItemInstance, SourceInventory, SourceEntry) ||
		SourceInventory == this)
	{
		return false;
	}
	if (!Placement.ContainerHandle.IsValid())
	{
		return false;
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Transfer;
	Query.Search = ERpgInventoryPlacementSearch::Exact;
	Query.Subject = FRpgInventoryPlacementSubject::FromIncomingInstance(
		SourceInventory,
		SourceEntry,
		StackCount);
	Query.TargetContainer = Placement.ContainerHandle;
	Query.ExactPlacement = Placement;
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	return Plan.Code == ERpgInventoryMutationResultCode::Success &&
		Plan.AppliedQuantity == StackCount && Plan.Steps.Num() == 1 &&
		Plan.Steps[0].Resolution ==
			ERpgInventoryPlacementResolution::Place;
}

bool URpgInventoryManagerComponent::CanReceiveTransferredItemInstanceToPlacementIgnoringItem(
	URpgInventoryItemInstance* ItemInstance,
	int32 StackCount,
	FRpgInventoryGridPlacement Placement,
	URpgInventoryItemInstance* IgnoredItemInstance) const
{
	if (!ItemInstance || !ItemInstance->GetItemDef() || StackCount <= 0)
	{
		return false;
	}

	const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(ItemInstance->GetItemDef());
	if (StackCount > MaxStackSize)
	{
		return false;
	}
	if (!Placement.ContainerHandle.IsValid())
	{
		return false;
	}

	FRpgInventoryGridPlacement NormalizedPlacement;
	if (!TryNormalizePlacementForDefinition(
			ItemInstance->GetItemDef(),
			Placement.ContainerHandle,
			Placement.X,
			Placement.Y,
			Placement.bRotated,
			NormalizedPlacement))
	{
		return false;
	}

	if (!InventoryList.IsPlacementWithinGrid(NormalizedPlacement))
	{
		return false;
	}

	FRpgInventoryEntry ProposedEntry;
	ProposedEntry.Instance = ItemInstance;
	if (const FRpgInventoryEntry* ExistingEntry = InventoryList.FindEntryByInstance(ItemInstance))
	{
		ProposedEntry.Placement = ExistingEntry->Placement;
	}
	ERpgInventoryMutationResultCode GraphRuleCode = ERpgInventoryMutationResultCode::Success;
	if (!ValidatePlacementGraphRules(ProposedEntry, NormalizedPlacement, GraphRuleCode))
	{
		return false;
	}

	if (NormalizedPlacement.GetContainerHandle().IsRoot())
	{
		if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout())
		{
			FRpgInventorySlotAddress Address;
			Address.SetContainerHandle(NormalizedPlacement.GetContainerHandle());
			Address.X = NormalizedPlacement.X;
			Address.Y = NormalizedPlacement.Y;
			if (!InventoryLayout->CanItemUseSlotAddress(ItemInstance, Address))
			{
				return false;
			}
		}
	}

	const FRpgInventoryEntry* IgnoredEntry = InventoryList.FindEntryByInstance(IgnoredItemInstance);
	const bool bReplacingExistingEntry = IgnoredEntry != nullptr;
	return (bReplacingExistingEntry || IsCapacityUnlimited() || GetFreeEntryCount() > 0) &&
		InventoryList.CanPlaceEntryAt(NormalizedPlacement, IgnoredEntry);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::GetSingleItemOverlappingPlacementForItem(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement Placement, FRpgInventoryGridPlacement& OutNormalizedPlacement) const
{
	OutNormalizedPlacement = FRpgInventoryGridPlacement();
	if (!ItemInstance || !Placement.IsValid())
	{
		return nullptr;
	}

	if (!Placement.ContainerHandle.IsValid())
	{
		return nullptr;
	}
	FRpgInventoryGridPlacement NormalizedPlacement;
	if (!TryNormalizePlacementForDefinition(
			ItemInstance->GetItemDef(),
			Placement.ContainerHandle,
			Placement.X,
			Placement.Y,
			Placement.bRotated,
			NormalizedPlacement))
	{
		return nullptr;
	}

	if (!InventoryList.IsPlacementWithinGrid(NormalizedPlacement))
	{
		return nullptr;
	}

	TArray<const FRpgInventoryEntry*> OverlappingEntries;
	InventoryList.FindEntriesOverlapping(NormalizedPlacement, nullptr, OverlappingEntries);
	if (OverlappingEntries.Num() != 1 || !OverlappingEntries[0] || !OverlappingEntries[0]->Instance)
	{
		return nullptr;
	}

	OutNormalizedPlacement = NormalizedPlacement;
	return OverlappingEntries[0]->Instance.Get();
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::CommitAddPlacementPlan(
	URpgInventoryItemInstance* StagedInstance,
	const FRpgInventoryPlacementPlan& Plan,
	bool bMayCreateAdditionalInstances)
{
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority() || !StagedInstance ||
		StagedInstance->GetOuter() != OwningActor ||
		Plan.Code != ERpgInventoryMutationResultCode::Success ||
		Plan.AppliedQuantity != Plan.RequestedQuantity ||
		Plan.TargetRevision != InventoryRevision || Plan.Steps.IsEmpty())
	{
		return nullptr;
	}

	int32 PlacementCount = 0;
	int64 PlannedQuantity = 0;
	TMap<FGuid, int32> PendingMergeQuantity;
	TArray<FRpgInventoryGridPlacement> PlannedPlacements;
	for (const FRpgInventoryPlacementStep& Step : Plan.Steps)
	{
		if (Step.Quantity <= 0)
		{
			return nullptr;
		}
		PlannedQuantity += Step.Quantity;
		if (PlannedQuantity > MAX_int32)
		{
			return nullptr;
		}
		if (Step.Resolution == ERpgInventoryPlacementResolution::Merge)
		{
			FRpgInventoryEntry* Target =
				InventoryList.FindEntryByEntryId(Step.TargetEntryId);
			if (!Target || !Target->Instance ||
				Target->Instance->GetItemId() != Step.TargetItemId ||
				!StagedInstance->IsStackCompatibleWith(Target->Instance))
			{
				return nullptr;
			}
			PendingMergeQuantity.FindOrAdd(Target->EntryId) += Step.Quantity;
			if (PendingMergeQuantity[Target->EntryId] >
				InventoryList.GetFreeStackCapacity(Target->Instance))
			{
				return nullptr;
			}
		}
		else if (Step.Resolution == ERpgInventoryPlacementResolution::Place)
		{
			++PlacementCount;
			if (!Step.Placement.IsValid() ||
				PlannedPlacements.ContainsByPredicate(
					[&Step](const FRpgInventoryGridPlacement& Existing)
					{
						return Existing.Overlaps(Step.Placement);
					}) ||
				!InventoryList.CanPlaceEntryAt(Step.Placement))
			{
				return nullptr;
			}
			PlannedPlacements.Add(Step.Placement);
		}
		else
		{
			return nullptr;
		}
	}
	if (PlannedQuantity != Plan.RequestedQuantity)
	{
		return nullptr;
	}

	if (PlacementCount > 1 && !bMayCreateAdditionalInstances)
	{
		return nullptr;
	}
	if (!IsCapacityUnlimited() && GetFreeEntryCount() < PlacementCount)
	{
		return nullptr;
	}
	if (PlacementCount > 0 &&
		!InventoryList.CanInsertOwnedInstance(StagedInstance))
	{
		return nullptr;
	}

	TArray<URpgInventoryItemInstance*> PlacementInstances;
	TSet<FRpgInventoryItemId> PlacementItemIds;
	PlacementInstances.Reserve(PlacementCount);
	for (int32 Index = 0; Index < PlacementCount; ++Index)
	{
		URpgInventoryItemInstance* Instance = Index == 0
			? StagedInstance
			: NewObject<URpgInventoryItemInstance>(OwningActor);
		if (Index > 0)
		{
			Instance->SetItemDef(StagedInstance->GetItemDef());
			for (URpgInventoryItemFragment* Fragment :
				GetDefault<URpgInventoryItemDefinition>(
					StagedInstance->GetItemDef())->Fragments)
			{
				if (Fragment)
				{
					Fragment->OnInstanceCreated(Instance);
				}
			}
		}
		if (!InventoryList.CanInsertOwnedInstance(Instance) ||
			PlacementItemIds.Contains(Instance->GetItemId()))
		{
			return nullptr;
		}
		PlacementItemIds.Add(Instance->GetItemId());
		PlacementInstances.Add(Instance);
	}

	URpgInventoryItemInstance* ResultInstance = nullptr;
	int32 PlacementIndex = 0;
	struct FMergeNotification
	{
		FGuid EntryId;
		int32 OldCount = 0;
	};
	TArray<FMergeNotification> MergeNotifications;
	TArray<URpgInventoryItemInstance*> AddedInstances;
	for (const FRpgInventoryPlacementStep& Step : Plan.Steps)
	{
		if (Step.Resolution == ERpgInventoryPlacementResolution::Merge)
		{
			FRpgInventoryEntry* Target =
				InventoryList.FindEntryByEntryId(Step.TargetEntryId);
			check(Target && Target->Instance);
			FMergeNotification& Notification =
				MergeNotifications.AddDefaulted_GetRef();
			Notification.EntryId = Target->EntryId;
			Notification.OldCount = Target->StackCount;
			Target->StackCount += Step.Quantity;
			InventoryList.MarkItemDirty(*Target);
			if (!ResultInstance)
			{
				ResultInstance = Target->Instance;
			}
			continue;
		}

		URpgInventoryItemInstance* NewInstance =
			PlacementInstances[PlacementIndex++];
		FRpgInventoryEntry& NewEntry =
			InventoryList.Entries.AddDefaulted_GetRef();
		NewEntry.Instance = NewInstance;
		NewEntry.EntryId = FGuid::NewGuid();
		NewEntry.StackCount = Step.Quantity;
		NewEntry.Placement = Step.Placement;
		InventoryList.MarkItemDirty(NewEntry);
		AddedInstances.Add(NewInstance);
		if (!ResultInstance)
		{
			ResultInstance = NewInstance;
		}
	}

	if (!ResultInstance)
	{
		return nullptr;
	}

	if (!AddedInstances.IsEmpty())
	{
		InventoryList.SortEntriesByPlacement();
		InventoryList.MarkArrayDirty();
	}
	MarkInventoryStateDirty();
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		for (URpgInventoryItemInstance* NewInstance : AddedInstances)
		{
			AddReplicatedSubObject(
				NewInstance,
				ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly
					? COND_OwnerOnly
					: COND_None);
		}
	}

	// Synchronous listeners observe the fully committed graph, never an intermediate
	// state between compatible merges and newly allocated remainder stacks.
	for (const FMergeNotification& Notification : MergeNotifications)
	{
		if (FRpgInventoryEntry* Target =
			InventoryList.FindEntryByEntryId(Notification.EntryId))
		{
			InventoryList.BroadcastChangeMessage(
				*Target,
				Notification.OldCount,
				Target->StackCount);
		}
	}
	for (URpgInventoryItemInstance* AddedInstance : AddedInstances)
	{
		if (FRpgInventoryEntry* AddedEntry =
			InventoryList.FindEntryByInstance(AddedInstance))
		{
			InventoryList.BroadcastChangeMessage(
				*AddedEntry,
				0,
				AddedEntry->StackCount);
		}
	}
	return ResultInstance;
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::GrantItemDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef,
	int32 StackCount)
{
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority() || !ItemDef || StackCount <= 0)
	{
		return nullptr;
	}

	URpgInventoryItemInstance* StagedInstance =
		NewObject<URpgInventoryItemInstance>(OwningActor);
	StagedInstance->SetItemDef(ItemDef);
	for (URpgInventoryItemFragment* Fragment :
		GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment)
		{
			Fragment->OnInstanceCreated(StagedInstance);
		}
	}

	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	Query.Subject = FRpgInventoryPlacementSubject::FromGeneratedGrant(
		StagedInstance,
		StackCount);
	return CommitAddPlacementPlan(
		StagedInstance,
		EvaluatePlacement(Query),
		true);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::BootstrapItemInstance(
	URpgInventoryItemInstance* SourceItemInstance,
	int32 StackCount)
{
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority() ||
		!CanBootstrapItemInstance(SourceItemInstance, StackCount))
	{
		return nullptr;
	}

	if (SourceItemInstance->GetOuter() == OwningActor)
	{
		URpgInventoryItemInstance* ResultInstance = nullptr;
		return AddOwnedItemInstance(
			SourceItemInstance,
			StackCount,
			nullptr,
			&ResultInstance)
			? ResultInstance
			: nullptr;
	}

	URpgInventoryItemInstance* OwnedInstance = NewObject<URpgInventoryItemInstance>(OwningActor);
	OwnedInstance->SetItemDef(SourceItemInstance->GetItemDef());
	const URpgInventoryItemDefinition* ItemDefinition =
		GetDefault<URpgInventoryItemDefinition>(SourceItemInstance->GetItemDef());
	if (!ItemDefinition)
	{
		return nullptr;
	}

	for (URpgInventoryItemFragment* Fragment : ItemDefinition->Fragments)
	{
		if (Fragment)
		{
			Fragment->OnInstanceCreated(OwnedInstance);
		}
	}

	URpgInventoryItemInstance* ResultInstance = nullptr;
	if (!OwnedInstance->CopyRuntimeStateFrom(SourceItemInstance, false) ||
		!AddOwnedItemInstance(
			OwnedInstance,
			StackCount,
			nullptr,
			&ResultInstance))
	{
		return nullptr;
	}

	return ResultInstance;
}

bool URpgInventoryManagerComponent::CanBootstrapItemInstance(
	URpgInventoryItemInstance* SourceItemInstance,
	int32 StackCount) const
{
	if (!SourceItemInstance || !SourceItemInstance->GetItemDef() ||
		!SourceItemInstance->GetItemId().IsValid() || StackCount <= 0 ||
		IsItemManagedByAnyInventory(SourceItemInstance))
	{
		return false;
	}

	const AActor* OwningActor = GetOwner();
	if (!OwningActor)
	{
		return false;
	}

	if (StackCount > GetInventoryManagerMaxStackSizeForDefinition(
			SourceItemInstance->GetItemDef()))
	{
		return false;
	}

	if (SourceItemInstance->GetOuter() == OwningActor)
	{
		if (!InventoryList.CanInsertOwnedInstance(SourceItemInstance) ||
			HasItemIdentityConflictInAnyInventory(SourceItemInstance))
		{
			return false;
		}
	}

	const URpgInventoryItemInstance* PlacementInstance = SourceItemInstance;
	if (SourceItemInstance->GetOuter() != OwningActor)
	{
		URpgInventoryItemInstance* StagedClone =
			NewObject<URpgInventoryItemInstance>(GetTransientPackage());
		StagedClone->SetItemDef(SourceItemInstance->GetItemDef());
		for (URpgInventoryItemFragment* Fragment :
			GetDefault<URpgInventoryItemDefinition>(
				SourceItemInstance->GetItemDef())->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnInstanceCreated(StagedClone);
			}
		}
		if (!StagedClone->CopyRuntimeStateFrom(SourceItemInstance, false))
		{
			return false;
		}
		PlacementInstance = StagedClone;
	}

	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	Query.Subject = FRpgInventoryPlacementSubject::FromDetachedInstance(
		PlacementInstance,
		StackCount);
	const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
	return Plan.Code == ERpgInventoryMutationResultCode::Success &&
		Plan.AppliedQuantity == StackCount;
}

bool URpgInventoryManagerComponent::AddOwnedItemInstance(
	URpgInventoryItemInstance* ItemInstance,
	int32 StackCount,
	const FRpgInventoryGridPlacement* Placement,
	URpgInventoryItemInstance** OutResultInstance)
{
	if (OutResultInstance)
	{
		*OutResultInstance = nullptr;
	}
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority() || !ItemInstance ||
		!InventoryList.CanInsertOwnedInstance(ItemInstance) ||
		IsItemManagedByAnyInventory(ItemInstance) ||
		HasItemIdentityConflictInAnyInventory(ItemInstance))
	{
		return false;
	}

	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Subject = FRpgInventoryPlacementSubject::FromDetachedInstance(
		ItemInstance,
		StackCount);
	if (Placement)
	{
		FRpgInventoryGridPlacement CanonicalPlacement = *Placement;
		if (!CanonicalPlacement.ContainerHandle.IsValid())
		{
			return false;
		}
		Query.Search = ERpgInventoryPlacementSearch::Exact;
		Query.TargetContainer = CanonicalPlacement.ContainerHandle;
		Query.ExactPlacement = CanonicalPlacement;
	}
	else
	{
		Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	}

	URpgInventoryItemInstance* ResultInstance = CommitAddPlacementPlan(
		ItemInstance,
		EvaluatePlacement(Query),
		false);
	if (!ResultInstance)
	{
		return false;
	}
	if (OutResultInstance)
	{
		*OutResultInstance = ResultInstance;
	}
	return true;
}

bool URpgInventoryManagerComponent::IsItemManagedByAnyInventory(
	const URpgInventoryItemInstance* ItemInstance) const
{
	const AActor* ItemOwner = ItemInstance ? ItemInstance->GetTypedOuter<AActor>() : nullptr;
	if (!ItemOwner)
	{
		return false;
	}

	TArray<URpgInventoryManagerComponent*> InventoryComponents;
	ItemOwner->GetComponents(InventoryComponents);
	for (const URpgInventoryManagerComponent* InventoryComponent : InventoryComponents)
	{
		if (InventoryComponent && InventoryComponent->ContainsItemInstance(
			const_cast<URpgInventoryItemInstance*>(ItemInstance)))
		{
			return true;
		}
	}
	return false;
}

bool URpgInventoryManagerComponent::HasItemIdentityConflictInAnyInventory(
	const URpgInventoryItemInstance* ItemInstance) const
{
	const AActor* ItemOwner = ItemInstance ? ItemInstance->GetTypedOuter<AActor>() : nullptr;
	if (!ItemOwner || !ItemInstance->GetItemId().IsValid())
	{
		return false;
	}

	TArray<URpgInventoryManagerComponent*> InventoryComponents;
	ItemOwner->GetComponents(InventoryComponents);
	for (const URpgInventoryManagerComponent* InventoryComponent : InventoryComponents)
	{
		if (!InventoryComponent)
		{
			continue;
		}

		for (const FRpgInventoryEntryView& Entry : InventoryComponent->GetAllEntries())
		{
			if (Entry.Instance && Entry.Instance != ItemInstance &&
				Entry.ItemId == ItemInstance->GetItemId())
			{
				return true;
			}
		}
	}
	return false;
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::AddItemDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef,
	int32 StackCount)
{
	return GrantItemDefinition(ItemDef, StackCount);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::AddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement)
{
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority() || !ItemDef ||
		StackCount <= 0)
	{
		return nullptr;
	}
	if (!Placement.ContainerHandle.IsValid())
	{
		return nullptr;
	}

	URpgInventoryItemInstance* StagedInstance =
		NewObject<URpgInventoryItemInstance>(OwningActor);
	StagedInstance->SetItemDef(ItemDef);
	for (URpgInventoryItemFragment* Fragment :
		GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment)
		{
			Fragment->OnInstanceCreated(StagedInstance);
		}
	}
	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Add;
	Query.Search = ERpgInventoryPlacementSearch::Exact;
	Query.Subject = FRpgInventoryPlacementSubject::FromGeneratedGrant(
		StagedInstance,
		StackCount);
	Query.TargetContainer = Placement.ContainerHandle;
	Query.ExactPlacement = Placement;
	return CommitAddPlacementPlan(
		StagedInstance,
		EvaluatePlacement(Query),
		false);
}

void URpgInventoryManagerComponent::AddItemInstance(URpgInventoryItemInstance* ItemInstance)
{
	AddItemInstanceWithStack(ItemInstance, 1);
}

void URpgInventoryManagerComponent::AddItemInstanceWithStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	AddOwnedItemInstance(ItemInstance, StackCount);
}

void URpgInventoryManagerComponent::AddItemInstanceWithStackToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement)
{
	AddOwnedItemInstance(ItemInstance, StackCount, &Placement);
}

bool URpgInventoryManagerComponent::AddStackToExistingItem(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority())
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

bool URpgInventoryManagerComponent::TryBuildRemovalDeltas(
	const FRpgInventoryEntry& RootEntry,
	int32 Quantity,
	TArray<FRpgInventoryMutationDelta>& OutDeltas,
	ERpgInventoryMutationResultCode& OutCode) const
{
	OutDeltas.Reset();
	OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
	if (!RootEntry.Instance || !RootEntry.Instance->GetItemId().IsValid() ||
		Quantity <= 0 || Quantity > RootEntry.StackCount)
	{
		return false;
	}

	const FRpgInventoryItemId RootItemId = RootEntry.Instance->GetItemId();
	TArray<const FRpgInventoryEntry*> RemovalEntries;
	RemovalEntries.Add(&RootEntry);
	for (const FRpgInventoryEntry& Candidate : InventoryList.Entries)
	{
		if (&Candidate == &RootEntry || !Candidate.Instance)
		{
			continue;
		}

		FRpgInventoryContainerHandle Handle = Candidate.Placement.GetContainerHandle();
		TSet<FRpgInventoryItemId> VisitedOwnerIds;
		for (int32 Guard = 0; Guard <= InventoryList.Entries.Num() && Handle.IsItemOwned(); ++Guard)
		{
			if (Handle.ItemOwnerId == RootItemId)
			{
				RemovalEntries.Add(&Candidate);
				break;
			}
			if (VisitedOwnerIds.Contains(Handle.ItemOwnerId))
			{
				break;
			}

			VisitedOwnerIds.Add(Handle.ItemOwnerId);
			const FRpgInventoryEntry* ParentEntry =
				InventoryList.FindEntryByItemId(Handle.ItemOwnerId);
			Handle = ParentEntry
				? ParentEntry->Placement.GetContainerHandle()
				: FRpgInventoryContainerHandle();
		}
	}

	const bool bHasContainerContract =
		RootEntry.Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;
	const bool bRemovesWholeEntry = Quantity == RootEntry.StackCount;
	if (RemovalEntries.Num() > 1 && !bHasContainerContract)
	{
		// A legal item-owned handle always resolves through a concrete provider. Do not let a broad
		// resource consume silently clean up or destroy descendants of a corrupted non-provider row.
		OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
		return false;
	}
	if (!bRemovesWholeEntry && (bHasContainerContract || RemovalEntries.Num() > 1))
	{
		// A container stack is one physical ownership root. Splitting/removing only part of it would leave
		// descendants without an unambiguous concrete owner, even when its current grids happen to be empty.
		return false;
	}

	if (!bRemovesWholeEntry)
	{
		FRpgInventoryMutationDelta& Delta = OutDeltas.AddDefaulted_GetRef();
		Delta.Kind = ERpgInventoryMutationDeltaKind::StackChanged;
		Delta.ItemId = RootItemId;
		Delta.BeforeContainer = RootEntry.Placement.GetContainerHandle();
		Delta.AfterContainer = Delta.BeforeContainer;
		Delta.BeforePlacement = RootEntry.Placement;
		Delta.AfterPlacement = RootEntry.Placement;
		Delta.PreviousQuantity = RootEntry.StackCount;
		Delta.NewQuantity = RootEntry.StackCount - Quantity;
		OutCode = ERpgInventoryMutationResultCode::Success;
		return true;
	}

	for (const FRpgInventoryEntry* RemovalEntry : RemovalEntries)
	{
		if (!RemovalEntry || !RemovalEntry->Instance)
		{
			OutDeltas.Reset();
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		FRpgInventoryMutationDelta& Delta = OutDeltas.AddDefaulted_GetRef();
		Delta.Kind = ERpgInventoryMutationDeltaKind::Removed;
		Delta.ItemId = RemovalEntry->Instance->GetItemId();
		Delta.BeforeContainer = RemovalEntry->Placement.GetContainerHandle();
		Delta.BeforePlacement = RemovalEntry->Placement;
		Delta.PreviousQuantity = RemovalEntry->StackCount;
		Delta.NewQuantity = 0;
	}

	// Descendants disappear before their physical owner in event/replication order.
	OutDeltas.Sort([](const FRpgInventoryMutationDelta& A, const FRpgInventoryMutationDelta& B)
	{
		return A.BeforeContainer.Depth > B.BeforeContainer.Depth;
	});
	OutCode = ERpgInventoryMutationResultCode::Success;
	return true;
}

bool URpgInventoryManagerComponent::CommitRemovalDeltas(
	const TArray<FRpgInventoryMutationDelta>& Deltas)
{
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority() || Deltas.IsEmpty())
	{
		return false;
	}

	TSet<FRpgInventoryItemId> ChangedItemIds;
	for (const FRpgInventoryMutationDelta& Delta : Deltas)
	{
		if (!Delta.ItemId.IsValid() || ChangedItemIds.Contains(Delta.ItemId))
		{
			return false;
		}
		ChangedItemIds.Add(Delta.ItemId);

		const FRpgInventoryEntry* Entry = InventoryList.FindEntryByItemId(Delta.ItemId);
		if (!Entry || !Entry->Instance ||
			Entry->StackCount != Delta.PreviousQuantity ||
			Entry->Placement != Delta.BeforePlacement ||
			Entry->Placement.GetContainerHandle() != Delta.BeforeContainer)
		{
			return false;
		}

		const bool bValidRemoval =
			Delta.Kind == ERpgInventoryMutationDeltaKind::Removed &&
			Delta.NewQuantity == 0;
		const bool bValidStackChange =
			Delta.Kind == ERpgInventoryMutationDeltaKind::StackChanged &&
			Delta.NewQuantity > 0 &&
			Delta.NewQuantity < Delta.PreviousQuantity &&
			Delta.AfterContainer == Delta.BeforeContainer &&
			Delta.AfterPlacement == Delta.BeforePlacement;
		if (!bValidRemoval && !bValidStackChange)
		{
			return false;
		}
	}

	TArray<int32> RemovalIndices;
	TArray<FRpgInventoryEntry> RemovedEntries;
	for (const FRpgInventoryMutationDelta& Delta : Deltas)
	{
		if (Delta.Kind != ERpgInventoryMutationDeltaKind::Removed)
		{
			continue;
		}

		const int32 EntryIndex =
			InventoryList.Entries.IndexOfByPredicate(
				[&Delta](const FRpgInventoryEntry& Entry)
				{
					return Entry.Instance &&
						Entry.Instance->GetItemId() == Delta.ItemId;
				});
		if (EntryIndex == INDEX_NONE)
		{
			return false;
		}
		RemovalIndices.Add(EntryIndex);
		RemovedEntries.Add(InventoryList.Entries[EntryIndex]);
	}
	RemovalIndices.Sort(TGreater<int32>());

	for (const FRpgInventoryMutationDelta& Delta : Deltas)
	{
		if (Delta.Kind != ERpgInventoryMutationDeltaKind::StackChanged)
		{
			continue;
		}

		FRpgInventoryEntry* Entry = InventoryList.FindEntryByItemId(Delta.ItemId);
		check(Entry);
		Entry->StackCount = Delta.NewQuantity;
		InventoryList.MarkItemDirty(*Entry);
	}

	for (const int32 EntryIndex : RemovalIndices)
	{
		InventoryList.Entries.RemoveAt(EntryIndex, 1, EAllowShrinking::No);
	}
	if (!RemovalIndices.IsEmpty())
	{
		InventoryList.MarkArrayDirty();
	}

	// Removed instances stop being replication subobjects before any synchronous listener can
	// re-bootstrap one. A listener that re-adds the same actor-owned UObject then registers it
	// cleanly instead of having this commit unregister the newly live entry afterward.
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRpgInventoryEntry& RemovedEntry : RemovedEntries)
		{
			URpgInventoryItemInstance* RemovedInstance =
				RemovedEntry.Instance;
			if (RemovedInstance)
			{
				RemoveReplicatedSubObject(RemovedInstance);
			}
		}
	}

	// Notify only after the complete state mutation is visible. Delta order is deepest-first for
	// subtrees, so listeners never observe a parent-removal message before its descendants.
	for (const FRpgInventoryMutationDelta& Delta : Deltas)
	{
		if (Delta.Kind != ERpgInventoryMutationDeltaKind::StackChanged)
		{
			continue;
		}

		FRpgInventoryEntry* Entry =
			InventoryList.FindEntryByItemId(Delta.ItemId);
		check(Entry);
		InventoryList.BroadcastChangeMessage(
			*Entry,
			Delta.PreviousQuantity,
			Delta.NewQuantity);
	}
	for (FRpgInventoryEntry& RemovedEntry : RemovedEntries)
	{
		InventoryList.BroadcastChangeMessage(
			RemovedEntry,
			RemovedEntry.StackCount,
			0);
	}

	MarkInventoryStateDirty();
	return true;
}

void URpgInventoryManagerComponent::RemoveItemInstance(URpgInventoryItemInstance* ItemInstance)
{
	if (!ItemInstance || !ContainsItemInstance(ItemInstance))
	{
		return;
	}

	const int32 StackCount = GetItemStackCount(ItemInstance);
	if (StackCount > 0)
	{
		ConsumeItemById(ItemInstance->GetItemId(), StackCount);
	}
}

bool URpgInventoryManagerComponent::RemoveItemInstanceStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	if (!ItemInstance || !ContainsItemInstance(ItemInstance) || StackCount <= 0)
	{
		return false;
	}

	const FRpgInventoryMutationResult Result =
		ConsumeItemById(ItemInstance->GetItemId(), StackCount);
	return Result.Code == ERpgInventoryMutationResultCode::Success &&
		Result.AppliedQuantity == StackCount;
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

bool URpgInventoryManagerComponent::CanConsumeItemById(
	FRpgInventoryItemId ItemId,
	int32 Quantity) const
{
	const FRpgInventoryEntry* Entry = InventoryList.FindEntryByItemId(ItemId);
	if (!Entry || !Entry->Instance || Quantity <= 0)
	{
		return false;
	}

	FRpgInventoryMutationRequest Request;
	Request.Operation = ERpgInventoryMutationOperation::Consume;
	Request.ItemId = ItemId;
	Request.Source = Entry->Placement.GetContainerHandle();
	Request.Quantity = Quantity;
	return PlanInventoryMutation(Request).IsSuccess();
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::ConsumeItemById(
	FRpgInventoryItemId ItemId,
	int32 Quantity)
{
	FRpgInventoryMutationRequest Request;
	Request.Operation = ERpgInventoryMutationOperation::Consume;
	Request.ItemId = ItemId;
	Request.Quantity = Quantity;
	if (const FRpgInventoryEntry* Entry = InventoryList.FindEntryByItemId(ItemId))
	{
		Request.Source = Entry->Placement.GetContainerHandle();
	}
	Request.EnsureRequestId();
	return ExecuteInventoryMutation(Request);
}

bool URpgInventoryManagerComponent::ConsumeItemsByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 NumToConsume)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || !ItemDef || NumToConsume < 0)
	{
		return false;
	}
	if (NumToConsume == 0)
	{
		return true;
	}

	int32 RemainingToConsume = NumToConsume;
	TArray<FRpgInventoryMutationDelta> PlannedDeltas;
	TSet<FRpgInventoryItemId> PlannedItemIds;
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		if (RemainingToConsume <= 0)
		{
			break;
		}
		if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDef ||
			Entry.Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr)
		{
			continue;
		}

		const int32 CountToConsume = FMath::Min(RemainingToConsume, Entry.StackCount);
		TArray<FRpgInventoryMutationDelta> EntryDeltas;
		ERpgInventoryMutationResultCode PlanCode = ERpgInventoryMutationResultCode::InvalidRequest;
		if (!TryBuildRemovalDeltas(Entry, CountToConsume, EntryDeltas, PlanCode))
		{
			return false;
		}
		for (const FRpgInventoryMutationDelta& Delta : EntryDeltas)
		{
			if (PlannedItemIds.Contains(Delta.ItemId))
			{
				return false;
			}
			PlannedItemIds.Add(Delta.ItemId);
			PlannedDeltas.Add(Delta);
		}
		RemainingToConsume -= CountToConsume;
	}

	return RemainingToConsume == 0 && CommitRemovalDeltas(PlannedDeltas);
}

FRpgInventoryMutationRequest URpgInventoryManagerComponent::BuildMoveMutationRequest(
	const FRpgInventoryMoveIntent& Intent) const
{
	FRpgInventoryMutationRequest Request;
	Request.RequestId = Intent.RequestId;
	Request.ItemId = Intent.ItemId;
	Request.ExpectedEntryId = Intent.ExpectedEntryId;
	Request.Source = Intent.ExpectedSourcePlacement.GetContainerHandle();
	Request.ExpectedSourcePlacement = Intent.ExpectedSourcePlacement;
	Request.ExpectedSourceQuantity = Intent.ExpectedQuantity;
	Request.Target = Intent.TargetPlacement.GetContainerHandle();
	Request.TargetPlacement = Intent.TargetPlacement;
	Request.Quantity = Intent.ExpectedQuantity;

	const bool bSameCell =
		Intent.ExpectedSourcePlacement.IsValid() &&
		Intent.TargetPlacement.IsValid() &&
		Intent.ExpectedSourcePlacement.GetContainerHandle() ==
			Intent.TargetPlacement.GetContainerHandle() &&
		Intent.ExpectedSourcePlacement.X == Intent.TargetPlacement.X &&
		Intent.ExpectedSourcePlacement.Y == Intent.TargetPlacement.Y;
	if (bSameCell &&
		Intent.ExpectedSourcePlacement.bRotated ==
			Intent.TargetPlacement.bRotated)
	{
		Request.Operation = ERpgInventoryMutationOperation::Move;
	}
	else
	{
		Request.Operation = bSameCell
			? ERpgInventoryMutationOperation::Rotate
			: ERpgInventoryMutationOperation::Move;
	}
	return Request;
}

FRpgInventoryMutationRequest
URpgInventoryManagerComponent::BuildEquipmentMoveMutationRequest(
	const FRpgInventoryMoveIntent& Intent) const
{
	FRpgInventoryMutationRequest Request = BuildMoveMutationRequest(Intent);
	// Equip is an internal placement semantic. It preserves the concrete moving identity and is never accepted
	// through the generic UI mutation RPC, including a valid no-op placement.
	Request.Operation = ERpgInventoryMutationOperation::Equip;
	return Request;
}

FRpgInventoryMutationRequest
URpgInventoryManagerComponent::BuildTransferMutationRequest(
	const FRpgInventoryTransferIntent& Intent,
	ERpgInventoryMutationOperation Operation)
{
	FRpgInventoryMutationRequest Request;
	Request.RequestId = Intent.RequestId;
	Request.Operation = Operation;
	Request.ItemId = Intent.ItemId;
	Request.ExpectedEntryId = Intent.ExpectedEntryId;
	Request.Source = Intent.ExpectedSourcePlacement.GetContainerHandle();
	Request.ExpectedSourcePlacement = Intent.ExpectedSourcePlacement;
	Request.ExpectedSourceQuantity = Intent.ExpectedSourceQuantity;
	Request.Target = Intent.TargetContainer;
	Request.TargetPlacement = Intent.TargetPlacement;
	Request.Quantity = Intent.Quantity;
	return Request;
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::PlanMoveItem(
	FRpgInventoryMoveIntent Intent) const
{
	Intent.EnsureRequestId();
	if (!IsCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
		Intent.ExpectedQuantity <= 0 ||
		!Intent.TargetPlacement.IsValid())
	{
		return MakeRejectedIntentResult(
			Intent.RequestId,
			ERpgInventoryMutationOperation::Move,
			Intent.ExpectedQuantity);
	}
	return PlanInventoryMutation(BuildMoveMutationRequest(Intent));
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::MoveItem(
	FRpgInventoryMoveIntent Intent)
{
	Intent.EnsureRequestId();
	const FRpgInventoryMutationRequest Request =
		BuildMoveMutationRequest(Intent);
	FRpgInventoryMutationResult Result;
	if (TryReplayRecentMutation(Request, nullptr, false, Result))
	{
		return Result;
	}
	if (!IsCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
		Intent.ExpectedQuantity <= 0 ||
		!Intent.TargetPlacement.IsValid())
	{
		return CacheRecentMutationResult(
			Request,
			nullptr,
			false,
			MakeRejectedIntentResult(
				Intent.RequestId,
				ERpgInventoryMutationOperation::Move,
				Intent.ExpectedQuantity));
	}
	return ExecuteInventoryMutation(Request);
}

FRpgInventoryMutationResult
URpgInventoryManagerComponent::PlanEquipmentMove(
	FRpgInventoryMoveIntent Intent) const
{
	Intent.EnsureRequestId();
	if (!IsCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
		Intent.ExpectedQuantity <= 0 ||
		!Intent.TargetPlacement.IsValid())
	{
		return MakeRejectedIntentResult(
			Intent.RequestId,
			ERpgInventoryMutationOperation::Equip,
			Intent.ExpectedQuantity);
	}
	return PlanInventoryMutation(
		BuildEquipmentMoveMutationRequest(Intent));
}

FRpgInventoryMutationResult
URpgInventoryManagerComponent::MoveEquipmentItem(
	FRpgInventoryMoveIntent Intent)
{
	Intent.EnsureRequestId();
	const FRpgInventoryMutationRequest Request =
		BuildEquipmentMoveMutationRequest(Intent);
	FRpgInventoryMutationResult Result;
	if (TryReplayRecentMutation(Request, nullptr, false, Result))
	{
		return Result;
	}
	if (!IsCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
		Intent.ExpectedQuantity <= 0 ||
		!Intent.TargetPlacement.IsValid())
	{
		return CacheRecentMutationResult(
			Request,
			nullptr,
			false,
			MakeRejectedIntentResult(
				Intent.RequestId,
				ERpgInventoryMutationOperation::Equip,
				Intent.ExpectedQuantity));
	}
	return ExecuteInventoryMutation(Request);
}

FRpgInventoryMutationResult
URpgInventoryManagerComponent::ExecuteTransferIntent(
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryTransferIntent Intent,
	ERpgInventoryMutationOperation Operation,
	bool bAllowPartialStack)
{
	Intent.EnsureRequestId();
	const FRpgInventoryMutationRequest Request =
		BuildTransferMutationRequest(Intent, Operation);
	FRpgInventoryMutationResult Result;
	if (TryReplayRecentMutation(
			Request,
			TargetInventory,
			bAllowPartialStack,
			Result))
	{
		return Result;
	}
	const bool bTargetPlacementUnset =
		IsCompletelyUnsetPlacement(Intent.TargetPlacement);
	const bool bTargetPlacementMatches =
		bTargetPlacementUnset ||
		!Intent.TargetPlacement.ContainerHandle.IsValid() ||
		Intent.TargetPlacement.ContainerHandle == Intent.TargetContainer;
	if (IsItemOwnedHandleDepthOverflow(
			Intent.ExpectedSourcePlacement.GetContainerHandle()) ||
		IsItemOwnedHandleDepthOverflow(Intent.TargetContainer) ||
		IsItemOwnedHandleDepthOverflow(
			Intent.TargetPlacement.GetContainerHandle()))
	{
		FRpgInventoryMutationResult Rejected =
			MakeRejectedIntentResult(
				Intent.RequestId,
				Operation,
				Intent.Quantity);
		Rejected.Code =
			ERpgInventoryMutationResultCode::MaxDepthExceeded;
		return CacheRecentMutationResult(
			Request,
			TargetInventory,
			bAllowPartialStack,
			MoveTemp(Rejected));
	}
	if (!TargetInventory ||
		!IsCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
		Intent.ExpectedSourceQuantity <= 0 ||
		!Intent.TargetContainer.IsValid() ||
		!bTargetPlacementMatches ||
		Intent.Quantity <= 0)
	{
		return CacheRecentMutationResult(
			Request,
			TargetInventory,
			bAllowPartialStack,
			MakeRejectedIntentResult(
				Intent.RequestId,
				Operation,
				Intent.Quantity));
	}
	return ExecuteCrossInventoryTransfer(
		TargetInventory,
		Request,
		bAllowPartialStack);
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::TransferItem(
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryTransferIntent Intent)
{
	return ExecuteTransferIntent(
		TargetInventory,
		MoveTemp(Intent),
		ERpgInventoryMutationOperation::Transfer,
		false);
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::PickupItem(
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryTransferIntent Intent,
	bool bAllowPartialStack)
{
	return ExecuteTransferIntent(
		TargetInventory,
		MoveTemp(Intent),
		ERpgInventoryMutationOperation::Pickup,
		bAllowPartialStack);
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::PlanDropItem(
	FRpgInventoryTransferIntent Intent) const
{
	Intent.EnsureRequestId();
	if (!IsCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
		Intent.ExpectedSourceQuantity <= 0 ||
		Intent.Quantity <= 0)
	{
		return MakeRejectedIntentResult(
			Intent.RequestId,
			ERpgInventoryMutationOperation::Drop,
			Intent.Quantity);
	}
	return PlanInventoryMutation(
		BuildTransferMutationRequest(
			Intent,
			ERpgInventoryMutationOperation::Drop));
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::DropItem(
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryTransferIntent Intent)
{
	return ExecuteTransferIntent(
		TargetInventory,
		MoveTemp(Intent),
		ERpgInventoryMutationOperation::Drop,
		false);
}

bool URpgInventoryManagerComponent::ApplyInventorySort(ERpgInventorySortMode SortMode)
{
	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority())
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
	if (IsInventoryMutationLocked() || !OwningActor ||
		!OwningActor->HasAuthority())
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
	const FRpgInventoryEntry* Entry =
		InventoryList.FindEntryByEntryId(EntryId);
	if (!Entry || !Entry->Instance)
	{
		return false;
	}

	FRpgInventoryMoveIntent Intent;
	Intent.EnsureRequestId();
	Intent.ItemId = Entry->Instance->GetItemId();
	Intent.ExpectedEntryId = Entry->EntryId;
	Intent.ExpectedSourcePlacement = Entry->Placement;
	Intent.ExpectedQuantity = Entry->StackCount;
	Intent.TargetPlacement = TargetPlacement;
	return MoveItem(Intent).IsSuccess();
}

bool URpgInventoryManagerComponent::CanMoveInventoryEntryToPlacement(FGuid EntryId, FRpgInventoryGridPlacement TargetPlacement) const
{
	const FRpgInventoryEntry* Entry =
		InventoryList.FindEntryByEntryId(EntryId);
	if (!Entry || !Entry->Instance)
	{
		return false;
	}

	FRpgInventoryMoveIntent Intent;
	Intent.EnsureRequestId();
	Intent.ItemId = Entry->Instance->GetItemId();
	Intent.ExpectedEntryId = Entry->EntryId;
	Intent.ExpectedSourcePlacement = Entry->Placement;
	Intent.ExpectedQuantity = Entry->StackCount;
	Intent.TargetPlacement = TargetPlacement;
	return PlanMoveItem(Intent).IsSuccess();
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::PlanInventoryMutation(FRpgInventoryMutationRequest Request) const
{
	Request.EnsureRequestId();
	FRpgInventoryMutationResult Result;
	Result.RequestId = Request.RequestId;
	Result.Operation = Request.Operation;
	Result.RequestedQuantity = Request.Quantity;

	if (Request.Operation == ERpgInventoryMutationOperation::None)
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Result;
	}

	if (Request.Operation == ERpgInventoryMutationOperation::Sort)
	{
		const FRpgInventoryContainerHandle SortContainer = Request.Source.IsValid() ? Request.Source : Request.Target;
		FRpgInventoryGridSize GridSize;
		if (!SortContainer.IsValid() || !GetGridSizeForContainerHandle(SortContainer, GridSize))
		{
			Result.Code = ERpgInventoryMutationResultCode::InvalidContainer;
			return Result;
		}

		Result.Code = ERpgInventoryMutationResultCode::Success;
		for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
		{
			if (Entry.Placement.GetContainerHandle() == SortContainer)
			{
				++Result.AppliedQuantity;
			}
		}
		return Result;
	}

	const FRpgInventoryEntry* MovingEntry = InventoryList.FindEntryByItemId(Request.ItemId);
	if (!MovingEntry || !MovingEntry->Instance)
	{
		Result.Code = ERpgInventoryMutationResultCode::ItemNotFound;
		return Result;
	}

	const FRpgInventoryContainerHandle CurrentContainer = MovingEntry->Placement.GetContainerHandle();
	if (!Request.Source.IsValid() || Request.Source != CurrentContainer)
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return Result;
	}
	if ((Request.ExpectedEntryId.IsValid() &&
			Request.ExpectedEntryId != MovingEntry->EntryId) ||
		(Request.ExpectedSourcePlacement.IsValid() &&
			!ArePlacementSnapshotsExactlyEqual(
				Request.ExpectedSourcePlacement,
				MovingEntry->Placement)) ||
		(Request.ExpectedSourceQuantity > 0 &&
			Request.ExpectedSourceQuantity != MovingEntry->StackCount))
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return Result;
	}

	if (Request.Operation == ERpgInventoryMutationOperation::Drop ||
		Request.Operation == ERpgInventoryMutationOperation::Consume)
	{
		TArray<FRpgInventoryMutationDelta> RemovalDeltas;
		ERpgInventoryMutationResultCode RemovalCode =
			ERpgInventoryMutationResultCode::InvalidRequest;
		if (!TryBuildRemovalDeltas(
				*MovingEntry,
				Request.Quantity,
				RemovalDeltas,
				RemovalCode))
		{
			Result.Code = RemovalCode;
			return Result;
		}

		Result.Deltas = MoveTemp(RemovalDeltas);
		Result.RequestedQuantity = Request.Quantity;
		Result.AppliedQuantity = Request.Quantity;
		Result.Code = ERpgInventoryMutationResultCode::Success;
		return Result;
	}

	if (Request.Operation == ERpgInventoryMutationOperation::Split)
	{
		if (!Request.ExpectedEntryId.IsValid() ||
			!Request.ExpectedSourcePlacement.IsValid() ||
			Request.ExpectedSourceQuantity <= 0)
		{
			Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return Result;
		}

		FRpgInventoryEntryView View;
		View.InventoryOwner = const_cast<URpgInventoryManagerComponent*>(this);
		View.Instance = MovingEntry->Instance;
		View.EntryId = MovingEntry->EntryId;
		View.ItemId = MovingEntry->Instance->GetItemId();
		View.StackCount = MovingEntry->StackCount;
		View.Placement = MovingEntry->Placement;
		FRpgInventoryPlacementQuery PlacementQuery;
		PlacementQuery.Purpose = ERpgInventoryPlacementPurpose::Split;
		PlacementQuery.Search = ERpgInventoryPlacementSearch::Exact;
		PlacementQuery.Subject = FRpgInventoryPlacementSubject::FromOwnedEntry(
			this,
			View,
			Request.Quantity);
		PlacementQuery.TargetContainer = Request.Target;
		PlacementQuery.ExactPlacement = Request.TargetPlacement;
		const FRpgInventoryPlacementPlan PlacementPlan =
			EvaluatePlacement(PlacementQuery);
		if (!PlacementPlan.IsSuccess() || PlacementPlan.Steps.Num() != 1 ||
			PlacementPlan.Steps[0].Resolution !=
				ERpgInventoryPlacementResolution::Place)
		{
			Result.Code = PlacementPlan.Code;
			return Result;
		}
		const FRpgInventoryGridPlacement& SplitPlacement =
			PlacementPlan.Steps[0].Placement;

		FRpgInventoryMutationDelta& SourceDelta = Result.Deltas.AddDefaulted_GetRef();
		SourceDelta.Kind = ERpgInventoryMutationDeltaKind::StackChanged;
		SourceDelta.ItemId = Request.ItemId;
		SourceDelta.BeforeContainer = CurrentContainer;
		SourceDelta.AfterContainer = CurrentContainer;
		SourceDelta.BeforePlacement = MovingEntry->Placement;
		SourceDelta.AfterPlacement = MovingEntry->Placement;
		SourceDelta.PreviousQuantity = MovingEntry->StackCount;
		SourceDelta.NewQuantity = MovingEntry->StackCount - Request.Quantity;

		FRpgInventoryMutationDelta& AddedDelta = Result.Deltas.AddDefaulted_GetRef();
		AddedDelta.Kind = ERpgInventoryMutationDeltaKind::Added;
		AddedDelta.AfterContainer = SplitPlacement.GetContainerHandle();
		AddedDelta.AfterPlacement = SplitPlacement;
		AddedDelta.NewQuantity = Request.Quantity;
		Result.AppliedQuantity = Request.Quantity;
		Result.Code = ERpgInventoryMutationResultCode::Success;
		return Result;
	}

	const bool bPlacementOperation =
		Request.Operation == ERpgInventoryMutationOperation::Move ||
		Request.Operation == ERpgInventoryMutationOperation::Rotate ||
		Request.Operation == ERpgInventoryMutationOperation::Merge ||
		Request.Operation == ERpgInventoryMutationOperation::Swap ||
		Request.Operation == ERpgInventoryMutationOperation::Equip;
	if (!bPlacementOperation || !Request.Target.IsValid())
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Result;
	}
	if (!Request.ExpectedEntryId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.ExpectedSourceQuantity <= 0 ||
		Request.Quantity != MovingEntry->StackCount)
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return Result;
	}

	FRpgInventoryEntryView View;
	View.InventoryOwner = const_cast<URpgInventoryManagerComponent*>(this);
	View.Instance = MovingEntry->Instance;
	View.EntryId = MovingEntry->EntryId;
	View.ItemId = MovingEntry->Instance->GetItemId();
	View.StackCount = MovingEntry->StackCount;
	View.Placement = MovingEntry->Placement;
	FRpgInventoryPlacementQuery PlacementQuery;
	PlacementQuery.Purpose = Request.Operation == ERpgInventoryMutationOperation::Equip
		? ERpgInventoryPlacementPurpose::Equip
		: ERpgInventoryPlacementPurpose::Move;
	PlacementQuery.Search = ERpgInventoryPlacementSearch::Exact;
	PlacementQuery.Subject = FRpgInventoryPlacementSubject::FromOwnedEntry(
		this,
		View);
	PlacementQuery.TargetContainer = Request.Target;
	PlacementQuery.ExactPlacement = Request.TargetPlacement;
	const FRpgInventoryPlacementPlan PlacementPlan =
		EvaluatePlacement(PlacementQuery);
	const bool bRequiresCompletePlacement =
		Request.Operation != ERpgInventoryMutationOperation::Merge;
	if (!PlacementPlan.IsSuccess() || PlacementPlan.Steps.Num() != 1 ||
		(bRequiresCompletePlacement &&
		 !PlacementPlan.IsCompleteSuccess()))
	{
		// Typed Move/Equip placement intents move one complete entry. A compatible
		// target with insufficient capacity may still describe a read-only partial
		// plan, while only an explicitly requested low-level Merge may commit it.
		Result.Code = PlacementPlan.Code ==
				ERpgInventoryMutationResultCode::PartiallyApplied
			? ERpgInventoryMutationResultCode::StackLimitReached
			: PlacementPlan.Code;
		return Result;
	}

	const FRpgInventoryPlacementStep& Step = PlacementPlan.Steps[0];
	if ((Request.Operation == ERpgInventoryMutationOperation::Merge &&
		 Step.Resolution != ERpgInventoryPlacementResolution::Merge) ||
		(Request.Operation == ERpgInventoryMutationOperation::Swap &&
		 Step.Resolution != ERpgInventoryPlacementResolution::Swap) ||
		(Request.Operation == ERpgInventoryMutationOperation::Rotate &&
		 (Step.Resolution != ERpgInventoryPlacementResolution::Place ||
		  Step.Placement.GetContainerHandle() != CurrentContainer ||
		  Step.Placement.X != MovingEntry->Placement.X ||
		  Step.Placement.Y != MovingEntry->Placement.Y ||
		  Step.Placement.bRotated == MovingEntry->Placement.bRotated)))
	{
		Result.Code = Request.Operation == ERpgInventoryMutationOperation::Merge
			? ERpgInventoryMutationResultCode::StackIncompatible
			: ERpgInventoryMutationResultCode::InvalidRequest;
		return Result;
	}

	Result.RequestedQuantity = MovingEntry->StackCount;
	Result.AppliedQuantity = PlacementPlan.AppliedQuantity;
	Result.Code = PlacementPlan.Code;
	if (Step.Resolution == ERpgInventoryPlacementResolution::NoOp)
	{
		return Result;
	}
	if (Step.Resolution == ERpgInventoryPlacementResolution::Place)
	{
		FRpgInventoryMutationDelta& Delta = Result.Deltas.AddDefaulted_GetRef();
		Delta.Kind = Request.Operation == ERpgInventoryMutationOperation::Rotate
			? ERpgInventoryMutationDeltaKind::Rotated
			: ERpgInventoryMutationDeltaKind::Moved;
		Delta.ItemId = Request.ItemId;
		Delta.BeforeContainer = CurrentContainer;
		Delta.AfterContainer = Step.Placement.GetContainerHandle();
		Delta.BeforePlacement = MovingEntry->Placement;
		Delta.AfterPlacement = Step.Placement;
		Delta.PreviousQuantity = MovingEntry->StackCount;
		Delta.NewQuantity = MovingEntry->StackCount;
		return Result;
	}
	if (Step.Resolution == ERpgInventoryPlacementResolution::Merge)
	{
		const FRpgInventoryEntry* TargetEntry =
			InventoryList.FindEntryByEntryId(Step.TargetEntryId);
		if (!TargetEntry || !TargetEntry->Instance)
		{
			Result.Code = ERpgInventoryMutationResultCode::InternalError;
			Result.AppliedQuantity = 0;
			return Result;
		}

		FRpgInventoryMutationDelta& MovingDelta = Result.Deltas.AddDefaulted_GetRef();
		MovingDelta.Kind = Step.Quantity == MovingEntry->StackCount
			? ERpgInventoryMutationDeltaKind::Removed
			: ERpgInventoryMutationDeltaKind::StackChanged;
		MovingDelta.ItemId = Request.ItemId;
		MovingDelta.BeforeContainer = CurrentContainer;
		MovingDelta.BeforePlacement = MovingEntry->Placement;
		MovingDelta.PreviousQuantity = MovingEntry->StackCount;
		MovingDelta.NewQuantity = MovingEntry->StackCount - Step.Quantity;
		if (MovingDelta.NewQuantity > 0)
		{
			MovingDelta.AfterContainer = CurrentContainer;
			MovingDelta.AfterPlacement = MovingEntry->Placement;
		}

		FRpgInventoryMutationDelta& TargetDelta = Result.Deltas.AddDefaulted_GetRef();
		TargetDelta.Kind = ERpgInventoryMutationDeltaKind::StackChanged;
		TargetDelta.ItemId = TargetEntry->Instance->GetItemId();
		TargetDelta.BeforeContainer = TargetEntry->Placement.GetContainerHandle();
		TargetDelta.AfterContainer = TargetEntry->Placement.GetContainerHandle();
		TargetDelta.BeforePlacement = TargetEntry->Placement;
		TargetDelta.AfterPlacement = TargetEntry->Placement;
		TargetDelta.PreviousQuantity = TargetEntry->StackCount;
		TargetDelta.NewQuantity = TargetEntry->StackCount + Step.Quantity;
		return Result;
	}
	if (Step.Resolution != ERpgInventoryPlacementResolution::Swap)
	{
		Result.Code = ERpgInventoryMutationResultCode::InternalError;
		Result.AppliedQuantity = 0;
		return Result;
	}

	const FRpgInventoryEntry* TargetEntry =
		InventoryList.FindEntryByEntryId(Step.DisplacedEntryId);
	if (!TargetEntry || !TargetEntry->Instance)
	{
		Result.Code = ERpgInventoryMutationResultCode::InternalError;
		Result.AppliedQuantity = 0;
		return Result;
	}
	FRpgInventoryMutationDelta& MovingDelta = Result.Deltas.AddDefaulted_GetRef();
	MovingDelta.Kind = ERpgInventoryMutationDeltaKind::Moved;
	MovingDelta.ItemId = Request.ItemId;
	MovingDelta.BeforeContainer = CurrentContainer;
	MovingDelta.AfterContainer = Step.Placement.GetContainerHandle();
	MovingDelta.BeforePlacement = MovingEntry->Placement;
	MovingDelta.AfterPlacement = Step.Placement;
	MovingDelta.PreviousQuantity = MovingEntry->StackCount;
	MovingDelta.NewQuantity = MovingEntry->StackCount;

	FRpgInventoryMutationDelta& TargetDelta = Result.Deltas.AddDefaulted_GetRef();
	TargetDelta.Kind = ERpgInventoryMutationDeltaKind::Moved;
	TargetDelta.ItemId = TargetEntry->Instance->GetItemId();
	TargetDelta.BeforeContainer = TargetEntry->Placement.GetContainerHandle();
	TargetDelta.AfterContainer = Step.DisplacedPlacement.GetContainerHandle();
	TargetDelta.BeforePlacement = TargetEntry->Placement;
	TargetDelta.AfterPlacement = Step.DisplacedPlacement;
	TargetDelta.PreviousQuantity = TargetEntry->StackCount;
	TargetDelta.NewQuantity = TargetEntry->StackCount;
	return Result;
}

bool URpgInventoryManagerComponent::AreMutationRequestsEquivalent(
	const FRpgInventoryMutationRequest& A,
	const FRpgInventoryMutationRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.Operation == B.Operation &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		A.Source == B.Source &&
		ArePlacementSnapshotsExactlyEqual(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.Target == B.Target &&
		A.Quantity == B.Quantity &&
		ArePlacementSnapshotsExactlyEqual(
			A.TargetPlacement,
			B.TargetPlacement);
}

bool URpgInventoryManagerComponent::TryReplayRecentMutation(
	const FRpgInventoryMutationRequest& Request,
	URpgInventoryManagerComponent* TargetInventory,
	bool bAllowPartialStack,
	FRpgInventoryMutationResult& OutResult)
{
	FRecentMutationRecord* Record =
		RecentMutationResults.Find(Request.RequestId);
	if (!Record)
	{
		return false;
	}

	URpgInventoryManagerComponent* CachedTargetInventory =
		Record->TargetInventory.Get();
	const bool bEpochMatches =
		Record->SourceMutationEpoch == MutationEpoch &&
		(!Record->bHadTargetInventory ||
			(CachedTargetInventory &&
				Record->TargetMutationEpoch ==
					CachedTargetInventory->GetMutationEpoch()));
	if (!bEpochMatches)
	{
		// Restore starts a new command namespace. Discard the stale record before
		// payload comparison so the same correlation id can describe a fresh
		// command against the restored source/target graphs.
		RecentMutationResults.Remove(Request.RequestId);
		RecentMutationOrder.Remove(Request.RequestId);
		return false;
	}

	const bool bHasTargetInventory = TargetInventory != nullptr;
	const bool bTargetMatches =
		Record->bHadTargetInventory == bHasTargetInventory &&
		(!bHasTargetInventory ||
			CachedTargetInventory == TargetInventory);
	if (Record->Kind == FRecentMutationRecord::EKind::SingleMutation &&
		AreMutationRequestsEquivalent(Record->Request, Request) &&
		bTargetMatches &&
		Record->bAllowPartialStack == bAllowPartialStack)
	{
		OutResult = Record->Result;
		return true;
	}

	// A correlation id identifies one immutable command. Reusing it with a
	// different payload, target, or partial policy must never replay success
	// for an operation that did not happen.
	OutResult = FRpgInventoryMutationResult();
	OutResult.RequestId = Request.RequestId;
	OutResult.Operation = Request.Operation;
	OutResult.RequestedQuantity = Request.Quantity;
	OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
	return true;
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::CacheRecentMutationResult(
	const FRpgInventoryMutationRequest& Request,
	URpgInventoryManagerComponent* TargetInventory,
	bool bAllowPartialStack,
	FRpgInventoryMutationResult Result)
{
	if (!Result.RequestId.IsValid())
	{
		return Result;
	}

	FRecentMutationRecord& Record =
		RecentMutationResults.FindOrAdd(Result.RequestId);
	Record.Kind = FRecentMutationRecord::EKind::SingleMutation;
	Record.Request = Request;
	Record.TargetContainers.Reset();
	Record.AffectedTargetItemIds.Reset();
	Record.TargetInventory = TargetInventory;
	Record.bHadTargetInventory = TargetInventory != nullptr;
	Record.bAllowPartialStack = bAllowPartialStack;
	Record.SourceMutationEpoch = MutationEpoch;
	Record.TargetMutationEpoch = TargetInventory
		? TargetInventory->GetMutationEpoch()
		: 0;
	Record.Result = Result;
	RecentMutationOrder.Remove(Result.RequestId);
	RecentMutationOrder.Add(Result.RequestId);
	while (RecentMutationOrder.Num() > MaxRecentMutationResults)
	{
		RecentMutationResults.Remove(RecentMutationOrder[0]);
		RecentMutationOrder.RemoveAt(0, 1, EAllowShrinking::No);
	}
	return Result;
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::ExecuteInventoryMutation(FRpgInventoryMutationRequest Request)
{
	Request.EnsureRequestId();
	FRpgInventoryMutationResult Result;
	if (TryReplayRecentMutation(Request, nullptr, false, Result))
	{
		return Result;
	}
	auto CacheResult = [this, &Request](FRpgInventoryMutationResult ResultToCache)
	{
		return CacheRecentMutationResult(
			Request,
			nullptr,
			false,
			MoveTemp(ResultToCache));
	};
	if (IsInventoryMutationLocked())
	{
		Result.RequestId = Request.RequestId;
		Result.Operation = Request.Operation;
		Result.RequestedQuantity = Request.Quantity;
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return CacheResult(MoveTemp(Result));
	}

	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		Result.RequestId = Request.RequestId;
		Result.Operation = Request.Operation;
		Result.RequestedQuantity = Request.Quantity;
		Result.Code = ERpgInventoryMutationResultCode::AuthorityRequired;
		return CacheResult(MoveTemp(Result));
	}

	Result = PlanInventoryMutation(Request);
	if (!Result.IsSuccess())
	{
		return CacheResult(MoveTemp(Result));
	}
	if (Request.Operation == ERpgInventoryMutationOperation::Drop)
	{
		// A physical drop requires a durable target actor and therefore commits only through the dedicated
		// drop gateway as a cross-inventory transfer. The generic manager path is preview-only for Drop.
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		Result.AppliedQuantity = 0;
		Result.Deltas.Reset();
		return CacheResult(MoveTemp(Result));
	}

	FRpgInventoryEntry* Entry = InventoryList.FindEntryByItemId(Request.ItemId);
	bool bCommitted = false;
	bool bInventoryStateChanged = true;
	switch (Request.Operation)
	{
	case ERpgInventoryMutationOperation::Move:
	case ERpgInventoryMutationOperation::Rotate:
	case ERpgInventoryMutationOperation::Merge:
	case ERpgInventoryMutationOperation::Swap:
	case ERpgInventoryMutationOperation::Equip:
		bInventoryStateChanged = !Result.Deltas.IsEmpty();
		if (Entry)
		{
			FRpgInventoryGridPlacement Placement = Request.TargetPlacement;
			Placement.SetContainerHandle(Request.Target);
			bCommitted = InventoryList.MoveEntryToPlacement(
				Entry->EntryId,
				Placement,
				Request.Operation !=
					ERpgInventoryMutationOperation::Equip);
		}
		break;

	case ERpgInventoryMutationOperation::Split:
		if (Entry && Entry->Instance)
		{
			const FRpgInventoryMutationDelta* AddedDelta =
				Result.Deltas.FindByPredicate(
					[](const FRpgInventoryMutationDelta& Delta)
					{
						return Delta.Kind == ERpgInventoryMutationDeltaKind::Added;
					});
			if (AddedDelta && AddedDelta->AfterPlacement.IsValid())
			{
				URpgInventoryItemInstance* SplitInstance = NewObject<URpgInventoryItemInstance>(OwningActor);
				SplitInstance->SetItemDef(Entry->Instance->GetItemDef());
				for (URpgInventoryItemFragment* Fragment : GetDefault<URpgInventoryItemDefinition>(Entry->Instance->GetItemDef())->Fragments)
				{
					if (Fragment)
					{
						Fragment->OnInstanceCreated(SplitInstance);
					}
				}

				if (SplitInstance->CopyRuntimeStateFrom(Entry->Instance, false))
				{
					bool bRemovedEntry = false;
					if (InventoryList.RemoveEntryStack(Entry->Instance, Request.Quantity, bRemovedEntry) && !bRemovedEntry)
					{
						InventoryList.AddEntryAtPlacement(
							SplitInstance,
							Request.Quantity,
							AddedDelta->AfterPlacement);
						bCommitted = InventoryList.ContainsItemInstance(SplitInstance);
						if (!bCommitted)
						{
							InventoryList.AddStackToEntry(Entry->Instance, Request.Quantity);
						}
						else
						{
							for (FRpgInventoryMutationDelta& Delta : Result.Deltas)
							{
								if (Delta.Kind == ERpgInventoryMutationDeltaKind::Added)
								{
									Delta.ItemId = SplitInstance->GetItemId();
								}
							}
							if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
							{
								AddReplicatedSubObject(SplitInstance, ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
							}
						}
					}
				}
			}
		}
		break;

	case ERpgInventoryMutationOperation::Sort:
	{
		const int32 SortValue = FMath::Clamp(Request.Quantity, 0, static_cast<int32>(ERpgInventorySortMode::Recent));
		const FRpgInventoryContainerHandle SortContainer = Request.Source.IsValid() ? Request.Source : Request.Target;
		bInventoryStateChanged = InventoryList.ApplySort(
			static_cast<ERpgInventorySortMode>(SortValue),
			SortContainer,
			&bCommitted);
		break;
	}

	case ERpgInventoryMutationOperation::Consume:
		bInventoryStateChanged = false;
		bCommitted = CommitRemovalDeltas(Result.Deltas);
		break;

	default:
		break;
	}

	if (!bCommitted)
	{
		Result.Code = ERpgInventoryMutationResultCode::InternalError;
		Result.AppliedQuantity = 0;
		Result.Deltas.Reset();
	}
	else if (bInventoryStateChanged)
	{
		MarkInventoryStateDirty();
	}

	return CacheResult(MoveTemp(Result));
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

const URpgPlayerInventoryLayoutComponent* URpgInventoryManagerComponent::FindOwningPlayerInventoryLayout() const
{
	const ARpgPlayerState* RpgPlayerState = Cast<ARpgPlayerState>(GetOwner());
	const ARpgPlayerController* RpgPlayerController = RpgPlayerState ? RpgPlayerState->GetRpgPlayerController() : nullptr;
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPlayerController ? RpgPlayerController->GetPlayerInventoryLayoutComponent() : nullptr;
	return InventoryLayout && RpgPlayerState && RpgPlayerState->GetInventoryManagerComponent() == this ? InventoryLayout : nullptr;
}

bool URpgInventoryManagerComponent::FValidatedInventoryGraph::GatherSubtreeIndices(
	int32 RootIndex,
	TArray<int32>& OutIndices) const
{
	OutIndices.Reset();
	if (!ParentIndexByEntry.IsValidIndex(RootIndex))
	{
		return false;
	}

	for (int32 CandidateIndex = 0;
		 CandidateIndex < ParentIndexByEntry.Num();
		 ++CandidateIndex)
	{
		int32 CurrentIndex = CandidateIndex;
		for (int32 Guard = 0;
			 Guard <= ParentIndexByEntry.Num() && CurrentIndex != INDEX_NONE;
			 ++Guard)
		{
			if (CurrentIndex == RootIndex)
			{
				OutIndices.Add(CandidateIndex);
				break;
			}
			CurrentIndex = ParentIndexByEntry.IsValidIndex(CurrentIndex)
				? ParentIndexByEntry[CurrentIndex]
				: INDEX_NONE;
		}
	}

	return OutIndices.Contains(RootIndex);
}

bool URpgInventoryManagerComponent::ValidateInventoryGraph(
	const TArray<FRpgInventoryEntry>& Entries,
	const UObject* ExpectedInstanceOuter,
	bool bEnforceCapacity,
	FValidatedInventoryGraph& OutGraph,
	ERpgInventoryMutationResultCode& OutCode) const
{
	OutGraph = FValidatedInventoryGraph();
	OutCode = ERpgInventoryMutationResultCode::Success;
	// Capacity is the outer import/target policy and intentionally wins over
	// deeper corruption diagnostics for a graph that cannot be admitted anyway.
	if (bEnforceCapacity && !IsCapacityUnlimited() &&
		Entries.Num() > GetMaxEntries())
	{
		OutCode = ERpgInventoryMutationResultCode::NoSpace;
		return false;
	}

	const int32 EntryCount = Entries.Num();
	OutGraph.IndexByItemId.Reserve(EntryCount);
	OutGraph.IndexByEntryId.Reserve(EntryCount);
	OutGraph.ParentIndexByEntry.Init(INDEX_NONE, EntryCount);
	OutGraph.RootIndexByEntry.Init(INDEX_NONE, EntryCount);
	OutGraph.DeepestRelativeDepthByEntry.Init(0, EntryCount);

	for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
	{
		const FRpgInventoryEntry& Entry = Entries[EntryIndex];
		if (!Entry.Instance || !Entry.Instance->GetItemDef() ||
			(ExpectedInstanceOuter &&
			 Entry.Instance->GetOuter() != ExpectedInstanceOuter))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		const FRpgInventoryItemId ItemId = Entry.Instance->GetItemId();
		if (!ItemId.IsValid())
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		if (OutGraph.IndexByItemId.Contains(ItemId))
		{
			OutCode = ERpgInventoryMutationResultCode::DuplicateItemId;
			return false;
		}
		OutGraph.IndexByItemId.Add(ItemId, EntryIndex);

		if (!Entry.EntryId.IsValid())
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		if (OutGraph.IndexByEntryId.Contains(Entry.EntryId))
		{
			OutCode = ERpgInventoryMutationResultCode::DuplicateEntryId;
			return false;
		}
		OutGraph.IndexByEntryId.Add(Entry.EntryId, EntryIndex);

		if (Entry.StackCount <= 0 ||
			Entry.StackCount > GetInventoryManagerMaxStackSizeForDefinition(
				Entry.Instance->GetItemDef()))
		{
			OutCode = ERpgInventoryMutationResultCode::StackLimitReached;
			return false;
		}

		const FRpgInventoryContainerHandle Handle =
			Entry.Placement.GetContainerHandle();
		if (IsItemOwnedHandleDepthOverflow(Handle))
		{
			OutCode = ERpgInventoryMutationResultCode::MaxDepthExceeded;
			return false;
		}
		if (!Handle.IsValid())
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}
		if (!Entry.Placement.IsValid())
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}

		FRpgInventoryGridPlacement NormalizedPlacement;
		if (!TryNormalizePlacementForDefinition(
				Entry.Instance->GetItemDef(),
				Handle,
				Entry.Placement.X,
				Entry.Placement.Y,
				Entry.Placement.bRotated,
				NormalizedPlacement) ||
			!ArePlacementSnapshotsExactlyEqual(
				NormalizedPlacement,
				Entry.Placement))
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}
	}

	TArray<FRpgInventoryItemContainerDefinition> ItemOwnedDefinitions;
	ItemOwnedDefinitions.SetNum(EntryCount);
	TArray<FRpgInventoryGridSize> GridSizes;
	GridSizes.SetNum(EntryCount);
	for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
	{
		const FRpgInventoryEntry& Entry = Entries[EntryIndex];
		const FRpgInventoryContainerHandle Handle =
			Entry.Placement.GetContainerHandle();
		if (Handle.IsRoot())
		{
			if (!GetGridSizeForContainerHandle(Handle, GridSizes[EntryIndex]))
			{
				OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}
			continue;
		}

		const int32* OwnerIndex =
			OutGraph.IndexByItemId.Find(Handle.ItemOwnerId);
		if (!OwnerIndex || !Entries.IsValidIndex(*OwnerIndex))
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}
		OutGraph.ParentIndexByEntry[EntryIndex] = *OwnerIndex;

		const URpgInventoryFragment_ItemContainer* ContainerFragment =
			Entries[*OwnerIndex].Instance
				? Entries[*OwnerIndex].Instance->FindFragmentByClass<
					URpgInventoryFragment_ItemContainer>()
				: nullptr;
		TArray<FRpgInventoryItemContainerDefinition> Definitions;
		if (ContainerFragment)
		{
			ContainerFragment->GetProvidedContainers(Definitions);
		}
		const FRpgInventoryItemContainerDefinition* Definition =
			Definitions.FindByPredicate(
				[&Handle](
					const FRpgInventoryItemContainerDefinition& Candidate)
				{
					return Candidate.ContainerId == Handle.ContainerId &&
						Candidate.IsValid();
				});
		if (!Definition)
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}
		ItemOwnedDefinitions[EntryIndex] = *Definition;
		GridSizes[EntryIndex] = Definition->GridSize;
	}

	TArray<uint8> VisitState;
	VisitState.Init(0, EntryCount);
	TArray<int32> AncestryPath;
	AncestryPath.Reserve(EntryCount);
	for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
	{
		if (VisitState[EntryIndex] == 2)
		{
			continue;
		}

		AncestryPath.Reset();
		int32 CurrentIndex = EntryIndex;
		int32 ResolvedRootIndex = INDEX_NONE;
		while (CurrentIndex != INDEX_NONE)
		{
			if (!Entries.IsValidIndex(CurrentIndex))
			{
				OutCode =
					ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}
			if (VisitState[CurrentIndex] == 2)
			{
				ResolvedRootIndex =
					OutGraph.RootIndexByEntry[CurrentIndex];
				break;
			}
			if (VisitState[CurrentIndex] == 1)
			{
				OutCode =
					ERpgInventoryMutationResultCode::CycleDetected;
				return false;
			}

			VisitState[CurrentIndex] = 1;
			AncestryPath.Add(CurrentIndex);
			CurrentIndex =
				OutGraph.ParentIndexByEntry[CurrentIndex];
		}

		for (int32 PathIndex = AncestryPath.Num() - 1;
			 PathIndex >= 0;
			 --PathIndex)
		{
			const int32 PathEntryIndex = AncestryPath[PathIndex];
			if (OutGraph.ParentIndexByEntry[PathEntryIndex] == INDEX_NONE)
			{
				ResolvedRootIndex = PathEntryIndex;
			}
			if (!Entries.IsValidIndex(ResolvedRootIndex))
			{
				OutCode =
					ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}
			OutGraph.RootIndexByEntry[PathEntryIndex] =
				ResolvedRootIndex;
			VisitState[PathEntryIndex] = 2;
		}
	}

	TMap<FRpgInventoryContainerHandle, TArray<FRpgInventoryGridPlacement>>
		OccupancyByContainer;
	for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
	{
		const FRpgInventoryEntry& Entry = Entries[EntryIndex];
		const FRpgInventoryContainerHandle Handle =
			Entry.Placement.GetContainerHandle();
		const int32 ParentIndex =
			OutGraph.ParentIndexByEntry[EntryIndex];
		if (ParentIndex != INDEX_NONE)
		{
			const uint8 ParentDepth = Entries[ParentIndex]
				.Placement.GetContainerHandle().Depth;
			if (ParentDepth >= RpgInventoryMaxItemOwnedDepth)
			{
				OutCode = ERpgInventoryMutationResultCode::MaxDepthExceeded;
				return false;
			}
			const uint8 ExpectedDepth = static_cast<uint8>(ParentDepth + 1);
			if (Handle.Depth != ExpectedDepth)
			{
				OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}

			const FRpgInventoryItemContainerDefinition& Definition =
				ItemOwnedDefinitions[EntryIndex];
			if (!Definition.AllowsItemDefinition(
					Entry.Instance->GetItemDef(),
					Handle.Depth))
			{
				OutCode = GetItemContainerRuleFailureCode(
					Definition,
					Entry.Instance->GetItemDef(),
					Handle.Depth);
				return false;
			}
		}
		else if (const URpgPlayerInventoryLayoutComponent* Layout =
				 FindOwningPlayerInventoryLayout())
		{
			FRpgInventorySlotAddress Address;
			Address.SetContainerHandle(Handle);
			Address.X = Entry.Placement.X;
			Address.Y = Entry.Placement.Y;
			if (!Layout->CanItemUseSlotAddress(Entry.Instance, Address))
			{
				OutCode = ERpgInventoryMutationResultCode::ItemNotAllowed;
				return false;
			}
		}

		TArray<FRpgInventoryGridPlacement>& Occupancy =
			OccupancyByContainer.FindOrAdd(Handle);
		const ERpgInventoryMutationResultCode PlacementCode =
			EvaluateScratchPlacement(
				Entry.Placement,
				GridSizes[EntryIndex],
				Occupancy);
		if (PlacementCode != ERpgInventoryMutationResultCode::Success)
		{
			OutCode = PlacementCode;
			return false;
		}
		Occupancy.Add(Entry.Placement);

		int32 CurrentIndex = EntryIndex;
		uint8 RelativeDepth = 0;
		while (OutGraph.ParentIndexByEntry[CurrentIndex] != INDEX_NONE)
		{
			CurrentIndex = OutGraph.ParentIndexByEntry[CurrentIndex];
			++RelativeDepth;
			OutGraph.DeepestRelativeDepthByEntry[CurrentIndex] =
				FMath::Max(
					OutGraph.DeepestRelativeDepthByEntry[CurrentIndex],
					RelativeDepth);
		}
	}

	return true;
}

bool URpgInventoryManagerComponent::ValidateLiveInventoryGraph(
	bool bEnforceCapacity,
	FValidatedInventoryGraph& OutGraph,
	ERpgInventoryMutationResultCode& OutCode) const
{
	return ValidateInventoryGraph(
		InventoryList.Entries,
		GetOwner(),
		bEnforceCapacity,
		OutGraph,
		OutCode);
}

bool URpgInventoryManagerComponent::IsInventoryMutationLocked() const
{
	return bIsApplyingPickupBatch || bIsPlanningPickupBatch ||
		bIsApplyingCollectBatch || bIsApplyingCrossInventoryTransfer ||
		bIsRestoringInventoryGraph;
}

bool URpgInventoryManagerComponent::GetGridSizeForContainerHandle(FRpgInventoryContainerHandle ContainerHandle, FRpgInventoryGridSize& OutGridSize) const
{
	OutGridSize = FRpgInventoryGridSize();
	if (!ContainerHandle.IsValid())
	{
		return false;
	}

	if (ContainerHandle.IsRoot())
	{
		if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout())
		{
			return InventoryLayout->GetGridSizeForContainerHandle(
				ContainerHandle,
				OutGridSize);
		}

		if (ContainerHandle ==
			FRpgInventoryContainerHandle::MakeRoot(DefaultContainerId))
		{
			OutGridSize = DefaultGridSize;
			return OutGridSize.IsValid();
		}

		return false;
	}

	FRpgInventoryItemContainerDefinition Definition;
	if (!GetItemContainerDefinition(ContainerHandle, Definition))
	{
		return false;
	}

	OutGridSize = Definition.GridSize;
	return OutGridSize.IsValid();
}

bool URpgInventoryManagerComponent::GetItemContainerDefinition(
	const FRpgInventoryContainerHandle& ContainerHandle,
	FRpgInventoryItemContainerDefinition& OutDefinition) const
{
	OutDefinition = FRpgInventoryItemContainerDefinition();
	if (!ContainerHandle.IsItemOwned())
	{
		return false;
	}

	const FRpgInventoryEntry* OwnerEntry = InventoryList.FindEntryByItemId(ContainerHandle.ItemOwnerId);
	const URpgInventoryItemInstance* OwnerItem = OwnerEntry ? OwnerEntry->Instance.Get() : nullptr;
	const URpgInventoryFragment_ItemContainer* ContainerFragment = OwnerItem
		? OwnerItem->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()
		: nullptr;
	if (!OwnerEntry || !ContainerFragment)
	{
		return false;
	}

	const uint8 ExpectedDepth = OwnerEntry->Placement.GetContainerHandle().GetDirectChildDepth();
	if (ExpectedDepth == 0 || ContainerHandle.Depth != ExpectedDepth)
	{
		return false;
	}

	TArray<FRpgInventoryItemContainerDefinition> Definitions;
	ContainerFragment->GetProvidedContainers(Definitions);
	if (const FRpgInventoryItemContainerDefinition* FoundDefinition = Definitions.FindByPredicate(
		[&ContainerHandle](const FRpgInventoryItemContainerDefinition& Candidate)
		{
			return Candidate.ContainerId == ContainerHandle.ContainerId && Candidate.IsValid();
		}))
	{
		OutDefinition = *FoundDefinition;
		return true;
	}

	return false;
}

bool URpgInventoryManagerComponent::ValidatePlacementGraphRules(
	const FRpgInventoryEntry& Entry,
	const FRpgInventoryGridPlacement& Placement,
	ERpgInventoryMutationResultCode& OutCode) const
{
	OutCode = ERpgInventoryMutationResultCode::Success;
	if (IsItemOwnedHandleDepthOverflow(
			Placement.GetContainerHandle()))
	{
		OutCode = ERpgInventoryMutationResultCode::MaxDepthExceeded;
		return false;
	}
	if (!Entry.Instance || !Placement.GetContainerHandle().IsValid())
	{
		OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
		return false;
	}

	const FRpgInventoryContainerHandle TargetHandle = Placement.GetContainerHandle();
	if (TargetHandle.IsRoot())
	{
		FRpgInventoryGridSize RootSize;
		if (!GetGridSizeForContainerHandle(TargetHandle, RootSize))
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}
		return true;
	}

	FRpgInventoryItemContainerDefinition Definition;
	if (!GetItemContainerDefinition(TargetHandle, Definition))
	{
		OutCode = TargetHandle.Depth > RpgInventoryMaxItemOwnedDepth
			? ERpgInventoryMutationResultCode::MaxDepthExceeded
			: ERpgInventoryMutationResultCode::InvalidContainer;
		return false;
	}

	if (WouldCreateContainerCycle(Entry.Instance->GetItemId(), TargetHandle))
	{
		OutCode = ERpgInventoryMutationResultCode::CycleDetected;
		return false;
	}

	if (!Definition.AllowsItemDefinition(Entry.Instance->GetItemDef(), TargetHandle.Depth))
	{
		OutCode = GetItemContainerRuleFailureCode(
			Definition,
			Entry.Instance->GetItemDef(),
			TargetHandle.Depth);
		return false;
	}

	const uint8 CurrentDepth = Entry.Placement.GetContainerHandle().Depth;
	uint8 DeepestRelativeDepth = 0;
	for (const FRpgInventoryEntry& Candidate : InventoryList.Entries)
	{
		if (!Candidate.Instance || Candidate.Instance == Entry.Instance)
		{
			continue;
		}

		FRpgInventoryContainerHandle AncestorHandle = Candidate.Placement.GetContainerHandle();
		for (int32 Guard = 0; Guard < InventoryList.Entries.Num() && AncestorHandle.IsItemOwned(); ++Guard)
		{
			if (AncestorHandle.ItemOwnerId == Entry.Instance->GetItemId())
			{
				DeepestRelativeDepth = FMath::Max<uint8>(DeepestRelativeDepth,
					Candidate.Placement.GetContainerHandle().Depth > CurrentDepth
						? Candidate.Placement.GetContainerHandle().Depth - CurrentDepth
						: 1);
				break;
			}

			const FRpgInventoryEntry* AncestorEntry = InventoryList.FindEntryByItemId(AncestorHandle.ItemOwnerId);
			AncestorHandle = AncestorEntry ? AncestorEntry->Placement.GetContainerHandle() : FRpgInventoryContainerHandle();
		}
	}

	if (TargetHandle.Depth + DeepestRelativeDepth > RpgInventoryMaxItemOwnedDepth)
	{
		OutCode = ERpgInventoryMutationResultCode::MaxDepthExceeded;
		return false;
	}

	return true;
}

bool URpgInventoryManagerComponent::WouldCreateContainerCycle(
	const FRpgInventoryItemId& MovingItemId,
	const FRpgInventoryContainerHandle& TargetContainer) const
{
	if (!MovingItemId.IsValid() || !TargetContainer.IsItemOwned())
	{
		return false;
	}

	FRpgInventoryItemId AncestorItemId = TargetContainer.ItemOwnerId;
	for (int32 Guard = 0; Guard <= InventoryList.Entries.Num() && AncestorItemId.IsValid(); ++Guard)
	{
		if (AncestorItemId == MovingItemId)
		{
			return true;
		}

		const FRpgInventoryEntry* AncestorEntry = InventoryList.FindEntryByItemId(AncestorItemId);
		if (!AncestorEntry)
		{
			return false;
		}

		const FRpgInventoryContainerHandle ParentHandle = AncestorEntry->Placement.GetContainerHandle();
		AncestorItemId = ParentHandle.IsItemOwned() ? ParentHandle.ItemOwnerId : FRpgInventoryItemId();
	}

	// Traversing more nodes than the graph contains means corrupt cyclic ancestry already exists.
	return AncestorItemId.IsValid();
}

bool URpgInventoryManagerComponent::ShouldUseSingleCellPlacementForContainer(
	const FRpgInventoryContainerHandle& ContainerHandle) const
{
	if (!ContainerHandle.IsRoot())
	{
		return false;
	}

	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout();
	if (!InventoryLayout)
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (Group.ContainerHandle == ContainerHandle)
		{
			return Group.GroupKind == ERpgInventorySlotGroupKind::Gear ||
				Group.GroupKind == ERpgInventorySlotGroupKind::Carry;
		}
	}

	return false;
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


