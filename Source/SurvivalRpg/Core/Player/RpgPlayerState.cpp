// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerState.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgMetaSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgMobilitySet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgPrimarySet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgVitalSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgWeaponAmmoSet.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

ARpgPlayerState::ARpgPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	CombatSet = CreateDefaultSubobject<URpgCombatSet>(TEXT("CombatSet"));
	MetaSet = CreateDefaultSubobject<URpgMetaSet>(TEXT("MetaSet"));
	MobilitySet = CreateDefaultSubobject<URpgMobilitySet>(TEXT("MobilitySet"));
	PrimarySet = CreateDefaultSubobject<URpgPrimarySet>(TEXT("PrimarySet"));
	VitalSet = CreateDefaultSubobject<URpgVitalSet>(TEXT("VitalSet"));
	WeaponAmmoSet = CreateDefaultSubobject<URpgWeaponAmmoSet>(TEXT("WeaponAmmoSet"));
	
	PlayerProgressionComponent = CreateDefaultSubobject<URpgPlayerProgressionComponent>(TEXT("PlayerProgressionComponent"));
	TradeSkillProgressionComponent = CreateDefaultSubobject<URpgTradeSkillProgressionComponent>(TEXT("TradeSkillProgressionComponent"));

}

ARpgPlayerController* ARpgPlayerState::GetRpgPlayerController() const
{
	return nullptr;
}

TObjectPtr<URpgAbilitySystemComponent> ARpgPlayerState::GetRpgAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
