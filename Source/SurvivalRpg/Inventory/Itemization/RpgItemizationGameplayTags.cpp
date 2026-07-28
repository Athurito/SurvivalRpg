#include "RpgItemizationGameplayTags.h"

namespace RpgItemizationGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_WeaponDamage, "Item.Stat.WeaponDamage", "Instance-specific weapon damage used only by the associated weapon ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_WeaponStagger, "Item.Stat.WeaponStagger", "Instance-specific weapon stagger damage used only by the associated weapon ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Armor, "Item.Stat.Armor", "Armor granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Strength, "Item.Stat.Strength", "Strength granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Intelligence, "Item.Stat.Intelligence", "Intelligence granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Resilience, "Item.Stat.Resilience", "Resilience granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Vitality, "Item.Stat.Vitality", "Vitality granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_ArmorPenetration, "Item.Stat.ArmorPenetration", "Armor penetration granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_CriticalHitChance, "Item.Stat.CriticalHitChance", "Critical-hit chance granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_CriticalHitDamage, "Item.Stat.CriticalHitDamage", "Critical-hit damage granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_CriticalHitResistance, "Item.Stat.CriticalHitResistance", "Critical-hit resistance granted while the item is equipped.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_MaxStamina, "Item.Stat.MaxStamina", "Maximum stamina granted while the item is equipped.");

	bool IsSupportedItemStat(const FGameplayTag& StatTag)
	{
		return StatTag == Item_Stat_WeaponDamage ||
			StatTag == Item_Stat_WeaponStagger ||
			StatTag == Item_Stat_Armor ||
			StatTag == Item_Stat_Strength ||
			StatTag == Item_Stat_Intelligence ||
			StatTag == Item_Stat_Resilience ||
			StatTag == Item_Stat_Vitality ||
			StatTag == Item_Stat_ArmorPenetration ||
			StatTag == Item_Stat_CriticalHitChance ||
			StatTag == Item_Stat_CriticalHitDamage ||
			StatTag == Item_Stat_CriticalHitResistance ||
			StatTag == Item_Stat_MaxStamina;
	}
}
