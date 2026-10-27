#pragma once

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/UI/RpgMvvmListEntryWidgets.h"

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

/** Records skill tree request rejections in arrival order. Test-only. */
UCLASS(NotBlueprintable, Transient)
class URpgSkillTreeAutomationTestRejectionListener final : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleRejected(FGameplayTag TreeTag, FGameplayTag NodeTag, ERpgSkillTreeRequestRejection Reason)
	{
		NodeTags.Add(NodeTag);
		Reasons.Add(Reason);
	}

	TArray<FGameplayTag> NodeTags;
	TArray<ERpgSkillTreeRequestRejection> Reasons;
};

/** Records the skill tree map's selected-node box reports. Test-only. */
UCLASS(NotBlueprintable, Transient)
class URpgSkillTreeAutomationTestMapListener final : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleSelectedNodeMoved(bool bHasSelection, FVector2D NodePosition, FVector2D NodeSize, FVector2D MapSize)
	{
		++Broadcasts;
		bLastHasSelection = bHasSelection;
		LastPosition = NodePosition;
		LastSize = NodeSize;
		LastMapSize = MapSize;
	}

	int32 Broadcasts = 0;
	bool bLastHasSelection = false;
	FVector2D LastPosition = FVector2D::ZeroVector;
	FVector2D LastSize = FVector2D::ZeroVector;
	FVector2D LastMapSize = FVector2D::ZeroVector;
};

/** Native skill tree map entry without authored layout; its entry item is the node view model it shows. Test-only. */
UCLASS(NotBlueprintable, Transient)
class URpgSkillTreeAutomationTestMapEntry final : public URpgMvvmListEntryWidget
{
	GENERATED_BODY()
};
