#include "RpgInventoryUiActionDomainHandlers.h"

#include "RpgInventoryItemCapabilities.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

void FRpgInventoryQuickAccessActionHandler::ActivateCarrySlot(
	int32 ActionBarSlotIndex,
	FGameplayTag ExpectedCarrySemanticRole)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		FindEquipmentLoadout();
	URpgActionBarComponent* ActionBar = FindActionBar();
	const FRpgActionBarSlot CurrentSlot = ActionBar
		? ActionBar->GetSlot(ActionBarSlotIndex)
		: FRpgActionBarSlot();
	const bool bCurrentSlotIsCarry =
		CurrentSlot.SlotType == ERpgActionBarSlotType::CarrySlot ||
		CurrentSlot.SlotType ==
			ERpgActionBarSlotType::CarrySlotBinding;
	FRpgInventorySlotAddress CarrySlotAddress;
	if (!PlayerInventory || !InventoryLayout || !EquipmentLoadout ||
		!ActionBar ||
		!FMath::IsWithinInclusive(ActionBarSlotIndex, 0, 7) ||
		!ExpectedCarrySemanticRole.IsValid() ||
		!bCurrentSlotIsCarry ||
		CurrentSlot.CarrySemanticRole !=
			ExpectedCarrySemanticRole ||
		!InventoryLayout->TryMakeSlotAddressForSemanticRole(
			ExpectedCarrySemanticRole,
			CarrySlotAddress) ||
		!InventoryLayout->IsCarrySlotAddress(CarrySlotAddress))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidSlot,
			PlayerInventory,
			nullptr,
			1);
		return;
	}

	URpgInventoryItemInstance* Item =
		InventoryLayout->GetItemInSlotAddress(CarrySlotAddress);
	if (!Item)
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::MissingItem,
			PlayerInventory,
			nullptr,
			1);
		return;
	}
	if (!InventoryLayout->CanBindSlotAddressToActionbar(
			CarrySlotAddress,
			Item))
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Equip,
			ERpgInventoryActionFeedbackResult::InvalidSlot,
			PlayerInventory,
			Item,
			1);
		return;
	}

	ERpgEquipmentSlot EquipmentSlotRole =
		ERpgEquipmentSlot::None;
	bool bActivated = false;
	if (InventoryLayout->TryGetEquipmentSlotRoleForAddress(
			CarrySlotAddress,
			EquipmentSlotRole) &&
		EquipmentSlotRole == ERpgEquipmentSlot::OffHand)
	{
		bActivated =
			EquipmentLoadout->ActivateOffHandItem(Item);
	}
	else if (EquipmentSlotRole == ERpgEquipmentSlot::MainHand)
	{
		bActivated =
			EquipmentLoadout->ActivateMainHandItem(Item);
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Equip,
		bActivated
			? ERpgInventoryActionFeedbackResult::Success
			: ERpgInventoryActionFeedbackResult::NotEquippable,
		PlayerInventory,
		Item,
		1);
}

void FRpgInventoryQuickAccessActionHandler::ClearActiveHands()
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout =
			FindEquipmentLoadout())
	{
		EquipmentLoadout->ClearActiveHands();
	}
}

