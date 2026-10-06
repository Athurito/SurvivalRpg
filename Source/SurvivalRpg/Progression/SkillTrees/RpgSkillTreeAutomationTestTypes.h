#pragma once

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgSkillTreeAutomationTestTypes.generated.h"

class URpgSkillTreeDefinition;

/** Editor-only tool item whose SkillTree fragment points at a transient tree supplied by the running test. */
UCLASS(NotBlueprintable, Transient)
class URpgSkillTreeAutomationTestToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgSkillTreeAutomationTestToolDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }

	/** Points the class default fragment at Tree; tests restore null when they finish. */
	static void SetTestSkillTree(const URpgSkillTreeDefinition* Tree);
};

/** Counts broadcasts of a parameterless skill tree view model event. Test-only. */
UCLASS(NotBlueprintable, Transient)
class URpgSkillTreeAutomationTestListener final : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleChanged() { ++Broadcasts; }

	int32 Broadcasts = 0;
};
