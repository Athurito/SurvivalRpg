// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgPlayerProgressionData.generated.h"

/** Designer-tuned character level rules read by the server-authoritative URpgPlayerProgressionComponent. */
UCLASS()
class SURVIVALRPG_API URpgPlayerProgressionData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/**
	 * Experience needed to advance from a level to the next: time is the current level, value the XP cost.
	 * Needs a positive key for every level below MaxLevel; a missing curve or a value of 0 stops levelling.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UCurveFloat* XPToNextLevel = nullptr;
	
	/** Unspent skill points granted on every level gained. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0", UIMin = "0"))
	int32 SkillPointsPerLevel = 1;

	/** Highest character level; experience stops accumulating there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxLevel = 60;
};
