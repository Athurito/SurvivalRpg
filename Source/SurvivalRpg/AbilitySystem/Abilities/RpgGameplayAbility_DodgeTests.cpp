#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RpgGameplayAbility_Dodge.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgDodgeRootMotionProfileSelectionTest,
	"SurvivalRpg.Combat.Dodge.SelectsEquipmentRootMotionProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgDodgeRootMotionProfileSelectionTest::RunTest(const FString& Parameters)
{
	FRpgDodgeRootMotionTuning Light;
	Light.ProfileName = TEXT("Dodge.Light");
	Light.MontagePlayRate = 1.2f;
	Light.TranslationScale = 1.15f;
	Light.StartSection = TEXT("Forward");

	FRpgDodgeRootMotionTuning Heavy;
	Heavy.ProfileName = TEXT("Dodge.Heavy");
	Heavy.MontagePlayRate = 0.8f;
	Heavy.TranslationScale = 0.7f;

	const TArray<FRpgDodgeRootMotionTuning> Tunings{Light, Heavy};
	const FRpgDodgeRootMotionTuning ResolvedHeavy = URpgGameplayAbility_Dodge::ResolveRootMotionTuning(
		TEXT("Dodge.Heavy"),
		Tunings,
		1.0f,
		1.0f);
	TestEqual(TEXT("Heavy selects its named root-motion profile"), ResolvedHeavy.ProfileName, FName(TEXT("Dodge.Heavy")));
	TestEqual(TEXT("Heavy playback rate is selected without changing movement speed"), ResolvedHeavy.MontagePlayRate, 0.8f);
	TestEqual(TEXT("Heavy translation scale is selected"), ResolvedHeavy.TranslationScale, 0.7f);

	const FRpgDodgeRootMotionTuning Missing = URpgGameplayAbility_Dodge::ResolveRootMotionTuning(
		TEXT("Dodge.Unconfigured"),
		Tunings,
		0.95f,
		0.9f);
	TestEqual(TEXT("Unknown names retain the semantic profile for diagnostics"), Missing.ProfileName, FName(TEXT("Dodge.Unconfigured")));
	TestEqual(TEXT("Unknown names use the ability playback fallback"), Missing.MontagePlayRate, 0.95f);
	TestEqual(TEXT("Unknown names use the ability root-motion fallback"), Missing.TranslationScale, 0.9f);
	return true;
}

#endif
