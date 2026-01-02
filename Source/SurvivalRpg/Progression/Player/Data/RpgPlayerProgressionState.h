// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgPlayerProgressionState.generated.h"

USTRUCT(BlueprintType)
struct FPlayerProgressionState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Level = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float XP = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 UnspentSkillPoints = 0;
};
