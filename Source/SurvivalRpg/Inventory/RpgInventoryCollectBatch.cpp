// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgInventoryCollectBatchPrivate
{
	constexpr int32 MaxRecentMutationResults = 64;

	bool ArePlacementsExactlyEqual(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool AreRuntimeStatesExactlyEqual(
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

	bool IsPlacementWithinGrid(
		const FRpgInventoryGridPlacement& Placement,
		const FRpgInventoryGridSize& GridSize)
	{
		if (!Placement.IsValid() || !GridSize.IsValid())
		{
			return false;
		}
		const FRpgInventoryGridSize Occupied = Placement.GetOccupiedSize();
		return Placement.X >= 0 && Placement.Y >= 0 &&
			Placement.X + Occupied.Width <= GridSize.Width &&
			Placement.Y + Occupied.Height <= GridSize.Height;
	}

	bool OverlapsScratch(
		const FRpgInventoryGridPlacement& Placement,
		const TArray<FRpgInventoryGridPlacement>& Occupancy)
	{
		return Occupancy.ContainsByPredicate(
			[&Placement](const FRpgInventoryGridPlacement& Existing)
			{
				return Existing.Overlaps(Placement);
			});
	}

	bool AreContainerListsEqual(
		const TArray<FRpgInventoryContainerHandle>& A,
		const TArray<FRpgInventoryContainerHandle>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index] != B[Index])
			{
				return false;
			}
		}
		return true;
	}
}

struct URpgInventoryManagerComponent::FPreparedCollectBatch
{
	struct FSourceEntry
	{
		int32 LiveIndex = INDEX_NONE;
		FRpgInventoryEntry Before;
		FRpgInventoryItemId ExpectedItemId;
		TSubclassOf<URpgInventoryItemDefinition> ExpectedItemDefinition;
		TObjectPtr<UObject> ExpectedOuter = nullptr;
		int32 PlannedCount = 0;
		bool bRemove = false;
		bool bProviderMove = false;
		bool bHadAnyMerge = false;
		bool bRuntimeCaptured = false;
		TArray<FRpgInventoryFragmentStatePayload> ExpectedRuntimeState;
	};

	struct FMergeContribution
	{
		int32 SourceIndex = INDEX_NONE;
		int32 Quantity = 0;
	};

	struct FTargetEntry
	{
		int32 LiveIndex = INDEX_NONE;
		FRpgInventoryEntry Before;
		TObjectPtr<URpgInventoryItemInstance> CompatibilityInstance = nullptr;
		TObjectPtr<URpgInventoryItemInstance> StagedInstance = nullptr;
		FRpgInventoryItemId ItemId;
		TSubclassOf<URpgInventoryItemDefinition> ExpectedItemDefinition;
		TObjectPtr<UObject> ExpectedOuter = nullptr;
		FGuid EntryId;
		int32 InitialCount = 0;
		int32 PlannedCount = 0;
		FRpgInventoryGridPlacement Placement;
		int32 SourceIndex = INDEX_NONE;
		int32 ProviderRootSourceIndex = INDEX_NONE;
		bool bExisting = false;
		bool bProviderMove = false;
		bool bRuntimeCaptured = false;
		TArray<FRpgInventoryFragmentStatePayload> ExpectedRuntimeState;
		TArray<FMergeContribution> MergeContributions;
	};

	TObjectPtr<URpgInventoryManagerComponent> SourceInventory = nullptr;
	TObjectPtr<URpgInventoryManagerComponent> TargetInventory = nullptr;
	FGuid RequestId;
	TArray<FRpgInventoryContainerHandle> TargetContainers;
	int32 SourceRevision = INDEX_NONE;
	int32 TargetRevision = INDEX_NONE;
	int32 SourceEntryCount = 0;
	int32 TargetEntryCount = 0;
	int32 RequestedQuantity = 0;
	int32 AppliedQuantity = 0;
	TArray<FSourceEntry> SourceEntries;
	TArray<FTargetEntry> TargetEntries;
	TArray<int32> RootSourceIndices;
	TMap<int32, TArray<int32>> SubtreeIndicesByRoot;
	TArray<FRpgInventoryItemId> AffectedTargetItemIds;
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>> StagedInstanceRoots;
};

bool URpgInventoryManagerComponent::TryReplayRecentCollectRootBatch(
	FGuid RequestId,
	URpgInventoryManagerComponent* TargetInventory,
	const TArray<FRpgInventoryContainerHandle>& TargetContainers,
	FRpgInventoryMutationResult& OutResult,
	TArray<FRpgInventoryItemId>& OutAffectedTargetItemIds)
{
	using namespace RpgInventoryCollectBatchPrivate;

	FRecentMutationRecord* Record = RecentMutationResults.Find(RequestId);
	if (!Record)
	{
		return false;
	}

	URpgInventoryManagerComponent* CachedTarget = Record->TargetInventory.Get();
	const bool bEpochMatches =
		Record->SourceMutationEpoch == MutationEpoch &&
		(!Record->bHadTargetInventory ||
			(CachedTarget &&
			 Record->TargetMutationEpoch == CachedTarget->GetMutationEpoch()));
	if (!bEpochMatches)
	{
		RecentMutationResults.Remove(RequestId);
		RecentMutationOrder.Remove(RequestId);
		return false;
	}

	if (Record->Kind == FRecentMutationRecord::EKind::CollectRootBatch &&
		Record->bHadTargetInventory == (TargetInventory != nullptr) &&
		CachedTarget == TargetInventory &&
		AreContainerListsEqual(Record->TargetContainers, TargetContainers))
	{
		OutResult = Record->Result;
		OutAffectedTargetItemIds = Record->AffectedTargetItemIds;
		return true;
	}

	OutResult = FRpgInventoryMutationResult();
	OutResult.RequestId = RequestId;
	OutResult.Operation = ERpgInventoryMutationOperation::Pickup;
	OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
	OutAffectedTargetItemIds.Reset();
	return true;
}

