// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "Templates/Greater.h"
#include "UObject/UObjectGlobals.h"

namespace RpgInventoryManagerTransactionsPrivate
{
	int32 GetTransactionsInventoryMaxStackSizeForDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		return URpgInventoryManagerComponent::
			GetEffectiveMaxStackSizeForDefinition(ItemDef);
	}
}

using RpgInventoryManagerTransactionsPrivate::
	GetTransactionsInventoryMaxStackSizeForDefinition;

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

	if (StackCount > GetTransactionsInventoryMaxStackSizeForDefinition(
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
