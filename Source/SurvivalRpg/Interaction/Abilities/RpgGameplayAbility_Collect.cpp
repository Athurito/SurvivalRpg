#include "RpgGameplayAbility_Collect.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Collect)

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

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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

	TArray<URpgInventoryItemInstance*> AddedItems;
	AddPickupToInventory(InventoryComponent, Pickup->GetPickupInventory(), AddedItems);

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

void URpgGameplayAbility_Collect::AddPickupToInventory(URpgInventoryManagerComponent* InventoryComponent, const FInventoryPickup& PickupInventory, TArray<URpgInventoryItemInstance*>& OutAddedItems)
{
	if (InventoryComponent == nullptr)
	{
		return;
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
		if (AddedItem == nullptr || AddedItem->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() == nullptr)
		{
			continue;
		}

		const int32 SlotIndex = QuickBarComponent->GetNextFreeItemSlot();
		if (SlotIndex == INDEX_NONE)
		{
			return;
		}

		QuickBarComponent->AddItemToSlot(SlotIndex, AddedItem);
		if (bActivateFirstSlot && !bActivatedSlot)
		{
			QuickBarComponent->SetActiveSlotIndex(SlotIndex);
			bActivatedSlot = true;
		}
	}
}
