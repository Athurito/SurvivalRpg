// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgStaminaSet.generated.h"

/**
 * Attribute set for stamina-like combat resources.
 *
 * Health, healing, and damage meta attributes live in URpgHealthSet. Keeping
 * stamina separate mirrors Lyra's single-source-of-truth style for health.
 */
UCLASS()
class SURVIVALRPG_API URpgStaminaSet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgStaminaSet();

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Stamina", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(URpgStaminaSet, Stamina);

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(URpgStaminaSet, MaxStamina);

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Stamina", ReplicatedUsing = OnRep_StaminaRegen)
	FGameplayAttributeData StaminaRegen;
	ATTRIBUTE_ACCESSORS_BASIC(URpgStaminaSet, StaminaRegen);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_StaminaRegen(const FGameplayAttributeData& OldValue) const;

private:
	void ClampStaminaAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
