// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPickupable.h"

#include "RpgInventoryManagerComponent.h"
#include "GameFramework/Actor.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IPickupable)

class URpgInventoryManagerComponent;
class UActorComponent;

namespace
{
	int32 GetPickupableMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		const URpgInventoryFragment_ItemTraits* Traits = ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
		return Traits ? Traits->GetMaxStackSize() : 1;
	}

	bool CanPickupableInventoryAcceptPickup(URpgInventoryManagerComponent* InventoryComponent, const FInventoryPickup& PickupInventory)
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
			const int32 MaxStackSize = GetPickupableMaxStackSizeForDefinition(EntryDefinition);
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
			const int32 MaxStackSize = GetPickupableMaxStackSizeForDefinition(ItemDefinition);
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

UPickupableStatics::UPickupableStatics()
	: Super(FObjectInitializer::Get())
{
}

TScriptInterface<IPickupable> UPickupableStatics::GetFirstPickupableFromActor(AActor* Actor)
{
	// If the actor is directly pickupable, return that.
	TScriptInterface<IPickupable> PickupableActor(Actor);
	if (PickupableActor)
	{
		return PickupableActor;
	}

	// If the actor isn't pickupable, it might have a component that has a pickupable interface.
	TArray<UActorComponent*> PickupableComponents = Actor ? Actor->GetComponentsByInterface(UPickupable::StaticClass()) : TArray<UActorComponent*>();
	if (PickupableComponents.Num() > 0)
	{
		// Get first pickupable, if the user needs more sophisticated pickup distinction, will need to be solved elsewhere.
		return TScriptInterface<IPickupable>(PickupableComponents[0]);
	}

	return TScriptInterface<IPickupable>();
}

bool UPickupableStatics::AddPickupToInventory(URpgInventoryManagerComponent* InventoryComponent, TScriptInterface<IPickupable> Pickup)
{
	if (!InventoryComponent || !Pickup)
	{
		return false;
	}

	const FInventoryPickup& PickupInventory = Pickup->GetPickupInventory();
	if (!CanPickupableInventoryAcceptPickup(InventoryComponent, PickupInventory))
	{
		return false;
	}

	for (const FPickupTemplate& Template : PickupInventory.Templates)
	{
		InventoryComponent->AddItemDefinition(Template.ItemDef, Template.StackCount);
	}

	for (const FPickupInstance& Instance : PickupInventory.Instances)
	{
		InventoryComponent->AddItemInstance(Instance.Item);
	}

	return true;
}
