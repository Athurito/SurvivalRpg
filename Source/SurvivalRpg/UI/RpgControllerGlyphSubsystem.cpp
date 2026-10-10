#include "RpgControllerGlyphSubsystem.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgControllerGlyphSubsystem)

namespace RpgControllerGlyphs
{
	/** Gamepad controller data registered for the current platform, one per gamepad name, in config order. */
	static TArray<const UCommonInputBaseControllerData*> GetGamepadControllerData()
	{
		TArray<const UCommonInputBaseControllerData*> Result;
		const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
		if (!PlatformSettings)
		{
			return Result;
		}

		TSet<FName> SeenNames;
		for (const TSoftClassPtr<UCommonInputBaseControllerData>& DataClass : PlatformSettings->GetControllerData())
		{
			const TSubclassOf<UCommonInputBaseControllerData> LoadedClass = DataClass.LoadSynchronous();
			const UCommonInputBaseControllerData* Data = LoadedClass ? LoadedClass.GetDefaultObject() : nullptr;
			if (!Data || Data->InputType != ECommonInputType::Gamepad || Data->GamepadName.IsNone())
			{
				continue;
			}

			bool bAlreadySeen = false;
			SeenNames.Add(Data->GamepadName, &bAlreadySeen);
			if (!bAlreadySeen)
			{
				Result.Add(Data);
			}
		}
		return Result;
	}

	static bool CanChangeGamepadType()
	{
		const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
		return PlatformSettings && PlatformSettings->CanChangeGamepadType();
	}
}

void URpgControllerGlyphSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UCommonInputSubsystem>();
	Super::Initialize(Collection);

	// Apply the saved choice before any screen shows a prompt. An unknown set (for example from another platform's
	// config) keeps the platform default.
	if (SavedGamepadGlyphSet.IsNone() || !IsGamepadGlyphSetAvailable(SavedGamepadGlyphSet) ||
		!RpgControllerGlyphs::CanChangeGamepadType())
	{
		return;
	}

	UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetLocalPlayer());
	if (InputSubsystem && InputSubsystem->GetCurrentGamepadName() != SavedGamepadGlyphSet)
	{
		InputSubsystem->SetGamepadInputType(SavedGamepadGlyphSet);
	}
}

TArray<FRpgGamepadGlyphOption> URpgControllerGlyphSubsystem::GetGamepadGlyphOptions()
{
	TArray<FRpgGamepadGlyphOption> Options;
	for (const UCommonInputBaseControllerData* Data : RpgControllerGlyphs::GetGamepadControllerData())
	{
		FRpgGamepadGlyphOption& Option = Options.AddDefaulted_GetRef();
		Option.GamepadName = Data->GamepadName;
		Option.DisplayName =
			Data->GamepadDisplayName.IsEmpty() ? FText::FromName(Data->GamepadName) : Data->GamepadDisplayName;
	}
	return Options;
}

bool URpgControllerGlyphSubsystem::IsGamepadGlyphSetAvailable(FName GamepadName)
{
	if (GamepadName.IsNone())
	{
		return false;
	}

	for (const UCommonInputBaseControllerData* Data : RpgControllerGlyphs::GetGamepadControllerData())
	{
		if (Data->GamepadName == GamepadName)
		{
			return true;
		}
	}
	return false;
}

FName URpgControllerGlyphSubsystem::GetGamepadGlyphSet() const
{
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetLocalPlayer());
	return InputSubsystem ? InputSubsystem->GetCurrentGamepadName() : NAME_None;
}

bool URpgControllerGlyphSubsystem::SetGamepadGlyphSet(FName GamepadName)
{
	if (!IsGamepadGlyphSetAvailable(GamepadName) || !RpgControllerGlyphs::CanChangeGamepadType())
	{
		return false;
	}

	UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetLocalPlayer());
	if (!InputSubsystem)
	{
		return false;
	}

	// SetGamepadInputType broadcasts OnInputMethodChanged, so every CommonUI glyph refreshes.
	if (InputSubsystem->GetCurrentGamepadName() != GamepadName)
	{
		InputSubsystem->SetGamepadInputType(GamepadName);
	}

	if (SavedGamepadGlyphSet != GamepadName)
	{
		SavedGamepadGlyphSet = GamepadName;
		SaveConfig();
	}
	return true;
}

bool URpgControllerGlyphSubsystem::GetGamepadKeyBrush(const FKey& Key, FSlateBrush& OutBrush) const
{
	const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
	return PlatformSettings && Key.IsValid() &&
		PlatformSettings->TryGetInputBrush(OutBrush, Key, ECommonInputType::Gamepad, GetGamepadGlyphSet());
}

UTexture2D* URpgControllerGlyphSubsystem::GetGamepadControllerTexture() const
{
	const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
	if (!PlatformSettings)
	{
		return nullptr;
	}

	for (const UCommonInputBaseControllerData* Data :
		 PlatformSettings->GetControllerDataForInputType(ECommonInputType::Gamepad, GetGamepadGlyphSet()))
	{
		if (UTexture2D* Texture = Data->ControllerTexture.LoadSynchronous())
		{
			return Texture;
		}
	}
	return nullptr;
}
