// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCombatSet.h"

#include "Net/UnrealNetwork.h"


URpgCombatSet::URpgCombatSet()
	: BaseDamage(0.0f)
	, BaseHeal(0.0f)
{
}

void URpgCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgCombatSet, BaseDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgCombatSet, BaseHeal, COND_OwnerOnly, REPNOTIFY_Always);
}

void URpgCombatSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgCombatSet, BaseDamage, OldValue);
}

void URpgCombatSet::OnRep_BaseHeal(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgCombatSet, BaseHeal, OldValue);
}