#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

class URpgEquipmentDefinition;
class URpgInventoryItemInstance;

/**
 * Shared compatibility rules for placing inventory-owned items into Gear and Carry roles.
 *
 * This policy is stateless: URpgInventoryManagerComponent remains the physical inventory authority,
 * while equipment/loadout and UI use the same read-only compatibility decision.
 */
struct SURVIVALRPG_API FRpgInventoryEquipmentPlacementPolicy
{
	/** Returns true for every Gear or runtime hand slot managed by the RPG inventory/equipment bridge. */
	static bool IsManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Returns true for MainHand and OffHand activation roles. */
	static bool IsHandEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Returns true for equipment slots whose item contributes item-owned inventory containers. */
	static bool IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Resolves an exact Carry activation role to its corresponding runtime hand slot. */
	static bool TryGetHandSlotForCarryRole(FGameplayTag CarryRole, ERpgEquipmentSlot& OutEquipmentSlot);

	/** Returns the static equipment definition referenced by the item's equippable fragment, if configured. */
	static const URpgEquipmentDefinition* FindEquipmentDefinition(const URpgInventoryItemInstance* Item);

	/**
	 * Returns whether the item is compatible with the requested Gear or hand role.
	 *
	 * Slot-container equipment additionally requires an ItemContainer fragment. Definition-less providers retain
	 * their explicit legacy compatibility until the versioned inventory migration removes that fallback.
	 */
	static bool CanItemUseEquipmentSlot(const URpgInventoryItemInstance* Item, ERpgEquipmentSlot EquipmentSlot);
};
