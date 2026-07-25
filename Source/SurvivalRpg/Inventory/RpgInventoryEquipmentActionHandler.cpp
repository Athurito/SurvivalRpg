#include "RpgInventoryUiActionDomainHandlers.h"

#include "RpgInventoryEquipmentPlacementPolicy.h"
#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	bool TryGetEquipmentEntrySnapshot(
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

	bool IsSameLogicalEquipmentPlacement(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A.GetContainerHandle() == B.GetContainerHandle() &&
			A.X == B.X &&
			A.Y == B.Y &&
			A.bRotated == B.bRotated;
	}

	bool IsExactEquipmentPlacementSnapshot(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool AreEquipmentHandlerSelectionsEquivalent(
		const FRpgEquipmentSelectionSaveData& A,
		const FRpgEquipmentSelectionSaveData& B)
	{
		if (A.ActiveMainHandItemId != B.ActiveMainHandItemId ||
			A.ActiveOffHandItemId != B.ActiveOffHandItemId ||
			A.RememberedOffhands.Num() != B.RememberedOffhands.Num())
		{
			return false;
		}

		for (int32 Index = 0;
			Index < A.RememberedOffhands.Num();
			++Index)
		{
			if (A.RememberedOffhands[Index].MainHandItemId !=
					B.RememberedOffhands[Index].MainHandItemId ||
				A.RememberedOffhands[Index].OffHandItemId !=
					B.RememberedOffhands[Index].OffHandItemId)
			{
				return false;
			}
		}
		return true;
	}

	FRpgInventoryPlacementPlan MakeRejectedEquipmentPlacementPlan(
		ERpgInventoryMutationResultCode Code,
		int32 RequestedQuantity)
	{
		FRpgInventoryPlacementPlan Plan;
		Plan.Code = Code;
		Plan.RequestedQuantity = RequestedQuantity;
		return Plan;
	}
}

