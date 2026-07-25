#pragma once

#include "CoreMinimal.h"

#include "RpgPlayerInventoryLayoutViewTypes.generated.h"

/** UI-only visual state for one designable spatial inventory cell widget. */
UENUM(BlueprintType)
enum class ERpgInventorySpatialCellVisualState : uint8
{
	/** Normal empty or background cell state. */
	Normal,

	/** Pointer is hovering this cell and no stronger state is active. */
	Hovered,

	/** Logical controller cursor is on this cell. */
	Selected,

	/** Current held or dragged payload can be placed on this cell. */
	ValidPreview,

	/** Current held or dragged payload cannot be placed on this cell. */
	InvalidPreview,

	/** A locally valid placement is awaiting an authoritative server acknowledgement. */
	PendingPreview,

	/** The authoritative server rejected the most recent placement request. */
	RejectedPreview,

	/** Cell is occupied by the origin cell of an item overlay. */
	Occupied,

	/** Cell is covered by a multi-cell item whose origin is elsewhere. */
	Covered
};
