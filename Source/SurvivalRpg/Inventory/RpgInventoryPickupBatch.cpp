// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"

#include "IPickupable.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgInventoryPickupBatchPrivate
{
	bool ArePlacementsExactlyEqual(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool IsPlacementWithinGrid(
		const FRpgInventoryGridPlacement& Placement,
		const FRpgInventoryGridSize& GridSize)
	{
		if (!Placement.IsValid() || !GridSize.IsValid())
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

	bool OverlapsScratch(
		const FRpgInventoryGridPlacement& Placement,
		const TArray<FRpgInventoryGridPlacement>& ScratchOccupancy)
	{
		return ScratchOccupancy.ContainsByPredicate(
			[&Placement](const FRpgInventoryGridPlacement& Existing)
			{
				return Existing.Overlaps(Placement);
			});
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

	int32 GetMaxStackSize(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		return URpgInventoryManagerComponent::
			GetEffectiveMaxStackSizeForDefinition(ItemDefinition);
	}

	bool TryGetRequestedQuantity(
		const FInventoryPickup& Pickup,
		int32& OutRequestedQuantity)
	{
		OutRequestedQuantity = 0;
		if (Pickup.Templates.IsEmpty() && Pickup.Instances.IsEmpty())
		{
			return false;
		}

		int64 RequestedQuantity = 0;
		for (const FPickupTemplate& Template : Pickup.Templates)
		{
			if (!Template.ItemDef || Template.StackCount <= 0)
			{
				return false;
			}
			RequestedQuantity += Template.StackCount;
			if (RequestedQuantity > MAX_int32)
			{
				return false;
			}
		}

		for (const FPickupInstance& Instance : Pickup.Instances)
		{
			if (!Instance.Item)
			{
				return false;
			}
			++RequestedQuantity;
			if (RequestedQuantity > MAX_int32)
			{
				return false;
			}
		}

		OutRequestedQuantity = static_cast<int32>(RequestedQuantity);
		return OutRequestedQuantity > 0;
	}
}

struct URpgInventoryManagerComponent::FPreparedPickupBatch
{
	struct FMergeContribution
	{
		TObjectPtr<URpgInventoryItemInstance> IncomingInstance = nullptr;
		int32 Quantity = 0;
	};

	struct FEntry
	{
		TObjectPtr<URpgInventoryItemInstance> Instance = nullptr;
		FRpgInventoryItemId ItemId;
		FGuid EntryId;
		int32 BeforeCount = 0;
		int32 PlannedCount = 0;
		FRpgInventoryGridPlacement Placement;
		bool bExisting = false;
		TArray<FMergeContribution> MergeContributions;
	};

	struct FPayloadSource
	{
		TObjectPtr<URpgInventoryItemInstance> OriginalInstance = nullptr;
		TObjectPtr<URpgInventoryItemInstance> StagedInstance = nullptr;
		FRpgInventoryItemId ExpectedOriginalItemId;
		TArray<FRpgInventoryFragmentStatePayload> ExpectedRuntimeState;
		bool bWasCloned = false;
	};

	int32 BaseRevision = INDEX_NONE;
	int32 OriginalEntryCount = 0;
	int32 RequestedQuantity = 0;
	TArray<FEntry> Entries;
	TArray<int32> RepresentativeEntryIndices;
	TArray<FPayloadSource> PayloadSources;
	TArray<TStrongObjectPtr<URpgInventoryItemInstance>> InstanceRoots;
};

bool URpgInventoryManagerComponent::CanAddPickupBatch(
	const FInventoryPickup& Pickup) const
{
	if (IsInventoryMutationLocked())
	{
		return false;
	}

	TGuardValue<bool> PlanningGuard(bIsPlanningPickupBatch, true);
	FPreparedPickupBatch Prepared;
	ERpgInventoryMutationResultCode Code =
		ERpgInventoryMutationResultCode::InvalidRequest;
	return PreparePickupBatch(
		Pickup,
		GetTransientPackage(),
		Prepared,
		Code);
}

bool URpgInventoryManagerComponent::PreparePickupBatch(
	const FInventoryPickup& Pickup,
	UObject* StagingOuter,
	FPreparedPickupBatch& OutPrepared,
	ERpgInventoryMutationResultCode& OutCode) const
{
	using namespace RpgInventoryPickupBatchPrivate;

	OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
	OutPrepared.BaseRevision = InventoryRevision;
	OutPrepared.OriginalEntryCount = InventoryList.Entries.Num();
	OutPrepared.RequestedQuantity = 0;
	OutPrepared.Entries.Reset();
	OutPrepared.RepresentativeEntryIndices.Reset();
	OutPrepared.PayloadSources.Reset();
	OutPrepared.InstanceRoots.Reset();

	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		OutCode = ERpgInventoryMutationResultCode::AuthorityRequired;
		return false;
	}
	if (!StagingOuter ||
		!TryGetRequestedQuantity(Pickup, OutPrepared.RequestedQuantity))
	{
		return false;
	}
	FValidatedInventoryGraph LiveGraph;
	if (!ValidateLiveInventoryGraph(true, LiveGraph, OutCode))
	{
		return false;
	}

	OutPrepared.Entries.Reserve(
		InventoryList.Entries.Num() + Pickup.Templates.Num() +
		Pickup.Instances.Num());
	OutPrepared.RepresentativeEntryIndices.Reserve(
		Pickup.Templates.Num() + Pickup.Instances.Num());

	TSet<FGuid> ReservedEntryIds;
	TSet<FRpgInventoryItemId> CurrentInventoryItemIds;
	TArray<FRpgInventoryGridPlacement> ScratchOccupancy;
	ScratchOccupancy.Reserve(InventoryList.Entries.Num() + 8);
	for (const FRpgInventoryEntry& Existing : InventoryList.Entries)
	{
		if (!Existing.Instance || !Existing.EntryId.IsValid() ||
			!Existing.Instance->GetItemDef() ||
			!Existing.Instance->GetItemId().IsValid() ||
			Existing.StackCount <= 0 ||
			Existing.StackCount > GetMaxStackSize(
				Existing.Instance->GetItemDef()) ||
			!Existing.Placement.IsValid() ||
			ReservedEntryIds.Contains(Existing.EntryId) ||
			CurrentInventoryItemIds.Contains(
				Existing.Instance->GetItemId()) ||
			!InventoryList.IsPlacementWithinGrid(Existing.Placement) ||
			OverlapsScratch(Existing.Placement, ScratchOccupancy))
		{
			OutCode = ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		FRpgInventoryGridPlacement NormalizedPlacement;
		if (!TryNormalizePlacementForDefinition(
				Existing.Instance->GetItemDef(),
				Existing.Placement.GetContainerHandle(),
				Existing.Placement.X,
				Existing.Placement.Y,
				Existing.Placement.bRotated,
				NormalizedPlacement) ||
			!ArePlacementsExactlyEqual(
				NormalizedPlacement,
				Existing.Placement))
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidPlacement;
			return false;
		}

		ERpgInventoryMutationResultCode GraphCode =
			ERpgInventoryMutationResultCode::Success;
		if (!ValidatePlacementGraphRules(
				Existing,
				Existing.Placement,
				GraphCode))
		{
			OutCode = GraphCode;
			return false;
		}

		ReservedEntryIds.Add(Existing.EntryId);
		CurrentInventoryItemIds.Add(Existing.Instance->GetItemId());
		ScratchOccupancy.Add(Existing.Placement);

		FPreparedPickupBatch::FEntry& PreparedEntry =
			OutPrepared.Entries.AddDefaulted_GetRef();
		PreparedEntry.Instance = Existing.Instance;
		PreparedEntry.ItemId = Existing.Instance->GetItemId();
		PreparedEntry.EntryId = Existing.EntryId;
		PreparedEntry.BeforeCount = Existing.StackCount;
		PreparedEntry.PlannedCount = Existing.StackCount;
		PreparedEntry.Placement = Existing.Placement;
		PreparedEntry.bExisting = true;
	}

	TSet<FRpgInventoryItemId> ActorItemIds = CurrentInventoryItemIds;
	TArray<URpgInventoryManagerComponent*> ActorInventories;
	OwningActor->GetComponents(ActorInventories);
	for (const URpgInventoryManagerComponent* SiblingInventory : ActorInventories)
	{
		if (!SiblingInventory || SiblingInventory == this)
		{
			continue;
		}

		for (const FRpgInventoryEntryView& SiblingEntry :
			SiblingInventory->GetAllEntries())
		{
			if (!SiblingEntry.Instance || !SiblingEntry.ItemId.IsValid())
			{
				OutCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
			if (ActorItemIds.Contains(SiblingEntry.ItemId))
			{
				OutCode = ERpgInventoryMutationResultCode::DuplicateItemId;
				return false;
			}
			ActorItemIds.Add(SiblingEntry.ItemId);
		}
	}

	TSet<FRpgInventoryItemId> PlannedNewItemIds;
	TSet<const URpgInventoryItemInstance*> SeenPayloadInstances;

	auto StageDefinitionInstance =
		[&OutPrepared, StagingOuter](
			TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
			-> URpgInventoryItemInstance*
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition
			? GetDefault<URpgInventoryItemDefinition>(ItemDefinition)
			: nullptr;
		if (!ItemCDO)
		{
			return nullptr;
		}

		URpgInventoryItemInstance* StagedInstance =
			NewObject<URpgInventoryItemInstance>(StagingOuter);
		if (!StagedInstance)
		{
			return nullptr;
		}

		OutPrepared.InstanceRoots.Emplace(StagedInstance);
		StagedInstance->SetItemDef(ItemDefinition);
		for (const URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnInstanceCreated(StagedInstance);
			}
		}
		return StagedInstance->GetItemId().IsValid()
			? StagedInstance
			: nullptr;
	};

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

	auto ReserveNewItemIdentity =
		[&ActorItemIds, &PlannedNewItemIds](
			URpgInventoryItemInstance* Instance,
			bool bMayRegenerateIdentity)
	{
		if (!Instance || !Instance->GetItemId().IsValid())
		{
			return false;
		}

		if (!ActorItemIds.Contains(Instance->GetItemId()) &&
			!PlannedNewItemIds.Contains(Instance->GetItemId()))
		{
			PlannedNewItemIds.Add(Instance->GetItemId());
			return true;
		}
		if (!bMayRegenerateIdentity)
		{
			return false;
		}

		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const FRpgInventoryItemId Candidate =
				FRpgInventoryItemId::NewId();
			if (Candidate.IsValid() &&
				!ActorItemIds.Contains(Candidate) &&
				!PlannedNewItemIds.Contains(Candidate) &&
				Instance->RestoreItemId(Candidate))
			{
				PlannedNewItemIds.Add(Candidate);
				return true;
			}
		}
		return false;
	};

	auto BuildSearchContainers =
		[this](
			const URpgInventoryItemInstance* Incoming,
			TArray<FRpgInventoryContainerHandle>& OutContainers)
	{
		OutContainers.Reset();
		if (!Incoming || !Incoming->GetItemDef())
		{
			return;
		}

		if (const URpgPlayerInventoryLayoutComponent* Layout =
			FindOwningPlayerInventoryLayout())
		{
			for (const FRpgInventorySlotGroupView& Group :
				Layout->GetSlotGroups())
			{
				if (Group.GroupKind ==
						ERpgInventorySlotGroupKind::Content &&
					Group.Rule.AllowsItem(Incoming))
				{
					OutContainers.AddUnique(Group.ContainerHandle);
				}
			}
		}
		else if (!DefaultContainerId.IsNone())
		{
			OutContainers.Add(
				FRpgInventoryContainerHandle::MakeRoot(
					DefaultContainerId));
		}
	};

	auto FindPlacement =
		[this, &ScratchOccupancy, &BuildSearchContainers](
			URpgInventoryItemInstance* Incoming,
			int32 StackCount,
			FRpgInventoryGridPlacement& OutPlacement,
			ERpgInventoryMutationResultCode& OutPlacementCode)
	{
		OutPlacement = FRpgInventoryGridPlacement();
		OutPlacementCode = ERpgInventoryMutationResultCode::NoSpace;
		TArray<FRpgInventoryContainerHandle> SearchContainers;
		BuildSearchContainers(Incoming, SearchContainers);
		if (SearchContainers.IsEmpty())
		{
			OutPlacementCode =
				ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}

		for (const FRpgInventoryContainerHandle& Container :
			SearchContainers)
		{
			FRpgInventoryGridSize GridSize;
			if (!GetGridSizeForContainerHandle(Container, GridSize) ||
				!GridSize.IsValid())
			{
				OutPlacementCode =
					ERpgInventoryMutationResultCode::InvalidContainer;
				continue;
			}

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
						if (!TryNormalizePlacementForDefinition(
								Incoming->GetItemDef(),
								Container,
								X,
								Y,
								bRotated,
								Candidate) ||
							!IsPlacementWithinGrid(
								Candidate,
								GridSize) ||
							OverlapsScratch(
								Candidate,
								ScratchOccupancy))
						{
							continue;
						}

						FRpgInventoryPlacementQuery Query;
						Query.Purpose =
							ERpgInventoryPlacementPurpose::Add;
						Query.Search =
							ERpgInventoryPlacementSearch::Exact;
						Query.Subject =
							FRpgInventoryPlacementSubject::
								FromDetachedInstance(
									Incoming,
									StackCount);
						Query.TargetContainer = Container;
						Query.ExactPlacement = Candidate;
						const FRpgInventoryPlacementPlan Plan =
							EvaluatePlacement(Query);
						OutPlacementCode = Plan.Code;
						if (Plan.Code ==
								ERpgInventoryMutationResultCode::Success &&
							Plan.AppliedQuantity == StackCount &&
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
						if (Plan.Code ==
							ERpgInventoryMutationResultCode::Success)
						{
							OutPlacementCode =
								ERpgInventoryMutationResultCode::InternalError;
							return false;
						}
					}
				}
			}
		}
		return false;
	};

	auto PlanIncoming =
		[this,
		 &OutPrepared,
		 &ScratchOccupancy,
		 &ActorItemIds,
		 &PlannedNewItemIds,
		 &BuildSearchContainers,
		 &FindPlacement,
		 &MakeUniqueEntryId,
		 &ReserveNewItemIdentity,
		 &StageDefinitionInstance](
			URpgInventoryItemInstance* Prototype,
			int32 RequestedCount,
			bool bAllowStackMerge,
			bool bMayCreateAdditionalInstances,
			bool bMayRegeneratePrototypeIdentity,
			int32& OutRepresentativeIndex,
			ERpgInventoryMutationResultCode& OutPlanCode)
	{
		OutRepresentativeIndex = INDEX_NONE;
		OutPlanCode = ERpgInventoryMutationResultCode::InvalidRequest;
		if (!Prototype || !Prototype->GetItemDef() ||
			!Prototype->GetItemId().IsValid() || RequestedCount <= 0)
		{
			return false;
		}

		const int32 MaxStackSize =
			GetMaxStackSize(Prototype->GetItemDef());
		if (MaxStackSize <= 0 ||
			(!bMayCreateAdditionalInstances &&
			 RequestedCount > MaxStackSize))
		{
			OutPlanCode =
				ERpgInventoryMutationResultCode::StackLimitReached;
			return false;
		}

		TArray<FRpgInventoryContainerHandle> SearchContainers;
		BuildSearchContainers(Prototype, SearchContainers);
		if (SearchContainers.IsEmpty())
		{
			OutPlanCode =
				ERpgInventoryMutationResultCode::InvalidContainer;
			return false;
		}

		int32 RemainingCount = RequestedCount;
		if (bAllowStackMerge &&
			!Prototype->FindFragmentByClass<
			URpgInventoryFragment_ItemContainer>() && MaxStackSize > 1)
		{
			TArray<int32> MergeCandidates;
			for (int32 EntryIndex = 0;
				 EntryIndex < OutPrepared.Entries.Num();
				 ++EntryIndex)
			{
				const FPreparedPickupBatch::FEntry& Entry =
					OutPrepared.Entries[EntryIndex];
				if (Entry.Instance && Entry.PlannedCount > 0 &&
					Entry.PlannedCount < MaxStackSize &&
					SearchContainers.Contains(
						Entry.Placement.GetContainerHandle()) &&
					Prototype->IsStackCompatibleWith(
						Entry.Instance))
				{
					MergeCandidates.Add(EntryIndex);
				}
			}

			MergeCandidates.Sort(
				[&OutPrepared, &SearchContainers](int32 A, int32 B)
				{
					const FPreparedPickupBatch::FEntry& EntryA =
						OutPrepared.Entries[A];
					const FPreparedPickupBatch::FEntry& EntryB =
						OutPrepared.Entries[B];
					const int32 RankA = SearchContainers.IndexOfByKey(
						EntryA.Placement.GetContainerHandle());
					const int32 RankB = SearchContainers.IndexOfByKey(
						EntryB.Placement.GetContainerHandle());
					if (RankA != RankB)
					{
						return RankA < RankB;
					}
					if (EntryA.Placement.Y != EntryB.Placement.Y)
					{
						return EntryA.Placement.Y < EntryB.Placement.Y;
					}
					if (EntryA.Placement.X != EntryB.Placement.X)
					{
						return EntryA.Placement.X < EntryB.Placement.X;
					}
					return EntryA.EntryId.ToString() <
						EntryB.EntryId.ToString();
				});

			for (const int32 EntryIndex : MergeCandidates)
			{
				if (RemainingCount <= 0)
				{
					break;
				}

				FPreparedPickupBatch::FEntry& Target =
					OutPrepared.Entries[EntryIndex];
				const int32 MergeQuantity = FMath::Min(
					RemainingCount,
					MaxStackSize - Target.PlannedCount);
				if (MergeQuantity <= 0)
				{
					continue;
				}

				Target.PlannedCount += MergeQuantity;
				FPreparedPickupBatch::FMergeContribution& Contribution =
					Target.MergeContributions.AddDefaulted_GetRef();
				Contribution.IncomingInstance = Prototype;
				Contribution.Quantity = MergeQuantity;
				RemainingCount -= MergeQuantity;
				if (OutRepresentativeIndex == INDEX_NONE)
				{
					OutRepresentativeIndex = EntryIndex;
				}
			}
		}

		bool bPrototypeAvailable = true;
		while (RemainingCount > 0)
		{
			if (!IsCapacityUnlimited() &&
				InventoryList.Entries.Num() + PlannedNewItemIds.Num() >=
					GetMaxEntries())
			{
				OutPlanCode = ERpgInventoryMutationResultCode::NoSpace;
				return false;
			}

			URpgInventoryItemInstance* PlacementInstance = Prototype;
			bool bMayRegenerateIdentity =
				bMayRegeneratePrototypeIdentity;
			if (!bPrototypeAvailable)
			{
				if (!bMayCreateAdditionalInstances)
				{
					OutPlanCode =
						ERpgInventoryMutationResultCode::StackLimitReached;
					return false;
				}
				PlacementInstance = StageDefinitionInstance(
					Prototype->GetItemDef());
				bMayRegenerateIdentity = true;
			}
			if (!PlacementInstance ||
				!ReserveNewItemIdentity(
					PlacementInstance,
					bMayRegenerateIdentity))
			{
				OutPlanCode =
					ERpgInventoryMutationResultCode::DuplicateItemId;
				return false;
			}

			const int32 StackCount =
				FMath::Min(MaxStackSize, RemainingCount);
			FRpgInventoryGridPlacement Placement;
			ERpgInventoryMutationResultCode PlacementCode =
				ERpgInventoryMutationResultCode::NoSpace;
			if (!FindPlacement(
					PlacementInstance,
					StackCount,
					Placement,
					PlacementCode))
			{
				OutPlanCode = PlacementCode;
				return false;
			}

			const FGuid EntryId = MakeUniqueEntryId();
			if (!EntryId.IsValid())
			{
				OutPlanCode =
					ERpgInventoryMutationResultCode::InternalError;
				return false;
			}

			FPreparedPickupBatch::FEntry& NewEntry =
				OutPrepared.Entries.AddDefaulted_GetRef();
			NewEntry.Instance = PlacementInstance;
			NewEntry.ItemId = PlacementInstance->GetItemId();
			NewEntry.EntryId = EntryId;
			NewEntry.BeforeCount = 0;
			NewEntry.PlannedCount = StackCount;
			NewEntry.Placement = Placement;
			NewEntry.bExisting = false;
			ScratchOccupancy.Add(Placement);
			const int32 NewEntryIndex =
				OutPrepared.Entries.Num() - 1;
			if (OutRepresentativeIndex == INDEX_NONE)
			{
				OutRepresentativeIndex = NewEntryIndex;
			}

			RemainingCount -= StackCount;
			bPrototypeAvailable = false;
		}

		OutPlanCode = ERpgInventoryMutationResultCode::Success;
		return OutRepresentativeIndex != INDEX_NONE;
	};

	for (const FPickupTemplate& Template : Pickup.Templates)
	{
		URpgInventoryItemInstance* Prototype =
			StageDefinitionInstance(Template.ItemDef);
		int32 RepresentativeIndex = INDEX_NONE;
		ERpgInventoryMutationResultCode PlanCode =
			ERpgInventoryMutationResultCode::InvalidRequest;
		if (!Prototype ||
			!PlanIncoming(
				Prototype,
				Template.StackCount,
				true,
				true,
				true,
				RepresentativeIndex,
				PlanCode))
		{
			OutCode = PlanCode;
			return false;
		}
		OutPrepared.RepresentativeEntryIndices.Add(
			RepresentativeIndex);
	}

	for (const FPickupInstance& PickupInstance : Pickup.Instances)
	{
		URpgInventoryItemInstance* SourceInstance = PickupInstance.Item;
		if (!SourceInstance || !SourceInstance->GetItemDef() ||
			!SourceInstance->GetItemId().IsValid() ||
			SeenPayloadInstances.Contains(SourceInstance) ||
			IsItemManagedByAnyInventory(SourceInstance))
		{
			OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
			return false;
		}
		SeenPayloadInstances.Add(SourceInstance);
		OutPrepared.InstanceRoots.Emplace(SourceInstance);

		const bool bReuseOwnedInstance =
			SourceInstance->GetOuter() == OwningActor;
		URpgInventoryItemInstance* StagedInstance = SourceInstance;
		if (bReuseOwnedInstance)
		{
			if (!InventoryList.CanInsertOwnedInstance(SourceInstance) ||
				HasItemIdentityConflictInAnyInventory(SourceInstance))
			{
				OutCode =
					ERpgInventoryMutationResultCode::DuplicateItemId;
				return false;
			}
		}
		else
		{
			StagedInstance = StageDefinitionInstance(
				SourceInstance->GetItemDef());
			if (!StagedInstance ||
				!StagedInstance->CopyRuntimeStateFrom(
					SourceInstance,
					false))
			{
				OutCode =
					ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
		}

		FPreparedPickupBatch::FPayloadSource& PreparedSource =
			OutPrepared.PayloadSources.AddDefaulted_GetRef();
		PreparedSource.OriginalInstance = SourceInstance;
		PreparedSource.StagedInstance = StagedInstance;
		PreparedSource.ExpectedOriginalItemId = SourceInstance->GetItemId();
		PreparedSource.bWasCloned = !bReuseOwnedInstance;
		if (PreparedSource.bWasCloned)
		{
			TArray<FRpgInventoryFragmentStatePayload> StagedRuntimeState;
			if (!SourceInstance->ExportRuntimeState(
					PreparedSource.ExpectedRuntimeState) ||
				!StagedInstance->ExportRuntimeState(StagedRuntimeState) ||
				!AreRuntimeStatesExactlyEqual(
					PreparedSource.ExpectedRuntimeState,
					StagedRuntimeState))
			{
				OutCode = ERpgInventoryMutationResultCode::InternalError;
				return false;
			}
		}

		int32 RepresentativeIndex = INDEX_NONE;
		ERpgInventoryMutationResultCode PlanCode =
			ERpgInventoryMutationResultCode::InvalidRequest;
		if (!PlanIncoming(
				StagedInstance,
				1,
				false,
				false,
				!bReuseOwnedInstance,
				RepresentativeIndex,
				PlanCode))
		{
			OutCode = PlanCode;
			return false;
		}
		OutPrepared.RepresentativeEntryIndices.Add(
			RepresentativeIndex);
	}

	if (InventoryRevision != OutPrepared.BaseRevision)
	{
		OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
		return false;
	}

	OutCode = ERpgInventoryMutationResultCode::Success;
	return true;
}