void FRpgInventoryEquipmentActionHandler::
	ApplyInventoryEquipmentIntent(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryEquipmentIntent Intent)
{
	Intent.EnsureRequestId();
	if (TryReplayRecentEquipmentIntentResult(Inventory, Intent))
	{
		return;
	}

	const bool bKnownOperation =
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::
				EquipDefaultAndActivate ||
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::EquipToSlot ||
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::MoveToCarry ||
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent ||
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::
				ClearActiveSelection;
	bool bTargetSlotMatchesOperation =
		Intent.TargetEquipmentSlot == ERpgEquipmentSlot::None;
	if (Intent.Operation ==
		ERpgInventoryEquipmentIntentOperation::EquipToSlot)
	{
		bTargetSlotMatchesOperation =
			FRpgInventoryEquipmentPlacementPolicy::
				IsManagedEquipmentSlot(Intent.TargetEquipmentSlot);
	}
	else if (Intent.Operation ==
		ERpgInventoryEquipmentIntentOperation::
			ClearActiveSelection)
	{
		bTargetSlotMatchesOperation =
			FRpgInventoryEquipmentPlacementPolicy::
				IsHandEquipmentSlot(Intent.TargetEquipmentSlot);
	}
	if (!bKnownOperation || !bTargetSlotMatchesOperation ||
		!Intent.ItemId.IsValid() ||
		!Intent.ExpectedEntryId.IsValid() ||
		!Intent.ExpectedSourcePlacement.IsValid() ||
		Intent.ExpectedQuantity <= 0)
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			nullptr,
			Intent.ExpectedQuantity);
		return;
	}

	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Intent.ExpectedQuantity);
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	if (Inventory != PlayerInventory)
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			ERpgInventoryActionFeedbackResult::WrongInventory,
			Inventory->FindItemById(Intent.ItemId),
			Intent.ExpectedQuantity);
		return;
	}

	FRpgInventoryEntryView CurrentEntry;
	if (!TryGetEquipmentEntrySnapshot(
			Inventory,
			Intent.ItemId,
			CurrentEntry))
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			ERpgInventoryActionFeedbackResult::MissingItem,
			nullptr,
			Intent.ExpectedQuantity);
		return;
	}

	URpgInventoryItemInstance* Item = CurrentEntry.Instance;
	if (!Item ||
		CurrentEntry.EntryId != Intent.ExpectedEntryId ||
		!IsExactEquipmentPlacementSnapshot(
			CurrentEntry.Placement,
			Intent.ExpectedSourcePlacement) ||
		CurrentEntry.StackCount != Intent.ExpectedQuantity)
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Intent.ExpectedQuantity);
		return;
	}

	const bool bHasEquippableFragment =
		Item->FindFragmentByClass<
			URpgInventoryFragment_EquippableItem>() != nullptr;
	if (Intent.Operation !=
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent &&
		!bHasEquippableFragment)
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			ERpgInventoryActionFeedbackResult::NotEquippable,
			Item,
			Intent.ExpectedQuantity);
		return;
	}

	bool bSucceeded = false;
	ERpgInventoryActionFeedbackResult FailureResult =
		ERpgInventoryActionFeedbackResult::NoValidSlot;
	switch (Intent.Operation)
	{
	case ERpgInventoryEquipmentIntentOperation::
		EquipDefaultAndActivate:
		bSucceeded =
			TryAssignItemToDefaultEquipmentDestination(Item);
		break;

	case ERpgInventoryEquipmentIntentOperation::EquipToSlot:
		bSucceeded =
			FRpgInventoryEquipmentPlacementPolicy::
				IsHandEquipmentSlot(Intent.TargetEquipmentSlot)
				? TryMoveAndActivateItemInCarry(
					Item,
					Intent.TargetEquipmentSlot)
				: TryMoveItemToGearSlot(
					Intent.TargetEquipmentSlot,
					Item);
		break;

	case ERpgInventoryEquipmentIntentOperation::MoveToCarry:
		if (!bHasEquippableFragment)
		{
			FailureResult =
				ERpgInventoryActionFeedbackResult::NotEquippable;
			break;
		}
		bSucceeded = TryMoveItemToFirstCompatibleCarrySlot(Item);
		break;

	case ERpgInventoryEquipmentIntentOperation::UnequipToContent:
	{
		URpgPlayerInventoryLayoutComponent* InventoryLayout =
			FindPlayerInventoryLayout();
		FRpgInventorySlotAddress SourceAddress;
		if (!InventoryLayout ||
			!InventoryLayout->TryMakeSlotAddressFromPlacement(
				CurrentEntry.Placement,
				SourceAddress) ||
			(!InventoryLayout->IsGearSlotAddress(SourceAddress) &&
				!InventoryLayout->IsCarrySlotAddress(SourceAddress)))
		{
			FailureResult =
				ERpgInventoryActionFeedbackResult::InvalidSlot;
			break;
		}
		const FRpgInventoryEquipmentQueryHandler QueryHandler(
			GetReadOnlyActionComponent());
		if (InventoryLayout->IsGearSlotAddress(SourceAddress) &&
			!QueryHandler.CanMoveItemOutOfGearSlot(SourceAddress))
		{
			FailureResult =
				ERpgInventoryActionFeedbackResult::ServerRejected;
			break;
		}
		FailureResult =
			ERpgInventoryActionFeedbackResult::InventoryFull;
		bSucceeded =
			TryMoveItemToFirstCompatibleContentSlot(Item);
		break;
	}

	case ERpgInventoryEquipmentIntentOperation::ClearActiveSelection:
	{
		FailureResult =
			ERpgInventoryActionFeedbackResult::MissingItem;
		URpgEquipmentLoadoutComponent* EquipmentLoadout =
			FindEquipmentLoadout();
		URpgPlayerInventoryLayoutComponent* InventoryLayout =
			FindPlayerInventoryLayout();
		FRpgInventorySlotAddress SourceAddress;
		ERpgEquipmentSlot SourceEquipmentSlot =
			ERpgEquipmentSlot::None;
		if (!EquipmentLoadout ||
			!InventoryLayout ||
			!InventoryLayout->TryMakeSlotAddressFromPlacement(
				CurrentEntry.Placement,
				SourceAddress) ||
			!InventoryLayout->TryGetEquipmentSlotRoleForAddress(
				SourceAddress,
				SourceEquipmentSlot) ||
			SourceEquipmentSlot != Intent.TargetEquipmentSlot ||
			EquipmentLoadout->GetItemInEquipmentSlot(
				Intent.TargetEquipmentSlot) != Item)
		{
			break;
		}
		bSucceeded =
			Intent.TargetEquipmentSlot ==
				ERpgEquipmentSlot::MainHand
				? EquipmentLoadout->ClearActiveMainHand()
				: EquipmentLoadout->ClearActiveOffHand(true);
		break;
	}

	default:
		break;
	}

	if (!bSucceeded)
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			FailureResult,
			Item,
			Intent.ExpectedQuantity);
		return;
	}

	if (Intent.Operation !=
		ERpgInventoryEquipmentIntentOperation::ClearActiveSelection)
	{
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}
	SendAndCacheEquipmentIntentFeedback(
		Inventory,
		Intent,
		ERpgInventoryActionFeedbackResult::Success,
		Item,
		Intent.ExpectedQuantity);
}

