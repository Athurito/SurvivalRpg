#include "RpgInventoryUiActionDomainHandlers.h"

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

	FGameplayTag GetTransactionActionTagForMutation(
		ERpgInventoryMutationOperation Operation)
	{
		switch (Operation)
		{
		case ERpgInventoryMutationOperation::Split:
			return RpgGameplayTags::Rpg_Inventory_Action_Split;
		case ERpgInventoryMutationOperation::Equip:
			return RpgGameplayTags::Rpg_Inventory_Action_Equip;
		case ERpgInventoryMutationOperation::Drop:
			return RpgGameplayTags::Rpg_Inventory_Action_Drop;
		default:
			return RpgGameplayTags::Rpg_Inventory_Action_Transfer;
		}
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

	bool IsGenericTransactionMutationOperationAllowed(
		ERpgInventoryMutationOperation Operation)
	{
		switch (Operation)
		{
		case ERpgInventoryMutationOperation::Split:
		case ERpgInventoryMutationOperation::Sort:
			return true;
		default:
			return false;
		}
	}

	FRpgInventorySpatialCapability GetTransactionSpatialContract(
		const URpgInventoryItemInstance* Item)
	{
		return FRpgInventoryItemCapabilities::ResolveSpatial(Item);
	}
}

void FRpgInventoryTransactionActionHandler::InventoryMutation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMutationRequest Request)
{
	Request.EnsureRequestId();
	const FGameplayTag ActionTag =
		GetTransactionActionTagForMutation(Request.Operation);
	if (!IsGenericTransactionMutationOperationAllowed(Request.Operation))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Verbose,
			TEXT("Rejected operation %d through generic RequestInventoryMutation; use its dedicated validated request API."),
			static_cast<int32>(Request.Operation));
		SendActionFeedback(
			ActionTag,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			nullptr,
			Request.Quantity,
			Request.RequestId,
			Request.ItemId);
		return;
	}

	URpgInventoryItemInstance* ItemBeforeMutation =
		Inventory
			? Inventory->FindItemById(Request.ItemId)
			: nullptr;
	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendActionFeedback(
			ActionTag,
			ERpgInventoryActionFeedbackResult::NoAccess,
			Inventory,
			ItemBeforeMutation,
			Request.Quantity,
			Request.RequestId,
			Request.ItemId);
		return;
	}

	const int32 InventoryRevisionBefore =
		Inventory->GetInventoryRevision();
	const FRpgInventoryMutationResult Result =
		Inventory->ExecuteInventoryMutation(Request);
	if (!Result.IsSuccess())
	{
		SendActionFeedback(
			ActionTag,
			GetTransactionFeedbackForMutationResult(Result.Code),
			Inventory,
			ItemBeforeMutation,
			Request.Quantity,
			Result.RequestId,
			Request.ItemId);
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

	SendActionFeedback(
		ActionTag,
		ERpgInventoryActionFeedbackResult::Success,
		Inventory,
		ItemBeforeMutation,
		Result.AppliedQuantity,
		Result.RequestId,
		Request.ItemId);
}

void FRpgInventoryTransactionActionHandler::MoveInventoryItem(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMoveIntent Intent)
{
	Intent.EnsureRequestId();
	URpgInventoryItemInstance* Item =
		Inventory ? Inventory->FindItemById(Intent.ItemId) : nullptr;
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
	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::NoAccess,
			Inventory,
			Item,
			Intent.ExpectedQuantity,
			Intent.RequestId,
			Intent.ItemId);
		return;
	}

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
	URpgInventoryItemInstance* Item =
		SourceInventory
			? SourceInventory->FindItemById(Intent.ItemId)
			: nullptr;
	if (!SourceInventory || !TargetInventory ||
		!CanAccessInventory(SourceInventory) ||
		!CanAccessInventory(TargetInventory))
	{
		SendAndCacheExactTransferFeedback(
			SourceInventory,
			TargetInventory,
			Intent,
			ERpgInventoryActionFeedbackResult::NoAccess,
			Item,
			Intent.Quantity);
		return;
	}
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

