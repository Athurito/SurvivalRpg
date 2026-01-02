// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPrimarySet.h"

#include "Net/UnrealNetwork.h"

URpgPrimarySet::URpgPrimarySet()
{
}

void URpgPrimarySet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//Replicate Properties
	DOREPLIFETIME_CONDITION_NOTIFY(URpgPrimarySet, Strength, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgPrimarySet, Intelligence, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgPrimarySet, Resilience, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgPrimarySet, Vitality, COND_None, REPNOTIFY_Always);
}

void URpgPrimarySet::OnRep_Strength(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgPrimarySet, Strength, OldValue);
}

void URpgPrimarySet::OnRep_Intelligence(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgPrimarySet, Intelligence, OldValue);
}

void URpgPrimarySet::OnRep_Resilience(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgPrimarySet, Resilience, OldValue);
}

void URpgPrimarySet::OnRep_Vitality(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgPrimarySet, Vitality, OldValue);
}
