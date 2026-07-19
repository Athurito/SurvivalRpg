#include "RpgPrimaryGameLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgPrimaryGameLayoutTests
{
	class FScopedLayoutWorld
	{
	public:
		FScopedLayoutWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedLayoutWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPrimaryGameLayoutAuthoredLayersTest,
	"SurvivalRpg.UI.PrimaryGameLayout.AuthoredLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPrimaryGameLayoutAuthoredLayersTest::RunTest(const FString& Parameters)
{
	using namespace RpgPrimaryGameLayoutTests;

	FScopedLayoutWorld TestWorld;
	if (!TestNotNull(TEXT("Standalone layout world exists"), TestWorld.GetWorld()))
	{
		return false;
	}

	UClass* LayoutClass = LoadClass<URpgPrimaryGameLayout>(
		nullptr,
		TEXT("/Game/SurvivalRpg/UI/CUI_RpgPrimaryGameLayout.CUI_RpgPrimaryGameLayout_C"));
	if (!TestNotNull(TEXT("Authored PrimaryGameLayout class loads"), LayoutClass))
	{
		return false;
	}

	URpgPrimaryGameLayout* Layout =
		CreateWidget<URpgPrimaryGameLayout>(TestWorld.GetWorld(), LayoutClass);
	if (!TestNotNull(TEXT("Authored PrimaryGameLayout instance initializes"), Layout))
	{
		return false;
	}

	TestNotNull(TEXT("RootOverlay is authored in the Widget Blueprint"), Layout->GetWidgetFromName(TEXT("RootOverlay")));
	TestNotNull(TEXT("GameLayer is authored in the Widget Blueprint"), Layout->GetWidgetFromName(TEXT("GameLayer")));
	TestNotNull(TEXT("GameMenuLayer is authored in the Widget Blueprint"), Layout->GetWidgetFromName(TEXT("GameMenuLayer")));
	TestNotNull(TEXT("MenuLayer is authored in the Widget Blueprint"), Layout->GetWidgetFromName(TEXT("MenuLayer")));
	TestNotNull(TEXT("ModalLayer is authored in the Widget Blueprint"), Layout->GetWidgetFromName(TEXT("ModalLayer")));

	TestNotNull(TEXT("Gameplay layer is registered"), Layout->GetLayerWidget(RpgGameplayTags::UI_Layer_Game));
	TestNotNull(TEXT("Game-menu layer is registered"), Layout->GetLayerWidget(RpgGameplayTags::UI_Layer_GameMenu));
	TestNotNull(TEXT("Menu layer is registered"), Layout->GetLayerWidget(RpgGameplayTags::UI_Layer_Menu));
	TestNotNull(TEXT("Modal layer is registered"), Layout->GetLayerWidget(RpgGameplayTags::UI_Layer_Modal));
	return true;
}

#endif
