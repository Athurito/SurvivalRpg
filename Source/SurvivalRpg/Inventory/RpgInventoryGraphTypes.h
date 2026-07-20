#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryGraphTypes.generated.h"

class UPackageMap;
class URpgInventoryItemDefinition;

/** Maximum number of item-owned container levels below an inventory root. */
inline constexpr uint8 RpgInventoryMaxItemOwnedDepth = 4;

/**
 * Persistent identity of one concrete inventory item.
 *
 * The server creates the GUID once. It survives placement changes, equipment changes, cross-inventory transfers,
 * reconnects, and disk-save round trips; UI and RPCs may safely address items by this value instead of UObject pointers.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryItemId
{
	GENERATED_BODY()

	/** Persistent server-authored GUID. Runtime code must never reuse the same value for two live items. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Identity")
	FGuid Value;

	FRpgInventoryItemId() = default;
	explicit FRpgInventoryItemId(const FGuid& InValue)
		: Value(InValue)
	{
	}

	/** Creates a new locally generated identity. Authoritative inventory code owns when this is called. */
	static FRpgInventoryItemId NewId()
	{
		return FRpgInventoryItemId(FGuid::NewGuid());
	}

	/** Returns whether this identity contains a usable GUID. */
	bool IsValid() const
	{
		return Value.IsValid();
	}

	/** Clears the identity. Intended for initialization and failed-import rollback only. */
	void Reset()
	{
		Value.Invalidate();
	}

	/** Compact network serializer used by replicated entries and transaction RPCs. */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Value;
		bOutSuccess = !Ar.IsError();
		return bOutSuccess;
	}

	FString ToString() const
	{
		return Value.ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	friend bool operator==(const FRpgInventoryItemId& A, const FRpgInventoryItemId& B)
	{
		return A.Value == B.Value;
	}

	friend bool operator!=(const FRpgInventoryItemId& A, const FRpgInventoryItemId& B)
	{
		return !(A == B);
	}

	friend uint32 GetTypeHash(const FRpgInventoryItemId& ItemId)
	{
		return GetTypeHash(ItemId.Value);
	}
};

template<>
struct TStructOpsTypeTraits<FRpgInventoryItemId> : public TStructOpsTypeTraitsBase2<FRpgInventoryItemId>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

