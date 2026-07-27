#include "RpgPrimaryGameLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameUIPolicy.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgGameUIPolicy.h"
#include "SurvivalRpg/UI/RpgPrimaryGameLayerContract.h"
#include "UObject/UnrealType.h"

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
	FRpgPrimaryGameLayoutConfiguredPolicyTest,
	"SurvivalRpg.UI.PrimaryGameLayout.ConfiguredPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPrimaryGameLayoutConfiguredPolicyTest::RunTest(const FString& Parameters)
{
	constexpr TCHAR AuthoredLayoutPath[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_RpgPrimaryGameLayout.CUI_RpgPrimaryGameLayout_C");
	const URpgGameUIPolicy* Policy = GetDefault<URpgGameUIPolicy>();
	if (!TestNotNull(TEXT("Rpg UI policy CDO exists"), Policy))
	{
		return false;
	}

	const FSoftClassProperty* ConfiguredClassProperty =
		FindFProperty<FSoftClassProperty>(
			URpgGameUIPolicy::StaticClass(),
			TEXT("RootLayoutClass"));
	const FSoftClassProperty* AppliedClassProperty =
		FindFProperty<FSoftClassProperty>(
			UGameUIPolicy::StaticClass(),
			TEXT("LayoutClass"));
	if (!TestNotNull(
			TEXT("Rpg policy exposes its configured root-layout property"),
			ConfiguredClassProperty) ||
		!TestNotNull(
			TEXT("CommonGame policy exposes its applied layout property"),
			AppliedClassProperty))
	{
		return false;
	}

	const FSoftObjectPtr ConfiguredClass =
		ConfiguredClassProperty->GetPropertyValue_InContainer(Policy);
	const FSoftObjectPtr AppliedClass =
		AppliedClassProperty->GetPropertyValue_InContainer(Policy);
	TestEqual(
		TEXT("Project config selects the exact authored root layout"),
		ConfiguredClass.ToSoftObjectPath().ToString(),
		FString(AuthoredLayoutPath));
	TestEqual(
		TEXT("CommonGame receives the exact authored root layout"),
		AppliedClass.ToSoftObjectPath().ToString(),
		FString(AuthoredLayoutPath));

	UClass* LayoutClass =
		LoadClass<URpgPrimaryGameLayout>(nullptr, AuthoredLayoutPath);
	TestNotNull(TEXT("Configured root-layout class loads"), LayoutClass);
	TestTrue(
		TEXT("Configured policy does not fall back to the native layout class"),
		LayoutClass != URpgPrimaryGameLayout::StaticClass());
	return true;
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

	const RpgPrimaryGameLayers::FContract& LayerContract =
		RpgPrimaryGameLayers::GetContract();
	TestTrue(
		TEXT("Layer contract exposes UI.Layer.Game"),
		LayerContract.Game ==
			RpgGameplayTags::UI_Layer_Game);
	TestTrue(
		TEXT("Layer contract exposes UI.Layer.GameMenu"),
		LayerContract.GameMenu ==
			RpgGameplayTags::UI_Layer_GameMenu);
	TestTrue(
		TEXT("Layer contract exposes UI.Layer.Menu"),
		LayerContract.Menu ==
			RpgGameplayTags::UI_Layer_Menu);
	TestTrue(
		TEXT("Layer contract exposes UI.Layer.Modal"),
		LayerContract.Modal ==
			RpgGameplayTags::UI_Layer_Modal);

	TestNotNull(TEXT("Gameplay layer is registered"), Layout->GetLayerWidget(LayerContract.Game));
	TestNotNull(TEXT("Game-menu layer is registered"), Layout->GetLayerWidget(LayerContract.GameMenu));
	TestNotNull(TEXT("Menu layer is registered"), Layout->GetLayerWidget(LayerContract.Menu));
	TestNotNull(TEXT("Modal layer is registered"), Layout->GetLayerWidget(LayerContract.Modal));
	return true;
}

#endif
