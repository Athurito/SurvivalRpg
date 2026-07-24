#include "RpgInventoryUiActionDomainHandlers.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemCapabilities.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryItemUseContext.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_ApplyItemEffects.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "UObject/UObjectGlobals.h"

void FRpgInventoryItemUseActionHandler::ExecuteItemAction(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemActionRequest Request)
{
	const FGameplayTag ActionTag =
		Request.Intent == ERpgInventoryItemActionIntent::Use
			? RpgGameplayTags::Rpg_Inventory_Action_Use
			: Request.Intent ==
					ERpgInventoryItemActionIntent::MoveToCarry
				? RpgGameplayTags::
						Rpg_Inventory_Action_MoveToCarry
				: RpgGameplayTags::
						Rpg_Inventory_Action_EquipAndActivate;
	if (Request.Intent != ERpgInventoryItemActionIntent::Use)
	{
		SendActionFeedback(
			ActionTag,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return;
	}

	if (!Inventory || !CanAccessInventory(Inventory))
	{
		SendActionFeedback(
			ActionTag,
			ERpgInventoryActionFeedbackResult::NoAccess,
			Inventory,
			nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return;
	}

	URpgInventoryItemInstance* Item =
		Inventory->FindItemById(Request.ItemId);
	if (!Item)
	{
		SendActionFeedback(
			ActionTag,
			ERpgInventoryActionFeedbackResult::MissingItem,
			Inventory,
			nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return;
	}

	UseInventoryItem(
		Inventory,
		Item,
		Request.StackCount,
		Request.RequestId);
}

void FRpgInventoryItemUseActionHandler::UseInventoryItem(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FGuid& RequestId)
{
	const FRpgInventoryItemId ItemId =
		Item ? Item->GetItemId() : FRpgInventoryItemId();
	auto SendUseFeedback =
		[this, Inventory, Item, &RequestId, ItemId](
			ERpgInventoryActionFeedbackResult Result,
			int32 Count)
		{
			SendActionFeedback(
				RpgGameplayTags::Rpg_Inventory_Action_Use,
				Result,
				Inventory,
				Item,
				Count,
				RequestId,
				ItemId);
		};

	if (!Inventory || !Item)
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			StackCount);
		return;
	}

	if (!CanAccessInventory(Inventory))
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::NoAccess,
			StackCount);
		return;
	}

	const int32 AvailableCount =
		Inventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::MissingItem,
			StackCount);
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	const FRpgInventoryUseCapabilityEvaluation UseCapability =
		FRpgInventoryItemCapabilities::EvaluateUse(
			Item,
			Inventory,
			PlayerInventory,
			AvailableCount,
			StackCount);
	switch (UseCapability.Result)
	{
	case ERpgInventoryUseCapabilityResult::NotConfigured:
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::CannotUse,
			StackCount);
		return;

	case ERpgInventoryUseCapabilityResult::WrongInventory:
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::WrongInventory,
			StackCount);
		return;

	case ERpgInventoryUseCapabilityResult::InvalidRequest:
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			StackCount);
		return;

	case ERpgInventoryUseCapabilityResult::InsufficientQuantity:
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::MissingItem,
			UseCapability.RequiredConsumeCount);
		return;

	case ERpgInventoryUseCapabilityResult::Available:
		break;

	default:
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::CannotUse,
			StackCount);
		return;
	}

	const URpgInventoryFragment_UsableItem* UsableFragment =
		UseCapability.UseContract;
	const int32 UseCount = StackCount;
	const int32 ConsumeCount =
		UseCapability.RequiredConsumeCount;
	if (ConsumeCount > 0 &&
		!Inventory->CanConsumeItemById(
			Item->GetItemId(),
			ConsumeCount))
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::ServerRejected,
			ConsumeCount);
		return;
	}

	URpgAbilitySystemComponent* AbilitySystem =
		FindPlayerAbilitySystem();
	if (!AbilitySystem)
	{
		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::ServerRejected,
			StackCount);
		return;
	}

	AActor* AvatarActor = GetRequestingActor();
	FGameplayEventData EventData;
	EventData.EventTag =
		RpgGameplayTags::Rpg_Inventory_Action_Use;
	EventData.Instigator = AvatarActor;
	EventData.Target = AvatarActor;
	EventData.OptionalObject = Item;
	EventData.EventMagnitude = static_cast<float>(UseCount);

	URpgInventoryItemUseContext* UseContext =
		NewObject<URpgInventoryItemUseContext>(
			GetItemUseContextOuter());
	UseContext->Initialize(
		Inventory,
		Item,
		UseCount,
		ConsumeCount);
	const bool bConsumesFromPlayerInventory =
		Inventory == PlayerInventory && ConsumeCount > 0;
	if (bConsumesFromPlayerInventory)
	{
		const TWeakObjectPtr<URpgInventoryManagerComponent>
			WeakInventory = Inventory;
		const FRpgInventoryItemId UsedItemId =
			Item->GetItemId();
		const TSharedRef<bool> bRequiresEquipmentCleanup =
			MakeShared<bool>(false);
		UseContext->SetConsumePreflightCallback(
			MakeUseConsumePreflight(
				WeakInventory,
				UsedItemId,
				ConsumeCount,
				bRequiresEquipmentCleanup));
		UseContext->SetConsumeSucceededCallback(
			MakeUseConsumeSucceeded(
				WeakInventory,
				UsedItemId,
				bRequiresEquipmentCleanup));
	}

	const bool bUsesApplyEffectsContext =
		UsableFragment->UseAbility->IsChildOf(
			URpgGameplayAbility_ApplyItemEffects::StaticClass());
	if (bUsesApplyEffectsContext)
	{
		URpgGameplayAbility_ApplyItemEffects::
			RegisterPendingUseContext(
				AbilitySystem,
				Item,
				UseContext);
	}

	FGameplayAbilitySpec UseSpec(
		UsableFragment->UseAbility,
		FMath::Max(1, UsableFragment->AbilityLevel),
		INDEX_NONE,
		Item);
	const FGameplayAbilitySpecHandle ActivatedHandle =
		AbilitySystem->GiveAbilityAndActivateOnce(
			UseSpec,
			&EventData);
	if (!ActivatedHandle.IsValid())
	{
		if (bUsesApplyEffectsContext)
		{
			URpgGameplayAbility_ApplyItemEffects::
				ClearPendingUseContext(
					AbilitySystem,
					Item);
		}

		SendUseFeedback(
			ERpgInventoryActionFeedbackResult::AbilityRejected,
			StackCount);
		return;
	}

	if (UsableFragment->bConsumeOnActivationAccepted &&
		ConsumeCount > 0)
	{
		if (!UseContext->TryConsume())
		{
			SendUseFeedback(
				ERpgInventoryActionFeedbackResult::
					ServerRejected,
				ConsumeCount);
			return;
		}
	}

	SendUseFeedback(
		ERpgInventoryActionFeedbackResult::Success,
		ConsumeCount);
}
