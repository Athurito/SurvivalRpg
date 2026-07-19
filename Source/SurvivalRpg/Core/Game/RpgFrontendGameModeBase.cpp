#include "RpgFrontendGameModeBase.h"

#include "CommonPlayerController.h"
#include "SurvivalRpg/UI/RpgFrontendHUD.h"

ARpgFrontendGameModeBase::ARpgFrontendGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerControllerClass = ACommonPlayerController::StaticClass();
	HUDClass = ARpgFrontendHUD::StaticClass();
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = true;
}
