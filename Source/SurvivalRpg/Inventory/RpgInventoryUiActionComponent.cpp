#include "RpgInventoryUiActionComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryEquipmentPlacementPolicy.h"
#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_SlotContainerProvider.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryItemUseContext.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_ApplyItemEffects.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseConstructionSiteActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiActionComponent)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryUiActions, Log, All);

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(const URpgInventoryItemInstance* Item)
	{
		return Item ? Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
	}

	const URpgInventoryFragment_ItemTraits* GetUiActionItemTraitsForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	bool IsMaterialItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits && Traits->IsMaterial();
	}

	bool IsMaterialItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetUiActionItemTraitsForDefinition(ItemDefinition);
		return Traits && Traits->IsMaterial();
	}

	bool IsStackableItem(const URpgInventoryItemInstance* Item)
	{
		return Item &&
			URpgInventoryManagerComponent::
				GetEffectiveMaxStackSizeForDefinition(
					Item->GetItemDef()) > 1;
	}

	bool TryGetInventoryEntrySnapshot(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		FRpgInventoryEntryView& OutEntry)
	{
		OutEntry = FRpgInventoryEntryView();
		if (!Inventory || !ItemId.IsValid())
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId && Entry.Instance)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	bool IsSameLogicalInventoryPlacement(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A.GetContainerHandle() == B.GetContainerHandle() &&
			A.X == B.X &&
			A.Y == B.Y &&
			A.bRotated == B.bRotated;
	}

	bool IsReplayEpochCurrent(
		const TWeakObjectPtr<URpgInventoryManagerComponent>& Inventory,
		bool bHadInventory,
		uint64 ExpectedEpoch)
	{
		if (!bHadInventory)
		{
			return true;
		}

		const URpgInventoryManagerComponent* CurrentInventory =
			Inventory.Get();
		return CurrentInventory &&
			CurrentInventory->GetMutationEpoch() == ExpectedEpoch;
	}

	bool HasEquivalentRuntimeState(
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

	bool AreEquipmentSelectionsEquivalent(
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

	ERpgInventoryManualDropPolicy GetManualDropPolicy(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits ? Traits->GetResolvedManualDropPolicy() : ERpgInventoryManualDropPolicy::Direct;
	}

	bool CanTargetAcceptTransferredStack(URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 TransferCount, bool bTransfersWholeEntry)
	{
		if (!TargetInventory || !Item || TransferCount <= 0)
		{
			return false;
		}

		if (Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() !=
				nullptr &&
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

	void GatherCraftingStationsForOutputInventory(
		const URpgInventoryManagerComponent* Inventory,
		TArray<const URpgCraftingStationComponent*>& OutStations)
	{
		OutStations.Reset();
		if (!Inventory)
		{
			return;
		}

		const UWorld* InventoryWorld = Inventory->GetWorld();
		auto AddMatchingStation =
			[Inventory, InventoryWorld, &OutStations](
				const URpgCraftingStationComponent* CraftingStation)
			{
				if (IsValid(CraftingStation) &&
					!CraftingStation->HasAnyFlags(
						RF_ClassDefaultObject | RF_ArchetypeObject) &&
					CraftingStation->GetOutputInventory() == Inventory &&
					(!InventoryWorld ||
						CraftingStation->GetWorld() == InventoryWorld))
				{
					OutStations.AddUnique(CraftingStation);
				}
			};

		// Dedicated station output inventories are the hot preview path. Check every station component on the owner,
		// not just FindComponentByClass's first match, without scanning the global UObject table on every drag frame.
		if (const AActor* InventoryOwner = Inventory->GetOwner())
		{
			TInlineComponentArray<URpgCraftingStationComponent*> OwnerStations;
			InventoryOwner->GetComponents(OwnerStations);
			for (const URpgCraftingStationComponent* CraftingStation :
				OwnerStations)
			{
				AddMatchingStation(CraftingStation);
			}
		}
		if (!OutStations.IsEmpty())
		{
			return;
		}

		// SetOutputInventoryManager also permits an externally owned inventory. This is a rare configuration path;
		// fall back to exact live discovery so it remains secure and independent of component enumeration order.
		for (TObjectIterator<URpgCraftingStationComponent> It; It; ++It)
		{
			AddMatchingStation(*It);
		}
	}

	bool IsCraftingOutputInventory(const URpgInventoryManagerComponent* Inventory)
	{
		TArray<const URpgCraftingStationComponent*> CraftingStations;
		GatherCraftingStationsForOutputInventory(Inventory, CraftingStations);
		return !CraftingStations.IsEmpty();
	}

	bool IsUiTransferDirectionAllowed(
		const URpgInventoryManagerComponent* SourceInventory,
		const URpgInventoryManagerComponent* TargetInventory)
	{
		if (!SourceInventory || !TargetInventory)
		{
			return false;
		}

		// Crafting owns all deposits into its output buffer. UI transfers may only reorder it internally or withdraw.
		return SourceInventory == TargetInventory ||
			!IsCraftingOutputInventory(TargetInventory);
	}

	FGameplayTag GetActionTagForMutation(ERpgInventoryMutationOperation Operation)
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

	FGameplayTag GetActionTagForEquipmentIntent(
		ERpgInventoryEquipmentIntentOperation Operation)
	{
		switch (Operation)
		{
		case ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate:
			return RpgGameplayTags::
				Rpg_Inventory_Action_EquipAndActivate;
		case ERpgInventoryEquipmentIntentOperation::MoveToCarry:
			return RpgGameplayTags::Rpg_Inventory_Action_MoveToCarry;
		case ERpgInventoryEquipmentIntentOperation::EquipToSlot:
		case ERpgInventoryEquipmentIntentOperation::UnequipToContent:
		case ERpgInventoryEquipmentIntentOperation::ClearActiveSelection:
		default:
			return RpgGameplayTags::Rpg_Inventory_Action_Equip;
		}
	}

	ERpgInventoryActionFeedbackResult GetFeedbackForMutationResult(ERpgInventoryMutationResultCode Code)
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

	bool IsGenericUiMutationOperationAllowed(ERpgInventoryMutationOperation Operation)
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

	FRpgInventoryGridSize GetUiActionFootprint(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment = Item
			? Item->FindFragmentByClass<URpgInventoryFragment_SpatialItem>()
			: nullptr;
		if (SpatialFragment && SpatialFragment->Footprint.IsValid())
		{
			return SpatialFragment->Footprint;
		}

		FRpgInventoryGridSize Fallback;
		Fallback.Width = 1;
		Fallback.Height = 1;
		return Fallback;
	}

	bool CanUiActionRotate(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment = Item
			? Item->FindFragmentByClass<URpgInventoryFragment_SpatialItem>()
			: nullptr;
		return SpatialFragment && SpatialFragment->bAllowRotation;
	}

	int32 GetAvailableUpgradeCostCount(
		const URpgInventoryManagerComponent* PlayerInventory,
		const URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		int32 AvailableCount = 0;

		if (ConsumeOrder != ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly && PlayerInventory)
		{
			AvailableCount += PlayerInventory->GetTotalItemCountByDefinition(ItemDefinition);
		}

		if (ConsumeOrder != ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly && BaseStorage)
		{
			AvailableCount += BaseStorage->GetResourceCount(ItemDefinition);
		}

		return AvailableCount;
	}

	bool ConsumeUpgradeCostFromPlayer(URpgInventoryManagerComponent* PlayerInventory, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 CountToConsume)
	{
		return CountToConsume <= 0 || (PlayerInventory && PlayerInventory->ConsumeItemsByDefinition(ItemDefinition, CountToConsume));
	}

	bool ConsumeUpgradeCostFromBase(URpgBaseStorageComponent* BaseStorage, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 CountToConsume)
	{
		return CountToConsume <= 0 || (BaseStorage && BaseStorage->WithdrawResource(ItemDefinition, CountToConsume));
	}

	bool ConsumeUpgradeCost(
		URpgInventoryManagerComponent* PlayerInventory,
		URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 Count,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		if (!ItemDefinition || Count <= 0)
		{
			return false;
		}

		int32 RemainingCount = Count;
		auto ConsumeFromBase = [&]()
		{
			const int32 AvailableInBase = BaseStorage ? BaseStorage->GetResourceCount(ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInBase, RemainingCount);
			if (!ConsumeUpgradeCostFromBase(BaseStorage, ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		auto ConsumeFromPlayer = [&]()
		{
			const int32 AvailableInPlayer = PlayerInventory ? PlayerInventory->GetTotalItemCountByDefinition(ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInPlayer, RemainingCount);
			if (!ConsumeUpgradeCostFromPlayer(PlayerInventory, ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		switch (ConsumeOrder)
		{
		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseThenPlayer:
			return ConsumeFromBase() && ConsumeFromPlayer() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerThenBase:
			return ConsumeFromPlayer() && ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly:
			return ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly:
			return ConsumeFromPlayer() && RemainingCount <= 0;
		}

		return false;
	}
}

URpgInventoryUiActionComponent::URpgInventoryUiActionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ManualDropActorClass = ARpgDroppedInventoryActor::StaticClass();
}

void URpgInventoryUiActionComponent::RequestInventoryMutation_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMutationRequest Request)
{
	Request.EnsureRequestId();
	const FGameplayTag ActionTag = GetActionTagForMutation(Request.Operation);
	if (!IsGenericUiMutationOperationAllowed(Request.Operation))
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

	URpgInventoryItemInstance* ItemBeforeMutation = Inventory ? Inventory->FindItemById(Request.ItemId) : nullptr;
	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::NoAccess, Inventory, ItemBeforeMutation, Request.Quantity, Request.RequestId, Request.ItemId);
		return;
	}

	const int32 InventoryRevisionBefore = Inventory->GetInventoryRevision();
	const FRpgInventoryMutationResult Result =
		Inventory->ExecuteInventoryMutation(Request);
	if (!Result.IsSuccess())
	{
		SendActionFeedback(
			ActionTag,
			GetFeedbackForMutationResult(Result.Code),
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

	SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::Success, Inventory, ItemBeforeMutation, Result.AppliedQuantity, Result.RequestId, Request.ItemId);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryItem_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMoveIntent Intent)
{
	Intent.EnsureRequestId();
	URpgInventoryItemInstance* Item =
		Inventory ? Inventory->FindItemById(Intent.ItemId) : nullptr;
	FRpgInventoryEntryView SourceBeforeMove;
	const bool bCommitsCurrentSourceSnapshot =
		TryGetInventoryEntrySnapshot(
			Inventory,
			Intent.ItemId,
			SourceBeforeMove) &&
		SourceBeforeMove.EntryId == Intent.ExpectedEntryId &&
		SourceBeforeMove.Placement ==
			Intent.ExpectedSourcePlacement &&
		SourceBeforeMove.StackCount ==
			Intent.ExpectedQuantity;
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
			GetFeedbackForMutationResult(Result.Code),
			Inventory,
			Item,
			Intent.ExpectedQuantity,
			Result.RequestId,
			Intent.ItemId);
		return;
	}

	if (bCommitsCurrentSourceSnapshot &&
		Inventory == FindPlayerInventory() &&
		!IsSameLogicalInventoryPlacement(
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

void URpgInventoryUiActionComponent::RequestTransferInventoryItem_Implementation(
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
	if (!IsUiTransferDirectionAllowed(SourceInventory, TargetInventory))
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

	FRpgInventoryEntryView SourceEntry;
	const bool bHasLiveSourceEntry =
		Item &&
		TryGetInventoryEntrySnapshot(
			SourceInventory,
			Intent.ItemId,
			SourceEntry);

	const bool bTransfersWholePlayerEntry =
		bHasLiveSourceEntry &&
		SourceInventory == FindPlayerInventory() &&
		Intent.Quantity == SourceEntry.StackCount;
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		bTransfersWholePlayerEntry ? FindEquipmentLoadout() : nullptr;
	if (EquipmentLoadout &&
		!EquipmentLoadout->CanRemoveItemFromLoadout(Item))
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
			GetFeedbackForMutationResult(Result.Code),
			Item,
			Intent.Quantity);
		return;
	}

	if (bTransfersWholePlayerEntry)
	{
		const bool bClearedAssignments =
			ClearPlayerAssignmentsForItem(Item);
		ensureMsgf(
			bClearedAssignments,
			TEXT("Validated player assignment clear failed after transfer. Item=%s ItemId=%s"),
			*GetNameSafe(Item),
			*Intent.ItemId.ToString());
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

void URpgInventoryUiActionComponent::RequestExecuteInventoryItemAction_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemActionRequest Request)
{
	const FGameplayTag ActionTag = Request.Intent == ERpgInventoryItemActionIntent::Use
		? RpgGameplayTags::Rpg_Inventory_Action_Use
		: Request.Intent == ERpgInventoryItemActionIntent::MoveToCarry
			? RpgGameplayTags::Rpg_Inventory_Action_MoveToCarry
			: RpgGameplayTags::Rpg_Inventory_Action_EquipAndActivate;
	if (Request.Intent != ERpgInventoryItemActionIntent::Use)
	{
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::InvalidRequest, Inventory, nullptr,
			Request.StackCount, Request.RequestId, Request.ItemId);
		return;
	}

	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::NoAccess, Inventory, nullptr, Request.StackCount, Request.RequestId, Request.ItemId);
		return;
	}

	URpgInventoryItemInstance* Item = Inventory->FindItemById(Request.ItemId);
	if (!Item)
	{
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::MissingItem, Inventory, nullptr, Request.StackCount, Request.RequestId, Request.ItemId);
		return;
	}

	ExecuteUseInventoryItem(Inventory, Item, Request.StackCount, Request.RequestId);
}

void URpgInventoryUiActionComponent::
	RequestApplyInventoryEquipmentIntent_Implementation(
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
			ERpgInventoryEquipmentIntentOperation::UnequipToContent ||
		Intent.Operation ==
			ERpgInventoryEquipmentIntentOperation::ClearActiveSelection;
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
		ERpgInventoryEquipmentIntentOperation::ClearActiveSelection)
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
	if (!TryGetInventoryEntrySnapshot(
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
		CurrentEntry.Placement != Intent.ExpectedSourcePlacement ||
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
	const bool bHasContainerFragment =
		Item->FindFragmentByClass<
			URpgInventoryFragment_ItemContainer>() != nullptr;
	if (Intent.Operation !=
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent &&
		!bHasEquippableFragment && !bHasContainerFragment)
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
			TryAssignItemToDefaultEquipmentDestination(
				Item);
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
				ERpgInventoryActionFeedbackResult::
					NotEquippable;
			break;
		}
		bSucceeded =
			TryMoveItemToFirstCompatibleCarrySlot(
				Item);
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
		if (InventoryLayout->IsGearSlotAddress(SourceAddress) &&
			!CanMoveItemOutOfGearSlot(SourceAddress))
		{
			FailureResult =
				ERpgInventoryActionFeedbackResult::
					ServerRejected;
			break;
		}
		FailureResult =
			ERpgInventoryActionFeedbackResult::InventoryFull;
		bSucceeded =
			TryMoveItemToFirstCompatibleContentSlot(
				Item);
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
		const FGameplayTag ExpectedActivationRole =
			Intent.TargetEquipmentSlot ==
				ERpgEquipmentSlot::MainHand
				? RpgGameplayTags::Equipment_Slot_MainHand
				: RpgGameplayTags::Equipment_Slot_OffHand;
		if (!EquipmentLoadout ||
			!InventoryLayout ||
			!InventoryLayout->TryMakeSlotAddressFromPlacement(
				CurrentEntry.Placement,
				SourceAddress) ||
			InventoryLayout->GetCarryActivationRole(SourceAddress) !=
				ExpectedActivationRole ||
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

void URpgInventoryUiActionComponent::RequestQuickTransferItem_Implementation(
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
	if (!SourceInventory || !TargetInventory || !CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
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

	if (!IsUiTransferDirectionAllowed(SourceInventory, TargetInventory))
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

	URpgInventoryItemInstance* Item = SourceInventory->FindItemById(Request.ItemId);
	if (!Item)
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

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	const int32 RequestedCount = Request.StackCount <= 0 ? AvailableCount : Request.StackCount;
	if (AvailableCount <= 0 || RequestedCount <= 0 || RequestedCount > AvailableCount ||
		(SourceInventory == TargetInventory && RequestedCount != AvailableCount))
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
	if (!FindQuickTransferDestination(SourceInventory, TargetInventory, Request, TargetContainer, TargetPlacement))
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

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetInventoryEntrySnapshot(
			SourceInventory,
			Request.ItemId,
			SourceEntry))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::MissingItem,
			Item,
			RequestedCount);
		return;
	}

	const bool bTransfersWholePlayerEntry =
		SourceInventory != TargetInventory &&
		SourceInventory == FindPlayerInventory() &&
		RequestedCount == AvailableCount;
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		bTransfersWholePlayerEntry ? FindEquipmentLoadout() : nullptr;
	if (EquipmentLoadout &&
		!EquipmentLoadout->CanRemoveItemFromLoadout(Item))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			RequestedCount);
		return;
	}

	const FGuid ExpectedEntryId = Request.ExpectedEntryId.IsValid()
		? Request.ExpectedEntryId
		: SourceEntry.EntryId;
	const FRpgInventoryGridPlacement ExpectedSourcePlacement =
		Request.ExpectedSourcePlacement.IsValid()
			? Request.ExpectedSourcePlacement
			: SourceEntry.Placement;
	FRpgInventoryMutationResult MutationResult;
	if (SourceInventory == TargetInventory)
	{
		FRpgInventoryMoveIntent MoveIntent;
		MoveIntent.RequestId = Request.RequestId;
		MoveIntent.ItemId = Request.ItemId;
		MoveIntent.ExpectedEntryId = ExpectedEntryId;
		MoveIntent.ExpectedSourcePlacement =
			ExpectedSourcePlacement;
		MoveIntent.ExpectedQuantity = RequestedCount;
		MoveIntent.TargetPlacement = TargetPlacement;
		const bool bPreservesEquipmentIdentity =
			SourceInventory == FindPlayerInventory() &&
			(IsPlayerEquipmentPlacement(
				MoveIntent.ExpectedSourcePlacement) ||
				IsPlayerEquipmentPlacement(
					MoveIntent.TargetPlacement));
		MutationResult = bPreservesEquipmentIdentity
			? SourceInventory->MoveEquipmentItem(MoveTemp(MoveIntent))
			: SourceInventory->MoveItem(MoveTemp(MoveIntent));
	}
	else
	{
		FRpgInventoryTransferIntent TransferIntent;
		TransferIntent.RequestId = Request.RequestId;
		TransferIntent.ItemId = Request.ItemId;
		TransferIntent.ExpectedEntryId = ExpectedEntryId;
		TransferIntent.ExpectedSourcePlacement =
			ExpectedSourcePlacement;
		TransferIntent.TargetContainer = TargetContainer;
		TransferIntent.Quantity = RequestedCount;
		// An invalid exact placement deliberately lets the cross-inventory planner merge across every compatible
		// stack in the selected container before allocating the remaining quantity at its deterministic first fit.
		MutationResult = SourceInventory->TransferItem(
			TargetInventory,
			MoveTemp(TransferIntent));
	}

	if (!MutationResult.IsSuccess() || MutationResult.AppliedQuantity != RequestedCount)
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			GetFeedbackForMutationResult(MutationResult.Code),
			Item,
			RequestedCount);
		return;
	}

	if (bTransfersWholePlayerEntry)
	{
		const bool bClearedAssignments =
			ClearPlayerAssignmentsForItem(Item);
		ensureMsgf(
			bClearedAssignments,
			TEXT("Validated player assignment clear failed after quick transfer. Item=%s ItemId=%s"),
			*GetNameSafe(Item),
			*Request.ItemId.ToString());
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
				ExpectedSourcePlacement)) ||
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