FRpgInventoryPlacementPlan
FRpgInventoryEquipmentQueryHandler::PlanEquipmentIntentPlacement(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryEquipmentIntent& Intent,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	OutTargetPlacement = FRpgInventoryGridPlacement();
	const bool bPlansEquipToSlot =
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::EquipToSlot;
	const bool bPlansUnequipToContent =
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::UnequipToContent;
	const bool bOperationMatchesTarget =
		(bPlansEquipToSlot &&
			FRpgInventoryEquipmentPlacementPolicy::
				IsManagedEquipmentSlot(
					Intent.TargetEquipmentSlot)) ||
		(bPlansUnequipToContent &&
			Intent.TargetEquipmentSlot == ERpgEquipmentSlot::None);
	if (!Inventory || Inventory != FindPlayerInventory() ||
		!CanAccessInventory(Inventory) ||
		!bOperationMatchesTarget ||
		!Intent.ItemId.IsValid() ||
		!Intent.ExpectedEntryId.IsValid() ||
		!Intent.ExpectedSourcePlacement.IsValid() ||
		Intent.ExpectedQuantity <= 0)
	{
		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::InvalidRequest,
			Intent.ExpectedQuantity);
	}

	FRpgInventoryEntryView SourceEntry;
	URpgInventoryItemInstance* Item =
		Inventory->FindItemById(Intent.ItemId);
	if (!Item ||
		!TryGetEquipmentEntrySnapshot(
			Inventory,
			Intent.ItemId,
			SourceEntry))
	{
		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::ItemNotFound,
			Intent.ExpectedQuantity);
	}
	if (SourceEntry.Instance != Item ||
		SourceEntry.EntryId != Intent.ExpectedEntryId ||
		!IsExactEquipmentPlacementSnapshot(
			SourceEntry.Placement,
			Intent.ExpectedSourcePlacement) ||
		SourceEntry.StackCount != Intent.ExpectedQuantity)
	{
		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::SourceMismatch,
			Intent.ExpectedQuantity);
	}

	if (bPlansEquipToSlot &&
		!FRpgInventoryEquipmentPlacementPolicy::
			CanItemUseEquipmentSlot(
				Item,
				Intent.TargetEquipmentSlot))
	{
		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::ItemNotAllowed,
			Intent.ExpectedQuantity);
	}

	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	if (!InventoryLayout)
	{
		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::InvalidContainer,
			Intent.ExpectedQuantity);
	}

	auto EvaluateCandidate =
		[Inventory, &SourceEntry](
			const FRpgInventoryGridPlacement& Candidate)
		{
			FRpgInventoryPlacementQuery Query;
			Query.Purpose = ERpgInventoryPlacementPurpose::Equip;
			Query.Search = ERpgInventoryPlacementSearch::Exact;
			Query.Subject =
				FRpgInventoryPlacementSubject::FromOwnedEntry(
					Inventory,
					SourceEntry,
					SourceEntry.StackCount);
			Query.TargetContainer = Candidate.GetContainerHandle();
			Query.ExactPlacement = Candidate;
			return Inventory->EvaluatePlacement(Query);
		};

	if (bPlansUnequipToContent)
	{
		FRpgInventorySlotAddress SourceAddress;
		if (!InventoryLayout->TryMakeSlotAddressFromPlacement(
				SourceEntry.Placement,
				SourceAddress) ||
			(!InventoryLayout->IsGearSlotAddress(SourceAddress) &&
				!InventoryLayout->IsCarrySlotAddress(SourceAddress)) ||
			(InventoryLayout->IsGearSlotAddress(SourceAddress) &&
				!CanMoveItemOutOfGearSlot(SourceAddress)))
		{
			return MakeRejectedEquipmentPlacementPlan(
				ERpgInventoryMutationResultCode::ItemNotAllowed,
				Intent.ExpectedQuantity);
		}

		ERpgEquipmentSlot DisappearingProviderSlot =
			ERpgEquipmentSlot::None;
		ERpgEquipmentSlot SourceEquipmentSlot =
			ERpgEquipmentSlot::None;
		if (InventoryLayout->IsGearSlotAddress(SourceAddress) &&
			InventoryLayout->TryGetEquipmentSlotForGearContainer(
				SourceAddress.GetContainerHandle(),
				SourceEquipmentSlot) &&
			URpgPlayerInventoryLayoutComponent::
				IsSlotContainerEquipmentSlot(SourceEquipmentSlot))
		{
			DisappearingProviderSlot = SourceEquipmentSlot;
		}

		for (const FRpgInventorySlotGroupView& Group :
			InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind !=
					ERpgInventorySlotGroupKind::Content ||
				!Group.Rule.AllowsItem(Item) ||
				(DisappearingProviderSlot !=
						ERpgEquipmentSlot::None &&
					Group.bProvidedByEquipment &&
					Group.SourceEquipmentSlot ==
						DisappearingProviderSlot))
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					const FRpgInventorySlotAddress TargetAddress =
						Group.MakeAddress(X, Y);
					FRpgInventoryGridPlacement Candidate;
					if (!InventoryLayout->CanItemUseSlotAddress(
							Item,
							TargetAddress) ||
						!InventoryLayout->ResolveSlotAddress(
							TargetAddress,
							Candidate))
					{
						continue;
					}

					FRpgInventoryPlacementPlan Plan =
						EvaluateCandidate(Candidate);
					const bool
						bIsNonSwappingContentPlacement =
							Plan.IsCompleteSuccess() &&
							Plan.Steps.Num() == 1 &&
							Plan.Steps[0].Resolution ==
								ERpgInventoryPlacementResolution::
									Place;
					if (bIsNonSwappingContentPlacement)
					{
						OutTargetPlacement =
							Plan.Steps[0].Placement;
						return Plan;
					}
				}
			}
		}

		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::NoSpace,
			Intent.ExpectedQuantity);
	}

	if (FRpgInventoryEquipmentPlacementPolicy::
			IsHandEquipmentSlot(Intent.TargetEquipmentSlot))
	{
		const URpgEquipmentLoadoutComponent* EquipmentLoadout =
			FindEquipmentLoadout();
		if (!EquipmentLoadout ||
			!EquipmentLoadout->CanActivateItemInEquipmentSlot(
				Intent.TargetEquipmentSlot,
				Item))
		{
			return MakeRejectedEquipmentPlacementPlan(
				ERpgInventoryMutationResultCode::ItemNotAllowed,
				Intent.ExpectedQuantity);
		}

		for (const FRpgInventorySlotGroupView& Group :
			InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind !=
					ERpgInventorySlotGroupKind::Carry ||
				Group.EquipmentSlotRole !=
					Intent.TargetEquipmentSlot ||
				!Group.Rule.AllowsItem(Item))
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					const FRpgInventorySlotAddress TargetAddress =
						Group.MakeAddress(X, Y);
					FRpgInventoryGridPlacement Candidate;
					if (!InventoryLayout->CanItemUseSlotAddress(
							Item,
							TargetAddress) ||
						!InventoryLayout->ResolveSlotAddress(
							TargetAddress,
							Candidate))
					{
						continue;
					}

					FRpgInventoryPlacementPlan Plan =
						EvaluateCandidate(Candidate);
					if (Plan.IsCompleteSuccess())
					{
						OutTargetPlacement =
							Plan.Steps[0].Placement;
						return Plan;
					}
				}
			}
		}

		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::NoSpace,
			Intent.ExpectedQuantity);
	}

	FRpgInventorySlotAddress TargetAddress;
	if (!InventoryLayout->TryMakeGearSlotAddress(
			Intent.TargetEquipmentSlot,
			TargetAddress) ||
		!InventoryLayout->ResolveSlotAddress(
			TargetAddress,
			OutTargetPlacement) ||
		!InventoryLayout->CanItemUseSlotAddress(
			Item,
			TargetAddress) ||
		!CanMoveItemOutOfGearSlot(TargetAddress))
	{
		OutTargetPlacement = FRpgInventoryGridPlacement();
		return MakeRejectedEquipmentPlacementPlan(
			ERpgInventoryMutationResultCode::ItemNotAllowed,
			Intent.ExpectedQuantity);
	}

	FRpgInventoryPlacementPlan Plan =
		EvaluateCandidate(OutTargetPlacement);
	if (Plan.IsCompleteSuccess())
	{
		OutTargetPlacement = Plan.Steps[0].Placement;
	}
	return Plan;
}

