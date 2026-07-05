#pragma once

#include "CoreMinimal.h"

#include "RpgInventorySpatialTypes.generated.h"

/**
 * Two-dimensional size in inventory grid cells.
 *
 * Designers tune item footprints and container dimensions in cells. Runtime placement is server-authoritative and
 * UI should treat these values as read-only presentation data.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryGridSize
{
	GENERATED_BODY()

	/** Width in grid cells. Values below 1 are invalid for runtime placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
	int32 Width = 1;

	/** Height in grid cells. Values below 1 are invalid for runtime placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
	int32 Height = 1;

	bool IsValid() const
	{
		return Width > 0 && Height > 0;
	}

	FRpgInventoryGridSize GetRotated(bool bRotated) const
	{
		if (!bRotated)
		{
			return *this;
		}

		FRpgInventoryGridSize Result;
		Result.Width = Height;
		Result.Height = Width;
		return Result;
	}
};

/**
 * One concrete server-authored item placement inside an inventory grid container.
 *
 * ContainerId identifies the grid, X/Y is the top-left occupied cell, and Width/Height is the unrotated item
 * footprint stored with the entry so late-joining clients can reconstruct occupancy without loading UI widgets.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryGridPlacement
{
	GENERATED_BODY()

	/** Stable grid id such as Gear.Head, WeaponSlot1, Pockets, Backpack, or a world storage id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial")
	FName ContainerId = NAME_None;

	/** Top-left occupied grid cell X coordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial", meta = (ClampMin = "0", UIMin = "0"))
	int32 X = INDEX_NONE;

	/** Top-left occupied grid cell Y coordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial", meta = (ClampMin = "0", UIMin = "0"))
	int32 Y = INDEX_NONE;

	/** Unrotated item footprint width in cells. Runtime occupancy swaps width/height when bRotated is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
	int32 Width = 1;

	/** Unrotated item footprint height in cells. Runtime occupancy swaps width/height when bRotated is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
	int32 Height = 1;

	/** Whether the item footprint is rotated 90 degrees clockwise for this placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spatial")
	bool bRotated = false;

	bool IsValid() const
	{
		return !ContainerId.IsNone() && X >= 0 && Y >= 0 && Width > 0 && Height > 0;
	}

	FRpgInventoryGridSize GetUnrotatedSize() const
	{
		FRpgInventoryGridSize Result;
		Result.Width = Width;
		Result.Height = Height;
		return Result;
	}

	FRpgInventoryGridSize GetOccupiedSize() const
	{
		return GetUnrotatedSize().GetRotated(bRotated);
	}

	bool ContainsCell(int32 CellX, int32 CellY) const
	{
		const FRpgInventoryGridSize OccupiedSize = GetOccupiedSize();
		return IsValid() &&
			CellX >= X &&
			CellY >= Y &&
			CellX < X + OccupiedSize.Width &&
			CellY < Y + OccupiedSize.Height;
	}

	bool Overlaps(const FRpgInventoryGridPlacement& Other) const
	{
		if (!IsValid() || !Other.IsValid() || ContainerId != Other.ContainerId)
		{
			return false;
		}

		const FRpgInventoryGridSize ThisSize = GetOccupiedSize();
		const FRpgInventoryGridSize OtherSize = Other.GetOccupiedSize();
		return X < Other.X + OtherSize.Width &&
			X + ThisSize.Width > Other.X &&
			Y < Other.Y + OtherSize.Height &&
			Y + ThisSize.Height > Other.Y;
	}
};
