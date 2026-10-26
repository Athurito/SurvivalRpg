#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "RpgInventoryFragment_WeaponRunes.generated.h"

class URpgWeaponRuneDefinition;

/**
 * Runes engraved on every instance of a weapon item. Designer-authored static data on the item definition; the
 * equipped weapon instance reads it on the server. Rune slots that change per item instance are later work and will
 * add to this list.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Weapon Runes"))
class SURVIVALRPG_API URpgInventoryFragment_WeaponRunes : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** The first rune with an enabled Echo block, or null. */
	const URpgWeaponRuneDefinition* FindEchoRune() const;

	/** Runes on the weapon, in authoring order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runes")
	TArray<TObjectPtr<const URpgWeaponRuneDefinition>> Runes;
};
