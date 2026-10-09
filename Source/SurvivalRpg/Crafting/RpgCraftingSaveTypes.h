#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "RpgCraftingSaveTypes.generated.h"

class URpgCraftingRecipeDefinition;
class URpgInventoryItemDefinition;

/** Durable credit for materials paid into the unit in progress; never stores runtime component pointers. */
USTRUCT()
struct SURVIVALRPG_API FRpgCraftingRefundSaveData
{
	GENERATED_BODY()

	/** Definition of an ordinary, losslessly consumable material. */
	UPROPERTY(SaveGame)
	TSoftClassPtr<URpgInventoryItemDefinition> ItemDefinition;

	/** Units owed back if the order stops before the paid unit completes. */
	UPROPERTY(SaveGame)
	int32 Count = 0;

	/** Stable original chest identity; an absent chest falls back to other connected chests. */
	UPROPERTY(SaveGame)
	FName InventoryId;
};

/** The station's one order, with relative time so unloaded worlds never produce items offline. */
USTRUCT()
struct SURVIVALRPG_API FRpgCraftingOrderSaveData
{
	GENERATED_BODY()

	/** Stable identity used by stop and target commands. */
	UPROPERTY(SaveGame)
	FGuid OrderId;

	/** Authored recipe asset, loaded before the order may resume. */
	UPROPERTY(SaveGame)
	TSoftObjectPtr<URpgCraftingRecipeDefinition> Recipe;

	/** Persistent container id of the chest receiving the outputs; None stores automatically. */
	UPROPERTY(SaveGame)
	FName TargetContainerId;

	/** Requested units and units whose outputs were committed. */
	UPROPERTY(SaveGame)
	int32 QuantityTotal = 0;

	UPROPERTY(SaveGame)
	int32 QuantityCompleted = 0;

	/** Serialized ERpgCraftingOrderState value; validated before restore. */
	UPROPERTY(SaveGame)
	uint8 State = 0;

	/** Explicit pause survives reload. */
	UPROPERTY(SaveGame)
	bool bPaused = false;

	/** True when the unit in progress has already consumed its materials. */
	UPROPERTY(SaveGame)
	bool bUnitPaid = false;

	/** Seconds left on the paid unit when the snapshot was captured. */
	UPROPERTY(SaveGame)
	float RemainingTime = 0.0f;

	/** Exactly one unit's costs while bUnitPaid, otherwise empty. */
	UPROPERTY(SaveGame)
	TArray<FRpgCraftingRefundSaveData> UnitCredits;
};

/** Station order saved in the same world snapshot as the chests' inventory graphs. */
USTRUCT()
struct SURVIVALRPG_API FRpgCraftingStationSaveData
{
	GENERATED_BODY()

	/** Stable placed or built station identity. */
	UPROPERTY(SaveGame)
	FName StationId;

	/** False for an idle station. */
	UPROPERTY(SaveGame)
	bool bHasOrder = false;

	/** The order, valid only while bHasOrder. */
	UPROPERTY(SaveGame)
	FRpgCraftingOrderSaveData Order;
};