bool URpgInventoryManagerComponent::RevalidatePickupBatch(
	const FPreparedPickupBatch& Prepared,
	ERpgInventoryMutationResultCode& OutCode) const
{
	using namespace RpgInventoryPickupBatchPrivate;

	OutCode = ERpgInventoryMutationResultCode::InvalidRequest;
	const AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		OutCode = ERpgInventoryMutationResultCode::AuthorityRequired;
		return false;
	}
	if (Prepared.BaseRevision != InventoryRevision ||
		Prepared.OriginalEntryCount != InventoryList.Entries.Num() ||
		Prepared.RequestedQuantity <= 0)
	{
		OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
		return false;
	}
	FValidatedInventoryGraph LiveGraph;
	if (!ValidateLiveInventoryGraph(true, LiveGraph, OutCode))
	{
		return false;
	}

	for (const FPreparedPickupBatch::FPayloadSource& Source :
		Prepared.PayloadSources)
	{
		if (!Source.OriginalInstance || !Source.StagedInstance ||
			!Source.ExpectedOriginalItemId.IsValid() ||
			Source.OriginalInstance->GetItemId() !=
				Source.ExpectedOriginalItemId ||
			!Source.OriginalInstance->GetItemDef() ||
			Source.OriginalInstance->GetItemDef() !=
				Source.StagedInstance->GetItemDef() ||
			IsItemManagedByAnyInventory(Source.OriginalInstance))
		{
			OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
			return false;
		}

		if (Source.bWasCloned)
		{
			TArray<FRpgInventoryFragmentStatePayload> CurrentOriginalState;
			TArray<FRpgInventoryFragmentStatePayload> CurrentStagedState;
			if (Source.OriginalInstance == Source.StagedInstance ||
				!Source.OriginalInstance->ExportRuntimeState(
					CurrentOriginalState) ||
				!Source.StagedInstance->ExportRuntimeState(
					CurrentStagedState) ||
				!AreRuntimeStatesExactlyEqual(
					Source.ExpectedRuntimeState,
					CurrentOriginalState) ||
				!AreRuntimeStatesExactlyEqual(
					Source.ExpectedRuntimeState,
					CurrentStagedState))
			{
				OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
				return false;
			}
		}
		else if (Source.OriginalInstance != Source.StagedInstance)
		{
			OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
			return false;
		}
	}

	auto CanUseCurrentContentPlacement =
		[this](
			const URpgInventoryItemInstance* Incoming,
			const FRpgInventoryGridPlacement& Placement)
	{
		if (!Incoming || !Placement.IsValid())
		{
			return false;
		}

		if (const URpgPlayerInventoryLayoutComponent* Layout =
			FindOwningPlayerInventoryLayout())
		{
			for (const FRpgInventorySlotGroupView& Group :
				Layout->GetSlotGroups())
			{
				if (Group.ContainerHandle !=
						Placement.GetContainerHandle() ||
					Group.GroupKind !=
						ERpgInventorySlotGroupKind::Content ||
					!Group.ContainsCell(Placement.X, Placement.Y) ||
					!Group.Rule.AllowsItem(Incoming))
				{
					continue;
				}

				FRpgInventorySlotAddress Address;
				Address.SetContainerHandle(
					Placement.GetContainerHandle());
				Address.X = Placement.X;
				Address.Y = Placement.Y;
				return Layout->CanItemUseSlotAddress(
					const_cast<URpgInventoryItemInstance*>(Incoming),
					Address);
			}
			return false;
		}

		return !DefaultContainerId.IsNone() &&
			Placement.GetContainerHandle() ==
				FRpgInventoryContainerHandle::MakeRoot(
					DefaultContainerId);
	};

	int32 NewEntryCount = 0;
	TArray<FRpgInventoryGridPlacement> ScratchOccupancy;
	ScratchOccupancy.Reserve(Prepared.Entries.Num());
	TSet<FGuid> EntryIds;
	TSet<FRpgInventoryItemId> ActorItemIds;
	for (const FPreparedPickupBatch::FEntry& PreparedEntry :
		Prepared.Entries)
	{
		if (!PreparedEntry.bExisting)
		{
			++NewEntryCount;
			continue;
		}

		const FRpgInventoryEntry* Current =
			InventoryList.FindEntryByEntryId(PreparedEntry.EntryId);
		if (!Current || Current->Instance != PreparedEntry.Instance ||
			!Current->Instance ||
			Current->Instance->GetItemId() != PreparedEntry.ItemId ||
			Current->StackCount != PreparedEntry.BeforeCount ||
			!ArePlacementsExactlyEqual(
				Current->Placement,
				PreparedEntry.Placement) ||
			EntryIds.Contains(Current->EntryId) ||
			ActorItemIds.Contains(Current->Instance->GetItemId()))
		{
			OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
			return false;
		}

		FRpgInventoryGridPlacement NormalizedPlacement;
		ERpgInventoryMutationResultCode GraphCode =
			ERpgInventoryMutationResultCode::Success;
		if (!TryNormalizePlacementForDefinition(
				Current->Instance->GetItemDef(),
				Current->Placement.GetContainerHandle(),
				Current->Placement.X,
				Current->Placement.Y,
				Current->Placement.bRotated,
				NormalizedPlacement) ||
			!ArePlacementsExactlyEqual(
				NormalizedPlacement,
				Current->Placement) ||
			!InventoryList.IsPlacementWithinGrid(Current->Placement) ||
			!ValidatePlacementGraphRules(
				*Current,
				Current->Placement,
				GraphCode))
		{
			OutCode = GraphCode == ERpgInventoryMutationResultCode::Success
				? ERpgInventoryMutationResultCode::InvalidPlacement
				: GraphCode;
			return false;
		}

		const int32 MaxStackSize =
			GetMaxStackSize(Current->Instance->GetItemDef());
		if (PreparedEntry.PlannedCount < Current->StackCount ||
			PreparedEntry.PlannedCount > MaxStackSize)
		{
			OutCode =
				ERpgInventoryMutationResultCode::StackLimitReached;
			return false;
		}
		for (const FPreparedPickupBatch::FMergeContribution& Contribution :
			PreparedEntry.MergeContributions)
		{
			if (!Contribution.IncomingInstance ||
				Contribution.Quantity <= 0 ||
				!CanUseCurrentContentPlacement(
					Contribution.IncomingInstance,
					Current->Placement) ||
				!Contribution.IncomingInstance->IsStackCompatibleWith(
					Current->Instance))
			{
				OutCode =
					ERpgInventoryMutationResultCode::StackIncompatible;
				return false;
			}
		}

		EntryIds.Add(Current->EntryId);
		ActorItemIds.Add(Current->Instance->GetItemId());
		ScratchOccupancy.Add(Current->Placement);
	}

	TArray<URpgInventoryManagerComponent*> ActorInventories;
	const_cast<AActor*>(OwningActor)->GetComponents(ActorInventories);
	for (const URpgInventoryManagerComponent* SiblingInventory : ActorInventories)
	{
		if (!SiblingInventory || SiblingInventory == this)
		{
			continue;
		}
		for (const FRpgInventoryEntryView& SiblingEntry :
			SiblingInventory->GetAllEntries())
		{
			if (!SiblingEntry.Instance || !SiblingEntry.ItemId.IsValid() ||
				ActorItemIds.Contains(SiblingEntry.ItemId))
			{
				OutCode =
					ERpgInventoryMutationResultCode::DuplicateItemId;
				return false;
			}
			ActorItemIds.Add(SiblingEntry.ItemId);
		}
	}

	if (!IsCapacityUnlimited() &&
		InventoryList.Entries.Num() + NewEntryCount > GetMaxEntries())
	{
		OutCode = ERpgInventoryMutationResultCode::NoSpace;
		return false;
	}

	TSet<const URpgInventoryItemInstance*> NewInstances;
	for (const FPreparedPickupBatch::FEntry& PreparedEntry :
		Prepared.Entries)
	{
		if (PreparedEntry.bExisting)
		{
			continue;
		}
		if (!PreparedEntry.Instance ||
			PreparedEntry.Instance->GetOuter() != OwningActor ||
			!PreparedEntry.Instance->GetItemDef() ||
			!PreparedEntry.ItemId.IsValid() ||
			PreparedEntry.Instance->GetItemId() != PreparedEntry.ItemId ||
			PreparedEntry.PlannedCount <= 0 ||
			PreparedEntry.PlannedCount > GetMaxStackSize(
				PreparedEntry.Instance->GetItemDef()) ||
			!PreparedEntry.EntryId.IsValid() ||
			EntryIds.Contains(PreparedEntry.EntryId) ||
			ActorItemIds.Contains(
				PreparedEntry.Instance->GetItemId()) ||
			NewInstances.Contains(PreparedEntry.Instance) ||
			!InventoryList.CanInsertOwnedInstance(
				PreparedEntry.Instance) ||
			OverlapsScratch(
				PreparedEntry.Placement,
				ScratchOccupancy))
		{
			OutCode = EntryIds.Contains(PreparedEntry.EntryId)
				? ERpgInventoryMutationResultCode::DuplicateEntryId
				: ActorItemIds.Contains(
					PreparedEntry.Instance
						? PreparedEntry.Instance->GetItemId()
						: FRpgInventoryItemId())
					? ERpgInventoryMutationResultCode::DuplicateItemId
					: ERpgInventoryMutationResultCode::InternalError;
			return false;
		}

		FRpgInventoryPlacementQuery Query;
		Query.Purpose = ERpgInventoryPlacementPurpose::Add;
		Query.Search = ERpgInventoryPlacementSearch::Exact;
		Query.Subject =
			FRpgInventoryPlacementSubject::FromDetachedInstance(
				PreparedEntry.Instance,
				PreparedEntry.PlannedCount);
		Query.TargetContainer =
			PreparedEntry.Placement.GetContainerHandle();
		Query.ExactPlacement = PreparedEntry.Placement;
		const FRpgInventoryPlacementPlan Plan = EvaluatePlacement(Query);
		if (Plan.Code != ERpgInventoryMutationResultCode::Success ||
			Plan.AppliedQuantity != PreparedEntry.PlannedCount ||
			Plan.Steps.Num() != 1 ||
			Plan.Steps[0].Resolution !=
				ERpgInventoryPlacementResolution::Place ||
			!ArePlacementsExactlyEqual(
				Plan.Steps[0].Placement,
				PreparedEntry.Placement))
		{
			OutCode = Plan.Code ==
					ERpgInventoryMutationResultCode::Success
				? ERpgInventoryMutationResultCode::InternalError
				: Plan.Code;
			return false;
		}

		for (const FPreparedPickupBatch::FMergeContribution& Contribution :
			PreparedEntry.MergeContributions)
		{
			if (!Contribution.IncomingInstance ||
				Contribution.Quantity <= 0 ||
				!Contribution.IncomingInstance->IsStackCompatibleWith(
					PreparedEntry.Instance))
			{
				OutCode =
					ERpgInventoryMutationResultCode::StackIncompatible;
				return false;
			}
		}

		EntryIds.Add(PreparedEntry.EntryId);
		ActorItemIds.Add(PreparedEntry.Instance->GetItemId());
		NewInstances.Add(PreparedEntry.Instance);
		ScratchOccupancy.Add(PreparedEntry.Placement);
	}

	TArray<FRpgInventoryEntry> ProjectedEntries;
	ProjectedEntries.Reserve(Prepared.Entries.Num());
	for (const FPreparedPickupBatch::FEntry& PreparedEntry :
		Prepared.Entries)
	{
		FRpgInventoryEntry& Entry = ProjectedEntries.AddDefaulted_GetRef();
		Entry.Instance = PreparedEntry.Instance;
		Entry.EntryId = PreparedEntry.EntryId;
		Entry.StackCount = PreparedEntry.PlannedCount;
		Entry.Placement = PreparedEntry.Placement;
	}
	FValidatedInventoryGraph ProjectedGraph;
	if (!ValidateInventoryGraph(
			ProjectedEntries,
			OwningActor,
			true,
			ProjectedGraph,
			OutCode))
	{
		return false;
	}

	if (InventoryRevision != Prepared.BaseRevision)
	{
		OutCode = ERpgInventoryMutationResultCode::SourceMismatch;
		return false;
	}

	OutCode = ERpgInventoryMutationResultCode::Success;
	return true;
}

