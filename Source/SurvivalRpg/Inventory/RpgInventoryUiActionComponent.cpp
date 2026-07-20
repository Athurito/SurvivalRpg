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
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits && Traits->GetMaxStackSize() > 1;
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

		if (IsStackableItem(Item))
		{
			return TargetInventory->CanAddItemDefinition(Item->GetItemDef(), TransferCount);
		}

		return bTransfersWholeEntry &&
			TargetInventory->CanReceiveTransferredItemInstance(Item, TransferCount);
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
		case ERpgInventoryMutationOperation::Move:
		case ERpgInventoryMutationOperation::Rotate:
		case ERpgInventoryMutationOperation::Merge:
		case ERpgInventoryMutationOperation::Swap:
		case ERpgInventoryMutationOperation::Split:
		case ERpgInventoryMutationOperation::Sort:
		case ERpgInventoryMutationOperation::Equip:
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

	const FRpgInventoryMutationResult Result = Inventory->ExecuteInventoryMutation(Request);
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

	SyncEquipmentLoadoutFromGearSlots();
	SyncActiveHandsFromCarrySlots();
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->RefreshEquipmentLoadState();
	}
	SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::Success, Inventory, ItemBeforeMutation, Result.AppliedQuantity, Result.RequestId, Request.ItemId);
}

void URpgInventoryUiActionComponent::RequestExecuteInventoryItemAction_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemActionRequest Request)
{
	const bool bKnownIntent = Request.Intent == ERpgInventoryItemActionIntent::Use ||
		Request.Intent == ERpgInventoryItemActionIntent::EquipAndActivate ||
		Request.Intent == ERpgInventoryItemActionIntent::MoveToCarry;
	const FGameplayTag ActionTag = Request.Intent == ERpgInventoryItemActionIntent::Use
		? RpgGameplayTags::Rpg_Inventory_Action_Use
		: Request.Intent == ERpgInventoryItemActionIntent::MoveToCarry
			? RpgGameplayTags::Rpg_Inventory_Action_MoveToCarry
			: RpgGameplayTags::Rpg_Inventory_Action_EquipAndActivate;
	if (!bKnownIntent)
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

	if (Request.Intent == ERpgInventoryItemActionIntent::Use)
	{
		ExecuteUseInventoryItem(Inventory, Item, Request.StackCount, Request.RequestId);
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (Inventory != PlayerInventory)
	{
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::WrongInventory, Inventory, Item, 1, Request.RequestId, Request.ItemId);
		return;
	}

	if (Request.Intent == ERpgInventoryItemActionIntent::MoveToCarry)
	{
		if (!Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>())
		{
			SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::NotEquippable, Inventory, Item, 1, Request.RequestId, Request.ItemId);
			return;
		}

		if (!TryMoveItemToFirstCompatibleCarrySlot(Item))
		{
			SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::NoValidSlot, Inventory, Item, 1, Request.RequestId, Request.ItemId);
			return;
		}

		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
		if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
		{
			EquipmentLoadout->RefreshEquipmentLoadState();
		}
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::Success, Inventory, Item, 1, Request.RequestId, Request.ItemId);
		return;
	}

	if (!Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() &&
		!Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
	{
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::NotEquippable, Inventory, Item, 1, Request.RequestId, Request.ItemId);
		return;
	}

	if (!TryAssignItemToDefaultEquipmentDestination(Item))
	{
		SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::NoValidSlot, Inventory, Item, 1, Request.RequestId, Request.ItemId);
		return;
	}

	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->RefreshEquipmentLoadState();
	}
	SendActionFeedback(ActionTag, ERpgInventoryActionFeedbackResult::Success, Inventory, Item, 1, Request.RequestId, Request.ItemId);
}

