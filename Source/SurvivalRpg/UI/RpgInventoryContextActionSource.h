#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryContextActionTypes.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"
#include "UObject/Interface.h"

#include "RpgInventoryContextActionSource.generated.h"

/** Presentation source represented by one immutable context-menu checkout. */
UENUM()
enum class ERpgInventoryContextActionSourceKind : uint8
{
	None,
	SpatialEntry,
	Address,
	Equipment
};

/**
 * Read-only, exact source snapshot captured when an inventory context menu opens.
 *
 * The action array is a transient capability projection. Stable item, entry, placement, address, quantity, and
 * equipment identities are re-queried before dispatch so a pooled modal cannot act on a replaced presentation.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgInventoryContextActionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	ERpgInventoryContextActionSourceKind SourceKind =
		ERpgInventoryContextActionSourceKind::None;

	UPROPERTY(Transient)
	FGuid EntryId;

	UPROPERTY(Transient)
	FRpgInventoryItemId ItemId;

	UPROPERTY(Transient)
	FRpgInventoryGridPlacement SourcePlacement;

	UPROPERTY(Transient)
	FRpgInventorySlotAddress SlotAddress;

	UPROPERTY(Transient)
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;

	UPROPERTY(Transient)
	int32 StackCount = 0;

	UPROPERTY(Transient)
	int32 QuickAccessSlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<ERpgInventoryContextAction> Actions;

	bool IsValid() const
	{
		if (SourceKind == ERpgInventoryContextActionSourceKind::None ||
			!ItemId.IsValid() ||
			Actions.IsEmpty())
		{
			return false;
		}

		switch (SourceKind)
		{
		case ERpgInventoryContextActionSourceKind::SpatialEntry:
			return EntryId.IsValid() &&
				SourcePlacement.IsValid() &&
				StackCount > 0;

		case ERpgInventoryContextActionSourceKind::Address:
			return EntryId.IsValid() &&
				SourcePlacement.IsValid() &&
				SlotAddress.IsValid() &&
				StackCount > 0;

		case ERpgInventoryContextActionSourceKind::Equipment:
			return EquipmentSlot != ERpgEquipmentSlot::None;

		case ERpgInventoryContextActionSourceKind::None:
		default:
			return false;
		}
	}

	/** Compares only stable represented state; transient action availability is re-queried independently. */
	bool MatchesStableSource(const FRpgInventoryContextActionSnapshot& Other) const
	{
		return IsValid() &&
			Other.IsValid() &&
			SourceKind == Other.SourceKind &&
			EntryId == Other.EntryId &&
			ItemId == Other.ItemId &&
			SourcePlacement == Other.SourcePlacement &&
			SlotAddress == Other.SlotAddress &&
			EquipmentSlot == Other.EquipmentSlot &&
			StackCount == Other.StackCount;
	}
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URpgInventoryContextActionSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Native presentation seam shared by spatial Content, logical Address/Carry, and Gear sources.
 *
 * Implementations query the screen coordinator for current capabilities and dispatch only after comparing the exact
 * source snapshot again. The interface never mutates inventory directly.
 */
class SURVIVALRPG_API IRpgInventoryContextActionSource
{
	GENERATED_BODY()

public:
	virtual bool QueryInventoryContextActions(
		FRpgInventoryContextActionSnapshot& OutSnapshot) const = 0;

	virtual bool ExecuteInventoryContextAction(
		const FRpgInventoryContextActionSnapshot& ExpectedSnapshot,
		ERpgInventoryContextAction Action,
		int32 QuickAccessSlotIndex = INDEX_NONE) = 0;
};
