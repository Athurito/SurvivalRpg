// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPickupable.h"

#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryManagerComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IPickupable)

class UActorComponent;

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

	// Once a dropped actor has promoted its replicated loot manager to canonical state,
	// its payload is only a presentation snapshot. Copying that snapshot would duplicate
	// item instances instead of removing them through the source-owned transfer path.
	if (const ARpgDroppedInventoryActor* DroppedInventoryActor =
			Cast<ARpgDroppedInventoryActor>(Pickup.GetObject());
		DroppedInventoryActor && DroppedInventoryActor->IsLootInventoryCanonical())
	{
		return false;
	}

	TArray<FRpgInventoryItemId> AffectedItemIds;
	return InventoryComponent->AddPickupBatch(
		Pickup->GetPickupInventory(),
		AffectedItemIds).IsSuccess();
}