FRpgInventoryMutationResult
URpgInventoryManagerComponent::CacheRecentCollectRootBatchResult(
	URpgInventoryManagerComponent* TargetInventory,
	const TArray<FRpgInventoryContainerHandle>& TargetContainers,
	FRpgInventoryMutationResult Result,
	const TArray<FRpgInventoryItemId>& AffectedTargetItemIds)
{
	using namespace RpgInventoryCollectBatchPrivate;

	if (!Result.RequestId.IsValid())
	{
		return Result;
	}

	FRecentMutationRecord& Record =
		RecentMutationResults.FindOrAdd(Result.RequestId);
	Record.Kind = FRecentMutationRecord::EKind::CollectRootBatch;
	Record.Request = FRpgInventoryMutationRequest();
	Record.TargetContainers = TargetContainers;
	Record.AffectedTargetItemIds = AffectedTargetItemIds;
	Record.TargetInventory = TargetInventory;
	Record.bHadTargetInventory = TargetInventory != nullptr;
	Record.bAllowPartialStack = true;
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

FRpgInventoryMutationResult URpgInventoryManagerComponent::CollectRootItemsBatch(
	URpgInventoryManagerComponent* TargetInventory,
	const TArray<FRpgInventoryContainerHandle>& TargetContainers,
	FGuid RequestId,
	TArray<FRpgInventoryItemId>& OutAffectedTargetItemIds)
{
	OutAffectedTargetItemIds.Reset();
	FRpgInventoryMutationResult Result;
	Result.RequestId = RequestId;
	Result.Operation = ERpgInventoryMutationOperation::Pickup;
	if (RequestId.IsValid() &&
		TryReplayRecentCollectRootBatch(
			RequestId,
			TargetInventory,
			TargetContainers,
			Result,
			OutAffectedTargetItemIds))
	{
		return Result;
	}

	auto Reject = [this,
		TargetInventory,
		&TargetContainers,
		&OutAffectedTargetItemIds,
		&Result](ERpgInventoryMutationResultCode Code)
	{
		Result.Code = Code;
		Result.AppliedQuantity = 0;
		Result.Deltas.Reset();
		OutAffectedTargetItemIds.Reset();
		return CacheRecentCollectRootBatchResult(
			TargetInventory,
			TargetContainers,
			MoveTemp(Result),
			OutAffectedTargetItemIds);
	};

	if (!RequestId.IsValid() || !TargetInventory || TargetInventory == this ||
		bIsApplyingCollectBatch || TargetInventory->bIsApplyingCollectBatch ||
		bIsApplyingPickupBatch || bIsPlanningPickupBatch ||
		TargetInventory->bIsApplyingPickupBatch ||
		TargetInventory->bIsPlanningPickupBatch)
	{
		return Reject(ERpgInventoryMutationResultCode::InvalidRequest);
	}

	TGuardValue<bool> SourceGuard(bIsApplyingCollectBatch, true);
	TGuardValue<bool> TargetGuard(
		TargetInventory->bIsApplyingCollectBatch,
		true);
	FPreparedCollectBatch Prepared;
	ERpgInventoryMutationResultCode Code =
		ERpgInventoryMutationResultCode::InvalidRequest;
	if (!PrepareCollectRootItemsBatch(
			TargetInventory,
			TargetContainers,
			RequestId,
			Prepared,
			Code))
	{
		Result.RequestedQuantity = Prepared.RequestedQuantity;
		return Reject(Code);
	}
	if (!RevalidateCollectRootItemsBatch(Prepared, Code))
	{
		Result.RequestedQuantity = Prepared.RequestedQuantity;
		return Reject(Code);
	}

	return CommitCollectRootItemsBatch(
		Prepared,
		OutAffectedTargetItemIds);
}

bool URpgInventoryManagerComponent::PrepareCollectRootItemsBatch(
	URpgInventoryManagerComponent* TargetInventory,
	const TArray<FRpgInventoryContainerHandle>& TargetContainers,
	FGuid RequestId,
	FPreparedCollectBatch& OutPrepared,
	ERpgInventoryMutationResultCode& OutCode)
{
	using namespace RpgInventoryCollectBatchPrivate;

	OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
	OutPrepared.SourceInventory = this;
	OutPrepared.TargetInventory = TargetInventory;
	OutPrepared.RequestId = RequestId;
	OutPrepared.TargetContainers = TargetContainers;
	OutPrepared.SourceRevision = InventoryRevision;
	OutPrepared.TargetRevision = TargetInventory
		? TargetInventory->InventoryRevision
		: INDEX_NONE;
	OutPrepared.SourceEntryCount = InventoryList.Entries.Num();
	OutPrepared.TargetEntryCount = TargetInventory
		? TargetInventory->InventoryList.Entries.Num()
		: 0;

	AActor* const SourceOwner = GetOwner();
	AActor* const TargetOwner = TargetInventory
		? TargetInventory->GetOwner()
		: nullptr;
	if (!RequestId.IsValid() || !SourceOwner || !TargetOwner ||
		!SourceOwner->HasAuthority() || !TargetOwner->HasAuthority())
	{
		OutCode = ERpgInventoryMutationResultCode::AuthorityRequired;
		return false;
	}
	if (!TargetInventory || TargetInventory == this ||
		SourceOwner->GetWorld() != TargetOwner->GetWorld() ||
		TargetContainers.IsEmpty())
	{
		OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
		return false;
	}

	TSet<FRpgInventoryContainerHandle> SeenTargetContainers;
	for (const FRpgInventoryContainerHandle& Container : TargetContainers)
	{
		FRpgInventoryGridSize GridSize;
		if (!Container.IsValid() ||
			SeenTargetContainers.Contains(Container) ||
			!TargetInventory->GetGridSizeForContainerHandle(
				Container,
				GridSize) ||
			!GridSize.IsValid())
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}
		SeenTargetContainers.Add(Container);
	}

	auto BuildLiveIndex = [](
		URpgInventoryManagerComponent* Inventory,
		const AActor* ExpectedOwner,
		TMap<FRpgInventoryItemId, int32>& OutIndexByItemId,
		TSet<FGuid>& OutEntryIds,
		ERpgInventoryMutationResultCode& OutGraphCode)
	{
		OutIndexByItemId.Reset();
		OutEntryIds.Reset();
		OutIndexByItemId.Reserve(Inventory->InventoryList.Entries.Num());
		OutEntryIds.Reserve(Inventory->InventoryList.Entries.Num());
		for (int32 EntryIndex = 0;
			 EntryIndex < Inventory->InventoryList.Entries.Num();
			 ++EntryIndex)
		{
			const FRpgInventoryEntry& Entry =
				Inventory->InventoryList.Entries[EntryIndex];
			if (!Entry.Instance || Entry.Instance->GetOuter() != ExpectedOwner ||
				!Entry.Instance->GetItemDef() ||
				!Entry.Instance->GetItemId().IsValid() ||
				!Entry.EntryId.IsValid() || Entry.StackCount <= 0 ||
				Entry.StackCount >
					URpgInventoryManagerComponent::
						GetEffectiveMaxStackSizeForDefinition(
							Entry.Instance->GetItemDef()) ||
				!Entry.Placement.IsValid() ||
				!Entry.Placement.GetContainerHandle().IsValid())
			{
				OutGraphCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
			if (OutIndexByItemId.Contains(Entry.Instance->GetItemId()))
			{
				OutGraphCode = ERpgInventoryMutationResultCode::DuplicateItemId;
				return false;
			}
			if (OutEntryIds.Contains(Entry.EntryId))
			{
				OutGraphCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
			OutIndexByItemId.Add(Entry.Instance->GetItemId(), EntryIndex);
			OutEntryIds.Add(Entry.EntryId);
		}

		for (int32 EntryIndex = 0;
			 EntryIndex < Inventory->InventoryList.Entries.Num();
			 ++EntryIndex)
		{
			const FRpgInventoryEntry& Entry =
				Inventory->InventoryList.Entries[EntryIndex];
			FRpgInventoryGridPlacement Normalized;
			ERpgInventoryMutationResultCode PlacementCode =
				ERpgInventoryMutationResultCode::Success;
			if (!Inventory->TryNormalizePlacementForDefinition(
					Entry.Instance->GetItemDef(),
					Entry.Placement.GetContainerHandle(),
					Entry.Placement.X,
					Entry.Placement.Y,
					Entry.Placement.bRotated,
					Normalized) ||
				!ArePlacementsExactlyEqual(Normalized, Entry.Placement) ||
				!Inventory->InventoryList.IsPlacementWithinGrid(
					Entry.Placement) ||
				!Inventory->ValidatePlacementGraphRules(
					Entry,
					Entry.Placement,
					PlacementCode))
			{
				OutGraphCode = PlacementCode ==
					ERpgInventoryMutationResultCode::Success
						? ERpgInventoryMutationResultCode::InvalidPlacement
						: PlacementCode;
				return false;
			}

			for (int32 OtherIndex = EntryIndex + 1;
				 OtherIndex < Inventory->InventoryList.Entries.Num();
				 ++OtherIndex)
			{
				const FRpgInventoryEntry& Other =
					Inventory->InventoryList.Entries[OtherIndex];
				if (Entry.Placement.GetContainerHandle() ==
						Other.Placement.GetContainerHandle() &&
					Entry.Placement.Overlaps(Other.Placement))
				{
					OutGraphCode = ERpgInventoryMutationResultCode::Occupied;
					return false;
				}
			}
		}

		OutGraphCode = ERpgInventoryMutationResultCode::Success;
		return true;
	};

	TMap<FRpgInventoryItemId, int32> SourceIndexByItemId;
	TMap<FRpgInventoryItemId, int32> TargetIndexByItemId;
	TSet<FGuid> SourceEntryIds;
	TSet<FGuid> TargetEntryIds;
	ERpgInventoryMutationResultCode GraphCode =
		ERpgInventoryMutationResultCode::Success;
	if (!BuildLiveIndex(
			this,
			SourceOwner,
			SourceIndexByItemId,
			SourceEntryIds,
			GraphCode) ||
		!BuildLiveIndex(
			TargetInventory,
			TargetOwner,
			TargetIndexByItemId,
			TargetEntryIds,
			GraphCode))
	{
		OutCode = GraphCode;
		return false;
	}
	if (!TargetInventory->IsCapacityUnlimited() &&
		TargetInventory->InventoryList.Entries.Num() >
			TargetInventory->GetMaxEntries())
	{
		OutCode = ERpgInventoryMutationResultCode::NoSpace;
		return false;
	}

	OutPrepared.SourceEntries.Reserve(InventoryList.Entries.Num());
	for (int32 SourceIndex = 0;
		 SourceIndex < InventoryList.Entries.Num();
		 ++SourceIndex)
	{
		FPreparedCollectBatch::FSourceEntry& PreparedSource =
			OutPrepared.SourceEntries.AddDefaulted_GetRef();
		PreparedSource.LiveIndex = SourceIndex;
		PreparedSource.Before = InventoryList.Entries[SourceIndex];
		PreparedSource.ExpectedItemId =
			PreparedSource.Before.Instance->GetItemId();
		PreparedSource.ExpectedItemDefinition =
			PreparedSource.Before.Instance->GetItemDef();
		PreparedSource.ExpectedOuter =
			PreparedSource.Before.Instance->GetOuter();
		PreparedSource.PlannedCount = PreparedSource.Before.StackCount;
	}

	OutPrepared.TargetEntries.Reserve(
		TargetInventory->InventoryList.Entries.Num() + 8);
	for (int32 TargetIndex = 0;
		 TargetIndex < TargetInventory->InventoryList.Entries.Num();
		 ++TargetIndex)
	{
		const FRpgInventoryEntry& Existing =
			TargetInventory->InventoryList.Entries[TargetIndex];
		FPreparedCollectBatch::FTargetEntry& PreparedTarget =
			OutPrepared.TargetEntries.AddDefaulted_GetRef();
		PreparedTarget.LiveIndex = TargetIndex;
		PreparedTarget.Before = Existing;
		PreparedTarget.CompatibilityInstance = Existing.Instance;
		PreparedTarget.ItemId = Existing.Instance->GetItemId();
		PreparedTarget.ExpectedItemDefinition =
			Existing.Instance->GetItemDef();
		PreparedTarget.ExpectedOuter = Existing.Instance->GetOuter();
		PreparedTarget.EntryId = Existing.EntryId;
		PreparedTarget.InitialCount = Existing.StackCount;
		PreparedTarget.PlannedCount = Existing.StackCount;
		PreparedTarget.Placement = Existing.Placement;
		PreparedTarget.bExisting = true;
	}

	for (int32 SourceIndex = 0;
		 SourceIndex < OutPrepared.SourceEntries.Num();
		 ++SourceIndex)
	{
		const FRpgInventoryEntry& Entry =
			OutPrepared.SourceEntries[SourceIndex].Before;
		if (Entry.Placement.GetContainerHandle().IsRoot())
		{
			OutPrepared.RootSourceIndices.Add(SourceIndex);
			OutPrepared.SubtreeIndicesByRoot.FindOrAdd(SourceIndex).Add(
				SourceIndex);
			continue;
		}

		int32 CurrentIndex = SourceIndex;
		TSet<int32> Visited;
		int32 RootIndex = INDEX_NONE;
		while (OutPrepared.SourceEntries.IsValidIndex(CurrentIndex))
		{
			if (Visited.Contains(CurrentIndex))
			{
				OutCode = ERpgInventoryMutationResultCode::CycleDetected;
				return false;
			}
			Visited.Add(CurrentIndex);
			const FRpgInventoryContainerHandle Handle =
				OutPrepared.SourceEntries[CurrentIndex]
					.Before.Placement.GetContainerHandle();
			if (Handle.IsRoot())
			{
				RootIndex = CurrentIndex;
				break;
			}
			const int32* ParentIndex =
				SourceIndexByItemId.Find(Handle.ItemOwnerId);
			if (!ParentIndex)
			{
				OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}
			CurrentIndex = *ParentIndex;
		}
		if (RootIndex == INDEX_NONE)
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}
		OutPrepared.SubtreeIndicesByRoot.FindOrAdd(RootIndex).Add(
			SourceIndex);
	}

	OutPrepared.RootSourceIndices.Sort(
		[&OutPrepared](int32 A, int32 B)
		{
			const FRpgInventoryEntry& Left =
				OutPrepared.SourceEntries[A].Before;
			const FRpgInventoryEntry& Right =
				OutPrepared.SourceEntries[B].Before;
			const FString LeftContainer =
				Left.Placement.GetContainerHandle().ToString();
			const FString RightContainer =
				Right.Placement.GetContainerHandle().ToString();
			if (LeftContainer != RightContainer)
			{
				return LeftContainer < RightContainer;
			}
			if (Left.Placement.Y != Right.Placement.Y)
			{
				return Left.Placement.Y < Right.Placement.Y;
			}
			if (Left.Placement.X != Right.Placement.X)
			{
				return Left.Placement.X < Right.Placement.X;
			}
			return Left.EntryId.ToString() < Right.EntryId.ToString();
		});

	int64 RequestedQuantity = 0;
	for (const int32 RootIndex : OutPrepared.RootSourceIndices)
	{
		RequestedQuantity +=
			OutPrepared.SourceEntries[RootIndex].Before.StackCount;
		if (RequestedQuantity > MAX_int32)
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		TArray<int32>& Subtree =
			OutPrepared.SubtreeIndicesByRoot.FindChecked(RootIndex);
		Subtree.Sort(
			[&OutPrepared, RootIndex](int32 A, int32 B)
			{
				if (A == RootIndex || B == RootIndex)
				{
					return A == RootIndex && B != RootIndex;
				}
				const FRpgInventoryEntry& Left =
					OutPrepared.SourceEntries[A].Before;
				const FRpgInventoryEntry& Right =
					OutPrepared.SourceEntries[B].Before;
				const uint8 LeftDepth =
					Left.Placement.GetContainerHandle().Depth;
				const uint8 RightDepth =
					Right.Placement.GetContainerHandle().Depth;
				if (LeftDepth != RightDepth)
				{
					return LeftDepth < RightDepth;
				}
				if (Left.Placement.Y != Right.Placement.Y)
				{
					return Left.Placement.Y < Right.Placement.Y;
				}
				if (Left.Placement.X != Right.Placement.X)
				{
					return Left.Placement.X < Right.Placement.X;
				}
				return Left.EntryId.ToString() < Right.EntryId.ToString();
			});
	}
	OutPrepared.RequestedQuantity = static_cast<int32>(RequestedQuantity);
	if (OutPrepared.RequestedQuantity <= 0)
	{
		OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
		return false;
	}

	TArray<URpgInventoryManagerComponent*> TargetOwnerInventories;
	TargetOwner->GetComponents(TargetOwnerInventories);
	TSet<FRpgInventoryItemId> ReservedTargetOwnerItemIds;
	for (URpgInventoryManagerComponent* Inventory : TargetOwnerInventories)
	{
		if (!Inventory ||
			(Inventory == this && SourceOwner == TargetOwner))
		{
			continue;
		}
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.ItemId.IsValid())
			{
				ReservedTargetOwnerItemIds.Add(Entry.ItemId);
			}
		}
	}

	TSet<FRpgInventoryItemId> PlannedTargetItemIds;
	for (const FPreparedCollectBatch::FTargetEntry& Entry :
		OutPrepared.TargetEntries)
	{
		PlannedTargetItemIds.Add(Entry.ItemId);
	}
	TSet<FGuid> ReservedEntryIds = SourceEntryIds;
	ReservedEntryIds.Append(TargetEntryIds);

	auto MakeUniqueEntryId = [&ReservedEntryIds]()
	{
		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const FGuid Candidate = FGuid::NewGuid();
			if (Candidate.IsValid() &&
				!ReservedEntryIds.Contains(Candidate))
			{
				ReservedEntryIds.Add(Candidate);
				return Candidate;
			}
		}
		return FGuid();
	};

	auto MakeUniqueItemId = [
		&SourceIndexByItemId,
		&ReservedTargetOwnerItemIds,
		&PlannedTargetItemIds]()
	{
		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const FRpgInventoryItemId Candidate =
				FRpgInventoryItemId::NewId();
			if (Candidate.IsValid() &&
				!SourceIndexByItemId.Contains(Candidate) &&
				!ReservedTargetOwnerItemIds.Contains(Candidate) &&
				!PlannedTargetItemIds.Contains(Candidate))
			{
				return Candidate;
			}
		}
		return FRpgInventoryItemId();
	};

	auto BuildScratchOccupancy = [&OutPrepared]()
	{
		TArray<FRpgInventoryGridPlacement> Occupancy;
		Occupancy.Reserve(OutPrepared.TargetEntries.Num());
		for (const FPreparedCollectBatch::FTargetEntry& Entry :
			OutPrepared.TargetEntries)
		{
			if (Entry.CompatibilityInstance && Entry.PlannedCount > 0)
			{
				Occupancy.Add(Entry.Placement);
			}
		}
		return Occupancy;
	};

	auto FindPlacement = [
		this,
		TargetInventory,
		&OutPrepared,
		&BuildScratchOccupancy](
		int32 SourceIndex,
		int32 Quantity,
		const FRpgInventoryContainerHandle& Container,
		FRpgInventoryGridPlacement& OutPlacement)
	{
		OutPlacement = FRpgInventoryGridPlacement();
		if (!OutPrepared.SourceEntries.IsValidIndex(SourceIndex) ||
			Quantity <= 0)
		{
			return false;
		}
		const FRpgInventoryEntry& SourceEntry =
			OutPrepared.SourceEntries[SourceIndex].Before;
		if (!SourceEntry.Instance)
		{
			return false;
		}

		FRpgInventoryGridSize GridSize;
		if (!TargetInventory->GetGridSizeForContainerHandle(
				Container,
				GridSize) ||
			!GridSize.IsValid())
		{
			return false;
		}
		const TArray<FRpgInventoryGridPlacement> Occupancy =
			BuildScratchOccupancy();
		for (int32 OrientationIndex = 0;
			 OrientationIndex < 2;
			 ++OrientationIndex)
		{
			const bool bRotated = OrientationIndex == 1;
			for (int32 Y = 0; Y < GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < GridSize.Width; ++X)
				{
					FRpgInventoryGridPlacement Candidate;
					if (!TargetInventory->TryNormalizePlacementForDefinition(
							SourceEntry.Instance->GetItemDef(),
							Container,
							X,
							Y,
							bRotated,
							Candidate) ||
						!IsPlacementWithinGrid(Candidate, GridSize) ||
						OverlapsScratch(Candidate, Occupancy))
					{
						continue;
					}

					FRpgInventoryEntryView SourceView;
					SourceView.InventoryOwner = this;
					SourceView.Instance = SourceEntry.Instance;
					SourceView.EntryId = SourceEntry.EntryId;
					SourceView.ItemId = SourceEntry.Instance->GetItemId();
					SourceView.StackCount = SourceEntry.StackCount;
					SourceView.Placement = SourceEntry.Placement;
					FRpgInventoryPlacementQuery Query;
					Query.Purpose = ERpgInventoryPlacementPurpose::Transfer;
					Query.Search = ERpgInventoryPlacementSearch::Exact;
					Query.Subject =
						FRpgInventoryPlacementSubject::FromIncomingInstance(
							this,
							SourceView,
							Quantity);
					Query.TargetContainer = Container;
					Query.ExactPlacement = Candidate;
					const FRpgInventoryPlacementPlan Plan =
						TargetInventory->EvaluatePlacement(Query);
					if (Plan.Code ==
							ERpgInventoryMutationResultCode::Success &&
						Plan.AppliedQuantity == Quantity &&
						Plan.Steps.Num() == 1 &&
						Plan.Steps[0].Resolution ==
							ERpgInventoryPlacementResolution::Place &&
						ArePlacementsExactlyEqual(
							Plan.Steps[0].Placement,
							Candidate))
					{
						OutPlacement = Candidate;
						return true;
					}
				}
			}
		}
		return false;
	};

	auto HasEntryCapacity = [TargetInventory, &OutPrepared](int32 Additional)
	{
		return Additional <= 0 || TargetInventory->IsCapacityUnlimited() ||
			OutPrepared.TargetEntries.Num() + Additional <=
				TargetInventory->GetMaxEntries();
	};

	auto CaptureSourceRuntime = [&OutPrepared](int32 SourceIndex)
	{
		if (!OutPrepared.SourceEntries.IsValidIndex(SourceIndex))
		{
			return false;
		}
		FPreparedCollectBatch::FSourceEntry& Source =
			OutPrepared.SourceEntries[SourceIndex];
		if (Source.bRuntimeCaptured)
		{
			return true;
		}
		if (!Source.Before.Instance ||
			!Source.Before.Instance->ExportRuntimeState(
				Source.ExpectedRuntimeState))
		{
			return false;
		}
		Source.bRuntimeCaptured = true;
		return true;
	};

	for (const int32 RootIndex : OutPrepared.RootSourceIndices)
	{
		FPreparedCollectBatch::FSourceEntry& Root =
			OutPrepared.SourceEntries[RootIndex];
		const TArray<int32>& Subtree =
			OutPrepared.SubtreeIndicesByRoot.FindChecked(RootIndex);
		const bool bTransfersProvider = Subtree.Num() > 1 ||
			Root.Before.Instance->FindFragmentByClass<
				URpgInventoryFragment_ItemContainer>() != nullptr;

		if (bTransfersProvider)
		{
			bool bIdentityConflict = false;
			for (const int32 SourceIndex : Subtree)
			{
				const FRpgInventoryItemId ItemId =
					OutPrepared.SourceEntries[SourceIndex]
						.ExpectedItemId;
				if (ReservedTargetOwnerItemIds.Contains(ItemId) ||
					PlannedTargetItemIds.Contains(ItemId))
				{
					bIdentityConflict = true;
					break;
				}
			}
			if (bIdentityConflict || !HasEntryCapacity(Subtree.Num()))
			{
				continue;
			}

			FRpgInventoryGridPlacement RootPlacement;
			bool bFoundPlacement = false;
			for (const FRpgInventoryContainerHandle& Container :
				TargetContainers)
			{
				if (FindPlacement(
						RootIndex,
						Root.Before.StackCount,
						Container,
						RootPlacement))
				{
					bFoundPlacement = true;
					break;
				}
			}
			if (!bFoundPlacement)
			{
				continue;
			}

			bool bPreparedSubtree = true;
			for (const int32 SourceIndex : Subtree)
			{
				FPreparedCollectBatch::FSourceEntry& Source =
					OutPrepared.SourceEntries[SourceIndex];
				const FGuid TargetEntryId = MakeUniqueEntryId();
				if (!TargetEntryId.IsValid() ||
					!CaptureSourceRuntime(SourceIndex))
				{
					bPreparedSubtree = false;
					break;
				}

				FPreparedCollectBatch::FTargetEntry& Addition =
					OutPrepared.TargetEntries.AddDefaulted_GetRef();
				Addition.CompatibilityInstance = Source.Before.Instance;
				Addition.ItemId = Source.ExpectedItemId;
				Addition.ExpectedItemDefinition =
					Source.ExpectedItemDefinition;
				Addition.ExpectedOuter = TargetOwner;
				Addition.EntryId = TargetEntryId;
				Addition.InitialCount = Source.Before.StackCount;
				Addition.PlannedCount = Source.Before.StackCount;
				Addition.Placement = Source.Before.Placement;
				Addition.SourceIndex = SourceIndex;
				Addition.ProviderRootSourceIndex = RootIndex;
				Addition.bProviderMove = true;
				if (SourceIndex == RootIndex)
				{
					Addition.Placement = RootPlacement;
				}
				else
				{
					FRpgInventoryContainerHandle RebasedContainer =
						Addition.Placement.GetContainerHandle();
					const int32 DepthDelta =
						static_cast<int32>(
							RootPlacement.GetContainerHandle().Depth) -
						static_cast<int32>(
							Root.Before.Placement.GetContainerHandle().Depth);
					const int32 RebasedDepth =
						static_cast<int32>(RebasedContainer.Depth) +
						DepthDelta;
					if (!RebasedContainer.IsItemOwned() ||
						RebasedDepth <= 0 ||
						RebasedDepth > RpgInventoryMaxItemOwnedDepth)
					{
						bPreparedSubtree = false;
						break;
					}
					RebasedContainer.Depth =
						static_cast<uint8>(RebasedDepth);
					Addition.Placement.SetContainerHandle(
						RebasedContainer);
				}
				Source.bRemove = true;
				Source.bProviderMove = true;
				Source.PlannedCount = 0;
				PlannedTargetItemIds.Add(Addition.ItemId);
			}
			if (!bPreparedSubtree)
			{
				OutCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}

			OutPrepared.AppliedQuantity += Root.Before.StackCount;
			OutPrepared.AffectedTargetItemIds.AddUnique(
				Root.ExpectedItemId);
			continue;
		}

		const FRpgInventoryItemId RootItemId = Root.ExpectedItemId;
		if (ReservedTargetOwnerItemIds.Contains(RootItemId) ||
			PlannedTargetItemIds.Contains(RootItemId))
		{
			continue;
		}

		int32 Remaining = Root.Before.StackCount;
		int32 RootApplied = 0;
		for (const FRpgInventoryContainerHandle& Container : TargetContainers)
		{
			if (Remaining <= 0)
			{
				break;
			}

			TArray<int32> MergeCandidates;
			for (int32 TargetPlanIndex = 0;
				 TargetPlanIndex < OutPrepared.TargetEntries.Num();
				 ++TargetPlanIndex)
			{
				const FPreparedCollectBatch::FTargetEntry& Target =
					OutPrepared.TargetEntries[TargetPlanIndex];
				const int32 MaxStack = GetEffectiveMaxStackSizeForDefinition(
					Target.CompatibilityInstance
						? Target.CompatibilityInstance->GetItemDef()
						: nullptr);
				if (Target.CompatibilityInstance &&
					Target.Placement.GetContainerHandle() == Container &&
					Target.PlannedCount > 0 &&
					Target.PlannedCount < MaxStack &&
					Root.Before.Instance->IsStackCompatibleWith(
						Target.CompatibilityInstance))
				{
					MergeCandidates.Add(TargetPlanIndex);
				}
			}
			MergeCandidates.Sort(
				[&OutPrepared](int32 A, int32 B)
				{
					const FPreparedCollectBatch::FTargetEntry& Left =
						OutPrepared.TargetEntries[A];
					const FPreparedCollectBatch::FTargetEntry& Right =
						OutPrepared.TargetEntries[B];
					if (Left.Placement.Y != Right.Placement.Y)
					{
						return Left.Placement.Y < Right.Placement.Y;
					}
					if (Left.Placement.X != Right.Placement.X)
					{
						return Left.Placement.X < Right.Placement.X;
					}
					return Left.EntryId.ToString() < Right.EntryId.ToString();
				});

			for (const int32 TargetPlanIndex : MergeCandidates)
			{
				if (Remaining <= 0)
				{
					break;
				}
				FPreparedCollectBatch::FTargetEntry& Target =
					OutPrepared.TargetEntries[TargetPlanIndex];
				const int32 MaxStack = GetEffectiveMaxStackSizeForDefinition(
					Target.CompatibilityInstance->GetItemDef());
				const int32 Quantity = FMath::Min(
					Remaining,
					MaxStack - Target.PlannedCount);
				if (Quantity <= 0)
				{
					continue;
				}
				if (Target.bExisting && !Target.bRuntimeCaptured)
				{
					if (!Target.Before.Instance->ExportRuntimeState(
							Target.ExpectedRuntimeState))
					{
						OutCode = ERpgInventoryMutationResultCode::InternalError;
						return false;
					}
					Target.bRuntimeCaptured = true;
				}
				FPreparedCollectBatch::FMergeContribution& Contribution =
					Target.MergeContributions.AddDefaulted_GetRef();
				Contribution.SourceIndex = RootIndex;
				Contribution.Quantity = Quantity;
				Target.PlannedCount += Quantity;
				Remaining -= Quantity;
				RootApplied += Quantity;
				Root.bHadAnyMerge = true;
				OutPrepared.AffectedTargetItemIds.AddUnique(Target.ItemId);
			}

			if (Remaining <= 0 || !HasEntryCapacity(1))
			{
				continue;
			}

			FRpgInventoryGridPlacement Placement;
			if (!FindPlacement(
					RootIndex,
					Remaining,
					Container,
					Placement))
			{
				continue;
			}

			const FRpgInventoryItemId TargetItemId =
				Root.bHadAnyMerge
					? MakeUniqueItemId()
					: RootItemId;
			const FGuid TargetEntryId = MakeUniqueEntryId();
			if (!TargetItemId.IsValid() || !TargetEntryId.IsValid() ||
				ReservedTargetOwnerItemIds.Contains(TargetItemId) ||
				PlannedTargetItemIds.Contains(TargetItemId))
			{
				OutCode = ERpgInventoryMutationResultCode::DuplicateItemId;
				return false;
			}

			FPreparedCollectBatch::FTargetEntry& Addition =
				OutPrepared.TargetEntries.AddDefaulted_GetRef();
			Addition.CompatibilityInstance = Root.Before.Instance;
			Addition.ItemId = TargetItemId;
			Addition.ExpectedItemDefinition =
				Root.ExpectedItemDefinition;
			Addition.ExpectedOuter = TargetOwner;
			Addition.EntryId = TargetEntryId;
			Addition.InitialCount = Remaining;
			Addition.PlannedCount = Remaining;
			Addition.Placement = Placement;
			Addition.SourceIndex = RootIndex;
			PlannedTargetItemIds.Add(TargetItemId);
			OutPrepared.AffectedTargetItemIds.AddUnique(TargetItemId);
			RootApplied += Remaining;
			Remaining = 0;
		}

		if (RootApplied > 0)
		{
			if (!CaptureSourceRuntime(RootIndex))
			{
				OutCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
			Root.PlannedCount = Remaining;
			Root.bRemove = Remaining == 0;
			OutPrepared.AppliedQuantity += RootApplied;
		}
	}

	if (OutPrepared.AppliedQuantity <= 0)
	{
		OutCode = ERpgInventoryMutationResultCode::NoSpace;
		return false;
	}

	for (FPreparedCollectBatch::FTargetEntry& Target :
		OutPrepared.TargetEntries)
	{
		if (Target.bExisting)
		{
			continue;
		}
		if (!OutPrepared.SourceEntries.IsValidIndex(Target.SourceIndex) ||
			!CaptureSourceRuntime(Target.SourceIndex))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		FPreparedCollectBatch::FSourceEntry& Source =
			OutPrepared.SourceEntries[Target.SourceIndex];
		const bool bReuseSourceInstance =
			SourceOwner == TargetOwner &&
			Target.ItemId == Source.ExpectedItemId &&
			Source.bRemove;
		if (bReuseSourceInstance)
		{
			Target.StagedInstance = Source.Before.Instance;
			Target.CompatibilityInstance = Source.Before.Instance;
			continue;
		}

		URpgInventoryItemInstance* StagedInstance =
			NewObject<URpgInventoryItemInstance>(TargetOwner);
		if (!StagedInstance)
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		OutPrepared.StagedInstanceRoots.Emplace(StagedInstance);
		StagedInstance->SetItemDef(Source.ExpectedItemDefinition);
		if (!StagedInstance->RestoreItemId(Target.ItemId))
		{
			OutCode = ERpgInventoryMutationResultCode::DuplicateItemId;
			return false;
		}
		const URpgInventoryItemDefinition* ItemCDO =
			GetDefault<URpgInventoryItemDefinition>(
				Source.ExpectedItemDefinition);
		if (!ItemCDO)
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		for (URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnInstanceCreated(StagedInstance);
			}
		}
		if (!StagedInstance->ImportRuntimeState(
				Source.ExpectedRuntimeState))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		TArray<FRpgInventoryFragmentStatePayload> StagedState;
		if (!StagedInstance->ExportRuntimeState(StagedState) ||
			!AreRuntimeStatesExactlyEqual(
				Source.ExpectedRuntimeState,
				StagedState))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		Target.StagedInstance = StagedInstance;
		Target.CompatibilityInstance = StagedInstance;
	}

	if (InventoryRevision != OutPrepared.SourceRevision ||
		TargetInventory->InventoryRevision != OutPrepared.TargetRevision)
	{
		OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
		return false;
	}

	OutCode = OutPrepared.AppliedQuantity ==
		OutPrepared.RequestedQuantity
		? ERpgInventoryMutationResultCode::Success
		: ERpgInventoryMutationResultCode::PartiallyApplied;
	return true;
}

