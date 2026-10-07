#pragma once

#include "GameFramework/Info.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "RpgHarvestInstanceStockShard.generated.h"

class ARpgHarvestInstanceStockShard;
class FLifetimeProperty;
class URpgHarvestInstanceStockComponent;

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

/** FastArray of one stock shard: the instanced resources whose stock currently differs from their authored state. */
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

	/** Returns the entry index of Key, or INDEX_NONE. Clients rebuild the key index once after each update. */
	int32 FindIndex(const FIntVector& Key) const;

	/** Appends an entry for Key in its authored state and returns its index. Authority only. */
	int32 AddEntry(const FIntVector& Key);

	/** Removes the entry at EntryIndex. Authority only. */
	void RemoveEntryAt(int32 EntryIndex);

	const TArray<FRpgHarvestInstanceStockEntry>& GetEntries() const { return Entries; }
	FRpgHarvestInstanceStockEntry& GetEntry(const int32 EntryIndex) { return Entries[EntryIndex]; }
	const FRpgHarvestInstanceStockEntry& GetEntry(const int32 EntryIndex) const { return Entries[EntryIndex]; }

private:
	friend class ARpgHarvestInstanceStockShard;

	UPROPERTY()
	TArray<FRpgHarvestInstanceStockEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<ARpgHarvestInstanceStockShard> OwnerShard = nullptr;

	/** Keys added, changed or removed by the current replication update, presented once the update has been applied. */
	TArray<FIntVector> PendingChangedKeys;
	TArray<FIntVector> PendingRemovedKeys;

	/** Entry index by key. Kept current on the server; rebuilt on clients after an update changed the array. */
	mutable TMap<FIntVector, int32> IndexByKey;
	mutable bool bIndexDirty = false;
};

template<>
struct TStructOpsTypeTraits<FRpgHarvestInstanceStockList> : public TStructOpsTypeTraitsBase2<FRpgHarvestInstanceStockList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * One replicated share of the world's instance stock (URpgHarvestInstanceStockComponent).
 *
 * The stock of thousands of changed instances does not fit one replicated property: a FastArray update carries at most
 * 2,048 changes, and an actor's initial replication to a late joiner must fit one 64 KB bunch, about 1,700 entries. The
 * stock component therefore spreads its entries over a fixed number of these always-relevant actors by a hash of the
 * instance key, so each shard replicates over its own channel and stays well below both limits. The server spawns a
 * shard when the first instance of its share changes; clients receive the shards and hand their updates to the stock
 * component. Runtime-only: never placed, saved or designer-facing.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class GF_HARVESTING_MAGIC_API ARpgHarvestInstanceStockShard final : public AInfo
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestInstanceStockShard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Index of this shard among the stock's shards, from 0 to URpgHarvestInstanceStockComponent::NumShards - 1. */
	int32 GetShardIndex() const { return ShardIndex; }

	/** Assigns the shard index before the server finishes spawning the shard. Authority only. */
	void InitializeShard(int32 InShardIndex);

	const FRpgHarvestInstanceStockList& GetStock() const { return Stock; }

	/** Mutable stock for the server's stock component; marks nothing dirty by itself. */
	FRpgHarvestInstanceStockList& GetMutableStock() { return Stock; }

	/** Returns whether the stock component of this world presents this shard's updates. */
	bool IsRegistered() const { return RegisteredStock.IsValid(); }

	/** Links this shard to the stock component that presents its updates; the component calls it. */
	void SetRegisteredStock(URpgHarvestInstanceStockComponent* InStock) { RegisteredStock = InStock; }

protected:
	//~ AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor interface

private:
	friend struct FRpgHarvestInstanceStockList;

	/** Client: hands one applied replication update to the registered stock component. */
	void HandleReplicatedUpdate(TConstArrayView<FIntVector> ChangedKeys, TConstArrayView<FIntVector> RemovedKeys);

	/** This shard's share of the server-authored stock; replicated to every client. */
	UPROPERTY(Replicated)
	FRpgHarvestInstanceStockList Stock;

	/** Shard index; replicated once with the shard. */
	UPROPERTY(Replicated)
	uint8 ShardIndex = 0;

	/** Stock component that presents this shard's updates. Local only. */
	TWeakObjectPtr<URpgHarvestInstanceStockComponent> RegisteredStock;
};
