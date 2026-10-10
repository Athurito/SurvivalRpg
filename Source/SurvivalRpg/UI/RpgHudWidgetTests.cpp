#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SurvivalRpg/UI/RpgHudFadeBox.h"
#include "SurvivalRpg/UI/RpgTrailingProgressBar.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHudFadeStateTest,
	"SurvivalRpg.UI.Hud.FadeHoldsThenFades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHudFadeStateTest::RunTest(const FString& Parameters)
{
	FRpgHudFadeSettings Settings;
	Settings.HiddenOpacity = 0.4f;
	Settings.HoldSeconds = 2.0f;
	Settings.FadeInSeconds = 0.5f;
	Settings.FadeOutSeconds = 1.0f;

	FRpgHudFadeState State;
	TestTrue(TEXT("A new box holds its content first"), State.Advance(10.0, 0.0f, Settings));
	TestEqual(TEXT("A new box starts fully visible"), State.Opacity, 1.0f);
	TestTrue(TEXT("The hold keeps the box advancing"), State.Advance(11.9, 0.1f, Settings));
	TestEqual(TEXT("The content stays visible during the hold"), State.Opacity, 1.0f);

	TestTrue(TEXT("The fade-out keeps the box advancing"), State.Advance(12.5, 0.5f, Settings));
	TestEqual(TEXT("Half the fade-out time drops half the range"), State.Opacity, 0.5f);
	TestFalse(TEXT("The box settles at its hidden opacity"), State.Advance(13.0, 0.5f, Settings));
	TestEqual(TEXT("The hidden opacity is a floor"), State.Opacity, 0.4f);

	State.SetPinned(true);
	State.Advance(14.0, 0.25f, Settings);
	TestEqual(TEXT("Pinning fades in at the fade-in rate"), State.Opacity, 0.9f);
	TestFalse(TEXT("A pinned box at full opacity settles"), State.Advance(14.25, 0.25f, Settings));
	TestFalse(TEXT("A pinned box never fades"), State.Advance(30.0, 5.0f, Settings) || State.Opacity < 1.0f);

	State.SetPinned(false);
	TestTrue(TEXT("Unpinning starts a hold"), State.Advance(40.0, 0.1f, Settings));
	TestEqual(TEXT("The hold after unpinning keeps full opacity"), State.Opacity, 1.0f);
	State.Advance(42.0, 1.0f, Settings);
	TestEqual(TEXT("After the hold the box fades out"), State.Opacity, 0.4f);

	State.Pulse();
	State.Advance(50.0, 0.5f, Settings);
	TestEqual(TEXT("A pulse shows the content again"), State.Opacity, 1.0f);
	TestTrue(TEXT("A pulse holds like an unpin"), State.Advance(51.5, 0.1f, Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProgressTrailStateTest,
	"SurvivalRpg.UI.Hud.TrailDrainsAfterDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProgressTrailStateTest::RunTest(const FString& Parameters)
{
	FRpgProgressTrailState Trail;
	Trail.SetTarget(0.8f);
	TestEqual(TEXT("A rise shows at once"), Trail.Displayed, 0.8f);
	TestFalse(TEXT("A settled trail stops advancing"), Trail.Advance(1.0, 0.1f, 0.5f, 1.0f));

	Trail.SetTarget(0.5f);
	TestEqual(TEXT("A drop keeps the old value first"), Trail.Displayed, 0.8f);
	TestTrue(TEXT("A drop keeps the trail advancing"), Trail.Advance(2.0, 0.1f, 0.5f, 1.0f));
	TestEqual(TEXT("The trail lingers during the delay"), Trail.Displayed, 0.8f);
	Trail.Advance(2.6, 0.1f, 0.5f, 1.0f);
	TestEqual(TEXT("After the delay the trail drains at its speed"), Trail.Displayed, 0.7f, 0.001f);

	Trail.SetTarget(0.4f);
	Trail.Advance(2.7, 0.1f, 0.5f, 1.0f);
	TestEqual(TEXT("A further drop restarts the delay"), Trail.Displayed, 0.7f, 0.001f);
	TestFalse(TEXT("The drain stops at the target"), Trail.Advance(3.5, 2.0f, 0.5f, 1.0f));
	TestEqual(TEXT("The trail ends at the target"), Trail.Displayed, 0.4f);

	Trail.SetTarget(0.9f);
	TestEqual(TEXT("A rise during a trail jumps to the new value"), Trail.Displayed, 0.9f);
	return true;
}

#endif
