#pragma once

#include "GameplayEffect.h"

#include "RpgItemizationEquipmentEffect.generated.h"

/**
 * Infinite additive effect whose SetByCaller magnitudes mirror one equipped item's global rolls.
 * Weapon-local damage and stagger deliberately are not modifiers on this effect.
 */
UCLASS(NotBlueprintable)
class SURVIVALRPG_API URpgItemizationEquipmentEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URpgItemizationEquipmentEffect();
};
