#pragma once

#include "Harvesting/RpgHarvestRewardProfile.h"

#include "RpgHarvestProfile.generated.h"

/**
 * Static designer-authored rules for harvestable resources such as ore veins, trees, and bushes.
 * Runtime stock and availability remain server-owned by the target component and are not persisted between sessions.
 */
UCLASS(BlueprintType, Const)
class GF_HARVESTING_MAGIC_API URpgHarvestProfile : public URpgHarvestRewardProfile
{
	GENERATED_BODY()

public:
	/** Upper bound for SectionCount; keeps replicated stock compact and reward batches bounded. */
	static constexpr int32 MaxSectionCount = 16;

	/**
	 * Number of logical stock sections in one resource. Every harvested section rolls LootTable once and awards
	 * SkillExperience once, so all harvest methods share the same total stock. One keeps single-hit depletion.
	 * Designer-tuned static data; instanced HISM resources currently support only one section.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Stock", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 SectionCount = 1;

	/**
	 * Harvesting tool category a request must carry, for example a pickaxe for ore. Requests match when their
	 * ToolTag equals or is a child of this tag. Empty accepts tool-less harvesting, including manual interaction;
	 * when set, resources do not offer the manual interaction harvest.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Tools", meta = (Categories = "Tool.Harvesting"))
	FGameplayTag RequiredToolTag;

	/**
	 * Extra stock sections a precise swing extracts when it strikes the resource's active weak point. The bonus comes
	 * from the same stock and is clamped to what remains, so the total yield never depends on weak-point hits.
	 * Zero disables weak points. The resource actor places its weak points (URpgHarvestableComponent).
	 * Designer-tuned static data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Stock", meta = (ClampMin = "0", ClampMax = "15", UIMin = "0", UIMax = "4"))
	int32 WeakPointBonusSections = 0;

	/** Earliest server-only respawn delay in seconds. Zero keeps the resource depleted for the session. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Respawn", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MinimumRespawnSeconds = 0.0f;

	/** Latest server-only respawn delay in seconds; values below the minimum are clamped at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Respawn", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MaximumRespawnSeconds = 0.0f;

	/** Returns SectionCount clamped to the supported range. */
	int32 GetClampedSectionCount() const
	{
		return FMath::Clamp(SectionCount, 1, MaxSectionCount);
	}

	/** Returns WeakPointBonusSections clamped to the supported range. */
	int32 GetClampedWeakPointBonusSections() const
	{
		return FMath::Clamp(WeakPointBonusSections, 0, MaxSectionCount - 1);
	}

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
