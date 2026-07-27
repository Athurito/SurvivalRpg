#pragma once

#include "CoreMinimal.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryInteractionScreenWidget;
class URpgInventoryPanelNavigationCoordinator;

/**
 * One transient interaction context owned by an active inventory screen.
 *
 * The value is passed atomically to presentation leaves and is never saved or replicated. Gameplay state and
 * authority remain in the inventory, layout, equipment, and UI-action components.
 */
struct SURVIVALRPG_API FRpgInventoryScreenPresentationContext
{
	/** Screen-scoped command/query coordinator shared by every inventory presentation leaf. */
	URpgInventoryDragDropCoordinator* DragDropCoordinator = nullptr;

	/** Screen-scoped CommonUI panel navigator shared by every focusable inventory presentation leaf. */
	URpgInventoryPanelNavigationCoordinator* PanelNavigationCoordinator = nullptr;

	/** Screen that exclusively owns transient context-menu, split, and drop-confirmation presentation. */
	URpgInventoryInteractionScreenWidget* PresentationHost = nullptr;

	/** True only while all three screen-owned presentation dependencies are available. */
	bool IsComplete() const
	{
		return DragDropCoordinator != nullptr &&
			PanelNavigationCoordinator != nullptr &&
			PresentationHost != nullptr;
	}
};
