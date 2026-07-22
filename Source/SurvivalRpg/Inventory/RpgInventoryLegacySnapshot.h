// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RpgInventoryGraphTypes.h"

#include "RpgInventoryLegacySnapshot.generated.h"

class URpgInventoryItemDefinition;

/** Explicit schema selector for the root-only inventory snapshots written before graph persistence. */
enum class ERpgLegacyInventorySnapshotVersion : uint8
{
	/** Original single-slot/list snapshot whose SortIndex defined deterministic item order. */
	SingleSlotV0 = 0,

	/** First spatial snapshot whose placement still represented root containers only. */
	SpatialV1 = 1
};

/**
 * One row from a pre-graph inventory snapshot.
 *
 * This reflected DTO exists only so explicitly versioned legacy data can be converted into
 * FRpgInventoryGraphSaveData. Runtime inventory state must never be restored directly from it.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySnapshotEntry
{
	GENERATED_BODY()

	/**
	 * Historical entry identity. Conversion may use a valid value as a one-time deterministic ItemId seed,
	 * but never restores it as a current FastArray entry id.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	FGuid EntryId;

	/** Persistent item identity added late in the legacy snapshot lifetime; valid values are preserved. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Inventory|Snapshot")
	FRpgInventoryItemId ItemId;

	/** Static item definition used to synthesize the legacy row's default fragment runtime state. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Legacy stack quantity. Conversion rejects non-positive values; canonical restore validates definition limits. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	int32 StackCount = 0;

	/**
	 * List position written by SingleSlotV0. Values must be non-negative and unique within one snapshot.
	 * Later spatial snapshots leave this at INDEX_NONE.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot", meta = (DeprecatedProperty, DeprecationMessage = "Only SingleSlotV0 conversion reads SortIndex; current persistence uses FRpgInventoryGraphSaveData."))
	int32 SortIndex = INDEX_NONE;

	/** Root-only placement written by SpatialV1; dimensions are ignored and rebuilt from the current definition. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	FRpgInventoryGridPlacement Placement;
};

/**
 * Root-only inventory snapshot retained solely as an input DTO for explicit legacy conversion.
 * Current saves use the versioned flattened inventory graph and preserve hierarchy plus fragment state.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySnapshot
{
	GENERATED_BODY()

	/** Optional root-container envelope shared by every row; a supplied conversion fallback must agree with it. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	FName ContainerId;

	/** Legacy root rows in their serialized order. The selected legacy version defines how order/placement is read. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	TArray<FRpgInventorySnapshotEntry> Entries;
};
