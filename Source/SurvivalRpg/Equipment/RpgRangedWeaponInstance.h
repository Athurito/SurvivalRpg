#pragma once

#include "RpgWeaponInstance.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySourceInterface.h"
#include "SurvivalRpg/Combat/RpgProjectileShot.h"
#include "SurvivalRpg/Combat/RpgRangedAim.h"

#include "RpgRangedWeaponInstance.generated.h"

class UPhysicalMaterial;

/**
 * Equipment instance of a ranged weapon such as the bow. It carries the weapon's projectile shot (ammunition, draw,
 * damage and speed curves, costs, launch point), which its shot ability reads through Start Projectile Shot From Weapon.
 * A ranged weapon has no melee attack definitions and cannot block. Ranged weapons are also ability calculation
 * sources, matching Lyra's split between base weapon equipment and ranged weapon source data.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgRangedWeaponInstance : public URpgWeaponInstance, public IRpgAbilitySourceInterface
{
	GENERATED_BODY()

public:
	URpgRangedWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The weapon's projectile shot. Designer-tuned static data on the weapon instance Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Ranged")
	const FRpgProjectileShotDefinition& GetShotDefinition() const { return ShotDefinition; }

	/** The weapon's aim stance: movement cap and camera while aiming or drawing. Designer-tuned static data. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Ranged")
	const FRpgRangedAimDefinition& GetAimDefinition() const { return AimDefinition; }

	/** Where the weapon's shots take their ammunition from. Designer-tuned static data. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Ranged")
	ERpgProjectileAmmunitionPolicy GetAmmunitionPolicy() const { return AmmunitionPolicy; }

	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;
	virtual float GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;

#if WITH_EDITOR
	/** Validates the shot definition with the same rules the shot applies at runtime. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** See GetShotDefinition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ranged")
	FRpgProjectileShotDefinition ShotDefinition;

	/** See GetAimDefinition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ranged")
	FRpgRangedAimDefinition AimDefinition;

	/**
	 * ShooterInventory for every weapon a player can hold: each shot consumes one ammunition item. Unlimited only for NPC
	 * weapon content that never enters an inventory, such as an enemy loadout's bow: its shots need no items, and
	 * ShotDefinition.UnlimitedRecoveryBudget bounds the arrows they return. Overrides the policy the shot ability passes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ranged")
	ERpgProjectileAmmunitionPolicy AmmunitionPolicy = ERpgProjectileAmmunitionPolicy::ShooterInventory;
};
