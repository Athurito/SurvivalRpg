// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RpgPlayerState.generated.h"

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
	UPROPERTY(VisibleAnywhere, Category = "Rpg|PlayerState")
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;
	
	// Combat attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class URpgCombatSet> CombatSet;
};
