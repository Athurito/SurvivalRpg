// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BasePawnData.generated.h"

class URpgInputConfig;
class URpgAbilitySet;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API UBasePawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URpgInputConfig> InputConfig;

	// Startup-only ability sets that should be granted whenever a pawn using this data is initialized.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TObjectPtr<const URpgAbilitySet>> AbilitySets;
};
