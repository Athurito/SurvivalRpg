#include "RpgInventoryUiActionDomainHandlers.h"

#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgPersonalStorageLockerActor.h"
#include "RpgInventoryFragment_ContainmentProfile.h"
#include "RpgInventoryFragment_StorageProfile.h"
#include "RpgInventoryItemCapabilities.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

namespace
{
	bool IsTransactionStackableItem(const URpgInventoryItemInstance* Item)
	{
		return Item &&
			URpgInventoryManagerComponent::
				GetEffectiveMaxStackSizeForDefinition(
					Item->GetItemDef()) > 1;
	}

	bool TryGetTransactionEntrySnapshot(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		FRpgInventoryEntryView& OutEntry)
	{
		OutEntry = FRpgInventoryEntryView();
		if (!Inventory || !ItemId.IsValid())
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry :
			Inventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId && Entry.Instance)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	bool IsSameLogicalTransactionPlacement(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A.GetContainerHandle() == B.GetContainerHandle() &&
			A.X == B.X &&
			A.Y == B.Y &&
			A.bRotated == B.bRotated;
	}

	bool IsExactTransactionPlacementSnapshot(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	FRpgInventoryPlacementPlan MakeRejectedTransactionPlacementPlan(
		ERpgInventoryMutationResultCode Code,
		int32 RequestedQuantity)
	{
		FRpgInventoryPlacementPlan Plan;
		Plan.Code = Code;
		Plan.RequestedQuantity = RequestedQuantity;
		return Plan;
	}

	ERpgInventoryActionFeedbackResult GetTransactionFeedbackForMutationResult(
		ERpgInventoryMutationResultCode Code)
	{
		switch (Code)
		{
		case ERpgInventoryMutationResultCode::Success:
		case ERpgInventoryMutationResultCode::PartiallyApplied:
			return ERpgInventoryActionFeedbackResult::Success;
		case ERpgInventoryMutationResultCode::ItemNotFound:
			return ERpgInventoryActionFeedbackResult::MissingItem;
		case ERpgInventoryMutationResultCode::InvalidPlacement:
		case ERpgInventoryMutationResultCode::InvalidContainer:
		case ERpgInventoryMutationResultCode::ItemNotAllowed:
			return ERpgInventoryActionFeedbackResult::InvalidSlot;
		case ERpgInventoryMutationResultCode::OutOfBounds:
		case ERpgInventoryMutationResultCode::Occupied:
		case ERpgInventoryMutationResultCode::NoSpace:
			return ERpgInventoryActionFeedbackResult::InventoryFull;
		case ERpgInventoryMutationResultCode::StackIncompatible:
		case ERpgInventoryMutationResultCode::StackLimitReached:
			return ERpgInventoryActionFeedbackResult::NotStackable;
		case ERpgInventoryMutationResultCode::InvalidRequest:
		case ERpgInventoryMutationResultCode::SourceMismatch:
			return ERpgInventoryActionFeedbackResult::InvalidRequest;
		default:
			return ERpgInventoryActionFeedbackResult::ServerRejected;
		}
	}

	FRpgInventorySpatialCapability GetTransactionSpatialContract(
		const URpgInventoryItemInstance* Item)
	{
		return FRpgInventoryItemCapabilities::ResolveSpatial(Item);
	}

	ARpgBaseCampActor* FindBaseCampForInventory(
		const URpgInventoryManagerComponent* Inventory)
	{
		if (!Inventory)
		{
			return nullptr;
		}

		if (ARpgBaseCampActor* BaseCamp =
			Cast<ARpgBaseCampActor>(Inventory->GetOwner()))
		{
			return BaseCamp;
		}

		if (const ARpgPersonalStorageLockerActor* Locker =
			Cast<ARpgPersonalStorageLockerActor>(Inventory->GetOwner()))
		{
			return Locker->GetBaseCamp();
		}

		return nullptr;
	}

	FGameplayTag ResolveStorageDestinationDomain(
		const URpgInventoryManagerComponent* Inventory,
		const URpgInventoryManagerComponent* PlayerInventory)
	{
		if (!Inventory)
		{
			return FGameplayTag();
		}

		if (const ARpgBaseCampActor* BaseCamp =
			Cast<ARpgBaseCampActor>(Inventory->GetOwner()))
		{
			if (Inventory == BaseCamp->GetArmoryInventoryComponent())
			{
				return RpgGameplayTags::Storage_Domain_Armory;
			}
			if (Inventory == BaseCamp->GetContainmentInventoryComponent())
			{
				return RpgGameplayTags::Storage_Domain_RiftContainment;
			}
		}

		if (Cast<ARpgPersonalStorageLockerActor>(Inventory->GetOwner()) ||
			Inventory == PlayerInventory)
		{
			return RpgGameplayTags::Storage_Domain_Personal;
		}

		return FGameplayTag();
	}

	bool IsBaseStorageDomainTransferAllowed(
		const URpgInventoryManagerComponent* SourceInventory,
		const URpgInventoryManagerComponent* TargetInventory,
		const URpgInventoryItemInstance* Item,
		const URpgInventoryManagerComponent* PlayerInventory,
		int32 TransferQuantity,
		int32 SourceQuantity)
	{
		if (!SourceInventory || !TargetInventory || !Item ||
			TransferQuantity <= 0 || SourceQuantity <= 0)
		{
			return false;
		}

		const URpgInventoryFragment_StorageProfile* StorageProfile =
			URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
				Item->GetItemDef());
		const ERpgInventoryStorageMode StorageMode = StorageProfile
			? StorageProfile->StorageMode
			: ERpgInventoryStorageMode::GridItem;
		const URpgInventoryFragment_ContainmentProfile* ContainmentProfile =
			Item->FindFragmentByClass<
				URpgInventoryFragment_ContainmentProfile>();

		ARpgBaseCampActor* SourceBase =
			FindBaseCampForInventory(SourceInventory);
		ARpgBaseCampActor* TargetBase =
			FindBaseCampForInventory(TargetInventory);
		if ((SourceBase && SourceBase->GetBaseStorageComponent() &&
				SourceBase->GetBaseStorageComponent()->IsMutationTainted()) ||
			(TargetBase && TargetBase->GetBaseStorageComponent() &&
				TargetBase->GetBaseStorageComponent()->IsMutationTainted()))
		{
			return false;
		}
		const bool bLeavesContainment =
			SourceBase &&
			SourceInventory ==
				SourceBase->GetContainmentInventoryComponent() &&
			TargetInventory != SourceInventory;
		const bool bEntersArmory =
			TargetBase &&
			TargetInventory == TargetBase->GetArmoryInventoryComponent();
		const bool bEntersContainment =
			TargetBase &&
			TargetInventory ==
				TargetBase->GetContainmentInventoryComponent();
		const bool bEntersPersonal =
			Cast<ARpgPersonalStorageLockerActor>(
				TargetInventory->GetOwner()) != nullptr;
		const FGameplayTag TargetDomain =
			ResolveStorageDestinationDomain(
				TargetInventory,
				PlayerInventory);

		if (StorageProfile &&
			(!StorageProfile->IsStructurallyValid() ||
				(TargetBase &&
					!TargetBase->GetBaseStorageComponent()->
						GetInstalledCapabilities().HasAllExact(
							StorageProfile->
								RequiredStorageCapabilityTags))))
		{
			return false;
		}

		if (bEntersContainment)
		{
			const URpgBaseStorageComponent* BaseStorage =
				TargetBase->GetBaseStorageComponent();
			return StorageMode ==
					ERpgInventoryStorageMode::SpecialContainedItem &&
				StorageProfile &&
				StorageProfile->StorageDomainTag ==
					RpgGameplayTags::Storage_Domain_RiftContainment &&
				ContainmentProfile &&
				ContainmentProfile->IsStructurallyValid() &&
				ContainmentProfile->RequiredSealedSlots == 1 &&
				TransferQuantity == 1 && SourceQuantity == 1 &&
				BaseStorage &&
				!BaseStorage->IsContainmentDomainOverCapacity() &&
				BaseStorage->HasInstalledCapability(
					RpgGameplayTags::
						Storage_Capability_RiftContainment) &&
				BaseStorage->GetInstalledCapabilities().HasAllExact(
					ContainmentProfile->
						RequiredContainmentCapabilityTags) &&
				BaseStorage->GetContainmentStrength() >=
					ContainmentProfile->RequiredContainmentStrength &&
				BaseStorage->GetCorruptionProtection() >=
					ContainmentProfile->RequiredCorruptionProtection;
		}

		if (bEntersArmory || bEntersPersonal)
		{
			if (StorageMode == ERpgInventoryStorageMode::BulkResource)
			{
				return false;
			}
			if (bEntersArmory && TargetBase->GetBaseStorageComponent() &&
				TargetBase->GetBaseStorageComponent()->
					IsArmoryDomainOverCapacity())
			{
				return false;
			}
			if (StorageMode == ERpgInventoryStorageMode::GridItem &&
				StorageProfile &&
				StorageProfile->StorageDomainTag != TargetDomain)
			{
				return false;
			}
			if (StorageMode ==
				ERpgInventoryStorageMode::SpecialContainedItem)
			{
				const FGameplayTag DestinationDomain =
					bEntersArmory
						? RpgGameplayTags::Storage_Domain_Armory
						: RpgGameplayTags::Storage_Domain_Personal;
				return ContainmentProfile &&
					Item->IsContainmentStabilized() &&
					ContainmentProfile->
						AllowedStabilizedDestinationDomains.
							HasTagExact(DestinationDomain);
			}
		}

		if (bLeavesContainment)
		{
			const FGameplayTag DestinationDomain =
				ResolveStorageDestinationDomain(
					TargetInventory,
					PlayerInventory);
			return StorageMode ==
					ERpgInventoryStorageMode::SpecialContainedItem &&
				ContainmentProfile && DestinationDomain.IsValid() &&
				Item->IsContainmentStabilized() &&
				ContainmentProfile->
					AllowedStabilizedDestinationDomains.
						HasTagExact(DestinationDomain);
		}

		return true;
	}