void URpgInventoryUiActionComponent::RequestAssignItemToEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	FRpgInventoryEquipmentIntent Intent;
	if (TryBuildCurrentEquipmentIntent(
			PlayerInventory,
			Item,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			EquipmentSlot,
			Intent))
	{
		RequestApplyInventoryEquipmentIntent_Implementation(
			PlayerInventory,
			MoveTemp(Intent));
		return;
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Equip,
		ERpgInventoryActionFeedbackResult::InvalidRequest,
		PlayerInventory,
		Item,
		1);
}

void URpgInventoryUiActionComponent::RequestClearEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		FindEquipmentLoadout();
	if (!PlayerInventory || !EquipmentLoadout ||
		!FRpgInventoryEquipmentPlacementPolicy::
			IsManagedEquipmentSlot(EquipmentSlot))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidSlot,
			PlayerInventory,
			nullptr,
			1);
		return;
	}

	if (FRpgInventoryEquipmentPlacementPolicy::
			IsHandEquipmentSlot(EquipmentSlot))
	{
		URpgInventoryItemInstance* ActiveItem =
			EquipmentLoadout->GetItemInEquipmentSlot(EquipmentSlot);
		FRpgInventoryEquipmentIntent Intent;
		if (TryBuildCurrentEquipmentIntent(
				PlayerInventory,
				ActiveItem,
				ERpgInventoryEquipmentIntentOperation::
					ClearActiveSelection,
				EquipmentSlot,
				Intent))
		{
			RequestApplyInventoryEquipmentIntent_Implementation(
				PlayerInventory,
				MoveTemp(Intent));
			return;
		}
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::MissingItem,
			PlayerInventory,
			ActiveItem,
			1);
		return;
	}

	FRpgInventorySlotAddress GearAddress;
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	URpgInventoryItemInstance* SlotItem = nullptr;
	if (InventoryLayout &&
		URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(
			EquipmentSlot,
			GearAddress))
	{
		SlotItem =
			InventoryLayout->GetItemInSlotAddress(GearAddress);
	}

	FRpgInventoryEquipmentIntent Intent;
	if (!TryBuildCurrentEquipmentIntent(
			PlayerInventory,
			SlotItem,
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent,
			ERpgEquipmentSlot::None,
			Intent))
	{
		SyncEquipmentLoadoutFromGearSlots();
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::MissingItem,
			PlayerInventory,
			SlotItem,
			1);
		return;
	}

	RequestApplyInventoryEquipmentIntent_Implementation(
		PlayerInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestTransferItemStack_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount)
{
	FRpgInventoryQuickTransferRequest Request;
	Request.EnsureRequestId();
	Request.ItemId = Item ? Item->GetItemId() : FRpgInventoryItemId();
	Request.StackCount = StackCount;
	FRpgInventoryEntryView SourceEntry;
	if (TryGetInventoryEntrySnapshot(
			SourceInventory,
			Request.ItemId,
			SourceEntry))
	{
		Request.ExpectedEntryId = SourceEntry.EntryId;
		Request.ExpectedSourcePlacement = SourceEntry.Placement;
	}
	RequestQuickTransferItem_Implementation(SourceInventory, TargetInventory, MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestTransferItemStackToPlacement_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, FRpgInventoryGridPlacement TargetPlacement)
{
	FRpgInventoryTransferIntent Intent;
	Intent.EnsureRequestId();
	Intent.ItemId = Item
		? Item->GetItemId()
		: FRpgInventoryItemId();
	Intent.TargetContainer =
		TargetPlacement.GetContainerHandle();
	Intent.TargetPlacement = TargetPlacement;
	Intent.Quantity = StackCount;
	FRpgInventoryEntryView SourceEntry;
	if (TryGetInventoryEntrySnapshot(
			SourceInventory,
			Intent.ItemId,
			SourceEntry))
	{
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
	}
	RequestTransferInventoryItem_Implementation(
		SourceInventory,
		TargetInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestApplyInventorySort_Implementation(URpgInventoryManagerComponent* Inventory, ERpgInventorySortMode SortMode)
{
	if (!CanAccessInventory(Inventory))
	{
		return;
	}

	Inventory->ApplyInventorySort(SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntry_Implementation(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetIndex)
{
	if (!CanAccessInventory(Inventory) || !Inventory->ContainsEntry(EntryId))
	{
		return;
	}

	Inventory->MoveInventoryEntry(EntryId, TargetIndex);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntryToPlacement_Implementation(URpgInventoryManagerComponent* Inventory, FGuid EntryId, FRpgInventoryGridPlacement TargetPlacement)
{
	if (!Inventory || !CanAccessInventory(Inventory) || !Inventory->ContainsEntry(EntryId))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::NoAccess, Inventory, nullptr, 1);
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries = Inventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry = Entries.FindByPredicate([EntryId](const FRpgInventoryEntryView& Candidate)
	{
		return Candidate.EntryId == EntryId;
	});
	if (!Entry || !Entry->Instance)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::MissingItem, Inventory, nullptr, 1);
		return;
	}

	FRpgInventoryMoveIntent Intent;
	Intent.EnsureRequestId();
	Intent.ItemId = Entry->ItemId;
	Intent.ExpectedEntryId = Entry->EntryId;
	Intent.ExpectedSourcePlacement = Entry->Placement;
	Intent.ExpectedQuantity = Entry->StackCount;
	Intent.TargetPlacement = TargetPlacement;
	RequestMoveInventoryItem_Implementation(
		Inventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestMoveItemToInventorySlotAddress_Implementation(URpgInventoryItemInstance* Item, FRpgInventorySlotAddress TargetAddress)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!PlayerInventory || !InventoryLayout || !Item || PlayerInventory->GetItemStackCount(Item) <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidRequest, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventoryGridPlacement TargetPlacement;
	if (!InventoryLayout->ResolveSlotAddress(TargetAddress, TargetPlacement) ||
		!InventoryLayout->CanItemUseSlotAddress(Item, TargetAddress))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventoryGridPlacement SourcePlacement;
	FRpgInventorySlotAddress SourceAddress;
	URpgInventoryItemInstance* TargetItem = PlayerInventory->GetItemAtContainerCell(
		TargetPlacement.GetContainerHandle(), TargetPlacement.X, TargetPlacement.Y);
	const bool bHasSourceAddress = PlayerInventory->GetItemPlacement(Item, SourcePlacement) &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(SourcePlacement, SourceAddress);
	if (bHasSourceAddress && InventoryLayout->IsGearSlotAddress(SourceAddress) && !CanMoveItemOutOfGearSlot(SourceAddress))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::ServerRejected, PlayerInventory, Item, 1);
		return;
	}

	if (bHasSourceAddress && InventoryLayout->IsGearSlotAddress(SourceAddress))
	{
		ERpgEquipmentSlot SourceEquipmentSlot = ERpgEquipmentSlot::None;
		if (URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearGroupId(SourceAddress.ContainerId, SourceEquipmentSlot) &&
			URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(SourceEquipmentSlot))
		{
			bool bTargetIsStaticContent = false;
			for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
			{
				if (Group.MakeAddress(TargetAddress.X, TargetAddress.Y).GetContainerHandle() == TargetAddress.GetContainerHandle() &&
					Group.ContainsCell(TargetAddress.X, TargetAddress.Y))
				{
					bTargetIsStaticContent = Group.GroupKind == ERpgInventorySlotGroupKind::Content && !Group.bProvidedByEquipment;
					break;
				}
			}

			if (!bTargetIsStaticContent)
			{
				SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
				return;
			}
		}
	}

	if (TargetItem &&
		bHasSourceAddress &&
		!InventoryLayout->CanItemUseSlotAddress(TargetItem, SourceAddress))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetInventoryEntrySnapshot(
			PlayerInventory,
			Item->GetItemId(),
			SourceEntry))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventoryMoveIntent Intent;
	Intent.EnsureRequestId();
	Intent.ItemId = SourceEntry.ItemId;
	Intent.ExpectedEntryId = SourceEntry.EntryId;
	Intent.ExpectedSourcePlacement = SourceEntry.Placement;
	Intent.ExpectedQuantity = SourceEntry.StackCount;
	Intent.TargetPlacement = TargetPlacement;
	const FRpgInventoryMutationResult Plan =
		PlayerInventory->PlanMoveItem(Intent);
	if (!Plan.IsSuccess())
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
		return;
	}

	const bool bPreservesEquipmentIdentity =
		IsPlayerEquipmentPlacement(SourceEntry.Placement) ||
		IsPlayerEquipmentPlacement(TargetPlacement);
	const FRpgInventoryMutationResult Result =
		bPreservesEquipmentIdentity
			? PlayerInventory->MoveEquipmentItem(Intent)
			: PlayerInventory->MoveItem(Intent);
	if (!Result.IsSuccess())
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::ServerRejected, PlayerInventory, Item, 1);
		return;
	}

	if (!IsSameLogicalInventoryPlacement(
			SourceEntry.Placement,
			TargetPlacement) &&
		(IsPlayerEquipmentPlacement(SourceEntry.Placement) ||
			IsPlayerEquipmentPlacement(TargetPlacement)))
	{
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		ERpgInventoryActionFeedbackResult::Success,
		PlayerInventory,
		Item,
		PlayerInventory->GetItemStackCount(Item));
}

void URpgInventoryUiActionComponent::RequestEquipSlotContainerItem_Implementation(ERpgEquipmentSlot ContainerSlot, URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory || !Item || PlayerInventory->GetItemStackCount(Item) <= 0 || !URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(ContainerSlot))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::InvalidRequest, PlayerInventory, Item, 1);
		return;
	}

	if (Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() == nullptr)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NotEquippable, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventoryEquipmentIntent Intent;
	if (!TryBuildCurrentEquipmentIntent(
			PlayerInventory,
			Item,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ContainerSlot,
			Intent))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			PlayerInventory,
			Item,
			1);
		return;
	}

	RequestApplyInventoryEquipmentIntent_Implementation(
		PlayerInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestUnequipSlotContainerItem_Implementation(
	ERpgEquipmentSlot ContainerSlot,
	FRpgInventoryItemId ExpectedProviderItemId)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!PlayerInventory ||
		!InventoryLayout ||
		!ExpectedProviderItemId.IsValid() ||
		!URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(ContainerSlot))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			PlayerInventory,
			nullptr,
			1);
		return;
	}

	FRpgInventorySlotAddress GearAddress;
	FRpgInventoryGridPlacement GearPlacement;
	if (!URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(ContainerSlot, GearAddress) ||
		!InventoryLayout->ResolveSlotAddress(GearAddress, GearPlacement))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidSlot,
			PlayerInventory,
			nullptr,
			1);
		return;
	}

	URpgInventoryItemInstance* ProviderItem = PlayerInventory->GetItemAtContainerCell(
		GearPlacement.GetContainerHandle(),
		GearPlacement.X,
		GearPlacement.Y);
	if (!ProviderItem)
	{
		// Repair a stale presentation/loadout mirror from the physical inventory truth.
		SyncEquipmentLoadoutFromGearSlots();
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::MissingItem,
			PlayerInventory,
			nullptr,
			1);
		return;
	}

	if (ProviderItem->GetItemId() != ExpectedProviderItemId)
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			PlayerInventory,
			ProviderItem,
			1);
		return;
	}

	FRpgInventoryEquipmentIntent Intent;
	if (!TryBuildCurrentEquipmentIntent(
			PlayerInventory,
			ProviderItem,
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent,
			ERpgEquipmentSlot::None,
			Intent))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			PlayerInventory,
			ProviderItem,
			1);
		return;
	}

	RequestApplyInventoryEquipmentIntent_Implementation(
		PlayerInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestActivateCarrySlot_Implementation(FRpgInventorySlotAddress CarrySlotAddress)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	if (!PlayerInventory || !InventoryLayout || !EquipmentLoadout || !InventoryLayout->IsCarrySlotAddress(CarrySlotAddress))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, nullptr, 1);
		return;
	}

	URpgInventoryItemInstance* Item = InventoryLayout->GetItemInSlotAddress(CarrySlotAddress);
	if (!Item)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::MissingItem, PlayerInventory, nullptr, 1);
		return;
	}

	const FGameplayTag ActivationRole = InventoryLayout->GetCarryActivationRole(CarrySlotAddress);
	bool bActivated = false;
	if (ActivationRole == RpgGameplayTags::Equipment_Slot_OffHand)
	{
		bActivated = EquipmentLoadout->ActivateOffHandItem(Item);
	}
	else if (ActivationRole == RpgGameplayTags::Equipment_Slot_MainHand)
	{
		bActivated = EquipmentLoadout->ActivateMainHandItem(Item);
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Equip,
		bActivated ? ERpgInventoryActionFeedbackResult::Success : ERpgInventoryActionFeedbackResult::NotEquippable,
		PlayerInventory,
		Item,
		1);
}