/**
 * Stable address of either an inventory root grid or a grid owned by a concrete item.
 *
 * Root handles use Root and ContainerId with an invalid ItemOwnerId at depth zero. Item-owned handles use the
 * owning item's persistent id plus a definition-local ContainerId; Root remains None so a filled bag can move
 * between roots without rewriting every descendant address. Depth is validated server-side and capped at four.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryContainerHandle
{
	GENERATED_BODY()

	/** Hard gameplay limit for item-owned levels below a root. RPC and save import must validate against this value. */
	static constexpr uint8 MaxItemOwnedDepth = RpgInventoryMaxItemOwnedDepth;

	/** Root grid id such as Pockets, Gear.Head, Carry.Weapon1, or a persistent storage id; None for item-owned grids. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Container")
	FName Root = NAME_None;

	/** Persistent id of the item that owns this grid; invalid for root grids. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Container")
	FRpgInventoryItemId ItemOwnerId;

	/** Root id for root grids or definition-local grid id for item-owned containers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Container")
	FName ContainerId = NAME_None;

	/** Number of item-owned levels below a root. Zero is a root; valid item-owned values are 1 through 4. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Container", meta = (ClampMin = "0", ClampMax = "4", UIMin = "0", UIMax = "4"))
	uint8 Depth = 0;

	/** Creates a handle for one top-level inventory grid. */
	static FRpgInventoryContainerHandle MakeRoot(FName RootId)
	{
		FRpgInventoryContainerHandle Result;
		Result.Root = RootId;
		Result.ContainerId = RootId;
		return Result;
	}

	/** Creates a handle for one definition-local grid owned by a concrete item. Invalid depth is retained for validation. */
	static FRpgInventoryContainerHandle MakeItemOwned(
		const FRpgInventoryItemId& OwnerId,
		FName LocalContainerId,
		uint8 InDepth)
	{
		FRpgInventoryContainerHandle Result;
		Result.ItemOwnerId = OwnerId;
		Result.ContainerId = LocalContainerId;
		Result.Depth = InDepth;
		return Result;
	}

	/** Returns whether this is a well-formed root-grid handle. */
	bool IsRoot() const
	{
		return !Root.IsNone() && Root == ContainerId && !ItemOwnerId.IsValid() && Depth == 0;
	}

	/** Returns whether this is a well-formed item-owned grid within the supported nesting limit. */
	bool IsItemOwned() const
	{
		return Root.IsNone() && ItemOwnerId.IsValid() && !ContainerId.IsNone() &&
			Depth > 0 && Depth <= MaxItemOwnedDepth;
	}

	/** Returns whether this handle may be resolved by the authoritative inventory graph. */
	bool IsValid() const
	{
		return IsRoot() || IsItemOwned();
	}

	/** Returns whether a further item-owned container level can be created below this container. */
	bool CanContainChildContainer() const
	{
		return IsValid() && Depth < MaxItemOwnedDepth;
	}

	/** Returns the depth a direct item-owned child would have, or zero when the limit has already been reached. */
	uint8 GetDirectChildDepth() const
	{
		return CanContainChildContainer() ? static_cast<uint8>(Depth + 1) : 0;
	}

	/** Compact network serializer used by replicated entries and transaction RPCs. */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Root;
		bool bItemIdSuccess = true;
		ItemOwnerId.NetSerialize(Ar, Map, bItemIdSuccess);
		Ar << ContainerId;
		Ar << Depth;
		bOutSuccess = bItemIdSuccess && !Ar.IsError();
		return bOutSuccess;
	}

	FString ToString() const
	{
		if (IsRoot())
		{
			return FString::Printf(TEXT("Root:%s"), *Root.ToString());
		}

		return FString::Printf(
			TEXT("Item:%s/%s@%u"),
			*ItemOwnerId.ToString(),
			*ContainerId.ToString(),
			static_cast<uint32>(Depth));
	}

	friend bool operator==(const FRpgInventoryContainerHandle& A, const FRpgInventoryContainerHandle& B)
	{
		return A.Root == B.Root &&
			A.ItemOwnerId == B.ItemOwnerId &&
			A.ContainerId == B.ContainerId &&
			A.Depth == B.Depth;
	}

	friend bool operator!=(const FRpgInventoryContainerHandle& A, const FRpgInventoryContainerHandle& B)
	{
		return !(A == B);
	}

	friend uint32 GetTypeHash(const FRpgInventoryContainerHandle& Handle)
	{
		uint32 Hash = HashCombine(GetTypeHash(Handle.Root), GetTypeHash(Handle.ItemOwnerId));
		Hash = HashCombine(Hash, GetTypeHash(Handle.ContainerId));
		return HashCombine(Hash, GetTypeHash(Handle.Depth));
	}
};