	void ApplyCommittedBaseStorageTransferState(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryItemId ItemId)
	{
		ARpgBaseCampActor* SourceBase =
			FindBaseCampForInventory(SourceInventory);
		ARpgBaseCampActor* TargetBase =
			FindBaseCampForInventory(TargetInventory);
		TMap<URpgBaseStorageComponent*, int64> RevisionsBeforeStateUpdate;
		const auto CaptureConcreteDomainBase =
			[&RevisionsBeforeStateUpdate](
				ARpgBaseCampActor* Base,
				const URpgInventoryManagerComponent* Inventory)
			{
				if (!Base || !Inventory ||
					(Inventory != Base->GetArmoryInventoryComponent() &&
					 Inventory != Base->GetContainmentInventoryComponent()))
				{
					return;
				}
				if (URpgBaseStorageComponent* Storage =
					Base->GetBaseStorageComponent())
				{
					RevisionsBeforeStateUpdate.FindOrAdd(Storage) =
						Storage->GetNetworkRevision();
				}
			};
		CaptureConcreteDomainBase(SourceBase, SourceInventory);
		CaptureConcreteDomainBase(TargetBase, TargetInventory);

		// Entering containment never rewrites the concrete lifecycle state. New items are Unstable by default,
		// while an already stabilized instance must remain stabilized after an intentional round trip.
		if (SourceBase &&
			SourceInventory ==
				SourceBase->GetContainmentInventoryComponent() &&
			!SourceInventory->FindItemById(ItemId))
		{
			SourceBase->ForgetContainmentItemState(ItemId);
		}

		for (const TPair<URpgBaseStorageComponent*, int64>& Pair :
			RevisionsBeforeStateUpdate)
		{
			if (Pair.Key && Pair.Key->GetNetworkRevision() == Pair.Value)
			{
				Pair.Key->NotifyExternalStorageStateMutation();
			}
		}
	}
}

