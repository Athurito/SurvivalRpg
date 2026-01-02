// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgVitalSet.h"

#include "GameplayEffectExtension.h"
#include "RpgMetaSet.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

void URpgVitalSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	ClampVitalAttributes(Attribute, NewValue);
}

void URpgVitalSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	URpgAbilitySystemComponent* TargetAsc = Cast<URpgAbilitySystemComponent>(&Data.Target);
	if (!TargetAsc) return;
	
	const URpgMetaSet* MetaSet = TargetAsc->GetSet<URpgMetaSet>();
	if (!MetaSet) return;

	// 1) Damage -> Health
	if (MetaSet && Data.EvaluatedData.Attribute == URpgMetaSet::GetDamageAttribute())
	{
		const float IncomingDamage = MetaSet->GetDamage();
		if (IncomingDamage > 0.f)
		{
			SetHealth(ClampAttribute(GetHealth() - IncomingDamage, 0.f, GetMaxHealth()));
		}

		// Damage immer wieder auf 0 setzen (Meta-Attribute sind “one-shot”)
		TargetAsc->SetNumericAttributeBase(URpgMetaSet::GetDamageAttribute(), 0.f);
	}

	// 2) Healing -> Health
	if (MetaSet && Data.EvaluatedData.Attribute == URpgMetaSet::GetHealingAttribute())
	{
		const float IncomingHealing = MetaSet->GetHealing();
		if (IncomingHealing > 0.f)
		{
			SetHealth(ClampAttribute(GetHealth() + IncomingHealing, 0.f, GetMaxHealth()));
		}

		TargetAsc->SetNumericAttributeBase(URpgMetaSet::GetHealingAttribute(), 0.f);
	}
}

void URpgVitalSet::OnRep_Health(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, Health, OldValue);
}

void URpgVitalSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, MaxHealth, OldValue);
}

void URpgVitalSet::OnRep_HealthRegen(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, HealthRegen, OldValue);
}

void URpgVitalSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, Stamina, OldValue);
}

void URpgVitalSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, MaxStamina, OldValue);
}
 
void URpgVitalSet::OnRep_StaminaRegen(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, StaminaRegen, OldValue);
}

void URpgVitalSet::OnRep_Mana(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, Mana, OldValue);
}

void URpgVitalSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue) const	
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, MaxMana, OldValue);
}

void URpgVitalSet::OnRep_ManaRegen(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgVitalSet, ManaRegen, OldValue);
}

void URpgVitalSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, HealthRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, StaminaRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgVitalSet, ManaRegen, COND_None, REPNOTIFY_Always);
}

void URpgVitalSet::ClampVitalAttributes(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = ClampAttribute(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = ClampAttribute(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = ClampAttribute(NewValue, 0.f, GetMaxMana());
	}
}
