// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RpgPlayerState.generated.h"

class ARpgPlayerController;
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
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerState")
	ARpgPlayerController* GetRpgPlayerController() const;
	
	TObjectPtr<URpgAbilitySystemComponent> GetRpgAbilitySystemComponent() const;
private:
	// The ability system component sub-object used by player characters.
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Rpg|AbilitySystem", meta = (AllowPrivateAccess = "true"))
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