void FRpgInventoryTransactionActionHandler::TransferItemStack(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount)
{
	FRpgInventoryQuickTransferRequest Request;
	Request.EnsureRequestId();
	Request.ItemId =
		Item ? Item->GetItemId() : FRpgInventoryItemId();
	Request.StackCount = StackCount;
	FRpgInventoryEntryView SourceEntry;
	if (TryGetTransactionEntrySnapshot(
			SourceInventory,
			Request.ItemId,
			SourceEntry))
	{
		Request.ExpectedEntryId = SourceEntry.EntryId;
		Request.ExpectedSourcePlacement = SourceEntry.Placement;
		Request.ExpectedSourceQuantity = SourceEntry.StackCount;
		Request.StackCount = StackCount <= 0
			? SourceEntry.StackCount
			: FMath::Min(StackCount, SourceEntry.StackCount);
	}
	QuickTransferItem(
		SourceInventory,
		TargetInventory,
		MoveTemp(Request));
}

void FRpgInventoryTransactionActionHandler::
	TransferItemStackToPlacement(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		FRpgInventoryGridPlacement TargetPlacement)
{
	FRpgInventoryTransferIntent Intent;
	Intent.EnsureRequestId();
	Intent.ItemId =
		Item ? Item->GetItemId() : FRpgInventoryItemId();
	Intent.TargetContainer =
		TargetPlacement.GetContainerHandle();
	Intent.TargetPlacement = TargetPlacement;
	Intent.Quantity = StackCount;
	FRpgInventoryEntryView SourceEntry;
	if (TryGetTransactionEntrySnapshot(
			SourceInventory,
			Intent.ItemId,
			SourceEntry))
	{
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedSourceQuantity = SourceEntry.StackCount;
	}
	TransferInventoryItem(
		SourceInventory,
		TargetInventory,
		MoveTemp(Intent));
}

void FRpgInventoryTransactionActionHandler::ApplyInventorySort(
	URpgInventoryManagerComponent* Inventory,
	ERpgInventorySortMode SortMode)
{
	if (!CanAccessInventory(Inventory))
	{
		return;
	}

	Inventory->ApplyInventorySort(SortMode);
}

void FRpgInventoryTransactionActionHandler::MoveInventoryEntry(
	URpgInventoryManagerComponent* Inventory,
	FGuid EntryId,
	int32 TargetIndex)
{
	if (!CanAccessInventory(Inventory) ||
		!Inventory->ContainsEntry(EntryId))
	{
		return;
	}

	Inventory->MoveInventoryEntry(EntryId, TargetIndex);
}

void FRpgInventoryTransactionActionHandler::
	MoveInventoryEntryToPlacement(
		URpgInventoryManagerComponent* Inventory,
		FGuid EntryId,
		FRpgInventoryGridPlacement TargetPlacement)
{
	if (!Inventory || !CanAccessInventory(Inventory) ||
		!Inventory->ContainsEntry(EntryId))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::NoAccess,
			Inventory,
			nullptr,
			1);
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries =
		Inventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry = Entries.FindByPredicate(
		[EntryId](const FRpgInventoryEntryView& Candidate)
		{
			return Candidate.EntryId == EntryId;
		});
	if (!Entry || !Entry->Instance)
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::MissingItem,
			Inventory,
			nullptr,
			1);
		return;
	}

	FRpgInventoryMoveIntent Intent;
	Intent.EnsureRequestId();
	Intent.ItemId = Entry->ItemId;
	Intent.ExpectedEntryId = Entry->EntryId;
	Intent.ExpectedSourcePlacement = Entry->Placement;
	Intent.ExpectedQuantity = Entry->StackCount;
	Intent.TargetPlacement = TargetPlacement;
	MoveInventoryItem(Inventory, MoveTemp(Intent));
}

void FRpgInventoryTransactionActionHandler::SplitItemStack(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement)
{
	SplitItemStackById(
		Inventory,
		Item ? Item->GetItemId() : FRpgInventoryItemId(),
		SplitCount,
		TargetPlacement,
		FGuid());
}

