#pragma once

#include "CoreMinimal.h"
#include "Harvesting/RpgHarvestableTarget.h"

class URpgHarvestProfile;
enum class ERpgHarvestRewardDeliveryResult : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogRpgHarvesting, Log, All);

/** Representation-independent snapshot of one harvestable resource's current stock. */
struct GF_HARVESTING_MAGIC_API FRpgHarvestStockSnapshot
{
	/** Current resource revision; advances only when the resource depletes or respawns. */
	int32 Revision = 0;

	/** Total logical sections in the current revision, at least one. */
	int32 SectionCount = 1;

	/** Sections already extracted in the current revision. */
	int32 HarvestedSections = 0;

	/** Whether the resource is currently available. */
	bool bActive = true;

	/** Returns the sections left in the current revision, never negative. */
	int32 GetRemainingSections() const
	{
		return bActive ? FMath::Max(0, SectionCount - HarvestedSections) : 0;
	}
};

/**
 * Stateless request validation and stock arithmetic shared by every harvestable representation
 * (actor nodes, instanced meshes, and future PCG instances), so all harvest methods extract from the same stock.
 */
struct GF_HARVESTING_MAGIC_API FRpgHarvestStockRules
{
	/**
	 * Evaluates Request against Stock without mutating anything. The caller has already verified that the request
	 * addresses this resource. A Harvested outcome reports the sections a commit would take and the stock left after it.
	 * Profile may be null for legacy deplete-only resources, which then skip tool and skill requirements.
	 */
	static FRpgHarvestResult Evaluate(
		const URpgHarvestProfile* Profile,
		const FRpgHarvestRequest& Request,
		const FRpgHarvestStockSnapshot& Stock);

	/** Returns whether Request carries the tool category required by Profile. */
	static bool MeetsToolRequirement(const URpgHarvestProfile* Profile, const FRpgHarvestRequest& Request);

	/** Maps a reward-service delivery result to the public result enum; Failed maps to None. */
	static ERpgHarvestDelivery ToDelivery(ERpgHarvestRewardDeliveryResult DeliveryResult);
};
