#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_OpenBaseStorageStation.generated.h"

class ARpgPlayerController;
class URpgBaseStorageStationComponent;
class URpgInventoryManagerComponent;

/**
 * Opens the base terminal/resource-unit screen for an interacted base storage station.
 *
 * The ability is local-only because it creates UI only. Deposits, withdrawals, sorting, and
 * upgrade installation remain server-authoritative through URpgInventoryUiActionComponent.
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
