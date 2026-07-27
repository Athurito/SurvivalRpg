#include "RpgInventoryUiActionDomainHandlers.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryItemCapabilities.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"

namespace
{
	bool IsManualDropStackableItem(
		const URpgInventoryItemInstance* Item)
	{
		return Item &&
			URpgInventoryManagerComponent::
				GetEffectiveMaxStackSizeForDefinition(
					Item->GetItemDef()) > 1;
	}

	bool IsExactManualDropPlacementSnapshot(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	ERpgInventoryActionFeedbackResult
		GetManualDropFeedbackForMutationResult(
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

	bool CanManualDropTargetAcceptTransferredStack(
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 TransferCount,
		bool bTransfersWholeEntry)
	{
		if (!TargetInventory || !Item || TransferCount <= 0)
		{
			return false;
		}

		if (Item->FindFragmentByClass<
				URpgInventoryFragment_ItemContainer>() != nullptr &&
			!bTransfersWholeEntry)
		{
			return false;
		}

		const FRpgInventoryContainerHandle TargetRoot =
			FRpgInventoryContainerHandle::MakeRoot(
				TargetInventory->GetDefaultContainerId());
		int32 RemainingCount = TransferCount;
		for (const FRpgInventoryEntryView& Entry :
			TargetInventory->GetAllEntries())
		{
			if (!Entry.Instance ||
				Entry.Placement.GetContainerHandle() != TargetRoot ||
				!Item->IsStackCompatibleWith(Entry.Instance))
			{
				continue;
			}

			RemainingCount -=
				TargetInventory->GetFreeStackCapacity(Entry.Instance);
			if (RemainingCount <= 0)
			{
				return true;
			}
		}

		FRpgInventoryGridSize GridSize;
		if (!TargetInventory->GetGridSizeForContainerHandle(
				TargetRoot,
				GridSize))
		{
			return false;
		}

		for (int32 RotationIndex = 0; RotationIndex < 2;
			++RotationIndex)
		{
			for (int32 Y = 0; Y < GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < GridSize.Width; ++X)
				{
					FRpgInventoryGridPlacement Placement;
					Placement.SetContainerHandle(TargetRoot);
					Placement.X = X;
					Placement.Y = Y;
					Placement.bRotated = RotationIndex == 1;
					if (TargetInventory
							->CanReceiveTransferredItemInstanceToPlacement(
								Item,
								RemainingCount,
								Placement))
					{
						return true;
					}
				}
			}
		}
		return false;
	}
}

void FRpgInventoryManualDropActionHandler::DropInventoryItemById(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryManualDropRequest Request)
{
	if (TryReplayRecentManualDropResult(Inventory, Request))
	{
		return;
	}

	if (!Request.RequestId.IsValid() || !Inventory ||
		!Request.EntryId.IsValid() || !Request.ItemId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.ExpectedSourceQuantity <= 0 || Request.StackCount <= 0 ||
		Request.StackCount > Request.ExpectedSourceQuantity)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			nullptr,
			Request.StackCount);
		return;
	}

	if (!CanAccessInventory(Inventory))
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Request.StackCount);
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries =
		Inventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry = Entries.FindByPredicate(
		[&Request](const FRpgInventoryEntryView& Candidate)
		{
			return Candidate.EntryId == Request.EntryId;
		});
	URpgInventoryItemInstance* Item =
		Entry ? Entry->Instance.Get() : nullptr;
	if (!Entry || !Item || Entry->ItemId != Request.ItemId ||
		Item->GetItemId() != Request.ItemId ||
		Inventory->FindItemById(Request.ItemId) != Item)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::MissingItem,
			nullptr,
			Request.StackCount);
		return;
	}

	if (!IsExactManualDropPlacementSnapshot(
			Entry->Placement,
			Request.ExpectedSourcePlacement) ||
		Entry->StackCount != Request.ExpectedSourceQuantity)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.StackCount);
		return;
	}

	const int32 AvailableCount = Entry->StackCount;
	if (AvailableCount <= 0)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::MissingItem,
			Item,
			Request.StackCount);
		return;
	}

	if (Request.StackCount > AvailableCount)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.StackCount);
		return;
	}

	const ERpgInventoryManualDropPolicy DropPolicy =
		FRpgInventoryItemCapabilities::ResolveManualDropPolicy(Item);
	if (DropPolicy == ERpgInventoryManualDropPolicy::Disabled)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::CannotDrop,
			Item,
			Request.StackCount);
		return;
	}

	if (DropPolicy == ERpgInventoryManualDropPolicy::Confirm &&
		!Request.bConfirmed)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::RequiresConfirmation,
			Item,
			Request.StackCount);
		return;
	}

	const bool bDropAsStackTemplate =
		IsManualDropStackableItem(Item);
	if (!bDropAsStackTemplate && Request.StackCount != AvailableCount)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.StackCount);
		return;
	}

	FRpgInventoryTransferIntent DropPlanIntent;
	DropPlanIntent.RequestId = Request.RequestId;
	DropPlanIntent.ItemId = Request.ItemId;
	DropPlanIntent.ExpectedEntryId = Request.EntryId;
	DropPlanIntent.ExpectedSourcePlacement =
		Request.ExpectedSourcePlacement;
	DropPlanIntent.ExpectedSourceQuantity =
		Request.ExpectedSourceQuantity;
	DropPlanIntent.Quantity = Request.StackCount;
	const FRpgInventoryMutationResult DropPlan =
		Inventory->PlanDropItem(DropPlanIntent);
	if (!DropPlan.IsSuccess() ||
		DropPlan.AppliedQuantity != Request.StackCount)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			GetManualDropFeedbackForMutationResult(DropPlan.Code),
			Item,
			Request.StackCount);
		return;
	}

	bool bSubtreeContainsDisabledItem = false;
	bool bSubtreeRequiresConfirmation = false;
	for (const FRpgInventoryMutationDelta& Delta : DropPlan.Deltas)
	{
		URpgInventoryItemInstance* PlannedItem =
			Inventory->FindItemById(Delta.ItemId);
		if (!PlannedItem)
		{
			SendAndCacheManualDropFeedback(
				Inventory,
				Request,
				ERpgInventoryActionFeedbackResult::ServerRejected,
				Item,
				Request.StackCount);
			return;
		}

		switch (FRpgInventoryItemCapabilities::
			ResolveManualDropPolicy(PlannedItem))
		{
		case ERpgInventoryManualDropPolicy::Disabled:
			bSubtreeContainsDisabledItem = true;
			break;
		case ERpgInventoryManualDropPolicy::Confirm:
			bSubtreeRequiresConfirmation = true;
			break;
		default:
			break;
		}
	}
	if (bSubtreeContainsDisabledItem)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::CannotDrop,
			Item,
			Request.StackCount);
		return;
	}
	if (bSubtreeRequiresConfirmation && !Request.bConfirmed)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::RequiresConfirmation,
			Item,
			Request.StackCount);
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	const bool bDropsWholePlayerEntry =
		Inventory == PlayerInventory &&
		Request.StackCount == AvailableCount;

	URpgInventoryManagerComponent* DropTargetInventory = nullptr;
	if (!TryTransferManualDrop(
			Inventory,
			Item,
			DropPlanIntent,
			DropTargetInventory))
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			Request.StackCount);
		return;
	}

	if (bDropsWholePlayerEntry &&
		IsPlayerEquipmentPlacement(
			Request.ExpectedSourcePlacement))
	{
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}

	SendAndCacheManualDropFeedback(
		Inventory,
		Request,
		ERpgInventoryActionFeedbackResult::Success,
		Item,
		Request.StackCount,
		DropTargetInventory);
}

