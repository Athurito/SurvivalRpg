#include "RpgGameplayAbility_Collect.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
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
	int32 GetCollectAbilityMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		return URpgInventoryManagerComponent::
			GetEffectiveMaxStackSizeForDefinition(ItemDef);
	}

	bool CanCollectAbilityInventoryAcceptPickup(URpgInventoryManagerComponent* InventoryComponent, const FInventoryPickup& PickupInventory)
	{
		if (!InventoryComponent || (PickupInventory.Templates.IsEmpty() && PickupInventory.Instances.IsEmpty()))
		{
			return false;
		}

		for (const FPickupTemplate& Template : PickupInventory.Templates)
		{
			if (!Template.ItemDef || Template.StackCount <= 0)
			{
				return false;
			}
		}

		for (const FPickupInstance& Instance : PickupInventory.Instances)
		{
			if (!Instance.Item || !InventoryComponent->CanBootstrapItemInstance(Instance.Item))
			{
				return false;
			}
		}

		if (InventoryComponent->IsCapacityUnlimited())
		{
			return true;
		}

		TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> ExistingFreeStackSpaceByDefinition;
		for (const FRpgInventoryEntryView& Entry : InventoryComponent->GetAllEntries())
		{
			const TSubclassOf<URpgInventoryItemDefinition> EntryDefinition = Entry.Instance ? Entry.Instance->GetItemDef() : nullptr;
			const int32 MaxStackSize = GetCollectAbilityMaxStackSizeForDefinition(EntryDefinition);
			if (EntryDefinition && MaxStackSize > 1)
			{
				ExistingFreeStackSpaceByDefinition.FindOrAdd(EntryDefinition) += FMath::Max(0, MaxStackSize - Entry.StackCount);
			}
		}

		TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> RequestedTemplateCounts;
		for (const FPickupTemplate& Template : PickupInventory.Templates)
		{
			RequestedTemplateCounts.FindOrAdd(Template.ItemDef) += Template.StackCount;
		}

		int32 RequiredNewEntries = 0;
		for (const FPickupInstance& Instance : PickupInventory.Instances)
		{
			RequiredNewEntries += InventoryComponent->GetRequiredNewEntryCountForItemInstance(Instance.Item, 1);
		}

		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& RequestedTemplateCount : RequestedTemplateCounts)
		{
			const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = RequestedTemplateCount.Key;
			const int32 MaxStackSize = GetCollectAbilityMaxStackSizeForDefinition(ItemDefinition);
			int32 RemainingCount = RequestedTemplateCount.Value;

			if (MaxStackSize > 1)
			{
				int32& ExistingFreeStackSpace = ExistingFreeStackSpaceByDefinition.FindOrAdd(ItemDefinition);
				const int32 FilledExistingStackCount = FMath::Min(ExistingFreeStackSpace, RemainingCount);
				ExistingFreeStackSpace -= FilledExistingStackCount;
				RemainingCount -= FilledExistingStackCount;
			}

			if (RemainingCount > 0)
			{
				RequiredNewEntries += FMath::DivideAndRoundUp(RemainingCount, MaxStackSize);
			}
		}

		return RequiredNewEntries <= InventoryComponent->GetFreeEntryCount();
	}

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
	if (!CanAddPickupToInventory(InventoryComponent, PickupInventory))
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

	TArray<URpgInventoryItemInstance*> AddedItems;
	if (!AddPickupToInventory(InventoryComponent, PickupInventory, AddedItems))
	{
		if (ARpgPlayerController* PlayerController = FindPlayerControllerForActor(InteractingActor))
		{
			if (URpgEquipmentLoadoutComponent* EquipmentLoadout = PlayerController->GetEquipmentLoadoutComponent())
			{
				EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory();
			}
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
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

bool URpgGameplayAbility_Collect::CanAddPickupToInventory(URpgInventoryManagerComponent* InventoryComponent, const FInventoryPickup& PickupInventory)
{
	return CanCollectAbilityInventoryAcceptPickup(InventoryComponent, PickupInventory);
}

bool URpgGameplayAbility_Collect::AddPickupToInventory(URpgInventoryManagerComponent* InventoryComponent, const FInventoryPickup& PickupInventory, TArray<URpgInventoryItemInstance*>& OutAddedItems)
{
	if (!CanAddPickupToInventory(InventoryComponent, PickupInventory))
	{
		return false;
	}
	const FRpgInventoryGraphSaveData InventoryBefore = InventoryComponent->ExportInventoryGraph();
	if (InventoryBefore.Items.Num() != InventoryComponent->GetAllEntries().Num())
	{
		return false;
	}
	auto Rollback = [InventoryComponent, &InventoryBefore, &OutAddedItems]()
	{
		FRpgInventoryMutationResult RollbackResult;
		InventoryComponent->ImportInventoryGraph(InventoryBefore, RollbackResult);
		OutAddedItems.Reset();
		return false;
	};

	for (const FPickupTemplate& Template : PickupInventory.Templates)
	{
		if (Template.ItemDef != nullptr && Template.StackCount > 0)
		{
			const int32 PreviousCount = InventoryComponent->GetTotalItemCountByDefinition(Template.ItemDef);
			URpgInventoryItemInstance* AddedItem = InventoryComponent->GrantItemDefinition(Template.ItemDef, Template.StackCount);
			if (!AddedItem || InventoryComponent->GetTotalItemCountByDefinition(Template.ItemDef) != PreviousCount + Template.StackCount)
			{
				return Rollback();
			}
			OutAddedItems.AddUnique(AddedItem);
		}
	}

	for (const FPickupInstance& Instance : PickupInventory.Instances)
	{
		if (Instance.Item != nullptr)
		{
			URpgInventoryItemInstance* AddedItem =
				InventoryComponent->BootstrapItemInstance(Instance.Item);
			if (!AddedItem || !InventoryComponent->ContainsItemInstance(AddedItem))
			{
				return Rollback();
			}
			OutAddedItems.AddUnique(AddedItem);
		}
	}

	return true;
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
