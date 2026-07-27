#include "RpgFrontendHUD.h"

#include "CommonLocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "PrimaryGameLayout.h"
#include "SurvivalRpg/Core/Game/RpgFrontendGameModeBase.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgFrontendHUD, Log, All);

namespace
{
	constexpr uint8 MaxRootLayoutRetries = 4;

	const ARpgFrontendGameModeBase* ResolveFrontendGameMode(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		if (const ARpgFrontendGameModeBase* AuthGameMode = World->GetAuthGameMode<ARpgFrontendGameModeBase>())
		{
			return AuthGameMode;
		}

		const AGameStateBase* GameState = World->GetGameState();
		return GameState ? GameState->GetDefaultGameMode<ARpgFrontendGameModeBase>() : nullptr;
	}
}

void ARpgFrontendHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!OwningPlayerController || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	BoundLocalPlayer = Cast<UCommonLocalPlayer>(OwningPlayerController->GetLocalPlayer());
	if (!BoundLocalPlayer)
	{
		UE_LOG(LogRpgFrontendHUD, Error, TEXT("Frontend HUD [%s] requires UCommonLocalPlayer."), *GetNameSafe(this));
		return;
	}

	PlayerControllerSetHandle = BoundLocalPlayer->CallAndRegister_OnPlayerControllerSet(
		UCommonLocalPlayer::FPlayerControllerSetDelegate::FDelegate::CreateUObject(
			this,
			&ThisClass::HandlePlayerControllerSet));
}

void ARpgFrontendHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OpenRetryTimerHandle);

	if (BoundLocalPlayer)
	{
		if (PlayerControllerSetHandle.IsValid())
		{
			BoundLocalPlayer->OnPlayerControllerSet.Remove(PlayerControllerSetHandle);
			PlayerControllerSetHandle.Reset();
		}

		if (OpenedScreenTag.IsValid())
		{
			if (URpgUIScreenSubsystem* ScreenSubsystem = BoundLocalPlayer->GetSubsystem<URpgUIScreenSubsystem>())
			{
				ScreenSubsystem->CloseScreen(OpenedScreenTag);
			}
		}
	}

	OpenedScreenTag = FGameplayTag();
	BoundLocalPlayer = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ARpgFrontendHUD::HandlePlayerControllerSet(UCommonLocalPlayer* LocalPlayer, APlayerController* PlayerController)
{
	if (LocalPlayer == BoundLocalPlayer && PlayerController == GetOwningPlayerController())
	{
		TryOpenInitialScreen();
	}
}

void ARpgFrontendHUD::TryOpenInitialScreen()
{
	if (OpenedScreenTag.IsValid() || !BoundLocalPlayer)
	{
		return;
	}

	const ARpgFrontendGameModeBase* FrontendGameMode = ResolveFrontendGameMode(GetWorld());
	const FGameplayTag InitialScreenTag = FrontendGameMode ? FrontendGameMode->GetInitialScreenTag() : FGameplayTag();
	if (!InitialScreenTag.IsValid())
	{
		UE_LOG(LogRpgFrontendHUD, Error, TEXT("Frontend map [%s] has no valid InitialScreenTag."),
			*GetNameSafe(GetWorld()));
		return;
	}

	if (!UPrimaryGameLayout::GetPrimaryGameLayout(BoundLocalPlayer))
	{
		if (RootLayoutRetryCount++ < MaxRootLayoutRetries)
		{
			OpenRetryTimerHandle =
				GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TryOpenInitialScreen);
		}
		else
		{
			UE_LOG(LogRpgFrontendHUD, Error,
				TEXT("Cannot open frontend screen [%s]: CommonGame did not provide a PrimaryGameLayout for [%s]."),
				*InitialScreenTag.ToString(),
				*GetNameSafe(BoundLocalPlayer));
		}
		return;
	}

	URpgUIScreenSubsystem* ScreenSubsystem = BoundLocalPlayer->GetSubsystem<URpgUIScreenSubsystem>();
	if (!ScreenSubsystem)
	{
		UE_LOG(LogRpgFrontendHUD, Error, TEXT("Cannot open frontend screen [%s]: local screen subsystem is missing."),
			*InitialScreenTag.ToString());
		return;
	}

	OpenedScreenTag = InitialScreenTag;
	ScreenSubsystem->OpenScreen(OpenedScreenTag);
}