template<>
struct TStructOpsTypeTraits<FRpgInventoryContainerHandle> : public TStructOpsTypeTraitsBase2<FRpgInventoryContainerHandle>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
	int32 Width = 1;

	/** Height in grid cells. Values below 1 are invalid for runtime placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
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

	friend bool operator==(const FRpgInventoryGridSize& A, const FRpgInventoryGridSize& B)
	{
		return A.Width == B.Width && A.Height == B.Height;
	}

	friend bool operator!=(const FRpgInventoryGridSize& A, const FRpgInventoryGridSize& B)
	{
		return !(A == B);
	}
};

/**
 * One concrete server-authored item placement inside an inventory graph container.
 *
 * ContainerHandle is the authoritative graph address. ContainerId remains serialized and synchronized as a legacy
 * Blueprint/UI seam while old assets and RPCs migrate from root-only grids to item-owned nested containers.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryGridPlacement
{
	GENERATED_BODY()

	/** Stable root or item-owned graph address. Invalid values fall back to legacy root ContainerId during migration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial")
	FRpgInventoryContainerHandle ContainerHandle;

	/** Legacy root/local grid id retained for existing Blueprints; SetContainerHandle keeps it synchronized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial")
	FName ContainerId = NAME_None;

	/** Top-left occupied grid cell X coordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial", meta = (ClampMin = "0", UIMin = "0"))
	int32 X = INDEX_NONE;

	/** Top-left occupied grid cell Y coordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial", meta = (ClampMin = "0", UIMin = "0"))
	int32 Y = INDEX_NONE;

	/** Unrotated item footprint width in cells. Runtime occupancy swaps width/height when bRotated is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
	int32 Width = 1;

	/** Unrotated item footprint height in cells. Runtime occupancy swaps width/height when bRotated is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial", meta = (ClampMin = "1", UIMin = "1"))
	int32 Height = 1;

	/** Whether the item footprint is rotated 90 degrees clockwise for this placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Spatial")
	bool bRotated = false;

	/** Returns the graph handle, adapting an existing ContainerId-only placement to a root handle when necessary. */
	FRpgInventoryContainerHandle GetContainerHandle() const
	{
		return ContainerHandle.IsValid()
			? ContainerHandle
			: FRpgInventoryContainerHandle::MakeRoot(ContainerId);
	}

	/** Assigns the graph handle and keeps the legacy ContainerId seam synchronized. */
	void SetContainerHandle(const FRpgInventoryContainerHandle& InContainerHandle)
	{
		ContainerHandle = InContainerHandle;
		ContainerId = InContainerHandle.ContainerId;
	}

	bool IsValid() const
	{
		return GetContainerHandle().IsValid() && X >= 0 && Y >= 0 && Width > 0 && Height > 0;
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
		if (!IsValid() || !Other.IsValid() || GetContainerHandle() != Other.GetContainerHandle())
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

	friend bool operator==(const FRpgInventoryGridPlacement& A, const FRpgInventoryGridPlacement& B)
	{
		return A.GetContainerHandle() == B.GetContainerHandle() &&
			A.X == B.X && A.Y == B.Y && A.Width == B.Width && A.Height == B.Height &&
			A.bRotated == B.bRotated;
	}

	friend bool operator!=(const FRpgInventoryGridPlacement& A, const FRpgInventoryGridPlacement& B)
	{
		return !(A == B);
	}
};

/** Authoritative operation requested from the inventory transaction planner. */
UENUM(BlueprintType)
enum class ERpgInventoryMutationOperation : uint8
{
	None,
	Move,
	Rotate,
	Merge,
	Swap,
	Split,
	Sort,
	Equip,
	Pickup,
	Transfer,
	Drop,
	Consume
};

/** Stable reason code returned by inventory planning and authoritative commits. */
UENUM(BlueprintType)
enum class ERpgInventoryMutationResultCode : uint8
{
	Success,
	PartiallyApplied,
	InvalidRequest,
	AuthorityRequired,
	ItemNotFound,
	SourceMismatch,
	InvalidContainer,
	InvalidPlacement,
	OutOfBounds,
	Occupied,
	NoSpace,
	StackIncompatible,
	StackLimitReached,
	ItemNotAllowed,
	NestedContainersNotAllowed,
	CycleDetected,
	MaxDepthExceeded,
	DuplicateItemId,
	InternalError
};

/** Kind of authoritative state change represented by one transaction delta. */
UENUM(BlueprintType)
enum class ERpgInventoryMutationDeltaKind : uint8
{
	None,
	Added,
	Removed,
	Moved,
	Rotated,
	StackChanged
};

/**
 * One concrete before/after change produced by the transaction planner.
 *
 * Results may contain several deltas for merge, swap, split, pickup, and whole-subtree transfer operations. UI reads
 * these as confirmation data; only the authoritative inventory graph applies them.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryMutationDelta
{
	GENERATED_BODY()

	/** Type of state change described by this delta. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	ERpgInventoryMutationDeltaKind Kind = ERpgInventoryMutationDeltaKind::None;

	/** Persistent identity of the changed item. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	FRpgInventoryItemId ItemId;

	/** Container address before the mutation; invalid when an item is newly added. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	FRpgInventoryContainerHandle BeforeContainer;

	/** Container address after the mutation; invalid when an item is removed. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	FRpgInventoryContainerHandle AfterContainer;

	/** Grid placement before the mutation. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	FRpgInventoryGridPlacement BeforePlacement;

	/** Grid placement after the mutation. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	FRpgInventoryGridPlacement AfterPlacement;

	/** Stack count before the mutation, or zero when newly added. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	int32 PreviousQuantity = 0;

	/** Stack count after the mutation, or zero when removed. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	int32 NewQuantity = 0;
};

/**
 * Serializable request consumed by both inventory preview planning and the public authoritative mutation path.
 * UObject pointers are intentionally excluded so client RPCs cannot smuggle ownership assumptions across inventories.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryMutationRequest
{
	GENERATED_BODY()

	/** Requested planner operation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Transaction")
	ERpgInventoryMutationOperation Operation = ERpgInventoryMutationOperation::None;

	/** Persistent identity of the primary item; may be invalid for a container-wide Sort operation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Transaction")
	FRpgInventoryItemId ItemId;

	/** Expected source container used for validation and stale-request rejection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Transaction")
	FRpgInventoryContainerHandle Source;

	/** Requested destination container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Transaction")
	FRpgInventoryContainerHandle Target;

	/** Requested item or stack amount. Whole-item operations normally use the current full stack count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Transaction", meta = (ClampMin = "0", UIMin = "0"))
	int32 Quantity = 1;

	/** Requested top-left grid placement and rotation inside Target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Transaction")
	FRpgInventoryGridPlacement TargetPlacement;

	/** Client-generated id used to correlate preview, server acknowledgement, rejection, and replicated confirmation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Transaction")
	FGuid RequestId;

	/** Assigns a request id when one was not already provided. */
	void EnsureRequestId()
	{
		if (!RequestId.IsValid())
		{
			RequestId = FGuid::NewGuid();
		}
	}
};

