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

	/** Residual work on the next section in RpgHarvestWork units, below one section. */
	int32 SectionWork = 0;

	/** Whether the resource is currently available. */
	bool bActive = true;

	/** Returns the sections left in the current revision, including a partly worked one, never negative. */
	int32 GetRemainingSections() const
	{
		return bActive ? FMath::Max(0, SectionCount - HarvestedSections) : 0;
	}

	/** Returns the work left until the resource depletes, in RpgHarvestWork units, never negative. */
	int32 GetRemainingWork() const
	{
		const int32 RemainingSections = GetRemainingSections();
		return RemainingSections > 0
			? RemainingSections * RpgHarvestWork::UnitsPerSection - FMath::Clamp(SectionWork, 0, RpgHarvestWork::UnitsPerSection - 1)
			: 0;
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
	 * addresses this resource. A Harvested outcome reports the work a commit would accept, the sections that work
	 * completes, the overkill beyond the remaining work, and the stock and residual work left after it.
	 * Profile may be null for legacy deplete-only resources, which then skip tool and skill requirements.
	 * ActiveWeakPoint is the representation's current weak point, or null when it has none; a request that may hit
	 * weak points and strikes it brings the profile's bonus sections as additional work from the same stock.
	 * ProtectionProbe is the resource's world location; an area request is rejected as Protected when it lies inside a
	 * harvest protection box. Null skips the check.
	 *
	 * Every commit evaluates again against the stock as it is at that moment on the server, so concurrent requests
	 * never reuse the same work; the revision only rejects requests aimed at a depleted or respawned stock.
	 */
	static FRpgHarvestResult Evaluate(
		const URpgHarvestProfile* Profile,
		const FRpgHarvestRequest& Request,
		const FRpgHarvestStockSnapshot& Stock,
		const FRpgHarvestWeakPoint* ActiveWeakPoint = nullptr,
		const FVector* ProtectionProbe = nullptr);

	/**
	 * Returns which of NumWeakPoints is active for Stock, or INDEX_NONE without weak points or stock. Derived only from
	 * replicated stock state, so the server and every client agree without extra replication. The active weak point
	 * moves on with every extracted section and with every new revision.
	 */
	static int32 GetActiveWeakPointIndex(const FRpgHarvestStockSnapshot& Stock, int32 NumWeakPoints);

	/** Returns whether Request carries the tool category required by Profile. */
	static bool MeetsToolRequirement(const URpgHarvestProfile* Profile, const FRpgHarvestRequest& Request);

	/**
	 * Returns whether Actor belongs to a renewable area: a level loaded at runtime as a level instance, such as a portal
	 * realm. Its resources are never saved and start over with their authored stock whenever the area loads again.
	 * World Partition cells and level instances placed in a map belong to the map's persistent layout.
	 */
	static bool IsInRenewableArea(const AActor& Actor);

	/**
	 * Maps a reward-service delivery result to the public result enum. Failed maps to None, and so does Batched until
	 * the harvest ability that opened the batch delivers it and reports the batch's path.
	 */
	static ERpgHarvestDelivery ToDelivery(ERpgHarvestRewardDeliveryResult DeliveryResult);
};
