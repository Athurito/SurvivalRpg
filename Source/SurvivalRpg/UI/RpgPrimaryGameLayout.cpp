#include "RpgPrimaryGameLayout.h"

#include "SurvivalRpg/UI/RpgPrimaryGameLayerContract.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgPrimaryGameLayout, Log, All);

void URpgPrimaryGameLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	const RpgPrimaryGameLayers::FContract& LayerContract =
		RpgPrimaryGameLayers::GetContract();

	if (GameLayer)
	{
		RegisterLayer(LayerContract.Game, GameLayer);
	}

	if (GameMenuLayer)
	{
		RegisterLayer(LayerContract.GameMenu, GameMenuLayer);
	}

	if (MenuLayer)
	{
		RegisterLayer(LayerContract.Menu, MenuLayer);
	}

	if (ModalLayer)
	{
		RegisterLayer(LayerContract.Modal, ModalLayer);
	}

	UE_LOG(LogRpgPrimaryGameLayout, Log, TEXT("Initialized layers for [%s]: Game=%s GameMenu=%s Menu=%s Modal=%s."),
		*GetNameSafe(this),
		*GetNameSafe(GameLayer),
		*GetNameSafe(GameMenuLayer),
		*GetNameSafe(MenuLayer),
		*GetNameSafe(ModalLayer));
}
