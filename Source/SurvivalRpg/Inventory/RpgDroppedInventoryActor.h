#pragma once

#include "SurvivalRpg/Core/RpgWorldCollectable.h"

#include "RpgDroppedInventoryActor.generated.h"

class URpgInventoryItemDefinition;

/**
 * Simple world pickup actor used for death drops, loot bundles, and scripted inventory piles.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgDroppedInventoryActor : public ARpgWorldCollectable
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;

	/** Sets the pickup contents before players interact with this actor. Server-authoritative runtime state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	void SetPickupInventory(const FInventoryPickup& NewPickupInventory);

	/** Adds count to an existing template pickup with the same definition, or creates one. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	bool MergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount);

	/** Returns true when this drop can merge another stack of the given item definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Drop")
	bool CanMergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

private:
	void EnsureDefaultPickupInteractionOption();
};