FRpgInventoryMutationResult URpgInventoryManagerComponent::AddPickupBatch(
	const FInventoryPickup& Pickup,
	TArray<FRpgInventoryItemId>& OutAffectedItemIds)
{
	OutAffectedItemIds.Reset();

	FRpgInventoryMutationResult Result;
	Result.RequestId = FGuid::NewGuid();
	Result.Operation = ERpgInventoryMutationOperation::Pickup;
	RpgInventoryPickupBatchPrivate::TryGetRequestedQuantity(
		Pickup,
		Result.RequestedQuantity);
	if (IsInventoryMutationLocked())
	{
		return Result;
	}

	TGuardValue<bool> ApplyingGuard(bIsApplyingPickupBatch, true);
	TGuardValue<bool> PlanningGuard(bIsPlanningPickupBatch, true);
	FPreparedPickupBatch Prepared;
	ERpgInventoryMutationResultCode Code =
		ERpgInventoryMutationResultCode::InvalidRequest;
	if (!PreparePickupBatch(
			Pickup,
			GetOwner(),
			Prepared,
			Code) ||
		!RevalidatePickupBatch(Prepared, Code))
	{
		Result.Code = Code == ERpgInventoryMutationResultCode::Success
			? ERpgInventoryMutationResultCode::InternalError
			: Code;
		Result.Deltas.Reset();
		return Result;
	}

	Result.RequestedQuantity = Prepared.RequestedQuantity;
	Result.AppliedQuantity = Prepared.RequestedQuantity;
	Result.Code = ERpgInventoryMutationResultCode::Success;

	int32 NewEntryCount = 0;
	int32 ChangedExistingCount = 0;
	for (const FPreparedPickupBatch::FEntry& PreparedEntry :
		Prepared.Entries)
	{
		if (PreparedEntry.bExisting)
		{
			ChangedExistingCount +=
				PreparedEntry.PlannedCount != PreparedEntry.BeforeCount;
		}
		else
		{
			++NewEntryCount;
		}
	}

	InventoryList.Entries.Reserve(
		InventoryList.Entries.Num() + NewEntryCount);
	Result.Deltas.Reserve(NewEntryCount + ChangedExistingCount);
	struct FEntryNotification
	{
		FRpgInventoryEntry Entry;
		int32 OldCount = 0;
	};
	TArray<FEntryNotification> EntryNotifications;
	TArray<TObjectPtr<URpgInventoryItemInstance>> AddedInstances;
	EntryNotifications.Reserve(ChangedExistingCount + NewEntryCount);
	AddedInstances.Reserve(NewEntryCount);

	for (const FPreparedPickupBatch::FEntry& PreparedEntry :
		Prepared.Entries)
	{
		if (PreparedEntry.bExisting)
		{
			if (PreparedEntry.PlannedCount == PreparedEntry.BeforeCount)
			{
				continue;
			}

			FRpgInventoryEntry* Current =
				InventoryList.FindEntryByEntryId(PreparedEntry.EntryId);
			check(Current && Current->Instance == PreparedEntry.Instance);
			FEntryNotification& Notification =
				EntryNotifications.AddDefaulted_GetRef();
			Notification.OldCount = Current->StackCount;

			FRpgInventoryMutationDelta& Delta =
				Result.Deltas.AddDefaulted_GetRef();
			Delta.Kind = ERpgInventoryMutationDeltaKind::StackChanged;
			Delta.ItemId = Current->Instance->GetItemId();
			Delta.BeforeContainer = Current->Placement.GetContainerHandle();
			Delta.AfterContainer = Current->Placement.GetContainerHandle();
			Delta.BeforePlacement = Current->Placement;
			Delta.AfterPlacement = Current->Placement;
			Delta.PreviousQuantity = Current->StackCount;
			Delta.NewQuantity = PreparedEntry.PlannedCount;

			Current->StackCount = PreparedEntry.PlannedCount;
			InventoryList.MarkItemDirty(*Current);
			Notification.Entry = *Current;
			continue;
		}

		FRpgInventoryEntry& NewEntry =
			InventoryList.Entries.AddDefaulted_GetRef();
		NewEntry.Instance = PreparedEntry.Instance;
		NewEntry.EntryId = PreparedEntry.EntryId;
		NewEntry.StackCount = PreparedEntry.PlannedCount;
		NewEntry.Placement = PreparedEntry.Placement;
		InventoryList.MarkItemDirty(NewEntry);
		AddedInstances.Add(NewEntry.Instance);
		FEntryNotification& Notification =
			EntryNotifications.AddDefaulted_GetRef();
		Notification.Entry = NewEntry;
		Notification.OldCount = 0;

		FRpgInventoryMutationDelta& Delta =
			Result.Deltas.AddDefaulted_GetRef();
		Delta.Kind = ERpgInventoryMutationDeltaKind::Added;
		Delta.ItemId = NewEntry.Instance->GetItemId();
		Delta.AfterContainer = NewEntry.Placement.GetContainerHandle();
		Delta.AfterPlacement = NewEntry.Placement;
		Delta.PreviousQuantity = 0;
		Delta.NewQuantity = NewEntry.StackCount;
	}

	if (NewEntryCount > 0)
	{
		InventoryList.SortEntriesByPlacement();
		InventoryList.MarkArrayDirty();
	}
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		for (URpgInventoryItemInstance* AddedInstance : AddedInstances)
		{
			AddReplicatedSubObject(
				AddedInstance,
				ReplicationPolicy ==
						ERpgInventoryReplicationPolicy::OwnerOnly
					? COND_OwnerOnly
					: COND_None);
		}
	}

	OutAffectedItemIds.Reserve(
		Prepared.RepresentativeEntryIndices.Num());
	for (const int32 EntryIndex :
		Prepared.RepresentativeEntryIndices)
	{
		check(Prepared.Entries.IsValidIndex(EntryIndex));
		const FPreparedPickupBatch::FEntry& AffectedEntry =
			Prepared.Entries[EntryIndex];
		check(AffectedEntry.Instance && AffectedEntry.ItemId.IsValid());
		OutAffectedItemIds.Add(AffectedEntry.ItemId);
	}

	// Every synchronous listener observes the complete batch graph. No individual
	// payload row is ever published before the remaining rows have committed.
	for (FEntryNotification& Notification : EntryNotifications)
	{
		InventoryList.BroadcastChangeMessage(
			Notification.Entry,
			Notification.OldCount,
			Notification.Entry.StackCount);
	}

	MarkInventoryStateDirty();

	return Result;
}
