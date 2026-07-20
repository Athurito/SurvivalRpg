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

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Inventory_Message_StackChanged, "Rpg.Inventory.Message.StackChanged");

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryManager, Log, All);

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

	bool HasCompleteSourceSnapshot(
		const FRpgInventoryItemId& ItemId,
		const FGuid& ExpectedEntryId,
		const FRpgInventoryGridPlacement& ExpectedSourcePlacement)
	{
		return ItemId.IsValid() &&
			ExpectedEntryId.IsValid() &&
			ExpectedSourcePlacement.IsValid();
	}

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
	const int32 RequiredNewEntries = GetRequiredNewEntryCount(ItemDef, StackCount);
	TArray<FRpgInventoryGridPlacement> PlannedPlacements;
	PlannedPlacements.Reserve(RequiredNewEntries);
	for (int32 NewEntryIndex = 0; NewEntryIndex < RequiredNewEntries; ++NewEntryIndex)
	{
		FRpgInventoryGridPlacement PlannedPlacement;
		if (!FindFirstFitPlacement(ItemDef, PlannedPlacement, PlannedPlacements))
		{
			UE_LOG(LogRpgInventoryManager, Warning,
				TEXT("AddEntry rejected before commit: the complete stack does not fit. Inventory=%s ItemDef=%s StackCount=%d"),
				*GetNameSafe(OwnerComponent), *GetNameSafe(ItemDef), StackCount);
			return nullptr;
		}

		PlannedPlacements.Add(PlannedPlacement);
	}

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

	int32 PlannedPlacementIndex = 0;
	while (RemainingCount > 0)
	{
		if (!PlannedPlacements.IsValidIndex(PlannedPlacementIndex))
		{
			ensureMsgf(false, TEXT("The preflight placement count diverged while committing %s"), *GetNameSafe(ItemDef));
			return nullptr;
		}
		const FRpgInventoryGridPlacement NewPlacement = PlannedPlacements[PlannedPlacementIndex++];

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

bool FRpgInventoryList::CanFullyAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const
{
	if (!ItemDef || StackCount <= 0)
	{
		return false;
	}

	const int32 RequiredNewEntries = GetRequiredNewEntryCount(ItemDef, StackCount);
	TArray<FRpgInventoryGridPlacement> ScratchOccupancy;
	ScratchOccupancy.Reserve(RequiredNewEntries);
	for (int32 NewEntryIndex = 0; NewEntryIndex < RequiredNewEntries; ++NewEntryIndex)
	{
		FRpgInventoryGridPlacement Placement;
		if (!FindFirstFitPlacement(ItemDef, Placement, ScratchOccupancy))
		{
			return false;
		}

		ScratchOccupancy.Add(Placement);
	}

	return true;
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

URpgInventoryItemInstance* FRpgInventoryList::GetItemAtCell(FName ContainerId, int32 X, int32 Y) const
{
	const FRpgInventoryEntry* Entry = FindEntryAtCell(ContainerId, X, Y);
	return Entry ? Entry->Instance.Get() : nullptr;
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

bool FRpgInventoryList::CanMoveEntryToPlacement(FGuid EntryId, const FRpgInventoryGridPlacement& TargetPlacement, FRpgInventoryGridPlacement* OutNormalizedTargetPlacement) const
{
	if (!EntryId.IsValid() || !TargetPlacement.IsValid())
	{
		return false;
	}

	const FRpgInventoryEntry* MovingEntry = FindEntryByEntryId(EntryId);
	if (!MovingEntry || !MovingEntry->Instance)
	{
		return false;
	}

	FRpgInventoryGridPlacement NormalizedTargetPlacement;
	if (!NormalizePlacementForEntry(*MovingEntry, TargetPlacement, NormalizedTargetPlacement))
	{
		return false;
	}

	if (OutNormalizedTargetPlacement)
	{
		*OutNormalizedTargetPlacement = NormalizedTargetPlacement;
	}

	if (!CanEntryUsePlacement(*MovingEntry, NormalizedTargetPlacement) ||
		!IsPlacementWithinGrid(NormalizedTargetPlacement))
	{
		return false;
	}

	TArray<const FRpgInventoryEntry*> OverlappingEntries;
	FindEntriesOverlapping(NormalizedTargetPlacement, MovingEntry, OverlappingEntries);
	if (OverlappingEntries.Num() == 0)
	{
		return true;
	}

	if (OverlappingEntries.Num() > 1)
	{
		return false;
	}

	const FRpgInventoryEntry* TargetEntry = OverlappingEntries[0];
	if (!TargetEntry || !TargetEntry->Instance)
	{
		return false;
	}

	if (TargetEntry->Instance && !MovingEntry->Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() &&
		MovingEntry->Instance->IsStackCompatibleWith(TargetEntry->Instance))
	{
		const int32 MaxStackSize = GetInventoryManagerMaxStackSizeForDefinition(TargetEntry->Instance->GetItemDef());
		const int32 FreeCapacity = FMath::Max(0, MaxStackSize - TargetEntry->StackCount);
		if (FreeCapacity > 0)
		{
			return true;
		}
	}

	FRpgInventoryGridPlacement TargetSwapPlacement;
	return CanPlaceEntryAt(NormalizedTargetPlacement, MovingEntry, TargetEntry) &&
		TryResolveDisplacedEntryPlacement(*MovingEntry, NormalizedTargetPlacement, *TargetEntry, TargetSwapPlacement);
}

bool FRpgInventoryList::MoveEntryToPlacement(FGuid EntryId, const FRpgInventoryGridPlacement& TargetPlacement)
{
	FRpgInventoryGridPlacement NormalizedTargetPlacement;
	if (!CanMoveEntryToPlacement(EntryId, TargetPlacement, &NormalizedTargetPlacement))
	{
		return false;
	}

	FRpgInventoryEntry* MovingEntry = FindEntryByEntryId(EntryId);
	if (!MovingEntry || !MovingEntry->Instance)
	{
		return false;
	}

	TArray<const FRpgInventoryEntry*> OverlappingEntries;
	FindEntriesOverlapping(NormalizedTargetPlacement, MovingEntry, OverlappingEntries);
	FRpgInventoryEntry* TargetEntry = OverlappingEntries.Num() == 1 ? FindEntryByEntryId(OverlappingEntries[0]->EntryId) : nullptr;
	if (!TargetEntry)
	{
		const int32 DepthDelta = static_cast<int32>(NormalizedTargetPlacement.GetContainerHandle().Depth) -
			static_cast<int32>(MovingEntry->Placement.GetContainerHandle().Depth);
		const FRpgInventoryItemId MovingItemId = MovingEntry->Instance->GetItemId();
		MovingEntry->Placement = NormalizedTargetPlacement;
		RebaseDescendantContainerDepths(MovingItemId, DepthDelta);
		MarkItemDirty(*MovingEntry);
		BroadcastChangeMessage(*MovingEntry, MovingEntry->StackCount, MovingEntry->StackCount, true);
		SortEntriesByPlacement();
		MarkArrayDirty();
		return true;
	}

	if (TargetEntry->Instance && !MovingEntry->Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() &&
		MovingEntry->Instance->IsStackCompatibleWith(TargetEntry->Instance))
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

	FRpgInventoryGridPlacement TargetSwapPlacement;
	if (!CanPlaceEntryAt(NormalizedTargetPlacement, MovingEntry, TargetEntry) ||
		!TryResolveDisplacedEntryPlacement(*MovingEntry, NormalizedTargetPlacement, *TargetEntry, TargetSwapPlacement))
	{
		return false;
	}

	const int32 MovingDepthDelta = static_cast<int32>(NormalizedTargetPlacement.GetContainerHandle().Depth) -
		static_cast<int32>(MovingEntry->Placement.GetContainerHandle().Depth);
	const int32 TargetDepthDelta = static_cast<int32>(TargetSwapPlacement.GetContainerHandle().Depth) -
		static_cast<int32>(TargetEntry->Placement.GetContainerHandle().Depth);
	const FRpgInventoryItemId MovingItemId = MovingEntry->Instance->GetItemId();
	const FRpgInventoryItemId TargetItemId = TargetEntry->Instance->GetItemId();
	MovingEntry->Placement = NormalizedTargetPlacement;
	TargetEntry->Placement = TargetSwapPlacement;
	RebaseDescendantContainerDepths(MovingItemId, MovingDepthDelta);
	RebaseDescendantContainerDepths(TargetItemId, TargetDepthDelta);
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
		SnapshotEntry.ItemId = Entry->Instance->GetItemId();
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
		if (SnapshotEntry.ItemId.IsValid())
		{
			NewEntry.Instance->RestoreItemId(SnapshotEntry.ItemId);
		}
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

FRpgInventoryEntry* FRpgInventoryList::FindEntryAtCell(FName ContainerId, int32 X, int32 Y)
{
	for (FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr && Entry.StackCount > 0 && Entry.Placement.ContainsCell(X, Y) &&
			Entry.Placement.GetContainerHandle() == FRpgInventoryContainerHandle::MakeRoot(ContainerId))
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
		if (Entry.Instance != nullptr && Entry.StackCount > 0 && Entry.Placement.ContainsCell(X, Y) &&
			Entry.Placement.GetContainerHandle() == FRpgInventoryContainerHandle::MakeRoot(ContainerId))
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
		Placement.X + OccupiedSize.Width <= GridSize.Width &&
		Placement.Y + OccupiedSize.Height <= GridSize.Height;
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
		const bool bMadePlacement = Inventory->TryMakePlacementForItemDefinition(
			ItemDefinition,
			TargetPlacement.ContainerId,
			TargetPlacement.X,
			TargetPlacement.Y,
			TargetPlacement.bRotated,
			OutNormalizedPlacement);
		if (bMadePlacement)
		{
			OutNormalizedPlacement.SetContainerHandle(TargetPlacement.GetContainerHandle());
		}
		return bMadePlacement;
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

	auto TryFindInGrid = [this, ItemDef, &AdditionalOccupancy](const FRpgInventoryContainerHandle& ContainerHandle, const FRpgInventoryGridSize& GridSize, bool bRotated, FRpgInventoryGridPlacement& CandidatePlacement)
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

		CandidatePlacement.SetContainerHandle(ContainerHandle);
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
				const bool bOverlapsScratch = AdditionalOccupancy.ContainsByPredicate(
					[&CandidatePlacement](const FRpgInventoryGridPlacement& OccupiedPlacement)
					{
						return OccupiedPlacement.Overlaps(CandidatePlacement);
					});
				if (!bOverlapsScratch && CanPlaceEntryAt(CandidatePlacement))
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

			if (TryFindInGrid(Group.ContainerHandle, Group.GridSize, false, OutPlacement))
			{
				return true;
			}

			if (CanInventoryManagerRotateDefinition(ItemDef) && TryFindInGrid(Group.ContainerHandle, Group.GridSize, true, OutPlacement))
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

	const FRpgInventoryContainerHandle DefaultHandle = FRpgInventoryContainerHandle::MakeRoot(Inventory->DefaultContainerId);
	if (TryFindInGrid(DefaultHandle, DefaultGridSize, false, OutPlacement))
	{
		return true;
	}

	return CanInventoryManagerRotateDefinition(ItemDef) && TryFindInGrid(DefaultHandle, DefaultGridSize, true, OutPlacement);
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

	auto TryOrientation = [ItemDef, &ContainerHandle, &GridSize, &ScratchOccupancy](bool bRotated, FRpgInventoryGridPlacement& Candidate)
	{
		const FRpgInventoryGridSize UnrotatedSize = GetInventoryManagerFootprintForDefinition(ItemDef, false);
		if (!UnrotatedSize.IsValid() || (bRotated && !CanInventoryManagerRotateDefinition(ItemDef)))
		{
			return false;
		}

		Candidate.SetContainerHandle(ContainerHandle);
		Candidate.Width = UnrotatedSize.Width;
		Candidate.Height = UnrotatedSize.Height;
		Candidate.bRotated = bRotated;
		const FRpgInventoryGridSize OccupiedSize = Candidate.GetOccupiedSize();
		for (int32 Y = 0; Y <= GridSize.Height - OccupiedSize.Height; ++Y)
		{
			for (int32 X = 0; X <= GridSize.Width - OccupiedSize.Width; ++X)
			{
				Candidate.X = X;
				Candidate.Y = Y;
				if (!ScratchOccupancy.ContainsByPredicate(
					[&Candidate](const FRpgInventoryGridPlacement& OccupiedPlacement)
					{
						return OccupiedPlacement.Overlaps(Candidate);
					}))
				{
					return true;
				}
			}
		}

		return false;
	};

	return TryOrientation(false, OutPlacement) || TryOrientation(true, OutPlacement);
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
		if (!(EntryContainer.IsRoot() && Inventory->ShouldUseSingleCellPlacementForContainer(Entry->Placement.ContainerId)) &&
			!FindFirstFitPlacementInContainer(Entry->Instance->GetItemDef(), EntryContainer, ContainerScratch, NewPlacement))
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
	return InventoryList.GetRequiredNewEntryCount(ItemDef, StackCount);
}

int32 URpgInventoryManagerComponent::GetRequiredNewEntryCountForItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount) const
{
	// Bootstrap preserves one concrete runtime instance and never merges it into a
	// different identity. Merge-aware physical moves use the dedicated transfer intents.
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

bool URpgInventoryManagerComponent::ExpandDefaultGridToMinimum(
	FRpgInventoryGridSize MinimumSize)
{
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

	const int32 RequiredNewEntries = InventoryList.GetRequiredNewEntryCount(ItemDef, StackCount);
	if (RequiredNewEntries <= 0)
	{
		return true;
	}

	if (!IsCapacityUnlimited() && RequiredNewEntries > GetFreeEntryCount())
	{
		return false;
	}

	return InventoryList.CanFullyAddItemDefinition(ItemDef, StackCount);
}

bool URpgInventoryManagerComponent::CanAddItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount) const
{
	return InventoryList.CanInsertOwnedInstance(ItemInstance) &&
		!IsItemManagedByAnyInventory(ItemInstance) &&
		!HasItemIdentityConflictInAnyInventory(ItemInstance) &&
		CanReceiveTransferredItemInstance(ItemInstance, StackCount);
}

bool URpgInventoryManagerComponent::CanReceiveTransferredItemInstance(
	URpgInventoryItemInstance* ItemInstance,
	int32 StackCount) const
{
	if (ItemInstance == nullptr || !ItemInstance->GetItemDef() || StackCount <= 0)
	{
		return false;
	}

	if (StackCount > GetInventoryManagerMaxStackSizeForDefinition(ItemInstance->GetItemDef()))
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

	FRpgInventoryGridPlacement NormalizedPlacement;
	if (!TryMakePlacementForItemDefinition(ItemDef, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated, NormalizedPlacement))
	{
		return false;
	}
	NormalizedPlacement.SetContainerHandle(Placement.GetContainerHandle());

	if (!InventoryList.IsPlacementWithinGrid(NormalizedPlacement))
	{
		return false;
	}

	if (NormalizedPlacement.GetContainerHandle().IsItemOwned())
	{
		FRpgInventoryItemContainerDefinition Definition;
		if (!GetItemContainerDefinition(NormalizedPlacement.GetContainerHandle(), Definition) ||
			!Definition.AllowsItemDefinition(ItemDef, NormalizedPlacement.GetContainerHandle().Depth))
		{
			return false;
		}
	}
	else if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindOwningPlayerInventoryLayout())
	{
		bool bAllowedByGroup = false;
		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.ContainerHandle == NormalizedPlacement.GetContainerHandle() &&
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

	TArray<const FRpgInventoryEntry*> OverlappingEntries;
	InventoryList.FindEntriesOverlapping(NormalizedPlacement, nullptr, OverlappingEntries);
	if (OverlappingEntries.Num() == 0)
	{
		return (IsCapacityUnlimited() || GetFreeEntryCount() > 0) && InventoryList.CanPlaceEntryAt(NormalizedPlacement);
	}

	if (OverlappingEntries.Num() > 1 || !OverlappingEntries[0]->Instance)
	{
		return false;
	}

	return OverlappingEntries[0]->Instance->GetItemDef() == ItemDef && InventoryList.GetFreeStackCapacity(OverlappingEntries[0]->Instance) >= StackCount;
}

bool URpgInventoryManagerComponent::CanAddItemInstanceToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement) const
{
	return InventoryList.CanInsertOwnedInstance(ItemInstance) &&
		!IsItemManagedByAnyInventory(ItemInstance) &&
		CanReceiveTransferredItemInstanceToPlacement(ItemInstance, StackCount, Placement);
}

bool URpgInventoryManagerComponent::CanReceiveTransferredItemInstanceToPlacement(
	URpgInventoryItemInstance* ItemInstance,
	int32 StackCount,
	FRpgInventoryGridPlacement Placement) const
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

	FRpgInventoryGridPlacement NormalizedPlacement;
	if (!TryMakePlacementForItemInstance(ItemInstance, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated, NormalizedPlacement))
	{
		return false;
	}
	NormalizedPlacement.SetContainerHandle(Placement.GetContainerHandle());

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

	return (IsCapacityUnlimited() || GetFreeEntryCount() > 0) && InventoryList.CanPlaceEntryAt(NormalizedPlacement);
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

	FRpgInventoryGridPlacement NormalizedPlacement;
	if (!TryMakePlacementForItemInstance(ItemInstance, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated, NormalizedPlacement))
	{
		return false;
	}
	NormalizedPlacement.SetContainerHandle(Placement.GetContainerHandle());

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

	FRpgInventoryGridPlacement NormalizedPlacement;
	if (!TryMakePlacementForItemInstance(ItemInstance, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated, NormalizedPlacement))
	{
		return nullptr;
	}
	NormalizedPlacement.SetContainerHandle(Placement.GetContainerHandle());

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

URpgInventoryItemInstance* URpgInventoryManagerComponent::GrantItemDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef,
	int32 StackCount)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || !CanAddItemDefinition(ItemDef, StackCount))
	{
		return nullptr;
	}

	TArray<URpgInventoryItemInstance*> NewInstances;
	URpgInventoryItemInstance* Result = InventoryList.AddEntry(ItemDef, StackCount, NewInstances);
	if (!Result)
	{
		return nullptr;
	}

	MarkInventoryStateDirty();
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		for (URpgInventoryItemInstance* NewInstance : NewInstances)
		{
			if (NewInstance)
			{
				AddReplicatedSubObject(NewInstance, ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
			}
		}
	}
	return Result;
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::BootstrapItemInstance(
	URpgInventoryItemInstance* SourceItemInstance,
	int32 StackCount)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() ||
		!CanBootstrapItemInstance(SourceItemInstance, StackCount))
	{
		return nullptr;
	}

	if (SourceItemInstance->GetOuter() == OwningActor)
	{
		return AddOwnedItemInstance(SourceItemInstance, StackCount) ? SourceItemInstance : nullptr;
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

	if (!OwnedInstance->CopyRuntimeStateFrom(SourceItemInstance, false) ||
		!AddOwnedItemInstance(OwnedInstance, StackCount))
	{
		return nullptr;
	}

	return OwnedInstance;
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

	if (SourceItemInstance->GetOuter() == OwningActor)
	{
		return CanAddItemInstance(SourceItemInstance, StackCount);
	}

	return CanReceiveTransferredItemInstance(SourceItemInstance, StackCount);
}

