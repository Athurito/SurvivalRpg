#pragma once

#include "SurvivalRpg/Core/RpgWorldCollectable.h"

#include "RpgDroppedInventoryActor.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;

/**
 * Simple world pickup actor used for death drops, loot bundles, and scripted inventory piles.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgDroppedInventoryActor : public ARpgWorldCollectable
{
	GENERATED_BODY()

public:
	ARpgDroppedInventoryActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual FInventoryPickup GetPickupInventory() const override;

	/** Sets the pickup contents before players interact with this actor. Server-authoritative runtime state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	void SetPickupInventory(const FInventoryPickup& NewPickupInventory);

	/** Inventory shown when the player cannot auto-take the whole drop. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Drop")
	URpgInventoryManagerComponent* GetLootInventoryManager() const { return LootInventoryComponent; }

	/** Adds count to an existing template pickup with the same definition, or creates one. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	bool MergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount);

	/** Returns true when this drop can merge another stack of the given item definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Drop")
	bool CanMergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

private:
	void EnsureDefaultPickupInteractionOption();
	void PopulateLootInventoryFromPickup(const FInventoryPickup& PickupInventory);
	FInventoryPickup BuildPickupInventoryFromLootInventory() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryManagerComponent> LootInventoryComponent;
};
