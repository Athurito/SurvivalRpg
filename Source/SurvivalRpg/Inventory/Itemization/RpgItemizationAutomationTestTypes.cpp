#include "RpgItemizationAutomationTestTypes.h"

#include "RpgInventoryFragment_Itemization.h"
#include "RpgItemizationGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgItemizationAutomationTestTypes)

namespace RpgItemizationAutomationTestTypes
{
	FRpgItemAffixDefinition MakeAffix(
		FName AffixId,
		FGameplayTag StatTag,
		float Minimum,
		float Maximum)
	{
		FRpgItemAffixDefinition Affix;
		Affix.AffixId = AffixId;
		Affix.DisplayName = FText::FromName(AffixId);
		Affix.StatTag = StatTag;
		Affix.Weight = 1.0f;
		Affix.MinimumValue = FScalableFloat(Minimum);
		Affix.MaximumValue = FScalableFloat(Maximum);
		return Affix;
	}
}

URpgItemizationAutomationTestProfile::URpgItemizationAutomationTestProfile(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	URpgItemAffixPool* Pool =
		CreateDefaultSubobject<URpgItemAffixPool>(TEXT("AffixPool"));
	Pool->Affixes =
	{
		RpgItemizationAutomationTestTypes::MakeAffix(
			TEXT("Affix.One"), RpgItemizationGameplayTags::Item_Stat_Strength, 1.0f, 2.0f),
		RpgItemizationAutomationTestTypes::MakeAffix(
			TEXT("Affix.Two"), RpgItemizationGameplayTags::Item_Stat_Intelligence, 2.0f, 3.0f),
		RpgItemizationAutomationTestTypes::MakeAffix(
			TEXT("Affix.Three"), RpgItemizationGameplayTags::Item_Stat_Vitality, 3.0f, 4.0f)
	};
	AffixPool = Pool;

	FRpgItemStatRollDefinition& BaseStat = BaseStats.AddDefaulted_GetRef();
	BaseStat.StatTag = RpgItemizationGameplayTags::Item_Stat_WeaponDamage;
	BaseStat.MinimumValue = FScalableFloat(10.0f);
	BaseStat.MaximumValue = FScalableFloat(20.0f);

	for (FRpgItemRarityWeight& Row : RarityWeights)
	{
		Row.Weight = Row.Rarity == ERpgItemRarity::Epic ? 1.0f : 0.0f;
	}
}

URpgItemizationAutomationTestItemDefinition::
	URpgItemizationAutomationTestItemDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Generated Equipment"));

	URpgInventoryFragment_SpatialItem* Spatial =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	Spatial->Footprint.Width = 1;
	Spatial->Footprint.Height = 1;
	Fragments.Add(Spatial);

	URpgInventoryFragment_ItemTraits* Traits =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	Traits->ItemCategory = ERpgInventoryItemCategory::Weapon;
	Traits->bCanStack = false;
	Traits->MaxStackSize = 1;
	Fragments.Add(Traits);

	URpgInventoryFragment_Itemization* Itemization =
		CreateDefaultSubobject<URpgInventoryFragment_Itemization>(TEXT("Itemization"));
	Itemization->ItemizationProfile =
		GetMutableDefault<URpgItemizationAutomationTestProfile>();
	Fragments.Add(Itemization);
}
