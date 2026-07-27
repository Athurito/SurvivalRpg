#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_OpenStorageContainer.generated.h"

class ARpgPlayerController;
class URpgInventoryContainerComponent;
class URpgInventoryManagerComponent;

/**
 * Opens the shared-storage inventory screen for an interacted container.
 *
 * The ability validates the target on the server, then asks the owning controller to
 * create local UI. Inventory transfers remain server-authoritative.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_OpenStorageContainer : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_OpenStorageContainer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	static ARpgPlayerController* FindPlayerControllerForActor(AActor* Actor);
	static URpgInventoryManagerComponent* FindPlayerInventory(ARpgPlayerController* PlayerController);
	static URpgInventoryContainerComponent* FindContainerComponent(AActor* Actor);
};
