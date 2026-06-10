#include "RpgUIScreenBlueprintLibrary.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "RpgUIScreenSubsystem.h"

URpgUIScreenSubsystem* URpgUIScreenBlueprintLibrary::GetRpgUIScreenSubsystem(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return nullptr;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		return LocalPlayer->GetSubsystem<URpgUIScreenSubsystem>();
	}

	return nullptr;
}

UCommonActivatableWidget* URpgUIScreenBlueprintLibrary::OpenUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag, UObject* Payload)
{
	if (URpgUIScreenSubsystem* ScreenSubsystem = GetRpgUIScreenSubsystem(PlayerController))
	{
		return ScreenSubsystem->OpenScreen(ScreenTag, Payload);
	}

	return nullptr;
}

UCommonActivatableWidget* URpgUIScreenBlueprintLibrary::ToggleUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag, UObject* Payload)
{
	if (URpgUIScreenSubsystem* ScreenSubsystem = GetRpgUIScreenSubsystem(PlayerController))
	{
		return ScreenSubsystem->ToggleScreen(ScreenTag, Payload);
	}

	return nullptr;
}

void URpgUIScreenBlueprintLibrary::CloseUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag)
{
	if (URpgUIScreenSubsystem* ScreenSubsystem = GetRpgUIScreenSubsystem(PlayerController))
	{
		ScreenSubsystem->CloseScreen(ScreenTag);
	}
}

UCommonActivatableWidget* URpgUIScreenBlueprintLibrary::GetActiveUIScreen(APlayerController* PlayerController, FGameplayTag ScreenTag)
{
	if (URpgUIScreenSubsystem* ScreenSubsystem = GetRpgUIScreenSubsystem(PlayerController))
	{
		return ScreenSubsystem->GetActiveScreen(ScreenTag);
	}

	return nullptr;
}
