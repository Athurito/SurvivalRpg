// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgCombatSet.generated.h"

/**
 * Source/offense combat attributes used by gameplay executions.
 *
 * This intentionally stays close to Lyra's CombatSet shape: values that are
 * only meaningful to the owning source are replicated owner-only. Public
 * defense, guard, and stagger state lives in URpgDefenseSet.
 */
UCLASS()
class SURVIVALRPG_API URpgCombatSet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgCombatSet();

	ATTRIBUTE_ACCESSORS_BASIC(URpgCombatSet, BaseDamage);
	ATTRIBUTE_ACCESSORS_BASIC(URpgCombatSet, BaseHeal);
	ATTRIBUTE_ACCESSORS_BASIC(URpgCombatSet, ArmorPenetration);
	ATTRIBUTE_ACCESSORS_BASIC(URpgCombatSet, CriticalHitChance);
	ATTRIBUTE_ACCESSORS_BASIC(URpgCombatSet, CriticalHitDamage);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_BaseDamage(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_BaseHeal(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_CriticalHitChance(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_CriticalHitDamage(const FGameplayAttributeData& OldValue) const;

private:
	// The base amount of damage to add in the damage execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseDamage, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseDamage;

	// The base amount of healing to add in the heal execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseHeal, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseHeal;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ArmorPenetration, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ArmorPenetration;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalHitChance, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CriticalHitChance;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalHitDamage, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CriticalHitDamage;
};
