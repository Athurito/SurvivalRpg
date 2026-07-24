#include "RpgInventoryUiActionDomainHandlers.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

AActor* FRpgInventoryUiActionDomainHandler::GetOwner() const
{
	return ActionComponent.GetOwner();
}

UWorld* FRpgInventoryUiActionDomainHandler::GetWorld() const
{
	return ActionComponent.GetWorld();
}

AActor* FRpgInventoryUiActionDomainHandler::GetRequestingActor() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	return OwnerController ? OwnerController->GetPawn() : GetOwner();
}

bool FRpgInventoryUiActionDomainHandler::CanAccessInventory(
	URpgInventoryManagerComponent* Inventory) const
{
	return ActionComponent.CanAccessInventory(Inventory);
}

bool FRpgInventoryUiActionDomainHandler::CanAccessBaseStorageStation(
	const URpgBaseStorageStationComponent* Station) const
{
	return ActionComponent.CanAccessBaseStorageStation(Station);
}

URpgInventoryManagerComponent*
FRpgInventoryUiActionDomainHandler::FindPlayerInventory() const
{
	return ActionComponent.FindPlayerInventory();
}

URpgEquipmentLoadoutComponent*
FRpgInventoryUiActionDomainHandler::FindEquipmentLoadout() const
{
	return ActionComponent.FindEquipmentLoadout();
}

URpgPlayerInventoryLayoutComponent*
FRpgInventoryUiActionDomainHandler::FindPlayerInventoryLayout() const
{
	return ActionComponent.FindPlayerInventoryLayout();
}

URpgActionBarComponent*
FRpgInventoryUiActionDomainHandler::FindActionBar() const
{
	return ActionComponent.FindActionBar();
}

URpgAbilitySystemComponent*
FRpgInventoryUiActionDomainHandler::FindPlayerAbilitySystem() const
{
	return ActionComponent.FindPlayerAbilitySystem();
}

bool FRpgInventoryUiActionDomainHandler::IsPlayerEquipmentPlacement(
	const FRpgInventoryGridPlacement& Placement) const
{
	return ActionComponent.IsPlayerEquipmentPlacement(Placement);
}

void FRpgInventoryUiActionDomainHandler::
	SyncEquipmentLoadoutFromGearSlots() const
{
	ActionComponent.SyncEquipmentLoadoutFromGearSlots();
}

void FRpgInventoryUiActionDomainHandler::
	SyncActiveHandsFromCarrySlots() const
{
	ActionComponent.SyncActiveHandsFromCarrySlots();
}

void FRpgInventoryUiActionDomainHandler::SendActionFeedback(
	FGameplayTag ActionTag,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FGuid& RequestId,
	FRpgInventoryItemId ItemId) const
{
	ActionComponent.SendActionFeedback(
		ActionTag,
		Result,
		Inventory,
		Item,
		StackCount,
		RequestId,
		ItemId);
}

bool FRpgInventoryUiActionDomainHandler::
	TryReplayRecentManualDropResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request)
{
	return ActionComponent.TryReplayRecentManualDropResult(
		Inventory,
		Request);
}

void FRpgInventoryUiActionDomainHandler::
	SendAndCacheManualDropFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount,
		URpgInventoryManagerComponent* TargetInventory)
{
	ActionComponent.SendAndCacheManualDropFeedback(
		Inventory,
		Request,
		Result,
		Item,
		FeedbackStackCount,
		TargetInventory);
}

UObject* FRpgInventoryUiActionDomainHandler::
	GetItemUseContextOuter() const
{
	return &ActionComponent;
}

FRpgInventoryUseConsumePreflight
FRpgInventoryUiActionDomainHandler::MakeUseConsumePreflight(
	TWeakObjectPtr<URpgInventoryManagerComponent> Inventory,
	FRpgInventoryItemId ItemId,
	int32 ConsumeCount,
	TSharedRef<bool> RequiresEquipmentCleanup) const
{
	URpgInventoryUiActionComponent* Component = &ActionComponent;
	return FRpgInventoryUseConsumePreflight::CreateWeakLambda(
		Component,
		[Component, Inventory, ItemId, ConsumeCount,
			RequiresEquipmentCleanup]()
		{
			URpgInventoryManagerComponent* CurrentInventory =
				Inventory.Get();
			URpgInventoryItemInstance* CurrentItem =
				CurrentInventory
					? CurrentInventory->FindItemById(ItemId)
					: nullptr;
			if (!CurrentItem)
			{
				return false;
			}

			FRpgInventoryGridPlacement CurrentPlacement;
			*RequiresEquipmentCleanup =
				CurrentInventory->GetItemPlacement(
					CurrentItem,
					CurrentPlacement) &&
				Component->IsPlayerEquipmentPlacement(
					CurrentPlacement);

			const int32 CurrentCount =
				CurrentInventory->GetItemStackCount(CurrentItem);
			if (ConsumeCount < CurrentCount)
			{
				return true;
			}
			return ConsumeCount == CurrentCount;
		});
}

FSimpleDelegate
FRpgInventoryUiActionDomainHandler::MakeUseConsumeSucceeded(
	TWeakObjectPtr<URpgInventoryManagerComponent> Inventory,
	FRpgInventoryItemId ItemId,
	TSharedRef<bool> RequiresEquipmentCleanup) const
{
	URpgInventoryUiActionComponent* Component = &ActionComponent;
	return FSimpleDelegate::CreateWeakLambda(
		Component,
		[Component, Inventory, ItemId,
			RequiresEquipmentCleanup]()
		{
			URpgInventoryManagerComponent* CurrentInventory =
				Inventory.Get();
			if (CurrentInventory &&
				CurrentInventory->FindItemById(ItemId))
			{
				// A delayed full consume may become partial after a merge.
				// Keep assignments until the concrete identity leaves.
				return;
			}

			if (!*RequiresEquipmentCleanup)
			{
				return;
			}

			Component->SyncEquipmentLoadoutFromGearSlots();
			Component->SyncActiveHandsFromCarrySlots();
		});
}

TSubclassOf<ARpgDroppedInventoryActor>
FRpgInventoryUiActionDomainHandler::GetManualDropActorClass() const
{
	return ActionComponent.ManualDropActorClass;
}

float FRpgInventoryUiActionDomainHandler::
	GetManualDropForwardDistance() const
{
	return ActionComponent.ManualDropForwardDistance;
}

float FRpgInventoryUiActionDomainHandler::GetManualDropUpOffset() const
{
	return ActionComponent.ManualDropUpOffset;
}

float FRpgInventoryUiActionDomainHandler::
	GetManualDropMergeRadius() const
{
	return ActionComponent.ManualDropMergeRadius;
}

void FRpgInventoryUiActionDomainHandler::RequestQuickTransferItem(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryQuickTransferRequest Request)
{
	ActionComponent.RequestQuickTransferItem_Implementation(
		SourceInventory,
		TargetInventory,
		MoveTemp(Request));
}
