#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "RpgCraftingSaveTypes.generated.h"

class URpgCraftingRecipeDefinition;
class URpgInventoryItemDefinition;

/** Durable credit for materials paid into an unfinished recipe; never stores runtime component pointers. */
USTRUCT()
struct SURVIVALRPG_API FRpgCraftingRefundSaveData
{
	GENERATED_BODY()

	/** Definition of an ordinary, losslessly consumable material. */
	UPROPERTY(SaveGame)
	TSoftClassPtr<URpgInventoryItemDefinition> ItemDefinition;

	/** Units still owed if the unfinished job is canceled. */
	UPROPERTY(SaveGame)
	int32 Count = 0;

	/** Stable original inventory identity; an absent source falls back to the station tray. */
	UPROPERTY(SaveGame)
	FName InventoryId;
};

/** One persisted job, with relative time so unloaded worlds never produce items offline. */
USTRUCT()
struct SURVIVALRPG_API FRpgCraftingJobSaveData
{
	GENERATED_BODY()

	/** Stable identity used by cancellation and UI commands. */
	UPROPERTY(SaveGame)
	FGuid JobId;

	/** Authored recipe asset, loaded before this job may resume. */
	UPROPERTY(SaveGame)
	TSoftObjectPtr<URpgCraftingRecipeDefinition> Recipe;

	/** Requested recipe units and units whose outputs were committed. */
	UPROPERTY(SaveGame)
	int32 QuantityTotal = 0;

	UPROPERTY(SaveGame)
	int32 QuantityCompleted = 0;

	/** Serialized ERpgCraftingJobState value; validated before restore. */
	UPROPERTY(SaveGame)
	uint8 State = 0;

	/** Seconds left on the active unit when the snapshot was captured. */
	UPROPERTY(SaveGame)
	float RemainingTime = 0.0f;

	/** Remaining credits are retained even when no refund inventory has space. */
	UPROPERTY(SaveGame)
	TArray<FRpgCraftingRefundSaveData> Refunds;
};

/** Station settings and queue saved in the same world snapshot as physical inventory graphs. */
USTRUCT()
struct SURVIVALRPG_API FRpgCraftingStationSaveData
{
	GENERATED_BODY()

	/** Stable placed or built station identity. */
	UPROPERTY(SaveGame)
	FName StationId;

	/** Persisted player choice; retaining outputs in the tray is the default. */
	UPROPERTY(SaveGame)
	bool bAutoDepositOutputs = false;

	/** Explicit station pause survives reload. */
	UPROPERTY(SaveGame)
	bool bPaused = false;

	/** Physical tray dimensions restored before its item graph. */
	UPROPERTY(SaveGame)
	FRpgInventoryGridSize OutputGridSize;

	/** Tray items and identities captured with the queue. */
	UPROPERTY(SaveGame)
	FRpgInventoryGraphSaveData OutputInventoryGraph;

	/** Ordered unfinished jobs and their exact refund claims. */
	UPROPERTY(SaveGame)
	TArray<FRpgCraftingJobSaveData> Jobs;
};
