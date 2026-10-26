#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RpgStateTreeCombatTasks.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgMeleeAttackStepCountTest,
	"SurvivalRpg.AI.MeleeAttack.StepCountFollowsTheWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMeleeAttackStepCountTest::RunTest(const FString& Parameters)
{
	const TArray<float> Weights = { 0.5f, 0.0f, 0.25f, 0.25f };
	TestEqual(TEXT("The start of the range picks one step"), FRpgStateTreeMeleeAttackTask::PickStepCount(Weights, 0.0f), 1);
	TestEqual(TEXT("Just below the first weight still picks one step"), FRpgStateTreeMeleeAttackTask::PickStepCount(Weights, 0.49f), 1);
	TestEqual(TEXT("A zero weight is never picked"), FRpgStateTreeMeleeAttackTask::PickStepCount(Weights, 0.5f), 3);
	TestEqual(TEXT("The last quarter picks four steps"), FRpgStateTreeMeleeAttackTask::PickStepCount(Weights, 0.99f), 4);
	TestEqual(TEXT("The end of the range picks the last positive weight"), FRpgStateTreeMeleeAttackTask::PickStepCount(Weights, 1.0f), 4);
	TestEqual(TEXT("Without a positive weight one step is picked"), FRpgStateTreeMeleeAttackTask::PickStepCount(TArray<float>{ 0.0f, -1.0f }, 0.7f), 1);
	TestEqual(TEXT("An empty list picks one step"), FRpgStateTreeMeleeAttackTask::PickStepCount(TArray<float>(), 0.3f), 1);
	return !HasAnyErrors();
}

#endif
