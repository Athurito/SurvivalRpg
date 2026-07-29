#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RpgHarvestRewardProfile.generated.h"

class ARpgDroppedInventoryActor;
class URpgLootTable;

/**
 * Shared static reward, progression, and overflow rules for any harvest target.
 * Runtime availability, reservations, depletion, and respawn remain owned by the concrete target component.
 */
UCLASS(Abstract, BlueprintType, Const)
class GF_HARVESTING_MAGIC_API URpgHarvestRewardProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Shared loot table evaluated exactly once on the server for each successful harvest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Rewards")
	TObjectPtr<URpgLootTable> LootTable;

	/** Data-driven trade-skill id, normally below Skill.Gathering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Progression", meta = (Categories = "Skill.Gathering"))
	FGameplayTag SkillTag;

	/** Minimum authoritative skill level required to begin and complete this harvest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Progression", meta = (ClampMin = "1", UIMin = "1", UIMax = "100"))
	int32 MinimumSkillLevel = 1;

	/** Skill XP awarded exactly once after the complete reward reaches inventory or a world drop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Progression", meta = (ClampMin = "0", UIMin = "0"))
	int32 SkillExperience = 0;

	/** Semantic source tags copied into the server-authored loot context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Context")
	FGameplayTagContainer SourceTags;

	/** Replicated pickup actor used when the complete reward batch cannot fit in the player inventory. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Overflow")
	TSubclassOf<ARpgDroppedInventoryActor> OverflowDropClass;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