void FRpgInventoryTransactionActionHandler::SplitItemStackById(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemId ItemId,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement,
	FGuid RequestId)
{
	URpgInventoryItemInstance* Item =
		Inventory ? Inventory->FindItemById(ItemId) : nullptr;
	int32 ActualSplitCount = 0;
	FRpgInventoryGridPlacement ActualTargetPlacement;
	const FRpgInventoryTransactionQueryHandler QueryHandler(
		GetReadOnlyActionComponent());
	if (!QueryHandler.CanSplitItemStack(
			Inventory,
			Item,
			SplitCount,
			TargetPlacement,
			ActualSplitCount,
			ActualTargetPlacement))
	{
		const ERpgInventoryActionFeedbackResult Result =
			!Item || !IsTransactionStackableItem(Item)
				? ERpgInventoryActionFeedbackResult::NotStackable
				: ERpgInventoryActionFeedbackResult::InventoryFull;
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Split,
			Result,
			Inventory,
			Item,
			SplitCount,
			RequestId,
			ItemId);
		return;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetTransactionEntrySnapshot(
			Inventory,
			ItemId,
			SourceEntry))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Split,
			ERpgInventoryActionFeedbackResult::MissingItem,
			Inventory,
			Item,
			ActualSplitCount,
			RequestId,
			ItemId);
		return;
	}

	FRpgInventoryMutationRequest Request;
	Request.Operation = ERpgInventoryMutationOperation::Split;
	Request.ItemId = ItemId;
	Request.ExpectedEntryId = SourceEntry.EntryId;
	Request.Source =
		SourceEntry.Placement.GetContainerHandle();
	Request.ExpectedSourcePlacement = SourceEntry.Placement;
	Request.ExpectedSourceQuantity = SourceEntry.StackCount;
	Request.Target =
		ActualTargetPlacement.GetContainerHandle();
	Request.TargetPlacement = ActualTargetPlacement;
	Request.Quantity = ActualSplitCount;
	Request.RequestId = RequestId;
	InventoryMutation(Inventory, Request);
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

bool FRpgInventoryTransactionQueryHandler::CanTransferItemStack(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount) const
{
	if (!SourceInventory || !TargetInventory ||
		SourceInventory == TargetInventory || !Item)
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetTransactionEntrySnapshot(
			SourceInventory,
			Item->GetItemId(),
			SourceEntry) ||
		SourceEntry.Instance != Item)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0
		? SourceEntry.StackCount
		: StackCount;
	if (RequestedCount <= 0 ||
		RequestedCount > SourceEntry.StackCount)
	{
		return false;
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.ItemId = SourceEntry.ItemId;
	Request.ExpectedEntryId = SourceEntry.EntryId;
	Request.ExpectedSourcePlacement = SourceEntry.Placement;
	Request.ExpectedSourceQuantity = SourceEntry.StackCount;
	Request.StackCount = RequestedCount;
	FRpgInventoryContainerHandle TargetContainer;
	FRpgInventoryGridPlacement TargetPlacement;
	return PlanQuickTransferDestination(
		SourceInventory,
		TargetInventory,
		Request,
		TargetContainer,
		TargetPlacement).IsCompleteSuccess();
}

bool FRpgInventoryTransactionQueryHandler::
	CanTransferItemStackToPlacement(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		FRpgInventoryGridPlacement TargetPlacement) const
{
	if (!SourceInventory || !TargetInventory ||
		SourceInventory == TargetInventory || !Item ||
		!TargetPlacement.IsValid())
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetTransactionEntrySnapshot(
			SourceInventory,
			Item->GetItemId(),
			SourceEntry) ||
		SourceEntry.Instance != Item)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0
		? SourceEntry.StackCount
		: StackCount;
	if (RequestedCount <= 0 ||
		RequestedCount > SourceEntry.StackCount)
	{
		return false;
	}

	FRpgInventoryTransferIntent Intent;
	Intent.ItemId = SourceEntry.ItemId;
	Intent.ExpectedEntryId = SourceEntry.EntryId;
	Intent.ExpectedSourcePlacement = SourceEntry.Placement;
	Intent.ExpectedSourceQuantity = SourceEntry.StackCount;
	Intent.TargetContainer =
		TargetPlacement.GetContainerHandle();
	Intent.TargetPlacement = TargetPlacement;
	Intent.Quantity = RequestedCount;
	return PlanExactTransferPlacement(
		SourceInventory,
		TargetInventory,
		Intent).IsCompleteSuccess();
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

	const int32 AvailableCount =
		Inventory->GetItemStackCount(Item);
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

	if (!ResolvedTargetPlacement.IsValid() ||
		Inventory->GetItemAtContainerCell(
			ResolvedTargetPlacement.GetContainerHandle(),
			ResolvedTargetPlacement.X,
			ResolvedTargetPlacement.Y) != nullptr)
	{
		return false;
	}

	if (!Inventory->CanAddItemDefinitionToPlacement(
			Item->GetItemDef(),
			RequestedSplitCount,
			ResolvedTargetPlacement))
	{
		return false;
	}

	OutSplitCount = RequestedSplitCount;
	OutTargetPlacement = ResolvedTargetPlacement;
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
