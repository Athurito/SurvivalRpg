#include "RpgInventoryFragment_SkillTree.h"

#include "RpgInventoryItemInstance.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_SkillTree)

#define LOCTEXT_NAMESPACE "RpgInventoryFragment_SkillTree"

URpgInventoryFragment_SkillTree::URpgInventoryFragment_SkillTree()
{
	ActiveInSlots.Add(ERpgEquipmentSlot::MainHand);
}

bool URpgInventoryFragment_SkillTree::IsActiveInSlot(const ERpgEquipmentSlot Slot) const
{
	return SkillTree && Slot != ERpgEquipmentSlot::None && ActiveInSlots.Contains(Slot);
}

const URpgSkillTreeDefinition* URpgInventoryFragment_SkillTree::FindSkillTreeOfItem(const UObject* ItemInstance)
{
	const URpgInventoryItemInstance* Item = Cast<URpgInventoryItemInstance>(ItemInstance);
	const URpgInventoryFragment_SkillTree* Fragment = Item ? Item->FindFragmentByClass<URpgInventoryFragment_SkillTree>() : nullptr;
	return Fragment ? Fragment->SkillTree.Get() : nullptr;
}

const URpgSkillTreeDefinition* URpgInventoryFragment_SkillTree::FindActiveSkillTreeOfItem(
	const UObject* ItemInstance,
	const ERpgEquipmentSlot Slot)
{
	const URpgInventoryItemInstance* Item = Cast<URpgInventoryItemInstance>(ItemInstance);
	const URpgInventoryFragment_SkillTree* Fragment = Item ? Item->FindFragmentByClass<URpgInventoryFragment_SkillTree>() : nullptr;
	return Fragment && Fragment->IsActiveInSlot(Slot) ? Fragment->SkillTree.Get() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryFragment_SkillTree::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	if (!SkillTree)
	{
		Context.AddError(LOCTEXT("MissingTree", "The SkillTree fragment needs a skill tree."));
		Result = EDataValidationResult::Invalid;
	}
	if (ActiveInSlots.IsEmpty() || ActiveInSlots.Contains(ERpgEquipmentSlot::None))
	{
		Context.AddError(LOCTEXT("InvalidSlots", "ActiveInSlots needs at least one real equipment slot."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
