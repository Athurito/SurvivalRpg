#pragma once

#include "Components/GameStateComponent.h"
#include "Harvesting/RpgHarvestInstanceStockShard.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "TimerManager.h"

#include "RpgHarvestInstanceStockComponent.generated.h"

class URpgHarvestableInstancesComponent;
class UWorld;
struct FRpgHarvestSavedStock;

/**
 * Server-authoritative stock of every instanced resource in the world, such as ore veins and trees placed by PCG.
 *
 * Instanced resources live on actors that do not replicate, such as PCG partition actors streamed by World Partition,
 * so their stock cannot replicate with them. This GameState component, added by the harvesting GameFeature, keeps it
 * instead: one FastArray entry per instance whose stock differs from its authored state, keyed by the instance's
 * authored world location in whole centimeters. Untouched instances cost nothing; a restored instance drops its
 * entry again. Server and clients load identical instance transforms, so they derive the same keys without
 * replicating any instance identity, and a component that streams in later applies the stored stock on BeginPlay.
 *
 * The entries replicate through NumShards always-relevant ARpgHarvestInstanceStockShard actors, chosen by a hash of the
 * key, because one replicated property cannot carry thousands of entries to a late joiner (HARV-10c).
 *
 * Representations (URpgHarvestableInstancesComponent) evaluate and commit harvests and present their instances; this
 * component only stores stock and schedules respawns. URpgHarvestPersistenceComponent saves the stock of instances
 * loaded with the map. Instances of a renewable area (FRpgHarvestStockRules::IsInRenewableArea) are restored when the
 * area unloads.
 */
UCLASS(ClassGroup = (Rpg), meta = (DisplayName = "RPG Harvest Instance Stock"))
class GF_HARVESTING_MAGIC_API URpgHarvestInstanceStockComponent final : public UGameStateComponent
{
	GENERATED_BODY()

public:
	/** Changes older than this many seconds are reported as initial state when they reach a client. */
	static constexpr float LiveChangeWindowSeconds = 1.5f;

	/** Longest cosmetic presentation delay of one change, in seconds. */
	static constexpr float MaxPresentationDelaySeconds = 2.5f;

	/**
	 * Number of replicated stock shards. Each shard stays below the engine's per-update and initial-bunch limits up to
	 * about 1,700 entries, so the stock holds roughly 25,000 changed instances before a late joiner could fail.
	 */
	static constexpr int32 NumShards = 16;

	/** Returns the shard that holds Key, the same on every machine. */
	static int32 GetShardIndex(const FIntVector& Key);

	explicit URpgHarvestInstanceStockComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the stock component of World's GameState, or null before the harvesting GameFeature added it. */
	static URpgHarvestInstanceStockComponent* FindForWorld(const UWorld* World);

	/** Returns the stable key of an instance whose authored world location is AuthoredWorldLocation. */
	static FIntVector MakeInstanceKey(const FVector& AuthoredWorldLocation);

	/** Returns the stock of the instance identified by Key for a resource type with SectionCount sections. */
	FRpgHarvestStockSnapshot GetStockSnapshot(const FIntVector& Key, int32 SectionCount) const;

	/**
	 * Records SectionsTaken extracted sections of the instance identified by Key. An instance whose stock runs out
	 * depletes, advances its revision, and is restored after RespawnDelaySeconds; zero or less keeps it depleted for
	 * the session. Every machine presents the change PresentationDelaySeconds after it happened on the server; the
	 * stock itself changes at once. HarvestYawDegrees is the cosmetic direction from the harvester toward the
	 * instance. Authority only; the caller has already delivered the reward. Returns false when nothing changed.
	 */
	bool ExtractSections(
		const FIntVector& Key,
		int32 SectionCount,
		int32 SectionsTaken,
		float RespawnDelaySeconds,
		float PresentationDelaySeconds = 0.0f,
		float HarvestYawDegrees = 0.0f);

	/**
	 * Returns the horizontal unit direction from the last harvester toward the instance identified by Key, the same on
	 * every machine. False for unchanged instances.
	 */
	bool GetHarvestDirection(const FIntVector& Key, FVector& OutDirection) const;

