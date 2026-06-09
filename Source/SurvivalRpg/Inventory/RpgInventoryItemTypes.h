#pragma once

#include "CoreMinimal.h"

#include "RpgInventoryItemTypes.generated.h"

/**
 * Broad gameplay category used by inventory UI, quickbar validation, crafting, and drop rules.
 */
UENUM(BlueprintType)
enum class ERpgInventoryItemCategory : uint8
{
	None,
	Material,
	Weapon,
	Shield,
	Armor,
	Consumable,
	Tool,
	Rune,
	Quest,
	Misc
};

/**
 * Per-item rule used when a player dies and the game decides which backpack items become world loot.
 */
UENUM(BlueprintType)
enum class ERpgInventoryDeathDropRule : uint8
{
	/** Never drop this item on player death. Use this for equipment, quest items, and unique progression objects. */
	Never,

	/** Drop this item only when the active player death-drop mode is MaterialsOnly or broader. */
	MaterialsOnly,

	/** Drop this item only when the active player death-drop mode allows non-equipment backpack items. */
	BackpackOnly
};

/**
 * Runtime player-facing setting for what the player drops on death.
 */
UENUM(BlueprintType)
enum class ERpgPlayerDeathDropMode : uint8
{
	/** Player death does not drop inventory items. */
	None,

	/** Player death drops items marked as materials only. This is the default V1 survival rule. */
	MaterialsOnly,

	/** Player death drops normal backpack items but never equipped or equippable gear. */
	AllBackpackExceptEquipment
};
