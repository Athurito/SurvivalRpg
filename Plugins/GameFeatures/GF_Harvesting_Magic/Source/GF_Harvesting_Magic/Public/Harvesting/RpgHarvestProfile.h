#pragma once

#include "Harvesting/RpgHarvestRewardProfile.h"

#include "RpgHarvestProfile.generated.h"

/**
 * Static designer-authored rules shared by every instance in one harvestable HISM component.
 * Runtime availability remains server-owned by the component and is not persisted between sessions.
 */
UCLASS(BlueprintType, Const)
class GF_HARVESTING_MAGIC_API URpgHarvestProfile : public URpgHarvestRewardProfile
{
	GENERATED_BODY()

public:
	/** Earliest server-only respawn delay in seconds. Zero keeps the instance depleted for the session. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Respawn", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MinimumRespawnSeconds = 0.0f;

	/** Latest server-only respawn delay in seconds; values below the minimum are clamped at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Respawn", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MaximumRespawnSeconds = 0.0f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
