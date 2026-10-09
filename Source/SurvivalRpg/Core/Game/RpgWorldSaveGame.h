#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "RpgPlayerSaveData.h"
#include "RpgWorldSaveParticipant.h"
#include "SurvivalRpg/Base/RpgBaseStorageSaveTypes.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageTypes.h"
#include "SurvivalRpg/Crafting/RpgCraftingSaveTypes.h"

#include "RpgWorldSaveGame.generated.h"

/** Save DTO for one persistent physical world container. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgWorldContainerSaveData
{
	GENERATED_BODY()

	/** Stable designer-authored id shared with URpgInventoryContainerComponent::PersistentContainerId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|World Container")
	FName PersistentContainerId = NAME_None;

	/** Blueprint class used to reconstruct player-built chests. Placed containers must retain their authored class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|World Container")
	TSoftClassPtr<AActor> ActorClass;

	/** Confirmed world transform, saved for both authored and player-built physical storage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|World Container")
	FTransform ActorTransform = FTransform::Identity;

	/** Complete chest settings; grid dimensions are applied before its item graph is reconstructed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|World Container")
	FRpgPhysicalStorageMetadata Metadata;

	/** Complete authoritative contents, including item-owned child-container subtrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|World Container")
	FRpgInventoryGraphSaveData InventoryGraph;
};

/**
 * Host-authoritative, versioned disk save for player profiles and persistent world containers.
 *
 * The GameMode owns capture/restore and writes immutable instances of this object. Runtime systems remain the source
 * of truth while a session is running; this object is only a reconstruction boundary across host restarts.
 */
UCLASS()
class SURVIVALRPG_API URpgWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/**
	 * Current top-level save schema emitted by this build. V4 adds WorldFeatures; V5 replaces station queues and trays
	 * with one order per station. Older station entries load as idle stations: their trays and queues are not migrated.
	 */
	static constexpr int32 CurrentSchemaVersion = 5;

	/** Physical-storage saves start fresh; prototype quantity-storage saves have no implicit migration. */
	static constexpr int32 MinimumSupportedSchemaVersion = 3;

	/** Selects the top-level migration/validation path before any profile is restored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save", meta = (ClampMin = "1", UIMin = "1"))
	int32 SchemaVersion = CurrentSchemaVersion;

	/** Monotonic host sequence used to choose the newest valid primary, backup, or recovery snapshot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Save")
	int64 SaveSequence = 0;

	/** Player profiles keyed by UniqueNetId string, or the configured stable offline profile key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	TMap<FString, FRpgPlayerSaveData> Players;

	/** Persistent physical world-container graphs keyed by their designer-authored stable id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	TMap<FName, FRpgWorldContainerSaveData> WorldContainers;

	/** Output graphs, persistent production/refund state and saved settings keyed by stable station identity. */
	UPROPERTY(SaveGame)
	TMap<FName, FRpgCraftingStationSaveData> CraftingStations;

	/** Persistent base-storage networks keyed by stable ARpgBaseCampActor::BaseId. Added in schema V2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	TMap<FName, FRpgBaseStorageSaveData> BaseStorages;

	/** World-shared storage knowledge restored into the GameState knowledge component. Added in schema V2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	FGameplayTagContainer StorageKnowledgeTags;

	/**
	 * Durable state of features outside the core schema, keyed by IRpgWorldSaveParticipant::GetWorldSaveFeatureId.
	 * Entries of inactive features are kept unchanged. Added in schema V4.
	 */
	UPROPERTY(SaveGame)
	TMap<FName, FRpgWorldFeatureSaveData> WorldFeatures;

	/** Validates the schema and pointer-free DTO envelopes without mutating any runtime gameplay state. */
	bool ValidateForLoad(FString& OutError) const;
};