void URpgInventoryUiActionComponent::RequestClearActiveHands_Implementation()
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->ClearActiveHands();
	}
}

void URpgInventoryUiActionComponent::RequestMutateQuickAccessBinding_Implementation(FRpgQuickAccessMutationRequest Request)
{
	Request.EnsureRequestId();

	URpgActionBarComponent* ActionBar = FindActionBar();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryItemInstance* ContextItem = PlayerInventory && Request.ContextItemId.IsValid()
		? PlayerInventory->FindItemById(Request.ContextItemId)
		: nullptr;
	auto SendQuickAccessFeedback = [this, &Request, PlayerInventory, ContextItem](ERpgInventoryActionFeedbackResult Result)
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			Result,
			PlayerInventory,
			ContextItem,
			1,
			Request.RequestId,
			Request.ContextItemId);
	};

	if (!ActionBar || !InventoryLayout || !PlayerInventory)
	{
		SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidRequest);
		return;
	}
	if (!FMath::IsWithinInclusive(Request.SlotIndex, 0, 7))
	{
		SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidSlot);
		return;
	}

	const auto IsCarryBinding = [](const FRpgActionBarSlot& Slot)
	{
		return Slot.SlotType == ERpgActionBarSlotType::CarrySlot ||
			Slot.SlotType == ERpgActionBarSlotType::CarrySlotBinding;
	};
	const auto IsConsumableBinding = [](const FRpgActionBarSlot& Slot)
	{
		return Slot.SlotType == ERpgActionBarSlotType::Consumable ||
			Slot.SlotType == ERpgActionBarSlotType::InventorySlotBinding;
	};

	switch (Request.Operation)
	{
	case ERpgQuickAccessMutationOperation::BindCarry:
	{
		if (!Request.SourceAddress.IsValid() || Request.ExpectedCarryRole.IsNone() ||
			Request.SourceAddress.ContainerId != Request.ExpectedCarryRole ||
			!InventoryLayout->IsCarrySlotAddress(Request.SourceAddress))
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidSlot);
			return;
		}

		URpgInventoryItemInstance* SourceItem = InventoryLayout->GetItemInSlotAddress(Request.SourceAddress);
		if (!Request.ContextItemId.IsValid() || !SourceItem || SourceItem->GetItemId() != Request.ContextItemId)
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::MissingItem);
			return;
		}
		if (!InventoryLayout->CanBindSlotAddressToActionbar(Request.SourceAddress, SourceItem))
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidSlot);
			return;
		}

		const bool bApplied =
			ActionBar->TryBindCarrySlotToSlotAuthority(Request.SlotIndex, Request.SourceAddress);
		SendQuickAccessFeedback(bApplied
			? ERpgInventoryActionFeedbackResult::Success
			: ERpgInventoryActionFeedbackResult::ServerRejected);
		return;
	}

	case ERpgQuickAccessMutationOperation::BindConsumable:
	{
		if (!Request.SourceAddress.IsValid() || !Request.ExpectedConsumableDefinition ||
			!Request.ExpectedPreferredItemId.IsValid() ||
			Request.ContextItemId != Request.ExpectedPreferredItemId ||
			InventoryLayout->IsCarrySlotAddress(Request.SourceAddress))
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidRequest);
			return;
		}

		URpgInventoryItemInstance* SourceItem = InventoryLayout->GetItemInSlotAddress(Request.SourceAddress);
		if (!SourceItem || SourceItem->GetItemId() != Request.ExpectedPreferredItemId ||
			SourceItem->GetItemDef() != Request.ExpectedConsumableDefinition)
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::MissingItem);
			return;
		}
		if (!InventoryLayout->CanBindSlotAddressToActionbar(Request.SourceAddress, SourceItem) ||
			SourceItem->FindFragmentByClass<URpgInventoryFragment_UsableItem>() == nullptr)
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::CannotUse);
			return;
		}

		const bool bApplied =
			ActionBar->TryBindInventorySlotToSlotAuthority(Request.SlotIndex, Request.SourceAddress);
		SendQuickAccessFeedback(bApplied
			? ERpgInventoryActionFeedbackResult::Success
			: ERpgInventoryActionFeedbackResult::ServerRejected);
		return;
	}

	case ERpgQuickAccessMutationOperation::ClearCarry:
	{
		if (Request.ExpectedCarryRole.IsNone())
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidRequest);
			return;
		}

		const FRpgActionBarSlot CurrentSlot = ActionBar->GetSlot(Request.SlotIndex);
		if (!IsCarryBinding(CurrentSlot) || CurrentSlot.CarryRole != Request.ExpectedCarryRole)
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::ServerRejected);
			return;
		}

		SendQuickAccessFeedback(ActionBar->TryClearSlotAuthority(Request.SlotIndex)
			? ERpgInventoryActionFeedbackResult::Success
			: ERpgInventoryActionFeedbackResult::ServerRejected);
		return;
	}

	case ERpgQuickAccessMutationOperation::ClearConsumable:
	{
		if (!Request.ExpectedConsumableDefinition)
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidRequest);
			return;
		}

		const FRpgActionBarSlot CurrentSlot = ActionBar->GetSlot(Request.SlotIndex);
		if (!IsConsumableBinding(CurrentSlot) ||
			CurrentSlot.ConsumableDefinition != Request.ExpectedConsumableDefinition ||
			CurrentSlot.PreferredItemId != Request.ExpectedPreferredItemId)
		{
			SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::ServerRejected);
			return;
		}

		SendQuickAccessFeedback(ActionBar->TryClearSlotAuthority(Request.SlotIndex)
			? ERpgInventoryActionFeedbackResult::Success
			: ERpgInventoryActionFeedbackResult::ServerRejected);
		return;
	}
	}

	SendQuickAccessFeedback(ERpgInventoryActionFeedbackResult::InvalidRequest);
}

void URpgInventoryUiActionComponent::RequestBindActionBarToInventorySlot_Implementation(int32 ActionBarSlotIndex, FRpgInventorySlotAddress SlotAddress)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	const URpgInventoryItemInstance* SourceItem = InventoryLayout
		? InventoryLayout->GetItemInSlotAddress(SlotAddress)
		: nullptr;

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation = ERpgQuickAccessMutationOperation::BindConsumable;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.SourceAddress = SlotAddress;
	Request.ExpectedConsumableDefinition = SourceItem ? SourceItem->GetItemDef() : nullptr;
	Request.ExpectedPreferredItemId = SourceItem ? SourceItem->GetItemId() : FRpgInventoryItemId();
	Request.ContextItemId = Request.ExpectedPreferredItemId;
	RequestMutateQuickAccessBinding_Implementation(Request);
}

void URpgInventoryUiActionComponent::RequestBindActionBarToCarrySlot_Implementation(int32 ActionBarSlotIndex, FRpgInventorySlotAddress CarrySlotAddress)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	const URpgInventoryItemInstance* SourceItem = InventoryLayout
		? InventoryLayout->GetItemInSlotAddress(CarrySlotAddress)
		: nullptr;

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation = ERpgQuickAccessMutationOperation::BindCarry;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.SourceAddress = CarrySlotAddress;
	Request.ExpectedCarryRole = CarrySlotAddress.ContainerId;
	Request.ContextItemId = SourceItem ? SourceItem->GetItemId() : FRpgInventoryItemId();
	RequestMutateQuickAccessBinding_Implementation(Request);
}

void URpgInventoryUiActionComponent::RequestClearActionBarCarryBinding_Implementation(
	int32 ActionBarSlotIndex,
	FName ExpectedCarryRole)
{
	URpgActionBarComponent* ActionBar = FindActionBar();
	const FRpgActionBarSlot CurrentSlot = ActionBar ? ActionBar->GetSlot(ActionBarSlotIndex) : FRpgActionBarSlot();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	const URpgInventoryItemInstance* ContextItem = InventoryLayout && CurrentSlot.SlotAddress.IsValid()
		? InventoryLayout->GetItemInSlotAddress(CurrentSlot.SlotAddress)
		: nullptr;

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation = ERpgQuickAccessMutationOperation::ClearCarry;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.ExpectedCarryRole = ExpectedCarryRole;
	Request.ContextItemId = ContextItem ? ContextItem->GetItemId() : FRpgInventoryItemId();
	RequestMutateQuickAccessBinding_Implementation(Request);
}

void URpgInventoryUiActionComponent::RequestClearActionBarConsumableBinding_Implementation(
	int32 ActionBarSlotIndex,
	TSubclassOf<URpgInventoryItemDefinition> ExpectedConsumableDefinition)
{
	URpgActionBarComponent* ActionBar = FindActionBar();
	const FRpgActionBarSlot CurrentSlot = ActionBar ? ActionBar->GetSlot(ActionBarSlotIndex) : FRpgActionBarSlot();

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation = ERpgQuickAccessMutationOperation::ClearConsumable;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.ExpectedConsumableDefinition = ExpectedConsumableDefinition;
	Request.ExpectedPreferredItemId = CurrentSlot.PreferredItemId;
	Request.ContextItemId = CurrentSlot.PreferredItemId;
	RequestMutateQuickAccessBinding_Implementation(Request);
}

void URpgInventoryUiActionComponent::RequestSplitItemStack_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 SplitCount, FRpgInventoryGridPlacement TargetPlacement)
{
	RequestSplitItemStackById_Implementation(
		Inventory,
		Item ? Item->GetItemId() : FRpgInventoryItemId(),
		SplitCount,
		TargetPlacement,
		FGuid());
}

void URpgInventoryUiActionComponent::RequestSplitItemStackById_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemId ItemId,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement,
	FGuid RequestId)
{
	URpgInventoryItemInstance* Item = Inventory ? Inventory->FindItemById(ItemId) : nullptr;
	int32 ActualSplitCount = 0;
	FRpgInventoryGridPlacement ActualTargetPlacement;
	if (!CanSplitItemStack(Inventory, Item, SplitCount, TargetPlacement, ActualSplitCount, ActualTargetPlacement))
	{
		const ERpgInventoryActionFeedbackResult Result = !Item || !IsStackableItem(Item)
			? ERpgInventoryActionFeedbackResult::NotStackable
			: ERpgInventoryActionFeedbackResult::InventoryFull;
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Split, Result, Inventory, Item, SplitCount, RequestId, ItemId);
		return;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TryGetInventoryEntrySnapshot(
			Inventory,
			ItemId,
			SourceEntry))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Split, ERpgInventoryActionFeedbackResult::MissingItem,
			Inventory, Item, ActualSplitCount, RequestId, ItemId);
		return;
	}

	FRpgInventoryMutationRequest Request;
	Request.Operation = ERpgInventoryMutationOperation::Split;
	Request.ItemId = ItemId;
	Request.ExpectedEntryId = SourceEntry.EntryId;
	Request.Source = SourceEntry.Placement.GetContainerHandle();
	Request.ExpectedSourcePlacement = SourceEntry.Placement;
	Request.Target = ActualTargetPlacement.GetContainerHandle();
	Request.TargetPlacement = ActualTargetPlacement;
	Request.Quantity = ActualSplitCount;
	Request.RequestId = RequestId;
	RequestInventoryMutation_Implementation(Inventory, Request);
}

void URpgInventoryUiActionComponent::RequestUseInventoryItem_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount)
{
	ExecuteUseInventoryItem(Inventory, Item, StackCount, FGuid());
}