void FRpgInventoryTransactionActionHandler::MoveInventoryItem(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMoveIntent Intent)
{
	Intent.EnsureRequestId();
	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::NoAccess,
			Inventory,
			nullptr,
			Intent.ExpectedQuantity,
			Intent.RequestId,
			Intent.ItemId);
		return;
	}

	URpgInventoryItemInstance* Item =
		Inventory->FindItemById(Intent.ItemId);
	FRpgInventoryEntryView SourceBeforeMove;
	const bool bCommitsCurrentSourceSnapshot =
		TryGetTransactionEntrySnapshot(
			Inventory,
			Intent.ItemId,
			SourceBeforeMove) &&
		SourceBeforeMove.EntryId == Intent.ExpectedEntryId &&
		IsExactTransactionPlacementSnapshot(
			SourceBeforeMove.Placement,
			Intent.ExpectedSourcePlacement) &&
		SourceBeforeMove.StackCount == Intent.ExpectedQuantity;

	const bool bPreservesEquipmentIdentity =
		Inventory == FindPlayerInventory() &&
		(IsPlayerEquipmentPlacement(Intent.ExpectedSourcePlacement) ||
			IsPlayerEquipmentPlacement(Intent.TargetPlacement));
	const FRpgInventoryMutationResult Result =
		bPreservesEquipmentIdentity
			? Inventory->MoveEquipmentItem(Intent)
			: Inventory->MoveItem(Intent);
	if (!Result.IsSuccess())
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			GetTransactionFeedbackForMutationResult(Result.Code),
			Inventory,
			Item,
			Intent.ExpectedQuantity,
			Result.RequestId,
			Intent.ItemId);
		return;
	}

	if (bCommitsCurrentSourceSnapshot &&
		Inventory == FindPlayerInventory() &&
		!IsSameLogicalTransactionPlacement(
			Intent.ExpectedSourcePlacement,
			Intent.TargetPlacement) &&
		(IsPlayerEquipmentPlacement(
				Intent.ExpectedSourcePlacement) ||
			IsPlayerEquipmentPlacement(Intent.TargetPlacement)))
	{
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		ERpgInventoryActionFeedbackResult::Success,
		Inventory,
		Inventory->FindItemById(Intent.ItemId),
		Result.AppliedQuantity,
		Result.RequestId,
		Intent.ItemId);
}

void FRpgInventoryTransactionActionHandler::TransferInventoryItem(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryTransferIntent Intent)
{
	Intent.EnsureRequestId();
	if (TryReplayRecentExactTransferResult(
			SourceInventory,
			TargetInventory,
			Intent))
	{
		return;
	}
	if (!Intent.ItemId.IsValid() || !Intent.ExpectedEntryId.IsValid() ||
		!Intent.ExpectedSourcePlacement.IsValid() ||
		Intent.ExpectedSourceQuantity <= 0 || Intent.Quantity <= 0 ||
		Intent.Quantity > Intent.ExpectedSourceQuantity)
	{
		SendAndCacheExactTransferFeedback(
			SourceInventory,
			TargetInventory,
			Intent,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			nullptr,
			Intent.Quantity);
		return;
	}
	if (!SourceInventory || !TargetInventory ||
		!CanAccessInventory(SourceInventory) ||
		!CanAccessInventory(TargetInventory))
	{
		SendAndCacheExactTransferFeedback(
			SourceInventory,
			TargetInventory,
			Intent,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Intent.Quantity);
		return;
	}

	URpgInventoryItemInstance* Item =
		SourceInventory->FindItemById(Intent.ItemId);
	if (!IsUiTransferDirectionAllowed(
			SourceInventory,
			TargetInventory))
	{
		SendAndCacheExactTransferFeedback(
			SourceInventory,
			TargetInventory,
			Intent,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			Intent.Quantity);
		return;
	}
	if (!IsBaseStorageDomainTransferAllowed(
			SourceInventory,
			TargetInventory,
			Item,
			FindPlayerInventory(),
			Intent.Quantity,
			Intent.ExpectedSourceQuantity))
	{
		SendAndCacheExactTransferFeedback(
			SourceInventory,
			TargetInventory,
			Intent,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			Intent.Quantity);
		return;
	}

	const FRpgInventoryMutationResult Result =
		SourceInventory->TransferItem(
			TargetInventory,
			Intent);
	if (!Result.IsSuccess() ||
		Result.AppliedQuantity != Intent.Quantity)
	{
		SendAndCacheExactTransferFeedback(
			SourceInventory,
			TargetInventory,
			Intent,
			GetTransactionFeedbackForMutationResult(Result.Code),
			Item,
			Intent.Quantity);
		return;
	}
	ApplyCommittedBaseStorageTransferState(
		SourceInventory,
		TargetInventory,
		Intent.ItemId);

	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	if ((SourceInventory == PlayerInventory &&
			IsPlayerEquipmentPlacement(
				Intent.ExpectedSourcePlacement)) ||
		(TargetInventory == PlayerInventory &&
			IsPlayerEquipmentPlacement(Intent.TargetPlacement)))
	{
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}
	SendAndCacheExactTransferFeedback(
		SourceInventory,
		TargetInventory,
		Intent,
		ERpgInventoryActionFeedbackResult::Success,
		SourceInventory->FindItemById(Intent.ItemId),
		Result.AppliedQuantity);
}