bool URpgInventoryManagerComponent::RevalidateCollectRootItemsBatch(
	const FPreparedCollectBatch& Prepared,
	ERpgInventoryMutationResultCode& OutCode) const
{
	using namespace RpgInventoryCollectBatchPrivate;

	OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
	URpgInventoryManagerComponent* const TargetInventory =
		Prepared.TargetInventory;
	const AActor* const SourceOwner = GetOwner();
	const AActor* const TargetOwner = TargetInventory
		? TargetInventory->GetOwner()
		: nullptr;
	if (Prepared.SourceInventory != this || !TargetInventory ||
		TargetInventory == this || !SourceOwner || !TargetOwner ||
		!SourceOwner->HasAuthority() || !TargetOwner->HasAuthority() ||
		SourceOwner->GetWorld() != TargetOwner->GetWorld())
	{
		OutCode = ERpgInventoryMutationResultCode::AuthorityRequired;
		return false;
	}
	if (Prepared.SourceRevision != InventoryRevision ||
		Prepared.TargetRevision != TargetInventory->InventoryRevision ||
		Prepared.SourceEntryCount != InventoryList.Entries.Num() ||
		Prepared.TargetEntryCount !=
			TargetInventory->InventoryList.Entries.Num() ||
		Prepared.RequestedQuantity <= 0 || Prepared.AppliedQuantity <= 0 ||
		Prepared.AppliedQuantity > Prepared.RequestedQuantity)
	{
		return false;
	}

	auto EntryMatches = [](
		const FRpgInventoryEntry& Live,
		const FRpgInventoryEntry& Before,
		const FRpgInventoryItemId& ExpectedItemId,
		TSubclassOf<URpgInventoryItemDefinition> ExpectedItemDefinition,
		const UObject* ExpectedOuter)
	{
		return Live.Instance == Before.Instance && Live.Instance &&
			Live.Instance->GetItemId() == ExpectedItemId &&
			Live.Instance->GetItemDef() == ExpectedItemDefinition &&
			Live.Instance->GetOuter() == ExpectedOuter &&
			Live.EntryId == Before.EntryId &&
			Live.StackCount == Before.StackCount &&
			ArePlacementsExactlyEqual(Live.Placement, Before.Placement);
	};

	for (const FPreparedCollectBatch::FSourceEntry& Source :
		Prepared.SourceEntries)
	{
		if (!InventoryList.Entries.IsValidIndex(Source.LiveIndex) ||
			!EntryMatches(
				InventoryList.Entries[Source.LiveIndex],
				Source.Before,
				Source.ExpectedItemId,
				Source.ExpectedItemDefinition,
				Source.ExpectedOuter))
		{
			return false;
		}
		if (Source.bRuntimeCaptured)
		{
			TArray<FRpgInventoryFragmentStatePayload> CurrentState;
			if (!Source.Before.Instance->ExportRuntimeState(CurrentState) ||
				!AreRuntimeStatesExactlyEqual(
					Source.ExpectedRuntimeState,
					CurrentState))
			{
				return false;
			}
		}
	}

	for (const FPreparedCollectBatch::FTargetEntry& Target :
		Prepared.TargetEntries)
	{
		if (!Target.bExisting)
		{
			continue;
		}
		if (!TargetInventory->InventoryList.Entries.IsValidIndex(
				Target.LiveIndex) ||
			!EntryMatches(
				TargetInventory->InventoryList.Entries[Target.LiveIndex],
				Target.Before,
				Target.ItemId,
				Target.ExpectedItemDefinition,
				Target.ExpectedOuter))
		{
			return false;
		}
		if (Target.bRuntimeCaptured)
		{
			TArray<FRpgInventoryFragmentStatePayload> CurrentState;
			if (!Target.Before.Instance->ExportRuntimeState(CurrentState) ||
				!AreRuntimeStatesExactlyEqual(
					Target.ExpectedRuntimeState,
					CurrentState))
			{
				return false;
			}
		}
	}

	auto ValidateLiveGraph = [](
		const URpgInventoryManagerComponent* Inventory,
		ERpgInventoryMutationResultCode& OutGraphCode)
	{
		for (int32 EntryIndex = 0;
			 EntryIndex < Inventory->InventoryList.Entries.Num();
			 ++EntryIndex)
		{
			const FRpgInventoryEntry& Entry =
				Inventory->InventoryList.Entries[EntryIndex];
			FRpgInventoryGridPlacement Normalized;
			ERpgInventoryMutationResultCode PlacementCode =
				ERpgInventoryMutationResultCode::Success;
			if (!Entry.Instance || !Entry.Instance->GetItemDef() ||
				!Inventory->TryNormalizePlacementForDefinition(
					Entry.Instance->GetItemDef(),
					Entry.Placement.GetContainerHandle(),
					Entry.Placement.X,
					Entry.Placement.Y,
					Entry.Placement.bRotated,
					Normalized) ||
				!ArePlacementsExactlyEqual(Normalized, Entry.Placement) ||
				!Inventory->InventoryList.IsPlacementWithinGrid(
					Entry.Placement) ||
				!Inventory->ValidatePlacementGraphRules(
					Entry,
					Entry.Placement,
					PlacementCode))
			{
				OutGraphCode = PlacementCode ==
					ERpgInventoryMutationResultCode::Success
						? ERpgInventoryMutationResultCode::InvalidPlacement
						: PlacementCode;
				return false;
			}
			for (int32 OtherIndex = EntryIndex + 1;
				 OtherIndex < Inventory->InventoryList.Entries.Num();
				 ++OtherIndex)
			{
				const FRpgInventoryEntry& Other =
					Inventory->InventoryList.Entries[OtherIndex];
				if (Entry.Placement.GetContainerHandle() ==
						Other.Placement.GetContainerHandle() &&
					Entry.Placement.Overlaps(Other.Placement))
				{
					OutGraphCode = ERpgInventoryMutationResultCode::Occupied;
					return false;
				}
			}
		}
		OutGraphCode = ERpgInventoryMutationResultCode::Success;
		return true;
	};

	ERpgInventoryMutationResultCode GraphCode =
		ERpgInventoryMutationResultCode::Success;
	if (!ValidateLiveGraph(this, GraphCode) ||
		!ValidateLiveGraph(TargetInventory, GraphCode))
	{
		OutCode = GraphCode;
		return false;
	}

	const int32 PlannedAdditionCount =
		Prepared.TargetEntries.Num() - Prepared.TargetEntryCount;
	if (PlannedAdditionCount < 0 ||
		(!TargetInventory->IsCapacityUnlimited() &&
		 TargetInventory->InventoryList.Entries.Num() +
			 PlannedAdditionCount > TargetInventory->GetMaxEntries()))
	{
		OutCode = ERpgInventoryMutationResultCode::NoSpace;
		return false;
	}

	auto ValidateExactSourceRootPlacement = [this, TargetInventory](
		const FPreparedCollectBatch::FSourceEntry& Source,
		int32 Quantity,
		const FRpgInventoryGridPlacement& Placement)
	{
		if (!Source.Before.Instance || Quantity <= 0 ||
			!Source.Before.Placement.GetContainerHandle().IsRoot() ||
			!Placement.GetContainerHandle().IsValid())
		{
			return false;
		}
		FRpgInventoryEntryView SourceView;
		SourceView.InventoryOwner = const_cast<
			URpgInventoryManagerComponent*>(this);
		SourceView.Instance = Source.Before.Instance;
		SourceView.EntryId = Source.Before.EntryId;
		SourceView.ItemId = Source.ExpectedItemId;
		SourceView.StackCount = Source.Before.StackCount;
		SourceView.Placement = Source.Before.Placement;
		FRpgInventoryPlacementQuery Query;
		Query.Purpose = ERpgInventoryPlacementPurpose::Transfer;
		Query.Search = ERpgInventoryPlacementSearch::Exact;
		Query.Subject = FRpgInventoryPlacementSubject::FromIncomingInstance(
			const_cast<URpgInventoryManagerComponent*>(this),
			SourceView,
			Quantity);
		Query.TargetContainer = Placement.GetContainerHandle();
		Query.ExactPlacement = Placement;
		const FRpgInventoryPlacementPlan Plan =
			TargetInventory->EvaluatePlacement(Query);
		return Plan.Code == ERpgInventoryMutationResultCode::Success &&
			Plan.AppliedQuantity == Quantity && Plan.Steps.Num() == 1 &&
			Plan.Steps[0].Resolution ==
				ERpgInventoryPlacementResolution::Place &&
			ArePlacementsExactlyEqual(Plan.Steps[0].Placement, Placement);
	};

	TArray<FRpgInventoryGridPlacement> ScratchOccupancy;
	ScratchOccupancy.Reserve(Prepared.TargetEntries.Num());
	TSet<FGuid> EntryIds;
	TSet<FRpgInventoryItemId> TargetItemIds;
	for (const FRpgInventoryEntry& Existing :
		TargetInventory->InventoryList.Entries)
	{
		ScratchOccupancy.Add(Existing.Placement);
		EntryIds.Add(Existing.EntryId);
		TargetItemIds.Add(Existing.Instance->GetItemId());
	}

	TArray<URpgInventoryManagerComponent*> TargetOwnerInventories;
	const_cast<AActor*>(TargetOwner)->GetComponents(TargetOwnerInventories);
	TArray<int32> TransferredBySource;
	TransferredBySource.Init(0, Prepared.SourceEntries.Num());
	for (const FPreparedCollectBatch::FTargetEntry& Target :
		Prepared.TargetEntries)
	{
		URpgInventoryItemInstance* TargetInstance = Target.bExisting
			? Target.Before.Instance.Get()
			: Target.StagedInstance.Get();
		if (!TargetInstance || !Target.ItemId.IsValid() ||
			TargetInstance->GetItemId() != Target.ItemId ||
			!Target.ExpectedItemDefinition ||
			TargetInstance->GetItemDef() !=
				Target.ExpectedItemDefinition ||
			TargetInstance->GetOuter() != Target.ExpectedOuter ||
			Target.PlannedCount <= 0 ||
			Target.PlannedCount > GetEffectiveMaxStackSizeForDefinition(
				TargetInstance->GetItemDef()))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		int32 ContributionQuantity = 0;
		for (const FPreparedCollectBatch::FMergeContribution& Contribution :
			Target.MergeContributions)
		{
			if (!Prepared.SourceEntries.IsValidIndex(
					Contribution.SourceIndex) ||
				Contribution.Quantity <= 0)
			{
				OutCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
			const FPreparedCollectBatch::FSourceEntry& Source =
				Prepared.SourceEntries[Contribution.SourceIndex];
			if (!Source.Before.Instance ||
				!Source.Before.Instance->IsStackCompatibleWith(
					TargetInstance))
			{
				OutCode = ERpgInventoryMutationResultCode::StackIncompatible;
				return false;
			}
			ContributionQuantity += Contribution.Quantity;
			TransferredBySource[Contribution.SourceIndex] +=
				Contribution.Quantity;
		}
		const int32 ExpectedBaseCount = Target.bExisting
			? Target.Before.StackCount
			: Target.InitialCount;
		if (ExpectedBaseCount <= 0 ||
			ExpectedBaseCount + ContributionQuantity !=
				Target.PlannedCount)
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		if (Target.bExisting)
		{
			continue;
		}
		if (!Prepared.SourceEntries.IsValidIndex(Target.SourceIndex) ||
			!Target.EntryId.IsValid() || EntryIds.Contains(Target.EntryId) ||
			TargetItemIds.Contains(Target.ItemId) ||
			TargetInstance->GetOuter() != TargetOwner)
		{
			OutCode = TargetItemIds.Contains(Target.ItemId)
				? ERpgInventoryMutationResultCode::DuplicateItemId
				: ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		const FPreparedCollectBatch::FSourceEntry& Source =
			Prepared.SourceEntries[Target.SourceIndex];
		TArray<FRpgInventoryFragmentStatePayload> StagedState;
		if (!Source.bRuntimeCaptured ||
			!TargetInstance->ExportRuntimeState(StagedState) ||
			!AreRuntimeStatesExactlyEqual(
				Source.ExpectedRuntimeState,
				StagedState))
		{
			OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
			return false;
		}

		if (Source.Before.Placement.GetContainerHandle().IsRoot())
		{
			FRpgInventoryGridSize GridSize;
			if (!Prepared.TargetContainers.Contains(
					Target.Placement.GetContainerHandle()) ||
				!TargetInventory->GetGridSizeForContainerHandle(
					Target.Placement.GetContainerHandle(),
					GridSize) ||
				!IsPlacementWithinGrid(Target.Placement, GridSize) ||
				!ValidateExactSourceRootPlacement(
					Source,
					Target.InitialCount,
					Target.Placement))
			{
				OutCode = ERpgInventoryMutationResultCode::InvalidPlacement;
				return false;
			}
		}
		else
		{
			FRpgInventoryGridSize SourceGridSize;
			const FPreparedCollectBatch::FTargetEntry* ProviderRoot =
				Prepared.TargetEntries.FindByPredicate(
					[&Target](
						const FPreparedCollectBatch::FTargetEntry& Candidate)
					{
						return !Candidate.bExisting &&
							Candidate.bProviderMove &&
							Candidate.SourceIndex ==
								Target.ProviderRootSourceIndex;
					});
			FRpgInventoryGridPlacement ExpectedPlacement =
				Source.Before.Placement;
			FRpgInventoryContainerHandle ExpectedContainer =
				ExpectedPlacement.GetContainerHandle();
			const int32 DepthDelta = ProviderRoot &&
				Prepared.SourceEntries.IsValidIndex(
					Target.ProviderRootSourceIndex)
				? static_cast<int32>(
					ProviderRoot->Placement.GetContainerHandle().Depth) -
					static_cast<int32>(
						Prepared.SourceEntries[
							Target.ProviderRootSourceIndex]
							.Before.Placement.GetContainerHandle().Depth)
				: 0;
			const int32 ExpectedDepth =
				static_cast<int32>(ExpectedContainer.Depth) + DepthDelta;
			if (ExpectedContainer.IsItemOwned() && ExpectedDepth > 0 &&
				ExpectedDepth <= RpgInventoryMaxItemOwnedDepth)
			{
				ExpectedContainer.Depth =
					static_cast<uint8>(ExpectedDepth);
				ExpectedPlacement.SetContainerHandle(ExpectedContainer);
			}
			if (!Target.bProviderMove || !ProviderRoot ||
				!ArePlacementsExactlyEqual(
					Target.Placement,
					ExpectedPlacement) ||
				!GetGridSizeForContainerHandle(
					Source.Before.Placement.GetContainerHandle(),
					SourceGridSize) ||
				!IsPlacementWithinGrid(
					Target.Placement,
					SourceGridSize))
			{
				OutCode = ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}
		}

		if (OverlapsScratch(Target.Placement, ScratchOccupancy))
		{
			OutCode = ERpgInventoryMutationResultCode::Occupied;
			return false;
		}

		for (URpgInventoryManagerComponent* Inventory :
			TargetOwnerInventories)
		{
			if (!Inventory)
			{
				continue;
			}
			URpgInventoryItemInstance* Existing =
				Inventory->FindItemById(Target.ItemId);
			if (!Existing)
			{
				continue;
			}
			const bool bMovesSameActorInstance =
				Inventory == this && SourceOwner == TargetOwner &&
				Source.bRemove && Existing == Source.Before.Instance &&
				TargetInstance == Existing;
			if (!bMovesSameActorInstance)
			{
				OutCode = ERpgInventoryMutationResultCode::DuplicateItemId;
				return false;
			}
		}

		EntryIds.Add(Target.EntryId);
		TargetItemIds.Add(Target.ItemId);
		ScratchOccupancy.Add(Target.Placement);
		if (!Target.bProviderMove)
		{
			TransferredBySource[Target.SourceIndex] += Target.InitialCount;
		}
	}

	int32 RecomputedAppliedQuantity = 0;
	for (const int32 RootIndex : Prepared.RootSourceIndices)
	{
		if (!Prepared.SourceEntries.IsValidIndex(RootIndex))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		const FPreparedCollectBatch::FSourceEntry& Root =
			Prepared.SourceEntries[RootIndex];
		if (Root.bProviderMove)
		{
			if (!Root.bRemove || Root.PlannedCount != 0)
			{
				OutCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
			RecomputedAppliedQuantity += Root.Before.StackCount;
		}
		else if (TransferredBySource[RootIndex] > 0)
		{
			if (TransferredBySource[RootIndex] > Root.Before.StackCount ||
				Root.PlannedCount != Root.Before.StackCount -
					TransferredBySource[RootIndex] ||
				Root.bRemove != (Root.PlannedCount == 0))
			{
				OutCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
			RecomputedAppliedQuantity += TransferredBySource[RootIndex];
		}
	}
	if (RecomputedAppliedQuantity != Prepared.AppliedQuantity)
	{
		OutCode = ERpgInventoryMutationResultCode::InternalError;
		return false;
	}

	TSet<FRpgInventoryItemId> AffectedIds;
	for (const FRpgInventoryItemId& ItemId :
		Prepared.AffectedTargetItemIds)
	{
		if (!ItemId.IsValid() || AffectedIds.Contains(ItemId) ||
			!Prepared.TargetEntries.ContainsByPredicate(
				[&ItemId](const FPreparedCollectBatch::FTargetEntry& Entry)
				{
					return Entry.ItemId == ItemId &&
						(!Entry.MergeContributions.IsEmpty() ||
						 !Entry.bExisting);
				}))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		AffectedIds.Add(ItemId);
	}

	if (InventoryRevision != Prepared.SourceRevision ||
		TargetInventory->InventoryRevision != Prepared.TargetRevision)
	{
		return false;
	}

	OutCode = Prepared.AppliedQuantity == Prepared.RequestedQuantity
		? ERpgInventoryMutationResultCode::Success
		: ERpgInventoryMutationResultCode::PartiallyApplied;
	return true;
}

FRpgInventoryMutationResult
URpgInventoryManagerComponent::CommitCollectRootItemsBatch(
	FPreparedCollectBatch& Prepared,
	TArray<FRpgInventoryItemId>& OutAffectedTargetItemIds)
{
	URpgInventoryManagerComponent* const TargetInventory =
		Prepared.TargetInventory;
	check(TargetInventory && TargetInventory != this);

	FRpgInventoryMutationResult Result;
	Result.RequestId = Prepared.RequestId;
	Result.Operation = ERpgInventoryMutationOperation::Pickup;
	Result.RequestedQuantity = Prepared.RequestedQuantity;
	Result.AppliedQuantity = Prepared.AppliedQuantity;
	Result.Code = Prepared.AppliedQuantity == Prepared.RequestedQuantity
		? ERpgInventoryMutationResultCode::Success
		: ERpgInventoryMutationResultCode::PartiallyApplied;

	auto IsOrdinaryIdentityMove = [&Prepared](
		const FPreparedCollectBatch::FTargetEntry& Target)
	{
		if (Target.bExisting || Target.bProviderMove ||
			!Prepared.SourceEntries.IsValidIndex(Target.SourceIndex))
		{
			return false;
		}
		const FPreparedCollectBatch::FSourceEntry& Source =
			Prepared.SourceEntries[Target.SourceIndex];
		return Source.bRemove && !Source.bHadAnyMerge &&
			Target.ItemId == Source.ExpectedItemId &&
			Target.InitialCount == Source.Before.StackCount;
	};

	TSet<int32> SourceRowsRepresentedAsMoves;
	for (const FPreparedCollectBatch::FTargetEntry& Target :
		Prepared.TargetEntries)
	{
		if (Target.bExisting)
		{
			if (Target.PlannedCount == Target.Before.StackCount)
			{
				continue;
			}
			FRpgInventoryMutationDelta& Delta =
				Result.Deltas.AddDefaulted_GetRef();
			Delta.Kind = ERpgInventoryMutationDeltaKind::StackChanged;
			Delta.ItemId = Target.ItemId;
			Delta.BeforeContainer =
				Target.Before.Placement.GetContainerHandle();
			Delta.AfterContainer = Delta.BeforeContainer;
			Delta.BeforePlacement = Target.Before.Placement;
			Delta.AfterPlacement = Target.Before.Placement;
			Delta.PreviousQuantity = Target.Before.StackCount;
			Delta.NewQuantity = Target.PlannedCount;
			continue;
		}

		check(Prepared.SourceEntries.IsValidIndex(Target.SourceIndex));
		const FPreparedCollectBatch::FSourceEntry& Source =
			Prepared.SourceEntries[Target.SourceIndex];
		const bool bMoved = Target.bProviderMove ||
			IsOrdinaryIdentityMove(Target);
		FRpgInventoryMutationDelta& Delta =
			Result.Deltas.AddDefaulted_GetRef();
		Delta.Kind = bMoved
			? ERpgInventoryMutationDeltaKind::Moved
			: ERpgInventoryMutationDeltaKind::Added;
		Delta.ItemId = Target.ItemId;
		if (bMoved)
		{
			Delta.BeforeContainer =
				Source.Before.Placement.GetContainerHandle();
			Delta.BeforePlacement = Source.Before.Placement;
			Delta.PreviousQuantity = Source.Before.StackCount;
			SourceRowsRepresentedAsMoves.Add(Target.SourceIndex);
		}
		Delta.AfterContainer = Target.Placement.GetContainerHandle();
		Delta.AfterPlacement = Target.Placement;
		Delta.NewQuantity = Target.PlannedCount;
	}

	for (int32 SourceIndex = 0;
		 SourceIndex < Prepared.SourceEntries.Num();
		 ++SourceIndex)
	{
		const FPreparedCollectBatch::FSourceEntry& Source =
			Prepared.SourceEntries[SourceIndex];
		if ((!Source.bRemove &&
			 Source.PlannedCount == Source.Before.StackCount) ||
			SourceRowsRepresentedAsMoves.Contains(SourceIndex))
		{
			continue;
		}

		FRpgInventoryMutationDelta& Delta =
			Result.Deltas.AddDefaulted_GetRef();
		Delta.Kind = Source.bRemove
			? ERpgInventoryMutationDeltaKind::Removed
			: ERpgInventoryMutationDeltaKind::StackChanged;
		Delta.ItemId = Source.ExpectedItemId;
		Delta.BeforeContainer =
			Source.Before.Placement.GetContainerHandle();
		Delta.BeforePlacement = Source.Before.Placement;
		Delta.PreviousQuantity = Source.Before.StackCount;
		Delta.NewQuantity = Source.PlannedCount;
		if (!Source.bRemove)
		{
			Delta.AfterContainer = Delta.BeforeContainer;
			Delta.AfterPlacement = Source.Before.Placement;
		}
	}

	struct FStableNotification
	{
		FRpgInventoryEntry Entry;
		int32 OldCount = 0;
	};
	TArray<FStableNotification> TargetChangedNotifications;
	TArray<FStableNotification> TargetAddedNotifications;
	TArray<FStableNotification> SourceChangedNotifications;
	TArray<FStableNotification> SourceRemovedNotifications;
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>>
		SourceRemovalInstanceRoots;

	int32 TargetAdditionCount = 0;
	for (const FPreparedCollectBatch::FTargetEntry& Target :
		Prepared.TargetEntries)
	{
		TargetAdditionCount += !Target.bExisting;
	}
	TargetInventory->InventoryList.Entries.Reserve(
		TargetInventory->InventoryList.Entries.Num() +
		TargetAdditionCount);
	TargetChangedNotifications.Reserve(
		Prepared.TargetEntryCount);
	TargetAddedNotifications.Reserve(TargetAdditionCount);
	SourceChangedNotifications.Reserve(Prepared.SourceEntries.Num());
	SourceRemovedNotifications.Reserve(Prepared.SourceEntries.Num());
	SourceRemovalInstanceRoots.Reserve(Prepared.SourceEntries.Num());

	for (const FPreparedCollectBatch::FTargetEntry& Target :
		Prepared.TargetEntries)
	{
		if (Target.bExisting)
		{
			if (Target.PlannedCount == Target.Before.StackCount)
			{
				continue;
			}
			FRpgInventoryEntry& Live =
				TargetInventory->InventoryList.Entries[Target.LiveIndex];
			FStableNotification& Notification =
				TargetChangedNotifications.AddDefaulted_GetRef();
			Notification.OldCount = Live.StackCount;
			Live.StackCount = Target.PlannedCount;
			TargetInventory->InventoryList.MarkItemDirty(Live);
			Notification.Entry = Live;
			continue;
		}

		FRpgInventoryEntry& Added =
			TargetInventory->InventoryList.Entries.AddDefaulted_GetRef();
		Added.Instance = Target.StagedInstance;
		Added.EntryId = Target.EntryId;
		Added.StackCount = Target.PlannedCount;
		Added.Placement = Target.Placement;
		TargetInventory->InventoryList.MarkItemDirty(Added);
		FStableNotification& Notification =
			TargetAddedNotifications.AddDefaulted_GetRef();
		Notification.Entry = Added;
	}
	if (TargetAdditionCount > 0)
	{
		TargetInventory->InventoryList.MarkArrayDirty();
	}

	TArray<int32> SourceRemovalIndices;
	for (const FPreparedCollectBatch::FSourceEntry& Source :
		Prepared.SourceEntries)
	{
		if (Source.bRemove)
		{
			SourceRemovalIndices.Add(Source.LiveIndex);
			FStableNotification& Notification =
				SourceRemovedNotifications.AddDefaulted_GetRef();
			Notification.Entry = Source.Before;
			Notification.OldCount = Source.Before.StackCount;
			SourceRemovalInstanceRoots.Emplace(
				Source.Before.Instance.Get());
			continue;
		}
		if (Source.PlannedCount != Source.Before.StackCount)
		{
			FRpgInventoryEntry& Live =
				InventoryList.Entries[Source.LiveIndex];
			FStableNotification& Notification =
				SourceChangedNotifications.AddDefaulted_GetRef();
			Notification.OldCount = Live.StackCount;
			Live.StackCount = Source.PlannedCount;
			InventoryList.MarkItemDirty(Live);
			Notification.Entry = Live;
		}
	}
	SourceRemovalIndices.Sort(TGreater<int32>());
	for (const int32 SourceIndex : SourceRemovalIndices)
	{
		InventoryList.Entries.RemoveAt(
			SourceIndex,
			1,
			EAllowShrinking::No);
	}
	if (!SourceRemovalIndices.IsEmpty())
	{
		InventoryList.MarkArrayDirty();
	}

	if (IsUsingRegisteredSubObjectList())
	{
		for (const FStableNotification& Removed :
			SourceRemovedNotifications)
		{
			RemoveReplicatedSubObject(Removed.Entry.Instance);
		}
	}
	if (TargetInventory->IsUsingRegisteredSubObjectList() &&
		TargetInventory->IsReadyForReplication())
	{
		for (const FStableNotification& Added : TargetAddedNotifications)
		{
			TargetInventory->AddReplicatedSubObject(
				Added.Entry.Instance,
				TargetInventory->ReplicationPolicy ==
						ERpgInventoryReplicationPolicy::OwnerOnly
					? COND_OwnerOnly
					: COND_None);
		}
	}

	MarkInventoryStateDirty();
	TargetInventory->MarkInventoryStateDirty();
	OutAffectedTargetItemIds = Prepared.AffectedTargetItemIds;
	Result = CacheRecentCollectRootBatchResult(
		TargetInventory,
		Prepared.TargetContainers,
		MoveTemp(Result),
		OutAffectedTargetItemIds);

	SourceRemovedNotifications.Sort(
		[](const FStableNotification& A, const FStableNotification& B)
		{
			return A.Entry.Placement.GetContainerHandle().Depth >
				B.Entry.Placement.GetContainerHandle().Depth;
		});
	TargetAddedNotifications.Sort(
		[](const FStableNotification& A, const FStableNotification& B)
		{
			return A.Entry.Placement.GetContainerHandle().Depth <
				B.Entry.Placement.GetContainerHandle().Depth;
		});

	for (FStableNotification& Changed : SourceChangedNotifications)
	{
		InventoryList.BroadcastChangeMessage(
			Changed.Entry,
			Changed.OldCount,
			Changed.Entry.StackCount);
	}
	for (FStableNotification& Removed : SourceRemovedNotifications)
	{
		InventoryList.BroadcastChangeMessage(
			Removed.Entry,
			Removed.OldCount,
			0);
	}
	for (FStableNotification& Changed : TargetChangedNotifications)
	{
		TargetInventory->InventoryList.BroadcastChangeMessage(
			Changed.Entry,
			Changed.OldCount,
			Changed.Entry.StackCount);
	}
	for (FStableNotification& Added : TargetAddedNotifications)
	{
		TargetInventory->InventoryList.BroadcastChangeMessage(
			Added.Entry,
			0,
			Added.Entry.StackCount);
	}

	return Result;
}