void URpgInventoryUiActionComponent::RequestQuickTransferItem_Implementation(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryQuickTransferRequest Request)
{
	if (!SourceInventory || !TargetInventory || !CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::NoAccess,
			SourceInventory, nullptr, Request.StackCount, Request.RequestId, Request.ItemId);
		return;
	}

	if (!IsUiTransferDirectionAllowed(SourceInventory, TargetInventory))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::ServerRejected,
			SourceInventory, nullptr, Request.StackCount, Request.RequestId, Request.ItemId);
		return;
	}

	URpgInventoryItemInstance* Item = SourceInventory->FindItemById(Request.ItemId);
	if (!Item)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::MissingItem,
			SourceInventory, nullptr, Request.StackCount, Request.RequestId, Request.ItemId);
		return;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	const int32 RequestedCount = Request.StackCount <= 0 ? AvailableCount : Request.StackCount;
	if (AvailableCount <= 0 || RequestedCount <= 0 || RequestedCount > AvailableCount ||
		(SourceInventory == TargetInventory && RequestedCount != AvailableCount))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidRequest,
			SourceInventory, Item, Request.StackCount, Request.RequestId, Request.ItemId);
		return;
	}

	FRpgInventoryContainerHandle TargetContainer;
	FRpgInventoryGridPlacement TargetPlacement;
	if (!FindQuickTransferDestination(SourceInventory, TargetInventory, Request, TargetContainer, TargetPlacement))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InventoryFull,
			SourceInventory, Item, RequestedCount, Request.RequestId, Request.ItemId);
		return;
	}

	FRpgInventoryGridPlacement SourcePlacement;
	if (!SourceInventory->GetItemPlacement(Item, SourcePlacement))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::MissingItem,
			SourceInventory, Item, RequestedCount, Request.RequestId, Request.ItemId);
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
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			SourceInventory,
			Item,
			RequestedCount,
			Request.RequestId,
			Request.ItemId);
		return;
	}

	FRpgInventoryMutationRequest MutationRequest;
	MutationRequest.RequestId = Request.RequestId;
	MutationRequest.ItemId = Request.ItemId;
	MutationRequest.Source = SourcePlacement.GetContainerHandle();
	MutationRequest.Target = TargetContainer;
	MutationRequest.Quantity = RequestedCount;
	FRpgInventoryMutationResult MutationResult;
	if (SourceInventory == TargetInventory)
	{
		MutationRequest.Operation = ERpgInventoryMutationOperation::Move;
		MutationRequest.TargetPlacement = TargetPlacement;
		MutationResult = SourceInventory->ExecuteInventoryMutation(MutationRequest);
	}
	else
	{
		MutationRequest.Operation = ERpgInventoryMutationOperation::Transfer;
		// An invalid exact placement deliberately lets the cross-inventory planner merge across every compatible
		// stack in the selected container before allocating the remaining quantity at its deterministic first fit.
		MutationResult = SourceInventory->ExecuteCrossInventoryTransfer(TargetInventory, MutationRequest, false);
	}

	if (!MutationResult.IsSuccess() || MutationResult.AppliedQuantity != RequestedCount)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, GetFeedbackForMutationResult(MutationResult.Code),
			SourceInventory, Item, RequestedCount, MutationResult.RequestId, Request.ItemId);
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
	SyncEquipmentLoadoutFromGearSlots();
	SyncActiveHandsFromCarrySlots();
	if (!EquipmentLoadout)
	{
		EquipmentLoadout = FindEquipmentLoadout();
	}
	if (EquipmentLoadout)
	{
		EquipmentLoadout->RefreshEquipmentLoadState();
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::Success,
		SourceInventory, Item, MutationResult.AppliedQuantity, MutationResult.RequestId, Request.ItemId);
}

void URpgInventoryUiActionComponent::RequestAssignItemToEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(EquipmentSlot))
	{
		URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
		if (!TryMoveAndActivateItemInCarry(Item, EquipmentSlot))
		{
			SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NoValidSlot, PlayerInventory, Item, 1);
			return;
		}

		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::Success, PlayerInventory, Item, 1);
		return;
	}

	if (TryMoveItemToGearSlot(EquipmentSlot, Item))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::Success, FindPlayerInventory(), Item, 1);
		return;
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NotEquippable, FindPlayerInventory(), Item, 1);
}

