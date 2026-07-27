#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_OpenBaseStorageStation.generated.h"

class ARpgPlayerController;
class URpgBaseStorageStationComponent;
class URpgInventoryManagerComponent;

/**
 * Opens the base terminal/resource-unit screen for an interacted base storage station.
 *
 * The ability validates the station on the server before opening local UI through a reliable
 * owning-client RPC. Deposits, withdrawals, sorting, and upgrades remain server-authoritative.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_OpenBaseStorageStation : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_OpenBaseStorageStation(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	static ARpgPlayerController* FindPlayerControllerForActor(AActor* Actor);
	static URpgInventoryManagerComponent* FindPlayerInventory(ARpgPlayerController* PlayerController);
	static URpgBaseStorageStationComponent* FindStorageStationComponent(AActor* Actor);
};
