// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Animation/RpgAnimationPlaybackRuntime.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAnimationPlaybackTimeTest,
	"SurvivalRpg.Animation.Playback.DirectionalCompletionTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAnimationPlaybackTimeTest::RunTest(const FString& Parameters)
{
	using RpgAnimationPlaybackRuntime::RemainingPlaybackSeconds;
	TestEqual(TEXT("Half-speed playback retains twice the remaining asset time"),
		RemainingPlaybackSeconds(1.5f, 2.0f, 0.5f), 1.0f);
	TestEqual(TEXT("Double-speed playback reaches the end in half the remaining asset time"),
		RemainingPlaybackSeconds(1.5f, 2.0f, 2.0f), 0.25f);
	TestEqual(TEXT("Reverse playback measures distance to the beginning"),
		RemainingPlaybackSeconds(0.25f, 2.0f, -0.5f), 0.5f);
	TestEqual(TEXT("Forward end is complete"), RemainingPlaybackSeconds(2.0f, 2.0f, 1.0f), 0.0f);
	TestEqual(TEXT("Reverse beginning is complete"), RemainingPlaybackSeconds(0.0f, 2.0f, -1.0f), 0.0f);
	TestEqual(TEXT("A paused clip cannot complete from the frame lead"),
		RemainingPlaybackSeconds(1.99f, 2.0f, 0.0f), MAX_flt);
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Non-finite play rate is left to the watchdog"),
		RemainingPlaybackSeconds(1.99f, 2.0f, NaN), MAX_flt);
	TestEqual(TEXT("Invalid asset time is left to the watchdog"),
		RemainingPlaybackSeconds(NaN, 2.0f, 1.0f), MAX_flt);
	for (const float Fps : {15.0f, 30.0f, 60.0f, 120.0f})
	{
		const float CompletionLead = 0.05f + 1.0f / Fps;
		TestFalse(FString::Printf(TEXT("Slow playback does not complete an animation-second early at %.0f FPS"), Fps),
			RemainingPlaybackSeconds(1.94f, 2.0f, 0.5f) <= CompletionLead);
		TestTrue(FString::Printf(TEXT("Fast playback respects the same playback-second lead at %.0f FPS"), Fps),
			RemainingPlaybackSeconds(1.94f, 2.0f, 2.0f) <= CompletionLead);
	}
	return true;
}

#endif