void URpgInventoryUiActionComponent::RequestClearEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot)
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		if (EquipmentSlot != ERpgEquipmentSlot::MainHand &&
			EquipmentSlot != ERpgEquipmentSlot::OffHand)
		{
			if (URpgInventoryItemInstance* SlotItem = EquipmentLoadout->GetItemInEquipmentSlot(EquipmentSlot))
			{
				FRpgInventorySlotAddress GearAddress;
				if (URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, GearAddress) &&
					!CanMoveItemOutOfGearSlot(GearAddress))
				{
					SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::ServerRejected, FindPlayerInventory(), SlotItem, 1);
					return;
				}

				if (!TryMoveItemToFirstCompatibleContentSlot(SlotItem))
				{
					SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::InventoryFull, FindPlayerInventory(), SlotItem, 1);
					return;
				}
			}
		}

		EquipmentLoadout->ClearEquipmentSlot(EquipmentSlot);
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
	}
}

void URpgInventoryUiActionComponent::RequestTransferItemStack_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount)
{
	FRpgInventoryQuickTransferRequest Request;
	Request.ItemId = Item ? Item->GetItemId() : FRpgInventoryItemId();
	Request.StackCount = StackCount;
	RequestQuickTransferItem_Implementation(SourceInventory, TargetInventory, MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestTransferItemStackToPlacement_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, FRpgInventoryGridPlacement TargetPlacement)
{
	if (!SourceInventory || !TargetInventory || !Item || SourceInventory == TargetInventory || !TargetPlacement.IsValid() ||
		!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory) ||
		!SourceInventory->ContainsItemInstance(Item))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::NoAccess, SourceInventory, Item, StackCount);
		return;
	}

	if (!IsUiTransferDirectionAllowed(SourceInventory, TargetInventory))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::ServerRejected, SourceInventory, Item, StackCount);
		return;
	}

	FRpgInventoryGridPlacement SourcePlacement;
	if (!SourceInventory->GetItemPlacement(Item, SourcePlacement))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::MissingItem, SourceInventory, Item, StackCount);
		return;
	}

	FRpgInventoryMutationRequest Request;
	Request.Operation = ERpgInventoryMutationOperation::Transfer;
	Request.ItemId = Item->GetItemId();
	Request.Source = SourcePlacement.GetContainerHandle();
	Request.Target = TargetPlacement.GetContainerHandle();
	Request.TargetPlacement = TargetPlacement;
	Request.Quantity = StackCount;
	Request.EnsureRequestId();
	const FRpgInventoryMutationResult MutationResult = SourceInventory->ExecuteCrossInventoryTransfer(TargetInventory, Request, false);
	if (!MutationResult.IsSuccess())
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			MutationResult.Code == ERpgInventoryMutationResultCode::NoSpace || MutationResult.Code == ERpgInventoryMutationResultCode::Occupied
				? ERpgInventoryActionFeedbackResult::InventoryFull
				: ERpgInventoryActionFeedbackResult::ServerRejected,
			SourceInventory, Item, StackCount);
		return;
	}

	if (SourceInventory == FindPlayerInventory() &&
		!SourceInventory->FindItemById(Request.ItemId))
	{
		ClearPlayerAssignmentsForItem(Item);
	}
	SyncEquipmentLoadoutFromGearSlots();
	SyncActiveHandsFromCarrySlots();
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->RefreshEquipmentLoadState();
	}
	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::Success, SourceInventory, Item, MutationResult.AppliedQuantity);
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

	FRpgInventoryMutationRequest Request;
	Request.Operation = Entry->Placement.GetContainerHandle() == TargetPlacement.GetContainerHandle() &&
		Entry->Placement.X == TargetPlacement.X && Entry->Placement.Y == TargetPlacement.Y &&
		Entry->Placement.bRotated != TargetPlacement.bRotated
			? ERpgInventoryMutationOperation::Rotate
			: ERpgInventoryMutationOperation::Move;
	Request.ItemId = Entry->ItemId;
	Request.Source = Entry->Placement.GetContainerHandle();
	Request.Target = TargetPlacement.GetContainerHandle();
	Request.TargetPlacement = TargetPlacement;
	Request.Quantity = Entry->StackCount;
	RequestInventoryMutation_Implementation(Inventory, Request);
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

	FGuid EntryId;
	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
	{
		if (Entry.Instance == Item)
		{
			EntryId = Entry.EntryId;
			break;
		}
	}

	if (!EntryId.IsValid() || !PlayerInventory->CanMoveInventoryEntryToPlacement(EntryId, TargetPlacement))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
		return;
	}

	if (!PlayerInventory->MoveInventoryEntryToPlacement(EntryId, TargetPlacement))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, ERpgInventoryActionFeedbackResult::ServerRejected, PlayerInventory, Item, 1);
		return;
	}

	SyncEquipmentLoadoutFromGearSlots();
	SyncActiveHandsFromCarrySlots();
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

	if (!TryMoveItemToGearSlot(ContainerSlot, Item))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::ServerRejected, PlayerInventory, Item, 1);
		return;
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::Success, PlayerInventory, Item, 1);
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

	RequestUnequipInventoryItemToContentSlot_Implementation(ProviderItem);
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

	FRpgInventoryGridPlacement SourcePlacement;
	if (!Inventory->GetItemPlacement(Item, SourcePlacement))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Split, ERpgInventoryActionFeedbackResult::MissingItem,
			Inventory, Item, ActualSplitCount, RequestId, ItemId);
		return;
	}

	FRpgInventoryMutationRequest Request;
	Request.Operation = ERpgInventoryMutationOperation::Split;
	Request.ItemId = ItemId;
	Request.Source = SourcePlacement.GetContainerHandle();
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

	const int32 UseCount = FMath::Max(1, StackCount);
	const int32 ConsumeCount = FMath::Max(0, UsableFragment->ConsumeCount) * UseCount;
	if (ConsumeCount > AvailableCount)
	{
		SendUseFeedback(ERpgInventoryActionFeedbackResult::MissingItem, ConsumeCount);
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
		if (Inventory == PlayerInventory && ConsumeCount >= AvailableCount)
		{
			if (!ClearPlayerAssignmentsForItem(Item))
			{
				SendUseFeedback(ERpgInventoryActionFeedbackResult::ServerRejected, ConsumeCount);
				return;
			}
		}

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

	if (!TryAssignItemToDefaultEquipmentDestination(Item))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NoValidSlot, PlayerInventory, Item, 1);
		return;
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::Success, PlayerInventory, Item, 1);
}

