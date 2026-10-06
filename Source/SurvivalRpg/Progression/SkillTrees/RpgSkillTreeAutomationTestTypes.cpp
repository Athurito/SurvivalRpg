#include "RpgSkillTreeAutomationTestTypes.h"

#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeAutomationTestTypes)

URpgSkillTreeAutomationTestToolDefinition::URpgSkillTreeAutomationTestToolDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("RpgSkillTreeAutomation", "ToolName", "Automation Skill Tree Tool");
	Fragments.Add(CreateDefaultSubobject<URpgInventoryFragment_SkillTree>(TEXT("SkillTreeFragment")));
}

void URpgSkillTreeAutomationTestToolDefinition::SetTestSkillTree(const URpgSkillTreeDefinition* Tree)
{
	URpgSkillTreeAutomationTestToolDefinition* Definition = GetMutableDefault<URpgSkillTreeAutomationTestToolDefinition>();
	for (URpgInventoryItemFragment* Fragment : Definition->Fragments)
	{
		if (URpgInventoryFragment_SkillTree* SkillTreeFragment = Cast<URpgInventoryFragment_SkillTree>(Fragment))
		{
			SkillTreeFragment->SkillTree = Tree;
		}
	}
}
