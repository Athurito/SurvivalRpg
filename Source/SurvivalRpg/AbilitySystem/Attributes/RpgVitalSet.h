// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgVitalSet.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgVitalSet : public URpgAttributeSet
{
	GENERATED_BODY()
	
	
public:
	// Health
	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, Health);

	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_HealthRegen)
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, HealthRegen);

	// Stamina
	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, Stamina);

	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, MaxStamina);

	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_StaminaRegen)
	FGameplayAttributeData StaminaRegen;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, StaminaRegen);

	// Mana (optional)
	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, Mana);

	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, MaxMana);

	UPROPERTY(BlueprintReadOnly, Category="Vitals", ReplicatedUsing=OnRep_ManaRegen)
	FGameplayAttributeData ManaRegen;
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, ManaRegen);

	// Hooks
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// Rep notifies
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_HealthRegen(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_StaminaRegen(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_ManaRegen(const FGameplayAttributeData& OldValue) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	
private:
	
	void ClampVitalAttributes(const FGameplayAttribute& Attribute, float& NewValue) const;

	
public:
	
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, Healing);
	ATTRIBUTE_ACCESSORS_BASIC(URpgVitalSet, Damage);

private:
	
	
	// -------------------------------------------------------------------
	//	Meta Attribute (please keep attributes that aren't 'stateful' below 
	// -------------------------------------------------------------------

	// Incoming healing. This is mapped directly to +Health
	UPROPERTY(BlueprintReadOnly, Category="Rpg|Health", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Healing;

	// Incoming damage. This is mapped directly to -Health
	UPROPERTY(BlueprintReadOnly, Category="Rpg|Health", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData Damage;
};
