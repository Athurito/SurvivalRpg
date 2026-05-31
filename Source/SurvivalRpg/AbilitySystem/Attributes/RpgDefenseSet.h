// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgDefenseSet.generated.h"

/**
 * Replicated defensive combat state.
 *
 * Values in this set affect incoming damage, block, perfect block, and stagger.
 * They are intentionally separate from URpgCombatSet so source/offense meta
 * values can keep Lyra-style owner-only replication.
 */
UCLASS()
class SURVIVALRPG_API URpgDefenseSet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgDefenseSet();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Rpg|Defense")
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, Armor);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BlockChance, Category = "Rpg|Defense")
	FGameplayAttributeData BlockChance;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, BlockChance);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalHitResistance, Category = "Rpg|Defense")
	FGameplayAttributeData CriticalHitResistance;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, CriticalHitResistance);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stagger, Category = "Rpg|Defense|Stagger")
	FGameplayAttributeData Stagger;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, Stagger);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStagger, Category = "Rpg|Defense|Stagger")
	FGameplayAttributeData MaxStagger;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, MaxStagger);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BlockAngleDegrees, Category = "Rpg|Defense|Block")
	FGameplayAttributeData BlockAngleDegrees;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, BlockAngleDegrees);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BlockStaminaCost, Category = "Rpg|Defense|Block")
	FGameplayAttributeData BlockStaminaCost;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, BlockStaminaCost);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BlockDamageReduction, Category = "Rpg|Defense|Block")
	FGameplayAttributeData BlockDamageReduction;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, BlockDamageReduction);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BlockStaggerDamageMultiplier, Category = "Rpg|Defense|Block")
	FGameplayAttributeData BlockStaggerDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, BlockStaggerDamageMultiplier);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PerfectBlockStaminaRestore, Category = "Rpg|Defense|Perfect Block")
	FGameplayAttributeData PerfectBlockStaminaRestore;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, PerfectBlockStaminaRestore);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PerfectBlockStaggerDamage, Category = "Rpg|Defense|Perfect Block")
	FGameplayAttributeData PerfectBlockStaggerDamage;
	ATTRIBUTE_ACCESSORS_BASIC(URpgDefenseSet, PerfectBlockStaggerDamage);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_BlockChance(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_CriticalHitResistance(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_Stagger(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_MaxStagger(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_BlockAngleDegrees(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_BlockStaminaCost(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_BlockDamageReduction(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_BlockStaggerDamageMultiplier(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_PerfectBlockStaminaRestore(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_PerfectBlockStaggerDamage(const FGameplayAttributeData& OldValue) const;

private:
	void ClampDefenseAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	void HandleStaggerThreshold(const FGameplayEffectModCallbackData& Data, float OldValue, float NewValue);
};
