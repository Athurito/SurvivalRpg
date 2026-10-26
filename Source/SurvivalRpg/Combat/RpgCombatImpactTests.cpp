#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameFramework/Actor.h"
#include "RpgCombatImpact.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPredictedImpactRegistryTest,
	"SurvivalRpg.Combat.HitFeedback.PredictionAbsorbsOneServerCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPredictedImpactRegistryTest::RunTest(const FString& Parameters)
{
	AActor* Target = NewObject<AActor>();
	AActor* Attacker = NewObject<AActor>();
	AActor* OtherAttacker = NewObject<AActor>();

	TestFalse(TEXT("Without a prediction the server's copy plays"), RpgCombatImpact::ConsumePredictedImpact(Target, Attacker));

	RpgCombatImpact::RecordPredictedImpact(Target, Attacker);
	TestFalse(TEXT("Another attacker's copy plays"), RpgCombatImpact::ConsumePredictedImpact(Target, OtherAttacker));
	TestFalse(TEXT("A copy for another target plays"), RpgCombatImpact::ConsumePredictedImpact(Attacker, Attacker));
	TestTrue(TEXT("The prediction absorbs the server's copy of the same impact"), RpgCombatImpact::ConsumePredictedImpact(Target, Attacker));
	TestFalse(TEXT("A second copy plays"), RpgCombatImpact::ConsumePredictedImpact(Target, Attacker));

	RpgCombatImpact::RecordPredictedImpact(Target, Attacker);
	RpgCombatImpact::RecordPredictedImpact(Target, Attacker);
	TestTrue(TEXT("Two predictions absorb the first copy"), RpgCombatImpact::ConsumePredictedImpact(Target, Attacker));
	TestTrue(TEXT("Two predictions absorb the second copy"), RpgCombatImpact::ConsumePredictedImpact(Target, Attacker));
	TestFalse(TEXT("A third copy plays"), RpgCombatImpact::ConsumePredictedImpact(Target, Attacker));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgImpactOutcomeCueTest,
	"SurvivalRpg.Combat.HitFeedback.OutcomeCues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgImpactOutcomeCueTest::RunTest(const FString& Parameters)
{
	const FGameplayTag HeavyHit = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Combat.Hit.Heavy"));
	TestEqual(TEXT("A hit plays the attack's own cue"),
		RpgCombatImpact::GetOutcomeCueTag(ERpgCombatImpactOutcome::Hit, HeavyHit), HeavyHit);
	TestEqual(TEXT("A block plays the shared block cue"),
		RpgCombatImpact::GetOutcomeCueTag(ERpgCombatImpactOutcome::Block, HeavyHit),
		FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Combat.Block")));
	TestEqual(TEXT("A perfect block plays the shared perfect-block cue"),
		RpgCombatImpact::GetOutcomeCueTag(ERpgCombatImpactOutcome::PerfectBlock, HeavyHit),
		FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Combat.PerfectBlock")));
	TestTrue(TEXT("The attacker predicts hits"), RpgCombatImpact::IsPredictedByOwner(ERpgCombatImpactOutcome::Hit));
	TestTrue(TEXT("The attacker predicts blocks"), RpgCombatImpact::IsPredictedByOwner(ERpgCombatImpactOutcome::Block));
	TestFalse(TEXT("A perfect block reaches the attacker from the server"),
		RpgCombatImpact::IsPredictedByOwner(ERpgCombatImpactOutcome::PerfectBlock));
	return !HasAnyErrors();
}

#endif
