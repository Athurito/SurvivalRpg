// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "UObject/UObjectGlobals.h"

namespace RpgInventoryManagerRulesPlannerPrivate
{
	ERpgInventoryMutationResultCode GetRulesItemContainerRuleFailureCode(
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
		if (bProvidesContainer &&
			!ContainerDefinition.bAllowNestedContainers)
		{
			return ERpgInventoryMutationResultCode::
				NestedContainersNotAllowed;
		}
		if (bProvidesContainer &&
			TargetDepth >=
				ContainerDefinition.GetEffectiveMaxNestingDepth())
		{
			return ERpgInventoryMutationResultCode::MaxDepthExceeded;
		}
		return ERpgInventoryMutationResultCode::ItemNotAllowed;
	}

	bool AreRulesPlacementSnapshotsExactlyEqual(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool IsRulesItemOwnedHandleDepthOverflow(
		const FRpgInventoryContainerHandle& Handle)
	{
		return Handle.Root.IsNone() && Handle.ItemOwnerId.IsValid() &&
			!Handle.ContainerId.IsNone() &&
			Handle.Depth > RpgInventoryMaxItemOwnedDepth;
	}

	ERpgInventoryMutationResultCode EvaluateRulesScratchPlacement(
		const FRpgInventoryGridPlacement& Placement,
		const FRpgInventoryGridSize& GridSize,
		const TArray<FRpgInventoryGridPlacement>& Occupancy)
	{
		if (!Placement.IsValid())
		{
			return ERpgInventoryMutationResultCode::InvalidPlacement;
		}

		const FRpgInventoryGridSize OccupiedSize =
			Placement.GetOccupiedSize();
		if (!GridSize.IsValid() || Placement.X < 0 ||
			Placement.Y < 0 ||
			static_cast<int64>(Placement.X) + OccupiedSize.Width >
				GridSize.Width ||
			static_cast<int64>(Placement.Y) + OccupiedSize.Height >
				GridSize.Height)
		{
			return ERpgInventoryMutationResultCode::OutOfBounds;
		}

		if (Occupancy.ContainsByPredicate(
			[&Placement](
				const FRpgInventoryGridPlacement& Existing)
			{
				return Existing.Overlaps(Placement);
			}))
		{
			return ERpgInventoryMutationResultCode::Occupied;
		}

		return ERpgInventoryMutationResultCode::Success;
	}
}

using RpgInventoryManagerRulesPlannerPrivate::
	AreRulesPlacementSnapshotsExactlyEqual;
using RpgInventoryManagerRulesPlannerPrivate::
	EvaluateRulesScratchPlacement;
using RpgInventoryManagerRulesPlannerPrivate::
	GetRulesItemContainerRuleFailureCode;
using RpgInventoryManagerRulesPlannerPrivate::
	IsRulesItemOwnedHandleDepthOverflow;

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
			Entry.StackCount > URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(
				Entry.Instance->GetItemDef()))
		{
			OutCode = ERpgInventoryMutationResultCode::StackLimitReached;
			return false;
		}

		const FRpgInventoryContainerHandle Handle =
			Entry.Placement.GetContainerHandle();
		if (IsRulesItemOwnedHandleDepthOverflow(Handle))
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
			!AreRulesPlacementSnapshotsExactlyEqual(
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
				OutCode = GetRulesItemContainerRuleFailureCode(
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
			EvaluateRulesScratchPlacement(
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
	if (IsRulesItemOwnedHandleDepthOverflow(
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
		OutCode = GetRulesItemContainerRuleFailureCode(
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