void URpgInventoryUiActionComponent::ExecuteUseInventoryItem(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FGuid& RequestId)
{
	const FRpgInventoryItemId ItemId = Item ? Item->GetItemId() : FRpgInventoryItemId();
	auto SendUseFeedback = [this, Inventory, Item, &RequestId, ItemId](ERpgInventoryActionFeedbackResult Result, int32 Count)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, Result, Inventory, Item, Count, RequestId, ItemId);
	};

	if (!Inventory || !Item)
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::InvalidRequest, StackCount);
		return;
	}

	if (!CanAccessInventory(Inventory))
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::NoAccess, StackCount);
		return;
	}

	const int32 AvailableCount = Inventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::MissingItem, StackCount);
		return;
	}

	const URpgInventoryFragment_UsableItem* UsableFragment = Item->FindFragmentByClass<URpgInventoryFragment_UsableItem>();
	if (!UsableFragment || !UsableFragment->UseAbility)
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::CannotUse, StackCount);
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (UsableFragment->bOnlyFromPlayerInventory && Inventory != PlayerInventory)
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::WrongInventory, StackCount);
		return;
	}

	const int32 ConsumePerUse =
		FMath::Max(0, UsableFragment->ConsumeCount);
	if (StackCount <= 0 ||
		(ConsumePerUse == 0 && StackCount != 1))
	{
		// Reusable/zero-cost items are one activation per request. Otherwise a client could
		// amplify EventMagnitude and effects without any inventory-backed upper bound.
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			StackCount);
		return;
	}

	const int64 RequestedConsumeCount =
		static_cast<int64>(ConsumePerUse) *
		static_cast<int64>(StackCount);
	if (RequestedConsumeCount > MAX_int32)
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			StackCount);
		return;
	}

	const int32 UseCount = StackCount;
	const int32 ConsumeCount =
		static_cast<int32>(RequestedConsumeCount);
	if (ConsumeCount > AvailableCount)
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::MissingItem, ConsumeCount);
		return;
	}
	if (ConsumeCount > 0 &&
		!Inventory->CanConsumeItemById(Item->GetItemId(), ConsumeCount))
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::ServerRejected,
			ConsumeCount);
		return;
	}

	const bool bConsumesWholePlayerEntry =
		Inventory == PlayerInventory &&
		ConsumeCount > 0 &&
		ConsumeCount == AvailableCount;
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		bConsumesWholePlayerEntry ? FindEquipmentLoadout() : nullptr;
	if (EquipmentLoadout &&
		!EquipmentLoadout->CanRemoveItemFromLoadout(Item))
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::ServerRejected,
			ConsumeCount);
		return;
	}

	URpgAbilitySystemComponent* AbilitySystem = FindPlayerAbilitySystem();
	if (!AbilitySystem)
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::ServerRejected, StackCount);
		return;
	}

	AController* OwnerController = Cast<AController>(GetOwner());
	AActor* AvatarActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	FGameplayEventData EventData;
	EventData.EventTag = RpgGameplayTags::Rpg_Inventory_Action_Use;
	EventData.Instigator = AvatarActor;
	EventData.Target = AvatarActor;
	EventData.OptionalObject = Item;
	EventData.EventMagnitude = static_cast<float>(UseCount);

	URpgInventoryItemUseContext* UseContext = NewObject<URpgInventoryItemUseContext>(this);
	UseContext->Initialize(Inventory, Item, UseCount, ConsumeCount);
	const bool bConsumesFromPlayerInventory =
		Inventory == PlayerInventory && ConsumeCount > 0;
	if (bConsumesFromPlayerInventory)
	{
		const TWeakObjectPtr<URpgInventoryItemInstance> WeakItem = Item;
		const TWeakObjectPtr<URpgInventoryManagerComponent> WeakInventory =
			Inventory;
		const FRpgInventoryItemId UsedItemId = Item->GetItemId();
		const TSharedRef<bool> bRequiresEquipmentCleanup =
			MakeShared<bool>(false);
		UseContext->SetConsumePreflightCallback(
			FRpgInventoryUseConsumePreflight::CreateWeakLambda(
				this,
				[this, WeakInventory, UsedItemId, ConsumeCount,
					bRequiresEquipmentCleanup]()
				{
					URpgInventoryManagerComponent* CurrentInventory =
						WeakInventory.Get();
					URpgInventoryItemInstance* CurrentItem =
						CurrentInventory
							? CurrentInventory->FindItemById(UsedItemId)
							: nullptr;
					if (!CurrentItem)
					{
						return false;
					}

					FRpgInventoryGridPlacement CurrentPlacement;
					*bRequiresEquipmentCleanup =
						(CurrentInventory->GetItemPlacement(
								CurrentItem,
								CurrentPlacement) &&
							IsPlayerEquipmentPlacement(
								CurrentPlacement));
					if (!*bRequiresEquipmentCleanup)
					{
						if (URpgEquipmentLoadoutComponent*
								CurrentLoadout =
									FindEquipmentLoadout())
						{
							const TArray<FRpgEquipmentLoadoutSlot>
								CurrentSlots =
									CurrentLoadout->
										GetLoadoutSlots();
							*bRequiresEquipmentCleanup =
								CurrentSlots.ContainsByPredicate(
										[CurrentItem](
											const FRpgEquipmentLoadoutSlot&
												Slot)
										{
											return Slot.Item ==
												CurrentItem;
										});
						}
					}

					const int32 CurrentCount =
						CurrentInventory->GetItemStackCount(
							CurrentItem);
					if (ConsumeCount < CurrentCount)
					{
						return true;
					}
					if (ConsumeCount != CurrentCount)
					{
						return false;
					}

					URpgEquipmentLoadoutComponent* CurrentLoadout =
						FindEquipmentLoadout();
					return !CurrentLoadout ||
						CurrentLoadout->CanRemoveItemFromLoadout(
							CurrentItem);
				}));
		UseContext->SetConsumeSucceededCallback(
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, WeakInventory, WeakItem, UsedItemId,
					bRequiresEquipmentCleanup]()
				{
					URpgInventoryManagerComponent* CurrentInventory =
						WeakInventory.Get();
					if (CurrentInventory &&
						CurrentInventory->FindItemById(UsedItemId))
					{
						// A delayed consume that was initially full may now be partial after
						// another stack merge. Keep valid assignments until the concrete
						// identity actually leaves the inventory.
						return;
					}

					URpgInventoryItemInstance* ConsumedItem =
						WeakItem.Get();
					if (!ConsumedItem)
					{
						return;
					}

					if (!*bRequiresEquipmentCleanup)
					{
						return;
					}

					const bool bClearedAssignments =
						ClearPlayerAssignmentsForItem(ConsumedItem);
					ensureMsgf(
						bClearedAssignments,
						TEXT("Validated player assignment clear failed after consuming item %s (%s)."),
						*GetNameSafe(ConsumedItem),
						*ConsumedItem->GetItemId().ToString());
					SyncEquipmentLoadoutFromGearSlots();
					SyncActiveHandsFromCarrySlots();
				}));
	}

	const bool bUsesApplyEffectsContext = UsableFragment->UseAbility->IsChildOf(URpgGameplayAbility_ApplyItemEffects::StaticClass());
	if (bUsesApplyEffectsContext)
	{
		URpgGameplayAbility_ApplyItemEffects::RegisterPendingUseContext(AbilitySystem, Item, UseContext);
	}

	FGameplayAbilitySpec UseSpec(UsableFragment->UseAbility, FMath::Max(1, UsableFragment->AbilityLevel), INDEX_NONE, Item);
	const FGameplayAbilitySpecHandle ActivatedHandle = AbilitySystem->GiveAbilityAndActivateOnce(UseSpec, &EventData);
	if (!ActivatedHandle.IsValid())
	{
		if (bUsesApplyEffectsContext)
		{
			URpgGameplayAbility_ApplyItemEffects::ClearPendingUseContext(AbilitySystem, Item);
		}

		SendUseFeedback(ERpgInventoryActionFeedbackResult::AbilityRejected, StackCount);
		return;
	}

	if (UsableFragment->bConsumeOnActivationAccepted && ConsumeCount > 0)
	{
		if (!UseContext->TryConsume())
		{
			SendUseFeedback(ERpgInventoryActionFeedbackResult::ServerRejected, ConsumeCount);
			return;
		}
	}

	SendUseFeedback(ERpgInventoryActionFeedbackResult::Success, ConsumeCount);
}

void URpgInventoryUiActionComponent::RequestEquipInventoryItem_Implementation(URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory || !Item || PlayerInventory->GetItemStackCount(Item) <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::MissingItem, PlayerInventory, Item, 1);
		return;
	}

	if (Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() == nullptr &&
		Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() == nullptr)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NotEquippable, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventoryEquipmentIntent Intent;
	if (!TryBuildCurrentEquipmentIntent(
			PlayerInventory,
			Item,
			ERpgInventoryEquipmentIntentOperation::
				EquipDefaultAndActivate,
			ERpgEquipmentSlot::None,
			Intent))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			PlayerInventory,
			Item,
			1);
		return;
	}

	RequestApplyInventoryEquipmentIntent_Implementation(
		PlayerInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestUnequipInventoryItemToContentSlot_Implementation(URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory || !Item ||
		PlayerInventory->GetItemStackCount(Item) <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::MissingItem, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventoryEquipmentIntent Intent;
	if (!TryBuildCurrentEquipmentIntent(
			PlayerInventory,
			Item,
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent,
			ERpgEquipmentSlot::None,
			Intent))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			PlayerInventory,
			Item,
			1);
		return;
	}

	RequestApplyInventoryEquipmentIntent_Implementation(
		PlayerInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestDropInventoryItem_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount, bool bConfirmed)
{
	FRpgInventoryManualDropRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = Item ? Item->GetItemId() : FRpgInventoryItemId();
	Request.StackCount = StackCount;
	Request.bConfirmed = bConfirmed;

	if (Inventory && Item)
	{
		const TArray<FRpgInventoryEntryView> Entries = Inventory->GetAllEntries();
		if (const FRpgInventoryEntryView* Entry = Entries.FindByPredicate(
				[Item](const FRpgInventoryEntryView& Candidate)
				{
					return Candidate.Instance.Get() == Item;
				}))
		{
			Request.EntryId = Entry->EntryId;
			Request.ItemId = Entry->ItemId;
			Request.ExpectedSourcePlacement = Entry->Placement;
			// Preserve the legacy pointer API's <= 0 "whole stack" and oversized-request clamp while translating it
			// into the exact-count ID contract. New callers must provide their exact count directly.
			Request.StackCount = StackCount <= 0
				? Entry->StackCount
				: FMath::Min(StackCount, Entry->StackCount);
		}
	}

	RequestDropInventoryItemById_Implementation(Inventory, MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestDropInventoryItemById_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryManualDropRequest Request)
{
	if (TryReplayRecentManualDropResult(Inventory, Request))
	{
		return;
	}

	if (!Request.RequestId.IsValid() || !Inventory || !Request.EntryId.IsValid() ||
		!Request.ItemId.IsValid() || !Request.ExpectedSourcePlacement.IsValid() ||
		Request.StackCount <= 0)
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

	const TArray<FRpgInventoryEntryView> Entries = Inventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry = Entries.FindByPredicate(
		[&Request](const FRpgInventoryEntryView& Candidate)
		{
			return Candidate.EntryId == Request.EntryId;
		});
	URpgInventoryItemInstance* Item = Entry ? Entry->Instance.Get() : nullptr;
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

	if (Entry->Placement != Request.ExpectedSourcePlacement)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.StackCount);
		return;
	}

	const int32 AvailableCount = Inventory->GetItemStackCount(Item);
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

	const ERpgInventoryManualDropPolicy DropPolicy = GetManualDropPolicy(Item);
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

	if (DropPolicy == ERpgInventoryManualDropPolicy::Confirm && !Request.bConfirmed)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::RequiresConfirmation,
			Item,
			Request.StackCount);
		return;
	}

	const bool bDropAsStackTemplate = IsStackableItem(Item);
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
	DropPlanIntent.Quantity = Request.StackCount;
	const FRpgInventoryMutationResult DropPlan =
		Inventory->PlanDropItem(DropPlanIntent);
	if (!DropPlan.IsSuccess() ||
		DropPlan.AppliedQuantity != Request.StackCount)
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			GetFeedbackForMutationResult(DropPlan.Code),
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

		switch (GetManualDropPolicy(PlannedItem))
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

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const bool bDropsWholePlayerEntry =
		Inventory == PlayerInventory && Request.StackCount == AvailableCount;
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		bDropsWholePlayerEntry ? FindEquipmentLoadout() : nullptr;
	if (EquipmentLoadout &&
		!EquipmentLoadout->CanRemoveItemFromLoadout(Item))
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			Request.StackCount);
		return;
	}

	URpgInventoryManagerComponent* DropTargetInventory = nullptr;
	if (!TryTransferManualDrop(
			Inventory,
			Item,
			Request.StackCount,
			Request.RequestId,
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

	if (bDropsWholePlayerEntry)
	{
		const bool bClearedAssignments =
			ClearPlayerAssignmentsForItem(Item);
		ensureMsgf(
			bClearedAssignments,
			TEXT("Validated player assignment clear failed after dropping item %s (%s)."),
			*GetNameSafe(Item),
			*Request.ItemId.ToString());
		if (IsPlayerEquipmentPlacement(
				Request.ExpectedSourcePlacement))
		{
			SyncEquipmentLoadoutFromGearSlots();
			SyncActiveHandsFromCarrySlots();
		}
	}

	SendAndCacheManualDropFeedback(
		Inventory,
		Request,
		ERpgInventoryActionFeedbackResult::Success,
		Item,
		Request.StackCount,
		DropTargetInventory);
}

bool URpgInventoryUiActionComponent::TryDepositMaterialStackToBase(
	URpgInventoryManagerComponent* Inventory,
	URpgBaseStorageComponent* BaseStorage,
	FRpgInventoryItemId ItemId,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 AvailableCount,
	int32 CountToStore) const
{
	if (!Inventory || !BaseStorage || !ItemId.IsValid() ||
		!ItemDefinition || AvailableCount <= 0 || CountToStore <= 0 ||
		CountToStore > AvailableCount)
	{
		return false;
	}

	const TArray<FRpgInventoryEntryView> EntriesBefore =
		Inventory->GetAllEntries();
	const FRpgInventoryEntryView* EntryBefore =
		EntriesBefore.FindByPredicate(
			[&ItemId](const FRpgInventoryEntryView& Candidate)
			{
				return Candidate.ItemId == ItemId;
			});
	const FRpgInventoryGraphSaveData GraphBefore =
		Inventory->ExportInventoryGraph();
	const FRpgInventorySavedItem* SavedItemBefore =
		GraphBefore.Items.FindByPredicate(
			[&ItemId](const FRpgInventorySavedItem& Candidate)
			{
				return Candidate.ItemId == ItemId;
			});
	if (!EntryBefore || !EntryBefore->Instance ||
		EntryBefore->StackCount != AvailableCount ||
		GraphBefore.Items.Num() != EntriesBefore.Num() ||
		!SavedItemBefore)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Error,
			TEXT("Material deposit skipped for %s because its exact pre-consume graph could not be exported."),
			*ItemId.ToString());
		return false;
	}

	const FRpgInventorySavedItem SavedItemSnapshot = *SavedItemBefore;
	URpgInventoryItemInstance* const InstanceBefore =
		EntryBefore->Instance;
	const FGuid EntryIdBefore = EntryBefore->EntryId;
	const FRpgInventoryGridPlacement PlacementBefore =
		EntryBefore->Placement;
	auto RestoreExactInventoryGraph = [&]()
	{
		FRpgInventoryMutationResult RollbackResult;
		if (!Inventory->ImportInventoryGraph(
				GraphBefore,
				RollbackResult) ||
			!RollbackResult.IsSuccess())
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Material-deposit exact graph rollback failed for %s after storing %d x %s was rejected (code %d)."),
				*ItemId.ToString(),
				CountToStore,
				*GetNameSafe(ItemDefinition),
				static_cast<int32>(RollbackResult.Code));
			return false;
		}

		const TArray<FRpgInventoryEntryView> RestoredEntries =
			Inventory->GetAllEntries();
		const FRpgInventoryEntryView* RestoredEntry =
			RestoredEntries.FindByPredicate(
				[&ItemId](const FRpgInventoryEntryView& Candidate)
				{
					return Candidate.ItemId == ItemId;
				});
		const FRpgInventoryGraphSaveData RestoredGraph =
			Inventory->ExportInventoryGraph();
		const FRpgInventorySavedItem* RestoredSavedItem =
			RestoredGraph.Items.FindByPredicate(
				[&ItemId](const FRpgInventorySavedItem& Candidate)
				{
					return Candidate.ItemId == ItemId;
				});
		const bool bExactRollback =
			RestoredEntry &&
			RestoredEntry->Instance == InstanceBefore &&
			RestoredEntry->EntryId == EntryIdBefore &&
			RestoredEntry->StackCount == AvailableCount &&
			RestoredEntry->Placement == PlacementBefore &&
			RestoredSavedItem &&
			RestoredSavedItem->ItemDefinition ==
				SavedItemSnapshot.ItemDefinition &&
			RestoredSavedItem->Container ==
				SavedItemSnapshot.Container &&
			RestoredSavedItem->Placement ==
				SavedItemSnapshot.Placement &&
			HasEquivalentRuntimeState(
				RestoredSavedItem->RuntimeState,
				SavedItemSnapshot.RuntimeState);
		if (!bExactRollback)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Material-deposit rollback for %s completed with item identity, entry identity, placement, or runtime-state drift."),
				*ItemId.ToString());
		}
		return bExactRollback;
	};

	// Keep one concrete unit alive for a whole-stack deposit so a failed
	// storage write can restore the same UObject and replicated EntryId.
	const int32 CountToConsumeBeforeStore =
		CountToStore == AvailableCount
			? CountToStore - 1
			: CountToStore;
	if (CountToConsumeBeforeStore > 0)
	{
		const FRpgInventoryMutationResult ConsumeResult =
			Inventory->ConsumeItemById(
				ItemId,
				CountToConsumeBeforeStore);
		if (!ConsumeResult.IsSuccess() ||
			ConsumeResult.AppliedQuantity !=
				CountToConsumeBeforeStore)
		{
			return false;
		}
	}

	if (!BaseStorage->StoreResource(
			ItemDefinition,
			CountToStore))
	{
		RestoreExactInventoryGraph();
		return false;
	}

	const int32 RemainingConsumeCount =
		CountToStore - CountToConsumeBeforeStore;
	if (RemainingConsumeCount <= 0)
	{
		return true;
	}

	const FRpgInventoryMutationResult FinalConsumeResult =
		Inventory->ConsumeItemById(
			ItemId,
			RemainingConsumeCount);
	if (FinalConsumeResult.IsSuccess() &&
		FinalConsumeResult.AppliedQuantity ==
			RemainingConsumeCount)
	{
		return true;
	}

	const bool bStorageRolledBack =
		BaseStorage->WithdrawResource(
			ItemDefinition,
			CountToStore);
	const bool bInventoryRolledBack =
		RestoreExactInventoryGraph();
	if (!bStorageRolledBack || !bInventoryRolledBack)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Error,
			TEXT("Material-deposit finalization rollback failed for %s (%d x %s, Storage=%s, Inventory=%s)."),
			*ItemId.ToString(),
			CountToStore,
			*GetNameSafe(ItemDefinition),
			bStorageRolledBack ? TEXT("restored") : TEXT("failed"),
			bInventoryRolledBack ? TEXT("restored") : TEXT("failed"));
	}
	return false;
}

