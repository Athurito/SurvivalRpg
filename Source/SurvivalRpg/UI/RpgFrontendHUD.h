#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgHUD.h"

#include "RpgFrontendHUD.generated.h"

class APlayerController;
class UCommonLocalPlayer;

/**
 * Map-owned frontend screen coordinator.
 *
 * The HUD never creates a widget or owns a viewport root. It opens the screen
 * tag configured by ARpgFrontendGameModeBase through the local screen router
 * and closes active or pending work when the map ends.
 */
UCLASS()
class SURVIVALRPG_API ARpgFrontendHUD : public ARpgHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandlePlayerControllerSet(UCommonLocalPlayer* LocalPlayer, APlayerController* PlayerController);
	void TryOpenInitialScreen();

	UPROPERTY(Transient)
	TObjectPtr<UCommonLocalPlayer> BoundLocalPlayer;

	FDelegateHandle PlayerControllerSetHandle;
	FTimerHandle OpenRetryTimerHandle;
	FGameplayTag OpenedScreenTag;
	uint8 RootLayoutRetryCount = 0;
};
