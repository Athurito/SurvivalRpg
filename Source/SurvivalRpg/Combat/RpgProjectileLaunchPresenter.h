#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RpgProjectileLaunchPresenter.generated.h"

class ARpgCombatProjectile;

/** Presentation of a weapon that shows its ammunition before the shot, such as a bow's nocked arrow. */
UINTERFACE(BlueprintType, MinimalAPI)
class URpgProjectileLaunchPresenter : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by an actor attached to a shooter, such as its bow. When a projectile's flight has just begun, the
 * projectile asks the actors attached to its instigator, on every machine with visuals, where its visuals start. Its
 * collision keeps the launched path; the visuals catch up with it on their own. Purely cosmetic: nothing here reaches
 * the server's outcome.
 */
class SURVIVALRPG_API IRpgProjectileLaunchPresenter
{
	GENERATED_BODY()

public:
	/**
	 * Cosmetic, on every machine with visuals: hands Projectile the ammunition this actor shows, such as the nocked
	 * arrow, and returns in OutVisualStart the world location of its tip. Return false when nothing is shown, or the shot
	 * is not this actor's. The presenter hides what it handed over until its next draw.
	 *
	 * The call may come late: the server launches while the shooter still draws, but a client receives the projectile
	 * after the replicated release, and the shooting client about a round trip after its own predicted release. A
	 * presenter therefore hands over the ammunition of the draw that ended, not a newer draw's, which keeps showing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rpg|Combat|Projectile")
	bool HandOffProjectileLaunch(ARpgCombatProjectile* Projectile, FVector& OutVisualStart);
};
