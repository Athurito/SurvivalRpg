#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "Templates/SubclassOf.h"

class AActor;
class ARpgDroppedInventoryActor;
class ARpgPlayerState;
class URpgHarvestRewardProfile;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;

/** Result of one complete server-authoritative harvest reward delivery. */
enum class ERpgHarvestRewardDeliveryResult : uint8
{
	Failed,
	Empty,
	Inventory,
	WorldDrop,

	/** The reward was rolled and added to the harvester's open FRpgHarvestRewardBatch, which delivers it later. */
	Batched
};

/** Native-only input for one atomic harvest reward roll and delivery. */
struct GF_HARVESTING_MAGIC_API FRpgHarvestRewardRequest
{
	/** Authoritative resource or corpse actor used as loot source and item-instance outer. */
	TObjectPtr<AActor> SourceActor = nullptr;

	/** Authoritative harvesting avatar, player state, or controller receiving the reward. */
	TObjectPtr<AActor> Harvester = nullptr;

	/** World transform used for the complete overflow drop when inventory preflight fails. */
	FTransform DeliveryTransform = FTransform::Identity;

	/** Relative tool or spell power multiplied into loot-table yield calculations. */
	float HarvestPower = 1.0f;

	/** Stable target-specific entropy such as a HISM index/revision or corpse revision. */
	int32 SeedSalt = 0;

	/**
	 * Number of independent loot-table rolls merged into the one delivered batch, normally one per harvested stock
	 * section. Values below one are rejected.
	 */
	int32 RollCount = 1;
};

/** Shared stateless harvest rules used by HISM resources and actor-backed corpses. */
class GF_HARVESTING_MAGIC_API FRpgHarvestRewardService
{
public:
	/** Resolves the canonical RPG player state from a pawn, controller, or player-state harvester. */
	static ARpgPlayerState* ResolveHarvesterPlayerState(AActor* Harvester);

	/** Checks the profile's authoritative trade-skill requirement without mutating progression. */
	static bool MeetsSkillGate(const URpgHarvestRewardProfile* Profile, AActor* Harvester);

	/** Rolls RollCount times and atomically delivers the merged batch to inventory or one replicated world drop. */
	static ERpgHarvestRewardDeliveryResult DeliverReward(
		const URpgHarvestRewardProfile* Profile,
		const FRpgHarvestRewardRequest& Request);

	/** Awards the profile's trade-skill XP once per harvested unit after a target has accepted successful delivery. */
	static void AwardExperience(const URpgHarvestRewardProfile* Profile, AActor* Harvester, int32 HarvestedUnits = 1);

private:
	friend class FRpgHarvestRewardBatch;

	/**
	 * Delivers Reward atomically to the harvester's inventory when bTryInventory is set and it fits, otherwise
	 * completely into one drop at DropTransform.
	 */
	static ERpgHarvestRewardDeliveryResult DeliverPickup(
		UWorld& World,
		AActor& Harvester,
		const FInventoryPickup& Reward,
		const FTransform& DropTransform,
		TSubclassOf<ARpgDroppedInventoryActor> DropClass,
		bool bTryInventory);
};

/**
 * Server-side scope that merges the rewards of several harvest commits into one delivery, so an area harvest fills
 * the inventory as one atomic batch or overflows into one drop instead of one drop per target.
 *
 * While a batch is open for a harvester, FRpgHarvestRewardService::DeliverReward still validates, rolls and
 * materializes every target's reward, but adds it to the batch and reports Batched; the target then extracts its
 * stock and awards XP as usual. Deliver sends the merged rewards. A batch destroyed with undelivered rewards delivers
 * them at the harvester, so material is never dropped silently. Game thread only; open batches may not overlap for one
 * harvester.
 *
 * A long-lived owner such as a harvest swarm keeps a closed batch and opens it only around each of its commits, so it
 * never collects rewards of unrelated harvests in between.
 */
class GF_HARVESTING_MAGIC_API FRpgHarvestRewardBatch : public FNoncopyable
{
public:
	/** Creates a batch for InHarvester; with bOpen it collects the harvester's rewards at once. */
	explicit FRpgHarvestRewardBatch(AActor* InHarvester, bool bOpen = true);
	~FRpgHarvestRewardBatch();

	/** Starts collecting the harvester's rewards. Fails when another batch of the harvester is open. */
	bool Open();

	/** Stops collecting rewards without delivering them; they wait for Deliver. */
	void Close();

	/** Returns whether the batch collects the harvester's rewards. */
	bool IsOpen() const;

	/** Closes the batch and drops its pending rewards without delivering them, for example when the world ends. */
	void Discard();

	/**
	 * Delivers every batched reward to the harvester's inventory as one atomic batch, or completely into one
	 * replicated drop at DropTransform. Returns Empty when nothing was batched. Later rewards are delivered directly.
	 */
	ERpgHarvestRewardDeliveryResult Deliver(const FTransform& DropTransform);

	/** Returns whether the batch holds rewards that were not delivered yet. */
	bool HasPendingRewards() const;

	/** Returns the open batch of Harvester, or null. */
	static FRpgHarvestRewardBatch* FindOpen(const AActor* Harvester);

private:
	friend class FRpgHarvestRewardService;

	/** Merges Reward into the pending rewards; stacks of the same item definition are combined. */
	bool Append(const FInventoryPickup& Reward, TSubclassOf<ARpgDroppedInventoryActor> InDropClass);

	TWeakObjectPtr<AActor> Harvester;
	TWeakObjectPtr<UWorld> World;
	FInventoryPickup PendingReward;
	TSubclassOf<ARpgDroppedInventoryActor> DropClass;

	static TArray<FRpgHarvestRewardBatch*> OpenBatches;
};