void URpgInventoryUiActionComponent::RequestDepositAllMaterialsToBase_Implementation(URpgBaseStorageStationComponent* Station)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage)
	{
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries = PlayerInventory->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		URpgInventoryItemInstance* Item = Entry.Instance;
		if (!Item || Entry.StackCount <= 0 || !IsMaterialItem(Item))
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
		if (!Station->AllowsResourceDefinition(ItemDefinition))
		{
			continue;
		}

		const int32 CountToDeposit = FMath::Min(Entry.StackCount, BaseStorage->GetFreeResourceCapacity(ItemDefinition));
		if (CountToDeposit <= 0 || !BaseStorage->CanStoreResource(ItemDefinition, CountToDeposit))
		{
			continue;
		}

		TryDepositMaterialStackToBase(
			PlayerInventory,
			BaseStorage,
			Entry.ItemId,
			ItemDefinition,
			Entry.StackCount,
			CountToDeposit);
	}
}

void URpgInventoryUiActionComponent::RequestDepositMaterialStackToBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage || !Item || !IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount = PlayerInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	const int32 TransferCount = FMath::Min(AvailableCount, RequestedCount);
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (TransferCount <= 0 || !BaseStorage->CanStoreResource(ItemDefinition, TransferCount))
	{
		return;
	}

	TryDepositMaterialStackToBase(
		PlayerInventory,
		BaseStorage,
		Item->GetItemId(),
		ItemDefinition,
		AvailableCount,
		TransferCount);
}

void URpgInventoryUiActionComponent::RequestWithdrawResourceFromBase_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage || !ItemDefinition || StackCount <= 0)
	{
		return;
	}

	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (BaseStorage->GetResourceCount(ItemDefinition) < StackCount || !PlayerInventory->CanAddItemDefinition(ItemDefinition, StackCount))
	{
		return;
	}

	if (BaseStorage->WithdrawResource(ItemDefinition, StackCount))
	{
		if (!PlayerInventory->GrantItemDefinition(
				ItemDefinition,
				StackCount) &&
			!BaseStorage->StoreResource(
				ItemDefinition,
				StackCount))
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Resource-withdraw rollback failed after the player inventory rejected %d x %s."),
				StackCount,
				*GetNameSafe(ItemDefinition));
		}
	}
}

void URpgInventoryUiActionComponent::RequestStoreItemInstanceInBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory = Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || Station->GetStationMode() != ERpgBaseStorageStationMode::Terminal || !PlayerInventory || !ArmoryInventory || !Item || IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount = PlayerInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 || RequestedCount != AvailableCount)
	{
		return;
	}

	FRpgInventoryQuickTransferRequest TransferRequest;
	TransferRequest.RequestId = FGuid::NewGuid();
	TransferRequest.ItemId = Item->GetItemId();
	TransferRequest.StackCount = AvailableCount;
	RequestQuickTransferItem_Implementation(PlayerInventory, ArmoryInventory, MoveTemp(TransferRequest));
}

void URpgInventoryUiActionComponent::RequestTakeItemInstanceFromBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory = Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || Station->GetStationMode() != ERpgBaseStorageStationMode::Terminal || !PlayerInventory || !ArmoryInventory || !Item)
	{
		return;
	}

	const int32 AvailableCount = ArmoryInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 || RequestedCount != AvailableCount)
	{
		return;
	}

	FRpgInventoryQuickTransferRequest TransferRequest;
	TransferRequest.RequestId = FGuid::NewGuid();
	TransferRequest.ItemId = Item->GetItemId();
	TransferRequest.StackCount = AvailableCount;
	RequestQuickTransferItem_Implementation(ArmoryInventory, PlayerInventory, MoveTemp(TransferRequest));
}

void URpgInventoryUiActionComponent::RequestInstallBaseStorageUpgrade_Implementation(URpgBaseStorageStationComponent* Station, URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!Station)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: Station is null. Owner=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!CanAccessBaseStorageStation(Station))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: station access denied. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!PlayerInventory)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: player inventory missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!BaseStorage)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: base storage missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!UpgradeDefinition)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: upgrade definition is null. Owner=%s Station=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station));
		return;
	}

	if (!Station->CanInstallUpgrade(UpgradeDefinition))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: station cannot install upgrade, maybe already installed or station tags do not match. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	const ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder = Station->GetUpgradeCostConsumeOrder();
	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Install base storage upgrade requested: Owner=%s Station=%s Upgrade=%s CostCount=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		UpgradeDefinition->Costs.Num());

	for (const FRpgBaseStorageUpgradeCost& Cost : UpgradeDefinition->Costs)
	{
		if (!Cost.ItemDefinition)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: empty cost item definition. Upgrade=%s"),
				*GetNameSafe(UpgradeDefinition));
			return;
		}

		if (Cost.Count <= 0)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: invalid cost count. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}

		if (!IsMaterialItemDefinition(Cost.ItemDefinition))
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: cost item is not marked as material. Upgrade=%s ItemDef=%s"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition));
			return;
		}

		const int32 AvailableCount = GetAvailableUpgradeCostCount(PlayerInventory, BaseStorage, Cost.ItemDefinition, ConsumeOrder);
		if (AvailableCount < Cost.Count)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: not enough resources. Upgrade=%s ItemDef=%s Available=%d Required=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				AvailableCount,
				Cost.Count);
			return;
		}
	}

	for (const FRpgBaseStorageUpgradeCost& Cost : UpgradeDefinition->Costs)
	{
		if (!ConsumeUpgradeCost(PlayerInventory, BaseStorage, Cost.ItemDefinition, Cost.Count, ConsumeOrder))
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: cost consume failed after validation. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}
	}

	const bool bInstalled = Station->InstallUpgrade(UpgradeDefinition);
	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Install base storage upgrade result: Owner=%s Station=%s Upgrade=%s Installed=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		bInstalled ? TEXT("true") : TEXT("false"));
}

void URpgInventoryUiActionComponent::RequestApplyBaseResourceSort_Implementation(URpgBaseStorageStationComponent* Station, ERpgInventorySortMode SortMode)
{
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage)
	{
		return;
	}

	BaseStorage->ApplyResourceSort(SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveBaseResourceEntry_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage || !Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	BaseStorage->MoveResourceEntry(ItemDefinition, TargetIndex);
}

void URpgInventoryUiActionComponent::RequestPlaceBaseBuildable_Implementation(ARpgBaseCampActor* BaseCamp, URpgBaseBuildableDefinition* BuildableDefinition, FTransform BuildTransform, bool bAutoContributeFromBase)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (!BaseCamp || !BuildableDefinition || !RequestingActor)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: missing input. Owner=%s BaseCamp=%s Buildable=%s RequestingActor=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(RequestingActor));
		return;
	}

	if (!BaseCamp->CanPlaceBuildableAtTransform(BuildableDefinition, BuildTransform, RequestingActor))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: placement validation denied. Owner=%s BaseCamp=%s Buildable=%s BuildActorClass=%s BaseDist=%.0f BuilderDist=%.0f BuildLocation=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(BuildableDefinition->BuildActorClass),
			FVector::Dist(BaseCamp->GetActorLocation(), BuildTransform.GetLocation()),
			FVector::Dist(RequestingActor->GetActorLocation(), BuildTransform.GetLocation()),
			*BuildTransform.GetLocation().ToCompactString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: world missing. Owner=%s BaseCamp=%s Buildable=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition));
		return;
	}

	TSubclassOf<ARpgBaseConstructionSiteActor> ConstructionSiteClass = BuildableDefinition->ConstructionSiteActorClass;
	if (!ConstructionSiteClass)
	{
		ConstructionSiteClass = ARpgBaseConstructionSiteActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = BaseCamp;
	SpawnParams.Instigator = Cast<APawn>(RequestingActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARpgBaseConstructionSiteActor* ConstructionSite = World->SpawnActor<ARpgBaseConstructionSiteActor>(ConstructionSiteClass, BuildTransform, SpawnParams);
	if (!ConstructionSite)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: construction site spawn failed. Owner=%s BaseCamp=%s Buildable=%s SiteClass=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(ConstructionSiteClass));
		return;
	}

	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Place buildable succeeded: Site=%s BaseCamp=%s Buildable=%s BuildActorClass=%s AutoContributeFromBase=%s Location=%s"),
		*GetNameSafe(ConstructionSite),
		*GetNameSafe(BaseCamp),
		*GetNameSafe(BuildableDefinition),
		*GetNameSafe(BuildableDefinition->BuildActorClass),
		bAutoContributeFromBase ? TEXT("true") : TEXT("false"),
		*BuildTransform.GetLocation().ToCompactString());

	ConstructionSite->InitializeConstructionSite(BaseCamp, BuildableDefinition);
	if (bAutoContributeFromBase && IsValid(ConstructionSite) && !ConstructionSite->IsConstructionComplete())
	{
		ConstructionSite->ContributeAllResources(RequestingActor, true);
	}
}

void URpgInventoryUiActionComponent::RequestContributeAllToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, bool bAllowBaseStorage)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (ConstructionSite && RequestingActor)
	{
		ConstructionSite->ContributeAllResources(RequestingActor, bAllowBaseStorage);
	}
}

void URpgInventoryUiActionComponent::RequestContributeMaterialToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, bool bAllowBaseStorage)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (ConstructionSite && RequestingActor && ItemDefinition && StackCount > 0)
	{
		ConstructionSite->ContributeMaterial(RequestingActor, ItemDefinition, StackCount, bAllowBaseStorage);
	}
}

void URpgInventoryUiActionComponent::RequestCraftRecipe_Implementation(URpgCraftingStationComponent* CraftingStation, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (!CraftingStation || !RecipeDefinition || !RequestingActor || !CraftingStation->CanCraftRecipeQuantity(RequestingActor, RecipeDefinition, Quantity))
	{
		return;
	}

	CraftingStation->QueueCraftRecipe(RequestingActor, RecipeDefinition, Quantity);
}

void URpgInventoryUiActionComponent::RequestCancelCraftJob_Implementation(URpgCraftingStationComponent* CraftingStation, FGuid JobId)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->CancelCraftJob(RequestingActor, JobId);
	}
}

void URpgInventoryUiActionComponent::RequestPauseCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->PauseCraftingStation(RequestingActor);
	}
}

void URpgInventoryUiActionComponent::RequestResumeCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->ResumeCraftingStation(RequestingActor);
	}
}

void URpgInventoryUiActionComponent::RequestSetCraftingOutputAutoDepositEnabled_Implementation(URpgCraftingStationComponent* CraftingStation, bool bEnabled)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->SetCraftingOutputAutoDepositEnabled(RequestingActor, bEnabled);
	}
}

bool URpgInventoryUiActionComponent::CanAccessInventory(URpgInventoryManagerComponent* Inventory) const
{
	if (Inventory == nullptr)
	{
		return false;
	}

	if (Inventory == FindPlayerInventory())
	{
		return true;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();

	TArray<const URpgCraftingStationComponent*> CraftingStations;
	GatherCraftingStationsForOutputInventory(Inventory, CraftingStations);
	if (!CraftingStations.IsEmpty())
	{
		// A shared/misconfigured output is accessible only when every live station claimant authorizes the actor.
		// This makes authorization independent of component enumeration order and fails closed on ambiguity.
		for (const URpgCraftingStationComponent* CraftingStation : CraftingStations)
		{
			if (!CraftingStation->CanActorAccess(RequestingActor))
			{
				return false;
			}
		}
		return true;
	}

	const URpgInventoryManagerComponent* BaseArmoryInventory = Inventory;
	const AActor* InventoryOwner = Inventory->GetOwner();
	const URpgBaseStorageStationComponent* Station = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgBaseStorageStationComponent>() : nullptr;
	if (Station && Station->GetArmoryInventory() == BaseArmoryInventory)
	{
		return Station->CanActorAccess(RequestingActor);
	}

	const URpgInventoryContainerComponent* Container = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
	if (!Container)
	{
		return false;
	}

	return Container->CanActorAccess(RequestingActor);
}

URpgInventoryManagerComponent* URpgInventoryUiActionComponent::FindPlayerInventory() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetInventoryManagerComponent();
		}
	}

	return nullptr;
}

URpgEquipmentLoadoutComponent* URpgInventoryUiActionComponent::FindEquipmentLoadout() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetEquipmentLoadoutComponent();
	}

	return nullptr;
}

URpgPlayerInventoryLayoutComponent* URpgInventoryUiActionComponent::FindPlayerInventoryLayout() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetPlayerInventoryLayoutComponent();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<URpgPlayerInventoryLayoutComponent>() : nullptr;
}

URpgActionBarComponent* URpgInventoryUiActionComponent::FindActionBar() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetActionBarComponent();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<URpgActionBarComponent>() : nullptr;
}

URpgAbilitySystemComponent* URpgInventoryUiActionComponent::FindPlayerAbilitySystem() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetRpgAbilitySystemComponent();
		}
	}

	return nullptr;
}

bool URpgInventoryUiActionComponent::FindQuickTransferDestination(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	FRpgInventoryContainerHandle& OutTargetContainer,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	OutTargetContainer = FRpgInventoryContainerHandle();
	OutTargetPlacement = FRpgInventoryGridPlacement();
	if (!SourceInventory || !TargetInventory ||
		!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory) ||
		!IsUiTransferDirectionAllowed(SourceInventory, TargetInventory))
	{
		return false;
	}

	URpgInventoryItemInstance* Item = SourceInventory->FindItemById(Request.ItemId);
	const int32 AvailableCount = Item ? SourceInventory->GetItemStackCount(Item) : 0;
	const int32 RequestedCount = Request.StackCount <= 0 ? AvailableCount : Request.StackCount;
	if (!Item || AvailableCount <= 0 || RequestedCount <= 0 || RequestedCount > AvailableCount ||
		(SourceInventory == TargetInventory && RequestedCount != AvailableCount))
	{
		return false;
	}

	TArray<FRpgInventoryContainerHandle> CandidateTargets;
	if (!Request.PreferredTargetContainers.IsEmpty())
	{
		for (const FRpgInventoryContainerHandle& Candidate : Request.PreferredTargetContainers)
		{
			if (Candidate.IsValid())
			{
				CandidateTargets.AddUnique(Candidate);
			}
		}
	}
	else
	{
		BuildDefaultQuickTransferTargets(SourceInventory, TargetInventory, Item, CandidateTargets);
	}

	const URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	for (const FRpgInventoryContainerHandle& CandidateTarget : CandidateTargets)
	{
		// Quick transfer into the player graph is content-only. Gear/Carry mutations remain explicit intents so a
		// forged preferred handle cannot silently equip or activate an item.
		if (TargetInventory == PlayerInventory)
		{
			const bool bIsContentTarget = InventoryLayout && InventoryLayout->GetSlotGroups().ContainsByPredicate(
				[&CandidateTarget, Item](const FRpgInventorySlotGroupView& Group)
				{
					return Group.ContainerHandle == CandidateTarget &&
						Group.GroupKind == ERpgInventorySlotGroupKind::Content &&
						Group.Rule.AllowsItem(Item);
				});
			if (!bIsContentTarget)
			{
				continue;
			}
		}

		FRpgInventoryGridPlacement CandidatePlacement;
		if (TryFindTransferPlacementInContainer(
			SourceInventory,
			TargetInventory,
			Item,
			RequestedCount,
			CandidateTarget,
			CandidatePlacement))
		{
			OutTargetContainer = CandidateTarget;
			OutTargetPlacement = CandidatePlacement;
			return true;
		}
	}

	return false;
}