void FRpgInventoryTransactionActionHandler::QuickTransferItem(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryQuickTransferRequest Request)
{
	Request.EnsureRequestId();
	if (TryReplayRecentQuickTransferResult(
			SourceInventory,
			TargetInventory,
			Request))
	{
		return;
	}
	if (!SourceInventory || !TargetInventory ||
		!CanAccessInventory(SourceInventory) ||
		!CanAccessInventory(TargetInventory))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Request.StackCount);
		return;
	}
	if (!Request.ItemId.IsValid() ||
		!Request.ExpectedEntryId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.ExpectedSourceQuantity <= 0 ||
		Request.StackCount <= 0 ||
		Request.StackCount > Request.ExpectedSourceQuantity)
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			nullptr,
			Request.StackCount);
		return;
	}

	if (!IsUiTransferDirectionAllowed(
			SourceInventory,
			TargetInventory))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			nullptr,
			Request.StackCount);
		return;
	}

	FRpgInventoryEntryView SourceEntry;
	URpgInventoryItemInstance* Item =
		SourceInventory->FindItemById(Request.ItemId);
	if (!Item ||
		!TryGetTransactionEntrySnapshot(
			SourceInventory,
			Request.ItemId,
			SourceEntry))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::MissingItem,
			nullptr,
			Request.StackCount);
		return;
	}

	if (SourceEntry.EntryId != Request.ExpectedEntryId ||
		!IsExactTransactionPlacementSnapshot(
			SourceEntry.Placement,
			Request.ExpectedSourcePlacement) ||
		SourceEntry.StackCount != Request.ExpectedSourceQuantity ||
		SourceEntry.Instance != Item)
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.StackCount);
		return;
	}
	if (!IsBaseStorageDomainTransferAllowed(
			SourceInventory,
			TargetInventory,
			Item,
			FindPlayerInventory(),
			Request.StackCount,
			SourceEntry.StackCount))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			Request.StackCount);
		return;
	}

	const int32 AvailableCount = SourceEntry.StackCount;
	const int32 RequestedCount = Request.StackCount;
	if (RequestedCount > AvailableCount ||
		(SourceInventory == TargetInventory &&
			RequestedCount != AvailableCount))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.StackCount);
		return;
	}

	FRpgInventoryContainerHandle TargetContainer;
	FRpgInventoryGridPlacement TargetPlacement;
	const FRpgInventoryTransactionQueryHandler QueryHandler(
		GetReadOnlyActionComponent());
	if (!QueryHandler.FindQuickTransferDestination(
			SourceInventory,
			TargetInventory,
			Request,
			TargetContainer,
			TargetPlacement))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::InventoryFull,
			Item,
			RequestedCount);
		return;
	}

	FRpgInventoryMutationResult MutationResult;
	if (SourceInventory == TargetInventory)
	{
		FRpgInventoryMoveIntent MoveIntent;
		MoveIntent.RequestId = Request.RequestId;
		MoveIntent.ItemId = Request.ItemId;
		MoveIntent.ExpectedEntryId = Request.ExpectedEntryId;
		MoveIntent.ExpectedSourcePlacement =
			Request.ExpectedSourcePlacement;
		MoveIntent.ExpectedQuantity =
			Request.ExpectedSourceQuantity;
		MoveIntent.TargetPlacement = TargetPlacement;
		const bool bPreservesEquipmentIdentity =
			SourceInventory == FindPlayerInventory() &&
			(IsPlayerEquipmentPlacement(
				MoveIntent.ExpectedSourcePlacement) ||
				IsPlayerEquipmentPlacement(
					MoveIntent.TargetPlacement));
		MutationResult = bPreservesEquipmentIdentity
			? SourceInventory->MoveEquipmentItem(
				MoveTemp(MoveIntent))
			: SourceInventory->MoveItem(MoveTemp(MoveIntent));
	}
	else
	{
		FRpgInventoryTransferIntent TransferIntent;
		TransferIntent.RequestId = Request.RequestId;
		TransferIntent.ItemId = Request.ItemId;
		TransferIntent.ExpectedEntryId = Request.ExpectedEntryId;
		TransferIntent.ExpectedSourcePlacement =
			Request.ExpectedSourcePlacement;
		TransferIntent.ExpectedSourceQuantity =
			Request.ExpectedSourceQuantity;
		TransferIntent.TargetContainer = TargetContainer;
		TransferIntent.Quantity = RequestedCount;
		// An invalid exact placement deliberately lets the cross-inventory
		// planner merge across every compatible stack in the selected
		// container before allocating the remaining quantity at first fit.
		MutationResult = SourceInventory->TransferItem(
			TargetInventory,
			MoveTemp(TransferIntent));
	}

	if (!MutationResult.IsSuccess() ||
		MutationResult.AppliedQuantity != RequestedCount)
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			GetTransactionFeedbackForMutationResult(MutationResult.Code),
			Item,
			RequestedCount);
		return;
	}
	if (SourceInventory != TargetInventory)
	{
		ApplyCommittedBaseStorageTransferState(
			SourceInventory,
			TargetInventory,
			Request.ItemId);
	}

	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	const bool bTargetTouchesPlayerEquipment =
		TargetInventory == PlayerInventory &&
		MutationResult.Deltas.ContainsByPredicate(
			[this](const FRpgInventoryMutationDelta& Delta)
			{
				return IsPlayerEquipmentPlacement(
					Delta.AfterPlacement);
			});
	if ((SourceInventory == PlayerInventory &&
			IsPlayerEquipmentPlacement(
				Request.ExpectedSourcePlacement)) ||
		bTargetTouchesPlayerEquipment)
	{
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}

	SendAndCacheQuickTransferFeedback(
		SourceInventory,
		TargetInventory,
		Request,
		ERpgInventoryActionFeedbackResult::Success,
		Item,
		MutationResult.AppliedQuantity);
}

