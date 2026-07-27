#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgInventoryItemTypes.h"
#include "RpgInventoryGraphTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgPlayerInventoryLayoutTypes.generated.h"

class UTexture2D;
class URpgActionBarComponent;
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

	/** Stable root or item-owned graph address. This is the only authoritative runtime identity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Layout")
	FRpgInventoryContainerHandle ContainerHandle;

	/** Zero-based grid cell X coordinate inside ContainerHandle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Layout", meta = (ClampMin = "0", UIMin = "0"))
	int32 X = INDEX_NONE;

	/** Zero-based grid cell Y coordinate inside ContainerHandle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory|Layout", meta = (ClampMin = "0", UIMin = "0"))
	int32 Y = INDEX_NONE;

	FRpgInventoryContainerHandle GetContainerHandle() const
	{
		return ContainerHandle;
	}

	void SetContainerHandle(const FRpgInventoryContainerHandle& InHandle)
	{
		ContainerHandle = InHandle;
		ContainerId_DEPRECATED = NAME_None;
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

private:
	friend class URpgActionBarComponent;

	/** Historical root/local container id read only by the authoritative Quick Access save converter. */
	UPROPERTY(SaveGame)
	FName ContainerId_DEPRECATED = NAME_None;

	/** Promotes a legacy-only root address or clears its matching dual-written shadow; conflicts fail closed. */
	bool TryMigrateDeprecatedRootForSave()
	{
		if (ContainerId_DEPRECATED.IsNone())
		{
			return true;
		}

		const bool bCanonicalHandleIsUnset =
			ContainerHandle.Root.IsNone() &&
			!ContainerHandle.ItemOwnerId.IsValid() &&
			ContainerHandle.ContainerId.IsNone() &&
			ContainerHandle.Depth == 0;
		if (bCanonicalHandleIsUnset)
		{
			const FRpgInventoryContainerHandle LegacyRoot =
				FRpgInventoryContainerHandle::MakeRoot(ContainerId_DEPRECATED);
			if (!LegacyRoot.IsValid())
			{
				return false;
			}

			ContainerHandle = LegacyRoot;
		}
		else if (!ContainerHandle.IsValid() ||
			ContainerHandle.ContainerId != ContainerId_DEPRECATED)
		{
			return false;
		}

		ContainerId_DEPRECATED = NAME_None;
		return true;
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

	/** Definition-local grid id. Runtime addresses combine it with root or provider-item identity in ContainerHandle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FName ContainerId = NAME_None;

	/**
	 * Stable semantic role used to find this static group without interpreting ContainerId.
	 * Roles are exact, layout-local singleton keys. Leave empty for generic groups and item-provided content.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (Categories = "Rpg.Inventory.Layout.Role"))
	FGameplayTag SemanticRole;

	/** Player-facing label shown by inventory screens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FText DisplayName;

	/** Optional icon for group headers or equipment-slot placeholders. UI-only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Layout area this group belongs to. Content accepts auto-added inventory items; gear and carry require explicit moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	ERpgInventorySlotGroupKind GroupKind = ERpgInventorySlotGroupKind::Content;

	/**
	 * Explicit gameplay equipment role for this static Gear or Carry group.
	 * Gear uses a non-hand slot, Carry uses MainHand or OffHand, and Content must remain None.
	 * This is immutable definition data; ContainerId and item categories never imply equipment behavior.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	ERpgEquipmentSlot EquipmentSlotRole = ERpgEquipmentSlot::None;

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

	/** Definition-local id used for labels and designer-authored group semantics; never use it as graph identity. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FName ContainerId = NAME_None;

	/** Exact designer-authored role used for stable static-group lookup; invalid for generic or item-owned groups. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FGameplayTag SemanticRole;

	/** Player-facing label shown by inventory screens. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FText DisplayName;

	/** Optional icon for group headers or equipment-slot placeholders. UI-only. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Layout area this group belongs to. UI uses this to split gear, carry, and item storage. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	ERpgInventorySlotGroupKind GroupKind = ERpgInventorySlotGroupKind::Content;

	/** Explicit gameplay equipment role copied from static definition data, or None for content/item-owned groups. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	ERpgEquipmentSlot EquipmentSlotRole = ERpgEquipmentSlot::None;

	/** Grid dimensions for this visible container. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FRpgInventoryGridSize GridSize;

	/** Server validation rule for every cell in this container. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FRpgInventorySlotRule Rule;

	/** True when this group was contributed by equipped bag/belt/pouch item data. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	bool bProvidedByEquipment = false;

	/** Typed equipment slot whose item provides this group, or None for static groups. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	ERpgEquipmentSlot SourceEquipmentSlot = ERpgEquipmentSlot::None;

	FRpgInventorySlotAddress MakeAddress(int32 X, int32 Y) const
	{
		FRpgInventorySlotAddress Address;
		Address.SetContainerHandle(ContainerHandle);
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