void URpgInventoryUiActionComponent::RequestUnequipInventoryItemToContentSlot_Implementation(URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!PlayerInventory || !InventoryLayout || !Item || PlayerInventory->GetItemStackCount(Item) <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::MissingItem, PlayerInventory, Item, 1);
		return;
	}

	FRpgInventorySlotAddress SourceAddress;
	FRpgInventoryGridPlacement SourcePlacement;
	if (!PlayerInventory->GetItemPlacement(Item, SourcePlacement) ||
		!InventoryLayout->TryMakeSlotAddressFromPlacement(SourcePlacement, SourceAddress))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
		return;
	}

	if (InventoryLayout->IsGearSlotAddress(SourceAddress) && !CanMoveItemOutOfGearSlot(SourceAddress))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::ServerRejected, PlayerInventory, Item, 1);
		return;
	}

	if (!InventoryLayout->IsGearSlotAddress(SourceAddress) && !InventoryLayout->IsCarrySlotAddress(SourceAddress))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::InvalidSlot, PlayerInventory, Item, 1);
		return;
	}

	if (!TryMoveItemToFirstCompatibleContentSlot(Item))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::InventoryFull, PlayerInventory, Item, 1);
		return;
	}

	SyncEquipmentLoadoutFromGearSlots();
	SyncActiveHandsFromCarrySlots();
	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::Success, PlayerInventory, Item, 1);
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

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const bool bDropsWholePlayerEntry =
		Inventory == PlayerInventory && Request.StackCount == AvailableCount;
	if (bDropsWholePlayerEntry && !ClearPlayerAssignmentsForItem(Item))
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			Request.StackCount);
		return;
	}

	if (!TryTransferManualDrop(
			Inventory,
			Item,
			Request.StackCount,
			Request.RequestId))
	{
		// Physical gear/carry placement never changed, so a failed transfer can restore runtime equipment directly.
		if (bDropsWholePlayerEntry)
		{
			SyncEquipmentLoadoutFromGearSlots();
			SyncActiveHandsFromCarrySlots();
		}
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
		SyncEquipmentLoadoutFromGearSlots();
		SyncActiveHandsFromCarrySlots();
		if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
		{
			EquipmentLoadout->RefreshEquipmentLoadState();
		}
	}

	SendAndCacheManualDropFeedback(
		Inventory,
		Request,
		ERpgInventoryActionFeedbackResult::Success,
		Item,
		Request.StackCount);
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

		if (PlayerInventory->RemoveItemInstanceStack(Item, CountToDeposit))
		{
			BaseStorage->StoreResource(ItemDefinition, CountToDeposit);
		}
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

	if (PlayerInventory->RemoveItemInstanceStack(Item, TransferCount))
	{
		BaseStorage->StoreResource(ItemDefinition, TransferCount);
	}
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
		PlayerInventory->GrantItemDefinition(ItemDefinition, StackCount);
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

	FRpgInventoryGridPlacement SourcePlacement;
	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0 || StackCount > AvailableCount || !SourceInventory->GetItemPlacement(Item, SourcePlacement))
	{
		return false;
	}

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
					FRpgInventoryMutationRequest PlanRequest;
					PlanRequest.Operation = ERpgInventoryMutationOperation::Move;
					PlanRequest.ItemId = Item->GetItemId();
					PlanRequest.Source = SourcePlacement.GetContainerHandle();
					PlanRequest.Target = TargetContainer;
					PlanRequest.TargetPlacement = Candidate;
					PlanRequest.Quantity = AvailableCount;
					const FRpgInventoryMutationResult PlanResult = SourceInventory->PlanInventoryMutation(PlanRequest);
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

