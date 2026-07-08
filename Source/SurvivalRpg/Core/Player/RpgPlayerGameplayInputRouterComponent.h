#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "RpgPlayerGameplayInputRouterComponent.generated.h"

/**
 * Controller-owned router for systemic gameplay hotkeys that are not direct ASC ability inputs.
 *
 * Pawn input binding forwards pressed/released gameplay tags here. The router keeps actionbar,
 * weapon-ability, and future player-controller-owned input features out of the pawn component.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPlayerGameplayInputRouterComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgPlayerGameplayInputRouterComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Routes a pressed systemic gameplay input tag such as InputTag.ActionBar.Slot.1 or InputTag.Weapon.Ability.1. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input")
	void HandleGameplayInputPressed(FGameplayTag InputTag);

	/** Routes a released systemic gameplay input tag such as InputTag.ActionBar.Slot.1 or InputTag.Weapon.Ability.1. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input")
	void HandleGameplayInputReleased(FGameplayTag InputTag);

private:
	static int32 GetActionBarSlotIndexFromInputTag(FGameplayTag InputTag);
	static int32 GetWeaponAbilitySlotIndexFromInputTag(FGameplayTag InputTag);
};
