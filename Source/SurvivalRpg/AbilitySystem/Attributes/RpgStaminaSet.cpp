// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgStaminaSet.h"

#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

URpgStaminaSet::URpgStaminaSet()
	: Stamina(100.0f)
	, MaxStamina(100.0f)
	, StaminaRegen(0.0f)
{
}

void URpgStaminaSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampStaminaAttribute(Attribute, NewValue);
}

void URpgStaminaSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampStaminaAttribute(Attribute, NewValue);
}

void URpgStaminaSet::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	const float OldValue,
	const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		if (URpgAbilitySystemComponent* AbilitySystemComponent = GetRpgAbilitySystemComponent())
		{
			AbilitySystemComponent->ApplyModToAttribute(
				GetStaminaAttribute(),
				EGameplayModOp::Override,
				NewValue);
		}
	}
}

void URpgStaminaSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgStaminaSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgStaminaSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgStaminaSet, StaminaRegen, COND_None, REPNOTIFY_Always);
}

void URpgStaminaSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgStaminaSet, Stamina, OldValue);
}

void URpgStaminaSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgStaminaSet, MaxStamina, OldValue);
}

void URpgStaminaSet::OnRep_StaminaRegen(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgStaminaSet, StaminaRegen, OldValue);
}

void URpgStaminaSet::ClampStaminaAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetStaminaAttribute())
	{
		NewValue = ClampAttribute(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetStaminaRegenAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}
