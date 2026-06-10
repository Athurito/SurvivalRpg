#include "RpgPrimaryGameLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void URpgPrimaryGameLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	CreateNativeFallbackLayers();

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
}

void URpgPrimaryGameLayout::CreateNativeFallbackLayers()
{
	if (!WidgetTree || WidgetTree->RootWidget || GameLayer || GameMenuLayer || MenuLayer || ModalLayer)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

	auto AddLayerStack = [this, RootOverlay](FName LayerName)
	{
		UCommonActivatableWidgetStack* Stack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), LayerName);
		UOverlaySlot* StackSlot = RootOverlay->AddChildToOverlay(Stack);
		StackSlot->SetHorizontalAlignment(HAlign_Fill);
		StackSlot->SetVerticalAlignment(VAlign_Fill);
		return Stack;
	};

	GameLayer = AddLayerStack(TEXT("GameLayer"));
	GameMenuLayer = AddLayerStack(TEXT("GameMenuLayer"));
	MenuLayer = AddLayerStack(TEXT("MenuLayer"));
	ModalLayer = AddLayerStack(TEXT("ModalLayer"));
}
