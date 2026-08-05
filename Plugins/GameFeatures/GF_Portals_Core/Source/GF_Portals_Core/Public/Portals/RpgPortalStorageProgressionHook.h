#pragma once

#include "CoreMinimal.h"

class UWorld;
class URpgLootTable;
struct FRpgPortalCompletedMessage;

/** Plugin-owned adapter from an eligible portal completion into generic core storage knowledge. */
struct GF_PORTALS_CORE_API FRpgPortalStorageProgressionHook
{
	/** Validates the exact, non-random reward contract required before world knowledge may be granted. */
	static bool IsDeterministicGuaranteedRewardTable(
		const URpgLootTable* RewardTable,
		FString& OutError);

	/**
	 * Delivers the authored gate reward and then grants the first Rift-containment discovery on the authoritative GameState.
	 * Returns true only after lossless delivery and a newly resolved discovery; repeated completions are no-ops.
	 */
	static bool ApplyFirstEligibleCompletion(
		UWorld* World,
		FRpgPortalCompletedMessage& InOutMessage);
};
