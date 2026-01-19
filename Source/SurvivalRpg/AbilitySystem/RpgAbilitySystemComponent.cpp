// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgAbilitySystemComponent.h"

URpgAbilitySystemComponent::URpgAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool URpgAbilitySystemComponent::GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	// Wenn Client: an Server delegieren
	if (!IsOwnerActorAuthoritative())
	{
		Server_GrantAbilitySet(AbilitySet, SourceObject);
		return true; // Anfrage raus, Server repliziert Ergebnis
	}

	return GrantAbilitySet_Internal(AbilitySet, SourceObject);
}

bool URpgAbilitySystemComponent::RemoveAbilitySet(const URpgAbilitySet* AbilitySet)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	if (!IsOwnerActorAuthoritative())
	{
		Server_RemoveAbilitySet(AbilitySet);
		return true;
	}

	return RemoveAbilitySet_Internal(AbilitySet);
}

bool URpgAbilitySystemComponent::HasAbilitySet(const URpgAbilitySet* AbilitySet) const
{
	return IsValid(AbilitySet) && GrantedAbilitySets.Contains(AbilitySet);
}

void URpgAbilitySystemComponent::Server_GrantAbilitySet_Implementation(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	GrantAbilitySet_Internal(AbilitySet, SourceObject);
}

void URpgAbilitySystemComponent::Server_RemoveAbilitySet_Implementation(const URpgAbilitySet* AbilitySet)
{
	RemoveAbilitySet_Internal(AbilitySet);
}

bool URpgAbilitySystemComponent::GrantAbilitySet_Internal(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	// Optional: doppelt grant verhindern
	if (GrantedAbilitySets.Contains(AbilitySet))
	{
		return false;
	}

	FRpgAbilitySet_GrantedHandles Handles;
	AbilitySet->GiveToAbilitySystem(this, &Handles, SourceObject);

	GrantedAbilitySets.Add(AbilitySet, Handles);
	return true;
}

bool URpgAbilitySystemComponent::RemoveAbilitySet_Internal(const URpgAbilitySet* AbilitySet)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	FRpgAbilitySet_GrantedHandles* Handles = GrantedAbilitySets.Find(AbilitySet);
	if (!Handles)
	{
		return false;
	}

	Handles->TakeFromAbilitySystem(this);
	GrantedAbilitySets.Remove(AbilitySet);
	return true;
}

void URpgAbilitySystemComponent::ApplyDefaultAbilitySetupIfNeeded(UObject* SourceObject)
{
	if (bDefaultSetupApplied)
	{
		return;
	}

	if (!IsOwnerActorAuthoritative())
	{
		return; // Abilities/Effects nur serverseitig geben
	}

	// Safety: ActorInfo muss gültig sein
	if (!AbilityActorInfo.IsValid() || AbilityActorInfo->AvatarActor.Get() == nullptr)
	{
		return;
	}

	if (DefaultAbilitySetup)
	{
		DefaultAbilitySetup->GiveToAbilitySystem(this, &DefaultGrantedHandles, SourceObject);
	}

	bDefaultSetupApplied = true;
}

void URpgAbilitySystemComponent::RemoveDefaultAbilitySetup()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	if (bDefaultSetupApplied)
	{
		DefaultGrantedHandles.TakeFromAbilitySystem(this);
		bDefaultSetupApplied = false;
	}
}

void URpgAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
}

