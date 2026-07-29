#pragma once


#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgTradeSkillState.generated.h"

/** Replicated and saved use-based progression for one semantic trade-skill tag. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FTradeSkillState
{
	GENERATED_BODY()

	/** Stable skill identity; runtime and save data are keyed by this tag rather than enum position. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Progression|Trade Skills", meta = (Categories = "Skill"))
	FGameplayTag SkillTag;

	/** Current use-based level, server-authored and constrained to the supported range 1-100. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Progression|Trade Skills")
	int32 Level = 1;

	/** Unspent experience toward the next level; server-authored and persisted across sessions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Progression|Trade Skills")
	float XP = 0.f;

	bool IsValid() const
	{
		return SkillTag.IsValid() && Level >= 1 && Level <= 100 && FMath::IsFinite(XP) && XP >= 0.0f;
	}
};
