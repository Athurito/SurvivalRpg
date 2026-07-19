#pragma once

#include "CoreMinimal.h"

#include "RpgInventoryContextActionTypes.generated.h"

/**
 * Semantic actions that an inventory presentation may offer for its currently represented source.
 *
 * Availability is a read-only client projection used for menus and controller hints. Gameplay mutation remains
 * server-authoritative and must be revalidated by the inventory action component.
 */
UENUM(BlueprintType)
enum class ERpgInventoryContextAction : uint8
{
	/** Opens the item's nested container presentation. */
	OpenContainer,

	/** Opens read-only item details. */
	Inspect,

	/** Executes the item's configured usable-item ability. */
	Use,

	/** Moves the item to its default equipment destination and activates it when applicable. */
	EquipAndActivate,

	/** Moves an equippable item to the first compatible Carry role. */
	MoveToCarry,

	/** Splits part of a stack into separate inventory space. */
	Split,

	/** Rotates a spatial item through a grid presentation that supports rotation. */
	Rotate,

	/** Binds or changes the item's Quick Access assignment. */
	QuickAccessBind,

	/** Clears the item's current Quick Access assignment. */
	QuickAccessUnbind,

	/** Quick-transfers the item to the currently resolved destination. */
	Transfer,

	/** Requests a manual world drop, including confirmation policy when configured. */
	Drop,

	/** Moves an equipped item back into compatible player-content space. */
	Unequip
};
