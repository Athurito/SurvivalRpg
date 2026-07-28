#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RpgHarvestProfile.generated.h"

class ARpgDroppedInventoryActor;
class URpgLootTable;

/**
 * Static designer-authored rules shared by every instance in one harvestable HISM component.
 * Runtime availability remains server-owned by the component and is not persisted between sessions.
 */
UCLASS(BlueprintType, Const)
class GF_HARVESTING_MAGIC_API URpgHarvestProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Shared loot table evaluated once on the server for each successful harvest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Rewards")
	TObjectPtr<URpgLootTable> LootTable;

	/** Data-driven trade-skill id, normally below Skill.Gathering. Empty disables skill progression and gating. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Progression", meta = (Categories = "Skill.Gathering"))
	FGameplayTag SkillTag;

	/** Minimum skill level required to harvest this node. Level one is the normal starting requirement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Progression", meta = (ClampMin = "1", UIMin = "1", UIMax = "100"))
	int32 MinimumSkillLevel = 1;

	/** Skill XP awarded exactly once after the reward reaches inventory or a world drop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Progression", meta = (ClampMin = "0", UIMin = "0"))
	int32 SkillExperience = 0;

	/** Earliest server-only respawn delay in seconds. Zero keeps the instance depleted for the session. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Respawn", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MinimumRespawnSeconds = 0.0f;

	/** Latest server-only respawn delay in seconds; values below the minimum are clamped at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Respawn", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MaximumRespawnSeconds = 0.0f;

	/** Semantic source tags copied into the loot context, such as Resource.Tree or Region.Rift. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Context")
	FGameplayTagContainer SourceTags;

	/** Replicated pickup actor used when the complete reward batch cannot fit in the player inventory. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Overflow")
	TSubclassOf<ARpgDroppedInventoryActor> OverflowDropClass;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
