#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "RpgCombatReactionProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatReactionSelectionTest,
	"SurvivalRpg.Combat.ReactionProfile.SelectsBySideWithFrontFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatReactionSelectionTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Front = NewObject<UAnimMontage>();
	UAnimMontage* Back = NewObject<UAnimMontage>();

	FRpgDirectionalReactionMontages Both;
	Both.Front = Front;
	Both.Back = Back;
	TestTrue(TEXT("An attacker in front selects the front montage"), Both.Select(false) == Front);
	TestTrue(TEXT("An attacker behind selects the back montage"), Both.Select(true) == Back);

	FRpgDirectionalReactionMontages FrontOnly;
	FrontOnly.Front = Front;
	TestTrue(TEXT("Without a back montage an attacker behind falls back to the front montage"), FrontOnly.Select(true) == Front);

	FRpgDirectionalReactionMontages BackOnly;
	BackOnly.Back = Back;
	TestNull(TEXT("Without a front montage an attacker in front selects none, so the ability default applies"), BackOnly.Select(false));
	TestTrue(TEXT("A back-only entry still serves an attacker behind"), BackOnly.Select(true) == Back);

	TestNull(TEXT("An empty entry selects none"), FRpgDirectionalReactionMontages().Select(true));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatReactionSideTest,
	"SurvivalRpg.Combat.ReactionProfile.AttackerSideUsesPlanarFacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatReactionSideTest::RunTest(const FString& Parameters)
{
	const FVector Location(100.0, 200.0, 0.0);
	const FVector Forward(0.0, 1.0, 0.0);

	TestFalse(TEXT("An attacker ahead is in front"), URpgCombatReactionProfile::IsLocationBehind(Location, Forward, Location + FVector(0.0, 150.0, 0.0)));
	TestTrue(TEXT("An attacker back is behind"), URpgCombatReactionProfile::IsLocationBehind(Location, Forward, Location + FVector(20.0, -150.0, 0.0)));
	TestFalse(TEXT("An attacker exactly to the side counts as in front"), URpgCombatReactionProfile::IsLocationBehind(Location, Forward, Location + FVector(150.0, 0.0, 0.0)));
	TestTrue(TEXT("Height does not move a rear attacker to the front"), URpgCombatReactionProfile::IsLocationBehind(Location, Forward, Location + FVector(0.0, -10.0, 500.0)));
	TestFalse(TEXT("A pitched facing still uses its planar direction"), URpgCombatReactionProfile::IsLocationBehind(Location, FVector(0.0, 0.5, -0.8), Location + FVector(0.0, 50.0, 0.0)));
	TestFalse(TEXT("A vertical facing has no side"), URpgCombatReactionProfile::IsLocationBehind(Location, FVector::UpVector, Location - FVector(0.0, 50.0, 0.0)));
	TestFalse(TEXT("A missing event has no attacker behind"), URpgCombatReactionProfile::IsAttackerBehind(nullptr, nullptr));
	return !HasAnyErrors();
}

#endif
