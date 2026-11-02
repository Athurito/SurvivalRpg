#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"

#include "RpgEmbeddedProjectileSubsystem.generated.h"

class ARpgEmbeddedProjectilePickup;
class UPrimitiveComponent;
class URpgInventoryItemDefinition;

/**
 * Server-side order of the temporary projectile pickups in one world (ranged combat roadmap RNG-03b). It enforces
 * MaxWorldArrowPickups by expiring the pickup closest to its deadline early, exactly like its normal expiry, and finds
 * the pickup a further projectile joins once its hit component is full. It owns no inventory, recovery or combat state:
 * each pickup's inventory stays the quantity owner.
 */
UCLASS(Config = Game)
class SURVIVALRPG_API URpgEmbeddedProjectileSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

	/** Server: adds a new pickup and expires the ones closest to their deadline while the world holds too many. */
	void RegisterPickup(ARpgEmbeddedProjectilePickup* Pickup);

	/** Server: forgets a pickup that is going. */
	void UnregisterPickup(ARpgEmbeddedProjectilePickup* Pickup);

	/**
	 * Server: the pickup a projectile at Location joins, or null when it gets its own: null while Host holds fewer than
	 * MaxSeparatePerHost pickups, otherwise the nearest pickup of Ammunition on Host within GroupRadius, if any.
	 */
	ARpgEmbeddedProjectilePickup* FindGroup(
		const UPrimitiveComponent* Host,
		const FVector& Location,
		TSubclassOf<URpgInventoryItemDefinition> Ammunition,
		int32 MaxSeparatePerHost,
		float GroupRadius) const;

	/** Server: pickups the world holds now. */
	int32 GetPickupCount() const;

	int32 GetMaxWorldArrowPickups() const { return MaxWorldArrowPickups; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetMaxWorldArrowPickupsForTests(int32 NewMax) { MaxWorldArrowPickups = NewMax; }
#endif

private:
	/**
	 * Most temporary projectile pickups one world holds. A new pickup beyond it expires the one closest to its deadline.
	 * Project config (DefaultGame.ini), at least 1.
	 */
	UPROPERTY(Config)
	int32 MaxWorldArrowPickups = 100;

	TArray<TWeakObjectPtr<ARpgEmbeddedProjectilePickup>> Pickups;
};