	/**
	 * Returns how many seconds this machine still waits before it presents the last change of Key, measured from the
	 * change's server time so that every machine presents it at the same moment. Zero for unchanged instances.
	 */
	float GetRemainingPresentationDelay(const FIntVector& Key) const;

	/** Restores the authored stock of the instance identified by Key and cancels its respawn. Authority only. */
	bool RestoreStock(const FIntVector& Key);

	/**
	 * Writes the saved form of Key's stock, including the remaining respawn time on the server. False, with the
	 * authored stock written, for unchanged instances.
	 */
	bool ExportSavedStock(const FIntVector& Key, FRpgHarvestSavedStock& OutStock) const;

	/**
	 * Replaces Key's stock with saved stock without loot; every machine presents it as initial state. Pristine stock
	 * drops the entry. A depleted stock with RespawnSeconds of zero or more schedules its respawn. Authority only;
	 * used by URpgHarvestPersistenceComponent.
	 */
	bool ApplySavedStock(const FIntVector& Key, const FRpgHarvestSavedStock& SavedStock);

	/** Returns the number of instances whose stock currently differs from their authored state. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	int32 GetNumChangedInstances() const;

	/** Returns the number of changed instances in the fullest shard, to check that shards stay below their limits. */
	int32 GetMaxShardEntries() const;

	/** Returns whether this machine owns the stock, which is the case on the server only. */
	bool HasStockAuthority() const;

	/** Adds a loaded representation so stock changes reach it, and applies the stock it already has. */
	void RegisterInstances(URpgHarvestableInstancesComponent& Instances);

	/** Removes a representation that streams out or ends play. */
	void UnregisterInstances(const URpgHarvestableInstancesComponent& Instances);

	/**
	 * Restores the authored stock of every instance of Instances and cancels their respawns, for a renewable area that
	 * unloads, such as a portal realm, so it starts over when it loads again. Authority only. Returns the number of
	 * instances restored.
	 */
	int32 RestoreInstances(const URpgHarvestableInstancesComponent& Instances);

	/**
	 * Links a shard to this stock: one the server spawned, or one that replicated to a client. A client presents the
	 * entries the shard already holds, and every update it receives from now on.
	 */
	void RegisterShard(ARpgHarvestInstanceStockShard& Shard);

	/** Forgets a shard that ends play. */
	void UnregisterShard(const ARpgHarvestInstanceStockShard& Shard);

	/** Client: presents one replication update of a registered shard. */
	void HandleShardUpdate(
		const ARpgHarvestInstanceStockShard& Shard,
		TConstArrayView<FIntVector> ChangedKeys,
		TConstArrayView<FIntVector> RemovedKeys);

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

private:
	ARpgHarvestInstanceStockShard* GetShard(int32 ShardIndex) const;
	const ARpgHarvestInstanceStockShard* FindShard(const FIntVector& Key) const;
	ARpgHarvestInstanceStockShard* FindOrSpawnShard(const FIntVector& Key);
	const FRpgHarvestInstanceStockEntry* FindEntry(const FIntVector& Key) const;
	void MarkStockChanged(ARpgHarvestInstanceStockShard& Shard, const FIntVector& Key);
	void NotifyStockChanged(const FIntVector& Key, bool bInitialState);
	bool IsInitialState(const FRpgHarvestInstanceStockEntry& Entry) const;
	float GetServerWorldTimeSeconds() const;
	void ScheduleRespawn(const FIntVector& Key, double Deadline);
	void ArmNextRespawnTimer();
	void HandleRespawnTimer();

	/**
	 * Shards holding the sparse server-authored stock, indexed by GetShardIndex; null until the server spawns one or it
	 * replicates. The shards replicate it to every client for presentation and target previews.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ARpgHarvestInstanceStockShard>> Shards;

	/** Loaded representations; changes are routed to the ones that contain the changed key. Local only. */
	TArray<TWeakObjectPtr<URpgHarvestableInstancesComponent>> RegisteredInstances;

	/** Server-only world-time deadlines of pending respawns. */
	TMap<FIntVector, double> RespawnDeadlines;

	/** World time the respawn timer is armed for; infinity while it is not armed. Server only. */
	double ArmedRespawnDeadline = TNumericLimits<double>::Max();

	/** One timer wakes only for the next due respawn; no tick is used. */
	FTimerHandle RespawnTimerHandle;
};