bool URpgInventoryUiActionComponent::TryAssignItemToDefaultEquipmentDestination(URpgInventoryItemInstance* Item)
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
		return TryMoveAndActivateItemInCarry(Item, DefaultSlot);
	}

	if (FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(DefaultSlot))
	{
		return TryMoveItemToGearSlot(DefaultSlot, Item);
	}

	if (TryMoveAndActivateItemInCarry(Item, ERpgEquipmentSlot::None))
	{
		return true;
	}

	return false;
}

bool URpgInventoryUiActionComponent::TryMoveAndActivateItemInCarry(URpgInventoryItemInstance* Item, ERpgEquipmentSlot PreferredHandSlot)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	if (!Item || !PlayerInventory || !InventoryLayout || !EquipmentLoadout || !PlayerInventory->ContainsItemInstance(Item))
	{
		return false;
	}

	FRpgInventoryGridPlacement OriginalPlacement;
	if (!PlayerInventory->GetItemPlacement(Item, OriginalPlacement))
	{
		return false;
	}

	URpgInventoryItemInstance* PreviousMainHand = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
	URpgInventoryItemInstance* PreviousOffHand = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
	const TArray<FRpgInventorySlotGroupView> Groups = InventoryLayout->GetSlotGroups();

	auto TryActivateRole = [EquipmentLoadout, Item](const FGameplayTag& ActivationRole)
	{
		return ActivationRole == RpgGameplayTags::Equipment_Slot_MainHand
			? EquipmentLoadout->ActivateMainHandItem(Item)
			: ActivationRole == RpgGameplayTags::Equipment_Slot_OffHand && EquipmentLoadout->ActivateOffHandItem(Item);
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

					FRpgInventoryMutationRequest EquipRequest;
					EquipRequest.Operation = ERpgInventoryMutationOperation::Equip;
					EquipRequest.ItemId = Item->GetItemId();
					EquipRequest.Source = OriginalPlacement.GetContainerHandle();
					EquipRequest.Target = TargetPlacement.GetContainerHandle();
					EquipRequest.TargetPlacement = TargetPlacement;
					EquipRequest.Quantity = PlayerInventory->GetItemStackCount(Item);
					EquipRequest.EnsureRequestId();
					if (!PlayerInventory->PlanInventoryMutation(EquipRequest).IsSuccess())
					{
						continue;
					}

					const FRpgInventoryMutationResult EquipResult = PlayerInventory->ExecuteInventoryMutation(EquipRequest);
					if (!EquipResult.IsSuccess())
					{
						continue;
					}

					SyncActiveHandsFromCarrySlots();
					if (TryActivateRole(DesiredRole))
					{
						return true;
					}

					// Activation was prevalidated and should be side-effect free on failure. Restore the exact physical state
					// through the same atomic swap planner before trying another compatible semantic carry role.
					FRpgInventoryGridPlacement CurrentPlacement;
					if (PlayerInventory->GetItemPlacement(Item, CurrentPlacement))
					{
						FRpgInventoryMutationRequest RollbackRequest;
						RollbackRequest.Operation = ERpgInventoryMutationOperation::Move;
						RollbackRequest.ItemId = Item->GetItemId();
						RollbackRequest.Source = CurrentPlacement.GetContainerHandle();
						RollbackRequest.Target = OriginalPlacement.GetContainerHandle();
						RollbackRequest.TargetPlacement = OriginalPlacement;
						RollbackRequest.Quantity = PlayerInventory->GetItemStackCount(Item);
						RollbackRequest.EnsureRequestId();
						const FRpgInventoryMutationResult RollbackResult = PlayerInventory->ExecuteInventoryMutation(RollbackRequest);
						if (!RollbackResult.IsSuccess())
						{
							return false;
						}
					}
					if (PreviousMainHand &&
						EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand) != PreviousMainHand)
					{
						EquipmentLoadout->ActivateMainHandItem(PreviousMainHand);
					}
					if (PreviousOffHand &&
						EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand) != PreviousOffHand)
					{
						EquipmentLoadout->ActivateOffHandItem(PreviousOffHand);
					}
				}
			}
		}
	}

	return false;
}

