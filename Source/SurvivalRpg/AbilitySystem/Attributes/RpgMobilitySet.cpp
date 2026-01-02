// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgMobilitySet.h"

#include "Net/UnrealNetwork.h"

void URpgMobilitySet::OnRep_MovementSpeed(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgMobilitySet, MovementSpeed, OldValue);
}

void URpgMobilitySet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgMobilitySet, MovementSpeed, COND_None, REPNOTIFY_Always);
}
