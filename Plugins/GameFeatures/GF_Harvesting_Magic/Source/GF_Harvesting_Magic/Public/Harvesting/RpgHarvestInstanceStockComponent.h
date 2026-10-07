#pragma once

#include "Components/GameStateComponent.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "TimerManager.h"

#include "RpgHarvestInstanceStockComponent.generated.h"

class URpgHarvestableInstancesComponent;
class URpgHarvestInstanceStockComponent;
class FLifetimeProperty;
class UWorld;
struct FRpgHarvestSavedStock;

/** Replicated stock of one instanced resource whose stock differs from its authored, fully stocked state. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestInstanceStockEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Stable instance key: the authored world location of the instance in whole centimeters. */
	UPROPERTY()
	FIntVector Key = FIntVector::ZeroValue;

	/**
	 * Revision of the instance's current stock; advances when the instance depletes. A restored instance drops its
	 * entry and starts again at zero.
	 */
	UPROPERTY()
	int32 Revision = 0;

	/** Stock sections already extracted in the current revision. */
	UPROPERTY()
	uint8 HarvestedSections = 0;

	/** False while the instance is depleted and waits for its respawn. */
	UPROPERTY()
	bool bActive = true;

	/** Server world time in seconds of the last change; lets late joiners present old changes without animating. */
	UPROPERTY()
	float LastChangeServerTime = 0.0f;

	/** Cosmetic delay of the last change's presentation after LastChangeServerTime, in hundredths of a second. */
	UPROPERTY()
	uint8 PresentationDelayCentiseconds = 0;

	/**
	 * Cosmetic horizontal direction from the last harvester toward the instance, as a yaw in 256 steps, so every
	 * machine presents the change the same way, for example felling a tree away from whoever felled it.
	 */
	UPROPERTY()
	uint8 HarvestYaw = 0;
};

/** FastArray holding only instanced resources whose stock currently differs from their authored state. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestInstanceStockList : public FFastArraySerializer
{
	GENERATED_BODY()

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgHarvestInstanceStockEntry, FRpgHarvestInstanceStockList>(
			Entries,
			DeltaParams,
			*this);
	}

private:
	friend class URpgHarvestInstanceStockComponent;

	UPROPERTY()
	TArray<FRpgHarvestInstanceStockEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<URpgHarvestInstanceStockComponent> OwnerComponent = nullptr;

	/** Keys removed by the current replication update, presented once the update has been applied. */
	TArray<FIntVector> PendingRemovedKeys;
};

template<>
struct TStructOpsTypeTraits<FRpgHarvestInstanceStockList> : public TStructOpsTypeTraitsBase2<FRpgHarvestInstanceStockList>
{
	enum { WithNetDeltaSerializer = true };
};

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
 * Representations (URpgHarvestableInstancesComponent) evaluate and commit harvests and present their instances; this
 * component only stores stock and schedules respawns. URpgHarvestPersistenceComponent saves the stock of instances
 * loaded with the map.
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
	int32 GetNumChangedInstances() const { return Stock.Entries.Num(); }

	/** Returns whether this machine owns the stock, which is the case on the server only. */
	bool HasStockAuthority() const;

	/** Adds a loaded representation so stock changes reach it, and applies the stock it already has. */
	void RegisterInstances(URpgHarvestableInstancesComponent& Instances);

	/** Removes a representation that streams out or ends play. */
	void UnregisterInstances(const URpgHarvestableInstancesComponent& Instances);

protected:
	//~ UActorComponent interface
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent interface

private:
	friend struct FRpgHarvestInstanceStockList;

	const FRpgHarvestInstanceStockEntry* FindEntry(const FIntVector& Key) const;
	int32 FindEntryIndex(const FIntVector& Key) const;
	void MarkStockChanged(const FIntVector& Key);
	void NotifyStockChanged(const FIntVector& Key, bool bInitialState);
	bool IsInitialState(const FRpgHarvestInstanceStockEntry& Entry) const;
	float GetServerWorldTimeSeconds() const;
	void ArmNextRespawnTimer();
	void HandleRespawnTimer();

	/** Sparse server-authored stock; replicated to every client for presentation and target previews. */
	UPROPERTY(Replicated)
	FRpgHarvestInstanceStockList Stock;

	/** Loaded representations; changes are routed to the ones that contain the changed key. Local only. */
	TArray<TWeakObjectPtr<URpgHarvestableInstancesComponent>> RegisteredInstances;

	/** Server-only world-time deadlines of pending respawns. */
	TMap<FIntVector, double> RespawnDeadlines;

	/** One timer wakes only for the next due respawn; no tick is used. */
	FTimerHandle RespawnTimerHandle;
};