void URpgInventoryUiActionComponent::BuildDefaultQuickTransferTargets(
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

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (TargetInventory != PlayerInventory || !InventoryLayout)
	{
		const FRpgInventoryContainerHandle DefaultTarget = FRpgInventoryContainerHandle::MakeRoot(TargetInventory->GetDefaultContainerId());
		if (DefaultTarget.IsValid())
		{
			OutTargets.Add(DefaultTarget);
		}
		return;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = InventoryLayout->GetSlotGroups();
	auto AddGroups = [&Groups, Item, &OutTargets](FName ContainerId, FName ProviderSlotName)
	{
		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			const bool bMatchesContainer = !ContainerId.IsNone() && Group.ContainerId == ContainerId;
			const bool bMatchesProvider = !ProviderSlotName.IsNone() && Group.SourceEquipmentSlotName == ProviderSlotName;
			if (Group.GroupKind == ERpgInventorySlotGroupKind::Content &&
				(bMatchesContainer || bMatchesProvider) && Group.Rule.AllowsItem(Item))
			{
				OutTargets.AddUnique(Group.ContainerHandle);
			}
		}
	};

	if (SourceInventory != TargetInventory)
	{
		// External loot/storage enters quick-access content first, then the equipped backpack, followed by any
		// additional designer-defined content groups in their stable layout order.
		AddGroups(URpgPlayerInventoryLayoutComponent::PocketsGroupId, NAME_None);
		AddGroups(NAME_None, TEXT("Belt"));
		AddGroups(NAME_None, TEXT("Pouch"));
		AddGroups(NAME_None, TEXT("Backpack"));
		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			if (Group.GroupKind == ERpgInventorySlotGroupKind::Content && Group.Rule.AllowsItem(Item))
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

	const FRpgInventorySlotGroupView* SourceGroup = Groups.FindByPredicate(
		[&SourcePlacement](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == SourcePlacement.GetContainerHandle();
		});
	const bool bSourceIsBackpackContent = SourceGroup && SourceGroup->SourceEquipmentSlotName == FName(TEXT("Backpack"));
	const bool bSourceIsQuickContent = SourceGroup &&
		(SourceGroup->ContainerId == URpgPlayerInventoryLayoutComponent::PocketsGroupId ||
		 SourceGroup->SourceEquipmentSlotName == FName(TEXT("Belt")) ||
		 SourceGroup->SourceEquipmentSlotName == FName(TEXT("Pouch")));

	if (bSourceIsBackpackContent)
	{
		AddGroups(URpgPlayerInventoryLayoutComponent::PocketsGroupId, NAME_None);
		AddGroups(NAME_None, TEXT("Belt"));
		AddGroups(NAME_None, TEXT("Pouch"));
	}
	else if (bSourceIsQuickContent)
	{
		AddGroups(NAME_None, TEXT("Backpack"));
	}
	else
	{
		// Gear, Carry, and any other player roots quick-transfer into the backpack without activating equipment.
		AddGroups(NAME_None, TEXT("Backpack"));
	}
}

bool URpgInventoryUiActionComponent::TryFindTransferPlacementInContainer(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FRpgInventoryContainerHandle& TargetContainer,
	FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!SourceInventory || !TargetInventory || !Item || StackCount <= 0 || !TargetContainer.IsValid())
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0 ||
		StackCount > AvailableCount ||
		!TryGetInventoryEntrySnapshot(
			SourceInventory,
			Item->GetItemId(),
			SourceEntry))
	{
		return false;
	}
	const FRpgInventoryGridPlacement& SourcePlacement =
		SourceEntry.Placement;

	FRpgInventoryGridSize GridSize;
	if (!TargetInventory->GetGridSizeForContainerHandle(TargetContainer, GridSize))
	{
		return false;
	}

	const FRpgInventoryGridSize Footprint = GetUiActionFootprint(Item);
	const int32 RotationCount = CanUiActionRotate(Item) ? 2 : 1;
	if (SourceInventory == TargetInventory)
	{
		if (StackCount != AvailableCount || SourcePlacement.GetContainerHandle() == TargetContainer)
		{
			return false;
		}

		for (int32 RotationIndex = 0; RotationIndex < RotationCount; ++RotationIndex)
		{
			FRpgInventoryGridPlacement Candidate;
			Candidate.SetContainerHandle(TargetContainer);
			Candidate.Width = Footprint.Width;
			Candidate.Height = Footprint.Height;
			Candidate.bRotated = RotationIndex == 1;
			const FRpgInventoryGridSize OccupiedSize = Candidate.GetOccupiedSize();
			for (int32 Y = 0; Y <= GridSize.Height - OccupiedSize.Height; ++Y)
			{
				for (int32 X = 0; X <= GridSize.Width - OccupiedSize.Width; ++X)
				{
					Candidate.X = X;
					Candidate.Y = Y;
					FRpgInventoryMoveIntent MoveIntent;
					MoveIntent.ItemId = Item->GetItemId();
					MoveIntent.ExpectedEntryId =
						SourceEntry.EntryId;
					MoveIntent.ExpectedSourcePlacement =
						SourcePlacement;
					MoveIntent.ExpectedQuantity =
						SourceEntry.StackCount;
					MoveIntent.TargetPlacement = Candidate;
					const FRpgInventoryMutationResult PlanResult =
						SourceInventory->PlanMoveItem(
							MoveIntent);
					const bool bWouldSwap = PlanResult.Deltas.ContainsByPredicate(
						[Item](const FRpgInventoryMutationDelta& Delta)
						{
							return Delta.ItemId != Item->GetItemId() && Delta.Kind == ERpgInventoryMutationDeltaKind::Moved;
						});
					if (PlanResult.Code == ERpgInventoryMutationResultCode::Success &&
						PlanResult.AppliedQuantity == AvailableCount && !bWouldSwap)
					{
						OutPlacement = Candidate;
						return true;
					}
				}
			}
		}
		return false;
	}

	if (TargetInventory->FindItemById(Item->GetItemId()))
	{
		return false;
	}

	// Account for the complete source subtree when probing a deeper item-owned target. The target inventory cannot
	// derive this relative depth until reconstruction, so the quick-transfer scan validates it explicitly.
	uint8 DeepestRelativeDepth = 0;
	const TArray<FRpgInventoryEntryView> SourceEntries = SourceInventory->GetAllEntries();
	for (const FRpgInventoryEntryView& CandidateEntry : SourceEntries)
	{
		FRpgInventoryContainerHandle Ancestor = CandidateEntry.Placement.GetContainerHandle();
		for (int32 Guard = 0; Guard <= SourceEntries.Num() && Ancestor.IsItemOwned(); ++Guard)
		{
			if (Ancestor.ItemOwnerId == Item->GetItemId())
			{
				const uint8 CandidateDepth = CandidateEntry.Placement.GetContainerHandle().Depth;
				const uint8 SourceDepth = SourcePlacement.GetContainerHandle().Depth;
				DeepestRelativeDepth = FMath::Max<uint8>(DeepestRelativeDepth, CandidateDepth > SourceDepth ? CandidateDepth - SourceDepth : 1);
				break;
			}

			const FRpgInventoryEntryView* ParentEntry = SourceEntries.FindByPredicate(
				[&Ancestor](const FRpgInventoryEntryView& Entry)
				{
					return Entry.ItemId == Ancestor.ItemOwnerId;
				});
			Ancestor = ParentEntry ? ParentEntry->Placement.GetContainerHandle() : FRpgInventoryContainerHandle();
		}
	}
	if (TargetContainer.Depth + DeepestRelativeDepth > RpgInventoryMaxItemOwnedDepth)
	{
		return false;
	}

	int32 RemainingCount = StackCount;
	for (const FRpgInventoryEntryView& TargetEntry : TargetInventory->GetAllEntries())
	{
		if (!TargetEntry.Instance || TargetEntry.Placement.GetContainerHandle() != TargetContainer ||
			!Item->IsStackCompatibleWith(TargetEntry.Instance))
		{
			continue;
		}

		RemainingCount -= FMath::Min(RemainingCount, TargetInventory->GetFreeStackCapacity(TargetEntry.Instance));
		if (RemainingCount <= 0)
		{
			// No exact placement is needed: the authoritative cross-inventory planner will merge in stable entry order.
			return true;
		}
	}

	for (int32 RotationIndex = 0; RotationIndex < RotationCount; ++RotationIndex)
	{
		FRpgInventoryGridPlacement Candidate;
		Candidate.SetContainerHandle(TargetContainer);
		Candidate.Width = Footprint.Width;
		Candidate.Height = Footprint.Height;
		Candidate.bRotated = RotationIndex == 1;
		const FRpgInventoryGridSize OccupiedSize = Candidate.GetOccupiedSize();
		for (int32 Y = 0; Y <= GridSize.Height - OccupiedSize.Height; ++Y)
		{
			for (int32 X = 0; X <= GridSize.Width - OccupiedSize.Width; ++X)
			{
				Candidate.X = X;
				Candidate.Y = Y;
				if (TargetInventory->CanReceiveTransferredItemInstanceToPlacement(
					Item,
					RemainingCount,
					Candidate))
				{
					OutPlacement = Candidate;
					return true;
				}
			}
		}
	}

	return false;
}

bool URpgInventoryUiActionComponent::CanTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount) const
{
	if (!SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !Item)
	{
		return false;
	}

	if (!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory) ||
		!IsUiTransferDirectionAllowed(SourceInventory, TargetInventory))
	{
		return false;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (RequestedCount <= 0 || RequestedCount > AvailableCount)
	{
		return false;
	}

	return CanTargetAcceptTransferredStack(TargetInventory, Item, RequestedCount, RequestedCount >= AvailableCount);
}

bool URpgInventoryUiActionComponent::CanTransferItemStackToPlacement(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, FRpgInventoryGridPlacement TargetPlacement) const
{
	if (!SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !Item || !TargetPlacement.IsValid())
	{
		return false;
	}

	if (!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory) ||
		!IsUiTransferDirectionAllowed(SourceInventory, TargetInventory))
	{
		return false;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (RequestedCount <= 0 || RequestedCount > AvailableCount)
	{
		return false;
	}

	FRpgInventoryGridPlacement NormalizedTargetPlacement;
	URpgInventoryItemInstance* TargetItem = TargetInventory->GetSingleItemOverlappingPlacementForItem(Item, TargetPlacement, NormalizedTargetPlacement);
	if (TargetItem)
	{
		return Item->IsStackCompatibleWith(TargetItem) &&
			TargetInventory->GetFreeStackCapacity(TargetItem) >= RequestedCount;
	}

	if (RequestedCount >= AvailableCount &&
		TargetInventory->CanReceiveTransferredItemInstanceToPlacement(
			Item,
			AvailableCount,
			TargetPlacement))
	{
		return true;
	}

	if (TargetInventory->CanAddItemDefinitionToPlacement(Item->GetItemDef(), RequestedCount, TargetPlacement))
	{
		return true;
	}

	// The canonical cross-inventory transaction does not support reciprocal swaps.
	return false;
}

bool URpgInventoryUiActionComponent::CanSplitItemStack(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 SplitCount, FRpgInventoryGridPlacement TargetPlacement, int32& OutSplitCount, FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	OutSplitCount = 0;
	OutTargetPlacement = FRpgInventoryGridPlacement();

	if (!Inventory || !Item || !CanAccessInventory(Inventory) || !IsStackableItem(Item))
	{
		return false;
	}

	const int32 AvailableCount = Inventory->GetItemStackCount(Item);
	if (AvailableCount <= 1)
	{
		return false;
	}

	const int32 RequestedSplitCount = SplitCount <= 0 ? AvailableCount / 2 : SplitCount;
	if (RequestedSplitCount <= 0 || RequestedSplitCount >= AvailableCount)
	{
		return false;
	}

	FRpgInventoryGridPlacement ResolvedTargetPlacement = TargetPlacement;
	if (!ResolvedTargetPlacement.IsValid() && !FindFirstEmptyInventoryPlacement(Inventory, Item->GetItemDef(), ResolvedTargetPlacement))
	{
		return false;
	}

	if (!ResolvedTargetPlacement.IsValid() || Inventory->GetItemAtContainerCell(ResolvedTargetPlacement.GetContainerHandle(), ResolvedTargetPlacement.X, ResolvedTargetPlacement.Y) != nullptr)
	{
		return false;
	}

	if (!Inventory->CanAddItemDefinitionToPlacement(Item->GetItemDef(), RequestedSplitCount, ResolvedTargetPlacement))
	{
		return false;
	}

	OutSplitCount = RequestedSplitCount;
	OutTargetPlacement = ResolvedTargetPlacement;
	return true;
}

bool URpgInventoryUiActionComponent::FindFirstEmptyInventoryPlacement(URpgInventoryManagerComponent* Inventory, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!Inventory || !ItemDefinition)
	{
		return false;
	}

	if (URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory(); Inventory == PlayerInventory)
	{
		if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout())
		{
			for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
			{
				if (Group.GroupKind != ERpgInventorySlotGroupKind::Content || !Group.Rule.AllowsItemDefinition(ItemDefinition))
				{
					continue;
				}

				for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
				{
					for (int32 X = 0; X < Group.GridSize.Width; ++X)
					{
						FRpgInventoryGridPlacement Candidate;
						Candidate.SetContainerHandle(Group.ContainerHandle);
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
			}
		}

		return false;
	}

	const FName DefaultContainerId = Inventory->GetDefaultContainerId();
	const FRpgInventoryContainerHandle DefaultHandle = FRpgInventoryContainerHandle::MakeRoot(DefaultContainerId);
	FRpgInventoryGridSize GridSize;
	if (!Inventory->GetGridSizeForContainerHandle(DefaultHandle, GridSize))
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

bool URpgInventoryUiActionComponent::CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const
{
	if (!Station)
	{
		return false;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	return Station->CanActorAccess(RequestingActor);
}

bool URpgInventoryUiActionComponent::ClearPlayerAssignmentsForItem(URpgInventoryItemInstance* Item) const
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		return EquipmentLoadout->ClearItemFromAllEquipmentSlots(Item);
	}

	return true;
}

bool URpgInventoryUiActionComponent::TryBuildCurrentEquipmentIntent(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	ERpgInventoryEquipmentIntentOperation Operation,
	ERpgEquipmentSlot TargetEquipmentSlot,
	FRpgInventoryEquipmentIntent& OutIntent) const
{
	OutIntent = FRpgInventoryEquipmentIntent();
	if (!Inventory || !Item ||
		!Inventory->ContainsItemInstance(Item))
	{
		return false;
	}

	FRpgInventoryEntryView Entry;
	if (!TryGetInventoryEntrySnapshot(
			Inventory,
			Item->GetItemId(),
			Entry) ||
		Entry.Instance != Item ||
		!Entry.EntryId.IsValid() ||
		!Entry.Placement.IsValid() ||
		Entry.StackCount <= 0)
	{
		return false;
	}

	OutIntent.EnsureRequestId();
	OutIntent.ItemId = Entry.ItemId;
	OutIntent.ExpectedEntryId = Entry.EntryId;
	OutIntent.ExpectedSourcePlacement = Entry.Placement;
	OutIntent.ExpectedQuantity = Entry.StackCount;
	OutIntent.Operation = Operation;
	OutIntent.TargetEquipmentSlot = TargetEquipmentSlot;
	return true;
}

