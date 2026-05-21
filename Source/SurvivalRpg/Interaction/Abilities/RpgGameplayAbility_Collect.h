#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_Collect.generated.h"

class ARpgPlayerController;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgQuickBarComponent;
struct FInventoryPickup;

/**
 * Adds a pickupable interaction target to the instigating player's inventory.
 *
 * Optional quickbar slotting is kept data-driven per ability asset so normal
 * harvest pickups can stay inventory-only while test weapons can be equipped
 * immediately without character-side hardcoding.
 */
UCLASS(Abstract)
class SURVIVALRPG_API URpgGameplayAbility_Collect : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_Collect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collect")
	bool bDestroyCollectedActor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collect|QuickBar")
	bool bAddCollectedEquippableItemsToQuickBar = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collect|QuickBar", meta = (EditCondition = "bAddCollectedEquippableItemsToQuickBar"))
	bool bActivateFirstQuickBarSlot = false;

private:
	static URpgInventoryManagerComponent* FindInventoryManagerForActor(AActor* Actor);
	static ARpgPlayerController* FindPlayerControllerForActor(AActor* Actor);
	static void AddPickupToInventory(URpgInventoryManagerComponent* InventoryComponent, const FInventoryPickup& PickupInventory, TArray<URpgInventoryItemInstance*>& OutAddedItems);
	static void AddEquippableItemsToQuickBar(URpgQuickBarComponent* QuickBarComponent, const TArray<URpgInventoryItemInstance*>& AddedItems, bool bActivateFirstSlot);
};
