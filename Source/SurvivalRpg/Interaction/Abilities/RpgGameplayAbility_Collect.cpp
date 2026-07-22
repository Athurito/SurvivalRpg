#include "RpgGameplayAbility_Collect.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Collect)

namespace
{
	bool TransferDroppedInventoryGraph(
		URpgInventoryManagerComponent* LootInventory,
		URpgInventoryManagerComponent* PlayerInventory,
		ARpgPlayerController* PlayerController,
		TArray<FRpgInventoryItemId>& OutAddedItemIds)
	{
		if (!LootInventory || !PlayerInventory || !PlayerController)
		{
			return false;
		}

		TArray<FRpgInventoryContainerHandle> TargetContainers;
		if (const URpgPlayerInventoryLayoutComponent* Layout = PlayerController->GetPlayerInventoryLayoutComponent())
		{
			for (const FRpgInventorySlotGroupView& Group : Layout->GetSlotGroups())
			{
				if (Group.GroupKind == ERpgInventorySlotGroupKind::Content && Group.ContainerHandle.IsValid())
				{
					TargetContainers.AddUnique(Group.ContainerHandle);
				}
			}
		}
		if (TargetContainers.IsEmpty())
		{
			return false;
		}

		TArray<FRpgInventoryItemId> RootItemIds;
		for (const FRpgInventoryEntryView& Entry : LootInventory->GetAllEntries())
		{
			if (Entry.ItemId.IsValid() && Entry.Placement.GetContainerHandle().IsRoot())
			{
				RootItemIds.Add(Entry.ItemId);
			}
		}

		bool bTransferredAnything = false;
		for (const FRpgInventoryItemId& RootItemId : RootItemIds)
		{
			for (const FRpgInventoryContainerHandle& TargetContainer : TargetContainers)
			{
				URpgInventoryItemInstance* CurrentItem = LootInventory->FindItemById(RootItemId);
				const TArray<FRpgInventoryEntryView> CurrentEntries =
					LootInventory->GetAllEntries();
				const FRpgInventoryEntryView* SourceEntry =
					CurrentEntries.FindByPredicate(
						[RootItemId](const FRpgInventoryEntryView& Entry)
						{
							return Entry.ItemId == RootItemId;
						});
				if (!CurrentItem || !SourceEntry ||
					!SourceEntry->EntryId.IsValid() ||
					!SourceEntry->Placement.IsValid() ||
					SourceEntry->StackCount <= 0)
				{
					break;
				}

				const int32 CurrentStackCount = SourceEntry->StackCount;
				const bool bAllowPartialStackPickup =
					!CurrentItem->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() &&
					URpgInventoryManagerComponent::
						GetEffectiveMaxStackSizeForDefinition(
							CurrentItem->GetItemDef()) > 1;

				FRpgInventoryTransferIntent Intent;
				Intent.ItemId = RootItemId;
				Intent.ExpectedEntryId = SourceEntry->EntryId;
				Intent.ExpectedSourcePlacement = SourceEntry->Placement;
				Intent.ExpectedSourceQuantity = CurrentStackCount;
				Intent.TargetContainer = TargetContainer;
				Intent.Quantity = CurrentStackCount;
				Intent.EnsureRequestId();
				const FRpgInventoryMutationResult TransferResult = LootInventory->PickupItem(
					PlayerInventory,
					Intent,
					bAllowPartialStackPickup);
				if (!TransferResult.IsSuccess())
				{
					continue;
				}

				bTransferredAnything = true;
				for (const FRpgInventoryMutationDelta& Delta : TransferResult.Deltas)
				{
					if (Delta.ItemId.IsValid() && Delta.AfterContainer == TargetContainer)
					{
						OutAddedItemIds.AddUnique(Delta.ItemId);
					}
				}
			}
		}

		return bTransferredAnything;
	}
}

