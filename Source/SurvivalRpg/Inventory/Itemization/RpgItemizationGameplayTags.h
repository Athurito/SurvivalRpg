#pragma once

#include "NativeGameplayTags.h"

/** Native stat identifiers shared by item rolls, equipment effects, abilities, and UI. */
namespace RpgItemizationGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_WeaponDamage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_WeaponStagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_Armor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_Strength);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_Intelligence);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_Resilience);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_Vitality);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_ArmorPenetration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_CriticalHitChance);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_CriticalHitDamage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_CriticalHitResistance);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_MaxStamina);

	/** Returns whether StatTag belongs to the numeric catalog supported by first-slice itemization gameplay. */
	SURVIVALRPG_API bool IsSupportedItemStat(const FGameplayTag& StatTag);
}