bool URpgInventoryUiActionComponent::TryAssignItemToDefaultEquipmentDestination(
	URpgInventoryItemInstance* Item)
{
	if (!Item)
	{
		return false;
	}

	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	if (!EquipmentLoadout)
	{
		return false;
	}

	const URpgInventoryFragment_ItemContainer* SlotContainerProvider = Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>();
	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>();
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;

	if (SlotContainerProvider)
	{
		if (EquipmentCDO)
		{
			const ERpgEquipmentSlot DefaultSlot = EquipmentCDO->GetDefaultEquipSlot();
			if (URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(DefaultSlot) &&
				TryMoveItemToGearSlot(DefaultSlot, Item))
			{
				return true;
			}

			for (const ERpgEquipmentSlot AllowedSlot : EquipmentCDO->AllowedSlots)
			{
				if (URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(AllowedSlot) &&
					TryMoveItemToGearSlot(AllowedSlot, Item))
				{
					return true;
				}
			}
		}

		const ERpgEquipmentSlot FallbackSlots[] =
		{
			ERpgEquipmentSlot::Backpack,
			ERpgEquipmentSlot::Belt,
			ERpgEquipmentSlot::Pouch,
			ERpgEquipmentSlot::ResourceBag
		};

		for (const ERpgEquipmentSlot FallbackSlot : FallbackSlots)
		{
			if (TryMoveItemToGearSlot(FallbackSlot, Item))
			{
				return true;
			}
		}
	}

	if (!EquipmentCDO)
	{
		return false;
	}

	const ERpgEquipmentSlot DefaultSlot = EquipmentCDO->GetDefaultEquipSlot();
	if (FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(DefaultSlot))
	{
		return TryMoveAndActivateItemInCarry(
			Item,
			DefaultSlot);
	}

	if (FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(DefaultSlot))
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

bool URpgInventoryUiActionComponent::TryMoveAndActivateItemInCarry(
	URpgInventoryItemInstance* Item,
	ERpgEquipmentSlot PreferredHandSlot)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	if (!Item || !PlayerInventory ||
		!InventoryLayout || !EquipmentLoadout ||
		!PlayerInventory->ContainsItemInstance(Item))
	{
		return false;
	}

	FRpgInventoryEntryView OriginalEntry;
	if (!TryGetInventoryEntrySnapshot(
			PlayerInventory,
			Item->GetItemId(),
			OriginalEntry))
	{
		return false;
	}
	const FRpgInventoryGridPlacement OriginalPlacement =
		OriginalEntry.Placement;

	const FRpgEquipmentSelectionSaveData PreviousSelection =
		EquipmentLoadout->ExportEquipmentSelection();
	const TArray<FRpgInventorySlotGroupView> Groups = InventoryLayout->GetSlotGroups();

	auto TryActivateRole = [EquipmentLoadout, Item](const FGameplayTag& ActivationRole)
	{
		return ActivationRole == RpgGameplayTags::Equipment_Slot_MainHand
			? EquipmentLoadout->SetMainHandItemActive(Item)
			: ActivationRole == RpgGameplayTags::Equipment_Slot_OffHand &&
				EquipmentLoadout->SetOffHandItemActive(Item);
	};
	auto MatchesPreferredRole = [PreferredHandSlot](const FGameplayTag& ActivationRole)
	{
		return PreferredHandSlot == ERpgEquipmentSlot::None ||
			(PreferredHandSlot == ERpgEquipmentSlot::MainHand && ActivationRole == RpgGameplayTags::Equipment_Slot_MainHand) ||
			(PreferredHandSlot == ERpgEquipmentSlot::OffHand && ActivationRole == RpgGameplayTags::Equipment_Slot_OffHand);
	};

	// None prefers MainHand deterministically; an explicit preferred hand probes exactly one semantic role.
	TArray<FGameplayTag, TInlineAllocator<2>> RoleOrder;
	if (PreferredHandSlot == ERpgEquipmentSlot::OffHand)
	{
		RoleOrder.Add(RpgGameplayTags::Equipment_Slot_OffHand);
	}
	else
	{
		RoleOrder.Add(RpgGameplayTags::Equipment_Slot_MainHand);
		if (PreferredHandSlot == ERpgEquipmentSlot::None)
		{
			RoleOrder.Add(RpgGameplayTags::Equipment_Slot_OffHand);
		}
	}
	for (const FGameplayTag& DesiredRole : RoleOrder)
	{
		if (!MatchesPreferredRole(DesiredRole))
		{
			continue;
		}
		const ERpgEquipmentSlot RuntimeSlot = DesiredRole == RpgGameplayTags::Equipment_Slot_OffHand
			? ERpgEquipmentSlot::OffHand
			: ERpgEquipmentSlot::MainHand;
		if (!EquipmentLoadout->CanAssignItemToEquipmentSlot(RuntimeSlot, Item))
		{
			continue;
		}

		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Carry ||
				!Group.Rule.bCarrySlot ||
				Group.Rule.CarryActivationRole != DesiredRole ||
				!Group.Rule.AllowsItem(Item))
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					const FRpgInventorySlotAddress TargetAddress = Group.MakeAddress(X, Y);
					FRpgInventoryGridPlacement TargetPlacement;
					if (!InventoryLayout->CanItemUseSlotAddress(Item, TargetAddress) ||
						!InventoryLayout->ResolveSlotAddress(TargetAddress, TargetPlacement))
					{
						continue;
					}

					const bool bAlreadyAtTarget =
						OriginalPlacement.GetContainerHandle() == TargetPlacement.GetContainerHandle() &&
						OriginalPlacement.X == TargetPlacement.X &&
						OriginalPlacement.Y == TargetPlacement.Y;
					if (bAlreadyAtTarget)
					{
						return TryActivateRole(DesiredRole);
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

					if (TryActivateRole(DesiredRole))
					{
						return true;
					}

					// Activation was prevalidated and should be side-effect free on failure. Restore the exact physical state
					// through the same atomic swap planner before trying another compatible semantic carry role.
					FRpgInventoryEntryView CurrentEntry;
					if (!TryGetInventoryEntrySnapshot(
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
					RollbackIntent.ItemId =
						Item->GetItemId();
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
					if (!AreEquipmentSelectionsEquivalent(
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

bool URpgInventoryUiActionComponent::TryMoveItemToGearSlot(
	ERpgEquipmentSlot EquipmentSlot,
	URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!Item ||
		!PlayerInventory ||
		!InventoryLayout ||
		!FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(EquipmentSlot) ||
		FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(EquipmentSlot))
	{
		return false;
	}

	FRpgInventorySlotAddress TargetAddress;
	FRpgInventoryGridPlacement TargetPlacement;
	if (!URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, TargetAddress) ||
		!InventoryLayout->ResolveSlotAddress(TargetAddress, TargetPlacement) ||
		!InventoryLayout->CanItemUseSlotAddress(Item, TargetAddress) ||
		!CanMoveItemOutOfGearSlot(TargetAddress))
	{
		return false;
	}

	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
	{
		if (Entry.Instance != Item)
		{
			continue;
		}

		FRpgInventoryMoveIntent MoveIntent;
		MoveIntent.EnsureRequestId();
		MoveIntent.ItemId = Entry.ItemId;
		MoveIntent.ExpectedEntryId = Entry.EntryId;
		MoveIntent.ExpectedSourcePlacement = Entry.Placement;
		MoveIntent.ExpectedQuantity = Entry.StackCount;
		MoveIntent.TargetPlacement = TargetPlacement;
		if (!PlayerInventory->PlanEquipmentMove(MoveIntent).IsSuccess())
		{
			return false;
		}
		const FRpgInventoryMutationResult MoveResult =
			PlayerInventory->MoveEquipmentItem(MoveIntent);
		if (MoveResult.IsSuccess() &&
			MoveResult.AppliedQuantity == Entry.StackCount)
		{
			return true;
		}
	}

	return false;
}

bool URpgInventoryUiActionComponent::TryMoveItemToFirstCompatibleCarrySlot(
	URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!Item || !PlayerInventory ||
		!InventoryLayout ||
		PlayerInventory->GetItemStackCount(Item) <= 0)
	{
		return false;
	}

	FRpgInventorySlotAddress CurrentAddress;
	FRpgInventoryGridPlacement CurrentPlacement;
	if (PlayerInventory->GetItemPlacement(Item, CurrentPlacement) &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(CurrentPlacement, CurrentAddress) &&
		InventoryLayout->IsCarrySlotAddress(CurrentAddress) &&
		InventoryLayout->CanItemUseSlotAddress(Item, CurrentAddress))
	{
		return true;
	}

	FRpgInventoryEntryView SourceEntry;
	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
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

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (Group.GroupKind != ERpgInventorySlotGroupKind::Carry || !Group.Rule.bCarrySlot || !Group.Rule.AllowsItem(Item))
		{
			continue;
		}

		for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < Group.GridSize.Width; ++X)
			{
				FRpgInventoryGridPlacement TargetPlacement;
				TargetPlacement.SetContainerHandle(Group.ContainerHandle.IsValid()
					? Group.ContainerHandle
					: FRpgInventoryContainerHandle::MakeRoot(Group.ContainerId));
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

bool URpgInventoryUiActionComponent::CanMoveItemToFirstCompatibleContentSlot(
	URpgInventoryItemInstance* Item,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	OutTargetPlacement = FRpgInventoryGridPlacement();
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	const int32 StackCount = PlayerInventory && Item
		? PlayerInventory->GetItemStackCount(Item)
		: 0;
	if (!Item || !PlayerInventory || !InventoryLayout || StackCount <= 0)
	{
		return false;
	}

	FName DisappearingProviderSourceName = NAME_None;
	FRpgInventorySlotAddress SourceAddress;
	FRpgInventoryGridPlacement SourcePlacement;
	if (PlayerInventory->GetItemPlacement(Item, SourcePlacement) &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(SourcePlacement, SourceAddress) &&
		InventoryLayout->IsGearSlotAddress(SourceAddress))
	{
		ERpgEquipmentSlot SourceEquipmentSlot = ERpgEquipmentSlot::None;
		if (URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearGroupId(SourceAddress.ContainerId, SourceEquipmentSlot) &&
			URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(SourceEquipmentSlot))
		{
			switch (SourceEquipmentSlot)
			{
			case ERpgEquipmentSlot::Backpack:
				DisappearingProviderSourceName = FName(TEXT("Backpack"));
				break;
			case ERpgEquipmentSlot::Belt:
				DisappearingProviderSourceName = FName(TEXT("Belt"));
				break;
			case ERpgEquipmentSlot::Pouch:
				DisappearingProviderSourceName = FName(TEXT("Pouch"));
				break;
			case ERpgEquipmentSlot::ResourceBag:
				DisappearingProviderSourceName = FName(TEXT("ResourceBag"));
				break;
			default:
				break;
			}
		}
	}

	FRpgInventoryEntryView SourceEntry;
	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
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

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (Group.GroupKind != ERpgInventorySlotGroupKind::Content || !Group.Rule.AllowsItem(Item))
		{
			continue;
		}

		if (!DisappearingProviderSourceName.IsNone() && Group.bProvidedByEquipment && Group.SourceEquipmentSlotName == DisappearingProviderSourceName)
		{
			continue;
		}

		for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < Group.GridSize.Width; ++X)
			{
				FRpgInventoryGridPlacement TargetPlacement;
				TargetPlacement.SetContainerHandle(Group.ContainerHandle.IsValid()
					? Group.ContainerHandle
					: FRpgInventoryContainerHandle::MakeRoot(Group.ContainerId));
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
						MoveIntent).IsSuccess() &&
					PlayerInventory->CanReceiveTransferredItemInstanceToPlacementIgnoringItem(
						Item,
						StackCount,
						TargetPlacement,
						Item))
				{
					OutTargetPlacement = TargetPlacement;
					return true;
				}
			}
		}
	}

	return false;
}

bool URpgInventoryUiActionComponent::TryMoveItemToFirstCompatibleContentSlot(
	URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	FRpgInventoryGridPlacement TargetPlacement;
	if (!PlayerInventory ||
		!CanMoveItemToFirstCompatibleContentSlot(Item, TargetPlacement))
	{
		return false;
	}

	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
	{
		if (Entry.Instance == Item &&
			Entry.ItemId == Item->GetItemId() &&
			Entry.EntryId.IsValid())
		{
			FRpgInventoryMoveIntent MoveIntent;
			MoveIntent.EnsureRequestId();
			MoveIntent.ItemId = Entry.ItemId;
			MoveIntent.ExpectedEntryId = Entry.EntryId;
			MoveIntent.ExpectedSourcePlacement = Entry.Placement;
			MoveIntent.ExpectedQuantity = Entry.StackCount;
			MoveIntent.TargetPlacement = TargetPlacement;
			const FRpgInventoryMutationResult MoveResult =
				PlayerInventory->MoveEquipmentItem(MoveIntent);
			return MoveResult.IsSuccess() &&
				MoveResult.AppliedQuantity == Entry.StackCount;
		}
	}

	return false;
}

bool URpgInventoryUiActionComponent::CanMoveItemOutOfGearSlot(const FRpgInventorySlotAddress& SourceAddress) const
{
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
	if (!URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearGroupId(SourceAddress.ContainerId, EquipmentSlot))
	{
		return true;
	}

	if (!URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(EquipmentSlot))
	{
		return true;
	}

	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	return !InventoryLayout || InventoryLayout->CanUnequipSlotContainer(EquipmentSlot);
}

bool URpgInventoryUiActionComponent::IsPlayerEquipmentPlacement(
	const FRpgInventoryGridPlacement& Placement) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	FRpgInventorySlotAddress Address;
	return InventoryLayout && Placement.IsValid() &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(
			Placement,
			Address) &&
		(InventoryLayout->IsGearSlotAddress(Address) ||
			InventoryLayout->IsCarrySlotAddress(Address));
}

void URpgInventoryUiActionComponent::SyncEquipmentLoadoutFromGearSlots() const
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	if (!EquipmentLoadout || !PlayerInventory || !InventoryLayout)
	{
		return;
	}

	const ERpgEquipmentSlot PhysicalSlots[] =
	{
		ERpgEquipmentSlot::Head,
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Hands,
		ERpgEquipmentSlot::Legs,
		ERpgEquipmentSlot::Feet,
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag
	};

	bool bPhysicalMirrorChanged = false;
	for (const ERpgEquipmentSlot EquipmentSlot : PhysicalSlots)
	{
		FRpgInventorySlotAddress Address;
		URpgInventoryItemInstance* PhysicalItem = nullptr;
		if (URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(
				EquipmentSlot,
				Address))
		{
			PhysicalItem =
				InventoryLayout->GetItemInSlotAddress(Address);
		}

		if (EquipmentLoadout->GetItemInEquipmentSlot(
				EquipmentSlot) != PhysicalItem)
		{
			bPhysicalMirrorChanged = true;
			break;
		}
	}

	if (bPhysicalMirrorChanged)
	{
		// Rebuild once from the complete Gear snapshot so observers never see per-slot intermediate states.
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory();
		return;
	}

	// Carry changes affect load without changing the non-hand Gear mirror. Do not rebuild runtime actors or GAS grants.
	EquipmentLoadout->RefreshEquipmentLoadState();
}

void URpgInventoryUiActionComponent::SyncActiveHandsFromCarrySlots() const
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!EquipmentLoadout || !InventoryLayout)
	{
		return;
	}

	auto IsItemInCarrySlot = [InventoryLayout](const URpgInventoryItemInstance* Item, bool bOffHand)
	{
		if (!Item)
		{
			return false;
		}

		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Carry || !Group.Rule.bCarrySlot)
			{
				continue;
			}

			const bool bGroupIsOffHand = Group.Rule.CarryActivationRole == RpgGameplayTags::Equipment_Slot_OffHand;
			if (bGroupIsOffHand != bOffHand)
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					if (InventoryLayout->GetItemInSlotAddress(Group.MakeAddress(X, Y)) == Item)
					{
						return true;
					}
				}
			}
		}

		return false;
	};

	if (URpgInventoryItemInstance* MainHandItem = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
		MainHandItem && !IsItemInCarrySlot(MainHandItem, false))
	{
		EquipmentLoadout->ClearActiveMainHand();
	}

	if (URpgInventoryItemInstance* OffHandItem = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
		OffHandItem && !IsItemInCarrySlot(OffHandItem, true))
	{
		EquipmentLoadout->ClearActiveOffHand(true);
	}
}

