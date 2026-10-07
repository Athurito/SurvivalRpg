#pragma once

#include "Components/GameStateComponent.h"
#include "SurvivalRpg/Core/Game/RpgWorldSaveParticipant.h"

#include "RpgHarvestPersistenceComponent.generated.h"

class AActor;
class ARpgGameModeBase;
class URpgHarvestableComponent;
class URpgHarvestableInstancesComponent;
class URpgHarvestInstanceStockComponent;
class UWorld;

/** Saved stock of one harvested resource. Pristine resources are never saved. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestSavedStock
{
	GENERATED_BODY()

	/** Revision of the saved stock; requests aimed at an older revision are rejected as stale. */
	UPROPERTY(SaveGame)
	int32 Revision = 0;

	/** Stock sections already extracted in the saved revision. */
	UPROPERTY(SaveGame)
	int32 HarvestedSections = 0;

	/** False while the resource is depleted and waits for its respawn. */
	UPROPERTY(SaveGame)
	bool bActive = true;

	/**
	 * Game-time seconds left until a depleted resource respawns. The countdown pauses while the host is offline.
	 * Negative when the resource stays depleted, because its profile has no respawn.
	 */
	UPROPERTY(SaveGame)
	float RespawnSeconds = -1.0f;

	/** Returns whether this is the authored, fully stocked state, which needs no save entry. */
	bool IsPristine() const { return bActive && HarvestedSections <= 0; }
};

/** Saved stock of one actor-backed resource placed in a map. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestSavedNode
{
	GENERATED_BODY()

	/** Stable id from URpgHarvestPersistenceComponent::MakeNodeId: the actor's name in its map. */
	UPROPERTY(SaveGame)
	FName NodeId = NAME_None;

	UPROPERTY(SaveGame)
	FRpgHarvestSavedStock Stock;
};

/** Saved stock of one instanced resource, keyed like URpgHarvestInstanceStockComponent. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestSavedInstance
{
	GENERATED_BODY()

	/** Authored world location of the instance in whole centimeters, the stock component's stable key. */
	UPROPERTY(SaveGame)
	int32 KeyX = 0;

	UPROPERTY(SaveGame)
	int32 KeyY = 0;

	UPROPERTY(SaveGame)
	int32 KeyZ = 0;

	UPROPERTY(SaveGame)
	FRpgHarvestSavedStock Stock;

	FIntVector GetKey() const { return FIntVector(KeyX, KeyY, KeyZ); }
};

/** Saved harvest state of one map. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestSavedMap
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FRpgHarvestSavedNode> Nodes;

	UPROPERTY(SaveGame)
	TArray<FRpgHarvestSavedInstance> Instances;
};

/** The harvesting feature's entry in the host's world snapshot. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestSaveData
{
	GENERATED_BODY()

	/** Saved harvest state by map package name, without the PIE prefix. Maps that are not loaded keep their entry. */
	UPROPERTY(SaveGame)
	TMap<FName, FRpgHarvestSavedMap> Maps;
};

/**
 * Saves the stock of harvested resources with the host's world snapshot, so felled trees and emptied veins stay
 * harvested after the host reloads the world.
 *
 * Server-only GameState component, added by the harvesting GameFeature. It registers with the GameMode as an
 * IRpgWorldSaveParticipant. The stock itself stays in URpgHarvestableComponent and URpgHarvestInstanceStockComponent;
 * they report every change here, and this component keeps a sparse record of each resource whose stock differs from
 * its authored state, also while World Partition has streamed the resource out. A depleted resource keeps its
 * remaining respawn time; the countdown pauses while the host is offline.
 *
 * Only resources loaded with the map are saved: actor nodes are identified by their actor name, instances by their
 * authored location. Resources spawned at runtime stay session-scoped.
 */
UCLASS(ClassGroup = (Rpg), meta = (DisplayName = "RPG Harvest Persistence"))
class GF_HARVESTING_MAGIC_API URpgHarvestPersistenceComponent final : public UGameStateComponent, public IRpgWorldSaveParticipant
{
	GENERATED_BODY()

public:
	/** Schema of the serialized FRpgHarvestSaveData payload. */
	static constexpr int32 CurrentSchemaVersion = 1;

