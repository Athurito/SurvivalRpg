// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Core/Game/RpgPlayerSaveData.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace RpgInventoryManagerPersistencePrivate
{
	bool ArePersistenceRuntimeStatesExactlyEqual(
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

	bool IsPersistenceItemOwnedHandleDepthOverflow(
		const FRpgInventoryContainerHandle& Handle)
	{
		return Handle.Root.IsNone() && Handle.ItemOwnerId.IsValid() &&
			!Handle.ContainerId.IsNone() &&
			Handle.Depth > RpgInventoryMaxItemOwnedDepth;
	}

	bool ArePersistencePlacementSnapshotsExactlyEqual(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}
}

using RpgInventoryManagerPersistencePrivate::
	ArePersistencePlacementSnapshotsExactlyEqual;
using RpgInventoryManagerPersistencePrivate::
	ArePersistenceRuntimeStatesExactlyEqual;
using RpgInventoryManagerPersistencePrivate::
	IsPersistenceItemOwnedHandleDepthOverflow;

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
	const bool bRestored = RestoreInventoryGraphInternal(
		SaveData,
		OutResult,
		true,
		true,
		false,
		INDEX_NONE,
		nullptr);
	return bRestored;
}

bool URpgInventoryManagerComponent::ValidateInventoryGraphForRestore(
	const FRpgInventoryGraphSaveData& SaveData,
	FRpgInventoryMutationResult& OutResult)
{
	return RestoreInventoryGraphInternal(
		SaveData,
		OutResult,
		false,
		false,
		false,
		INDEX_NONE,
		nullptr);
}

bool URpgInventoryManagerComponent::
	TryMigrateLegacyRootPlacementsForRestore(
		const FRpgInventoryGraphSaveData& SaveData,
		int32 LegacyPlayerSchemaVersion,
		FRpgInventoryGraphSaveData& OutMigratedSaveData,
		FRpgInventoryMutationResult& OutResult)
{
	OutMigratedSaveData = FRpgInventoryGraphSaveData();
	const bool bMigrated = RestoreInventoryGraphInternal(
		SaveData,
		OutResult,
		false,
		false,
		true,
		LegacyPlayerSchemaVersion,
		&OutMigratedSaveData);
	if (!bMigrated)
	{
		OutMigratedSaveData = FRpgInventoryGraphSaveData();
	}
	return bMigrated;
}

bool URpgInventoryManagerComponent::RestoreRuntimeCheckpoint(
	const FRpgInventoryGraphSaveData& SaveData,
	FRpgInventoryMutationResult& OutResult)
{
	return RestoreInventoryGraphInternal(
		SaveData,
		OutResult,
		false,
		true,
		false,
		INDEX_NONE,
		nullptr);
}

