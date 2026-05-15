// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameStateBase.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"

ARpgGameStateBase::ARpgGameStateBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExperienceManagerComponent = CreateDefaultSubobject<URpgExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void ARpgGameStateBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

UAbilitySystemComponent* ARpgGameStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
