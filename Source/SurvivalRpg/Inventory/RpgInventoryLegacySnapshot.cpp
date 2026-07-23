// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryLegacySnapshot.h"

#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgInventoryLegacySnapshotPrivate
{
	struct FPlannedLegacyItem
	{
		int32 SourceIndex = INDEX_NONE;
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
		FRpgInventorySavedItem SavedItem;
	};

	void ResetCurrentOutput(FRpgInventoryGraphSaveData& OutSaveData)
	{
		OutSaveData = FRpgInventoryGraphSaveData();
		OutSaveData.SchemaVersion = FRpgInventoryGraphSaveData::CurrentSchemaVersion;
		OutSaveData.Items.Reset();
	}

	bool IsCompletelyEmptyHandle(const FRpgInventoryContainerHandle& Handle)
	{
		return Handle.Root.IsNone() &&
			!Handle.ItemOwnerId.IsValid() &&
			Handle.ContainerId.IsNone() &&
			Handle.Depth == 0;
	}

	bool TryReadRootFromLegacyPlacement(
		const FRpgInventoryGridPlacement& Placement,
		FName& OutRoot,
		FString& OutError)
	{
		OutRoot = NAME_None;
		OutError.Reset();

		const FRpgInventoryContainerHandle& RawHandle = Placement.ContainerHandle;
		if (!IsCompletelyEmptyHandle(RawHandle))
		{
			if (!RawHandle.IsRoot())
			{
				OutError = TEXT("legacy placement contains an item-owned or partially invalid container handle");
				return false;
			}

			OutRoot = RawHandle.Root;
		}

		if (!Placement.ContainerId_DEPRECATED.IsNone())
		{
			if (!OutRoot.IsNone() &&
				OutRoot != Placement.ContainerId_DEPRECATED)
			{
				OutError = TEXT("legacy placement ContainerId disagrees with its root container handle");
				return false;
			}

			OutRoot = Placement.ContainerId_DEPRECATED;
		}

		return true;
	}
}

