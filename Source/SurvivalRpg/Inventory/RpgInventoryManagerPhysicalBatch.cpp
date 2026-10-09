#include "RpgInventoryManagerComponent.h"

#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "Itemization/RpgInventoryFragment_Itemization.h"
#include "Itemization/RpgItemizationGenerator.h"
#include "Itemization/RpgItemizationProfile.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgPhysicalBatchPrivate
{
	bool SameOperations(const TArray<FRpgInventoryBatchOperation>& A, const TArray<FRpgInventoryBatchOperation>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].SourceInventory != B[Index].SourceInventory || A[Index].TargetInventory != B[Index].TargetInventory ||
				A[Index].ItemId != B[Index].ItemId || A[Index].ItemDefinition != B[Index].ItemDefinition ||
				A[Index].Quantity != B[Index].Quantity || A[Index].ExpectedSourceRevision != B[Index].ExpectedSourceRevision ||
				A[Index].ExpectedTargetRevision != B[Index].ExpectedTargetRevision ||
				A[Index].ItemizationSourceLevel != B[Index].ItemizationSourceLevel || A[Index].ItemizationSeed != B[Index].ItemizationSeed) { return false; }
		}
		return true;
	}

	bool SameCapacity(const TArray<FRpgInventoryBatchCapacityChange>& A, const TArray<FRpgInventoryBatchCapacityChange>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Inventory != B[Index].Inventory || A[Index].ExpectedRevision != B[Index].ExpectedRevision ||
				A[Index].NewGridSize.Width != B[Index].NewGridSize.Width || A[Index].NewGridSize.Height != B[Index].NewGridSize.Height) { return false; }
		}
		return true;
	}

	bool SameState(const TArray<FRpgInventoryFragmentStatePayload>& A, const TArray<FRpgInventoryFragmentStatePayload>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].FragmentId != B[Index].FragmentId || A[Index].Version != B[Index].Version || A[Index].Payload != B[Index].Payload) { return false; }
		}
		return true;
	}

	/** Profile that rolls a definition grant, or null when the definition is not itemized or not validly configured. */
	const URpgItemizationProfile* FindGrantItemizationProfile(TSubclassOf<URpgInventoryItemDefinition> Definition)
	{
		const URpgInventoryItemDefinition* CDO = Definition ? GetDefault<URpgInventoryItemDefinition>(Definition) : nullptr;
		const URpgInventoryFragment_Itemization* Fragment = CDO
			? Cast<URpgInventoryFragment_Itemization>(CDO->FindFragmentByClass(URpgInventoryFragment_Itemization::StaticClass()))
			: nullptr;
		const URpgItemizationProfile* Profile = Fragment ? Fragment->ItemizationProfile.Get() : nullptr;
		return Profile && Profile->HasValidConfiguration() ? Profile : nullptr;
	}
}