bool URpgInventoryManagerComponent::AddOwnedItemInstance(
	URpgInventoryItemInstance* ItemInstance,
	int32 StackCount,
	const FRpgInventoryGridPlacement* Placement)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || !ItemInstance ||
		!InventoryList.CanInsertOwnedInstance(ItemInstance) ||
		IsItemManagedByAnyInventory(ItemInstance) ||
		HasItemIdentityConflictInAnyInventory(ItemInstance))
	{
		return false;
	}

	bool bAdded = false;
	if (Placement)
	{
		if (!CanAddItemInstanceToPlacement(ItemInstance, StackCount, *Placement))
		{
			return false;
		}

		FRpgInventoryGridPlacement NormalizedPlacement = MakePlacementForItemInstance(
			ItemInstance,
			Placement->ContainerId,
			Placement->X,
			Placement->Y,
			Placement->bRotated);
		NormalizedPlacement.SetContainerHandle(Placement->GetContainerHandle());
		bAdded = InventoryList.AddEntryAtPlacement(ItemInstance, StackCount, NormalizedPlacement);
	}
	else
	{
		if (!CanAddItemInstance(ItemInstance, StackCount))
		{
			return false;
		}
		bAdded = InventoryList.AddEntry(ItemInstance, StackCount);
	}

	if (!bAdded)
	{
		return false;
	}

	MarkInventoryStateDirty();
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		AddReplicatedSubObject(
			ItemInstance,
			ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
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
	URpgInventoryItemInstance* Result = nullptr;
	if (OwningActor && OwningActor->HasAuthority() &&
		CanAddItemDefinitionToPlacement(ItemDef, StackCount, Placement))
	{
		FRpgInventoryGridPlacement NormalizedPlacement = MakePlacementForItemDefinition(ItemDef, Placement.ContainerId, Placement.X, Placement.Y, Placement.bRotated);
		NormalizedPlacement.SetContainerHandle(Placement.GetContainerHandle());
		TArray<URpgInventoryItemInstance*> NewInstances;
		Result = InventoryList.AddEntryAtPlacement(ItemDef, StackCount, NormalizedPlacement, NewInstances);
		if (!Result)
		{
			return nullptr;
		}

		MarkInventoryStateDirty();
		if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
		{
			for (URpgInventoryItemInstance* NewInstance : NewInstances)
			{
				if (NewInstance)
				{
					AddReplicatedSubObject(NewInstance, ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
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
	AddOwnedItemInstance(ItemInstance, StackCount);
}

void URpgInventoryManagerComponent::AddItemInstanceWithStackToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement)
{
	AddOwnedItemInstance(ItemInstance, StackCount, &Placement);
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
	if (!OwningActor || !OwningActor->HasAuthority() || Deltas.IsEmpty())
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

URpgInventoryItemInstance* URpgInventoryManagerComponent::GetItemAtCell(FName ContainerId, int32 X, int32 Y) const
{
	return InventoryList.GetItemAtCell(ContainerId, X, Y);
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
		Request.Operation = ERpgInventoryMutationOperation::None;
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
	Request.Target = Intent.TargetContainer;
	Request.TargetPlacement = Intent.TargetPlacement;
	Request.Quantity = Intent.Quantity;
	return Request;
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::PlanMoveItem(
	FRpgInventoryMoveIntent Intent) const
{
	Intent.EnsureRequestId();
	if (!HasCompleteSourceSnapshot(
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
	if (!HasCompleteSourceSnapshot(
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
	const bool bTargetPlacementMatches =
		!Intent.TargetPlacement.IsValid() ||
		Intent.TargetPlacement.GetContainerHandle() ==
			Intent.TargetContainer;
	if (!TargetInventory ||
		!HasCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
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
	if (!HasCompleteSourceSnapshot(
			Intent.ItemId,
			Intent.ExpectedEntryId,
			Intent.ExpectedSourcePlacement) ||
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
			Request.ExpectedSourcePlacement != MovingEntry->Placement))
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
		if (Request.Quantity <= 0 || Request.Quantity >= MovingEntry->StackCount ||
			GetInventoryManagerMaxStackSizeForDefinition(MovingEntry->Instance->GetItemDef()) <= 1 ||
			MovingEntry->Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr)
		{
			Result.Code = ERpgInventoryMutationResultCode::StackLimitReached;
			return Result;
		}

		FRpgInventoryGridPlacement SplitPlacement;
		if (!InventoryList.NormalizePlacementForEntry(*MovingEntry, Request.TargetPlacement, SplitPlacement))
		{
			Result.Code = ERpgInventoryMutationResultCode::InvalidPlacement;
			return Result;
		}
		if (Request.Target.IsValid() && SplitPlacement.GetContainerHandle() != Request.Target)
		{
			Result.Code = ERpgInventoryMutationResultCode::InvalidContainer;
			return Result;
		}
		ERpgInventoryMutationResultCode GraphCode = ERpgInventoryMutationResultCode::Success;
		if (!ValidatePlacementGraphRules(*MovingEntry, SplitPlacement, GraphCode))
		{
			Result.Code = GraphCode;
			return Result;
		}
		if (!InventoryList.CanEntryUsePlacement(*MovingEntry, SplitPlacement))
		{
			Result.Code = ERpgInventoryMutationResultCode::ItemNotAllowed;
			return Result;
		}
		if (!InventoryList.IsPlacementWithinGrid(SplitPlacement))
		{
			Result.Code = ERpgInventoryMutationResultCode::OutOfBounds;
			return Result;
		}
		if (!InventoryList.CanPlaceEntryAt(SplitPlacement) || (!IsCapacityUnlimited() && GetFreeEntryCount() <= 0))
		{
			Result.Code = ERpgInventoryMutationResultCode::Occupied;
			return Result;
		}

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
	if (Request.ExpectedEntryId.IsValid() &&
		Request.ExpectedSourcePlacement.IsValid() &&
		Request.Quantity != MovingEntry->StackCount)
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return Result;
	}

	FRpgInventoryGridPlacement RequestedPlacement = Request.TargetPlacement;
	RequestedPlacement.SetContainerHandle(Request.Target);
	FRpgInventoryGridPlacement NormalizedTarget;
	if (!InventoryList.NormalizePlacementForEntry(*MovingEntry, RequestedPlacement, NormalizedTarget))
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidPlacement;
		return Result;
	}

	// Rotate is deliberately an in-place operation. Treating it as a generic
	// placement request would let a forged Rotate RPC move, merge, or swap an
	// item while bypassing the semantic operation contract exposed to the UI.
	if (Request.Operation == ERpgInventoryMutationOperation::Rotate &&
		(NormalizedTarget.GetContainerHandle() != CurrentContainer ||
		 NormalizedTarget.X != MovingEntry->Placement.X ||
		 NormalizedTarget.Y != MovingEntry->Placement.Y ||
		 NormalizedTarget.bRotated == MovingEntry->Placement.bRotated))
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Result;
	}

	ERpgInventoryMutationResultCode GraphCode = ERpgInventoryMutationResultCode::Success;
	if (!ValidatePlacementGraphRules(*MovingEntry, NormalizedTarget, GraphCode))
	{
		Result.Code = GraphCode;
		return Result;
	}
	if (!InventoryList.IsPlacementWithinGrid(NormalizedTarget))
	{
		Result.Code = ERpgInventoryMutationResultCode::OutOfBounds;
		return Result;
	}
	if (!InventoryList.CanEntryUsePlacement(*MovingEntry, NormalizedTarget))
	{
		Result.Code = ERpgInventoryMutationResultCode::ItemNotAllowed;
		return Result;
	}
	if (!InventoryList.CanMoveEntryToPlacement(MovingEntry->EntryId, NormalizedTarget))
	{
		Result.Code = ERpgInventoryMutationResultCode::Occupied;
		return Result;
	}

	TArray<const FRpgInventoryEntry*> Overlaps;
	InventoryList.FindEntriesOverlapping(NormalizedTarget, MovingEntry, Overlaps);
	if (Request.Operation == ERpgInventoryMutationOperation::Rotate && !Overlaps.IsEmpty())
	{
		Result.Code = ERpgInventoryMutationResultCode::Occupied;
		return Result;
	}
	const FRpgInventoryEntry* TargetEntry = Overlaps.Num() == 1 ? Overlaps[0] : nullptr;
	const bool bCompatibleMerge = TargetEntry && TargetEntry->Instance &&
		!MovingEntry->Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() &&
		MovingEntry->Instance->IsStackCompatibleWith(TargetEntry->Instance) &&
		InventoryList.GetFreeStackCapacity(TargetEntry->Instance) > 0;

	if (Request.Operation == ERpgInventoryMutationOperation::Merge && !bCompatibleMerge)
	{
		Result.Code = ERpgInventoryMutationResultCode::StackIncompatible;
		return Result;
	}
	if (Request.Operation == ERpgInventoryMutationOperation::Swap && (!TargetEntry || bCompatibleMerge))
	{
		Result.Code = ERpgInventoryMutationResultCode::Occupied;
		return Result;
	}

	Result.RequestedQuantity = MovingEntry->StackCount;
	if (!TargetEntry)
	{
		FRpgInventoryMutationDelta& Delta = Result.Deltas.AddDefaulted_GetRef();
		Delta.Kind = Request.Operation == ERpgInventoryMutationOperation::Rotate
			? ERpgInventoryMutationDeltaKind::Rotated
			: ERpgInventoryMutationDeltaKind::Moved;
		Delta.ItemId = Request.ItemId;
		Delta.BeforeContainer = CurrentContainer;
		Delta.AfterContainer = NormalizedTarget.GetContainerHandle();
		Delta.BeforePlacement = MovingEntry->Placement;
		Delta.AfterPlacement = NormalizedTarget;
		Delta.PreviousQuantity = MovingEntry->StackCount;
		Delta.NewQuantity = MovingEntry->StackCount;
		Result.AppliedQuantity = MovingEntry->StackCount;
		Result.Code = ERpgInventoryMutationResultCode::Success;
		return Result;
	}

	if (bCompatibleMerge)
	{
		const int32 MergeQuantity = FMath::Min(MovingEntry->StackCount, InventoryList.GetFreeStackCapacity(TargetEntry->Instance));
		FRpgInventoryMutationDelta& MovingDelta = Result.Deltas.AddDefaulted_GetRef();
		MovingDelta.Kind = MergeQuantity == MovingEntry->StackCount
			? ERpgInventoryMutationDeltaKind::Removed
			: ERpgInventoryMutationDeltaKind::StackChanged;
		MovingDelta.ItemId = Request.ItemId;
		MovingDelta.BeforeContainer = CurrentContainer;
		MovingDelta.BeforePlacement = MovingEntry->Placement;
		MovingDelta.PreviousQuantity = MovingEntry->StackCount;
		MovingDelta.NewQuantity = MovingEntry->StackCount - MergeQuantity;
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
		TargetDelta.NewQuantity = TargetEntry->StackCount + MergeQuantity;
		Result.AppliedQuantity = MergeQuantity;
		Result.Code = MergeQuantity == MovingEntry->StackCount
			? ERpgInventoryMutationResultCode::Success
			: ERpgInventoryMutationResultCode::PartiallyApplied;
		return Result;
	}

	FRpgInventoryGridPlacement TargetSwapPlacement;
	if (!InventoryList.TryResolveDisplacedEntryPlacement(*MovingEntry, NormalizedTarget, *TargetEntry, TargetSwapPlacement))
	{
		Result.Code = ERpgInventoryMutationResultCode::NoSpace;
		return Result;
	}

	FRpgInventoryMutationDelta& MovingDelta = Result.Deltas.AddDefaulted_GetRef();
	MovingDelta.Kind = ERpgInventoryMutationDeltaKind::Moved;
	MovingDelta.ItemId = Request.ItemId;
	MovingDelta.BeforeContainer = CurrentContainer;
	MovingDelta.AfterContainer = NormalizedTarget.GetContainerHandle();
	MovingDelta.BeforePlacement = MovingEntry->Placement;
	MovingDelta.AfterPlacement = NormalizedTarget;
	MovingDelta.PreviousQuantity = MovingEntry->StackCount;
	MovingDelta.NewQuantity = MovingEntry->StackCount;

	FRpgInventoryMutationDelta& TargetDelta = Result.Deltas.AddDefaulted_GetRef();
	TargetDelta.Kind = ERpgInventoryMutationDeltaKind::Moved;
	TargetDelta.ItemId = TargetEntry->Instance->GetItemId();
	TargetDelta.BeforeContainer = TargetEntry->Placement.GetContainerHandle();
	TargetDelta.AfterContainer = TargetSwapPlacement.GetContainerHandle();
	TargetDelta.BeforePlacement = TargetEntry->Placement;
	TargetDelta.AfterPlacement = TargetSwapPlacement;
	TargetDelta.PreviousQuantity = TargetEntry->StackCount;
	TargetDelta.NewQuantity = TargetEntry->StackCount;
	Result.AppliedQuantity = MovingEntry->StackCount;
	Result.Code = ERpgInventoryMutationResultCode::Success;
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
		A.ExpectedSourcePlacement == B.ExpectedSourcePlacement &&
		A.Target == B.Target &&
		A.Quantity == B.Quantity &&
		A.TargetPlacement == B.TargetPlacement;
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
	if (AreMutationRequestsEquivalent(Record->Request, Request) &&
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
	Record.Request = Request;
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
		if (Entry)
		{
			FRpgInventoryGridPlacement Placement = Request.TargetPlacement;
			Placement.SetContainerHandle(Request.Target);
			bCommitted = InventoryList.MoveEntryToPlacement(Entry->EntryId, Placement);
		}
		break;

	case ERpgInventoryMutationOperation::Split:
		if (Entry && Entry->Instance)
		{
			FRpgInventoryGridPlacement SplitPlacement = Request.TargetPlacement;
			SplitPlacement.SetContainerHandle(Request.Target);
			FRpgInventoryGridPlacement NormalizedSplitPlacement;
			if (InventoryList.NormalizePlacementForEntry(*Entry, SplitPlacement, NormalizedSplitPlacement))
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
						InventoryList.AddEntryAtPlacement(SplitInstance, Request.Quantity, NormalizedSplitPlacement);
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
	const AActor* SourceOwner = GetOwner();
	const AActor* TargetOwner = TargetInventory ? TargetInventory->GetOwner() : nullptr;
	if (!SourceOwner || !TargetOwner || !SourceOwner->HasAuthority() || !TargetOwner->HasAuthority())
	{
		Result.Code = ERpgInventoryMutationResultCode::AuthorityRequired;
		return CacheResult(MoveTemp(Result));
	}
	if (!TargetInventory || TargetInventory == this ||
		(Request.Operation != ERpgInventoryMutationOperation::Transfer &&
			Request.Operation != ERpgInventoryMutationOperation::Pickup &&
			Request.Operation != ERpgInventoryMutationOperation::Drop) ||
		!Request.Source.IsValid() || !Request.Target.IsValid())
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return CacheResult(MoveTemp(Result));
	}

	const FRpgInventoryEntry* SourceEntry = InventoryList.FindEntryByItemId(Request.ItemId);
	if (!SourceEntry || !SourceEntry->Instance)
	{
		Result.Code = ERpgInventoryMutationResultCode::ItemNotFound;
		return CacheResult(MoveTemp(Result));
	}
	if (SourceEntry->Placement.GetContainerHandle() != Request.Source)
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return CacheResult(MoveTemp(Result));
	}
	if ((Request.ExpectedEntryId.IsValid() &&
			Request.ExpectedEntryId != SourceEntry->EntryId) ||
		(Request.ExpectedSourcePlacement.IsValid() &&
			Request.ExpectedSourcePlacement != SourceEntry->Placement))
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return CacheResult(MoveTemp(Result));
	}
	if (TargetInventory->FindItemById(Request.ItemId))
	{
		Result.Code = ERpgInventoryMutationResultCode::DuplicateItemId;
		return CacheResult(MoveTemp(Result));
	}

	const int32 RequestedQuantity = Request.Quantity <= 0 ? SourceEntry->StackCount : Request.Quantity;
	if (RequestedQuantity <= 0 || RequestedQuantity > SourceEntry->StackCount)
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return CacheResult(MoveTemp(Result));
	}
	Result.RequestedQuantity = RequestedQuantity;

	FRpgInventoryGraphSaveData SourceBefore = ExportInventoryGraph();
	FRpgInventoryGraphSaveData TargetBefore = TargetInventory->ExportInventoryGraph();
	if (SourceBefore.Items.Num() != InventoryList.Entries.Num() ||
		TargetBefore.Items.Num() !=
			TargetInventory->InventoryList.Entries.Num())
	{
		// Export uses an empty item array as its fail-closed signal. Never confuse a failed
		// non-empty export with an actually empty graph and overwrite either inventory.
		Result.Code = ERpgInventoryMutationResultCode::InternalError;
		return CacheResult(MoveTemp(Result));
	}
	FRpgInventoryGraphSaveData SourceAfter = SourceBefore;
	FRpgInventoryGraphSaveData TargetAfter = TargetBefore;
	FRpgInventorySavedItem* SourceSavedItem = SourceAfter.Items.FindByPredicate(
		[&Request](const FRpgInventorySavedItem& Saved)
		{
			return Saved.ItemId == Request.ItemId;
		});
	if (!SourceSavedItem)
	{
		Result.Code = ERpgInventoryMutationResultCode::InternalError;
		return CacheResult(MoveTemp(Result));
	}

	auto IsDescendantOf = [&SourceAfter](const FRpgInventorySavedItem& Candidate, const FRpgInventoryItemId& AncestorId)
	{
		FRpgInventoryContainerHandle Handle = Candidate.Container;
		for (int32 Guard = 0; Guard <= SourceAfter.Items.Num() && Handle.IsItemOwned(); ++Guard)
		{
			if (Handle.ItemOwnerId == AncestorId)
			{
				return true;
			}
			const FRpgInventorySavedItem* Parent = SourceAfter.Items.FindByPredicate(
				[&Handle](const FRpgInventorySavedItem& Saved)
				{
					return Saved.ItemId == Handle.ItemOwnerId;
				});
			Handle = Parent ? Parent->Container : FRpgInventoryContainerHandle();
		}
		return false;
	};

	TArray<FRpgInventoryItemId> SubtreeIds;
	SubtreeIds.Add(Request.ItemId);
	for (const FRpgInventorySavedItem& Candidate : SourceAfter.Items)
	{
		if (Candidate.ItemId != Request.ItemId && IsDescendantOf(Candidate, Request.ItemId))
		{
			SubtreeIds.Add(Candidate.ItemId);
		}
	}
	const bool bTransfersSubtree = SubtreeIds.Num() > 1 || SourceEntry->Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;
	if (bTransfersSubtree && RequestedQuantity != SourceEntry->StackCount)
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return CacheResult(MoveTemp(Result));
	}

	TArray<URpgInventoryManagerComponent*> TargetOwnerInventories;
	TargetOwner->GetComponents(TargetOwnerInventories);
	for (const FRpgInventoryItemId& IncomingItemId : SubtreeIds)
	{
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
			Result.Code =
				ERpgInventoryMutationResultCode::DuplicateItemId;
			return CacheResult(MoveTemp(Result));
		}
	}

	auto FindFreePlacementInTarget = [&](int32 StackQuantity, FRpgInventoryGridPlacement& OutPlacement)
	{
		FRpgInventoryGridSize GridSize;
		if (!TargetInventory->GetGridSizeForContainerHandle(Request.Target, GridSize))
		{
			return false;
		}

		const bool bHasExactPlacement = Request.TargetPlacement.IsValid();
		const int32 RotationCount = CanInventoryManagerRotateDefinition(SourceEntry->Instance->GetItemDef()) ? 2 : 1;
		for (int32 RotationIndex = 0; RotationIndex < RotationCount; ++RotationIndex)
		{
			FRpgInventoryGridPlacement Candidate;
			Candidate.SetContainerHandle(Request.Target);
			Candidate.Width = GetInventoryManagerFootprintForDefinition(SourceEntry->Instance->GetItemDef(), false).Width;
			Candidate.Height = GetInventoryManagerFootprintForDefinition(SourceEntry->Instance->GetItemDef(), false).Height;
			Candidate.bRotated = bHasExactPlacement ? Request.TargetPlacement.bRotated : RotationIndex == 1;
			const FRpgInventoryGridSize OccupiedSize = Candidate.GetOccupiedSize();
			const int32 StartX = bHasExactPlacement ? Request.TargetPlacement.X : 0;
			const int32 StartY = bHasExactPlacement ? Request.TargetPlacement.Y : 0;
			const int32 EndX = bHasExactPlacement ? StartX : GridSize.Width - OccupiedSize.Width;
			const int32 EndY = bHasExactPlacement ? StartY : GridSize.Height - OccupiedSize.Height;
			for (int32 Y = StartY; Y <= EndY; ++Y)
			{
				for (int32 X = StartX; X <= EndX; ++X)
				{
					Candidate.X = X;
					Candidate.Y = Y;
					if (TargetInventory->CanReceiveTransferredItemInstanceToPlacement(
						SourceEntry->Instance,
						StackQuantity,
						Candidate))
					{
						OutPlacement = Candidate;
						return true;
					}
				}
			}
			if (bHasExactPlacement)
			{
				break;
			}
		}
		return false;
	};

	int32 AppliedQuantity = 0;
	if (bTransfersSubtree)
	{
		FRpgInventoryGridPlacement TargetPlacement;
		if (!FindFreePlacementInTarget(RequestedQuantity, TargetPlacement))
		{
			Result.Code = ERpgInventoryMutationResultCode::NoSpace;
			return CacheResult(MoveTemp(Result));
		}

		const int32 DepthDelta = static_cast<int32>(TargetPlacement.GetContainerHandle().Depth) -
			static_cast<int32>(SourceSavedItem->Container.Depth);
		for (const FRpgInventoryItemId& SubtreeId : SubtreeIds)
		{
			FRpgInventorySavedItem* MovingSavedItem = SourceAfter.Items.FindByPredicate(
				[&SubtreeId](const FRpgInventorySavedItem& Saved)
				{
					return Saved.ItemId == SubtreeId;
				});
			if (!MovingSavedItem)
			{
				Result.Code = ERpgInventoryMutationResultCode::InternalError;
				return CacheResult(MoveTemp(Result));
			}

			const FRpgInventorySavedItem SourceSavedItemBeforeMove =
				*MovingSavedItem;
			FRpgInventorySavedItem TargetSavedItem =
				SourceSavedItemBeforeMove;
			if (SubtreeId == Request.ItemId)
			{
				TargetSavedItem.Container = TargetPlacement.GetContainerHandle();
				TargetSavedItem.Placement = TargetPlacement;
			}
			else
			{
				const int32 NewDepth = static_cast<int32>(TargetSavedItem.Container.Depth) + DepthDelta;
				if (NewDepth <= 0 || NewDepth > RpgInventoryMaxItemOwnedDepth)
				{
					Result.Code = ERpgInventoryMutationResultCode::MaxDepthExceeded;
					return CacheResult(MoveTemp(Result));
				}
				TargetSavedItem.Container.Depth = static_cast<uint8>(NewDepth);
				TargetSavedItem.Placement.SetContainerHandle(TargetSavedItem.Container);
			}

			FRpgInventoryMutationDelta& Delta =
				Result.Deltas.AddDefaulted_GetRef();
			Delta.Kind = ERpgInventoryMutationDeltaKind::Moved;
			Delta.ItemId = SubtreeId;
			Delta.BeforeContainer = SourceSavedItemBeforeMove.Container;
			Delta.AfterContainer = TargetSavedItem.Container;
			Delta.BeforePlacement = SourceSavedItemBeforeMove.Placement;
			Delta.AfterPlacement = TargetSavedItem.Placement;
			Delta.PreviousQuantity = SourceSavedItemBeforeMove.StackCount;
			Delta.NewQuantity = SourceSavedItemBeforeMove.StackCount;
			TargetAfter.Items.Add(MoveTemp(TargetSavedItem));
		}

		SourceAfter.Items.RemoveAll([&SubtreeIds](const FRpgInventorySavedItem& Saved)
		{
			return SubtreeIds.Contains(Saved.ItemId);
		});
		AppliedQuantity = RequestedQuantity;

	}
	else
	{
		int32 RemainingQuantity = RequestedQuantity;
		const bool bExactTarget = Request.TargetPlacement.IsValid();
		for (const FRpgInventoryEntryView& TargetEntry : TargetInventory->GetAllEntries())
		{
			if (RemainingQuantity <= 0 || !TargetEntry.Instance ||
				TargetEntry.Placement.GetContainerHandle() != Request.Target ||
				!SourceEntry->Instance->IsStackCompatibleWith(TargetEntry.Instance))
			{
				continue;
			}
			if (bExactTarget && !TargetEntry.Placement.ContainsCell(Request.TargetPlacement.X, Request.TargetPlacement.Y))
			{
				continue;
			}

			FRpgInventorySavedItem* TargetSavedItem = TargetAfter.Items.FindByPredicate(
				[&TargetEntry](const FRpgInventorySavedItem& Saved)
				{
					return Saved.ItemId == TargetEntry.ItemId;
				});
			const int32 FreeCapacity = TargetInventory->GetFreeStackCapacity(TargetEntry.Instance);
			if (!TargetSavedItem || FreeCapacity <= 0)
			{
				continue;
			}

			const int32 MergeQuantity = FMath::Min(RemainingQuantity, FreeCapacity);
			TargetSavedItem->StackCount += MergeQuantity;
			RemainingQuantity -= MergeQuantity;
			AppliedQuantity += MergeQuantity;

			FRpgInventoryMutationDelta& Delta = Result.Deltas.AddDefaulted_GetRef();
			Delta.Kind = ERpgInventoryMutationDeltaKind::StackChanged;
			Delta.ItemId = TargetEntry.ItemId;
			Delta.BeforeContainer = TargetEntry.Placement.GetContainerHandle();
			Delta.AfterContainer = TargetEntry.Placement.GetContainerHandle();
			Delta.BeforePlacement = TargetEntry.Placement;
			Delta.AfterPlacement = TargetEntry.Placement;
			Delta.PreviousQuantity = TargetEntry.StackCount;
			Delta.NewQuantity = TargetEntry.StackCount + MergeQuantity;
			if (bExactTarget)
			{
				break;
			}
		}

		if (RemainingQuantity > 0)
		{
			FRpgInventoryGridPlacement TargetPlacement;
			if (FindFreePlacementInTarget(RemainingQuantity, TargetPlacement))
			{
				FRpgInventorySavedItem NewTargetItem = *SourceSavedItem;
				const bool bPreserveSourceIdentity = AppliedQuantity == 0 && RemainingQuantity == SourceEntry->StackCount;
				NewTargetItem.ItemId = bPreserveSourceIdentity ? Request.ItemId : FRpgInventoryItemId::NewId();
				NewTargetItem.StackCount = RemainingQuantity;
				NewTargetItem.Container = TargetPlacement.GetContainerHandle();
				NewTargetItem.Placement = TargetPlacement;
				TargetAfter.Items.Add(NewTargetItem);

				FRpgInventoryMutationDelta& Delta = Result.Deltas.AddDefaulted_GetRef();
				Delta.Kind = ERpgInventoryMutationDeltaKind::Added;
				Delta.ItemId = NewTargetItem.ItemId;
				Delta.AfterContainer = NewTargetItem.Container;
				Delta.AfterPlacement = NewTargetItem.Placement;
				Delta.NewQuantity = RemainingQuantity;
				AppliedQuantity += RemainingQuantity;
				RemainingQuantity = 0;
			}
		}

		if (RemainingQuantity > 0 && !bAllowPartialStackPickup)
		{
			Result.Code = ERpgInventoryMutationResultCode::NoSpace;
			Result.Deltas.Reset();
			return CacheResult(MoveTemp(Result));
		}
		if (AppliedQuantity <= 0)
		{
			Result.Code = ERpgInventoryMutationResultCode::NoSpace;
			return CacheResult(MoveTemp(Result));
		}

		SourceSavedItem = SourceAfter.Items.FindByPredicate(
			[&Request](const FRpgInventorySavedItem& Saved)
			{
				return Saved.ItemId == Request.ItemId;
			});
		if (!SourceSavedItem)
		{
			Result.Code = ERpgInventoryMutationResultCode::InternalError;
			return CacheResult(MoveTemp(Result));
		}
		const int32 SourcePreviousQuantity = SourceSavedItem->StackCount;
		SourceSavedItem->StackCount -= AppliedQuantity;
		if (SourceSavedItem->StackCount <= 0)
		{
			SourceAfter.Items.RemoveAll([&Request](const FRpgInventorySavedItem& Saved)
			{
				return Saved.ItemId == Request.ItemId;
			});
		}

		FRpgInventoryMutationDelta& SourceDelta = Result.Deltas.AddDefaulted_GetRef();
		SourceDelta.Kind = AppliedQuantity == SourcePreviousQuantity
			? ERpgInventoryMutationDeltaKind::Removed
			: ERpgInventoryMutationDeltaKind::StackChanged;
		SourceDelta.ItemId = Request.ItemId;
		SourceDelta.BeforeContainer = Request.Source;
		SourceDelta.BeforePlacement = SourceEntry->Placement;
		SourceDelta.PreviousQuantity = SourcePreviousQuantity;
		SourceDelta.NewQuantity = SourcePreviousQuantity - AppliedQuantity;
		if (SourceDelta.NewQuantity > 0)
		{
			SourceDelta.AfterContainer = Request.Source;
			SourceDelta.AfterPlacement = SourceEntry->Placement;
		}
	}

	FRpgInventoryMutationResult TargetImportResult;
	if (!TargetInventory->ImportInventoryGraphInternal(
			TargetAfter,
			TargetImportResult,
			this,
			false,
			false))
	{
		Result.Code = TargetImportResult.Code;
		Result.Deltas.Reset();
		return CacheResult(MoveTemp(Result));
	}

	FRpgInventoryMutationResult SourceImportResult;
	if (!ImportInventoryGraphInternal(
			SourceAfter,
			SourceImportResult,
			nullptr,
			true,
			false))
	{
		FRpgInventoryMutationResult RollbackResult;
		TargetInventory->ImportInventoryGraphInternal(
			TargetBefore,
			RollbackResult,
			this,
			false,
			false);
		Result.Code = SourceImportResult.Code;
		Result.Deltas.Reset();
		return CacheResult(MoveTemp(Result));
	}

	Result.AppliedQuantity = AppliedQuantity;
	Result.Code = AppliedQuantity == RequestedQuantity
		? ERpgInventoryMutationResultCode::Success
		: ERpgInventoryMutationResultCode::PartiallyApplied;
	return CacheResult(MoveTemp(Result));
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
				AddReplicatedSubObject(Entry.Instance, ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
			}
		}
	}
}

FRpgInventoryGraphSaveData URpgInventoryManagerComponent::ExportInventoryGraph() const
{
	FRpgInventoryGraphSaveData SaveData;
	SaveData.Items.Reserve(InventoryList.Entries.Num());
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		if (!Entry.Instance || Entry.StackCount <= 0 || !Entry.Instance->GetItemId().IsValid())
		{
			SaveData.Items.Reset();
			return SaveData;
		}

		FRpgInventorySavedItem& SavedItem = SaveData.Items.AddDefaulted_GetRef();
		SavedItem.ItemId = Entry.Instance->GetItemId();
		SavedItem.ItemDefinition = TSoftClassPtr<URpgInventoryItemDefinition>(Entry.Instance->GetItemDef());
		SavedItem.StackCount = Entry.StackCount;
		SavedItem.Container = Entry.Placement.GetContainerHandle();
		SavedItem.Placement = Entry.Placement;
		SavedItem.Placement.SetContainerHandle(SavedItem.Container);
		if (!Entry.Instance->ExportRuntimeState(SavedItem.RuntimeState))
		{
			UE_LOG(LogRpgInventoryManager, Error, TEXT("Graph export rejected item %s because its runtime state could not be serialized."),
				*Entry.Instance->GetItemId().ToString());
			SaveData.Items.Reset();
			return SaveData;
		}
	}

	return SaveData;
}