void FRpgInventoryTransactionActionHandler::SplitItemStackById(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventorySplitRequest Request)
{
	if (TryReplayRecentSplitResult(Inventory, Request))
	{
		return;
	}

	if (!Request.RequestId.IsValid() ||
		!Request.ItemId.IsValid() ||
		!Request.ExpectedEntryId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.ExpectedSourceQuantity <= 1 ||
		Request.SplitCount <= 0 ||
		Request.SplitCount >= Request.ExpectedSourceQuantity ||
		!Request.TargetPlacement.IsValid())
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			nullptr,
			Request.SplitCount);
		return;
	}

	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Request.SplitCount);
		return;
	}

	FRpgInventoryEntryView SourceEntry;
	URpgInventoryItemInstance* Item =
		Inventory->FindItemById(Request.ItemId);
	if (!TryGetTransactionEntrySnapshot(
			Inventory,
			Request.ItemId,
			SourceEntry) ||
		!Item)
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::MissingItem,
			nullptr,
			Request.SplitCount);
		return;
	}
	if (SourceEntry.Instance != Item ||
		SourceEntry.EntryId != Request.ExpectedEntryId ||
		!IsExactTransactionPlacementSnapshot(
			SourceEntry.Placement,
			Request.ExpectedSourcePlacement) ||
		SourceEntry.StackCount != Request.ExpectedSourceQuantity)
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.SplitCount);
		return;
	}
	if (!IsTransactionStackableItem(Item))
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::NotStackable,
			Item,
			Request.SplitCount);
		return;
	}

	int32 ResolvedSplitCount = 0;
	FRpgInventoryGridPlacement ResolvedTargetPlacement;
	const FRpgInventoryTransactionQueryHandler QueryHandler(
		GetReadOnlyActionComponent());
	if (!QueryHandler.CanSplitItemStack(
			Inventory,
			Item,
			Request.SplitCount,
			Request.TargetPlacement,
			ResolvedSplitCount,
			ResolvedTargetPlacement) ||
		ResolvedSplitCount != Request.SplitCount ||
		!IsExactTransactionPlacementSnapshot(
			ResolvedTargetPlacement,
			Request.TargetPlacement))
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InventoryFull,
			Item,
			Request.SplitCount);
		return;
	}

	FRpgInventoryMutationRequest MutationRequest;
	MutationRequest.Operation = ERpgInventoryMutationOperation::Split;
	MutationRequest.ItemId = Request.ItemId;
	MutationRequest.ExpectedEntryId = Request.ExpectedEntryId;
	MutationRequest.Source =
		Request.ExpectedSourcePlacement.GetContainerHandle();
	MutationRequest.ExpectedSourcePlacement =
		Request.ExpectedSourcePlacement;
	MutationRequest.ExpectedSourceQuantity =
		Request.ExpectedSourceQuantity;
	MutationRequest.Target =
		Request.TargetPlacement.GetContainerHandle();
	MutationRequest.TargetPlacement = Request.TargetPlacement;
	MutationRequest.Quantity = Request.SplitCount;
	MutationRequest.RequestId = Request.RequestId;

	const int32 InventoryRevisionBefore =
		Inventory->GetInventoryRevision();
	const FRpgInventoryMutationResult Result =
		Inventory->ExecuteInventoryMutation(MoveTemp(MutationRequest));
	if (!Result.IsSuccess() ||
		Result.AppliedQuantity != Request.SplitCount)
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			GetTransactionFeedbackForMutationResult(Result.Code),
			Item,
			Request.SplitCount);
		return;
	}

	const bool bCommittedPlayerEquipmentMutation =
		Inventory == FindPlayerInventory() &&
		Inventory->GetInventoryRevision() != InventoryRevisionBefore &&
		Result.Deltas.ContainsByPredicate(
			[this](const FRpgInventoryMutationDelta& Delta)
			{
				return IsPlayerEquipmentPlacement(
						Delta.BeforePlacement) ||
					IsPlayerEquipmentPlacement(
						Delta.AfterPlacement);
			});
	if (bCommittedPlayerEquipmentMutation)
	{
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}

	SendAndCacheSplitFeedback(
		Inventory,
		Request,
		ERpgInventoryActionFeedbackResult::Success,
		Item,
		Result.AppliedQuantity);
}

FRpgInventoryPlacementPlan
FRpgInventoryTransactionQueryHandler::PlanExactTransferPlacement(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent) const
{
	if (!SourceInventory || !TargetInventory ||
		SourceInventory == TargetInventory ||
		!CanAccessInventory(SourceInventory) ||
		!CanAccessInventory(TargetInventory) ||
		!IsUiTransferDirectionAllowed(
			SourceInventory,
			TargetInventory) ||
		!Intent.ItemId.IsValid() ||
		!Intent.ExpectedEntryId.IsValid() ||
		!Intent.ExpectedSourcePlacement.IsValid() ||
		Intent.ExpectedSourceQuantity <= 0 ||
		Intent.Quantity <= 0 ||
		Intent.Quantity > Intent.ExpectedSourceQuantity ||
		!Intent.TargetContainer.IsValid() ||
		!Intent.TargetPlacement.IsValid() ||
		Intent.TargetPlacement.GetContainerHandle() !=
			Intent.TargetContainer)
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::InvalidRequest,
			Intent.Quantity);
	}

	FRpgInventoryEntryView SourceEntry;
	URpgInventoryItemInstance* Item =
		SourceInventory->FindItemById(Intent.ItemId);
	if (!Item ||
		!TryGetTransactionEntrySnapshot(
			SourceInventory,
			Intent.ItemId,
			SourceEntry))
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::ItemNotFound,
			Intent.Quantity);
	}
	if (SourceEntry.Instance != Item ||
		SourceEntry.EntryId != Intent.ExpectedEntryId ||
		!IsExactTransactionPlacementSnapshot(
			SourceEntry.Placement,
			Intent.ExpectedSourcePlacement) ||
		SourceEntry.StackCount != Intent.ExpectedSourceQuantity)
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::SourceMismatch,
			Intent.Quantity);
	}
	if (!IsBaseStorageDomainTransferAllowed(
			SourceInventory,
			TargetInventory,
			Item,
			FindPlayerInventory(),
			Intent.Quantity,
			SourceEntry.StackCount))
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::ItemNotAllowed,
			Intent.Quantity);
	}

	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Transfer;
	Query.Search = ERpgInventoryPlacementSearch::Exact;
	Query.Subject =
		FRpgInventoryPlacementSubject::FromIncomingInstance(
			SourceInventory,
			SourceEntry,
			Intent.Quantity);
	Query.TargetContainer = Intent.TargetContainer;
	Query.ExactPlacement = Intent.TargetPlacement;
	return TargetInventory->EvaluatePlacement(Query);
}