void FRpgInventoryQuickAccessActionHandler::MutateBinding(
	FRpgQuickAccessMutationRequest Request)
{
	Request.EnsureRequestId();

	URpgActionBarComponent* ActionBar = FindActionBar();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgInventoryItemInstance* ContextItem =
		PlayerInventory && Request.ContextItemId.IsValid()
			? PlayerInventory->FindItemById(
				Request.ContextItemId)
			: nullptr;
	auto SendQuickAccessFeedback =
		[this,
		 &Request,
		 PlayerInventory,
		 ContextItem](
			ERpgInventoryActionFeedbackResult Result)
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
		SendQuickAccessFeedback(
			ERpgInventoryActionFeedbackResult::InvalidRequest);
		return;
	}
	if (!FMath::IsWithinInclusive(Request.SlotIndex, 0, 7))
	{
		SendQuickAccessFeedback(
			ERpgInventoryActionFeedbackResult::InvalidSlot);
		return;
	}

	const auto IsCarryBinding =
		[](const FRpgActionBarSlot& Slot)
	{
		return Slot.SlotType ==
				ERpgActionBarSlotType::CarrySlot ||
			Slot.SlotType ==
				ERpgActionBarSlotType::CarrySlotBinding;
	};
	const auto IsConsumableBinding =
		[](const FRpgActionBarSlot& Slot)
	{
		return Slot.SlotType ==
				ERpgActionBarSlotType::Consumable ||
			Slot.SlotType ==
				ERpgActionBarSlotType::InventorySlotBinding;
	};

	switch (Request.Operation)
	{
	case ERpgQuickAccessMutationOperation::BindCarry:
	{
		FGameplayTag AuthoritativeSemanticRole;
		if (!Request.SourceAddress.IsValid() ||
			!Request.ExpectedCarrySemanticRole.IsValid() ||
			!InventoryLayout->TryGetSemanticRoleForAddress(
				Request.SourceAddress,
				AuthoritativeSemanticRole) ||
			AuthoritativeSemanticRole !=
				Request.ExpectedCarrySemanticRole ||
			!InventoryLayout->IsCarrySlotAddress(
				Request.SourceAddress))
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::InvalidSlot);
			return;
		}

		URpgInventoryItemInstance* SourceItem =
			InventoryLayout->GetItemInSlotAddress(
				Request.SourceAddress);
		if (!Request.ContextItemId.IsValid() || !SourceItem ||
			SourceItem->GetItemId() != Request.ContextItemId)
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::MissingItem);
			return;
		}
		if (!InventoryLayout->CanBindSlotAddressToActionbar(
				Request.SourceAddress,
				SourceItem))
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::InvalidSlot);
			return;
		}

		const bool bApplied =
			ActionBar->TryBindCarrySlotToSlotAuthority(
				Request.SlotIndex,
				Request.SourceAddress);
		SendQuickAccessFeedback(
			bApplied
				? ERpgInventoryActionFeedbackResult::Success
				: ERpgInventoryActionFeedbackResult::
					ServerRejected);
		return;
	}

	case ERpgQuickAccessMutationOperation::BindConsumable:
	{
		if (!Request.SourceAddress.IsValid() ||
			!Request.ExpectedConsumableDefinition ||
			!Request.ExpectedPreferredItemId.IsValid() ||
			Request.ContextItemId !=
				Request.ExpectedPreferredItemId ||
			InventoryLayout->IsCarrySlotAddress(
				Request.SourceAddress))
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::
					InvalidRequest);
			return;
		}

		URpgInventoryItemInstance* SourceItem =
			InventoryLayout->GetItemInSlotAddress(
				Request.SourceAddress);
		if (!SourceItem ||
			SourceItem->GetItemId() !=
				Request.ExpectedPreferredItemId ||
			SourceItem->GetItemDef() !=
				Request.ExpectedConsumableDefinition)
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::MissingItem);
			return;
		}
		if (!InventoryLayout->CanBindSlotAddressToActionbar(
				Request.SourceAddress,
				SourceItem) ||
			!FRpgInventoryItemCapabilities::HasUsableContract(
				SourceItem))
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::CannotUse);
			return;
		}

		const bool bApplied =
			ActionBar->TryBindInventorySlotToSlotAuthority(
				Request.SlotIndex,
				Request.SourceAddress);
		SendQuickAccessFeedback(
			bApplied
				? ERpgInventoryActionFeedbackResult::Success
				: ERpgInventoryActionFeedbackResult::
					ServerRejected);
		return;
	}

	case ERpgQuickAccessMutationOperation::ClearCarry:
	{
		if (!Request.ExpectedCarrySemanticRole.IsValid())
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::
					InvalidRequest);
			return;
		}

		const FRpgActionBarSlot CurrentSlot =
			ActionBar->GetSlot(Request.SlotIndex);
		if (!IsCarryBinding(CurrentSlot) ||
			CurrentSlot.CarrySemanticRole !=
				Request.ExpectedCarrySemanticRole)
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::
					ServerRejected);
			return;
		}

		SendQuickAccessFeedback(
			ActionBar->TryClearSlotAuthority(Request.SlotIndex)
				? ERpgInventoryActionFeedbackResult::Success
				: ERpgInventoryActionFeedbackResult::
					ServerRejected);
		return;
	}

	case ERpgQuickAccessMutationOperation::ClearConsumable:
	{
		if (!Request.ExpectedConsumableDefinition)
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::
					InvalidRequest);
			return;
		}

		const FRpgActionBarSlot CurrentSlot =
			ActionBar->GetSlot(Request.SlotIndex);
		if (!IsConsumableBinding(CurrentSlot) ||
			CurrentSlot.ConsumableDefinition !=
				Request.ExpectedConsumableDefinition ||
			CurrentSlot.PreferredItemId !=
				Request.ExpectedPreferredItemId)
		{
			SendQuickAccessFeedback(
				ERpgInventoryActionFeedbackResult::
					ServerRejected);
			return;
		}

		SendQuickAccessFeedback(
			ActionBar->TryClearSlotAuthority(Request.SlotIndex)
				? ERpgInventoryActionFeedbackResult::Success
				: ERpgInventoryActionFeedbackResult::
					ServerRejected);
		return;
	}
	}

	SendQuickAccessFeedback(
		ERpgInventoryActionFeedbackResult::InvalidRequest);
}

