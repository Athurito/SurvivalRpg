#include "RpgInventoryFragment_WeaponRunes.h"

#include "RpgWeaponRuneDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_WeaponRunes)

const URpgWeaponRuneDefinition* URpgInventoryFragment_WeaponRunes::FindEchoRune() const
{
	for (const URpgWeaponRuneDefinition* Rune : Runes)
	{
		if (Rune && Rune->EchoStrike.bEnabled)
		{
			return Rune;
		}
	}
	return nullptr;
}
