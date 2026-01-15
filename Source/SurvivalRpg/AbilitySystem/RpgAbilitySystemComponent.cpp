// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgAbilitySystemComponent.h"

URpgAbilitySystemComponent::URpgAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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

