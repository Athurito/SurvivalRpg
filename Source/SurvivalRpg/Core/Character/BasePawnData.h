// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BasePawnData.generated.h"

class URpgInputConfig;
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
};