	explicit URpgHarvestPersistenceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the persistence component of World's GameState, or null on clients and before the GameFeature added it. */
	static URpgHarvestPersistenceComponent* FindForWorld(const UWorld* World);

	/** Returns the save key of World's map: its package name without the PIE prefix. */
	static FName MakeMapId(const UWorld& World);

	/** Returns the stable save id of a resource actor loaded with its map, or None for actors spawned at runtime. */
	static FName MakeNodeId(const AActor& Actor);

	/** Adds a node that began play on the server and applies its saved stock. */
	void RegisterNode(URpgHarvestableComponent& Node);

	/** Records the final stock of a node that ends play or streams out, and removes it. */
	void UnregisterNode(const URpgHarvestableComponent& Node);

	/** Records the current stock of a registered node after a server change. */
	void RecordNode(const URpgHarvestableComponent& Node);

	/** Records the stock of Key after Instances harvested it, when Instances was loaded with the map. */
	void RecordInstance(
		const URpgHarvestableInstancesComponent& Instances,
		const URpgHarvestInstanceStockComponent& Stock,
		const FIntVector& Key);

	/** Drops the record of an instance whose authored stock was restored, for example by its respawn. */
	void ForgetInstance(const FIntVector& Key);

	/** Applies every saved instance record to a stock component that began play after this component. */
	void ApplyInstanceRecords(URpgHarvestInstanceStockComponent& Stock);

	/** Returns the number of resources in this map whose stock is recorded because it differs from the authored state. */
	int32 GetNumRecordedResources() const { return NodeRecords.Num() + InstanceRecords.Num(); }

	//~ IRpgWorldSaveParticipant interface
	virtual FName GetWorldSaveFeatureId() const override;
	virtual bool CaptureWorldSaveData(FRpgWorldFeatureSaveData& OutData) override;
	virtual bool RestoreWorldSaveData(const FRpgWorldFeatureSaveData* SavedData) override;
	//~ End IRpgWorldSaveParticipant interface

	/** Serializes Data into a payload of CurrentSchemaVersion. */
	static bool WritePayload(const FRpgHarvestSaveData& Data, FRpgWorldFeatureSaveData& OutData);

	/** Reads a payload; false when its schema is unknown or it does not deserialize. */
	static bool ReadPayload(const FRpgWorldFeatureSaveData& Data, FRpgHarvestSaveData& OutData);

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

private:
	/** Server record of one changed resource; the respawn countdown is held as a world-time deadline. */
	struct FRecord
	{
		FRpgHarvestSavedStock Stock;
		double RespawnDeadline = -1.0;
	};

	bool HasPersistenceAuthority() const;
	double GetWorldTime() const;
	FRecord MakeRecord(const FRpgHarvestSavedStock& Stock) const;
	FRpgHarvestSavedStock MakeSavedStock(const FRecord& Record) const;
	bool IsRespawnDue(const FRecord& Record) const;
	void SetNodeRecord(FName NodeId, const FRpgHarvestSavedStock& Stock);
	void SetInstanceRecord(const FIntVector& Key, const FRpgHarvestSavedStock& Stock);
	void ApplyNodeRecord(URpgHarvestableComponent& Node, FName NodeId);
	void ResetAppliedRecords();
	void MarkSaveDirty() const;
	ARpgGameModeBase* GetGameMode() const;

	/** Changed actor nodes of this map by node id, including nodes that are streamed out. Server only. */
	TMap<FName, FRecord> NodeRecords;

	/** Changed instances of this map by stock key, including instances that are streamed out. Server only. */
	TMap<FIntVector, FRecord> InstanceRecords;

	/** Nodes that began play on the server; derived registration. */
	TMap<FName, TWeakObjectPtr<URpgHarvestableComponent>> LiveNodes;

	/** Saved entries of other maps, written back unchanged. */
	TMap<FName, FRpgHarvestSavedMap> OtherMaps;

	/** Save key of the loaded map. */
	FName MapId = NAME_None;

	/** Suppresses dirty marks while saved state is applied. */
	bool bApplyingSavedState = false;
};
