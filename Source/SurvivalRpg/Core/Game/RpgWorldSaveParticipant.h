#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "RpgWorldSaveParticipant.generated.h"

/**
 * Durable world state of one feature outside the core save schema, such as a GameFeature's resource stock.
 *
 * The owning feature serializes and validates its payload. The GameMode stores it in the host's world snapshot and
 * keeps it unchanged while the feature is not active, so disabling a GameFeature never erases its saved state.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgWorldFeatureSaveData
{
	GENERATED_BODY()

	/** Feature-owned schema of Payload; positive. Only the owning feature interprets it. */
	UPROPERTY(SaveGame)
	int32 SchemaVersion = 0;

	/** Opaque state serialized by the owning feature. Host-authoritative; never replicated. */
	UPROPERTY(SaveGame)
	TArray<uint8> Payload;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URpgWorldSaveParticipant : public UInterface
{
	GENERATED_BODY()
};

/**
 * Server-side feature object whose durable world state the GameMode saves with the host's world snapshot.
 *
 * A participant registers with ARpgGameModeBase::RegisterWorldSaveParticipant, reports changes through
 * MarkWorldFeatureSaveDirty and unregisters before it ends play. The GameMode captures it whenever it writes a
 * snapshot and restores it from the selected snapshot, including on rollback to the pristine state.
 */
class SURVIVALRPG_API IRpgWorldSaveParticipant
{
	GENERATED_BODY()

public:
	/** Stable key of this feature's entry in the world snapshot. Must not change between builds. */
	virtual FName GetWorldSaveFeatureId() const = 0;

	/** Writes the feature's complete durable state. Returning false blocks disk writes for this session. */
	virtual bool CaptureWorldSaveData(FRpgWorldFeatureSaveData& OutData) = 0;

	/**
	 * Replaces the feature's durable runtime state with SavedData, or with its pristine state when SavedData is null.
	 * Returning false rejects the whole snapshot candidate.
	 */
	virtual bool RestoreWorldSaveData(const FRpgWorldFeatureSaveData* SavedData) = 0;
};
