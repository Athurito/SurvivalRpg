#include "RpgPrimaryGameLayout.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgPrimaryGameLayout, Log, All);

void URpgPrimaryGameLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (GameLayer)
	{
		RegisterLayer(RpgGameplayTags::UI_Layer_Game, GameLayer);
	}

	if (GameMenuLayer)
	{
		RegisterLayer(RpgGameplayTags::UI_Layer_GameMenu, GameMenuLayer);
	}

	if (MenuLayer)
	{
		RegisterLayer(RpgGameplayTags::UI_Layer_Menu, MenuLayer);
	}

	if (ModalLayer)
	{
		RegisterLayer(RpgGameplayTags::UI_Layer_Modal, ModalLayer);
	}

	UE_LOG(LogRpgPrimaryGameLayout, Log, TEXT("Initialized layers for [%s]: Game=%s GameMenu=%s Menu=%s Modal=%s."),
		*GetNameSafe(this),
		*GetNameSafe(GameLayer),
		*GetNameSafe(GameMenuLayer),
		*GetNameSafe(MenuLayer),
		*GetNameSafe(ModalLayer));
}
