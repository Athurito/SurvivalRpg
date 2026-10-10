// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "CommonInputBaseTypes.h"
#include "Engine/PlatformSettingsManager.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "RpgControllerGlyphSubsystem.h"
#include "Styling/SlateBrush.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGamepadGlyphOptionsTest,
	"SurvivalRpg.UI.Input.GamepadGlyphOptions",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGamepadGlyphOptionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UCommonInputPlatformSettings* PlatformSettings =
		UCommonInputPlatformSettings::Get();
	if (!TestNotNull(
			TEXT("CommonInput has settings for this platform"),
			PlatformSettings))
	{
		return false;
	}

	const TArray<FRpgGamepadGlyphOption> Options =
		URpgControllerGlyphSubsystem::GetGamepadGlyphOptions();

	// Windows cannot tell controller brands apart, so the player needs a choice between glyph sets.
	TestTrue(
		TEXT("The settings menu offers more than one gamepad glyph set"),
		Options.Num() > 1);

	const FName DefaultGamepad = PlatformSettings->GetDefaultGamepadName();
	TestTrue(
		TEXT("The platform's default gamepad is one of the options"),
		Options.ContainsByPredicate(
			[DefaultGamepad](const FRpgGamepadGlyphOption& Option)
			{
				return Option.GamepadName == DefaultGamepad;
			}));

	TSet<FName> SeenNames;
	for (const FRpgGamepadGlyphOption& Option : Options)
	{
		bool bAlreadySeen = false;
		SeenNames.Add(Option.GamepadName, &bAlreadySeen);
		TestFalse(
			FString::Printf(
				TEXT("Glyph set %s is listed once"),
				*Option.GamepadName.ToString()),
			bAlreadySeen);
		TestFalse(
			FString::Printf(
				TEXT("Glyph set %s has a player-facing name"),
				*Option.GamepadName.ToString()),
			Option.DisplayName.IsEmpty());
		TestTrue(
			FString::Printf(
				TEXT("Glyph set %s is accepted by the subsystem"),
				*Option.GamepadName.ToString()),
			URpgControllerGlyphSubsystem::IsGamepadGlyphSetAvailable(
				Option.GamepadName));

		FSlateBrush Brush;
		TestTrue(
			FString::Printf(
				TEXT("Glyph set %s draws the bottom face button"),
				*Option.GamepadName.ToString()),
			PlatformSettings->TryGetInputBrush(
				Brush,
				EKeys::Gamepad_FaceButton_Bottom,
				ECommonInputType::Gamepad,
				Option.GamepadName));
	}

	TestFalse(
		TEXT("An unregistered glyph set is rejected"),
		URpgControllerGlyphSubsystem::IsGamepadGlyphSetAvailable(
			FName(TEXT("RpgTestUnknownGamepad"))));
	TestFalse(
		TEXT("No glyph set name is rejected"),
		URpgControllerGlyphSubsystem::IsGamepadGlyphSetAvailable(
			NAME_None));
	return true;
}

#endif
