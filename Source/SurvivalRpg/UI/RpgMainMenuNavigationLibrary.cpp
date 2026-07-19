#include "RpgMainMenuNavigationLibrary.h"

#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgFrontendWidgets.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgMainMenuNavigation, Log, All);

namespace
{
	ULocalPlayer* ResolveContextLocalPlayer(const UObject* WorldContextObject)
	{
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(WorldContextObject))
		{
			return UserWidget->GetOwningLocalPlayer();
		}

		if (const APlayerController* PlayerController = Cast<APlayerController>(WorldContextObject))
		{
			return PlayerController->GetLocalPlayer();
		}

		if (ULocalPlayer* LocalPlayer = const_cast<ULocalPlayer*>(Cast<ULocalPlayer>(WorldContextObject)))
		{
			return LocalPlayer;
		}

		return nullptr;
	}
}

void URpgMainMenuNavigationLibrary::MainMenu_PushToMainStack(
	const UObject* WorldContextObject,
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	if (URpgMainMenuStackWidget* MainMenu = ResolveMainMenu(WorldContextObject))
	{
		MainMenu->PushToMainStack(ActivatableWidgetClass);
	}
}

void URpgMainMenuNavigationLibrary::MainMenu_PushToOptionStack(
	const UObject* WorldContextObject,
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	if (URpgMainMenuStackWidget* MainMenu = ResolveMainMenu(WorldContextObject))
	{
		MainMenu->PushToOptionStack(ActivatableWidgetClass);
	}
}

void URpgMainMenuNavigationLibrary::MainMenu_PushToPopupStack(
	const UObject* WorldContextObject,
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	if (URpgMainMenuStackWidget* MainMenu = ResolveMainMenu(WorldContextObject))
	{
		MainMenu->PushToPopupStack(ActivatableWidgetClass);
	}
}

UCommonActivatableWidget* URpgMainMenuNavigationLibrary::MainMenu_PushToOption1Stack(
	const UObject* WorldContextObject,
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	if (URpgMainMenuStackWidget* MainMenu = ResolveMainMenu(WorldContextObject))
	{
		return MainMenu->PushToOption1Stack(ActivatableWidgetClass);
	}

	return nullptr;
}

UCommonActivatableWidget* URpgMainMenuNavigationLibrary::MainMenu_PushToOption2Stack(
	const UObject* WorldContextObject,
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	if (URpgMainMenuStackWidget* MainMenu = ResolveMainMenu(WorldContextObject))
	{
		return MainMenu->PushToOption2Stack(ActivatableWidgetClass);
	}

	return nullptr;
}

URpgMainMenuStackWidget* URpgMainMenuNavigationLibrary::ResolveMainMenu(const UObject* WorldContextObject)
{
	ULocalPlayer* LocalPlayer = ResolveContextLocalPlayer(WorldContextObject);
	URpgUIScreenSubsystem* ScreenSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<URpgUIScreenSubsystem>()
		: nullptr;
	URpgMainMenuStackWidget* MainMenu = ScreenSubsystem
		? Cast<URpgMainMenuStackWidget>(ScreenSubsystem->GetActiveScreen(RpgGameplayTags::UI_Screen_MainMenu))
		: nullptr;

	if (!MainMenu)
	{
		UE_LOG(LogRpgMainMenuNavigation, Warning,
			TEXT("Main Menu navigation ignored: UI.Screen.MainMenu is not active for local player [%s]."),
			*GetNameSafe(LocalPlayer));
	}

	return MainMenu;
}
