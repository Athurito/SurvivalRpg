#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgPortalEncounterDefinition.generated.h"

class AActor;
class ARpgPortalExitActor;
class ARpgPortalRealmEventDirector;
class UGameplayEffect;
class UWorld;

UENUM(BlueprintType)
enum class ERpgPortalEncounterMode : uint8
{
	/** Loads a realm level and drives the portal flow from realm markers. */
	Realm,

	/** Spawns an authored enemy composition directly into the overworld around the portal. */
	BrokenOutbreak
};

UENUM(BlueprintType)
enum class ERpgPortalRealmHostingMode : uint8
{
	/** Loads the realm as a dynamic level instance in the current server world. */
	StreamedPocketWorld
};

/** One fixed enemy class/count pair for BrokenOutbreak portal encounters. */
USTRUCT(BlueprintType)
struct GF_PORTALS_CORE_API FRpgPortalEnemySpawnEntry
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
 * Realm encounters define what level, boss and exit portal to use; gameplay
 * positions come from ARpgPortalRealmMarkerActor instances inside the realm
 * level. BrokenOutbreak encounters skip realm streaming and use
 * EnemySpawnEntries to spill enemies into the overworld.
 */
UCLASS(BlueprintType)
class GF_PORTALS_CORE_API URpgPortalEncounterDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	URpgPortalEncounterDefinition();

	/** Selects whether this portal opens a realm or breaks open in the overworld. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	ERpgPortalEncounterMode EncounterMode = ERpgPortalEncounterMode::Realm;

	/** Selects how Realm encounters are hosted. V1 supports streamed pocket worlds only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	ERpgPortalRealmHostingMode HostingMode = ERpgPortalRealmHostingMode::StreamedPocketWorld;

	/** Fixed enemy composition for BrokenOutbreak portals. Ignored by Realm encounters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	TArray<FRpgPortalEnemySpawnEntry> EnemySpawnEntries;

	/** Radius used to arrange BrokenOutbreak enemy spawns around the portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SpawnRadius = 600.0f;

	/** Boss class spawned at the realm level's BossSpawn marker. Ignored by BrokenOutbreak encounters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	TSubclassOf<AActor> BossClass;

	/** Realm map streamed as a level instance when a player enters this portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UWorld> RealmLevel;

	/** Optional per-definition exit portal class spawned at the realm ExitPortal marker after boss defeat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	TSubclassOf<ARpgPortalExitActor> ExitPortalActorClass;

	/** Realm identity/state tags granted while a participant is inside this realm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	FGameplayTagContainer RealmTags;

	/** One-shot GameplayEffects applied after a participant successfully enters the realm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	TArray<TSubclassOf<UGameplayEffect>> EffectsOnEnter;

	/** Persistent GameplayEffects kept active while a participant remains inside the realm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	TArray<TSubclassOf<UGameplayEffect>> EffectsWhileInside;

	/** One-shot GameplayEffects applied only when a participant exits normally through the exit portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	TArray<TSubclassOf<UGameplayEffect>> EffectsOnExit;

	/** Optional server-side director spawned into the realm level to drive realm-local events and presentation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Realm")
	TSubclassOf<ARpgPortalRealmEventDirector> RealmEventDirectorClass;

	/** Tags copied into the completion message for reward, quest or world-state listeners. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	FGameplayTagContainer CompletionTags;

	/** Stability value shown/tracked when the encounter reaches full completion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Stability", meta = (ClampMin = "1.0"))
	float MaxStability = 3.0f;

	/** Main interaction text while the realm portal can be entered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText EnterInteractionText;

	/** Supporting interaction text while the realm portal can be entered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText EnterInteractionSubText;

	/** Main interaction text shown on the spawned realm exit portal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText ExitInteractionText;

	/** Supporting interaction text shown on the spawned realm exit portal. */
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