URpgGameplayAbility_Collect::URpgGameplayAbility_Collect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgGameplayAbility_Collect::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* InteractingActor = TriggerEventData ? const_cast<AActor*>(ToRawPtr(TriggerEventData->Instigator)) : nullptr;
	if (InteractingActor == nullptr && ActorInfo->AvatarActor.IsValid())
	{
		InteractingActor = ActorInfo->AvatarActor.Get();
	}

	AActor* TargetActor = TriggerEventData ? const_cast<AActor*>(ToRawPtr(TriggerEventData->Target)) : nullptr;
	if (TargetActor == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const TScriptInterface<IPickupable> Pickup = UPickupableStatics::GetFirstPickupableFromActor(TargetActor);
	URpgInventoryManagerComponent* InventoryComponent = FindInventoryManagerForActor(InteractingActor);
	if (!Pickup || InventoryComponent == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ARpgDroppedInventoryActor* DroppedInventoryActor =
			Cast<ARpgDroppedInventoryActor>(TargetActor);
		DroppedInventoryActor &&
		DroppedInventoryActor->IsLootInventoryCanonical())
	{
		URpgInventoryManagerComponent* LootInventory = DroppedInventoryActor->GetLootInventoryManager();
		ARpgPlayerController* PlayerController = FindPlayerControllerForActor(InteractingActor);
		if (!LootInventory || !PlayerController || !CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		TArray<FRpgInventoryItemId> AddedItemIds;
		const bool bTransferredAnything = TransferDroppedInventoryGraph(
			LootInventory,
			InventoryComponent,
			PlayerController,
			AddedItemIds);

		if (PlayerController->GetEquipmentLoadoutComponent())
		{
			if (bAssignCollectedEquippableItemsToEquipment && bTransferredAnything)
			{
				TArray<URpgInventoryItemInstance*> AddedItems;
				for (const FRpgInventoryItemId& ItemId : AddedItemIds)
				{
					if (URpgInventoryItemInstance* AddedItem = InventoryComponent->FindItemById(ItemId))
					{
						AddedItems.Add(AddedItem);
					}
				}
				AssignEquippableItemsToEquipment(
					InventoryComponent,
					PlayerController->GetInventoryUiActionComponent(),
					AddedItems);
			}
		}

		if (LootInventory->GetAllEntries().IsEmpty())
		{
			if (bDestroyCollectedActor && TargetActor->HasAuthority() && TargetActor != InteractingActor)
			{
				TargetActor->Destroy();
			}
		}
		else
		{
			PlayerController->ClientOpenLootInventory(InventoryComponent, LootInventory, DroppedInventoryActor);
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FInventoryPickup PickupInventory = Pickup->GetPickupInventory();
	if (!InventoryComponent->CanAddPickupBatch(PickupInventory))
	{
		if (ARpgDroppedInventoryActor* DroppedInventoryActor =
				Cast<ARpgDroppedInventoryActor>(TargetActor);
			DroppedInventoryActor &&
			DroppedInventoryActor->IsLootInventoryCanonical())
		{
			if (URpgInventoryManagerComponent* LootInventory = DroppedInventoryActor->GetLootInventoryManager())
			{
				if (ARpgPlayerController* PlayerController = FindPlayerControllerForActor(InteractingActor))
				{
					PlayerController->ClientOpenLootInventory(InventoryComponent, LootInventory, DroppedInventoryActor);
				}
			}
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TArray<FRpgInventoryItemId> AddedItemIds;
	const FRpgInventoryMutationResult PickupResult =
		InventoryComponent->AddPickupBatch(PickupInventory, AddedItemIds);
	if (!PickupResult.IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TArray<URpgInventoryItemInstance*> AddedItems;
	for (const FRpgInventoryItemId& ItemId : AddedItemIds)
	{
		if (URpgInventoryItemInstance* AddedItem =
				InventoryComponent->FindItemById(ItemId))
		{
			AddedItems.AddUnique(AddedItem);
		}
	}

	if (bAssignCollectedEquippableItemsToEquipment)
	{
		if (ARpgPlayerController* PlayerController = FindPlayerControllerForActor(InteractingActor))
		{
			AssignEquippableItemsToEquipment(
				InventoryComponent,
				PlayerController->GetInventoryUiActionComponent(),
				AddedItems);
		}
	}

	if (bDestroyCollectedActor && TargetActor->HasAuthority() && TargetActor != InteractingActor)
	{
		TargetActor->Destroy();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

URpgInventoryManagerComponent* URpgGameplayAbility_Collect::FindInventoryManagerForActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (URpgInventoryManagerComponent* InventoryComponent = Actor->FindComponentByClass<URpgInventoryManagerComponent>())
	{
		return InventoryComponent;
	}

	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			if (URpgInventoryManagerComponent* InventoryComponent = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
			{
				return InventoryComponent;
			}
		}

		if (AController* Controller = Pawn->GetController())
		{
			return FindInventoryManagerForActor(Controller);
		}
	}

	if (const AController* Controller = Cast<AController>(Actor))
	{
		if (APlayerState* PlayerState = Controller->PlayerState)
		{
			return PlayerState->FindComponentByClass<URpgInventoryManagerComponent>();
		}
	}

	return Actor->GetOwner() ? FindInventoryManagerForActor(Actor->GetOwner()) : nullptr;
}

ARpgPlayerController* URpgGameplayAbility_Collect::FindPlayerControllerForActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(Actor))
	{
		return PlayerController;
	}

	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		return Cast<ARpgPlayerController>(Pawn->GetController());
	}

	if (Actor->GetOwner())
	{
		return FindPlayerControllerForActor(Actor->GetOwner());
	}

	return nullptr;
}

void URpgGameplayAbility_Collect::AssignEquippableItemsToEquipment(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryUiActionComponent* InventoryActions,
	const TArray<URpgInventoryItemInstance*>& AddedItems)
{
	if (!Inventory || !InventoryActions)
	{
		return;
	}

	for (URpgInventoryItemInstance* AddedItem : AddedItems)
	{
		if (!AddedItem ||
			!AddedItem->FindFragmentByClass<URpgInventoryFragment_EquippableItem>())
		{
			continue;
		}

		// A previous synchronous equip may have swapped this later item into another placement.
		// Resolve each source snapshot immediately before submitting its own immutable command.
		const TArray<FRpgInventoryEntryView> Entries =
			Inventory->GetAllEntries();
		const FRpgInventoryEntryView* Entry =
			Entries.FindByPredicate(
				[AddedItem](
					const FRpgInventoryEntryView& Candidate)
				{
					return Candidate.Instance == AddedItem;
				});
		if (!Entry || !Entry->EntryId.IsValid() ||
			!Entry->Placement.IsValid() ||
			Entry->StackCount <= 0)
		{
			continue;
		}

		FRpgInventoryEquipmentIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = Entry->ItemId;
		Intent.ExpectedEntryId = Entry->EntryId;
		Intent.ExpectedSourcePlacement = Entry->Placement;
		Intent.ExpectedQuantity = Entry->StackCount;
		Intent.Operation =
			ERpgInventoryEquipmentIntentOperation::
				EquipDefaultAndActivate;
		InventoryActions->RequestApplyInventoryEquipmentIntent(
			Inventory,
			Intent);
	}
}
