#pragma once

#include "RpgInventoryItemDefinition.h"

#include "RpgInventoryFragment_Ammunition.generated.h"

class ARpgCombatProjectile;

/**
 * Static item data that makes an item ammunition for projectile shots, such as the standard arrow.
 *
 * A shot (URpgAbilityTask_ProjectileShot) consumes one item of the definition its shot definition names and launches
 * this fragment's projectile class. The projectile class carries the flight, damage effect and impact recovery policy,
 * so arrow variants differ by the projectile they name, never by a fragment class per arrow type.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_Ammunition final : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Projectile one item of this ammunition launches. Designer-owned static data; required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammunition")
	TSubclassOf<ARpgCombatProjectile> ProjectileClass;

	/** Returns the projectile class of ItemDef's Ammunition fragment, or null when ItemDef is no ammunition. */
	static TSubclassOf<ARpgCombatProjectile> FindProjectileClass(TSubclassOf<URpgInventoryItemDefinition> ItemDef);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
