// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RpgGameplayAbility.generated.h"

class ARpgPlayerController;
/**
 * 
 */
UCLASS(Abstract, Meta = (ShortTooltip = "The base gameplay ability class used by this project."))
class SURVIVALRPG_API URpgGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	URpgGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ability")
	ARpgPlayerController* GetRpgPlayerControllerFromActorInfo() const;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	bool bShouldShowInAbilitiesBar = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Activation")
	bool bAutoActivateWhenGranted = false;
	
protected:
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	bool HasPlayerController() const;
};
