#include "RpgItemizationTypes.h"

#include "Math/UnrealMathUtility.h"
#include "RpgItemizationGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgItemizationTypes)

bool FRpgItemizationState::IsStructurallyValid() const
{
	if (!bGenerated)
	{
		return ItemLevel == 0 && Rarity == ERpgItemRarity::Common &&
			BaseStats.IsEmpty() && Affixes.IsEmpty();
	}

	if (ItemLevel <= 0 ||
		static_cast<uint8>(Rarity) > static_cast<uint8>(ERpgItemRarity::Epic) ||
		Affixes.Num() != GetRpgAffixCountForRarity(Rarity))
	{
		return false;
	}

	TSet<FGameplayTag> SeenBaseStats;
	for (const FRpgRolledItemStat& Stat : BaseStats)
	{
		if (!Stat.StatTag.IsValid() || !FMath::IsFinite(Stat.Value) ||
			SeenBaseStats.Contains(Stat.StatTag))
		{
			return false;
		}
		SeenBaseStats.Add(Stat.StatTag);
	}

	TSet<FName> SeenAffixes;
	for (const FRpgRolledItemAffix& Affix : Affixes)
	{
		if (Affix.AffixId.IsNone() || !Affix.StatTag.IsValid() ||
			!FMath::IsFinite(Affix.Value) || SeenAffixes.Contains(Affix.AffixId))
		{
			return false;
		}
		SeenAffixes.Add(Affix.AffixId);
	}

	return true;
}

float FRpgItemizationState::GetBaseValueForStat(FGameplayTag StatTag) const
{
	if (!StatTag.IsValid())
	{
		return 0.0f;
	}

	for (const FRpgRolledItemStat& Stat : BaseStats)
	{
		if (Stat.StatTag == StatTag)
		{
			return Stat.Value;
		}
	}
	return 0.0f;
}

float FRpgItemizationState::GetTotalValueForStat(FGameplayTag StatTag) const
{
	float Total = GetBaseValueForStat(StatTag);
	if (!StatTag.IsValid())
	{
		return Total;
	}

	for (const FRpgRolledItemAffix& Affix : Affixes)
	{
		if (Affix.StatTag == StatTag)
		{
			Total += Affix.Value;
		}
	}
	return Total;
}

int32 GetRpgAffixCountForRarity(ERpgItemRarity Rarity)
{
	switch (Rarity)
	{
	case ERpgItemRarity::Common:
		return 0;
	case ERpgItemRarity::Uncommon:
		return 1;
	case ERpgItemRarity::Rare:
		return 2;
	case ERpgItemRarity::Epic:
		return 3;
	default:
		return 0;
	}
}

FText GetRpgItemStatDisplayName(const FGameplayTag& StatTag)
{
	using namespace RpgItemizationGameplayTags;
	if (StatTag == Item_Stat_WeaponDamage) { return NSLOCTEXT("RpgItemization", "WeaponDamage", "Damage"); }
	if (StatTag == Item_Stat_WeaponStagger) { return NSLOCTEXT("RpgItemization", "WeaponStagger", "Stagger"); }
	if (StatTag == Item_Stat_Armor) { return NSLOCTEXT("RpgItemization", "Armor", "Armor"); }
	if (StatTag == Item_Stat_Strength) { return NSLOCTEXT("RpgItemization", "Strength", "Strength"); }
	if (StatTag == Item_Stat_Intelligence) { return NSLOCTEXT("RpgItemization", "Intelligence", "Intelligence"); }
	if (StatTag == Item_Stat_Resilience) { return NSLOCTEXT("RpgItemization", "Resilience", "Resilience"); }
	if (StatTag == Item_Stat_Vitality) { return NSLOCTEXT("RpgItemization", "Vitality", "Vitality"); }
	if (StatTag == Item_Stat_ArmorPenetration) { return NSLOCTEXT("RpgItemization", "ArmorPenetration", "Armor Penetration"); }
	if (StatTag == Item_Stat_CriticalHitChance) { return NSLOCTEXT("RpgItemization", "CriticalHitChance", "Critical Hit Chance"); }
	if (StatTag == Item_Stat_CriticalHitDamage) { return NSLOCTEXT("RpgItemization", "CriticalHitDamage", "Critical Hit Damage"); }
	if (StatTag == Item_Stat_CriticalHitResistance) { return NSLOCTEXT("RpgItemization", "CriticalHitResistance", "Critical Hit Resistance"); }
	if (StatTag == Item_Stat_MaxStamina) { return NSLOCTEXT("RpgItemization", "MaxStamina", "Maximum Stamina"); }
	return FText::FromName(StatTag.GetTagName());
}
