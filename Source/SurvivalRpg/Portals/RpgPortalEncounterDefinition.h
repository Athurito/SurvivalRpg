#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgPortalEncounterDefinition.generated.h"

class AActor;
class ARpgPortalExitActor;
class UWorld;

UENUM(BlueprintType)
enum class ERpgPortalEncounterMode : uint8
{
	/** Loads a dungeon level and drives the portal flow from dungeon markers. */
	Dungeon,

	/** Spawns an authored enemy composition directly into the overworld around the portal. */
	BrokenOutbreak
};

/** One fixed enemy class/count pair for BrokenOutbreak portal encounters. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPortalEnemySpawnEntry
{
	GENERATED_BODY()

	/** Enemy actor class to spawn for this entry. Usually a feature-owned enemy Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TSubclassOf<AActor> EnemyClass;

	/** Number of actors of EnemyClass to spawn. Zero entries are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (ClampMin = "0"))
	int32 Count = 1;
};

/**
 * Authoring data for one portal encounter variant.
 *
 * Dungeon encounters define what level, boss and exit portal to use; gameplay
 * positions come from ARpgPortalDungeonMarkerActor instances inside the dungeon
 * level. BrokenOutbreak encounters skip dungeon streaming and use
 * EnemySpawnEntries to spill enemies into the overworld.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgPortalEncounterDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	URpgPortalEncounterDefinition();

	/** Selects whether this portal opens a dungeon or breaks open in the overworld. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	ERpgPortalEncounterMode EncounterMode = ERpgPortalEncounterMode::Dungeon;

	/** Fixed enemy composition for BrokenOutbreak portals. Ignored by Dungeon encounters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	TArray<FRpgPortalEnemySpawnEntry> EnemySpawnEntries;

	/** Radius used to arrange BrokenOutbreak enemy spawns around the portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SpawnRadius = 600.0f;

	/** Boss class spawned at the dungeon level's BossSpawn marker. Ignored by BrokenOutbreak encounters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	TSubclassOf<AActor> BossClass;

	/** Dungeon map streamed as a level instance when a player enters this portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UWorld> DungeonLevel;

	/** Optional per-definition exit portal class spawned at the dungeon ExitPortal marker after boss defeat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	TSubclassOf<ARpgPortalExitActor> ExitPortalActorClass;

	/** Tags copied into the completion message for reward, quest or world-state listeners. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	FGameplayTagContainer CompletionTags;

	/** Stability value shown/tracked when the encounter reaches full completion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Stability", meta = (ClampMin = "1.0"))
	float MaxStability = 3.0f;

	/** Main interaction text while the dungeon portal can be entered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText EnterInteractionText;

	/** Supporting interaction text while the dungeon portal can be entered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText EnterInteractionSubText;

	/** Main interaction text shown on the spawned dungeon exit portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText ExitInteractionText;

	/** Supporting interaction text shown on the spawned dungeon exit portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText ExitInteractionSubText;

	/** Main interaction text once the overworld portal is sealable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText CloseInteractionText;

	/** Supporting interaction text once the overworld portal is sealable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText CloseInteractionSubText;

	/** Keeps rewards gated until a boss-aware listener marks the completion eligible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Reward")
	bool bRewardsRequireBossDefeat = true;
};