bool FRpgInventoryManualDropActionHandler::TryTransferManualDrop(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryItemInstance* Item,
	const FRpgInventoryTransferIntent& Intent,
	URpgInventoryManagerComponent*& OutTargetInventory)
{
	OutTargetInventory = nullptr;
	UWorld* const World = GetWorld();
	if (!SourceInventory || !Item || Intent.Quantity <= 0 ||
		Intent.ExpectedSourceQuantity <= 0 ||
		Intent.Quantity > Intent.ExpectedSourceQuantity ||
		!Intent.RequestId.IsValid() || !World ||
		!SourceInventory->ContainsItemInstance(Item))
	{
		return false;
	}

	const FTransform DropTransform = GetManualDropTransform();
	const bool bTransfersWholeEntry =
		Intent.Quantity == Intent.ExpectedSourceQuantity;
	auto CanTransferIntoActor =
		[SourceInventory, Item, &Intent, bTransfersWholeEntry](
			const ARpgDroppedInventoryActor* DropActor)
		{
			URpgInventoryManagerComponent* TargetInventory =
				DropActor
					? DropActor->GetLootInventoryManager()
					: nullptr;
			return TargetInventory != SourceInventory &&
				CanManualDropTargetAcceptTransferredStack(
					TargetInventory,
					Item,
					Intent.Quantity,
					bTransfersWholeEntry);
		};

	auto TryTransferIntoActor =
		[SourceInventory, &Intent](
			ARpgDroppedInventoryActor* DropActor)
		{
			if (!DropActor ||
				DropActor->GetLootInventoryManager() == SourceInventory)
			{
				return false;
			}

			const FRpgInventoryMutationResult TransferResult =
				DropActor->TransferItemFromInventoryByIntent(
					SourceInventory,
					Intent);
			return TransferResult.IsSuccess() &&
				TransferResult.AppliedQuantity == Intent.Quantity;
		};

	ARpgDroppedInventoryActor* TargetDropActor = nullptr;
	const float MergeRadius = GetManualDropMergeRadius();
	if (MergeRadius > 0.0f)
	{
		const float MergeRadiusSq = FMath::Square(MergeRadius);
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			ARpgDroppedInventoryActor* ExistingDrop = *It;
			if (ExistingDrop &&
				!ExistingDrop->IsPendingKillPending() &&
				FVector::DistSquared(
					ExistingDrop->GetActorLocation(),
					DropTransform.GetLocation()) <= MergeRadiusSq &&
				CanTransferIntoActor(ExistingDrop))
			{
				TargetDropActor = ExistingDrop;
				break;
			}
		}
	}

	bool bSpawnedTargetActor = false;
	if (!TargetDropActor)
	{
		TSubclassOf<ARpgDroppedInventoryActor> DropClass =
			GetManualDropActorClass();
		if (!DropClass)
		{
			DropClass = ARpgDroppedInventoryActor::StaticClass();
		}

		AController* OwnerController = Cast<AController>(GetOwner());
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.Instigator =
			OwnerController ? OwnerController->GetPawn() : nullptr;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::
				AdjustIfPossibleButAlwaysSpawn;

		TargetDropActor =
			World->SpawnActor<ARpgDroppedInventoryActor>(
				DropClass,
				DropTransform,
				SpawnParameters);
		bSpawnedTargetActor = TargetDropActor != nullptr;
	}

	if (bSpawnedTargetActor)
	{
		// A freshly spawned manual-drop actor represents only the concrete item
		// being discarded. Blueprint defaults are for placed/world-authored
		// loot and must not be duplicated into every player-created drop.
		TargetDropActor->SetPickupInventory(FInventoryPickup());
	}

	if (TargetDropActor && TryTransferIntoActor(TargetDropActor))
	{
		OutTargetInventory =
			TargetDropActor->GetLootInventoryManager();
		return true;
	}

	if (bSpawnedTargetActor)
	{
		TargetDropActor->Destroy();
	}
	return false;
}

FTransform
FRpgInventoryManualDropActionHandler::GetManualDropTransform() const
{
	const AController* OwnerController =
		Cast<AController>(GetOwner());
	const APawn* Pawn =
		OwnerController ? OwnerController->GetPawn() : nullptr;
	const AActor* SourceActor =
		Pawn ? Cast<AActor>(Pawn) : GetOwner();
	if (!SourceActor)
	{
		return FTransform::Identity;
	}

	FVector SpawnLocation = SourceActor->GetActorLocation();
	SpawnLocation += SourceActor->GetActorForwardVector() *
		GetManualDropForwardDistance();
	SpawnLocation += FVector::UpVector * GetManualDropUpOffset();
	return FTransform(SourceActor->GetActorRotation(), SpawnLocation);
}
