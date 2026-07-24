// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "AbilitySystemComponent.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
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

	int32 GetRulesInventoryMaxStackSizeForDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition
			? GetDefault<URpgInventoryItemDefinition>(ItemDefinition)
			: nullptr;
		if (ItemCDO &&
			ItemCDO->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()))
		{
			// One provider owns one concrete set of item-owned grids, so
			// physical container providers can never share one stack entry.
			return 1;
		}

		const URpgInventoryFragment_ItemTraits* Traits = ItemCDO
			? Cast<URpgInventoryFragment_ItemTraits>(
				ItemCDO->FindFragmentByClass(
					URpgInventoryFragment_ItemTraits::StaticClass()))
			: nullptr;
		return Traits ? Traits->GetMaxStackSize() : 1;
	}

	FRpgInventoryGridSize GetRulesInventoryFootprintForDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		bool bRotated)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment =
			URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
				ItemDefinition);
		if (SpatialFragment)
		{
			return SpatialFragment->GetFootprint(bRotated);
		}

		FRpgInventoryGridSize InvalidFootprint;
		InvalidFootprint.Width = 0;
		InvalidFootprint.Height = 0;
		return InvalidFootprint;
	}

	bool CanRulesInventoryRotateDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment =
			URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
				ItemDefinition);
		return SpatialFragment && SpatialFragment->bAllowRotation;
	}

	bool IsRulesCompleteSourceSnapshot(
		const FRpgInventoryItemId& ItemId,
		const FGuid& ExpectedEntryId,
		const FRpgInventoryGridPlacement& ExpectedSourcePlacement)
	{
		return ItemId.IsValid() &&
			ExpectedEntryId.IsValid() &&
			ExpectedSourcePlacement.ContainerHandle.IsValid() &&
			ExpectedSourcePlacement.IsValid();
	}

	bool FindRulesFirstFitInScratch(
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
		for (int32 OrientationIndex = 0;
			 OrientationIndex < OrientationCount;
			 ++OrientationIndex)
		{
			FRpgInventoryGridPlacement Candidate;
			Candidate.SetContainerHandle(ContainerHandle);
			Candidate.Width = UnrotatedFootprint.Width;
			Candidate.Height = UnrotatedFootprint.Height;
			Candidate.bRotated = OrientationIndex == 1;
			const FRpgInventoryGridSize OccupiedSize =
				Candidate.GetOccupiedSize();
			for (int32 Y = 0;
				 Y <= GridSize.Height - OccupiedSize.Height;
				 ++Y)
			{
				for (int32 X = 0;
					 X <= GridSize.Width - OccupiedSize.Width;
					 ++X)
				{
					Candidate.X = X;
					Candidate.Y = Y;
					if (EvaluateRulesScratchPlacement(
							Candidate,
							GridSize,
							Occupancy) ==
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

using RpgInventoryManagerRulesPlannerPrivate::
	AreRulesPlacementSnapshotsExactlyEqual;
using RpgInventoryManagerRulesPlannerPrivate::
	CanRulesInventoryRotateDefinition;
using RpgInventoryManagerRulesPlannerPrivate::
	EvaluateRulesScratchPlacement;
using RpgInventoryManagerRulesPlannerPrivate::
	FindRulesFirstFitInScratch;
using RpgInventoryManagerRulesPlannerPrivate::
	GetRulesItemContainerRuleFailureCode;
using RpgInventoryManagerRulesPlannerPrivate::
	GetRulesInventoryFootprintForDefinition;
using RpgInventoryManagerRulesPlannerPrivate::
	GetRulesInventoryMaxStackSizeForDefinition;
using RpgInventoryManagerRulesPlannerPrivate::
	IsRulesCompleteSourceSnapshot;
using RpgInventoryManagerRulesPlannerPrivate::
	IsRulesItemOwnedHandleDepthOverflow;

int32 URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	return GetRulesInventoryMaxStackSizeForDefinition(ItemDefinition);
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

	if (CapacityMode ==
			ERpgInventoryCapacityMode::AbilitySystemAttribute &&
		CapacityAttribute.IsValid())
	{
		if (const UAbilitySystemComponent* ASC =
				FindCapacityAbilitySystem())
		{
			return FMath::Max(
				0,
				FMath::RoundToInt(
					ASC->GetNumericAttribute(CapacityAttribute)));
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
		GetRulesInventoryFootprintForDefinition(ItemDef, false);
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

	if (bRotated && !CanRulesInventoryRotateDefinition(ItemDef))
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
			!IsRulesCompleteSourceSnapshot(
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
			!AreRulesPlacementSnapshotsExactlyEqual(
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
		GetRulesInventoryFootprintForDefinition(
			Subject.ItemDefinition,
			false);
	if (!DefinitionFootprint.IsValid())
	{
		Plan.Code = ERpgInventoryMutationResultCode::InvalidPlacement;
		return Plan;
	}
	const bool bDefinitionAllowsRotation =
		CanRulesInventoryRotateDefinition(Subject.ItemDefinition);

	const int32 MaxStackSize =
		GetRulesInventoryMaxStackSizeForDefinition(Subject.ItemDefinition);
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
				OutCode = GetRulesItemContainerRuleFailureCode(
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
					OutCode = GetRulesItemContainerRuleFailureCode(
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
		OutCode = EvaluateRulesScratchPlacement(
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
			!AreRulesPlacementSnapshotsExactlyEqual(
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
				!FindRulesFirstFitInScratch(
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
