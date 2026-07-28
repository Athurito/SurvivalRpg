#include "RpgItemizationEquipmentEffect.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgPrimarySet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgItemizationEquipmentEffect)

namespace
{
	void AddSetByCallerModifier(
		UGameplayEffect& Effect,
		const FGameplayAttribute& Attribute,
		const FGameplayTag& MagnitudeTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = MagnitudeTag;

		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	}
}

URpgItemizationEquipmentEffect::URpgItemizationEquipmentEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	using namespace RpgItemizationGameplayTags;
	AddSetByCallerModifier(*this, URpgDefenseSet::GetArmorAttribute(), Item_Stat_Armor);
	AddSetByCallerModifier(*this, URpgPrimarySet::GetStrengthAttribute(), Item_Stat_Strength);
	AddSetByCallerModifier(*this, URpgPrimarySet::GetIntelligenceAttribute(), Item_Stat_Intelligence);
	AddSetByCallerModifier(*this, URpgPrimarySet::GetResilienceAttribute(), Item_Stat_Resilience);
	AddSetByCallerModifier(*this, URpgPrimarySet::GetVitalityAttribute(), Item_Stat_Vitality);
	AddSetByCallerModifier(*this, URpgCombatSet::GetArmorPenetrationAttribute(), Item_Stat_ArmorPenetration);
	AddSetByCallerModifier(*this, URpgCombatSet::GetCriticalHitChanceAttribute(), Item_Stat_CriticalHitChance);
	AddSetByCallerModifier(*this, URpgCombatSet::GetCriticalHitDamageAttribute(), Item_Stat_CriticalHitDamage);
	AddSetByCallerModifier(*this, URpgDefenseSet::GetCriticalHitResistanceAttribute(), Item_Stat_CriticalHitResistance);
	AddSetByCallerModifier(*this, URpgStaminaSet::GetMaxStaminaAttribute(), Item_Stat_MaxStamina);
}