FRpgInventoryPlacementPlan
FRpgInventoryTransactionQueryHandler::PlanQuickTransferDestination(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	FRpgInventoryContainerHandle& OutTargetContainer,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	OutTargetContainer = FRpgInventoryContainerHandle();
	OutTargetPlacement = FRpgInventoryGridPlacement();
	if (!SourceInventory || !TargetInventory ||
		!CanAccessInventory(SourceInventory) ||
		!CanAccessInventory(TargetInventory) ||
		!IsUiTransferDirectionAllowed(
			SourceInventory,
			TargetInventory) ||
		!Request.ItemId.IsValid() ||
		!Request.ExpectedEntryId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.ExpectedSourceQuantity <= 0 ||
		Request.StackCount <= 0 ||
		Request.StackCount > Request.ExpectedSourceQuantity)
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::InvalidRequest,
			Request.StackCount);
	}

	FRpgInventoryEntryView SourceEntry;
	URpgInventoryItemInstance* Item =
		SourceInventory->FindItemById(Request.ItemId);
	if (!Item ||
		!TryGetTransactionEntrySnapshot(
			SourceInventory,
			Request.ItemId,
			SourceEntry) ||
		SourceEntry.Instance != Item ||
		SourceEntry.EntryId != Request.ExpectedEntryId ||
		!IsExactTransactionPlacementSnapshot(
			SourceEntry.Placement,
			Request.ExpectedSourcePlacement) ||
		SourceEntry.StackCount != Request.ExpectedSourceQuantity)
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::SourceMismatch,
			Request.StackCount);
	}
	if (!IsBaseStorageDomainTransferAllowed(
			SourceInventory,
			TargetInventory,
			Item,
			FindPlayerInventory(),
			Request.StackCount,
			SourceEntry.StackCount))
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::ItemNotAllowed,
			Request.StackCount);
	}

	const int32 AvailableCount = SourceEntry.StackCount;
	const int32 RequestedCount = Request.StackCount;
	if (RequestedCount > AvailableCount ||
		(SourceInventory == TargetInventory &&
			RequestedCount != AvailableCount))
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::InvalidRequest,
			RequestedCount);
	}

	TArray<FRpgInventoryContainerHandle> CandidateTargets;
	if (!Request.PreferredTargetContainers.IsEmpty())
	{
		for (const FRpgInventoryContainerHandle& Candidate :
			Request.PreferredTargetContainers)
		{
			if (Candidate.IsValid())
			{
				CandidateTargets.AddUnique(Candidate);
			}
		}
	}
	else
	{
		BuildDefaultQuickTransferTargets(
			SourceInventory,
			TargetInventory,
			Item,
			CandidateTargets);
	}

	const URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	FRpgInventoryPlacementPlan LastPlan =
		MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::NoSpace,
			RequestedCount);
	for (const FRpgInventoryContainerHandle& CandidateTarget :
		CandidateTargets)
	{
		// Quick transfer into the player graph is content-only. Gear/Carry
		// mutations remain explicit intents, so a forged preferred handle
		// cannot silently equip or activate an item.
		if (TargetInventory == PlayerInventory)
		{
			const bool bIsContentTarget =
				InventoryLayout &&
				InventoryLayout->GetSlotGroups().ContainsByPredicate(
					[&CandidateTarget, Item](
						const FRpgInventorySlotGroupView& Group)
					{
						return Group.ContainerHandle ==
								CandidateTarget &&
							Group.GroupKind ==
								ERpgInventorySlotGroupKind::Content &&
							Group.Rule.AllowsItem(Item);
					});
			if (!bIsContentTarget)
			{
				continue;
			}
		}

		FRpgInventoryGridPlacement CandidatePlacement;
		FRpgInventoryPlacementPlan CandidatePlan =
			PlanQuickTransferInContainer(
				SourceInventory,
				TargetInventory,
				Item,
				RequestedCount,
				CandidateTarget,
				CandidatePlacement);
		if (CandidatePlan.IsCompleteSuccess())
		{
			OutTargetContainer = CandidateTarget;
			OutTargetPlacement = CandidatePlacement;
			return CandidatePlan;
		}
		LastPlan = MoveTemp(CandidatePlan);
	}

	return LastPlan;
}

bool FRpgInventoryTransactionQueryHandler::
	FindQuickTransferDestination(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		FRpgInventoryContainerHandle& OutTargetContainer,
		FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return PlanQuickTransferDestination(
		SourceInventory,
		TargetInventory,
		Request,
		OutTargetContainer,
		OutTargetPlacement).IsCompleteSuccess();
}

void FRpgInventoryTransactionQueryHandler::
	BuildDefaultQuickTransferTargets(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		TArray<FRpgInventoryContainerHandle>& OutTargets) const
{
	OutTargets.Reset();
	if (!SourceInventory || !TargetInventory || !Item)
	{
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	if (TargetInventory != PlayerInventory || !InventoryLayout)
	{
		const FRpgInventoryContainerHandle DefaultTarget =
			FRpgInventoryContainerHandle::MakeRoot(
				TargetInventory->GetDefaultContainerId());
		if (DefaultTarget.IsValid())
		{
			OutTargets.Add(DefaultTarget);
		}
		return;
	}

	const TArray<FRpgInventorySlotGroupView> Groups =
		InventoryLayout->GetSlotGroups();
	FRpgInventorySlotGroupView PrimaryContentGroup;
	const FRpgInventoryContainerHandle PrimaryContentHandle =
		InventoryLayout->TryGetSlotGroupBySemanticRole(
			RpgGameplayTags::
				Rpg_Inventory_Layout_Role_Content_Primary,
			PrimaryContentGroup)
			? PrimaryContentGroup.ContainerHandle
			: FRpgInventoryContainerHandle();
	auto AddGroups =
		[&Groups, Item, &OutTargets](
			const FRpgInventoryContainerHandle& ContainerHandle,
			ERpgEquipmentSlot ProviderSlot)
		{
			for (const FRpgInventorySlotGroupView& Group : Groups)
			{
				const bool bMatchesContainer =
					ContainerHandle.IsValid() &&
					Group.ContainerHandle == ContainerHandle;
				const bool bMatchesProvider =
					ProviderSlot != ERpgEquipmentSlot::None &&
					Group.SourceEquipmentSlot == ProviderSlot;
				if (Group.GroupKind ==
						ERpgInventorySlotGroupKind::Content &&
					Group.ContainerHandle.IsValid() &&
					(bMatchesContainer || bMatchesProvider) &&
					Group.Rule.AllowsItem(Item))
				{
					OutTargets.AddUnique(Group.ContainerHandle);
				}
			}
		};

	if (SourceInventory != TargetInventory)
	{
		// External loot/storage enters quick-access content first, then the
		// equipped backpack, followed by stable designer-defined content.
		AddGroups(
			PrimaryContentHandle,
			ERpgEquipmentSlot::None);
		AddGroups(
			FRpgInventoryContainerHandle(),
			ERpgEquipmentSlot::Belt);
		AddGroups(
			FRpgInventoryContainerHandle(),
			ERpgEquipmentSlot::Pouch);
		AddGroups(
			FRpgInventoryContainerHandle(),
			ERpgEquipmentSlot::Backpack);
		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			if (Group.GroupKind ==
					ERpgInventorySlotGroupKind::Content &&
				Group.ContainerHandle.IsValid() &&
				Group.Rule.AllowsItem(Item))
			{
				OutTargets.AddUnique(Group.ContainerHandle);
			}
		}
		return;
	}

	FRpgInventoryGridPlacement SourcePlacement;
	if (!SourceInventory->GetItemPlacement(Item, SourcePlacement))
	{
		return;
	}

	const FRpgInventorySlotGroupView* SourceGroup =
		Groups.FindByPredicate(
			[&SourcePlacement](
				const FRpgInventorySlotGroupView& Group)
			{
				return Group.ContainerHandle ==
					SourcePlacement.GetContainerHandle();
			});
	const bool bSourceIsBackpackContent =
		SourceGroup &&
		SourceGroup->SourceEquipmentSlot ==
			ERpgEquipmentSlot::Backpack;
	const bool bSourceIsQuickContent =
		SourceGroup &&
		((PrimaryContentHandle.IsValid() &&
		  SourceGroup->ContainerHandle == PrimaryContentHandle) ||
		 SourceGroup->SourceEquipmentSlot == ERpgEquipmentSlot::Belt ||
		 SourceGroup->SourceEquipmentSlot == ERpgEquipmentSlot::Pouch);

	if (bSourceIsBackpackContent)
	{
		AddGroups(
			PrimaryContentHandle,
			ERpgEquipmentSlot::None);
		AddGroups(
			FRpgInventoryContainerHandle(),
			ERpgEquipmentSlot::Belt);
		AddGroups(
			FRpgInventoryContainerHandle(),
			ERpgEquipmentSlot::Pouch);
	}
	else if (bSourceIsQuickContent)
	{
		AddGroups(
			FRpgInventoryContainerHandle(),
			ERpgEquipmentSlot::Backpack);
	}
	else
	{
		// Gear, Carry, and other player roots quick-transfer into the backpack
		// without activating equipment.
		AddGroups(
			FRpgInventoryContainerHandle(),
			ERpgEquipmentSlot::Backpack);
	}
}

