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

FRpgInventoryItemUseExecutionResult
	FRpgInventoryItemUseActionHandler::ExecuteUseRequest(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryUseRequest& Request)
{
	auto MakeResult = [](
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackUseCount)
	{
		FRpgInventoryItemUseExecutionResult ExecutionResult;
		ExecutionResult.Result = Result;
		ExecutionResult.Item = Item;
		ExecutionResult.FeedbackUseCount = FeedbackUseCount;
		return ExecutionResult;
	};

	if (!IsValid(Inventory) || !Request.ItemId.IsValid() ||
		Request.UseCount <= 0)
	{
		return MakeResult(
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			nullptr,
			Request.UseCount);
	}

	if (!CanAccessInventory(Inventory))
	{
		return MakeResult(
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Request.UseCount);
	}

	URpgInventoryItemInstance* Item =
		Inventory->FindItemById(Request.ItemId);
	if (!IsValid(Item))
	{
		return MakeResult(
			ERpgInventoryActionFeedbackResult::MissingItem,
			nullptr,
			Request.UseCount);
	}

	const int32 AvailableCount =
		Inventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return MakeResult(
			ERpgInventoryActionFeedbackResult::MissingItem,
			Item,
			Request.UseCount);
	}

	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	const FRpgInventoryUseCapabilityEvaluation UseCapability =
		FRpgInventoryItemCapabilities::EvaluateUse(
			Item,
			Inventory,
			PlayerInventory,
			AvailableCount,
			Request.UseCount);
	switch (UseCapability.Result)
	{
	case ERpgInventoryUseCapabilityResult::NotConfigured:
		return MakeResult(
			ERpgInventoryActionFeedbackResult::CannotUse,
			Item,
			Request.UseCount);

	case ERpgInventoryUseCapabilityResult::WrongInventory:
		return MakeResult(
			ERpgInventoryActionFeedbackResult::WrongInventory,
			Item,
			Request.UseCount);

	case ERpgInventoryUseCapabilityResult::InvalidRequest:
		return MakeResult(
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Item,
			Request.UseCount);

	case ERpgInventoryUseCapabilityResult::InsufficientQuantity:
		return MakeResult(
			ERpgInventoryActionFeedbackResult::MissingItem,
			Item,
			UseCapability.RequiredConsumeCount);

	case ERpgInventoryUseCapabilityResult::Available:
		break;

	default:
		return MakeResult(
			ERpgInventoryActionFeedbackResult::CannotUse,
			Item,
			Request.UseCount);
	}

	const URpgInventoryFragment_UsableItem* UsableFragment =
		UseCapability.UseContract;
	if (!UsableFragment || !UsableFragment->UseAbility)
	{
		return MakeResult(
			ERpgInventoryActionFeedbackResult::CannotUse,
			Item,
			Request.UseCount);
	}

	const int32 UseCount = Request.UseCount;
	const int32 ConsumeCount =
		UseCapability.RequiredConsumeCount;
	if (ConsumeCount > 0 &&
		!Inventory->CanConsumeItemById(
			Item->GetItemId(),
			ConsumeCount))
	{
		return MakeResult(
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			ConsumeCount);
	}

	URpgAbilitySystemComponent* AbilitySystem =
		FindPlayerAbilitySystem();
	if (!AbilitySystem)
	{
		return MakeResult(
			ERpgInventoryActionFeedbackResult::ServerRejected,
			Item,
			Request.UseCount);
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

		return MakeResult(
			ERpgInventoryActionFeedbackResult::AbilityRejected,
			Item,
			Request.UseCount);
	}

	if (UsableFragment->bConsumeOnActivationAccepted &&
		ConsumeCount > 0)
	{
		if (!UseContext->TryConsume())
		{
			return MakeResult(
				ERpgInventoryActionFeedbackResult::
					ServerRejected,
				Item,
				ConsumeCount);
		}
	}

	return MakeResult(
		ERpgInventoryActionFeedbackResult::Success,
		Item,
		ConsumeCount);
}
