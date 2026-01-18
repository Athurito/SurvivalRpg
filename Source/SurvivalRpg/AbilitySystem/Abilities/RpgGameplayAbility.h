// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RpgGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	bool bShouldShowInAbilitiesBar = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Activation")
	bool bAutoActivateWhenGranted = false;
};
