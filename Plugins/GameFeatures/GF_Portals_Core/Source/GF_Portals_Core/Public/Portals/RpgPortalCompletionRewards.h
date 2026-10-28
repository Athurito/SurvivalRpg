#pragma once

#include "CoreMinimal.h"

struct FRpgPortalCompletedMessage;

/** Plugin-owned adapter from an eligible portal completion into the players' guaranteed reward claims. */
struct GF_PORTALS_CORE_API FRpgPortalCompletionRewards
{
	/**
	 * Grants the encounter's CompletionReward to every participant of an eligible completion, as one claim per player
	 * with the id of MakeClaimId. Server only; returns how many claims were newly granted, so a repeated call is a no-op.
	 */
	static int32 GrantCompletionRewards(const FRpgPortalCompletedMessage& Message);

	/** Claim id of a completion's reward: "Portal/<Encounter>/<CompletionId>". */
	static FName MakeClaimId(const FRpgPortalCompletedMessage& Message);
};