bool FRpgInventoryEquipmentQueryHandler::
	CanMoveItemToFirstCompatibleContentSlot(
		URpgInventoryItemInstance* Item,
		FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	OutTargetPlacement = FRpgInventoryGridPlacement();
	FRpgInventoryEntryView SourceEntry;
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	if (!Item || !PlayerInventory ||
		!TryGetEquipmentEntrySnapshot(
			PlayerInventory,
			Item->GetItemId(),
			SourceEntry) ||
		SourceEntry.Instance != Item)
	{
		return false;
	}

	FRpgInventoryEquipmentIntent Intent;
	Intent.ItemId = SourceEntry.ItemId;
	Intent.ExpectedEntryId = SourceEntry.EntryId;
	Intent.ExpectedSourcePlacement = SourceEntry.Placement;
	Intent.ExpectedQuantity = SourceEntry.StackCount;
	Intent.Operation =
		ERpgInventoryEquipmentIntentOperation::UnequipToContent;
	return PlanEquipmentIntentPlacement(
		PlayerInventory,
		Intent,
		OutTargetPlacement).IsCompleteSuccess();
}

bool FRpgInventoryEquipmentActionHandler::
	TryAssignItemToDefaultEquipmentDestination(
		URpgInventoryItemInstance* Item)
{
	if (!Item)
	{
		return false;
	}

	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		FindEquipmentLoadout();
	if (!EquipmentLoadout)
	{
		return false;
	}

	const URpgInventoryFragment_ItemContainer*
		ContainerFragment =
			Item->FindFragmentByClass<
				URpgInventoryFragment_ItemContainer>();
	const URpgInventoryFragment_EquippableItem*
		EquippableFragment =
			Item->FindFragmentByClass<
				URpgInventoryFragment_EquippableItem>();
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition =
		EquippableFragment
			? EquippableFragment->GetEquipmentDefinition()
			: nullptr;
	const URpgEquipmentDefinition* EquipmentCDO =
		EquipmentDefinition
			? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition)
			: nullptr;

	if (ContainerFragment)
	{
		if (EquipmentCDO)
		{
			const ERpgEquipmentSlot DefaultSlot =
				EquipmentCDO->GetDefaultEquipSlot();
			if (URpgPlayerInventoryLayoutComponent::
					IsSlotContainerEquipmentSlot(DefaultSlot) &&
				TryMoveItemToGearSlot(DefaultSlot, Item))
			{
				return true;
			}

			for (const ERpgEquipmentSlot AllowedSlot :
				EquipmentCDO->AllowedSlots)
			{
				if (URpgPlayerInventoryLayoutComponent::
						IsSlotContainerEquipmentSlot(AllowedSlot) &&
					TryMoveItemToGearSlot(AllowedSlot, Item))
				{
					return true;
				}
			}
		}
	}

	if (!EquipmentCDO)
	{
		return false;
	}

	const ERpgEquipmentSlot DefaultSlot =
		EquipmentCDO->GetDefaultEquipSlot();
	if (FRpgInventoryEquipmentPlacementPolicy::
			IsHandEquipmentSlot(DefaultSlot))
	{
		return TryMoveAndActivateItemInCarry(Item, DefaultSlot);
	}

	if (FRpgInventoryEquipmentPlacementPolicy::
			IsManagedEquipmentSlot(DefaultSlot))
	{
		return TryMoveItemToGearSlot(DefaultSlot, Item);
	}

	if (TryMoveAndActivateItemInCarry(
			Item,
			ERpgEquipmentSlot::None))
	{
		return true;
	}

	return false;
}

