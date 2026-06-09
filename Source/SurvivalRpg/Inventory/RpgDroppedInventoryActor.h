#pragma once

#include "SurvivalRpg/Core/RpgWorldCollectable.h"

#include "RpgDroppedInventoryActor.generated.h"

/**
 * Simple world pickup actor used for death drops, loot bundles, and scripted inventory piles.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgDroppedInventoryActor : public ARpgWorldCollectable
{
	GENERATED_BODY()

public:
	/** Sets the pickup contents before players interact with this actor. Server-authoritative runtime state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	void SetPickupInventory(const FInventoryPickup& NewPickupInventory);
};
