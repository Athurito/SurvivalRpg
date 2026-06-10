#include "RpgGameplayAbility_Collect.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Collect)

namespace
{
	int32 GetCollectAbilityMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		const URpgInventoryFragment_ItemTraits* Traits = ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
		return Traits ? Traits->GetMaxStackSize() : 1;
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
			if (!Instance.Item)
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

	const FInventoryPickup PickupInventory = Pickup->GetPickupInventory();
	if (!CanAddPickupToInventory(InventoryComponent, PickupInventory))
	{
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
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bAddCollectedEquippableItemsToQuickBar)
	{
		if (ARpgPlayerController* PlayerController = FindPlayerControllerForActor(InteractingActor))
		{
			AddEquippableItemsToQuickBar(PlayerController->GetQuickBarComponent(), AddedItems, bActivateFirstQuickBarSlot);
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

	for (const FPickupTemplate& Template : PickupInventory.Templates)
	{
		if (Template.ItemDef != nullptr && Template.StackCount > 0)
		{
			if (URpgInventoryItemInstance* AddedItem = InventoryComponent->AddItemDefinition(Template.ItemDef, Template.StackCount))
			{
				OutAddedItems.Add(AddedItem);
			}
		}
	}

	for (const FPickupInstance& Instance : PickupInventory.Instances)
	{
		if (Instance.Item != nullptr)
		{
			InventoryComponent->AddItemInstance(Instance.Item);
			OutAddedItems.Add(Instance.Item);
		}
	}

	return true;
}

void URpgGameplayAbility_Collect::AddEquippableItemsToQuickBar(URpgQuickBarComponent* QuickBarComponent, const TArray<URpgInventoryItemInstance*>& AddedItems, bool bActivateFirstSlot)
{
	if (QuickBarComponent == nullptr)
	{
		return;
	}

	bool bActivatedSlot = false;
	for (URpgInventoryItemInstance* AddedItem : AddedItems)
	{
		const URpgInventoryFragment_EquippableItem* EquippableFragment = AddedItem ? AddedItem->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() : nullptr;
		const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
		const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
		if (EquipmentCDO == nullptr)
		{
			continue;
		}

		const ERpgEquipmentSlot EquipmentSlot = EquipmentCDO->GetDefaultEquipSlot();
		int32 SlotIndex = INDEX_NONE;

		const int32 ActiveSlotIndex = QuickBarComponent->GetActiveSlotIndex();
		if (QuickBarComponent->GetItemInLoadoutSlot(ActiveSlotIndex, EquipmentSlot) == nullptr)
		{
			SlotIndex = ActiveSlotIndex;
		}
		else
		{
			const TArray<FRpgQuickBarLoadoutSlot> LoadoutSlots = QuickBarComponent->GetLoadoutSlots();
			for (int32 Index = 0; Index < LoadoutSlots.Num(); ++Index)
			{
				if (LoadoutSlots[Index].GetItemForSlot(EquipmentSlot) == nullptr)
				{
					SlotIndex = Index;
					break;
				}
			}
		}

		if (SlotIndex == INDEX_NONE)
		{
			return;
		}

		QuickBarComponent->AddItemToLoadoutSlot(SlotIndex, EquipmentSlot, AddedItem);
		if (bActivateFirstSlot && !bActivatedSlot)
		{
			QuickBarComponent->SetActiveSlotIndex(SlotIndex);
			bActivatedSlot = true;
		}
	}
}
