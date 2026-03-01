// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgMobilitySet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgPrimarySet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgVitalSet.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

ARpgPlayerState::ARpgPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	// CombatSet = CreateDefaultSubobject<URpgCombatSet>(TEXT("CombatSet"));
	// MobilitySet = CreateDefaultSubobject<URpgMobilitySet>(TEXT("MobilitySet"));
	// PrimarySet = CreateDefaultSubobject<URpgPrimarySet>(TEXT("PrimarySet"));
	// VitalSet = CreateDefaultSubobject<URpgVitalSet>(TEXT("VitalSet"));
	HealthSet = CreateDefaultSubobject<URpgHealthSet>(TEXT("HealthSet"));
	
	
	
	PlayerProgressionComponent = CreateDefaultSubobject<URpgPlayerProgressionComponent>(TEXT("PlayerProgressionComponent"));
	TradeSkillProgressionComponent = CreateDefaultSubobject<URpgTradeSkillProgressionComponent>(TEXT("TradeSkillProgressionComponent"));

}

ARpgPlayerController* ARpgPlayerState::GetRpgPlayerController() const
{
	return nullptr;
}

void ARpgPlayerState::SendAbilitiesChangedEvent()
{
	FGameplayEventData EventData;
	EventData.EventTag = FGameplayTag::RequestGameplayTag("Event.Abilities.Changed");
	EventData.Instigator = this;
	EventData.Target = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

TObjectPtr<URpgAbilitySystemComponent> ARpgPlayerState::GetRpgAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARpgPlayerState::SetPawnData(const UBasePawnData* InPawnData)
{
	check(InPawnData);
	if (PawnData) return;
	PawnData = InPawnData;
}