bool FRpgInventoryEquipmentActionHandler::
	TryMoveAndActivateItemInCarry(
		URpgInventoryItemInstance* Item,
		ERpgEquipmentSlot PreferredHandSlot)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		FindEquipmentLoadout();
	if (!Item || !PlayerInventory ||
		!InventoryLayout || !EquipmentLoadout ||
		!PlayerInventory->ContainsItemInstance(Item))
	{
		return false;
	}

	FRpgInventoryEntryView OriginalEntry;
	if (!TryGetEquipmentEntrySnapshot(
			PlayerInventory,
			Item->GetItemId(),
			OriginalEntry))
	{
		return false;
	}
	const FRpgInventoryGridPlacement OriginalPlacement =
		OriginalEntry.Placement;

	FRpgInventoryGridPlacement PlannedTargetPlacement;
	const bool bHasExplicitTargetPlan =
		FRpgInventoryEquipmentPlacementPolicy::
			IsHandEquipmentSlot(PreferredHandSlot);
	if (bHasExplicitTargetPlan)
	{
		FRpgInventoryEquipmentIntent EquipmentIntent;
		EquipmentIntent.ItemId = OriginalEntry.ItemId;
		EquipmentIntent.ExpectedEntryId = OriginalEntry.EntryId;
		EquipmentIntent.ExpectedSourcePlacement =
			OriginalEntry.Placement;
		EquipmentIntent.ExpectedQuantity = OriginalEntry.StackCount;
		EquipmentIntent.Operation =
			ERpgInventoryEquipmentIntentOperation::EquipToSlot;
		EquipmentIntent.TargetEquipmentSlot = PreferredHandSlot;
		const FRpgInventoryEquipmentQueryHandler QueryHandler(
			GetReadOnlyActionComponent());
		if (!QueryHandler.PlanEquipmentIntentPlacement(
				PlayerInventory,
				EquipmentIntent,
				PlannedTargetPlacement).IsCompleteSuccess())
		{
			return false;
		}
	}

	const FRpgEquipmentSelectionSaveData PreviousSelection =
		EquipmentLoadout->ExportEquipmentSelection();
	const TArray<FRpgInventorySlotGroupView> Groups =
		InventoryLayout->GetSlotGroups();

	auto TryActivateSlot =
		[EquipmentLoadout, Item](
			ERpgEquipmentSlot EquipmentSlot)
		{
			return EquipmentSlot == ERpgEquipmentSlot::MainHand
				? EquipmentLoadout->SetMainHandItemActive(Item)
				: EquipmentSlot == ERpgEquipmentSlot::OffHand &&
					EquipmentLoadout->SetOffHandItemActive(Item);
		};
	auto MatchesPreferredSlot =
		[PreferredHandSlot](ERpgEquipmentSlot EquipmentSlot)
		{
			return PreferredHandSlot == ERpgEquipmentSlot::None ||
				PreferredHandSlot == EquipmentSlot;
		};

	// None prefers MainHand deterministically. An explicit hand probes
	// exactly one typed semantic role.
	TArray<ERpgEquipmentSlot, TInlineAllocator<2>> SlotOrder;
	if (PreferredHandSlot == ERpgEquipmentSlot::OffHand)
	{
		SlotOrder.Add(ERpgEquipmentSlot::OffHand);
	}
	else
	{
		SlotOrder.Add(ERpgEquipmentSlot::MainHand);
		if (PreferredHandSlot == ERpgEquipmentSlot::None)
		{
			SlotOrder.Add(ERpgEquipmentSlot::OffHand);
		}
	}
	for (const ERpgEquipmentSlot DesiredSlot : SlotOrder)
	{
		if (!MatchesPreferredSlot(DesiredSlot))
		{
			continue;
		}
		if (!EquipmentLoadout->CanActivateItemInEquipmentSlot(
				DesiredSlot,
				Item))
		{
			continue;
		}

		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			if (Group.GroupKind !=
					ERpgInventorySlotGroupKind::Carry ||
				Group.EquipmentSlotRole != DesiredSlot ||
				!Group.Rule.AllowsItem(Item))
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					const FRpgInventorySlotAddress TargetAddress =
						Group.MakeAddress(X, Y);
					FRpgInventoryGridPlacement TargetPlacement;
					if (!InventoryLayout->CanItemUseSlotAddress(
							Item,
							TargetAddress) ||
						!InventoryLayout->ResolveSlotAddress(
							TargetAddress,
							TargetPlacement))
					{
						continue;
					}
					if (bHasExplicitTargetPlan &&
						!IsSameLogicalEquipmentPlacement(
							TargetPlacement,
							PlannedTargetPlacement))
					{
						continue;
					}

					const bool bAlreadyAtTarget =
						OriginalPlacement.GetContainerHandle() ==
							TargetPlacement.GetContainerHandle() &&
						OriginalPlacement.X == TargetPlacement.X &&
						OriginalPlacement.Y == TargetPlacement.Y;
					if (bAlreadyAtTarget)
					{
						return TryActivateSlot(DesiredSlot);
					}

					FRpgInventoryMoveIntent EquipIntent;
					EquipIntent.EnsureRequestId();
					EquipIntent.ItemId = Item->GetItemId();
					EquipIntent.ExpectedEntryId =
						OriginalEntry.EntryId;
					EquipIntent.ExpectedSourcePlacement =
						OriginalPlacement;
					EquipIntent.ExpectedQuantity =
						OriginalEntry.StackCount;
					EquipIntent.TargetPlacement = TargetPlacement;
					if (!PlayerInventory->PlanEquipmentMove(
							EquipIntent).IsSuccess())
					{
						continue;
					}

					const FRpgInventoryMutationResult EquipResult =
						PlayerInventory->MoveEquipmentItem(
							EquipIntent);
					if (!EquipResult.IsSuccess())
					{
						return false;
					}

					if (TryActivateSlot(DesiredSlot))
					{
						return true;
					}

					// Activation was prevalidated and should be
					// side-effect free. Restore the exact physical state
					// before trying another compatible Carry role.
					FRpgInventoryEntryView CurrentEntry;
					if (!TryGetEquipmentEntrySnapshot(
							PlayerInventory,
							Item->GetItemId(),
							CurrentEntry))
					{
						SyncEquipmentLoadoutFromGearSlots();
						SyncActiveHandsFromCarrySlots();
						return false;
					}

					FRpgInventoryMoveIntent RollbackIntent;
					RollbackIntent.EnsureRequestId();
					RollbackIntent.ItemId = Item->GetItemId();
					RollbackIntent.ExpectedEntryId =
						CurrentEntry.EntryId;
					RollbackIntent.ExpectedSourcePlacement =
						CurrentEntry.Placement;
					RollbackIntent.ExpectedQuantity =
						CurrentEntry.StackCount;
					RollbackIntent.TargetPlacement =
						OriginalPlacement;
					const FRpgInventoryMutationResult
						RollbackResult =
							PlayerInventory->MoveEquipmentItem(
								RollbackIntent);
					if (!RollbackResult.IsSuccess())
					{
						SyncEquipmentLoadoutFromGearSlots();
						SyncActiveHandsFromCarrySlots();
						return false;
					}
					const FRpgEquipmentSelectionSaveData
						CurrentSelection =
							EquipmentLoadout->
								ExportEquipmentSelection();
					if (!AreEquipmentHandlerSelectionsEquivalent(
							CurrentSelection,
							PreviousSelection))
					{
						EquipmentLoadout->RestoreEquipmentSelection(
							PreviousSelection);
					}
				}
			}
		}
	}

	return false;
}