bool URpgInventoryUiActionComponent::TryMoveItemToGearSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
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
		if (Entry.Instance == Item && PlayerInventory->MoveInventoryEntryToPlacement(Entry.EntryId, TargetPlacement))
		{
			SyncEquipmentLoadoutFromGearSlots();
			SyncActiveHandsFromCarrySlots();
			return true;
		}
	}

	return false;
}

bool URpgInventoryUiActionComponent::TryMoveItemToFirstCompatibleCarrySlot(URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!Item || !PlayerInventory || !InventoryLayout || PlayerInventory->GetItemStackCount(Item) <= 0)
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

	FGuid EntryId;
	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
	{
		if (Entry.Instance == Item)
		{
			EntryId = Entry.EntryId;
			break;
		}
	}

	if (!EntryId.IsValid())
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
				if (PlayerInventory->CanMoveInventoryEntryToPlacement(EntryId, TargetPlacement))
				{
					return PlayerInventory->MoveInventoryEntryToPlacement(EntryId, TargetPlacement);
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

	FGuid EntryId;
	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
	{
		if (Entry.Instance == Item)
		{
			EntryId = Entry.EntryId;
			break;
		}
	}

	if (!EntryId.IsValid())
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
				if (PlayerInventory->CanMoveInventoryEntryToPlacement(
						EntryId,
						TargetPlacement) &&
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
			return PlayerInventory->MoveInventoryEntryToPlacement(
				Entry.EntryId,
				TargetPlacement);
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

void URpgInventoryUiActionComponent::SyncEquipmentLoadoutFromGearSlots() const
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!EquipmentLoadout || !PlayerInventory || !InventoryLayout)
	{
		return;
	}

	const ERpgEquipmentSlot GearSlots[] =
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

	for (const ERpgEquipmentSlot EquipmentSlot : GearSlots)
	{
		FRpgInventorySlotAddress GearAddress;
		FRpgInventoryGridPlacement GearPlacement;
		if (!URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, GearAddress) ||
			!InventoryLayout->ResolveSlotAddress(GearAddress, GearPlacement))
		{
			continue;
		}

		URpgInventoryItemInstance* GearItem = PlayerInventory->GetItemAtContainerCell(
			GearPlacement.GetContainerHandle(), GearPlacement.X, GearPlacement.Y);
		if (EquipmentLoadout->GetItemInEquipmentSlot(EquipmentSlot) == GearItem)
		{
			continue;
		}

		if (GearItem)
		{
			EquipmentLoadout->AssignItemToEquipmentSlot(EquipmentSlot, GearItem);
		}
		else
		{
			EquipmentLoadout->ClearEquipmentSlot(EquipmentSlot);
		}
	}
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
		EquipmentLoadout->ClearActiveHands();
		return;
	}

	if (URpgInventoryItemInstance* OffHandItem = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
		OffHandItem && !IsItemInCarrySlot(OffHandItem, true))
	{
		EquipmentLoadout->ClearActiveOffHand(true);
	}
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
	int32 FeedbackStackCount)
{
	if (Request.RequestId.IsValid())
	{
		FRecentManualDropResult CachedResult;
		CachedResult.Inventory = Inventory;
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
	const FGuid& RequestId)
{
	if (!SourceInventory || !Item || StackCount <= 0 || !RequestId.IsValid() ||
		!GetWorld() || !SourceInventory->ContainsItemInstance(Item))
	{
		return false;
	}

	const FTransform DropTransform = GetManualDropTransform();
	FRpgInventoryGridPlacement SourcePlacement;
	if (!SourceInventory->GetItemPlacement(Item, SourcePlacement))
	{
		return false;
	}

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
		[SourceInventory, Item, StackCount, RequestId, &SourcePlacement](
			ARpgDroppedInventoryActor* DropActor)
	{
		URpgInventoryManagerComponent* TargetInventory = DropActor ? DropActor->GetLootInventoryManager() : nullptr;
		if (!TargetInventory || TargetInventory == SourceInventory)
		{
			return false;
		}

		FRpgInventoryMutationRequest Request;
		Request.Operation = ERpgInventoryMutationOperation::Transfer;
		Request.ItemId = Item->GetItemId();
		Request.Source = SourcePlacement.GetContainerHandle();
		Request.Target = FRpgInventoryContainerHandle::MakeRoot(TargetInventory->GetDefaultContainerId());
		Request.Quantity = StackCount;
		Request.RequestId = RequestId;
		const FRpgInventoryMutationResult TransferResult = SourceInventory->ExecuteCrossInventoryTransfer(TargetInventory, Request, false);
		if (TransferResult.IsSuccess())
		{
			DropActor->ForceNetUpdate();
			return true;
		}
		return false;
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

	if (TargetDropActor && TryTransferIntoActor(TargetDropActor))
	{
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
