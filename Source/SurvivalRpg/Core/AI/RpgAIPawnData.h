// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "RpgAIPawnData.generated.h"

class UStateTree;

UCLASS()
class SURVIVALRPG_API URpgAIPawnData : public URpgPawnData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|AI", meta = (Schema = "/Script/GameplayStateTreeModule.StateTreeAIComponentSchema"))
	TObjectPtr<UStateTree> StateTree = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|AI")
	FGameplayTagContainer RoleTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|AI")
	FGameplayTagContainer FactionTags;
};