/** Authoritative transaction outcome including the exact state changes that were committed. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryMutationResult
{
	GENERATED_BODY()

	/** Correlation id copied from the request. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	FGuid RequestId;

	/** Operation evaluated by the planner. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	ERpgInventoryMutationOperation Operation = ERpgInventoryMutationOperation::None;

	/** Machine-readable success or rejection reason used by UI feedback and tests. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	ERpgInventoryMutationResultCode Code = ERpgInventoryMutationResultCode::InvalidRequest;

	/** Amount originally requested. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	int32 RequestedQuantity = 0;

	/** Amount actually committed; may be smaller only for explicitly partial stack pickups. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	int32 AppliedQuantity = 0;

	/** Exact before/after state changes committed by the authoritative graph. Empty for rejected requests. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Transaction")
	TArray<FRpgInventoryMutationDelta> Deltas;

	/** Returns true for full or explicitly partial success. */
	bool IsSuccess() const
	{
		return Code == ERpgInventoryMutationResultCode::Success ||
			Code == ERpgInventoryMutationResultCode::PartiallyApplied;
	}
};

/** Versioned opaque state emitted and consumed by one item-definition fragment. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryFragmentStatePayload
{
	GENERATED_BODY()

	/** Stable fragment-owned identifier; must remain unchanged across compatible save migrations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	FName FragmentId = NAME_None;

	/** Fragment-specific payload schema version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence", meta = (ClampMin = "1", UIMin = "1"))
	int32 Version = 1;

	/** Opaque fragment-owned bytes. Import validates every payload before committing the graph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	TArray<uint8> Payload;
};

/** Save DTO for one flattened inventory-graph entry. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySavedItem
{
	GENERATED_BODY()

	/** Persistent item identity restored before equipment and quick-access bindings are resolved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	FRpgInventoryItemId ItemId;

	/** Soft class reference to the static item definition; missing definitions invalidate the complete import. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	TSoftClassPtr<URpgInventoryItemDefinition> ItemDefinition;

	/** Saved stack count validated against the definition's stack contract during import. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence", meta = (ClampMin = "1", UIMin = "1"))
	int32 StackCount = 1;

	/** Root or item-owned container that physically owns this entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	FRpgInventoryContainerHandle Container;

	/** Saved top-left grid placement, footprint, and rotation inside Container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	FRpgInventoryGridPlacement Placement;

	/** Versioned runtime state supplied by definition fragments such as durability, affixes, sockets, or rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	TArray<FRpgInventoryFragmentStatePayload> RuntimeState;
};

/** Versioned flattened inventory graph used as an atomic disk-save/import boundary. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryGraphSaveData
{
	GENERATED_BODY()

	/** Current core graph schema written by this build. */
	static constexpr int32 CurrentSchemaVersion = 1;

	/** Core graph schema version used to select validation and migration before import. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence", meta = (ClampMin = "1", UIMin = "1"))
	int32 SchemaVersion = CurrentSchemaVersion;

	/** Flat entries whose container handles reconstruct the root/child graph after full validation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Persistence")
	TArray<FRpgInventorySavedItem> Items;
};