bool URpgInventoryManagerComponent::RestoreInventoryGraph(
	const FRpgInventoryGraphSaveData& SaveData,
	FRpgInventoryMutationResult& OutResult)
{
	const bool bRestored = ImportInventoryGraphInternal(
		SaveData,
		OutResult,
		nullptr,
		false,
		true);
	OutResult.Operation = ERpgInventoryMutationOperation::Restore;
	return bRestored;
}

bool URpgInventoryManagerComponent::ImportInventoryGraph(
	const FRpgInventoryGraphSaveData& SaveData,
	FRpgInventoryMutationResult& OutResult)
{
	// Compatibility/runtime-recovery callers retain the legacy import result
	// semantic. Disk/profile reconstruction must use RestoreInventoryGraph.
	return ImportInventoryGraphInternal(
		SaveData,
		OutResult,
		nullptr,
		false,
		false);
}

bool URpgInventoryManagerComponent::ImportInventoryGraphInternal(
	const FRpgInventoryGraphSaveData& SaveData,
	FRpgInventoryMutationResult& OutResult,
	const URpgInventoryManagerComponent* AllowedSourceInventory,
	bool bAllowOverCapacityReduction,
	bool bEstablishNewMutationEpoch)
{
	OutResult = FRpgInventoryMutationResult();
	OutResult.RequestId = FGuid::NewGuid();
	OutResult.Operation = ERpgInventoryMutationOperation::Transfer;
	OutResult.RequestedQuantity = SaveData.Items.Num();

	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		OutResult.Code = ERpgInventoryMutationResultCode::AuthorityRequired;
		return false;
	}

	if (SaveData.SchemaVersion != FRpgInventoryGraphSaveData::CurrentSchemaVersion)
	{
		OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return false;
	}
	if (!IsCapacityUnlimited() &&
		SaveData.Items.Num() > GetMaxEntries() &&
		(!bAllowOverCapacityReduction ||
			SaveData.Items.Num() > InventoryList.Entries.Num()))
	{
		OutResult.Code = ERpgInventoryMutationResultCode::NoSpace;
		return false;
	}

	TArray<URpgInventoryManagerComponent*> SiblingInventories;
	OwningActor->GetComponents(SiblingInventories);

	struct FStagedSavedEntry
	{
		const FRpgInventorySavedItem* Saved = nullptr;
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
		TObjectPtr<URpgInventoryItemInstance> Instance;
		TObjectPtr<URpgInventoryItemInstance> CommittedInstance;
		FGuid CommittedEntryId;
		FRpgInventoryGridPlacement Placement;
	};

	TArray<FStagedSavedEntry> Staged;
	Staged.Reserve(SaveData.Items.Num());
	TMap<FRpgInventoryItemId, int32> StagedIndexById;
	for (const FRpgInventorySavedItem& SavedItem : SaveData.Items)
	{
		if (!SavedItem.ItemId.IsValid() || StagedIndexById.Contains(SavedItem.ItemId) || SavedItem.StackCount <= 0 ||
			!SavedItem.Container.IsValid() || !SavedItem.Placement.IsValid())
		{
			OutResult.Code = StagedIndexById.Contains(SavedItem.ItemId)
				? ERpgInventoryMutationResultCode::DuplicateItemId
				: ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		if (SavedItem.Placement.GetContainerHandle() !=
			SavedItem.Container)
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}

		const bool bConflictsWithActorSibling =
			SiblingInventories.ContainsByPredicate(
				[this, AllowedSourceInventory, &SavedItem](
					const URpgInventoryManagerComponent*
						CandidateInventory)
				{
					return CandidateInventory &&
						CandidateInventory != this &&
						CandidateInventory != AllowedSourceInventory &&
						CandidateInventory->FindItemById(
							SavedItem.ItemId) != nullptr;
				});
		if (bConflictsWithActorSibling)
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::DuplicateItemId;
			return false;
		}

		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = SavedItem.ItemDefinition.LoadSynchronous();
		if (!ItemDefinition || SavedItem.StackCount > GetInventoryManagerMaxStackSizeForDefinition(ItemDefinition))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::StackLimitReached;
			return false;
		}

		const bool bUsesSingleCellRootPlacement =
			SavedItem.Container.IsRoot() &&
			ShouldUseSingleCellPlacementForContainer(
				SavedItem.Container.ContainerId);
		const FRpgInventoryGridSize CurrentFootprint =
			bUsesSingleCellRootPlacement
				? FRpgInventoryGridSize()
				: GetInventoryManagerFootprintForDefinition(
					ItemDefinition,
					false);
		const bool bRotationAllowed =
			!bUsesSingleCellRootPlacement &&
			CanInventoryManagerRotateDefinition(ItemDefinition);
		if (!CurrentFootprint.IsValid() ||
			SavedItem.Placement.Width != CurrentFootprint.Width ||
			SavedItem.Placement.Height != CurrentFootprint.Height ||
			(SavedItem.Placement.bRotated && !bRotationAllowed))
		{
			// Current-schema data must already match the live definition contract.
			// Silently accepting an old footprint can overlap another entry after a
			// definition change; schema migration must rewrite it explicitly.
			OutResult.Code =
				ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}

		FStagedSavedEntry& Stage = Staged.AddDefaulted_GetRef();
		Stage.Saved = &SavedItem;
		Stage.ItemDefinition = ItemDefinition;
		Stage.Placement = SavedItem.Placement;
		Stage.Placement.SetContainerHandle(SavedItem.Container);
		Stage.Instance = NewObject<URpgInventoryItemInstance>(OwningActor);
		Stage.CommittedInstance = Stage.Instance;
		Stage.CommittedEntryId = FGuid::NewGuid();
		Stage.Instance->SetItemDef(ItemDefinition);
		if (!Stage.Instance->RestoreItemId(SavedItem.ItemId))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}

		for (URpgInventoryItemFragment* Fragment : GetDefault<URpgInventoryItemDefinition>(ItemDefinition)->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnInstanceCreated(Stage.Instance);
			}
		}
		if (!Stage.Instance->ImportRuntimeState(SavedItem.RuntimeState))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}

		StagedIndexById.Add(SavedItem.ItemId, Staged.Num() - 1);
	}

	TFunction<bool(int32, TSet<FRpgInventoryItemId>&)> ValidateAncestry;
	ValidateAncestry = [&](int32 EntryIndex, TSet<FRpgInventoryItemId>& Visiting) -> bool
	{
		const FStagedSavedEntry& Stage = Staged[EntryIndex];
		const FRpgInventoryContainerHandle Handle = Stage.Placement.GetContainerHandle();
		if (Handle.IsRoot())
		{
			return Handle.Depth == 0;
		}

		if (Visiting.Contains(Stage.Saved->ItemId) || Handle.ItemOwnerId == Stage.Saved->ItemId)
		{
			OutResult.Code = ERpgInventoryMutationResultCode::CycleDetected;
			return false;
		}

		const int32* OwnerIndex = StagedIndexById.Find(Handle.ItemOwnerId);
		if (!OwnerIndex)
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}

		Visiting.Add(Stage.Saved->ItemId);
		if (!ValidateAncestry(*OwnerIndex, Visiting))
		{
			return false;
		}
		Visiting.Remove(Stage.Saved->ItemId);

		const uint8 ExpectedDepth = Staged[*OwnerIndex].Placement.GetContainerHandle().GetDirectChildDepth();
		if (ExpectedDepth == 0 || ExpectedDepth != Handle.Depth)
		{
			OutResult.Code = ERpgInventoryMutationResultCode::MaxDepthExceeded;
			return false;
		}
		return true;
	};

	TMap<FRpgInventoryContainerHandle, TArray<FRpgInventoryGridPlacement>> Occupancy;
	for (int32 EntryIndex = 0; EntryIndex < Staged.Num(); ++EntryIndex)
	{
		FStagedSavedEntry& Stage = Staged[EntryIndex];
		TSet<FRpgInventoryItemId> Visiting;
		if (!ValidateAncestry(EntryIndex, Visiting))
		{
			if (OutResult.Code == ERpgInventoryMutationResultCode::InvalidRequest)
			{
				OutResult.Code = ERpgInventoryMutationResultCode::InvalidContainer;
			}
			return false;
		}

		FRpgInventoryGridSize GridSize;
		const FRpgInventoryContainerHandle Handle = Stage.Placement.GetContainerHandle();
		if (Handle.IsRoot())
		{
			if (!GetGridSizeForContainer(Handle.Root, GridSize))
			{
				OutResult.Code = ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}

			if (const URpgPlayerInventoryLayoutComponent* Layout = FindOwningPlayerInventoryLayout())
			{
				FRpgInventorySlotAddress Address;
				Address.SetContainerHandle(Handle);
				Address.X = Stage.Placement.X;
				Address.Y = Stage.Placement.Y;
				if (!Layout->CanItemUseSlotAddress(Stage.Instance, Address))
				{
					OutResult.Code = ERpgInventoryMutationResultCode::ItemNotAllowed;
					return false;
				}
			}
		}
		else
		{
			const int32* OwnerIndex = StagedIndexById.Find(Handle.ItemOwnerId);
			const URpgInventoryFragment_ItemContainer* Fragment = OwnerIndex
				? Staged[*OwnerIndex].Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()
				: nullptr;
			TArray<FRpgInventoryItemContainerDefinition> Definitions;
			if (Fragment)
			{
				Fragment->GetProvidedContainers(Definitions);
			}
			const FRpgInventoryItemContainerDefinition* Definition = Definitions.FindByPredicate(
				[&Handle](const FRpgInventoryItemContainerDefinition& Candidate)
				{
					return Candidate.ContainerId == Handle.ContainerId && Candidate.IsValid();
				});
			if (!Definition || !Definition->AllowsItemDefinition(Stage.ItemDefinition, Handle.Depth))
			{
				OutResult.Code = ERpgInventoryMutationResultCode::ItemNotAllowed;
				return false;
			}
			GridSize = Definition->GridSize;
		}

		const FRpgInventoryGridSize OccupiedSize = Stage.Placement.GetOccupiedSize();
		if (!GridSize.IsValid() || Stage.Placement.X < 0 || Stage.Placement.Y < 0 ||
			Stage.Placement.X + OccupiedSize.Width > GridSize.Width ||
			Stage.Placement.Y + OccupiedSize.Height > GridSize.Height)
		{
			OutResult.Code = ERpgInventoryMutationResultCode::OutOfBounds;
			return false;
		}

		TArray<FRpgInventoryGridPlacement>& ContainerOccupancy = Occupancy.FindOrAdd(Handle);
		if (ContainerOccupancy.ContainsByPredicate([&Stage](const FRpgInventoryGridPlacement& Existing)
			{
				return Existing.Overlaps(Stage.Placement);
			}))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::Occupied;
			return false;
		}
		ContainerOccupancy.Add(Stage.Placement);
	}

	// A validated graph import may only change placement, stack count, or fragment state for an item
	// that already belongs to this inventory. Preserve that item's runtime and replicated entry identity
	// so loadouts, UI payloads, and other gameplay references do not become stale after an atomic commit.
	struct FExistingRuntimeEntry
	{
		TObjectPtr<URpgInventoryItemInstance> Instance;
		FGuid EntryId;
	};

	TMap<FRpgInventoryItemId, FExistingRuntimeEntry> ExistingEntriesByItemId;
	ExistingEntriesByItemId.Reserve(InventoryList.Entries.Num());
	for (const FRpgInventoryEntry& ExistingEntry : InventoryList.Entries)
	{
		if (ExistingEntry.Instance && ExistingEntry.Instance->GetItemId().IsValid())
		{
			FExistingRuntimeEntry& ExistingRuntimeEntry =
				ExistingEntriesByItemId.Add(ExistingEntry.Instance->GetItemId());
			ExistingRuntimeEntry.Instance = ExistingEntry.Instance;
			ExistingRuntimeEntry.EntryId = ExistingEntry.EntryId;
		}
	}

	struct FRuntimeStateBackup
	{
		TObjectPtr<URpgInventoryItemInstance> Instance;
		TArray<FRpgInventoryFragmentStatePayload> RuntimeState;
	};

	TArray<FRuntimeStateBackup> RuntimeStateBackups;
	RuntimeStateBackups.Reserve(Staged.Num());
	auto RestoreReusedRuntimeState = [&RuntimeStateBackups]()
	{
		for (const FRuntimeStateBackup& Backup : RuntimeStateBackups)
		{
			if (Backup.Instance && !Backup.Instance->ImportRuntimeState(Backup.RuntimeState))
			{
				UE_LOG(LogRpgInventoryManager, Error,
					TEXT("Failed to restore runtime state for reused inventory item %s after an import commit failure."),
					*Backup.Instance->GetItemId().ToString());
			}
		}
	};

	for (FStagedSavedEntry& Stage : Staged)
	{
		const FExistingRuntimeEntry* ExistingEntry =
			ExistingEntriesByItemId.Find(Stage.Saved->ItemId);
		if (!ExistingEntry || !ExistingEntry->Instance ||
			ExistingEntry->Instance->GetItemDef() != Stage.ItemDefinition)
		{
			continue;
		}

		FRuntimeStateBackup Backup;
		Backup.Instance = ExistingEntry->Instance;
		if (!Backup.Instance->ExportRuntimeState(Backup.RuntimeState))
		{
			RestoreReusedRuntimeState();
			OutResult.Code = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		RuntimeStateBackups.Add(MoveTemp(Backup));

		if (!ExistingEntry->Instance->ImportRuntimeState(Stage.Saved->RuntimeState))
		{
			RestoreReusedRuntimeState();
			OutResult.Code = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		Stage.CommittedInstance = ExistingEntry->Instance;
		Stage.CommittedEntryId = ExistingEntry->EntryId.IsValid()
			? ExistingEntry->EntryId
			: FGuid::NewGuid();
	}

	// A successful disk/profile restore starts a new command epoch before any
	// committed graph is observable. Runtime transfer imports deliberately retain
	// their caches so normal request retries remain exactly-once.
	if (bEstablishNewMutationEpoch)
	{
		RecentMutationResults.Reset();
		RecentMutationOrder.Reset();
		++MutationEpoch;
		if (MutationEpoch == 0)
		{
			// Keep zero as the initial/default value so a theoretical wrap can
			// never make a pre-restore cache record look current again.
			++MutationEpoch;
		}
	}

	// Preserve stable notification payloads before replacing the array. No gameplay
	// callback may run while Entries is being traversed, reset, or rebuilt.
	TArray<FRpgInventoryEntry> PreviousEntries =
		InventoryList.Entries;
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRpgInventoryEntry& ExistingEntry : PreviousEntries)
		{
			if (ExistingEntry.Instance)
			{
				RemoveReplicatedSubObject(ExistingEntry.Instance);
			}
		}
	}

	InventoryList.Entries.Reset(Staged.Num());
	for (const FStagedSavedEntry& Stage : Staged)
	{
		FRpgInventoryEntry& NewEntry = InventoryList.Entries.AddDefaulted_GetRef();
		NewEntry.Instance = Stage.CommittedInstance;
		NewEntry.EntryId = Stage.CommittedEntryId;
		NewEntry.StackCount = Stage.Saved->StackCount;
		NewEntry.Placement = Stage.Placement;
		InventoryList.MarkItemDirty(NewEntry);
	}
	InventoryList.SortEntriesByPlacement();
	InventoryList.MarkArrayDirty();

	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		for (const FRpgInventoryEntry& NewEntry : InventoryList.Entries)
		{
			AddReplicatedSubObject(NewEntry.Instance, ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
		}
	}

	TArray<FRpgInventoryEntry> CommittedEntries =
		InventoryList.Entries;
	OutResult.Code = ERpgInventoryMutationResultCode::Success;
	OutResult.AppliedQuantity = Staged.Num();
	MarkInventoryStateDirty();

	// Both sets of callbacks now observe the complete committed graph. Copies keep iteration
	// valid even if a synchronous listener performs another inventory mutation.
	for (FRpgInventoryEntry& PreviousEntry : PreviousEntries)
	{
		InventoryList.BroadcastChangeMessage(
			PreviousEntry,
			PreviousEntry.StackCount,
			0,
			true);
	}
	for (FRpgInventoryEntry& CommittedEntry : CommittedEntries)
	{
		InventoryList.BroadcastChangeMessage(
			CommittedEntry,
			0,
			CommittedEntry.StackCount,
			true);
	}
	return true;
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

