#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgInventoryItemTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgPlayerInventoryLayoutTypes.generated.h"

class UTexture2D;
class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;

/**
 * Stable logical address for one visible player-inventory slot.
 *
 * The address is UI/save friendly. Server gameplay resolves it to the current global SortIndex slot
 * through URpgPlayerInventoryLayoutComponent before moving, binding, or activating items.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySlotAddress
{
	GENERATED_BODY()

	/** Logical slot group, for example WeaponSlot1, Belt, Backpack, or Pockets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FName GroupId = NAME_None;

	/** Zero-based slot inside GroupId. Values below 0 are invalid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (ClampMin = "0", UIMin = "0"))
	int32 LocalSlotIndex = INDEX_NONE;

	bool IsValid() const { return !GroupId.IsNone() && LocalSlotIndex >= 0; }

	friend bool operator==(const FRpgInventorySlotAddress& A, const FRpgInventorySlotAddress& B)
	{
		return A.GroupId == B.GroupId && A.LocalSlotIndex == B.LocalSlotIndex;
	}

	friend bool operator!=(const FRpgInventorySlotAddress& A, const FRpgInventorySlotAddress& B)
	{
		return !(A == B);
	}

	friend uint32 GetTypeHash(const FRpgInventorySlotAddress& Address)
	{
		return HashCombine(GetTypeHash(Address.GroupId), GetTypeHash(Address.LocalSlotIndex));
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

	/** Returns whether the item satisfies this slot rule. Null items are never accepted. */
	bool AllowsItem(const URpgInventoryItemInstance* Item) const;

	/** Returns whether an item definition satisfies this slot rule before an instance is created. */
	bool AllowsItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
};

/**
 * Definition data for one slot group before it is assigned a global inventory range.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySlotGroupDefinition
{
	GENERATED_BODY()

	/** Stable group id used in FRpgInventorySlotAddress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FName GroupId = NAME_None;

	/** Player-facing label shown by inventory screens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FText DisplayName;

	/** Optional icon for group headers or equipment-slot placeholders. UI-only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Layout area this group belongs to. Content accepts auto-added inventory items; gear and carry require explicit moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	ERpgInventorySlotGroupKind GroupKind = ERpgInventorySlotGroupKind::Content;

	/** Number of slots contributed by this group. Values below 1 are ignored at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout", meta = (ClampMin = "1", UIMin = "1"))
	int32 SlotCount = 1;

	/** Validation rule used by server moves and actionbar binding requests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Layout")
	FRpgInventorySlotRule Rule;
};

/**
 * Runtime view of one slot group after the layout maps it onto global inventory SortIndex slots.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySlotGroupView
{
	GENERATED_BODY()

	/** Stable group id used in FRpgInventorySlotAddress. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FName GroupId = NAME_None;

	/** Player-facing label shown by inventory screens. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FText DisplayName;

	/** Optional icon for group headers or equipment-slot placeholders. UI-only. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Layout area this group belongs to. UI uses this to split gear, carry, and item storage. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	ERpgInventorySlotGroupKind GroupKind = ERpgInventorySlotGroupKind::Content;

	/** First global SortIndex slot represented by this group. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	int32 FirstGlobalSlotIndex = INDEX_NONE;

	/** Number of contiguous global slots represented by this group. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	int32 SlotCount = 0;

	/** Server validation rule for every slot in this group. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FRpgInventorySlotRule Rule;

	/** True when this group was contributed by equipped bag/belt/pouch item data. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	bool bProvidedByEquipment = false;

	/** Equipment slot name whose item provides this group, or None for built-in body slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	FName SourceEquipmentSlotName = NAME_None;

	FRpgInventorySlotAddress MakeAddress(int32 LocalSlotIndex) const
	{
		FRpgInventorySlotAddress Address;
		Address.GroupId = GroupId;
		Address.LocalSlotIndex = LocalSlotIndex;
		return Address;
	}

	bool ContainsGlobalSlotIndex(int32 GlobalSlotIndex) const
	{
		return FirstGlobalSlotIndex != INDEX_NONE &&
			GlobalSlotIndex >= FirstGlobalSlotIndex &&
			GlobalSlotIndex < FirstGlobalSlotIndex + SlotCount;
	}
};
