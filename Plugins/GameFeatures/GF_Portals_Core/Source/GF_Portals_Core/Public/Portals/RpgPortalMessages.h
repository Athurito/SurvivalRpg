#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgPortalMessages.generated.h"

class AActor;
class URpgPortalEncounterDefinition;
class URpgLootTable;

/**
 * GameplayMessage payload emitted when an overworld portal is closed.
 *
 * The portal progression hook resolves the one-time storage discovery before
 * broadcast. Other reward, quest, or world-state listeners can subscribe to
 * the completion channel and inspect the authoritative outcome.
 * Consumers treat this payload as a read-only record of the committed result.
 */
USTRUCT(BlueprintType)
struct GF_PORTALS_CORE_API FRpgPortalCompletedMessage
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

	/** Whether the portal actor observed a realm boss defeat. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bBossDefeated = false;

	/** Server-computed eligibility after applying this encounter's boss-defeat policy. */
	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bRewardsEligible = false;

	/**
	 * World-storage discoveries created by this completion.
	 * Empty on ineligible or repeated completions, allowing reward listeners to remain idempotent.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Portal|Progression")
	FGameplayTagContainer NewlyGrantedWorldKnowledgeTags;

	/**
	 * Designer-authored reward table that was already delivered with a new knowledge grant.
	 * This is diagnostic/UI context only; listeners must not grant it a second time.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Portal|Progression")
	TSoftObjectPtr<URpgLootTable> FirstEligibleKnowledgeRewardTable;
};