bool URpgInventoryManagerComponent::GetGridSizeForContainerHandle(FRpgInventoryContainerHandle ContainerHandle, FRpgInventoryGridSize& OutGridSize) const
{
	OutGridSize = FRpgInventoryGridSize();
	if (!ContainerHandle.IsValid())
	{
		return false;
	}

	if (ContainerHandle.IsRoot())
	{
		return GetGridSizeForContainer(ContainerHandle.Root, OutGridSize);
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
		OutCode = Entry.Instance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()
			? ERpgInventoryMutationResultCode::NestedContainersNotAllowed
			: ERpgInventoryMutationResultCode::ItemNotAllowed;
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

bool URpgInventoryManagerComponent::ShouldUseSingleCellPlacementForContainer(FName ContainerId) const
{
	const FName ResolvedContainerId = ContainerId.IsNone() ? DefaultContainerId : ContainerId;
	if (ResolvedContainerId.IsNone())
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
		if (Group.ContainerId == ResolvedContainerId)
		{
			return Group.GroupKind == ERpgInventorySlotGroupKind::Gear ||
				Group.GroupKind == ERpgInventorySlotGroupKind::Carry;
		}
	}

	return false;
}

bool URpgInventoryManagerComponent::TryMakePlacementForItemDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDef,
	FName ContainerId,
	int32 X,
	int32 Y,
	bool bRotated,
	FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!ItemDef)
	{
		return false;
	}

	const FName ResolvedContainerId = ContainerId.IsNone() ? DefaultContainerId : ContainerId;
	if (ResolvedContainerId.IsNone())
	{
		return false;
	}

	OutPlacement.ContainerId = ResolvedContainerId;
	OutPlacement.X = X;
	OutPlacement.Y = Y;

	if (ShouldUseSingleCellPlacementForContainer(ResolvedContainerId))
	{
		OutPlacement.Width = 1;
		OutPlacement.Height = 1;
		OutPlacement.bRotated = false;
		return true;
	}

	if (bRotated && !CanInventoryManagerRotateDefinition(ItemDef))
	{
		OutPlacement = FRpgInventoryGridPlacement();
		return false;
	}

	const FRpgInventoryGridSize Footprint = GetInventoryManagerFootprintForDefinition(ItemDef, false);
	if (!Footprint.IsValid())
	{
		OutPlacement = FRpgInventoryGridPlacement();
		return false;
	}

	OutPlacement.Width = Footprint.Width;
	OutPlacement.Height = Footprint.Height;
	OutPlacement.bRotated = bRotated;
	return true;
}

bool URpgInventoryManagerComponent::TryMakePlacementForItemInstance(
	URpgInventoryItemInstance* ItemInstance,
	FName ContainerId,
	int32 X,
	int32 Y,
	bool bRotated,
	FRpgInventoryGridPlacement& OutPlacement) const
{
	return TryMakePlacementForItemDefinition(ItemInstance ? ItemInstance->GetItemDef() : nullptr, ContainerId, X, Y, bRotated, OutPlacement);
}

FRpgInventoryGridPlacement URpgInventoryManagerComponent::MakePlacementForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, FName ContainerId, int32 X, int32 Y, bool bRotated) const
{
	FRpgInventoryGridPlacement Placement;
	TryMakePlacementForItemDefinition(ItemDef, ContainerId, X, Y, bRotated, Placement);
	return Placement;
}

FRpgInventoryGridPlacement URpgInventoryManagerComponent::MakePlacementForItemInstance(URpgInventoryItemInstance* ItemInstance, FName ContainerId, int32 X, int32 Y, bool bRotated) const
{
	FRpgInventoryGridPlacement Placement;
	TryMakePlacementForItemInstance(ItemInstance, ContainerId, X, Y, bRotated, Placement);
	return Placement;
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