bool FRpgInventoryEquipmentActionHandler::TryMoveItemToGearSlot(
	ERpgEquipmentSlot EquipmentSlot,
	URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	if (!Item ||
		!PlayerInventory ||
		!FRpgInventoryEquipmentPlacementPolicy::
			IsManagedEquipmentSlot(EquipmentSlot) ||
		FRpgInventoryEquipmentPlacementPolicy::
			IsHandEquipmentSlot(EquipmentSlot))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetEquipmentEntrySnapshot(
			PlayerInventory,
			Item->GetItemId(),
			SourceEntry) ||
		SourceEntry.Instance != Item)
	{
		return false;
	}

	FRpgInventoryEquipmentIntent EquipmentIntent;
	EquipmentIntent.ItemId = SourceEntry.ItemId;
	EquipmentIntent.ExpectedEntryId = SourceEntry.EntryId;
	EquipmentIntent.ExpectedSourcePlacement = SourceEntry.Placement;
	EquipmentIntent.ExpectedQuantity = SourceEntry.StackCount;
	EquipmentIntent.Operation =
		ERpgInventoryEquipmentIntentOperation::EquipToSlot;
	EquipmentIntent.TargetEquipmentSlot = EquipmentSlot;
	FRpgInventoryGridPlacement TargetPlacement;
	const FRpgInventoryEquipmentQueryHandler QueryHandler(
		GetReadOnlyActionComponent());
	if (!QueryHandler.PlanEquipmentIntentPlacement(
			PlayerInventory,
			EquipmentIntent,
			TargetPlacement).IsCompleteSuccess())
	{
		return false;
	}

	FRpgInventoryMoveIntent MoveIntent;
	MoveIntent.EnsureRequestId();
	MoveIntent.ItemId = SourceEntry.ItemId;
	MoveIntent.ExpectedEntryId = SourceEntry.EntryId;
	MoveIntent.ExpectedSourcePlacement = SourceEntry.Placement;
	MoveIntent.ExpectedQuantity = SourceEntry.StackCount;
	MoveIntent.TargetPlacement = TargetPlacement;
	if (!PlayerInventory->PlanEquipmentMove(
			MoveIntent).IsSuccess())
	{
		return false;
	}
	const FRpgInventoryMutationResult MoveResult =
		PlayerInventory->MoveEquipmentItem(MoveIntent);
	return MoveResult.IsSuccess() &&
		MoveResult.AppliedQuantity == SourceEntry.StackCount;
}

