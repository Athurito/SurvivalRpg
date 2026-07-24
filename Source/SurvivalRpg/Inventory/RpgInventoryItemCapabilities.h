#pragma once

#include "CoreMinimal.h"
#include "RpgInventoryItemTypes.h"
#include "RpgInventorySpatialTypes.h"

class URpgInventoryFragment_UsableItem;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;

/**
 * Read-only result of evaluating one requested use against immutable item data and current source quantity.
 *
 * This value is derived on demand and is never replicated, saved, or cached by presentation code. The backing
 * fragment remains immutable definition data; the server still owns access, GAS activation, and item consumption.
 */
enum class ERpgInventoryUseCapabilityResult : uint8
{
	Available,
	NotConfigured,
	WrongInventory,
	InvalidRequest,
	InsufficientQuantity
};

/** Ephemeral use evaluation shared by local presentation policy and authoritative request validation. */
struct SURVIVALRPG_API FRpgInventoryUseCapabilityEvaluation
{
	/** Immutable definition contract used by the server after a successful evaluation. */
	const URpgInventoryFragment_UsableItem* UseContract = nullptr;

	/** Semantic result of the definition, source, request-count, and represented-quantity checks. */
	ERpgInventoryUseCapabilityResult Result =
		ERpgInventoryUseCapabilityResult::NotConfigured;

	/** Total inventory units required by the requested number of uses. Zero is valid for reusable items. */
	int32 RequiredConsumeCount = 0;

	bool IsAvailable() const
	{
		return Result == ERpgInventoryUseCapabilityResult::Available;
	}
};

/** Ephemeral projection of the sole valid SpatialItem definition contract. */
struct SURVIVALRPG_API FRpgInventorySpatialCapability
{
	FRpgInventorySpatialCapability()
	{
		Footprint.Width = 0;
		Footprint.Height = 0;
	}

	/** Unrotated footprint in inventory grid cells, or an invalid size when the definition contract is malformed. */
	FRpgInventoryGridSize Footprint;

	/** Whether the valid definition contract permits rotation. */
	bool bAllowRotation = false;

	bool IsValid() const
	{
		return Footprint.IsValid();
	}
};

/**
 * Stateless resolver for item-definition semantics consumed by inventory presentation and server validation.
 *
 * It deliberately exposes only derived capabilities. Dynamic source validity, placement, access, pending requests,
 * equipment conflicts, and GAS acceptance remain with their existing authoritative systems.
 */
struct SURVIVALRPG_API FRpgInventoryItemCapabilities
{
	/** Returns whether the effective first-match ItemContainer contract exposes at least one usable runtime grid. */
	static bool HasItemContainerContract(
		const URpgInventoryItemInstance* Item);

	/**
	 * Returns whether an effective UsableItem fragment exists.
	 *
	 * This is intentionally weaker than EvaluateUse and preserves legacy Quick Access authoring semantics, where the
	 * layout recognizes the fragment contract before the server validates its configured ability.
	 */
	static bool HasUsableContract(
		const URpgInventoryItemInstance* Item);

	/**
	 * Evaluates the shared definition/source/quantity portion of an item-use request.
	 *
	 * UseCount is the number of activations requested, while AvailableStackCount is the current represented quantity.
	 * Inventory access, item identity, ASC availability, ability requirements, and actual consumption are not checked.
	 */
	static FRpgInventoryUseCapabilityEvaluation EvaluateUse(
		const URpgInventoryItemInstance* Item,
		const URpgInventoryManagerComponent* SourceInventory,
		const URpgInventoryManagerComponent* PlayerInventory,
		int32 AvailableStackCount,
		int32 UseCount = 1);

	/** Resolves the same manual-drop policy used by presentation and authoritative root/subtree validation. */
	static ERpgInventoryManualDropPolicy ResolveManualDropPolicy(
		const URpgInventoryItemInstance* Item);

	/** Resolves the sole valid spatial footprint and rotation rule, failing closed on malformed definition data. */
	static FRpgInventorySpatialCapability ResolveSpatial(
		const URpgInventoryItemInstance* Item);

	/**
	 * Returns whether the primary quick action should dispatch Use instead of EquipAndActivate.
	 *
	 * bHasDefaultEquipmentDestination must come from the shared equipment placement policy. This preserves the
	 * existing usable-only, equippable-only, hybrid, and malformed-fragment fallback behavior.
	 */
	static bool ShouldUseAsPrimaryAction(
		const URpgInventoryItemInstance* Item,
		bool bHasDefaultEquipmentDestination);
};
