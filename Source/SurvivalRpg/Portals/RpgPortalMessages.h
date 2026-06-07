#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgPortalMessages.generated.h"

class AActor;
class URpgPortalEncounterDefinition;

/**
 * GameplayMessage payload emitted when an overworld portal is closed.
 *
 * This is intentionally reward-hook data only in the current slice. Reward,
 * progression, quest, or world-state listeners can subscribe to the portal
 * completion channel and decide what to do based on the definition, tags and
 * reward eligibility flags.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPortalCompletedMessage
{
	GENERATED_BODY()

	/** Portal actor that was closed. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	TObjectPtr<AActor> Portal = nullptr;

	/** Actor that completed the close interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	TObjectPtr<AActor> Instigator = nullptr;

	/** Encounter definition used by the closed portal. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	TObjectPtr<const URpgPortalEncounterDefinition> EncounterDefinition = nullptr;

	/** Definition-authored tags for content-specific completion listeners. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	FGameplayTagContainer CompletionTags;

	/** Final stability value at close time. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	float FinalStability = 0.0f;

	/** Whether this definition requires boss defeat before rewards should be considered. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bRewardsRequireBossDefeat = true;

	/** Whether the portal actor observed a dungeon boss defeat. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bBossDefeated = false;

	/** False in hook-only slices until a later reward/boss system opts in. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bRewardsEligible = false;
};