bool URpgInventoryManagerComponent::ConvertLegacyInventorySnapshot(
	ERpgLegacyInventorySnapshotVersion Version,
	const FRpgInventorySnapshot& Snapshot,
	FName FallbackRootContainerId,
	FRpgInventoryGraphSaveData& OutSaveData,
	FString& OutError) const
{
	using namespace RpgInventoryLegacySnapshotPrivate;

	ResetCurrentOutput(OutSaveData);
	OutError.Reset();

	auto Reject = [&OutSaveData, &OutError](FString Error)
	{
		ResetCurrentOutput(OutSaveData);
		OutError = MoveTemp(Error);
		return false;
	};

	if (Version != ERpgLegacyInventorySnapshotVersion::SingleSlotV0 &&
		Version != ERpgLegacyInventorySnapshotVersion::SpatialV1)
	{
		return Reject(FString::Printf(
			TEXT("Unsupported legacy inventory snapshot version %u."),
			static_cast<uint32>(Version)));
	}

	if (!Snapshot.ContainerId.IsNone() &&
		!FallbackRootContainerId.IsNone() &&
		Snapshot.ContainerId != FallbackRootContainerId)
	{
		return Reject(FString::Printf(
			TEXT("Legacy snapshot root '%s' disagrees with fallback root '%s'."),
			*Snapshot.ContainerId.ToString(),
			*FallbackRootContainerId.ToString()));
	}

	FName ResolvedRoot = !Snapshot.ContainerId.IsNone()
		? Snapshot.ContainerId
		: FallbackRootContainerId;

	TArray<FPlannedLegacyItem> SourceItems;
	SourceItems.Reserve(Snapshot.Entries.Num());
	TSet<FRpgInventoryItemId> ItemIds;
	for (int32 SourceIndex = 0; SourceIndex < Snapshot.Entries.Num(); ++SourceIndex)
	{
		const FRpgInventorySnapshotEntry& LegacyEntry = Snapshot.Entries[SourceIndex];
		if (!LegacyEntry.ItemDefinition)
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d has no item definition."),
				SourceIndex));
		}
		if (!GetDefault<URpgInventoryItemDefinition>(LegacyEntry.ItemDefinition))
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d has an unusable item definition."),
				SourceIndex));
		}
		if (LegacyEntry.StackCount <= 0)
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d has invalid stack count %d."),
				SourceIndex,
				LegacyEntry.StackCount));
		}

		const FRpgInventoryItemId ItemId = LegacyEntry.ItemId.IsValid()
			? LegacyEntry.ItemId
			: FRpgInventoryItemId(LegacyEntry.EntryId);
		if (!ItemId.IsValid())
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d has neither a valid ItemId nor EntryId."),
				SourceIndex));
		}
		if (ItemIds.Contains(ItemId))
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d resolves to duplicate ItemId '%s'."),
				SourceIndex,
				*ItemId.ToString()));
		}
		ItemIds.Add(ItemId);

		FPlannedLegacyItem& SourceItem = SourceItems.AddDefaulted_GetRef();
		SourceItem.SourceIndex = SourceIndex;
		SourceItem.ItemDefinition = LegacyEntry.ItemDefinition;
		SourceItem.SavedItem.ItemId = ItemId;
		SourceItem.SavedItem.ItemDefinition =
			TSoftClassPtr<URpgInventoryItemDefinition>(LegacyEntry.ItemDefinition);
		SourceItem.SavedItem.StackCount = LegacyEntry.StackCount;
	}

	TArray<FPlannedLegacyItem> PlannedItems;
	PlannedItems.Reserve(SourceItems.Num());

	if (Version == ERpgLegacyInventorySnapshotVersion::SpatialV1)
	{
		for (int32 SourceIndex = 0; SourceIndex < Snapshot.Entries.Num(); ++SourceIndex)
		{
			const FRpgInventorySnapshotEntry& LegacyEntry = Snapshot.Entries[SourceIndex];
			FName RowRoot;
			FString PlacementError;
			if (!TryReadRootFromLegacyPlacement(LegacyEntry.Placement, RowRoot, PlacementError))
			{
				return Reject(FString::Printf(
					TEXT("Legacy SpatialV1 row %d is invalid: %s."),
					SourceIndex,
					*PlacementError));
			}

			if (!RowRoot.IsNone())
			{
				if (!ResolvedRoot.IsNone() && ResolvedRoot != RowRoot)
				{
					return Reject(FString::Printf(
						TEXT("Legacy SpatialV1 row %d root '%s' disagrees with envelope root '%s'."),
						SourceIndex,
						*RowRoot.ToString(),
						*ResolvedRoot.ToString()));
				}

				ResolvedRoot = RowRoot;
			}

			if (ResolvedRoot.IsNone())
			{
				return Reject(FString::Printf(
					TEXT("Legacy SpatialV1 row %d has no root container in its placement or snapshot envelope."),
					SourceIndex));
			}
			if (LegacyEntry.Placement.X < 0 || LegacyEntry.Placement.Y < 0)
			{
				return Reject(FString::Printf(
					TEXT("Legacy SpatialV1 row %d has invalid coordinates (%d, %d)."),
					SourceIndex,
					LegacyEntry.Placement.X,
					LegacyEntry.Placement.Y));
			}

			const URpgInventoryFragment_SpatialItem* SpatialFragment =
				URpgInventoryItemDefinition::
					ResolveValidSpatialItemFragment(
						SourceItems[SourceIndex].ItemDefinition);
			if (!SpatialFragment)
			{
				return Reject(FString::Printf(
					TEXT("Legacy SpatialV1 row %d has no valid current SpatialItem contract."),
					SourceIndex));
			}
			if (LegacyEntry.Placement.bRotated &&
				!SpatialFragment->bAllowRotation)
			{
				return Reject(FString::Printf(
					TEXT("Legacy SpatialV1 row %d requests rotation that its current SpatialItem contract does not allow."),
					SourceIndex));
			}

			FPlannedLegacyItem& Planned = PlannedItems.Add_GetRef(SourceItems[SourceIndex]);
			Planned.SavedItem.Container = FRpgInventoryContainerHandle::MakeRoot(ResolvedRoot);
			Planned.SavedItem.Placement = FRpgInventoryGridPlacement();
			Planned.SavedItem.Placement.X = LegacyEntry.Placement.X;
			Planned.SavedItem.Placement.Y = LegacyEntry.Placement.Y;
			Planned.SavedItem.Placement.Width =
				SpatialFragment->Footprint.Width;
			Planned.SavedItem.Placement.Height =
				SpatialFragment->Footprint.Height;
			Planned.SavedItem.Placement.bRotated = LegacyEntry.Placement.bRotated;
			Planned.SavedItem.Placement.SetContainerHandle(Planned.SavedItem.Container);
		}
	}
	else
	{
		if (!Snapshot.Entries.IsEmpty() && ResolvedRoot.IsNone())
		{
			return Reject(TEXT("Legacy SingleSlotV0 snapshot requires a root container in the snapshot envelope or fallback."));
		}

		TSet<int32> SortIndices;
		TArray<int32> OrderedSourceIndices;
		OrderedSourceIndices.Reserve(Snapshot.Entries.Num());
		for (int32 SourceIndex = 0; SourceIndex < Snapshot.Entries.Num(); ++SourceIndex)
		{
			const FRpgInventorySnapshotEntry& LegacyEntry = Snapshot.Entries[SourceIndex];
			if (LegacyEntry.SortIndex < 0)
			{
				return Reject(FString::Printf(
					TEXT("Legacy SingleSlotV0 row %d has invalid SortIndex %d."),
					SourceIndex,
					LegacyEntry.SortIndex));
			}
			if (SortIndices.Contains(LegacyEntry.SortIndex))
			{
				return Reject(FString::Printf(
					TEXT("Legacy SingleSlotV0 row %d duplicates SortIndex %d."),
					SourceIndex,
					LegacyEntry.SortIndex));
			}
			SortIndices.Add(LegacyEntry.SortIndex);

			FName RowRoot;
			FString PlacementError;
			if (!TryReadRootFromLegacyPlacement(LegacyEntry.Placement, RowRoot, PlacementError))
			{
				return Reject(FString::Printf(
					TEXT("Legacy SingleSlotV0 row %d is invalid: %s."),
					SourceIndex,
					*PlacementError));
			}
			if (!RowRoot.IsNone() && RowRoot != ResolvedRoot)
			{
				return Reject(FString::Printf(
					TEXT("Legacy SingleSlotV0 row %d root '%s' disagrees with envelope root '%s'."),
					SourceIndex,
					*RowRoot.ToString(),
					*ResolvedRoot.ToString()));
			}

			OrderedSourceIndices.Add(SourceIndex);
		}

		OrderedSourceIndices.Sort(
			[&Snapshot](int32 A, int32 B)
			{
				return Snapshot.Entries[A].SortIndex < Snapshot.Entries[B].SortIndex;
			});

		const FRpgInventoryContainerHandle RootHandle =
			FRpgInventoryContainerHandle::MakeRoot(ResolvedRoot);
		TArray<FRpgInventoryGridPlacement> ScratchOccupancy;
		ScratchOccupancy.Reserve(OrderedSourceIndices.Num());
		for (int32 SourceIndex : OrderedSourceIndices)
		{
			FRpgInventoryGridPlacement Placement;
			if (!InventoryList.FindFirstFitPlacementInContainer(
				SourceItems[SourceIndex].ItemDefinition,
				RootHandle,
				ScratchOccupancy,
				Placement))
			{
				return Reject(FString::Printf(
					TEXT("Legacy SingleSlotV0 row %d (SortIndex %d) does not fit current root '%s'."),
					SourceIndex,
					Snapshot.Entries[SourceIndex].SortIndex,
					*ResolvedRoot.ToString()));
			}

			ScratchOccupancy.Add(Placement);
			FPlannedLegacyItem& Planned = PlannedItems.Add_GetRef(SourceItems[SourceIndex]);
			Planned.SavedItem.Container = RootHandle;
			Planned.SavedItem.Placement = Placement;
			Planned.SavedItem.Placement.SetContainerHandle(RootHandle);
		}
	}

	FRpgInventoryGraphSaveData Converted;
	Converted.SchemaVersion = FRpgInventoryGraphSaveData::CurrentSchemaVersion;
	Converted.Items.Reserve(PlannedItems.Num());
	for (FPlannedLegacyItem& Planned : PlannedItems)
	{
		TStrongObjectPtr<URpgInventoryItemInstance> StagedInstance(
			NewObject<URpgInventoryItemInstance>(GetTransientPackage()));
		if (!StagedInstance.IsValid())
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d could not allocate a transient item instance."),
				Planned.SourceIndex));
		}

		StagedInstance->SetItemDef(Planned.ItemDefinition);
		if (StagedInstance->GetItemDef() != Planned.ItemDefinition ||
			!StagedInstance->RestoreItemId(Planned.SavedItem.ItemId))
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d could not initialize definition and persistent identity."),
				Planned.SourceIndex));
		}

		const URpgInventoryItemDefinition* ItemCDO =
			GetDefault<URpgInventoryItemDefinition>(Planned.ItemDefinition);
		if (!ItemCDO)
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d lost its item definition during runtime-state synthesis."),
				Planned.SourceIndex));
		}
		for (const URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnInstanceCreated(StagedInstance.Get());
			}
		}

		if (StagedInstance->GetItemId() != Planned.SavedItem.ItemId ||
			!StagedInstance->ExportRuntimeState(Planned.SavedItem.RuntimeState))
		{
			return Reject(FString::Printf(
				TEXT("Legacy snapshot row %d could not synthesize current fragment runtime state."),
				Planned.SourceIndex));
		}

		Converted.Items.Add(MoveTemp(Planned.SavedItem));
	}

	OutSaveData = MoveTemp(Converted);
	OutError.Reset();
	return true;
}
