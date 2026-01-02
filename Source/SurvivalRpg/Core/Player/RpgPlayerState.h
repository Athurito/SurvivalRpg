// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RpgPlayerState.generated.h"

class URpgTradeSkillProgressionComponent;
class URpgPlayerProgressionComponent;
class URpgAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API ARpgPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	ARpgPlayerState();
	
private:
	// The ability system component sub-object used by player characters.
	UPROPERTY(VisibleAnywhere, Category = "Rpg|AbilitySystem")
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Rpg|Progression")
	TObjectPtr<URpgPlayerProgressionComponent> PlayerProgressionComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Rpg|Progression")
	TObjectPtr<URpgTradeSkillProgressionComponent> TradeSkillProgressionComponent;
	
	
	// Combat attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class URpgCombatSet> CombatSet;
	UPROPERTY()
	TObjectPtr<const class URpgMetaSet> MetaSet;
	UPROPERTY()
	TObjectPtr<const class URpgMobilitySet> MobilitySet;
	UPROPERTY()
	TObjectPtr<const class URpgPrimarySet> PrimarySet;
	UPROPERTY()
	TObjectPtr<const class URpgVitalSet> VitalSet;
	UPROPERTY()
	TObjectPtr<const class URpgWeaponAmmoSet> WeaponAmmoSet;
};
