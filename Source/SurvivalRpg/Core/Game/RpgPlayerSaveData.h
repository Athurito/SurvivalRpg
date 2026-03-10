// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgPlayerSaveData.generated.h"

/**
 * Per-player save data stored on the host.
 * Keyed by Steam NetId in the GameMode's TMap.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPlayerSaveData
{
	GENERATED_BODY()

	/** Last checkpoint transform for respawn. */
	UPROPERTY(BlueprintReadWrite, Category = "Rpg|Save")
	FTransform CheckpointTransform = FTransform::Identity;

	/** Whether a checkpoint has been set. */
	UPROPERTY(BlueprintReadWrite, Category = "Rpg|Save")
	bool bHasCheckpoint = false;

	// Future: inventory, quest progress, skill levels, etc.
};
