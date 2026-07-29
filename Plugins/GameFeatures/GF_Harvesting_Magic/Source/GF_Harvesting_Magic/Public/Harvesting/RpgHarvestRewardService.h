#pragma once

#include "CoreMinimal.h"

class AActor;
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
	WorldDrop
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
};

/** Shared stateless harvest rules used by HISM resources and actor-backed corpses. */
class GF_HARVESTING_MAGIC_API FRpgHarvestRewardService
{
public:
	/** Resolves the canonical RPG player state from a pawn, controller, or player-state harvester. */
	static ARpgPlayerState* ResolveHarvesterPlayerState(AActor* Harvester);

	/** Checks the profile's authoritative trade-skill requirement without mutating progression. */
	static bool MeetsSkillGate(const URpgHarvestRewardProfile* Profile, AActor* Harvester);

	/** Rolls once and atomically delivers the entire batch to inventory or one replicated world drop. */
	static ERpgHarvestRewardDeliveryResult DeliverReward(
		const URpgHarvestRewardProfile* Profile,
		const FRpgHarvestRewardRequest& Request);

	/** Awards the profile's trade-skill XP after a target has accepted successful delivery. */
	static void AwardExperience(const URpgHarvestRewardProfile* Profile, AActor* Harvester);
};
