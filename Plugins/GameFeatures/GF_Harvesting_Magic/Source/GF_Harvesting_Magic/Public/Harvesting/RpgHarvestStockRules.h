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

/** World-space weak point of a resource at the moment of evaluation. */
struct GF_HARVESTING_MAGIC_API FRpgHarvestWeakPoint
{
	/** World location in centimeters. */
	FVector Location = FVector::ZeroVector;

	/** Hit tolerance around Location in centimeters. */
	float Radius = 0.0f;

	/**
	 * Returns whether Hit strikes this weak point. A traced hit counts when its aim ray passes within Radius of
	 * Location and Location lies at most two radii beyond where the ray stopped: the marked point sits on the visible
	 * surface, which can lie behind a coarser collision hull or a swept contact. Points deeper inside or behind the
	 * resource never count. Hits without trace data count when ImpactPoint lies within Radius.
	 */
	bool IsStruckBy(const FHitResult& Hit) const;
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
	 * ActiveWeakPoint is the representation's current weak point, or null when it has none; a request that may hit
	 * weak points and strikes it takes the profile's bonus sections in addition, clamped to the remaining stock.
	 */
	static FRpgHarvestResult Evaluate(
		const URpgHarvestProfile* Profile,
		const FRpgHarvestRequest& Request,
		const FRpgHarvestStockSnapshot& Stock,
		const FRpgHarvestWeakPoint* ActiveWeakPoint = nullptr);

	/**
	 * Returns which of NumWeakPoints is active for Stock, or INDEX_NONE without weak points or stock. Derived only from
	 * replicated stock state, so the server and every client agree without extra replication. The active weak point
	 * moves on with every extracted section and with every new revision.
	 */
	static int32 GetActiveWeakPointIndex(const FRpgHarvestStockSnapshot& Stock, int32 NumWeakPoints);

	/** Returns whether Request carries the tool category required by Profile. */
	static bool MeetsToolRequirement(const URpgHarvestProfile* Profile, const FRpgHarvestRequest& Request);

	/** Maps a reward-service delivery result to the public result enum; Failed maps to None. */
	static ERpgHarvestDelivery ToDelivery(ERpgHarvestRewardDeliveryResult DeliveryResult);
};