bool URpgInventoryUiActionComponent::AreEquipmentIntentsEquivalent(
	const FRpgInventoryEquipmentIntent& A,
	const FRpgInventoryEquipmentIntent& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		A.ExpectedSourcePlacement == B.ExpectedSourcePlacement &&
		A.ExpectedQuantity == B.ExpectedQuantity &&
		A.Operation == B.Operation &&
		A.TargetEquipmentSlot == B.TargetEquipmentSlot;
}

bool URpgInventoryUiActionComponent::
	TryReplayRecentEquipmentIntentResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent)
{
	if (!Intent.RequestId.IsValid())
	{
		return false;
	}

	const FRecentEquipmentIntentResult* CachedResult =
		RecentEquipmentIntentResults.Find(Intent.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->Inventory,
			CachedResult->bHadInventory,
			CachedResult->InventoryMutationEpoch))
	{
		RecentEquipmentIntentResults.Remove(Intent.RequestId);
		RecentEquipmentIntentOrder.Remove(Intent.RequestId);
		return false;
	}

	if (CachedResult->Inventory.Get() != Inventory ||
		!AreEquipmentIntentsEquivalent(
			CachedResult->Intent,
			Intent))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected equipment RequestId collision for %s."),
			*Intent.RequestId.ToString());
		SendActionFeedback(
			GetActionTagForEquipmentIntent(Intent.Operation),
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			Inventory
				? Inventory->FindItemById(Intent.ItemId)
				: nullptr,
			Intent.ExpectedQuantity,
			Intent.RequestId,
			Intent.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedInventory =
		CachedResult->Inventory.Get();
	SendActionFeedback(
		GetActionTagForEquipmentIntent(
			CachedResult->Intent.Operation),
		CachedResult->Result,
		CachedInventory,
		CachedInventory
			? CachedInventory->FindItemById(
				CachedResult->Intent.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Intent.RequestId,
		CachedResult->Intent.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::
	SendAndCacheEquipmentIntentFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount)
{
	if (Intent.RequestId.IsValid())
	{
		FRecentEquipmentIntentResult CachedResult;
		CachedResult.Inventory = Inventory;
		CachedResult.bHadInventory = Inventory != nullptr;
		CachedResult.InventoryMutationEpoch = Inventory
			? Inventory->GetMutationEpoch()
			: 0;
		CachedResult.Intent = Intent;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentEquipmentIntentResults.Add(
			Intent.RequestId,
			MoveTemp(CachedResult));
		RecentEquipmentIntentOrder.Remove(Intent.RequestId);
		RecentEquipmentIntentOrder.Add(Intent.RequestId);
		while (RecentEquipmentIntentOrder.Num() >
			MaxRecentEquipmentIntentResults)
		{
			RecentEquipmentIntentResults.Remove(
				RecentEquipmentIntentOrder[0]);
			RecentEquipmentIntentOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		GetActionTagForEquipmentIntent(Intent.Operation),
		Result,
		Inventory,
		Item,
		FeedbackStackCount,
		Intent.RequestId,
		Intent.ItemId);
}

bool URpgInventoryUiActionComponent::AreExactTransferIntentsEquivalent(
	const FRpgInventoryTransferIntent& A,
	const FRpgInventoryTransferIntent& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		A.ExpectedSourcePlacement == B.ExpectedSourcePlacement &&
		A.TargetContainer == B.TargetContainer &&
		A.TargetPlacement == B.TargetPlacement &&
		A.Quantity == B.Quantity;
}

bool URpgInventoryUiActionComponent::TryReplayRecentExactTransferResult(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent)
{
	if (!Intent.RequestId.IsValid())
	{
		return false;
	}

	const FRecentExactTransferResult* CachedResult =
		RecentExactTransferResults.Find(Intent.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->SourceInventory,
			CachedResult->bHadSourceInventory,
			CachedResult->SourceMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentExactTransferResults.Remove(Intent.RequestId);
		RecentExactTransferOrder.Remove(Intent.RequestId);
		return false;
	}

	if (CachedResult->SourceInventory.Get() != SourceInventory ||
		CachedResult->TargetInventory.Get() != TargetInventory ||
		!AreExactTransferIntentsEquivalent(
			CachedResult->Intent,
			Intent))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected exact-transfer RequestId collision for %s."),
			*Intent.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			SourceInventory,
			SourceInventory
				? SourceInventory->FindItemById(Intent.ItemId)
				: nullptr,
			Intent.Quantity,
			Intent.RequestId,
			Intent.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedSource =
		CachedResult->SourceInventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		CachedResult->Result,
		CachedSource,
		CachedSource
			? CachedSource->FindItemById(
				CachedResult->Intent.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Intent.RequestId,
		CachedResult->Intent.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheExactTransferFeedback(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	if (Intent.RequestId.IsValid())
	{
		FRecentExactTransferResult CachedResult;
		CachedResult.SourceInventory = SourceInventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadSourceInventory = SourceInventory != nullptr;
		CachedResult.bHadTargetInventory = TargetInventory != nullptr;
		CachedResult.SourceMutationEpoch = SourceInventory
			? SourceInventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Intent = Intent;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentExactTransferResults.Add(
			Intent.RequestId,
			MoveTemp(CachedResult));
		RecentExactTransferOrder.Remove(Intent.RequestId);
		RecentExactTransferOrder.Add(Intent.RequestId);
		while (RecentExactTransferOrder.Num() >
			MaxRecentExactTransferResults)
		{
			RecentExactTransferResults.Remove(
				RecentExactTransferOrder[0]);
			RecentExactTransferOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		Result,
		SourceInventory,
		Item,
		FeedbackStackCount,
		Intent.RequestId,
		Intent.ItemId);
}

bool URpgInventoryUiActionComponent::AreQuickTransferRequestsEquivalent(
	const FRpgInventoryQuickTransferRequest& A,
	const FRpgInventoryQuickTransferRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		A.ExpectedSourcePlacement == B.ExpectedSourcePlacement &&
		A.StackCount == B.StackCount &&
		A.PreferredTargetContainers == B.PreferredTargetContainers;
}

bool URpgInventoryUiActionComponent::TryReplayRecentQuickTransferResult(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request)
{
	if (!Request.RequestId.IsValid())
	{
		return false;
	}

	const FRecentQuickTransferResult* CachedResult =
		RecentQuickTransferResults.Find(Request.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->SourceInventory,
			CachedResult->bHadSourceInventory,
			CachedResult->SourceMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentQuickTransferResults.Remove(Request.RequestId);
		RecentQuickTransferOrder.Remove(Request.RequestId);
		return false;
	}

	if (CachedResult->SourceInventory.Get() != SourceInventory ||
		CachedResult->TargetInventory.Get() != TargetInventory ||
		!AreQuickTransferRequestsEquivalent(
			CachedResult->Request,
			Request))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected quick-transfer RequestId collision for %s."),
			*Request.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			SourceInventory,
			SourceInventory
				? SourceInventory->FindItemById(Request.ItemId)
				: nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedSource =
		CachedResult->SourceInventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		CachedResult->Result,
		CachedSource,
		CachedSource
			? CachedSource->FindItemById(
				CachedResult->Request.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Request.RequestId,
		CachedResult->Request.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheQuickTransferFeedback(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	if (Request.RequestId.IsValid())
	{
		FRecentQuickTransferResult CachedResult;
		CachedResult.SourceInventory = SourceInventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadSourceInventory = SourceInventory != nullptr;
		CachedResult.bHadTargetInventory = TargetInventory != nullptr;
		CachedResult.SourceMutationEpoch = SourceInventory
			? SourceInventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Request = Request;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentQuickTransferResults.Add(
			Request.RequestId,
			MoveTemp(CachedResult));
		RecentQuickTransferOrder.Remove(Request.RequestId);
		RecentQuickTransferOrder.Add(Request.RequestId);
		while (RecentQuickTransferOrder.Num() >
			MaxRecentQuickTransferResults)
		{
			RecentQuickTransferResults.Remove(
				RecentQuickTransferOrder[0]);
			RecentQuickTransferOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		Result,
		SourceInventory,
		Item,
		FeedbackStackCount,
		Request.RequestId,
		Request.ItemId);
}

bool URpgInventoryUiActionComponent::AreManualDropRequestsEquivalent(
	const FRpgInventoryManualDropRequest& A,
	const FRpgInventoryManualDropRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.EntryId == B.EntryId &&
		A.ItemId == B.ItemId &&
		A.ExpectedSourcePlacement == B.ExpectedSourcePlacement &&
		A.StackCount == B.StackCount &&
		A.bConfirmed == B.bConfirmed;
}

bool URpgInventoryUiActionComponent::TryReplayRecentManualDropResult(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryManualDropRequest& Request)
{
	if (!Request.RequestId.IsValid())
	{
		return false;
	}

	const FRecentManualDropResult* CachedResult =
		RecentManualDropResults.Find(Request.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->Inventory,
			CachedResult->bHadInventory,
			CachedResult->InventoryMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentManualDropResults.Remove(Request.RequestId);
		RecentManualDropOrder.Remove(Request.RequestId);
		return false;
	}

	if (CachedResult->Inventory.Get() != Inventory ||
		!AreManualDropRequestsEquivalent(CachedResult->Request, Request))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected manual-drop RequestId collision for %s."),
			*Request.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Drop,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			Inventory ? Inventory->FindItemById(Request.ItemId) : nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedInventory =
		CachedResult->Inventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Drop,
		CachedResult->Result,
		CachedInventory,
		CachedInventory
			? CachedInventory->FindItemById(CachedResult->Request.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Request.RequestId,
		CachedResult->Request.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheManualDropFeedback(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryManualDropRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount,
	URpgInventoryManagerComponent* TargetInventory)
{
	if (Request.RequestId.IsValid())
	{
		FRecentManualDropResult CachedResult;
		CachedResult.Inventory = Inventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadInventory = Inventory != nullptr;
		CachedResult.bHadTargetInventory =
			TargetInventory != nullptr;
		CachedResult.InventoryMutationEpoch = Inventory
			? Inventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Request = Request;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentManualDropResults.Add(Request.RequestId, MoveTemp(CachedResult));
		RecentManualDropOrder.Remove(Request.RequestId);
		RecentManualDropOrder.Add(Request.RequestId);
		while (RecentManualDropOrder.Num() > MaxRecentManualDropResults)
		{
			RecentManualDropResults.Remove(RecentManualDropOrder[0]);
			RecentManualDropOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Drop,
		Result,
		Inventory,
		Item,
		FeedbackStackCount,
		Request.RequestId,
		Request.ItemId);
}

bool URpgInventoryUiActionComponent::TryTransferManualDrop(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FGuid& RequestId,
	URpgInventoryManagerComponent*& OutTargetInventory)
{
	OutTargetInventory = nullptr;
	if (!SourceInventory || !Item || StackCount <= 0 || !RequestId.IsValid() ||
		!GetWorld() || !SourceInventory->ContainsItemInstance(Item))
	{
		return false;
	}

	const FTransform DropTransform = GetManualDropTransform();
	const bool bTransfersWholeEntry =
		StackCount == SourceInventory->GetItemStackCount(Item);
	auto CanTransferIntoActor =
		[SourceInventory, Item, StackCount, bTransfersWholeEntry](
			const ARpgDroppedInventoryActor* DropActor)
		{
			URpgInventoryManagerComponent* TargetInventory =
				DropActor ? DropActor->GetLootInventoryManager() : nullptr;
			return TargetInventory != SourceInventory &&
				CanTargetAcceptTransferredStack(
				TargetInventory,
				Item,
				StackCount,
				bTransfersWholeEntry);
		};

	auto TryTransferIntoActor =
		[SourceInventory, Item, StackCount, RequestId](
			ARpgDroppedInventoryActor* DropActor)
	{
		if (!DropActor ||
			DropActor->GetLootInventoryManager() == SourceInventory)
		{
			return false;
		}

		const FRpgInventoryMutationResult TransferResult =
			DropActor->TransferItemFromInventory(
				SourceInventory,
				Item->GetItemId(),
				StackCount,
				RequestId);
		return TransferResult.IsSuccess() &&
			TransferResult.AppliedQuantity == StackCount;
	};

	ARpgDroppedInventoryActor* TargetDropActor = nullptr;
	if (ManualDropMergeRadius > 0.0f)
	{
		const float MergeRadiusSq = FMath::Square(ManualDropMergeRadius);
		for (TActorIterator<ARpgDroppedInventoryActor> It(GetWorld()); It; ++It)
		{
			ARpgDroppedInventoryActor* ExistingDrop = *It;
			if (ExistingDrop && !ExistingDrop->IsPendingKillPending() &&
				FVector::DistSquared(ExistingDrop->GetActorLocation(), DropTransform.GetLocation()) <= MergeRadiusSq &&
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
		TSubclassOf<ARpgDroppedInventoryActor> DropClass = ManualDropActorClass;
		if (!DropClass)
		{
			DropClass = ARpgDroppedInventoryActor::StaticClass();
		}

		AController* OwnerController = Cast<AController>(GetOwner());
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.Instigator = OwnerController ? OwnerController->GetPawn() : nullptr;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		TargetDropActor = GetWorld()->SpawnActor<ARpgDroppedInventoryActor>(
			DropClass,
			DropTransform,
			SpawnParameters);
		bSpawnedTargetActor = TargetDropActor != nullptr;
	}

	if (bSpawnedTargetActor)
	{
		// A freshly spawned manual-drop actor represents only the concrete item being
		// discarded. Blueprint defaults are for placed/world-authored loot and must not be
		// duplicated into every player-created drop.
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

FTransform URpgInventoryUiActionComponent::GetManualDropTransform() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	const APawn* Pawn = OwnerController ? OwnerController->GetPawn() : nullptr;
	const AActor* SourceActor = Pawn ? Cast<AActor>(Pawn) : GetOwner();
	if (!SourceActor)
	{
		return FTransform::Identity;
	}

	FVector SpawnLocation = SourceActor->GetActorLocation();
	SpawnLocation += SourceActor->GetActorForwardVector() * ManualDropForwardDistance;
	SpawnLocation += FVector::UpVector * ManualDropUpOffset;
	return FTransform(SourceActor->GetActorRotation(), SpawnLocation);
}

void URpgInventoryUiActionComponent::SendActionFeedback(
	FGameplayTag ActionTag,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FGuid& RequestId,
	FRpgInventoryItemId ItemId) const
{
	FRpgInventoryActionFeedbackMessage Message;
	Message.RequestId = RequestId;
	Message.ItemId = ItemId.IsValid() ? ItemId : (Item ? Item->GetItemId() : FRpgInventoryItemId());
	Message.ActionTag = ActionTag;
	Message.Result = Result;
	Message.InventoryOwner = Inventory;
	// Full transfers and merges can unregister the source subobject before this reliable RPC serializes. Stable ItemId
	// remains authoritative; include the UObject only while it is still owned by the reported inventory.
	Message.Item = Inventory && Item && Inventory->ContainsItemInstance(Item) ? Item : nullptr;
	Message.StackCount = StackCount;

	const_cast<URpgInventoryUiActionComponent*>(this)->ClientBroadcastInventoryActionFeedback(Message);
}

void URpgInventoryUiActionComponent::ClientBroadcastInventoryActionFeedback_Implementation(const FRpgInventoryActionFeedbackMessage& Message)
{
	if (!GetWorld())
	{
		return;
	}

	APlayerController* LocalRecipient = Cast<APlayerController>(GetOwner());
	if (!ensureMsgf(
			LocalRecipient,
			TEXT("Inventory UI feedback requires a PlayerController-owned action component: %s"),
			*GetNameSafe(this)))
	{
		return;
	}

	FRpgInventoryActionFeedbackMessage LocalMessage = Message;
	LocalMessage.Recipient = LocalRecipient;
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(GetWorld());
	MessageSubsystem.BroadcastMessage(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		LocalMessage);
}
