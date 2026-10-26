#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/RpgAIAssetTools.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "SurvivalRpg/Core/AI/RpgStateTreeCombatTasks.h"

namespace RpgStateTreeBuilderTests
{
	const TCHAR* Description = TEXT(R"JSON({
		"global_tasks": [{"struct": "/Script/SurvivalRpg.RpgStateTreeCombatControlTask",
			"values": {"TargetPolicy": "Nearest", "TargetSwitchCooldown": 2.5}}],
		"root": {"name": "Combat", "children": [
			{"name": "Recover",
			 "enter_conditions": [{"struct": "/Script/SurvivalRpg.RpgStateTreeOwnerHasTagsCondition",
				"values": {"Tags": "(GameplayTags=((TagName=\"State.Staggered\")))"}}],
			 "tasks": [{"struct": "/Script/SurvivalRpg.RpgStateTreeWaitWhileOwnerTaggedTask"}],
			 "transitions": [{"trigger": "OnStateCompleted", "to": "Combat"}]},
			{"name": "Circle",
			 "tasks": [{"struct": "/Script/SurvivalRpg.RpgStateTreeKeepCombatDistanceTask", "values": {"MinDistance": 300}}],
			 "transitions": [
				{"trigger": "OnEvent", "event": "AI.Event.TargetAttackStarted", "to": "Recover",
				 "conditions": [{"struct": "/Script/StateTreeModule.StateTreeRandomCondition", "values": {"Threshold": 0.4}}]},
				{"trigger": "OnStateCompleted", "to": "Combat"}]}
		]}
	})JSON");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgStateTreeBuilderTest,
	"SurvivalRpg.AI.StateTreeBuilder.BuildsCompilesAndReportsErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStateTreeBuilderTest::RunTest(const FString& Parameters)
{
	using namespace RpgStateTreeBuilderTests;

	UStateTree* StateTree = NewObject<UStateTree>(GetTransientPackage(), NAME_None, RF_Transient);
	const FString Errors = URpgAIAssetTools::BuildStateTreeFromJson(StateTree, Description);
	if (!TestTrue(FString::Printf(TEXT("The description builds and compiles: %s"), *Errors), Errors.IsEmpty()))
	{
		return false;
	}
	TestTrue(TEXT("The compiled tree is ready to run"), StateTree->IsReadyToRun());

	const UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (!TestNotNull(TEXT("Editor data exists"), EditorData) || !TestEqual(TEXT("One root"), EditorData->SubTrees.Num(), 1))
	{
		return false;
	}
	const UStateTreeState* Root = EditorData->SubTrees[0];
	TestEqual(TEXT("Two child states"), Root->Children.Num(), 2);
	TestEqual(TEXT("The event transition and the completion transition"), Root->Children[1]->Transitions.Num(), 2);
	TestEqual(TEXT("The event transition keeps its condition"), Root->Children[1]->Transitions[0].Conditions.Num(), 1);
	if (TestEqual(TEXT("One global task"), EditorData->GlobalTasks.Num(), 1))
	{
		const FRpgStateTreeCombatControlTaskInstanceData& ControlData =
			EditorData->GlobalTasks[0].Instance.Get<FRpgStateTreeCombatControlTaskInstanceData>();
		TestEqual(TEXT("Instance data takes the text values"), ControlData.TargetSwitchCooldown, 2.5f);
		TestTrue(TEXT("Enum values import by name"), ControlData.TargetPolicy == ERpgAICombatTargetPolicy::Nearest);
	}

	FString Broken = Description;
	Broken.ReplaceInline(TEXT("\"to\": \"Recover\""), TEXT("\"to\": \"Missing\""));
	const FString BrokenErrors = URpgAIAssetTools::BuildStateTreeFromJson(StateTree, Broken);
	TestTrue(TEXT("An unknown transition target is reported"), BrokenErrors.Contains(TEXT("no state named 'Missing'")));
	return !HasAnyErrors();
}

#endif
