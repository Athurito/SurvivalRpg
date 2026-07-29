// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "Templates/Greater.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace RpgInventoryManagerTransactionsPrivate
{
	constexpr int32 TransactionsMaxRecentMutationResults = 64;

	int32 GetTransactionsInventoryMaxStackSizeForDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		return URpgInventoryManagerComponent::
			GetEffectiveMaxStackSizeForDefinition(ItemDef);
	}

	FRpgInventoryMutationResult MakeTransactionsRejectedIntentResult(
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

	bool IsTransactionsCompleteSourceSnapshot(
		const FRpgInventoryItemId& ItemId,
		const FGuid& ExpectedEntryId,
		const FRpgInventoryGridPlacement& ExpectedSourcePlacement)
	{
		return ItemId.IsValid() &&
			ExpectedEntryId.IsValid() &&
			ExpectedSourcePlacement.ContainerHandle.IsValid() &&
			ExpectedSourcePlacement.IsValid();
	}

	bool IsTransactionsCompletelyUnsetPlacement(
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

	bool AreTransactionsPlacementSnapshotsExactlyEqual(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool IsTransactionsItemOwnedHandleDepthOverflow(
		const FRpgInventoryContainerHandle& Handle)
	{
		return Handle.Root.IsNone() && Handle.ItemOwnerId.IsValid() &&
			!Handle.ContainerId.IsNone() &&
			Handle.Depth > RpgInventoryMaxItemOwnedDepth;
	}

	bool AreTransactionsInventoryRuntimeStatesExactlyEqual(
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
}

using RpgInventoryManagerTransactionsPrivate::
	AreTransactionsInventoryRuntimeStatesExactlyEqual;
using RpgInventoryManagerTransactionsPrivate::
	AreTransactionsPlacementSnapshotsExactlyEqual;
using RpgInventoryManagerTransactionsPrivate::
	GetTransactionsInventoryMaxStackSizeForDefinition;
using RpgInventoryManagerTransactionsPrivate::
	IsTransactionsCompleteSourceSnapshot;
using RpgInventoryManagerTransactionsPrivate::
	IsTransactionsCompletelyUnsetPlacement;
using RpgInventoryManagerTransactionsPrivate::
	IsTransactionsItemOwnedHandleDepthOverflow;
using RpgInventoryManagerTransactionsPrivate::
	MakeTransactionsRejectedIntentResult;
using RpgInventoryManagerTransactionsPrivate::
	TransactionsMaxRecentMutationResults;

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
	MarkInventoryStateDirty();
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

bool URpgInventoryManagerComponent::CommitRemovalDeltas(
	const TArray<FRpgInventoryMutationDelta>& Deltas,
	bool bBroadcastPostCommit)
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

	if (bBroadcastPostCommit)
	{
		MarkInventoryStateDirty();
	}
	return true;
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
	if (!IsTransactionsCompleteSourceSnapshot(
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
			MakeTransactionsRejectedIntentResult(
				Intent.RequestId,
				ERpgInventoryMutationOperation::Move,
				Intent.ExpectedQuantity));
	}
	return ExecuteInventoryMutation(Request);
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
	if (!IsTransactionsCompleteSourceSnapshot(
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
			MakeTransactionsRejectedIntentResult(
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
		IsTransactionsCompletelyUnsetPlacement(Intent.TargetPlacement);
	const bool bTargetPlacementMatches =
		bTargetPlacementUnset ||
		!Intent.TargetPlacement.ContainerHandle.IsValid() ||
		Intent.TargetPlacement.ContainerHandle == Intent.TargetContainer;
	if (IsTransactionsItemOwnedHandleDepthOverflow(
			Intent.ExpectedSourcePlacement.GetContainerHandle()) ||
		IsTransactionsItemOwnedHandleDepthOverflow(Intent.TargetContainer) ||
		IsTransactionsItemOwnedHandleDepthOverflow(
			Intent.TargetPlacement.GetContainerHandle()))
	{
		FRpgInventoryMutationResult Rejected =
			MakeTransactionsRejectedIntentResult(
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
		!IsTransactionsCompleteSourceSnapshot(
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
			MakeTransactionsRejectedIntentResult(
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

bool URpgInventoryManagerComponent::AreMutationRequestsEquivalent(
	const FRpgInventoryMutationRequest& A,
	const FRpgInventoryMutationRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.Operation == B.Operation &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		A.Source == B.Source &&
		AreTransactionsPlacementSnapshotsExactlyEqual(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.Target == B.Target &&
		A.Quantity == B.Quantity &&
		AreTransactionsPlacementSnapshotsExactlyEqual(
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
	while (RecentMutationOrder.Num() >
		TransactionsMaxRecentMutationResults)
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

	case ERpgInventoryMutationOperation::Consume:
		bInventoryStateChanged = true;
		bCommitted = CommitRemovalDeltas(Result.Deltas, false);
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
	const bool bBroadcastPostCommit = bCommitted && bInventoryStateChanged;
	Result = CacheResult(MoveTemp(Result));
	if (bBroadcastPostCommit)
	{
		MarkInventoryStateDirty();
	}
	return Result;
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
	if (IsTransactionsItemOwnedHandleDepthOverflow(Request.Source) ||
		IsTransactionsItemOwnedHandleDepthOverflow(Request.Target) ||
		IsTransactionsItemOwnedHandleDepthOverflow(
			Request.ExpectedSourcePlacement.GetContainerHandle()) ||
		IsTransactionsItemOwnedHandleDepthOverflow(
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
	if (const URpgInventoryContainerComponent* TargetContainer =
			TargetOwner->FindComponentByClass<URpgInventoryContainerComponent>();
		TargetContainer &&
		TargetContainer->GetInventoryManager() == TargetInventory &&
		!TargetContainer->CanReceiveTransferFrom(this))
	{
		return Reject(ERpgInventoryMutationResultCode::ItemNotAllowed);
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
		AreTransactionsPlacementSnapshotsExactlyEqual(
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
			!AreTransactionsPlacementSnapshotsExactlyEqual(
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
	PlacementQuery.Search = IsTransactionsCompletelyUnsetPlacement(
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
					!AreTransactionsPlacementSnapshotsExactlyEqual(
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
					GetTransactionsInventoryMaxStackSizeForDefinition(
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
				!AreTransactionsPlacementSnapshotsExactlyEqual(
					Left.Placement,
					Right.Placement) ||
				Left.Quantity != Right.Quantity ||
				Left.TargetItemId != Right.TargetItemId ||
				Left.TargetEntryId != Right.TargetEntryId ||
				Left.DisplacedItemId != Right.DisplacedItemId ||
				Left.DisplacedEntryId != Right.DisplacedEntryId ||
				!AreTransactionsPlacementSnapshotsExactlyEqual(
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
			AreTransactionsPlacementSnapshotsExactlyEqual(
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
			!AreTransactionsInventoryRuntimeStatesExactlyEqual(
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

	MarkInventoryStateDirty();
	TargetInventory->MarkInventoryStateDirty();

	return Result;
}
