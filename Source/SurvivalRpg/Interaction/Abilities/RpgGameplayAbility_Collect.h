#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_Collect.generated.h"

class ARpgPlayerController;
class URpgInventoryUiActionComponent;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;

/**
 * Native authority and transaction mechanism for pickup and dropped-inventory collection.
 *
 * Optional equipment assignment is kept data-driven per ability asset so normal harvest pickups
 * can stay inventory-only while test weapons can be equipped immediately without pawn hardcoding.
 * Concrete GA assets own content identity and collection policy; native code preserves atomic inventory
 * mutations, source item identity, server equip intents, and the owning-client loot RPC.
 */
UCLASS(Abstract, Blueprintable)
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

	/** Designer policy: authority destroys a fully collected target after the inventory transaction succeeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collect")
	bool bDestroyCollectedActor = true;

	/**
	 * If true, collected equippable items are moved into a compatible Gear/Carry location and then activated.
	 * The authoritative inventory action may leave an item in normal content when no valid destination exists.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collect|Equipment")
	bool bAssignCollectedEquippableItemsToEquipment = false;

private:
	static URpgInventoryManagerComponent* FindInventoryManagerForActor(AActor* Actor);
	static ARpgPlayerController* FindPlayerControllerForActor(AActor* Actor);
	static void AssignEquippableItemsToEquipment(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryUiActionComponent* InventoryActions,
		const TArray<URpgInventoryItemInstance*>& AddedItems);
};