void FRpgInventoryQuickAccessActionHandler::BindInventorySlot(
	int32 ActionBarSlotIndex,
	FRpgInventorySlotAddress SlotAddress)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	const URpgInventoryItemInstance* SourceItem = InventoryLayout
		? InventoryLayout->GetItemInSlotAddress(SlotAddress)
		: nullptr;

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation =
		ERpgQuickAccessMutationOperation::BindConsumable;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.SourceAddress = SlotAddress;
	Request.ExpectedConsumableDefinition =
		SourceItem ? SourceItem->GetItemDef() : nullptr;
	Request.ExpectedPreferredItemId =
		SourceItem
			? SourceItem->GetItemId()
			: FRpgInventoryItemId();
	Request.ContextItemId = Request.ExpectedPreferredItemId;
	MutateBinding(MoveTemp(Request));
}

void FRpgInventoryQuickAccessActionHandler::BindCarrySlot(
	int32 ActionBarSlotIndex,
	FRpgInventorySlotAddress CarrySlotAddress)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	const URpgInventoryItemInstance* SourceItem = InventoryLayout
		? InventoryLayout->GetItemInSlotAddress(
			CarrySlotAddress)
		: nullptr;

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation =
		ERpgQuickAccessMutationOperation::BindCarry;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.SourceAddress = CarrySlotAddress;
	if (InventoryLayout)
	{
		InventoryLayout->TryGetSemanticRoleForAddress(
			CarrySlotAddress,
			Request.ExpectedCarrySemanticRole);
	}
	Request.ContextItemId =
		SourceItem
			? SourceItem->GetItemId()
			: FRpgInventoryItemId();
	MutateBinding(MoveTemp(Request));
}

void FRpgInventoryQuickAccessActionHandler::ClearCarryBinding(
	int32 ActionBarSlotIndex,
	FGameplayTag ExpectedCarrySemanticRole)
{
	URpgActionBarComponent* ActionBar = FindActionBar();
	const FRpgActionBarSlot CurrentSlot = ActionBar
		? ActionBar->GetSlot(ActionBarSlotIndex)
		: FRpgActionBarSlot();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	const URpgInventoryItemInstance* ContextItem =
		InventoryLayout && CurrentSlot.SlotAddress.IsValid()
			? InventoryLayout->GetItemInSlotAddress(
				CurrentSlot.SlotAddress)
			: nullptr;

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation =
		ERpgQuickAccessMutationOperation::ClearCarry;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.ExpectedCarrySemanticRole =
		ExpectedCarrySemanticRole;
	Request.ContextItemId =
		ContextItem
			? ContextItem->GetItemId()
			: FRpgInventoryItemId();
	MutateBinding(MoveTemp(Request));
}

void FRpgInventoryQuickAccessActionHandler::
	ClearConsumableBinding(
		int32 ActionBarSlotIndex,
		TSubclassOf<URpgInventoryItemDefinition>
			ExpectedConsumableDefinition)
{
	URpgActionBarComponent* ActionBar = FindActionBar();
	const FRpgActionBarSlot CurrentSlot = ActionBar
		? ActionBar->GetSlot(ActionBarSlotIndex)
		: FRpgActionBarSlot();

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.Operation =
		ERpgQuickAccessMutationOperation::ClearConsumable;
	Request.SlotIndex = ActionBarSlotIndex;
	Request.ExpectedConsumableDefinition =
		ExpectedConsumableDefinition;
	Request.ExpectedPreferredItemId =
		CurrentSlot.PreferredItemId;
	Request.ContextItemId = CurrentSlot.PreferredItemId;
	MutateBinding(MoveTemp(Request));
}
