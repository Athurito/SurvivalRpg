#pragma once

#include "RpgWeaponInstance.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySourceInterface.h"

#include "RpgRangedWeaponInstance.generated.h"

class UPhysicalMaterial;

/**
 * Ranged weapons are ability calculation sources, matching Lyra's split between
 * base weapon equipment and ranged weapon source data.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgRangedWeaponInstance : public URpgWeaponInstance, public IRpgAbilitySourceInterface
{
	GENERATED_BODY()

public:
	URpgRangedWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;
	virtual float GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;
};