FRpgInventoryPlacementPlan
FRpgInventoryTransactionQueryHandler::
	PlanQuickTransferInContainer(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FRpgInventoryContainerHandle& TargetContainer,
		FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!SourceInventory || !TargetInventory || !Item ||
		StackCount <= 0 || !TargetContainer.IsValid())
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::InvalidRequest,
			StackCount);
	}

	FRpgInventoryEntryView SourceEntry;
	const int32 AvailableCount =
		SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0 ||
		StackCount > AvailableCount ||
		!TryGetTransactionEntrySnapshot(
			SourceInventory,
			Item->GetItemId(),
			SourceEntry) ||
		SourceEntry.Instance != Item)
	{
		return MakeRejectedTransactionPlacementPlan(
			ERpgInventoryMutationResultCode::SourceMismatch,
			StackCount);
	}
	const FRpgInventoryGridPlacement& SourcePlacement =
		SourceEntry.Placement;
	if (SourceInventory == TargetInventory)
	{
		if (StackCount != AvailableCount ||
			SourcePlacement.GetContainerHandle() == TargetContainer)
		{
			return MakeRejectedTransactionPlacementPlan(
				ERpgInventoryMutationResultCode::InvalidRequest,
				StackCount);
		}

		FRpgInventoryGridSize GridSize;
		if (!TargetInventory->GetGridSizeForContainerHandle(
				TargetContainer,
				GridSize))
		{
			return MakeRejectedTransactionPlacementPlan(
				ERpgInventoryMutationResultCode::InvalidContainer,
				StackCount);
		}

		const FRpgInventorySpatialCapability SpatialCapability =
			GetTransactionSpatialContract(Item);
		if (!SpatialCapability.IsValid())
		{
			return MakeRejectedTransactionPlacementPlan(
				ERpgInventoryMutationResultCode::InvalidPlacement,
				StackCount);
		}

		const FRpgInventoryGridSize Footprint =
			SpatialCapability.Footprint;
		const int32 RotationCount =
			SpatialCapability.bAllowRotation ? 2 : 1;
		FRpgInventoryPlacementPlan LastPlan =
			MakeRejectedTransactionPlacementPlan(
				ERpgInventoryMutationResultCode::NoSpace,
				StackCount);
		for (int32 RotationIndex = 0;
			RotationIndex < RotationCount;
			++RotationIndex)
		{
			FRpgInventoryGridPlacement Candidate;
			Candidate.SetContainerHandle(TargetContainer);
			Candidate.Width = Footprint.Width;
			Candidate.Height = Footprint.Height;
			Candidate.bRotated = RotationIndex == 1;
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
					FRpgInventoryPlacementQuery Query;
					const bool bPreservesEquipmentIdentity =
						SourceInventory == FindPlayerInventory() &&
						(IsPlayerEquipmentPlacement(
							SourcePlacement) ||
						 IsPlayerEquipmentPlacement(Candidate));
					Query.Purpose = bPreservesEquipmentIdentity
						? ERpgInventoryPlacementPurpose::Equip
						: ERpgInventoryPlacementPurpose::Move;
					Query.Search =
						ERpgInventoryPlacementSearch::Exact;
					Query.Subject =
						FRpgInventoryPlacementSubject::
							FromOwnedEntry(
								SourceInventory,
								SourceEntry,
								StackCount);
					Query.TargetContainer = TargetContainer;
					Query.ExactPlacement = Candidate;
					FRpgInventoryPlacementPlan CandidatePlan =
						TargetInventory->EvaluatePlacement(Query);
					const bool bIsNonSwappingDestination =
						CandidatePlan.Steps.Num() == 1 &&
						(CandidatePlan.Steps[0].Resolution ==
							 ERpgInventoryPlacementResolution::Place ||
						 CandidatePlan.Steps[0].Resolution ==
							 ERpgInventoryPlacementResolution::Merge);
					if (CandidatePlan.IsCompleteSuccess() &&
						bIsNonSwappingDestination)
					{
						OutPlacement =
							CandidatePlan.Steps[0].Placement;
						return CandidatePlan;
					}
					if (!CandidatePlan.IsCompleteSuccess())
					{
						LastPlan = MoveTemp(CandidatePlan);
					}
				}
			}
		}
		return LastPlan;
	}

	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Transfer;
	Query.Search = ERpgInventoryPlacementSearch::FirstFit;
	Query.Subject =
		FRpgInventoryPlacementSubject::FromIncomingInstance(
			SourceInventory,
			SourceEntry,
			StackCount);
	Query.TargetContainer = TargetContainer;
	FRpgInventoryPlacementPlan Plan =
		TargetInventory->EvaluatePlacement(Query);
	if (Plan.IsCompleteSuccess())
	{
		if (const FRpgInventoryPlacementStep* PlaceStep =
			Plan.Steps.FindByPredicate(
				[](const FRpgInventoryPlacementStep& Step)
				{
					return Step.Resolution ==
						ERpgInventoryPlacementResolution::Place;
				}))
		{
			OutPlacement = PlaceStep->Placement;
		}
	}
	return Plan;
}

