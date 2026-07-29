#include "RpgInventoryUiActionDomainHandlers.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "UObject/UObjectIterator.h"

namespace
{
	void GatherUiActionCraftingStationsForOutputInventory(
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

		for (TObjectIterator<URpgCraftingStationComponent> It; It; ++It)
		{
			AddMatchingStation(*It);
		}
	}
}

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

bool FRpgInventoryUiActionDomainHandler::EvaluateInventoryAccess(
	URpgInventoryManagerComponent* Inventory) const
{
	if (!Inventory)
	{
		return false;
	}

	if (Inventory == FindPlayerInventory())
	{
		return true;
	}

	const AActor* RequestingActor = GetRequestingActor();
	TArray<const URpgCraftingStationComponent*> CraftingStations;
	GatherUiActionCraftingStationsForOutputInventory(
		Inventory,
		CraftingStations);
	if (!CraftingStations.IsEmpty())
	{
		// Shared output inventories fail closed unless every live claimant
		// authorizes the requesting actor.
		for (const URpgCraftingStationComponent* CraftingStation :
			CraftingStations)
		{
			if (!CraftingStation->CanActorAccess(RequestingActor))
			{
				return false;
			}
		}
		return true;
	}

	const AActor* InventoryOwner = Inventory->GetOwner();
	const URpgBaseStorageStationComponent* Station =
		InventoryOwner
			? InventoryOwner->FindComponentByClass<
				URpgBaseStorageStationComponent>()
			: nullptr;
	if (Station && Station->GetArmoryInventory() == Inventory)
	{
		return Station->CanActorAccess(RequestingActor);
	}

	const URpgInventoryContainerComponent* Container =
		InventoryOwner
			? InventoryOwner->FindComponentByClass<
				URpgInventoryContainerComponent>()
			: nullptr;
	return Container && Container->CanActorAccess(RequestingActor);
}

bool FRpgInventoryUiActionDomainHandler::CanAccessInventory(
	URpgInventoryManagerComponent* Inventory) const
{
	return EvaluateInventoryAccess(Inventory);
}

bool FRpgInventoryUiActionDomainHandler::IsUiTransferDirectionAllowed(
	const URpgInventoryManagerComponent* SourceInventory,
	const URpgInventoryManagerComponent* TargetInventory) const
{
	if (!SourceInventory || !TargetInventory)
	{
		return false;
	}

	if (SourceInventory == TargetInventory)
	{
		return true;
	}

	if (const AActor* TargetOwner = TargetInventory->GetOwner())
	{
		if (const URpgInventoryContainerComponent* Container =
				TargetOwner->FindComponentByClass<
					URpgInventoryContainerComponent>();
			Container && Container->GetInventoryManager() == TargetInventory &&
			!Container->CanReceiveTransferFrom(SourceInventory))
		{
			return false;
		}
	}

	// Crafting owns deposits into output buffers. UI actions may only
	// reorder an output internally or withdraw from it.
	TArray<const URpgCraftingStationComponent*> CraftingStations;
	GatherUiActionCraftingStationsForOutputInventory(
		TargetInventory,
		CraftingStations);
	return CraftingStations.IsEmpty();
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
	TryReplayRecentExactTransferResult(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent)
{
	return GetMutableActionComponent().TryReplayRecentExactTransferResult(
		SourceInventory,
		TargetInventory,
		Intent);
}

void FRpgInventoryUiActionDomainHandler::
	SendAndCacheExactTransferFeedback(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount)
{
	GetMutableActionComponent().SendAndCacheExactTransferFeedback(
		SourceInventory,
		TargetInventory,
		Intent,
		Result,
		Item,
		FeedbackStackCount);
}

bool FRpgInventoryUiActionDomainHandler::
	TryReplayRecentQuickTransferResult(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request)
{
	return GetMutableActionComponent().TryReplayRecentQuickTransferResult(
		SourceInventory,
		TargetInventory,
		Request);
}

void FRpgInventoryUiActionDomainHandler::
	SendAndCacheQuickTransferFeedback(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount)
{
	GetMutableActionComponent().SendAndCacheQuickTransferFeedback(
		SourceInventory,
		TargetInventory,
		Request,
		Result,
		Item,
		FeedbackStackCount);
}

bool FRpgInventoryUiActionDomainHandler::TryReplayRecentSplitResult(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventorySplitRequest& Request)
{
	return GetMutableActionComponent().TryReplayRecentSplitResult(
		Inventory,
		Request);
}

void FRpgInventoryUiActionDomainHandler::SendAndCacheSplitFeedback(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventorySplitRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	GetMutableActionComponent().SendAndCacheSplitFeedback(
		Inventory,
		Request,
		Result,
		Item,
		FeedbackStackCount);
}

bool FRpgInventoryUiActionDomainHandler::
	TryReplayRecentEquipmentIntentResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent)
{
	return GetMutableActionComponent().TryReplayRecentEquipmentIntentResult(
		Inventory,
		Intent);
}

void FRpgInventoryUiActionDomainHandler::
	SendAndCacheEquipmentIntentFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount)
{
	GetMutableActionComponent().SendAndCacheEquipmentIntentFeedback(
		Inventory,
		Intent,
		Result,
		Item,
		FeedbackStackCount);
}

bool FRpgInventoryUiActionDomainHandler::
	TryReplayRecentManualDropResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request)
{
	return GetMutableActionComponent().TryReplayRecentManualDropResult(
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
	GetMutableActionComponent().SendAndCacheManualDropFeedback(
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
	return &GetMutableActionComponent();
}

FRpgInventoryUseConsumePreflight
FRpgInventoryUiActionDomainHandler::MakeUseConsumePreflight(
	TWeakObjectPtr<URpgInventoryManagerComponent> Inventory,
	FRpgInventoryItemId ItemId,
	int32 ConsumeCount,
	TSharedRef<bool> RequiresEquipmentCleanup) const
{
	URpgInventoryUiActionComponent* Component =
		&GetMutableActionComponent();
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
	URpgInventoryUiActionComponent* Component =
		&GetMutableActionComponent();
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