bool URpgInventoryManagerComponent::CanApplyInventoryBatch(
	const TArray<FRpgInventoryBatchOperation>& Operations,
	ERpgInventoryMutationResultCode& OutCode,
	const TArray<FRpgInventoryBatchCapacityChange>& CapacityChanges)
{
	const FRpgInventoryMutationResult Result = ApplyInventoryBatchInternal(Operations, FGuid(), CapacityChanges, false, {}, {});
	OutCode = Result.Code;
	return Result.Code == ERpgInventoryMutationResultCode::Success;
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::ApplyInventoryBatch(
	const TArray<FRpgInventoryBatchOperation>& Operations,
	FGuid RequestId,
	const TArray<FRpgInventoryBatchCapacityChange>& CapacityChanges,
	TFunction<void()> CommitSideEffects,
	TFunction<bool()> RevalidateContext)
{
	return ApplyInventoryBatchInternal(Operations, RequestId, CapacityChanges, true, MoveTemp(CommitSideEffects), MoveTemp(RevalidateContext));
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::ApplyInventoryBatchInternal(
	const TArray<FRpgInventoryBatchOperation>& Operations,
	FGuid RequestId,
	const TArray<FRpgInventoryBatchCapacityChange>& CapacityChanges,
	bool bCommit,
	TFunction<void()> CommitSideEffects,
	TFunction<bool()> RevalidateContext)
{
	using namespace RpgPhysicalBatchPrivate;
	FRpgInventoryMutationResult Result;
	Result.RequestId = RequestId;
	Result.Operation = ERpgInventoryMutationOperation::Transfer;
	Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Result.Code = ERpgInventoryMutationResultCode::AuthorityRequired;
		return Result;
	}
	if (bCommit && (!RequestId.IsValid() || RecentMutationResults.Contains(RequestId))) { return Result; }
	if (bCommit)
	{
		if (const FRecentPhysicalBatch* Cached = RecentPhysicalBatches.Find(RequestId))
		{
			bool bSameEpoch = Cached->Inventories.Num() == Cached->Epochs.Num();
			for (int32 Index = 0; bSameEpoch && Index < Cached->Inventories.Num(); ++Index)
			{
				bSameEpoch = Cached->Inventories[Index].IsValid() && Cached->Inventories[Index]->MutationEpoch == Cached->Epochs[Index];
			}
			if (bSameEpoch)
			{
				return SameOperations(Operations, Cached->Operations) && SameCapacity(CapacityChanges, Cached->CapacityChanges)
					? Cached->Result : Result;
			}
			RecentPhysicalBatches.Remove(RequestId);
			RecentPhysicalBatchOrder.Remove(RequestId);
		}
	}
	if (Operations.IsEmpty() && CapacityChanges.IsEmpty()) { return Result; }

	struct FGraph
	{
		URpgInventoryManagerComponent* Inventory = nullptr;
		int32 Revision = 0;
		uint64 Epoch = 0;
		TArray<FRpgInventoryEntry> Before;
		TArray<FRpgInventoryEntry> Entries;
		TArray<FRpgInventoryItemId> ItemIds;
		TArray<TSubclassOf<URpgInventoryItemDefinition>> Definitions;
		TArray<TArray<FRpgInventoryFragmentStatePayload>> States;
		FRpgInventoryGridSize GridSize;
		bool bReceives = false;
		bool bChangesCapacity = false;
		bool bChanged = false;
	};
	TArray<FGraph> Graphs;
	TMap<URpgInventoryManagerComponent*, int32> Indices;
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>> InstanceRoots;
	TArray<TStrongObjectPtr<URpgInventoryManagerComponent>> InventoryRoots;
	const auto AddParticipant = [&Graphs, &Indices](URpgInventoryManagerComponent* Inventory)
	{
		if (Inventory && !Indices.Contains(Inventory))
		{
			const int32 Index = Graphs.AddDefaulted();
			Graphs[Index].Inventory = Inventory;
			Indices.Add(Inventory, Index);
		}
	};
	AddParticipant(this);
	int64 Requested = 0;
	for (const FRpgInventoryBatchOperation& Op : Operations)
	{
		if ((!Op.SourceInventory && !Op.TargetInventory) || Op.SourceInventory == Op.TargetInventory || Op.Quantity <= 0 ||
			(Op.SourceInventory && !Op.ItemId.IsValid()) || (!Op.SourceInventory && (!Op.ItemDefinition || Op.ItemId.IsValid())) ||
			Op.ItemizationSourceLevel < 0 ||
			(Op.ItemizationSourceLevel > 0 && (Op.SourceInventory || !Op.TargetInventory || !FindGrantItemizationProfile(Op.ItemDefinition)))) { return Result; }
		Requested += Op.Quantity;
		if (Requested > MAX_int32) { return Result; }
		AddParticipant(Op.SourceInventory);
		AddParticipant(Op.TargetInventory);
	}
	Result.RequestedQuantity = static_cast<int32>(Requested);
	for (const FRpgInventoryBatchCapacityChange& Change : CapacityChanges)
	{
		if (!Change.Inventory || !Change.NewGridSize.IsValid()) { return Result; }
		AddParticipant(Change.Inventory);
	}
	for (FGraph& Graph : Graphs)
	{
		if (!IsValid(Graph.Inventory) || !Graph.Inventory->GetOwner() || !Graph.Inventory->GetOwner()->HasAuthority() ||
			Graph.Inventory->GetWorld() != GetWorld() || Graph.Inventory->IsInventoryMutationLocked()) { return Result; }
		InventoryRoots.Emplace(Graph.Inventory);
	}
	for (FGraph& Graph : Graphs) { Graph.Inventory->bIsApplyingPhysicalBatch = true; }
	ON_SCOPE_EXIT { for (FGraph& Graph : Graphs) { Graph.Inventory->bIsApplyingPhysicalBatch = false; } };
	for (FGraph& Graph : Graphs)
	{
		URpgInventoryManagerComponent* Inventory = Graph.Inventory;
		Graph.Revision = Inventory->InventoryRevision;
		Graph.Epoch = Inventory->MutationEpoch;
		Graph.GridSize = Inventory->DefaultGridSize;
		Graph.Before = Inventory->InventoryList.Entries;
		Graph.Entries = Graph.Before;
		FValidatedInventoryGraph Validated;
		if (!Inventory->ValidateLiveInventoryGraph(false, Validated, Result.Code)) { return Result; }
		for (const FRpgInventoryEntry& Entry : Graph.Before)
		{
			InstanceRoots.Emplace(Entry.Instance.Get());
			Graph.ItemIds.Add(Entry.Instance->GetItemId());
			Graph.Definitions.Add(Entry.Instance->GetItemDef());
			if (!Entry.Instance->ExportRuntimeState(Graph.States.AddDefaulted_GetRef()))
			{
				Result.Code = ERpgInventoryMutationResultCode::InternalError;
				return Result;
			}
		}
	}
	for (const FRpgInventoryBatchCapacityChange& Change : CapacityChanges)
	{
		FGraph& Graph = Graphs[Indices[Change.Inventory]];
		if (Graph.bChangesCapacity || (Change.ExpectedRevision != INDEX_NONE && Graph.Revision != Change.ExpectedRevision) ||
			Change.NewGridSize.Width < Graph.GridSize.Width || Change.NewGridSize.Height < Graph.GridSize.Height ||
			Graph.Inventory->FindOwningPlayerInventoryLayout())
		{
			Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return Result;
		}
		Graph.bChangesCapacity = true;
		Graph.GridSize = Change.NewGridSize;
		Graph.bChanged = true;
	}

	const auto Stage = [&InstanceRoots](URpgInventoryManagerComponent* Inventory, TSubclassOf<URpgInventoryItemDefinition> Definition,
		URpgInventoryItemInstance* Source, bool bKeepIdentity) -> URpgInventoryItemInstance*
	{
		const URpgInventoryItemDefinition* CDO = Definition ? GetDefault<URpgInventoryItemDefinition>(Definition) : nullptr;
		if (!CDO) { return nullptr; }
		URpgInventoryItemInstance* Instance = NewObject<URpgInventoryItemInstance>(Inventory->GetOwner());
		InstanceRoots.Emplace(Instance);
		Instance->SetItemDef(Definition);
		for (const URpgInventoryItemFragment* Fragment : CDO->Fragments) { if (Fragment) { Fragment->OnInstanceCreated(Instance); } }
		return !Source || Instance->CopyRuntimeStateFrom(Source, bKeepIdentity) ? Instance : nullptr;
	};

	for (const FRpgInventoryBatchOperation& Op : Operations)
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		FGraph* Source = Op.SourceInventory ? &Graphs[Indices[Op.SourceInventory]] : nullptr;
		FGraph* Target = Op.TargetInventory ? &Graphs[Indices[Op.TargetInventory]] : nullptr;
		if ((Source && Op.ExpectedSourceRevision != INDEX_NONE && Source->Revision != Op.ExpectedSourceRevision) ||
			(Target && Op.ExpectedTargetRevision != INDEX_NONE && Target->Revision != Op.ExpectedTargetRevision))
		{
			Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
			return Result;
		}
		URpgInventoryItemInstance* Prototype = nullptr;
		TSubclassOf<URpgInventoryItemDefinition> Definition = Op.ItemDefinition;
		bool bKeepSourceIdentity = false;
		if (Source)
		{
			const int32 Index = Source->Entries.IndexOfByPredicate([&Op](const FRpgInventoryEntry& Entry) { return Entry.Instance->GetItemId() == Op.ItemId; });
			if (Index == INDEX_NONE) { Result.Code = ERpgInventoryMutationResultCode::ItemNotFound; return Result; }
			FRpgInventoryEntry& Entry = Source->Entries[Index];
			Prototype = Entry.Instance;
			Definition = Prototype->GetItemDef();
			if (Op.Quantity > Entry.StackCount || Prototype->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
			{
				Result.Code = ERpgInventoryMutationResultCode::ItemNotAllowed;
				return Result;
			}
			bKeepSourceIdentity = Op.Quantity == Entry.StackCount;
			Entry.StackCount -= Op.Quantity;
			if (Entry.StackCount == 0) { Source->Entries.RemoveAt(Index); }
			Source->bChanged = true;
		}
		if (!Target) { continue; }
		if (Source)
		{
			if (const URpgInventoryContainerComponent* Container = Target->Inventory->GetOwner()->FindComponentByClass<URpgInventoryContainerComponent>();
				Container && Container->GetInventoryManager() == Target->Inventory && !Container->CanReceiveTransferFrom(Source->Inventory))
			{
				Result.Code = ERpgInventoryMutationResultCode::ItemNotAllowed;
				return Result;
			}
		}
		Target->bReceives = true;
		Target->bChanged = true;
		if (!Prototype) { Prototype = Stage(Target->Inventory, Definition, nullptr, false); }
		if (!Prototype) { Result.Code = ERpgInventoryMutationResultCode::InternalError; return Result; }
		TArray<FRpgInventoryContainerHandle> Containers;
		if (const URpgPlayerInventoryLayoutComponent* Layout = Target->Inventory->FindOwningPlayerInventoryLayout())
		{
			for (const FRpgInventorySlotGroupView& Group : Layout->GetSlotGroups())
			{
				if (Group.GroupKind == ERpgInventorySlotGroupKind::Content && Group.Rule.AllowsItem(Prototype)) { Containers.AddUnique(Group.ContainerHandle); }
			}
		}
		else { Containers.Add(FRpgInventoryContainerHandle::MakeRoot(Target->Inventory->DefaultContainerId)); }
		// Rolled grants become one entry per piece with its own stats, so they never merge or share a stack.
		const URpgItemizationProfile* RollProfile = Op.ItemizationSourceLevel > 0 ? FindGrantItemizationProfile(Definition) : nullptr;
		FRandomStream ItemizationStream(Op.ItemizationSeed);
		const int32 MaxStack = RollProfile ? 1 : GetEffectiveMaxStackSizeForDefinition(Definition);
		int32 Remaining = Op.Quantity;
		for (FRpgInventoryEntry& Entry : Target->Entries)
		{
			if (!RollProfile && Remaining > 0 && Containers.Contains(Entry.Placement.ContainerHandle) && Prototype->IsStackCompatibleWith(Entry.Instance))
			{
				const int32 MergeCount = FMath::Min(Remaining, FMath::Max(0, MaxStack - Entry.StackCount));
				Entry.StackCount += MergeCount;
				Remaining -= MergeCount;
			}
		}
		bool bUsedOriginalIdentity = false;
		while (Remaining > 0)
		{
			FRpgInventoryEntry NewEntry;
			NewEntry.StackCount = FMath::Min(Remaining, MaxStack);
			NewEntry.Instance = Stage(Target->Inventory, Definition, Prototype, Source && bKeepSourceIdentity && !bUsedOriginalIdentity);
			if (!NewEntry.Instance) { Result.Code = ERpgInventoryMutationResultCode::InternalError; return Result; }
			if (RollProfile)
			{
				FRpgItemizationState RolledState;
				if (!FRpgItemizationGenerator::GenerateItemization(RollProfile, Op.ItemizationSourceLevel, ItemizationStream, RolledState) ||
					!NewEntry.Instance->ApplyItemizationState(RolledState))
				{
					Result.Code = ERpgInventoryMutationResultCode::InternalError;
					return Result;
				}
			}
			NewEntry.EntryId = FGuid::NewGuid();
			bool bFound = false;
			for (const FRpgInventoryContainerHandle& Container : Containers)
			{
				FRpgInventoryGridSize Size;
				if (!Target->Inventory->GetGridSizeForContainerHandle(Container, Size)) { continue; }
				// Capacity-only batches are supported. Placement uses the live capacity so a cost+grant batch
				// cannot accidentally rely on a grid expansion before the complete graph is validated.
				for (int32 Rotation = 0; Rotation < 2 && !bFound; ++Rotation)
				{
					for (int32 Y = 0; Y < Size.Height && !bFound; ++Y)
					{
						for (int32 X = 0; X < Size.Width && !bFound; ++X)
						{
							FRpgInventoryGridPlacement Placement;
							if (!Target->Inventory->TryNormalizePlacementForDefinition(Definition, Container, X, Y, Rotation != 0, Placement) ||
								Placement.X + Placement.GetOccupiedSize().Width > Size.Width || Placement.Y + Placement.GetOccupiedSize().Height > Size.Height ||
								Target->Entries.ContainsByPredicate([&Placement](const FRpgInventoryEntry& Existing) { return Existing.Placement.Overlaps(Placement); })) { continue; }
							NewEntry.Placement = Placement;
							ERpgInventoryMutationResultCode PlacementCode;
							bFound = Target->Inventory->ValidatePlacementGraphRules(NewEntry, Placement, PlacementCode);
						}
					}
				}
				if (bFound) { break; }
			}
			if (!bFound) { Result.Code = ERpgInventoryMutationResultCode::NoSpace; return Result; }
			Target->Entries.Add(NewEntry);
			Remaining -= NewEntry.StackCount;
			bUsedOriginalIdentity = true;
		}
	}

	TSet<FRpgInventoryItemId> FinalItemIds;
	for (FGraph& Graph : Graphs)
	{
		FValidatedInventoryGraph Validated;
		if (!Graph.Inventory->ValidateInventoryGraph(Graph.Entries, Graph.Inventory->GetOwner(), Graph.bReceives, Validated, Result.Code)) { return Result; }
		for (const FRpgInventoryEntry& Entry : Graph.Entries)
		{
			if (FinalItemIds.Contains(Entry.Instance->GetItemId())) { Result.Code = ERpgInventoryMutationResultCode::DuplicateItemId; return Result; }
			FinalItemIds.Add(Entry.Instance->GetItemId());
			TArray<URpgInventoryManagerComponent*> Siblings;
			Graph.Inventory->GetOwner()->GetComponents(Siblings);
			for (URpgInventoryManagerComponent* Sibling : Siblings)
			{
				if (!Indices.Contains(Sibling) && Sibling->FindItemById(Entry.Instance->GetItemId())) { Result.Code = ERpgInventoryMutationResultCode::DuplicateItemId; return Result; }
			}
		}
		if (Graph.Inventory->InventoryRevision != Graph.Revision || Graph.Inventory->MutationEpoch != Graph.Epoch ||
			Graph.Inventory->InventoryList.Entries.Num() != Graph.Before.Num()) { Result.Code = ERpgInventoryMutationResultCode::SourceMismatch; return Result; }
		for (int32 Index = 0; Index < Graph.Before.Num(); ++Index)
		{
			const FRpgInventoryEntry& Before = Graph.Before[Index];
			const FRpgInventoryEntry& Live = Graph.Inventory->InventoryList.Entries[Index];
			TArray<FRpgInventoryFragmentStatePayload> State;
			if (Live.Instance != Before.Instance || Live.StackCount != Before.StackCount || Live.EntryId != Before.EntryId || Live.Placement != Before.Placement ||
				Live.Instance->GetItemId() != Graph.ItemIds[Index] || Live.Instance->GetItemDef() != Graph.Definitions[Index] ||
				Live.Instance->GetOuter() != Graph.Inventory->GetOwner() || !Live.Instance->ExportRuntimeState(State) || !SameState(State, Graph.States[Index]))
			{
				Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
				return Result;
			}
		}
	}
	if (RevalidateContext && !RevalidateContext())
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return Result;
	}
	for (const FGraph& Graph : Graphs)
	{
		if (Graph.Inventory->InventoryRevision != Graph.Revision || Graph.Inventory->MutationEpoch != Graph.Epoch ||
			!IsValid(Graph.Inventory->GetOwner()))
		{
			Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
			return Result;
		}
	}
	Result.Code = ERpgInventoryMutationResultCode::Success;
	Result.AppliedQuantity = Result.RequestedQuantity;
	if (!bCommit) { return Result; }

	// Every operation, allocation, fragment hook and graph check has succeeded. Publish all graphs
	// before any revision observer, item notification, or caller-owned side effect can inspect them.
	for (FGraph& Graph : Graphs)
	{
		if (!Graph.bChanged) { continue; }
		URpgInventoryManagerComponent* Inventory = Graph.Inventory;
		Inventory->InventoryList.Entries = MoveTemp(Graph.Entries);
		if (Graph.bChangesCapacity) { Inventory->DefaultGridSize = Graph.GridSize; }
		for (FRpgInventoryEntry& Entry : Inventory->InventoryList.Entries)
		{
			const FRpgInventoryEntry* Before = Graph.Before.FindByPredicate([&Entry](const FRpgInventoryEntry& Old) { return Old.EntryId == Entry.EntryId; });
			if (!Before || Before->StackCount != Entry.StackCount) { Inventory->InventoryList.MarkItemDirty(Entry); }
			if (!Before && Inventory->IsUsingRegisteredSubObjectList() && Inventory->IsReadyForReplication())
			{
				Inventory->AddReplicatedSubObject(Entry.Instance, Inventory->ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
			}
		}
		for (const FRpgInventoryEntry& Before : Graph.Before)
		{
			if (!Inventory->InventoryList.FindEntryByEntryId(Before.EntryId) && Inventory->IsUsingRegisteredSubObjectList()) { Inventory->RemoveReplicatedSubObject(Before.Instance); }
		}
		Inventory->InventoryList.SortEntriesByPlacement();
		Inventory->InventoryList.MarkArrayDirty();
		++Inventory->InventoryRevision;
		Inventory->GetOwner()->FlushNetDormancy();
		Inventory->GetOwner()->ForceNetUpdate();
	}
	FRecentPhysicalBatch Record;
	Record.Operations = Operations;
	Record.CapacityChanges = CapacityChanges;
	Record.Result = Result;
	for (const FGraph& Graph : Graphs) { Record.Inventories.Add(Graph.Inventory); Record.Epochs.Add(Graph.Epoch); }
	RecentPhysicalBatches.Add(RequestId, MoveTemp(Record));
	RecentPhysicalBatchOrder.Add(RequestId);
	while (RecentPhysicalBatchOrder.Num() > 64) { RecentPhysicalBatches.Remove(RecentPhysicalBatchOrder[0]); RecentPhysicalBatchOrder.RemoveAt(0); }
	if (CommitSideEffects) { CommitSideEffects(); }
	for (FGraph& Graph : Graphs)
	{
		if (!Graph.bChanged) { continue; }
		URpgInventoryManagerComponent* Inventory = Graph.Inventory;
		for (FRpgInventoryEntry& Before : Graph.Before)
		{
			FRpgInventoryEntry* Current = Inventory->InventoryList.FindEntryByEntryId(Before.EntryId);
			if (!Current) { Inventory->InventoryList.BroadcastChangeMessage(Before, Before.StackCount, 0); }
			else if (Current->StackCount != Before.StackCount) { Inventory->InventoryList.BroadcastChangeMessage(*Current, Before.StackCount, Current->StackCount); }
		}
		for (FRpgInventoryEntry& Current : Inventory->InventoryList.Entries)
		{
			if (!Graph.Before.ContainsByPredicate([&Current](const FRpgInventoryEntry& Old) { return Old.EntryId == Current.EntryId; }))
			{
				Inventory->InventoryList.BroadcastChangeMessage(Current, 0, Current.StackCount);
			}
		}
		if (Graph.bChangesCapacity) { Inventory->BroadcastCapacityChanged(); }
		Inventory->OnInventoryPostCommit.Broadcast(Inventory);
	}
	return Result;
}
