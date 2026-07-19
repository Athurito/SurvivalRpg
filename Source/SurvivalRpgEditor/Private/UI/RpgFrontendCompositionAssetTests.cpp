#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Game/RpgFrontendGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/UI/RpgFrontendHUD.h"
#include "SurvivalRpg/UI/RpgFrontendWidgets.h"
#include "SurvivalRpg/UI/RpgMainMenuNavigationLibrary.h"
#include "SurvivalRpg/UI/RpgUIScreenRegistry.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonPlayerController.h"
#include "Components/WidgetSwitcher.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Input/UIActionBindingHandle.h"
#include "K2Node_CallFunction.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/CoreRedirects.h"
#include "WidgetBlueprint.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace RpgFrontendCompositionAssetTests
{
	constexpr TCHAR RegistryPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/"
			"DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry");
	constexpr TCHAR BootGameModeClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Core/Blueprints/"
			"BP_BootMenu_Gamemode.BP_BootMenu_Gamemode_C");
	constexpr TCHAR MainGameModeClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/MainMenu/"
			"BP_MainMenuGameMode.BP_MainMenuGameMode_C");
	constexpr TCHAR BootScreenClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/BootMenu/"
			"CUI_BootMenu.CUI_BootMenu_C");
	constexpr TCHAR BootScreenBlueprintPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/BootMenu/"
			"CUI_BootMenu.CUI_BootMenu");
	constexpr TCHAR MainStackBlueprintPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/MainMenu/"
			"CUI_MainMenuStack.CUI_MainMenuStack");
	constexpr TCHAR MainStackClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/MainMenu/"
			"CUI_MainMenuStack.CUI_MainMenuStack_C");
	constexpr TCHAR MainScreenClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/MainMenu/"
			"CUI_MainMenu.CUI_MainMenu_C");
	constexpr TCHAR NavigationLibraryBlueprintPath[] =
		TEXT(
			"/Game/SurvivalRpg/Utils/Libraries/"
			"BPFL_GUI_Library.BPFL_GUI_Library");
	constexpr TCHAR BootMapPath[] =
		TEXT(
			"/Game/SurvivalRpg/Maps/Menu/"
			"BootMenu.BootMenu");
	constexpr TCHAR MainMapPath[] =
		TEXT(
			"/Game/SurvivalRpg/Maps/Menu/"
			"MainMenu.MainMenu");

	constexpr const TCHAR* NavigationFunctionNames[] = {
		TEXT("MainMenu_PushToMainStack"),
		TEXT("MainMenu_PushToOptionStack"),
		TEXT("MainMenu_PushToPopupStack"),
		TEXT("MainMenu_PushToOption1Stack"),
		TEXT("MainMenu_PushToOption2Stack")
	};

	constexpr const TCHAR* NavigationConsumerPaths[] = {
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/MainMenu/"
			"CUI_MainMenu.CUI_MainMenu"),
		TEXT(
			"/Game/SurvivalRpg/UI/PlayMenu/"
			"CUI_PlayMenu.CUI_PlayMenu"),
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/LoadingMenu/"
			"CUI_LoadingMenu.CUI_LoadingMenu"),
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/LoadingMenu/"
			"Multiplayer/CUI_MultiplayerLoading."
			"CUI_MultiplayerLoading"),
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/LoadingMenu/"
			"Singleplayer/CUI_SinglePlayerLoading."
			"CUI_SinglePlayerLoading")
	};

	constexpr const TCHAR* LegacyPackageNames[] = {
		TEXT(
			"/Game/SurvivalRpg/Core/Blueprints/"
			"BP_BootMenuHud"),
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/MainMenu/"
			"BP_MainMenuHud"),
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/MainMenu/"
			"BP_MainMenuController")
	};

	bool IsNavigationFunction(const FName FunctionName)
	{
		for (const TCHAR* Candidate : NavigationFunctionNames)
		{
			if (FunctionName == Candidate)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFrontendCommonUICompositionAssetTest,
	"SurvivalRpg.UI.Frontend.CommonUIComposition",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFrontendCommonUICompositionAssetTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgFrontendCompositionAssetTests;

	const URpgUIScreenRegistry* Registry =
		LoadObject<URpgUIScreenRegistry>(nullptr, RegistryPath);
	UClass* BootGameModeClass =
		LoadClass<ARpgFrontendGameModeBase>(
			nullptr,
			BootGameModeClassPath);
	UClass* MainGameModeClass =
		LoadClass<ARpgFrontendGameModeBase>(
			nullptr,
			MainGameModeClassPath);
	UClass* BootScreenClass =
		LoadClass<URpgBootScreenWidget>(
			nullptr,
			BootScreenClassPath);
	UClass* MainStackClass =
		LoadClass<URpgMainMenuStackWidget>(
			nullptr,
			MainStackClassPath);
	UClass* MainScreenClass =
		LoadClass<UCommonActivatableWidget>(
			nullptr,
			MainScreenClassPath);
	const UWidgetBlueprint* MainStackBlueprint =
		LoadObject<UWidgetBlueprint>(
			nullptr,
			MainStackBlueprintPath);
	const UWidgetBlueprint* BootScreenBlueprint =
		LoadObject<UWidgetBlueprint>(
			nullptr,
			BootScreenBlueprintPath);
	const UBlueprint* NavigationLibraryBlueprint =
		LoadObject<UBlueprint>(
			nullptr,
			NavigationLibraryBlueprintPath);
	const FGameplayTag BootTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.Screen.Boot"));
	const FGameplayTag MainMenuTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.Screen.MainMenu"));
	const FGameplayTag MenuLayerTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.Layer.Menu"));

	if (!TestNotNull(TEXT("Screen registry loads"), Registry) ||
		!TestNotNull(
			TEXT("Boot frontend GameMode loads"),
			BootGameModeClass) ||
		!TestNotNull(
			TEXT("Main Menu frontend GameMode loads"),
			MainGameModeClass) ||
		!TestNotNull(
			TEXT("Boot activatable screen loads"),
			BootScreenClass) ||
		!TestNotNull(
			TEXT("Main Menu stack screen loads"),
			MainStackClass) ||
		!TestNotNull(
			TEXT("Initial Main Menu page loads"),
			MainScreenClass) ||
		!TestNotNull(
			TEXT("Main Menu stack Blueprint loads"),
			MainStackBlueprint) ||
		!TestNotNull(
			TEXT("Boot screen Blueprint loads"),
			BootScreenBlueprint) ||
		!TestNotNull(
			TEXT("Navigation compatibility Blueprint loads"),
			NavigationLibraryBlueprint))
	{
		return false;
	}

	TestTrue(
		TEXT(
			"Gameplay controller inherits the CommonGame "
			"controller lifecycle"),
		ARpgPlayerController::StaticClass()->IsChildOf(
			ACommonPlayerController::StaticClass()));

	const FCoreRedirectObjectName RedirectedSettingsPackage =
		FCoreRedirects::GetRedirectedName(
			ECoreRedirectFlags::Type_Package,
			FCoreRedirectObjectName(
				TEXT("/Game/Core/Systems/SG_Settings")));
	TestEqual(
		TEXT("Legacy audio settings saves resolve the moved settings class"),
		RedirectedSettingsPackage.ToString(),
		FString(
			TEXT(
				"/Game/SurvivalRpg/Core/Systems/"
				"SG_Settings")));

	for (const TTuple<
			FGameplayTag,
			FString,
			UClass*>& Expected :
		{
			MakeTuple(
				BootTag,
				FString(BootScreenClassPath),
				BootScreenClass),
			MakeTuple(
				MainMenuTag,
				FString(MainStackClassPath),
				MainStackClass)
		})
	{
		int32 EntryCount = 0;
		for (const FRpgUIScreenRegistryEntry& Candidate :
			Registry->Screens)
		{
			EntryCount += Candidate.ScreenTag ==
				Expected.Get<0>()
				? 1
				: 0;
		}
		TestEqual(
			*FString::Printf(
				TEXT("%s is registered exactly once"),
				*Expected.Get<0>().ToString()),
			EntryCount,
			1);

		FRpgUIScreenRegistryEntry Entry;
		if (TestTrue(
				*FString::Printf(
					TEXT("%s resolves"),
					*Expected.Get<0>().ToString()),
				Registry->FindScreen(
					Expected.Get<0>(),
					Entry)))
		{
			TestTrue(
				TEXT(
					"Frontend root uses UI.Layer.Menu"),
				Entry.LayerTag == MenuLayerTag);
			TestTrue(
				TEXT(
					"Frontend screen streaming suspends "
					"owning-player input"),
				Entry.bSuspendInputUntilLoaded);
			TestTrue(
				TEXT(
					"Frontend root is single-instance"),
				Entry.bSingleInstance);
			TestEqual(
				TEXT(
					"Frontend registry class path is exact"),
				Entry.WidgetClass.ToSoftObjectPath()
					.ToString(),
				Expected.Get<1>());
			TestEqual(
				TEXT(
					"Frontend registry class loads"),
				Entry.WidgetClass.LoadSynchronous(),
				Expected.Get<2>());
		}
	}

	for (const TTuple<
			UClass*,
			FGameplayTag>& Expected :
		{
			MakeTuple(BootGameModeClass, BootTag),
			MakeTuple(MainGameModeClass, MainMenuTag)
		})
	{
		const ARpgFrontendGameModeBase* Defaults =
			Cast<ARpgFrontendGameModeBase>(
				Expected.Get<0>()->GetDefaultObject());
		if (!TestNotNull(
				TEXT("Frontend GameMode defaults load"),
				Defaults))
		{
			continue;
		}

		TestEqual(
			TEXT(
				"Frontend GameMode uses CommonPlayerController"),
			Defaults->PlayerControllerClass.Get(),
			ACommonPlayerController::StaticClass());
		TestEqual(
			TEXT(
				"Frontend GameMode uses the tag-only HUD "
				"coordinator"),
			Defaults->HUDClass.Get(),
			ARpgFrontendHUD::StaticClass());
		TestNull(
			TEXT(
				"Frontend GameMode never spawns a gameplay pawn"),
			Defaults->DefaultPawnClass.Get());
		TestTrue(
			TEXT(
				"Frontend map configures the expected screen "
				"tag"),
			Defaults->GetInitialScreenTag() ==
				Expected.Get<1>());
	}

	for (UClass* FrontendScreenClass :
		{BootScreenClass, MainStackClass})
	{
		const URpgFrontendScreenWidget* Defaults =
			Cast<URpgFrontendScreenWidget>(
				FrontendScreenClass->GetDefaultObject());
		if (!TestNotNull(
				TEXT("Frontend screen defaults load"),
				Defaults))
		{
			continue;
		}

		const TOptional<FUIInputConfig> InputConfig =
			Defaults->GetDesiredInputConfig();
		TestTrue(
			TEXT(
				"Frontend screen declares an input config"),
			InputConfig.IsSet());
		if (InputConfig.IsSet())
		{
			TestTrue(
				TEXT(
					"Frontend screen uses menu-only input"),
				InputConfig->GetInputMode() ==
					ECommonInputMode::Menu);
			TestTrue(
				TEXT(
					"Frontend screen leaves the mouse "
					"uncaptured"),
				InputConfig->GetMouseCaptureMode() ==
					EMouseCaptureMode::NoCapture);
		}
	}

	const URpgBootScreenWidget* BootDefaults =
		Cast<URpgBootScreenWidget>(
			BootScreenClass->GetDefaultObject());
	if (TestNotNull(
			TEXT("Boot presenter defaults load"),
			BootDefaults))
	{
		const TArray<float>& PageDurations =
			BootDefaults->GetPageDisplayDurations();
		TestEqual(
			TEXT("Boot presenter has one duration per splash page"),
			PageDurations.Num(),
			3);
		if (PageDurations.Num() == 3)
		{
			TestEqual(
				TEXT("First Boot splash lasts one second"),
				PageDurations[0],
				1.0f);
			TestEqual(
				TEXT("Second Boot splash lasts two seconds"),
				PageDurations[1],
				2.0f);
			TestEqual(
				TEXT("Third Boot splash lasts two seconds"),
				PageDurations[2],
				2.0f);
		}
		TestEqual(
			TEXT("Boot presenter travels to the authored Main Menu map"),
			BootDefaults->GetDestinationMap()
				.ToSoftObjectPath()
				.ToString(),
			FString(MainMapPath));
	}

	const UWidgetBlueprintGeneratedClass* GeneratedBootScreen =
		Cast<UWidgetBlueprintGeneratedClass>(BootScreenClass);
	const UWidgetTree* BootScreenTree = GeneratedBootScreen
		? GeneratedBootScreen->GetWidgetTreeArchetype()
		: nullptr;
	if (TestNotNull(
			TEXT("Boot screen owns an authored WidgetTree"),
			BootScreenTree))
	{
		TestNotNull(
			TEXT("Boot presenter binds the authored WidgetSwitcher"),
			Cast<UWidgetSwitcher>(
				BootScreenTree->FindWidget(
					TEXT("WidgetSwitcher"))));
	}

	int32 BootLegacyNodeCount = 0;
	for (const UEdGraph* Graph : BootScreenBlueprint->UbergraphPages)
	{
		BootLegacyNodeCount += Graph ? Graph->Nodes.Num() : 0;
	}
	TestEqual(
		TEXT("Boot screen contains no latent Blueprint routing"),
		BootLegacyNodeCount,
		0);

	const UWidgetBlueprintGeneratedClass* GeneratedMainStack =
		Cast<UWidgetBlueprintGeneratedClass>(MainStackClass);
	const UWidgetTree* MainStackTree = GeneratedMainStack
		? GeneratedMainStack->GetWidgetTreeArchetype()
		: nullptr;
	if (TestNotNull(
			TEXT("Main Menu stack owns an authored WidgetTree"),
			MainStackTree))
	{
		for (const TCHAR* StackName :
			{
				TEXT("MenuStack"),
				TEXT("OptionStack"),
				TEXT("OptionStack_1"),
				TEXT("OptionStack_2"),
				TEXT("PopupStack")
			})
		{
			TestNotNull(
				*FString::Printf(
					TEXT("%s remains an authored stack"),
					StackName),
				Cast<UCommonActivatableWidgetStack>(
					MainStackTree->FindWidget(StackName)));
		}
	}

	const FClassProperty* InitialMenuProperty =
		FindFProperty<FClassProperty>(
			URpgMainMenuStackWidget::StaticClass(),
			TEXT("InitialMenuClass"));
	if (TestNotNull(
			TEXT(
				"Native Main Menu presenter exposes "
				"InitialMenuClass"),
			InitialMenuProperty))
	{
		const UObject* InitialMenuClass =
			InitialMenuProperty->GetPropertyValue_InContainer(
				MainStackClass->GetDefaultObject()).Get();
		TestTrue(
			TEXT(
				"Main Menu root pushes CUI_MainMenu first"),
			InitialMenuClass == MainScreenClass);
	}

	TestTrue(
		TEXT(
			"BPFL_GUI_Library inherits the native local-player "
			"navigation API"),
		NavigationLibraryBlueprint->GeneratedClass &&
			NavigationLibraryBlueprint->GeneratedClass
				->IsChildOf(
					URpgMainMenuNavigationLibrary::
						StaticClass()));

	for (const TCHAR* FunctionName : NavigationFunctionNames)
	{
		const bool bStillAuthoredInBlueprint =
			NavigationLibraryBlueprint->FunctionGraphs
				.ContainsByPredicate(
					[FunctionName](const TObjectPtr<UEdGraph>& Graph)
					{
						return Graph &&
							Graph->GetFName() ==
								FunctionName;
					});
		TestFalse(
			*FString::Printf(
				TEXT(
					"%s is no longer Blueprint-authored "
					"HUD-cast logic"),
				FunctionName),
			bStillAuthoredInBlueprint);
		TestNotNull(
			*FString::Printf(
				TEXT("%s resolves as an inherited function"),
				FunctionName),
			NavigationLibraryBlueprint->GeneratedClass
				? NavigationLibraryBlueprint->GeneratedClass
					->FindFunctionByName(FunctionName)
				: nullptr);
	}

	int32 NavigationCallCount = 0;
	int32 UnresolvedNavigationCallCount = 0;
	int32 NonNativeNavigationCallCount = 0;
	for (const TCHAR* ConsumerPath : NavigationConsumerPaths)
	{
		const UBlueprint* Consumer =
			LoadObject<UBlueprint>(nullptr, ConsumerPath);
		if (!TestNotNull(
				*FString::Printf(
					TEXT("Navigation consumer loads: %s"),
					ConsumerPath),
				Consumer))
		{
			continue;
		}
		TestTrue(
			*FString::Printf(
				TEXT(
					"Navigation consumer is compiled: %s"),
				ConsumerPath),
			Consumer->Status == BS_UpToDate ||
				Consumer->Status ==
					BS_UpToDateWithWarnings);

		TArray<UEdGraph*> Graphs;
		Consumer->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_CallFunction* CallNode =
					Cast<UK2Node_CallFunction>(Node);
				if (!CallNode ||
					!IsNavigationFunction(
						CallNode->GetFunctionName()))
				{
					continue;
				}

				++NavigationCallCount;
				const UFunction* TargetFunction =
					CallNode->GetTargetFunction();
				if (!TargetFunction)
				{
					++UnresolvedNavigationCallCount;
				}
				else if (TargetFunction->GetOuterUClass() !=
					URpgMainMenuNavigationLibrary::
						StaticClass())
				{
					++NonNativeNavigationCallCount;
				}
			}
		}
	}
	TestTrue(
		TEXT(
			"Existing Main Menu navigation calls were retained"),
		NavigationCallCount > 0);
	TestEqual(
		TEXT(
			"Every retained Main Menu navigation call resolves"),
		UnresolvedNavigationCallCount,
		0);
	TestEqual(
		TEXT(
			"Every retained Main Menu call resolves to native "
			"local-player routing"),
		NonNativeNavigationCallCount,
		0);

	const UWorld* BootWorld =
		LoadObject<UWorld>(nullptr, BootMapPath);
	const UWorld* MainWorld =
		LoadObject<UWorld>(nullptr, MainMapPath);
	if (TestNotNull(TEXT("Boot map loads"), BootWorld))
	{
		TestEqual(
			TEXT(
				"Boot map keeps its authored frontend "
				"GameMode"),
			BootWorld->GetWorldSettings()
				->DefaultGameMode.Get(),
			BootGameModeClass);
	}
	if (TestNotNull(TEXT("Main Menu map loads"), MainWorld))
	{
		TestEqual(
			TEXT(
				"Main Menu map keeps its authored frontend "
				"GameMode"),
			MainWorld->GetWorldSettings()
				->DefaultGameMode.Get(),
			MainGameModeClass);
	}

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<
			FAssetRegistryModule>(TEXT("AssetRegistry"))
			.Get();
	AssetRegistry.WaitForCompletion();
	for (const TCHAR* LegacyPackageName :
		LegacyPackageNames)
	{
		FString ExistingFilename;
		TestFalse(
			*FString::Printf(
				TEXT(
					"Legacy frontend package is absent: %s"),
				LegacyPackageName),
			FPackageName::DoesPackageExist(
				LegacyPackageName,
				&ExistingFilename));

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(
			FName(LegacyPackageName),
			Assets,
			/*bIncludeOnlyOnDiskAssets=*/ true,
			/*bSkipARFilteredAssets=*/ false);
		TestEqual(
			*FString::Printf(
				TEXT(
					"Legacy frontend package has no "
					"redirector: %s"),
				LegacyPackageName),
			Assets.Num(),
			0);
	}

	return true;
}

#endif