bool FRpgInventoryEquipmentActionHandler::
	TryMoveItemToFirstCompatibleCarrySlot(
		URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	if (!Item || !PlayerInventory ||
		!InventoryLayout ||
		PlayerInventory->GetItemStackCount(Item) <= 0)
	{
		return false;
	}

	FRpgInventorySlotAddress CurrentAddress;
	FRpgInventoryGridPlacement CurrentPlacement;
	if (PlayerInventory->GetItemPlacement(
			Item,
			CurrentPlacement) &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(
			CurrentPlacement,
			CurrentAddress) &&
		InventoryLayout->IsCarrySlotAddress(CurrentAddress) &&
		InventoryLayout->CanItemUseSlotAddress(Item, CurrentAddress))
	{
		return true;
	}

	FRpgInventoryEntryView SourceEntry;
	for (const FRpgInventoryEntryView& Entry :
		PlayerInventory->GetAllEntries())
	{
		if (Entry.Instance == Item)
		{
			SourceEntry = Entry;
			break;
		}
	}

	if (!SourceEntry.EntryId.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group :
		InventoryLayout->GetSlotGroups())
	{
		if (Group.GroupKind !=
				ERpgInventorySlotGroupKind::Carry ||
			!Group.ContainerHandle.IsRoot() ||
			!FRpgInventoryEquipmentPlacementPolicy::
				IsHandEquipmentSlot(Group.EquipmentSlotRole) ||
			!Group.Rule.AllowsItem(Item))
		{
			continue;
		}

		for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < Group.GridSize.Width; ++X)
			{
				FRpgInventoryGridPlacement TargetPlacement;
				TargetPlacement.SetContainerHandle(
					Group.ContainerHandle);
				TargetPlacement.X = X;
				TargetPlacement.Y = Y;
				TargetPlacement.Width = 1;
				TargetPlacement.Height = 1;
				FRpgInventoryMoveIntent MoveIntent;
				MoveIntent.EnsureRequestId();
				MoveIntent.ItemId = SourceEntry.ItemId;
				MoveIntent.ExpectedEntryId = SourceEntry.EntryId;
				MoveIntent.ExpectedSourcePlacement =
					SourceEntry.Placement;
				MoveIntent.ExpectedQuantity =
					SourceEntry.StackCount;
				MoveIntent.TargetPlacement = TargetPlacement;
				if (PlayerInventory->PlanEquipmentMove(
						MoveIntent).IsSuccess())
				{
					const FRpgInventoryMutationResult MoveResult =
						PlayerInventory->MoveEquipmentItem(
							MoveIntent);
					return MoveResult.IsSuccess() &&
						MoveResult.AppliedQuantity ==
							SourceEntry.StackCount;
				}
			}
		}
	}

	return false;
}