bool FRpgInventoryTransactionQueryHandler::CanSplitItemStack(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement,
	int32& OutSplitCount,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	OutSplitCount = 0;
	OutTargetPlacement = FRpgInventoryGridPlacement();

	if (!Inventory || !Item || !CanAccessInventory(Inventory) ||
		!IsTransactionStackableItem(Item))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetTransactionEntrySnapshot(
			Inventory,
			Item->GetItemId(),
			SourceEntry) ||
		SourceEntry.Instance != Item)
	{
		return false;
	}

	const int32 AvailableCount = SourceEntry.StackCount;
	if (AvailableCount <= 1)
	{
		return false;
	}

	const int32 RequestedSplitCount =
		SplitCount <= 0 ? AvailableCount / 2 : SplitCount;
	if (RequestedSplitCount <= 0 ||
		RequestedSplitCount >= AvailableCount)
	{
		return false;
	}

	FRpgInventoryGridPlacement ResolvedTargetPlacement =
		TargetPlacement;
	if (!ResolvedTargetPlacement.IsValid() &&
		!FindFirstEmptyInventoryPlacement(
			Inventory,
			Item->GetItemDef(),
			ResolvedTargetPlacement))
	{
		return false;
	}

	if (!ResolvedTargetPlacement.IsValid())
	{
		return false;
	}

	FRpgInventoryPlacementQuery Query;
	Query.Purpose = ERpgInventoryPlacementPurpose::Split;
	Query.Search = ERpgInventoryPlacementSearch::Exact;
	Query.Subject = FRpgInventoryPlacementSubject::FromOwnedEntry(
		Inventory,
		SourceEntry,
		RequestedSplitCount);
	Query.TargetContainer =
		ResolvedTargetPlacement.GetContainerHandle();
	Query.ExactPlacement = ResolvedTargetPlacement;
	const FRpgInventoryPlacementPlan Plan =
		Inventory->EvaluatePlacement(Query);
	if (!Plan.IsCompleteSuccess() ||
		Plan.AppliedQuantity != RequestedSplitCount ||
		Plan.Steps.Num() != 1 ||
		Plan.Steps[0].Resolution !=
			ERpgInventoryPlacementResolution::Place)
	{
		return false;
	}

	OutSplitCount = RequestedSplitCount;
	OutTargetPlacement = Plan.Steps[0].Placement;
	return true;
}

bool FRpgInventoryTransactionQueryHandler::
	FindFirstEmptyInventoryPlacement(
		URpgInventoryManagerComponent* Inventory,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!Inventory || !ItemDefinition)
	{
		return false;
	}

	if (URpgInventoryManagerComponent* PlayerInventory =
			FindPlayerInventory();
		Inventory == PlayerInventory)
	{
		if (const URpgPlayerInventoryLayoutComponent* InventoryLayout =
			FindPlayerInventoryLayout())
		{
			for (const FRpgInventorySlotGroupView& Group :
				InventoryLayout->GetSlotGroups())
			{
				if (Group.GroupKind !=
						ERpgInventorySlotGroupKind::Content ||
					!Group.Rule.AllowsItemDefinition(ItemDefinition))
				{
					continue;
				}

				for (int32 Y = 0;
					Y < Group.GridSize.Height;
					++Y)
				{
					for (int32 X = 0;
						X < Group.GridSize.Width;
						++X)
					{
						FRpgInventoryGridPlacement Candidate;
						Candidate.SetContainerHandle(
							Group.ContainerHandle);
						Candidate.X = X;
						Candidate.Y = Y;
						Candidate.Width = 1;
						Candidate.Height = 1;
						if (!Inventory->GetItemAtContainerCell(
								Candidate.GetContainerHandle(),
								Candidate.X,
								Candidate.Y) &&
							Inventory
								->CanAddItemDefinitionToPlacement(
									ItemDefinition,
									1,
									Candidate))
						{
							OutPlacement = Candidate;
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	const FName DefaultContainerId =
		Inventory->GetDefaultContainerId();
	const FRpgInventoryContainerHandle DefaultHandle =
		FRpgInventoryContainerHandle::MakeRoot(DefaultContainerId);
	FRpgInventoryGridSize GridSize;
	if (!Inventory->GetGridSizeForContainerHandle(
			DefaultHandle,
			GridSize))
	{
		return false;
	}

	for (int32 Y = 0; Y < GridSize.Height; ++Y)
	{
		for (int32 X = 0; X < GridSize.Width; ++X)
		{
			FRpgInventoryGridPlacement Candidate;
			Candidate.SetContainerHandle(DefaultHandle);
			Candidate.X = X;
			Candidate.Y = Y;
			Candidate.Width = 1;
			Candidate.Height = 1;
			if (!Inventory->GetItemAtContainerCell(
					Candidate.GetContainerHandle(),
					Candidate.X,
					Candidate.Y) &&
				Inventory->CanAddItemDefinitionToPlacement(
					ItemDefinition,
					1,
					Candidate))
			{
				OutPlacement = Candidate;
				return true;
			}
		}
	}

	return false;
}
