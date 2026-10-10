// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "CommonInputBaseTypes.h"
#include "Engine/PlatformSettingsManager.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWindowsDefaultInputTypeTest,
	"SurvivalRpg.UI.Input.WindowsDefaultInputType",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgWindowsDefaultInputTypeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UCommonInputPlatformSettings* WindowsSettings =
		UPlatformSettingsManager::Get()
			.GetSettingsForPlatform<UCommonInputPlatformSettings>(
				FName(TEXT("Windows")));
	if (!TestNotNull(
			TEXT("CommonInput has Windows platform settings"),
			WindowsSettings))
	{
		return false;
	}

	// PC players start on mouse and keyboard; a gamepad takes over on its first input.
	TestEqual(
		TEXT("Windows starts with mouse and keyboard glyphs, cursor and hover"),
		WindowsSettings->GetDefaultInputType(),
		ECommonInputType::MouseAndKeyboard);
	TestTrue(
		TEXT("Windows supports mouse and keyboard"),
		WindowsSettings->SupportsInputType(ECommonInputType::MouseAndKeyboard));
	TestTrue(
		TEXT("Windows supports switching to a gamepad"),
		WindowsSettings->SupportsInputType(ECommonInputType::Gamepad));
	TestTrue(
		TEXT("Windows has keyboard glyph data"),
		WindowsSettings
			->GetControllerDataForInputType(
				ECommonInputType::MouseAndKeyboard,
				NAME_None)
			.Num() > 0);
	TestTrue(
		TEXT("Windows has glyph data for its default gamepad"),
		WindowsSettings
			->GetControllerDataForInputType(
				ECommonInputType::Gamepad,
				WindowsSettings->GetDefaultGamepadName())
			.Num() > 0);
	return true;
}

#endif
