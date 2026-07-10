#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgInventoryItemTypes.h"
#include "RpgInventorySpatialTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgPlayerInventoryLayoutTypes.generated.h"

class UTexture2D;
class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;

/**
 * Stable logical address for one visible player-inventory slot.
 *
 * The address is UI/save friendly. Server gameplay resolves it to the current spatial grid cell
 * through URpgPlayerInventoryLayoutComponent before moving, binding, or activating items.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySlotAddress
{
	GENERATED_BODY()

	/** Stable root or item-owned graph address. Invalid legacy values fall back to ContainerId as a root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FRpgInventoryContainerHandle ContainerHandle;

	/** Logical grid container, for example WeaponSlot1, Belt, Backpack, or Pockets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FName ContainerId = NAME_None;

	/** Zero-based grid cell X coordinate inside ContainerId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (ClampMin = "0", UIMin = "0"))
	int32 X = INDEX_NONE;

	/** Zero-based grid cell Y coordinate inside ContainerId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (ClampMin = "0", UIMin = "0"))
	int32 Y = INDEX_NONE;

	FRpgInventoryContainerHandle GetContainerHandle() const
	{
		return ContainerHandle.IsValid() ? ContainerHandle : FRpgInventoryContainerHandle::MakeRoot(ContainerId);
	}

	void SetContainerHandle(const FRpgInventoryContainerHandle& InHandle)
	{
		ContainerHandle = InHandle;
		ContainerId = InHandle.ContainerId;
	}

	bool IsValid() const { return GetContainerHandle().IsValid() && X >= 0 && Y >= 0; }

	friend bool operator==(const FRpgInventorySlotAddress& A, const FRpgInventorySlotAddress& B)
	{
		return A.GetContainerHandle() == B.GetContainerHandle() && A.X == B.X && A.Y == B.Y;
	}

	friend bool operator!=(const FRpgInventorySlotAddress& A, const FRpgInventorySlotAddress& B)
	{
		return !(A == B);
	}

	friend uint32 GetTypeHash(const FRpgInventorySlotAddress& Address)
	{
		return HashCombine(HashCombine(GetTypeHash(Address.GetContainerHandle()), GetTypeHash(Address.X)), GetTypeHash(Address.Y));
	}
};

/** High-level player-inventory layout area used to keep gear, carry, and stored items visually distinct. */
UENUM(BlueprintType)
enum class ERpgInventorySlotGroupKind : uint8
{
	/** Regular item storage such as Pockets, Backpack contents, Belt contents, or Pouches. */
	Content,

	/** Persistent carry slot for weapons, shields, and tools that can be activated into runtime hands. */
	Carry,

	/** Dedicated equipped gear slot such as Head, Chest, Backpack, or Belt. */
	Gear
};

/**
 * Server-side rule for what may live in a player inventory slot group.
 *
 * Designers configure categories and tags on static body slots or bag-provided slots. UI may preview these rules,
 * but the server always validates again before moving items.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySlotRule
{
	GENERATED_BODY()

	/** Allowed broad item categories. Empty means every category is allowed unless blocked by tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	TArray<ERpgInventoryItemCategory> AllowedCategories;

	/** ItemTraits.ItemTags that must all be present for the slot to accept the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FGameplayTagContainer RequiredItemTags;

	/** ItemTraits.ItemTags that make the slot reject the item when any are present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FGameplayTagContainer BlockedItemTags;

	/** True when this slot group may be bound to the 1..8 actionbar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	bool bActionbarBindable = false;

	/** True when this slot group represents a weapon/tool/shield carry slot that can become active hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	bool bCarrySlot = false;

	/** Semantic equipment role activated by this carry group, normally MainHand or OffHand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (EditCondition = "bCarrySlot", Categories = "Equipment.Slot"))
	FGameplayTag CarryActivationRole;

	/** Returns whether the item satisfies this slot rule. Null items are never accepted. */
	bool AllowsItem(const URpgInventoryItemInstance* Item) const;

	/** Returns whether an item definition satisfies this slot rule before an instance is created. */
	bool AllowsItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
};

/**
 * Definition data for one visible grid container.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySlotGroupDefinition
{
	GENERATED_BODY()

	/** Stable grid id used in FRpgInventorySlotAddress and replicated item placements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FName ContainerId = NAME_None;

	/** Player-facing label shown by inventory screens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FText DisplayName;

	/** Optional icon for group headers or equipment-slot placeholders. UI-only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Layout area this group belongs to. Content accepts auto-added inventory items; gear and carry require explicit moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	ERpgInventorySlotGroupKind GroupKind = ERpgInventorySlotGroupKind::Content;

	/** Grid dimensions contributed by this container. Gear and carry containers should remain 1x1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (ClampMin = "1", UIMin = "1"))
	FRpgInventoryGridSize GridSize;

	/** Validation rule used by server moves and actionbar binding requests for cells in this container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FRpgInventorySlotRule Rule;
};

/**
 * Runtime view of one grid container after fixed and item-provided containers are resolved.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySlotGroupView
{
	GENERATED_BODY()

	/** Stable root or item-owned container address represented by this visible group. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FRpgInventoryContainerHandle ContainerHandle;

	/** Stable grid id used in FRpgInventorySlotAddress and replicated item placements. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FName ContainerId = NAME_None;

	/** Player-facing label shown by inventory screens. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FText DisplayName;

	/** Optional icon for group headers or equipment-slot placeholders. UI-only. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Layout area this group belongs to. UI uses this to split gear, carry, and item storage. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	ERpgInventorySlotGroupKind GroupKind = ERpgInventorySlotGroupKind::Content;

	/** Grid dimensions for this visible container. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FRpgInventoryGridSize GridSize;

	/** Server validation rule for every cell in this container. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FRpgInventorySlotRule Rule;

	/** True when this group was contributed by equipped bag/belt/pouch item data. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	bool bProvidedByEquipment = false;

	/** Equipment slot name whose item provides this group, or None for built-in body slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FName SourceEquipmentSlotName = NAME_None;

	FRpgInventorySlotAddress MakeAddress(int32 X, int32 Y) const
	{
		FRpgInventorySlotAddress Address;
		Address.SetContainerHandle(ContainerHandle.IsValid() ? ContainerHandle : FRpgInventoryContainerHandle::MakeRoot(ContainerId));
		Address.X = X;
		Address.Y = Y;
		return Address;
	}

	bool ContainsCell(int32 X, int32 Y) const
	{
		return GridSize.IsValid() &&
			X >= 0 &&
			Y >= 0 &&
			X < GridSize.Width &&
			Y < GridSize.Height;
	}
};
