// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgPlayerProgressionData.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgPlayerProgressionData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UCurveFloat* XPToNextLevel = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 SkillPointsPerLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 MaxLevel = 60;
};
