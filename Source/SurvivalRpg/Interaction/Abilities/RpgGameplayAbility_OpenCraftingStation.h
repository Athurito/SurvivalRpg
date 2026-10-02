#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_OpenCraftingStation.generated.h"

class ARpgPlayerController;
class URpgCraftingStationComponent;
class URpgInventoryManagerComponent;

/**
 * Opens the crafting station screen for an interacted crafting station.
 *
 * The ability validates the station on the server before opening local UI through a
 * reliable owning-client RPC. Crafting commands remain server-authoritative.
 * Concrete GA assets own ability content; this native bridge enforces access before the client RPC.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_OpenCraftingStation : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_OpenCraftingStation(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	static ARpgPlayerController* FindPlayerControllerForActor(AActor* Actor);
	static URpgInventoryManagerComponent* FindPlayerInventory(ARpgPlayerController* PlayerController);
	static URpgCraftingStationComponent* FindCraftingStationComponent(AActor* Actor);
};
