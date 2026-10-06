#pragma once

#include "RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgInventoryFragment_SkillTree.generated.h"

class URpgSkillTreeDefinition;

/**
 * Static item data that makes a weapon or tool carry a skill tree.
 *
 * Every item referencing the same tree shares its progress, which the character owns in URpgSkillTreeComponent. While
 * the item is equipped in one of ActiveInSlots, the equipment manager grants the abilities, effects and tags of the
 * learned nodes with the item's equipment instance as source, and Q/E/R follow the tree's slot assignment.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_SkillTree final : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	URpgInventoryFragment_SkillTree();

	/** Tree whose learned nodes this item grants. Designer-owned static data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TObjectPtr<const URpgSkillTreeDefinition> SkillTree = nullptr;

	/** Equipment slots in which the item counts as in use and grants its tree. Main hand by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TArray<ERpgEquipmentSlot> ActiveInSlots;

	/** Returns whether the item grants its tree while equipped in Slot. */
	bool IsActiveInSlot(ERpgEquipmentSlot Slot) const;

	/** Returns the skill tree of ItemInstance's definition, or null when it carries none. */
	static const URpgSkillTreeDefinition* FindSkillTreeOfItem(const UObject* ItemInstance);

	/** Returns ItemInstance's skill tree when the item grants it in Slot, otherwise null. */
	static const URpgSkillTreeDefinition* FindActiveSkillTreeOfItem(const UObject* ItemInstance, ERpgEquipmentSlot Slot);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
