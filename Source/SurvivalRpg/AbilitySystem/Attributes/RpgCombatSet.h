// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgCombatSet.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgCombatSet : public URpgAttributeSet
{
	GENERATED_BODY()
	
public:
	URpgCombatSet();
	
	ATTRIBUTE_ACCESSORS_BASIC(URpgCombatSet, BaseDamage);
	ATTRIBUTE_ACCESSORS_BASIC(URpgCombatSet, BaseHeal);

protected:

	UFUNCTION()
	void OnRep_BaseDamage(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_BaseHeal(const FGameplayAttributeData& OldValue) const;

private:

	// The base amount of damage to apply in the damage execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseDamage, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseDamage;

	// The base amount of healing to apply in the heal execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseHeal, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseHeal;
};
