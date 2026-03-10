// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgDownedComponent.h"

#include "AbilitySystemComponent.h"
#include "RpgHealthComponent.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"


void URpgDownedComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (AbilitySystemComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: Health component for owner [%s] has already been initialized with an ability system."), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: Cannot initialize health component for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}
	
	HealthComponent = Owner->FindComponentByClass<URpgHealthComponent>();
}

void URpgDownedComponent::UninitializeFromAbilitySystem()
{
	StopBleedout();

	AbilitySystemComponent = nullptr;
	HealthComponent = nullptr;
}

bool URpgDownedComponent::TryEnterDowned()
{
	if (!AbilitySystemComponent || !HealthComponent)
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::Status_Downed))
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::Status_CannotBeRevived))
	{
		return false;
	}

	if (HealthComponent->IsDeadOrDying())
	{
		return false;
	}

	AbilitySystemComponent->AddLooseGameplayTag(RpgGameplayTags::Status_Downed);
	AbilitySystemComponent->AddLooseGameplayTag(RpgGameplayTags::Status_Downed_BleedingOut);

	StartBleedout();

	return true;
}

void URpgDownedComponent::StartBleedout()
{
}

void URpgDownedComponent::StopBleedout()
{
}

bool URpgDownedComponent::IsDowned() const
{
}

void URpgDownedComponent::BeginRevive(AActor* Reviver)
{
}

void URpgDownedComponent::CancelRevive(AActor* Reviver)
{
}

void URpgDownedComponent::CompleteRevive(AActor* Reviver)
{
}
