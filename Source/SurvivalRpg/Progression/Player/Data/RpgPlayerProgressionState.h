// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgPlayerProgressionState.generated.h"

/** Replicated and saved general character progression owned authoritatively by the PlayerState. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FPlayerProgressionState
{
	GENERATED_BODY()

	/** Current character level; server-authored and persisted across sessions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Progression")
	int32 Level = 1;

	/** Unspent experience toward the next character level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Progression")
	float XP = 0.f;

	/** Earned progression points not yet spent on player-selected unlocks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Progression")
	int32 UnspentSkillPoints = 0;

	bool IsValid() const
	{
		return Level >= 1 && FMath::IsFinite(XP) && XP >= 0.0f && UnspentSkillPoints >= 0;
	}
};