bool URpgInventoryManagerComponent::RestoreInventoryGraphInternal(
	const FRpgInventoryGraphSaveData& SaveData,
	FRpgInventoryMutationResult& OutResult,
	bool bEstablishNewMutationEpoch,
	bool bCommitValidatedGraph,
	bool bAllowLegacyRootPlacementMigration,
	int32 LegacyPlayerSchemaVersion,
	FRpgInventoryGraphSaveData* OutMigratedSaveData)
{
	OutResult = FRpgInventoryMutationResult();
	OutResult.RequestId = FGuid::NewGuid();
	OutResult.Operation = ERpgInventoryMutationOperation::Restore;
	OutResult.RequestedQuantity = SaveData.Items.Num();

	AActor* OwningActor = GetOwner();
	if (IsInventoryMutationLocked())
	{
		OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return false;
	}
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
		SaveData.Items.Num() > GetMaxEntries())
	{
		OutResult.Code = ERpgInventoryMutationResultCode::NoSpace;
		return false;
	}
	TGuardValue<bool> RestoreGuard(bIsRestoringInventoryGraph, true);
	const int32 LiveRevisionBeforeStaging = InventoryRevision;
	struct FRestoreLiveEntrySnapshot
	{
		FRpgInventoryEntry Entry;
		FRpgInventoryItemId ItemId;
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
		TObjectPtr<UObject> InstanceOuter = nullptr;
	};
	TArray<FRestoreLiveEntrySnapshot> LiveEntriesBeforeStaging;
	LiveEntriesBeforeStaging.Reserve(InventoryList.Entries.Num());
	for (const FRpgInventoryEntry& LiveEntry : InventoryList.Entries)
	{
		FRestoreLiveEntrySnapshot& Snapshot =
			LiveEntriesBeforeStaging.AddDefaulted_GetRef();
		Snapshot.Entry = LiveEntry;
		if (LiveEntry.Instance)
		{
			Snapshot.ItemId = LiveEntry.Instance->GetItemId();
			Snapshot.ItemDefinition = LiveEntry.Instance->GetItemDef();
			Snapshot.InstanceOuter = LiveEntry.Instance->GetOuter();
		}
	}
	struct FRestoreRuntimeStateBackup
	{
		TObjectPtr<URpgInventoryItemInstance> Instance;
		TArray<FRpgInventoryFragmentStatePayload> RuntimeState;
	};
	TArray<FRestoreRuntimeStateBackup> LiveRuntimeStateBackups;
	LiveRuntimeStateBackups.Reserve(InventoryList.Entries.Num());
	for (const FRpgInventoryEntry& LiveEntry : InventoryList.Entries)
	{
		if (!LiveEntry.Instance || !LiveEntry.Instance->GetItemDef())
		{
			continue;
		}
		FRestoreRuntimeStateBackup& Backup =
			LiveRuntimeStateBackups.AddDefaulted_GetRef();
		Backup.Instance = LiveEntry.Instance;
		if (!Backup.Instance->ExportRuntimeState(Backup.RuntimeState))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
	}
	struct FScopedRestoreRuntimeStateRollback
	{
		explicit FScopedRestoreRuntimeStateRollback(
			const TArray<FRestoreRuntimeStateBackup>& InBackups)
			: Backups(InBackups)
		{
		}

		~FScopedRestoreRuntimeStateRollback()
		{
			if (!bArmed)
			{
				return;
			}
			for (const FRestoreRuntimeStateBackup& Backup : Backups)
			{
				if (!Backup.Instance)
				{
					continue;
				}
				TArray<FRpgInventoryFragmentStatePayload> CurrentRuntimeState;
				if (Backup.Instance->ExportRuntimeState(CurrentRuntimeState) &&
					ArePersistenceRuntimeStatesExactlyEqual(
						Backup.RuntimeState,
						CurrentRuntimeState))
				{
					continue;
				}
				if (!Backup.Instance->ImportRuntimeState(Backup.RuntimeState))
				{
					UE_LOG(
						LogRpgInventoryManager,
						Error,
						TEXT("Failed to roll back runtime state for inventory item %s after a rejected graph restore."),
						*Backup.Instance->GetItemId().ToString());
				}
			}
		}

		void Dismiss()
		{
			bArmed = false;
		}

		const TArray<FRestoreRuntimeStateBackup>& Backups;
		bool bArmed = true;
	};
	FScopedRestoreRuntimeStateRollback RuntimeStateRollback(
		LiveRuntimeStateBackups);

	TArray<URpgInventoryManagerComponent*> SiblingInventories;
	SiblingInventories.Reset();
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
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>> StagedInstanceRoots;
	StagedInstanceRoots.Reserve(SaveData.Items.Num());
	TMap<FRpgInventoryItemId, int32> StagedIndexById;
	for (const FRpgInventorySavedItem& SavedItem : SaveData.Items)
	{
		if (IsPersistenceItemOwnedHandleDepthOverflow(SavedItem.Container) ||
			IsPersistenceItemOwnedHandleDepthOverflow(
				SavedItem.Placement.GetContainerHandle()))
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::MaxDepthExceeded;
			return false;
		}
		if (!SavedItem.ItemId.IsValid())
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		if (StagedIndexById.Contains(SavedItem.ItemId))
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::DuplicateItemId;
			return false;
		}
		if (SavedItem.StackCount <= 0)
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::StackLimitReached;
			return false;
		}
		if (!SavedItem.Container.IsValid() ||
			!SavedItem.Placement.ContainerHandle.IsValid())
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}
		if (SavedItem.Placement.X < 0 || SavedItem.Placement.Y < 0)
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}
		if (SavedItem.Placement.ContainerHandle != SavedItem.Container)
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}

		const bool bConflictsWithActorSibling =
			SiblingInventories.ContainsByPredicate(
				[this, &SavedItem](
					const URpgInventoryManagerComponent*
						CandidateInventory)
				{
					return CandidateInventory &&
						CandidateInventory != this &&
						CandidateInventory->FindItemById(
							SavedItem.ItemId) != nullptr;
				});
		if (bConflictsWithActorSibling)
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::DuplicateItemId;
			return false;
		}

		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
			SavedItem.ItemDefinition.LoadSynchronous();
		if (!ItemDefinition)
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		if (SavedItem.StackCount >
			URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(ItemDefinition))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::StackLimitReached;
			return false;
		}

		FRpgInventoryGridPlacement CanonicalPlacement;
		if (!TryNormalizePlacementForDefinition(
				ItemDefinition,
				SavedItem.Container,
				SavedItem.Placement.X,
				SavedItem.Placement.Y,
				SavedItem.Placement.bRotated,
				CanonicalPlacement))
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}

		FStagedSavedEntry& Stage = Staged.AddDefaulted_GetRef();
		Stage.Saved = &SavedItem;
		Stage.ItemDefinition = ItemDefinition;
		Stage.Placement = CanonicalPlacement;
		Stage.Instance = NewObject<URpgInventoryItemInstance>(OwningActor);
		if (!Stage.Instance)
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		StagedInstanceRoots.Emplace(Stage.Instance.Get());
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

	TArray<FRpgInventoryEntry> StagedGraphEntries;
	StagedGraphEntries.Reserve(Staged.Num());
	for (const FStagedSavedEntry& Stage : Staged)
	{
		FRpgInventoryEntry& Entry =
			StagedGraphEntries.AddDefaulted_GetRef();
		Entry.Instance = Stage.Instance;
		Entry.EntryId = Stage.CommittedEntryId;
		Entry.StackCount = Stage.Saved->StackCount;
		Entry.Placement = Stage.Placement;
	}
	FValidatedInventoryGraph StagedGraph;
	bool bStagedGraphValid = ValidateInventoryGraph(
		StagedGraphEntries,
		OwningActor,
		true,
		StagedGraph,
		OutResult.Code);
	const ERpgInventoryMutationResultCode InitialValidationCode =
		OutResult.Code;
	if (!bStagedGraphValid &&
		bAllowLegacyRootPlacementMigration &&
		(InitialValidationCode ==
			 ERpgInventoryMutationResultCode::Occupied ||
		 InitialValidationCode ==
			 ERpgInventoryMutationResultCode::ItemNotAllowed))
	{
		const URpgPlayerInventoryLayoutComponent* Layout =
			FindOwningPlayerInventoryLayout();
		if (!Layout && InitialValidationCode !=
			ERpgInventoryMutationResultCode::Occupied)
		{
			return false;
		}
		const TArray<FRpgInventorySlotGroupView> Groups =
			Layout
				? Layout->GetSlotGroups()
				: TArray<FRpgInventorySlotGroupView>();

		TArray<int32> RootEntryIndices;
		RootEntryIndices.Reserve(StagedGraphEntries.Num());
		for (int32 EntryIndex = 0;
			 EntryIndex < StagedGraphEntries.Num();
			 ++EntryIndex)
		{
			if (StagedGraphEntries[EntryIndex]
					.Placement.GetContainerHandle().IsRoot())
			{
				RootEntryIndices.Add(EntryIndex);
			}
		}

		// Preserve authored positions in deterministic container/cell order. When
		// two legacy rows claim the same cells, the larger row wins an exact tie
		// and only the later conflicting row is considered for relocation.
		RootEntryIndices.Sort(
			[&StagedGraphEntries](int32 A, int32 B)
			{
				const FRpgInventoryEntry& EntryA =
					StagedGraphEntries[A];
				const FRpgInventoryEntry& EntryB =
					StagedGraphEntries[B];
				const FString ContainerA = EntryA.Placement
					.GetContainerHandle().ToString();
				const FString ContainerB = EntryB.Placement
					.GetContainerHandle().ToString();
				if (ContainerA != ContainerB)
				{
					return ContainerA < ContainerB;
				}
				if (EntryA.Placement.Y != EntryB.Placement.Y)
				{
					return EntryA.Placement.Y < EntryB.Placement.Y;
				}
				if (EntryA.Placement.X != EntryB.Placement.X)
				{
					return EntryA.Placement.X < EntryB.Placement.X;
				}
				const FRpgInventoryGridSize SizeA =
					EntryA.Placement.GetOccupiedSize();
				const FRpgInventoryGridSize SizeB =
					EntryB.Placement.GetOccupiedSize();
				const int64 AreaA =
					static_cast<int64>(SizeA.Width) * SizeA.Height;
				const int64 AreaB =
					static_cast<int64>(SizeB.Width) * SizeB.Height;
				if (AreaA != AreaB)
				{
					return AreaA > AreaB;
				}
				return EntryA.Instance->GetItemId().ToString() <
					EntryB.Instance->GetItemId().ToString();
			});

		TMap<FRpgInventoryContainerHandle,
			TArray<FRpgInventoryGridPlacement>> RootOccupancy;
		TArray<int32> RootEntriesToRelocate;
		TSet<int32> RuleInvalidRootEntries;
		auto FitsRootScratch =
			[this, &RootOccupancy](
				const FRpgInventoryGridPlacement& Placement)
			{
				FRpgInventoryGridSize GridSize;
				if (!GetGridSizeForContainerHandle(
						Placement.GetContainerHandle(),
						GridSize))
				{
					return false;
				}
				const FRpgInventoryGridSize OccupiedSize =
					Placement.GetOccupiedSize();
				const TArray<FRpgInventoryGridPlacement>* Occupancy =
					RootOccupancy.Find(
						Placement.GetContainerHandle());
				return Placement.IsValid() && GridSize.IsValid() &&
					Placement.X >= 0 && Placement.Y >= 0 &&
					static_cast<int64>(Placement.X) +
						OccupiedSize.Width <= GridSize.Width &&
					static_cast<int64>(Placement.Y) +
						OccupiedSize.Height <= GridSize.Height &&
					(!Occupancy ||
					 !Occupancy->ContainsByPredicate(
						 [&Placement](
							 const FRpgInventoryGridPlacement& Existing)
						 {
							 return Existing.Overlaps(Placement);
						 }));
			};

		// Reserve every still-valid authored root before placing legacy rows.
		// This prevents a formerly invalid equipment row from displacing an item
		// that already satisfies the current layout contract.
		for (const int32 EntryIndex : RootEntryIndices)
		{
			FRpgInventoryEntry& Entry =
				StagedGraphEntries[EntryIndex];
			const FRpgInventoryContainerHandle ContainerHandle =
				Entry.Placement.GetContainerHandle();
			FRpgInventoryGridSize GridSize;
			if (!GetGridSizeForContainerHandle(
					ContainerHandle,
					GridSize))
			{
				OutResult.Code =
					ERpgInventoryMutationResultCode::InvalidContainer;
				return false;
			}

			const FRpgInventoryGridSize OriginalOccupiedSize =
				Entry.Placement.GetOccupiedSize();
			if (Entry.Placement.X < 0 || Entry.Placement.Y < 0 ||
				static_cast<int64>(Entry.Placement.X) +
					OriginalOccupiedSize.Width > GridSize.Width ||
				static_cast<int64>(Entry.Placement.Y) +
					OriginalOccupiedSize.Height > GridSize.Height)
			{
				// This migration is intentionally not a general corruption repair.
				OutResult.Code =
					ERpgInventoryMutationResultCode::OutOfBounds;
				return false;
			}

			const FRpgInventorySlotGroupView* SourceGroup = Layout
				? Groups.FindByPredicate(
					[&Entry](
						const FRpgInventorySlotGroupView& Candidate)
					{
						return Candidate.ContainerHandle ==
								Entry.Placement.GetContainerHandle() &&
							Candidate.ContainsCell(
								Entry.Placement.X,
								Entry.Placement.Y);
					})
				: nullptr;
			FRpgInventorySlotAddress SourceAddress;
			SourceAddress.SetContainerHandle(ContainerHandle);
			SourceAddress.X = Entry.Placement.X;
			SourceAddress.Y = Entry.Placement.Y;
			const bool bSatisfiesCurrentRule = !Layout ||
				(SourceGroup && Layout->CanItemUseSlotAddress(
					Entry.Instance,
					SourceAddress));
			if (bSatisfiesCurrentRule &&
				FitsRootScratch(Entry.Placement))
			{
				RootOccupancy.FindOrAdd(ContainerHandle).Add(
					Entry.Placement);
				continue;
			}

			if (!bSatisfiesCurrentRule)
			{
				// Schema 1 briefly allowed the test Backpack in ResourceBag. This is
				// the only evidenced semantic Gear drift; all other source contracts
				// remain fail-closed instead of being inferred from current tuning.
				if (!SourceGroup ||
					SourceGroup->GroupKind !=
						ERpgInventorySlotGroupKind::Gear ||
					LegacyPlayerSchemaVersion !=
						FRpgPlayerSaveData::MinimumSupportedSchemaVersion ||
					ContainerHandle !=
						FRpgInventoryContainerHandle::MakeRoot(
							URpgPlayerInventoryLayoutComponent::
								GearResourceBagGroupId))
				{
					OutResult.Code =
						ERpgInventoryMutationResultCode::ItemNotAllowed;
					return false;
				}
				RuleInvalidRootEntries.Add(EntryIndex);
				RootEntriesToRelocate.Add(EntryIndex);
				continue;
			}

			// A collision between two semantically valid Gear/Carry rows is not a
			// packing migration: choosing which loadout entry wins would be gameplay.
			if (Layout &&
				(!SourceGroup || SourceGroup->GroupKind !=
					ERpgInventorySlotGroupKind::Content))
			{
				OutResult.Code =
					ERpgInventoryMutationResultCode::Occupied;
				return false;
			}
			RootEntriesToRelocate.Add(EntryIndex);
		}

		auto TryFirstFitInGroup =
			[this, Layout, &FitsRootScratch](
				const FRpgInventoryEntry& Entry,
				const FRpgInventorySlotGroupView& Group,
				bool bPreferredRotation,
				FRpgInventoryGridPlacement& OutPlacement)
			{
				OutPlacement = FRpgInventoryGridPlacement();
				if (!Entry.Instance || !Group.GridSize.IsValid())
				{
					return false;
				}
				auto TryOrientation =
					[this, Layout, &Entry, &Group, &FitsRootScratch,
					 &OutPlacement](bool bRotated)
				{
					for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
					{
						for (int32 X = 0; X < Group.GridSize.Width; ++X)
						{
							FRpgInventoryGridPlacement Candidate;
							if (!TryNormalizePlacementForDefinition(
									Entry.Instance->GetItemDef(),
									Group.ContainerHandle,
									X,
									Y,
									bRotated,
									Candidate) ||
								!FitsRootScratch(Candidate))
							{
								continue;
							}

							FRpgInventorySlotAddress Address;
							Address.SetContainerHandle(
								Group.ContainerHandle);
							Address.X = X;
							Address.Y = Y;
							if (!Layout ||
								Layout->CanItemUseSlotAddress(
									Entry.Instance,
									Address))
							{
								OutPlacement = Candidate;
								return true;
							}
						}
					}
					return false;
				};

				return TryOrientation(bPreferredRotation) ||
					TryOrientation(!bPreferredRotation);
			};

		bool bMovedAnyRootEntry = false;
		for (const int32 EntryIndex : RootEntriesToRelocate)
		{
			FRpgInventoryEntry& Entry =
				StagedGraphEntries[EntryIndex];
			const FRpgInventoryContainerHandle SourceHandle =
				Entry.Placement.GetContainerHandle();
			const FRpgInventorySlotGroupView* SourceGroup =
				Groups.FindByPredicate(
					[&SourceHandle](
						const FRpgInventorySlotGroupView& Candidate)
					{
						return Candidate.ContainerHandle == SourceHandle;
					});
			const bool bRuleInvalid =
				RuleInvalidRootEntries.Contains(EntryIndex);
			FRpgInventoryGridPlacement MigratedPlacement;

			if (!bRuleInvalid && SourceGroup)
			{
				TryFirstFitInGroup(
					Entry,
					*SourceGroup,
					Entry.Placement.bRotated,
					MigratedPlacement);
			}
			else if (!bRuleInvalid && !Layout)
			{
				FRpgInventoryGridSize SourceGridSize;
				if (!GetGridSizeForContainerHandle(
						SourceHandle,
						SourceGridSize))
				{
					OutResult.Code =
						ERpgInventoryMutationResultCode::InvalidContainer;
					return false;
				}
				FRpgInventorySlotGroupView GenericRootGroup;
				GenericRootGroup.ContainerHandle = SourceHandle;
				GenericRootGroup.GridSize = SourceGridSize;
				GenericRootGroup.GroupKind =
					ERpgInventorySlotGroupKind::Content;
				TryFirstFitInGroup(
					Entry,
					GenericRootGroup,
					Entry.Placement.bRotated,
					MigratedPlacement);
			}
			else if (bRuleInvalid && SourceGroup &&
				SourceGroup->GroupKind ==
					ERpgInventorySlotGroupKind::Gear)
			{
				const FRpgInventoryContainerHandle TargetHandle =
					FRpgInventoryContainerHandle::MakeRoot(
						URpgPlayerInventoryLayoutComponent::
							GearBackpackGroupId);
				const FRpgInventorySlotGroupView* TargetGroup =
					Groups.FindByPredicate(
						[&TargetHandle](
							const FRpgInventorySlotGroupView& Group)
						{
							return Group.ContainerHandle == TargetHandle &&
								Group.GroupKind ==
									ERpgInventorySlotGroupKind::Gear;
						});
				FRpgInventorySlotAddress TargetAddress;
				TargetAddress.SetContainerHandle(TargetHandle);
				TargetAddress.X = 0;
				TargetAddress.Y = 0;
				if (!TargetGroup ||
					!Layout->CanItemUseSlotAddress(
						Entry.Instance,
						TargetAddress))
				{
					OutResult.Code =
						ERpgInventoryMutationResultCode::ItemNotAllowed;
					return false;
				}
				TryFirstFitInGroup(
					Entry,
					*TargetGroup,
					Entry.Placement.bRotated,
					MigratedPlacement);
			}

			if (!MigratedPlacement.IsValid())
			{
				OutResult.Code =
					ERpgInventoryMutationResultCode::NoSpace;
				return false;
			}

			Entry.Placement = MigratedPlacement;
			Staged[EntryIndex].Placement = MigratedPlacement;
			RootOccupancy.FindOrAdd(
				MigratedPlacement.GetContainerHandle()).Add(
				MigratedPlacement);
			bMovedAnyRootEntry = true;
		}

		if (!bMovedAnyRootEntry)
		{
			// A nested overlap/rule failure remains outside this root migration.
			OutResult.Code = InitialValidationCode;
			return false;
		}
		bStagedGraphValid = ValidateInventoryGraph(
			StagedGraphEntries,
			OwningActor,
			true,
			StagedGraph,
			OutResult.Code);
	}
	if (!bStagedGraphValid)
	{
		return false;
	}

	if (OutMigratedSaveData)
	{
		*OutMigratedSaveData = SaveData;
		if (OutMigratedSaveData->Items.Num() != Staged.Num())
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		for (int32 EntryIndex = 0;
			 EntryIndex < Staged.Num();
			 ++EntryIndex)
		{
			FRpgInventorySavedItem& MigratedItem =
				OutMigratedSaveData->Items[EntryIndex];
			MigratedItem.Container =
				Staged[EntryIndex].Placement.GetContainerHandle();
			MigratedItem.Placement = Staged[EntryIndex].Placement;
		}
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

	for (FStagedSavedEntry& Stage : Staged)
	{
		const FExistingRuntimeEntry* ExistingEntry =
			ExistingEntriesByItemId.Find(Stage.Saved->ItemId);
		if (!ExistingEntry || !ExistingEntry->Instance ||
			ExistingEntry->Instance->GetItemDef() != Stage.ItemDefinition)
		{
			continue;
		}

		if (!ExistingEntry->Instance->ImportRuntimeState(Stage.Saved->RuntimeState))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		Stage.CommittedInstance = ExistingEntry->Instance;
		Stage.CommittedEntryId = ExistingEntry->EntryId.IsValid()
			? ExistingEntry->EntryId
			: FGuid::NewGuid();
	}

	auto LiveRowsStillMatchStagingSnapshot = [&]()
	{
		if (InventoryRevision != LiveRevisionBeforeStaging ||
			InventoryList.Entries.Num() != LiveEntriesBeforeStaging.Num())
		{
			return false;
		}
		for (int32 EntryIndex = 0;
			 EntryIndex < InventoryList.Entries.Num();
			 ++EntryIndex)
		{
			const FRpgInventoryEntry& Live =
				InventoryList.Entries[EntryIndex];
			const FRestoreLiveEntrySnapshot& Snapshot =
				LiveEntriesBeforeStaging[EntryIndex];
			const FRpgInventoryEntry& Before = Snapshot.Entry;
			if (Live.Instance != Before.Instance ||
				Live.EntryId != Before.EntryId ||
				Live.StackCount != Before.StackCount ||
				!ArePersistencePlacementSnapshotsExactlyEqual(
					Live.Placement,
					Before.Placement))
			{
				return false;
			}
			if (Live.Instance &&
				(Live.Instance->GetItemId() != Snapshot.ItemId ||
				 Live.Instance->GetItemDef() != Snapshot.ItemDefinition ||
				 Live.Instance->GetOuter() != Snapshot.InstanceOuter))
			{
				return false;
			}
		}
		return true;
	};
	if (!LiveRowsStillMatchStagingSnapshot())
	{
		OutResult.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return false;
	}

	TSet<const URpgInventoryItemInstance*> ReusedLiveInstances;
	for (const FStagedSavedEntry& Stage : Staged)
	{
		if (Stage.CommittedInstance &&
			Stage.CommittedInstance != Stage.Instance)
		{
			ReusedLiveInstances.Add(Stage.CommittedInstance.Get());
		}
	}
	for (const FRestoreRuntimeStateBackup& Backup :
		LiveRuntimeStateBackups)
	{
		if (!Backup.Instance ||
			ReusedLiveInstances.Contains(Backup.Instance.Get()))
		{
			continue;
		}
		TArray<FRpgInventoryFragmentStatePayload> CurrentRuntimeState;
		if (!Backup.Instance->ExportRuntimeState(CurrentRuntimeState))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
		if (!ArePersistenceRuntimeStatesExactlyEqual(
				Backup.RuntimeState,
				CurrentRuntimeState) &&
			!Backup.Instance->ImportRuntimeState(Backup.RuntimeState))
		{
			OutResult.Code = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}
	}

	TArray<FRpgInventoryEntry> ProspectiveEntries;
	ProspectiveEntries.Reserve(Staged.Num());
	for (const FStagedSavedEntry& Stage : Staged)
	{
		FRpgInventoryEntry& Entry =
			ProspectiveEntries.AddDefaulted_GetRef();
		Entry.Instance = Stage.CommittedInstance;
		Entry.EntryId = Stage.CommittedEntryId;
		Entry.StackCount = Stage.Saved->StackCount;
		Entry.Placement = Stage.Placement;
	}
	FValidatedInventoryGraph ProspectiveGraph;
	ERpgInventoryMutationResultCode ProspectiveCode =
		ERpgInventoryMutationResultCode::Success;
	if (!ValidateInventoryGraph(
			ProspectiveEntries,
			OwningActor,
			true,
			ProspectiveGraph,
			ProspectiveCode))
	{
		OutResult.Code = ProspectiveCode;
		return false;
	}

	SiblingInventories.Reset();
	OwningActor->GetComponents(SiblingInventories);
	for (const FRpgInventoryEntry& Prospective : ProspectiveEntries)
	{
		const bool bConflictsWithActorSibling =
			SiblingInventories.ContainsByPredicate(
				[this, &Prospective](
					const URpgInventoryManagerComponent* CandidateInventory)
				{
					return CandidateInventory && CandidateInventory != this &&
						CandidateInventory->FindItemById(
							Prospective.Instance->GetItemId()) != nullptr;
				});
		if (bConflictsWithActorSibling)
		{
			OutResult.Code =
				ERpgInventoryMutationResultCode::DuplicateItemId;
			return false;
		}
	}
	if (!bCommitValidatedGraph)
	{
		OutResult.Code = ERpgInventoryMutationResultCode::Success;
		OutResult.AppliedQuantity = 0;
		return true;
	}
	RuntimeStateRollback.Dismiss();

	// A successful disk/profile restore starts a new command epoch before any
	// committed graph is observable. The deprecated compatibility import retains
	// its current command namespace because it is not a profile boundary.
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
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>> PreviousEntryRoots;
	PreviousEntryRoots.Reserve(PreviousEntries.Num());
	for (const FRpgInventoryEntry& PreviousEntry : PreviousEntries)
	{
		if (PreviousEntry.Instance)
		{
			// Removed subobjects may otherwise be collected while an earlier
			// synchronous removal notification is still being delivered.
			PreviousEntryRoots.Emplace(PreviousEntry.Instance.Get());
		}
	}
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

	// Both callback sets observe the complete committed graph. The restore guard remains active through notification
	// delivery, so a synchronous listener may enqueue follow-up work but cannot reenter this authoritative commit.
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
	MarkInventoryStateDirty();
	return true;
}