bool FRpgInventoryEquipmentActionHandler::
	TryMoveItemToFirstCompatibleContentSlot(
		URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	FRpgInventoryEntryView SourceEntry;
	if (!Item || !PlayerInventory ||
		!TryGetEquipmentEntrySnapshot(
			PlayerInventory,
			Item->GetItemId(),
			SourceEntry) ||
		SourceEntry.Instance != Item)
	{
		return false;
	}

	FRpgInventoryEquipmentIntent EquipmentIntent;
	EquipmentIntent.ItemId = SourceEntry.ItemId;
	EquipmentIntent.ExpectedEntryId = SourceEntry.EntryId;
	EquipmentIntent.ExpectedSourcePlacement = SourceEntry.Placement;
	EquipmentIntent.ExpectedQuantity = SourceEntry.StackCount;
	EquipmentIntent.Operation =
		ERpgInventoryEquipmentIntentOperation::UnequipToContent;
	FRpgInventoryGridPlacement TargetPlacement;
	const FRpgInventoryEquipmentQueryHandler QueryHandler(
		GetReadOnlyActionComponent());
	if (!QueryHandler.PlanEquipmentIntentPlacement(
			PlayerInventory,
			EquipmentIntent,
			TargetPlacement).IsCompleteSuccess())
	{
		return false;
	}

	FRpgInventoryMoveIntent MoveIntent;
	MoveIntent.EnsureRequestId();
	MoveIntent.ItemId = SourceEntry.ItemId;
	MoveIntent.ExpectedEntryId = SourceEntry.EntryId;
	MoveIntent.ExpectedSourcePlacement = SourceEntry.Placement;
	MoveIntent.ExpectedQuantity = SourceEntry.StackCount;
	MoveIntent.TargetPlacement = TargetPlacement;
	const FRpgInventoryMutationResult MoveResult =
		PlayerInventory->MoveEquipmentItem(MoveIntent);
	return MoveResult.IsSuccess() &&
		MoveResult.AppliedQuantity == SourceEntry.StackCount;
}

bool FRpgInventoryEquipmentQueryHandler::CanMoveItemOutOfGearSlot(
	const FRpgInventorySlotAddress& SourceAddress) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
	if (!InventoryLayout ||
		!InventoryLayout->TryGetEquipmentSlotForGearContainer(
			SourceAddress.GetContainerHandle(),
			EquipmentSlot))
	{
		return false;
	}

	if (!URpgPlayerInventoryLayoutComponent::
			IsSlotContainerEquipmentSlot(EquipmentSlot))
	{
		return true;
	}

	return InventoryLayout->CanUnequipSlotContainer(EquipmentSlot);
}
