#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Game/RpgFrontendGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/UI/RpgFrontendHUD.h"
#include "SurvivalRpg/UI/RpgFrontendWidgets.h"
#include "SurvivalRpg/UI/RpgMainMenuNavigationLibrary.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"
#include "SurvivalRpg/UI/RpgUIScreenRegistry.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "CommonActivatableWidget.h"
#include "CommonPlayerController.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Input/UIActionBindingHandle.h"
#include "Internationalization/Text.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Tunnel.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
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
	constexpr TCHAR PlayMenuClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/PlayMenu/"
			"CUI_PlayMenu.CUI_PlayMenu_C");
	constexpr TCHAR LoadingMenuClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/LoadingMenu/"
			"CUI_LoadingMenu.CUI_LoadingMenu_C");
	constexpr TCHAR LoadingListClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/LoadingMenu/"
			"CUI_LoadingList.CUI_LoadingList_C");
	constexpr TCHAR PauseMenuClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Menus/GameMenu/PauseMenu/"
			"CUI_PauseMenu.CUI_PauseMenu_C");
	constexpr TCHAR ExitDesktopPopupClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Popup/"
			"CUI_ExitDesktop.CUI_ExitDesktop_C");
	constexpr TCHAR BackToMainMenuPopupClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Popup/"
			"CUI_BackToMainMenu.CUI_BackToMainMenu_C");
	constexpr TCHAR SettingsMenuClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/SettingsMenu/"
			"CUI_SettingsMenu.CUI_SettingsMenu_C");
	constexpr TCHAR LazySettingsPageClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/SettingsMenu/Modular/"
			"CUI_LazySettingsPage.CUI_LazySettingsPage_C");
	constexpr TCHAR AudioSettingsClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/SettingsMenu/ChildSettings/"
			"CUI_AudioSettings.CUI_AudioSettings_C");
	constexpr TCHAR GraphicSettingsClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/SettingsMenu/ChildSettings/"
			"CUI_GraphicSettings.CUI_GraphicSettings_C");
	constexpr TCHAR ApplyGraphicSettingsClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/SettingsMenu/Modular/"
			"ApplyButton/CUI_ApplyOption.CUI_ApplyOption_C");

	constexpr TCHAR MainMenuTableId[] =
		TEXT(
			"/Game/SurvivalRpg/Localization/Language/"
			"ST_MainMenu.ST_MainMenu");
	constexpr TCHAR PlayMenuTableId[] =
		TEXT(
			"/Game/SurvivalRpg/Localization/Language/"
			"ST_PlayMenu.ST_PlayMenu");
	constexpr TCHAR PauseMenuTableId[] =
		TEXT(
			"/Game/SurvivalRpg/Localization/Language/"
			"ST_PauseMenu.ST_PauseMenu");
	constexpr TCHAR PopupWindowTableId[] =
		TEXT(
			"/Game/SurvivalRpg/Localization/Language/"
			"ST_PopupWindow.ST_PopupWindow");
	constexpr TCHAR SettingsTabListTableId[] =
		TEXT(
			"/Game/SurvivalRpg/Localization/Language/"
			"ST_SettingsTabListButtons."
			"ST_SettingsTabListButtons");
	constexpr TCHAR LegacySettingsTabListTableId[] =
		TEXT(
			"/Game/Localization/Language/"
			"ST_SettingsTabListButtons."
			"ST_SettingsTabListButtons");
	constexpr TCHAR AudioSettingsTableId[] =
		TEXT(
			"/Game/SurvivalRpg/Localization/Language/"
			"ST_AudioSettings.ST_AudioSettings");
	constexpr TCHAR GraphicSettingsTableId[] =
		TEXT(
			"/Game/SurvivalRpg/Localization/Language/"
			"ST_GraphicSettings.ST_GraphicSettings");

	struct FLocalizedWidgetTextExpectation
	{
		const TCHAR* WidgetClassPath;
		const TCHAR* WidgetName;
		const TCHAR* PropertyName;
		const TCHAR* TableId;
		const TCHAR* Key;
	};

	const FLocalizedWidgetTextExpectation LocalizedTextBindings[] = {
		{
			MainScreenClassPath,
			TEXT("Button_Play"),
			TEXT("ButtonText"),
			MainMenuTableId,
			TEXT("Btn_MainMenu_Play")
		},
		{
			MainScreenClassPath,
			TEXT("Button_Load"),
			TEXT("ButtonText"),
			MainMenuTableId,
			TEXT("Btn_MainMenu_Load")
		},
		{
			MainScreenClassPath,
			TEXT("Button_Settings"),
			TEXT("ButtonText"),
			MainMenuTableId,
			TEXT("Btn_MainMenu_Settings")
		},
		{
			MainScreenClassPath,
			TEXT("Button_Credits"),
			TEXT("ButtonText"),
			MainMenuTableId,
			TEXT("Btn_MainMenu_Credits")
		},
		{
			MainScreenClassPath,
			TEXT("Button_Quit"),
			TEXT("ButtonText"),
			MainMenuTableId,
			TEXT("Btn_MainMenu_Quit")
		},
		{
			PlayMenuClassPath,
			TEXT("Button_PlaySolo"),
			TEXT("HeaderText"),
			PlayMenuTableId,
			TEXT("Btn_PlayMenu_PlaySolo")
		},
		{
			PlayMenuClassPath,
			TEXT("Button_PlaySolo"),
			TEXT("DescriptionText"),
			PlayMenuTableId,
			TEXT("Txt_PlaySolo")
		},
		{
			PlayMenuClassPath,
			TEXT("Button_Host"),
			TEXT("HeaderText"),
			PlayMenuTableId,
			TEXT("Btn_PlayMenu_Host")
		},
		{
			PlayMenuClassPath,
			TEXT("Button_Host"),
			TEXT("DescriptionText"),
			PlayMenuTableId,
			TEXT("Txt_Host")
		},
		{
			PlayMenuClassPath,
			TEXT("Button_Join"),
			TEXT("HeaderText"),
			PlayMenuTableId,
			TEXT("Btn_PlayMenu_Join")
		},
		{
			PlayMenuClassPath,
			TEXT("Button_Join"),
			TEXT("DescriptionText"),
			PlayMenuTableId,
			TEXT("Txt_Join")
		},
		{
			LoadingMenuClassPath,
			TEXT("Button_PlaySolo"),
			TEXT("HeaderText"),
			PlayMenuTableId,
			TEXT("Btn_PlayMenu_PlaySolo")
		},
		{
			LoadingMenuClassPath,
			TEXT("Button_PlaySolo"),
			TEXT("DescriptionText"),
			PlayMenuTableId,
			TEXT("Txt_PlaySolo")
		},
		{
			LoadingMenuClassPath,
			TEXT("Button_Host"),
			TEXT("HeaderText"),
			PlayMenuTableId,
			TEXT("Btn_PlayMenu_Host")
		},
		{
			LoadingMenuClassPath,
			TEXT("Button_Host"),
			TEXT("DescriptionText"),
			PlayMenuTableId,
			TEXT("Txt_Host")
		},
		{
			LoadingListClassPath,
			TEXT("---DisplayCategory---"),
			TEXT("CategoryText"),
			PlayMenuTableId,
			TEXT("Category_Loading_SavedGames")
		},
		{
			PauseMenuClassPath,
			TEXT("Button_Continue"),
			TEXT("ButtonText"),
			PauseMenuTableId,
			TEXT("Btn_PauseMenu_Continue")
		},
		{
			PauseMenuClassPath,
			TEXT("Button_Save"),
			TEXT("ButtonText"),
			PauseMenuTableId,
			TEXT("Btn_PauseMenu_Save")
		},
		{
			PauseMenuClassPath,
			TEXT("Button_Settings"),
			TEXT("ButtonText"),
			PauseMenuTableId,
			TEXT("Btn_PauseMenu_Settings")
		},
		{
			PauseMenuClassPath,
			TEXT("Button_ExitMenu"),
			TEXT("ButtonText"),
			PauseMenuTableId,
			TEXT("Btn_PauseMenu_ExitMenu")
		},
		{
			PauseMenuClassPath,
			TEXT("Button_ExitDesktop"),
			TEXT("ButtonText"),
			PauseMenuTableId,
			TEXT("Btn_PauseMenu_ExitDesktop")
		},
		{
			ExitDesktopPopupClassPath,
			TEXT("Txt_Confirmation"),
			TEXT("Text"),
			PopupWindowTableId,
			TEXT("Txt_ExitGame")
		},
		{
			ExitDesktopPopupClassPath,
			TEXT("Button_Accept"),
			TEXT("ButtonText"),
			PopupWindowTableId,
			TEXT("Btn_Popup_Accept")
		},
		{
			ExitDesktopPopupClassPath,
			TEXT("Button_Cancel"),
			TEXT("ButtonText"),
			PopupWindowTableId,
			TEXT("Btn_Popup_Cancel")
		},
		{
			BackToMainMenuPopupClassPath,
			TEXT("Txt_Confirmation"),
			TEXT("Text"),
			PopupWindowTableId,
			TEXT("Txt_BackToMenu")
		},
		{
			BackToMainMenuPopupClassPath,
			TEXT("Button_Accept"),
			TEXT("ButtonText"),
			PopupWindowTableId,
			TEXT("Btn_Popup_Accept")
		},
		{
			BackToMainMenuPopupClassPath,
			TEXT("Button_Cancel"),
			TEXT("ButtonText"),
			PopupWindowTableId,
			TEXT("Btn_Popup_Cancel")
		},
		{
			AudioSettingsClassPath,
			TEXT("VolumeCategory"),
			TEXT("CategoryText"),
			AudioSettingsTableId,
			TEXT("Category_Volume")
		},
		{
			AudioSettingsClassPath,
			TEXT("MasterVolume"),
			TEXT("OptionText"),
			AudioSettingsTableId,
			TEXT("Setting_Audio_Master")
		},
		{
			AudioSettingsClassPath,
			TEXT("MusicVolume"),
			TEXT("OptionText"),
			AudioSettingsTableId,
			TEXT("Setting_Audio_Music")
		},
		{
			AudioSettingsClassPath,
			TEXT("EffectsVolume"),
			TEXT("OptionText"),
			AudioSettingsTableId,
			TEXT("Setting_Audio_Effects")
		},
		{
			AudioSettingsClassPath,
			TEXT("UserInterfaceVolume"),
			TEXT("OptionText"),
			AudioSettingsTableId,
			TEXT("Setting_Audio_UI")
		},
		{
			AudioSettingsClassPath,
			TEXT("AmbienceVolume"),
			TEXT("OptionText"),
			AudioSettingsTableId,
			TEXT("Setting_Audio_Ambience")
		},
		{
			GraphicSettingsClassPath,
			TEXT("---DisplayCategory---"),
			TEXT("CategoryText"),
			GraphicSettingsTableId,
			TEXT("Category_Display")
		},
		{
			GraphicSettingsClassPath,
			TEXT("---GraphicsCategory---"),
			TEXT("CategoryText"),
			GraphicSettingsTableId,
			TEXT("Category_Graphic")
		},
		{
			GraphicSettingsClassPath,
			TEXT("---AdvancedGraphicsCategory---"),
			TEXT("CategoryText"),
			GraphicSettingsTableId,
			TEXT("Category_AdvancedGraphics")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_WindowMode"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Display_Windowmode")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Resolution"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Display_Resolution")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_GraphicPresets"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Preset")
		},
		{
			GraphicSettingsClassPath,
			TEXT("ResolutionScaling"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_ResolutionScaling")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_AntiAliasing"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_AntiAliasing")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_ViewDistance"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_ViewDistance")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Textures"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Textures")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Lighting"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Lighting")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Shadows"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Shadow")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Reflections"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Reflection")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_PostProcessing"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_PostProcessing")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Effects"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Effects")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Foliage"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Foliage")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_Shading"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_Shading")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_FrameRateLimit"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Advanced_FrameRateLimit")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_VSync"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Advanced_VSync")
		},
		{
			GraphicSettingsClassPath,
			TEXT("Option_AutoSetGraphics"),
			TEXT("OptionText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_AutoSetQuality")
		},
		{
			ApplyGraphicSettingsClassPath,
			TEXT("CUI_ApplyButtonBase"),
			TEXT("ButtonText"),
			GraphicSettingsTableId,
			TEXT("Setting_Graphic_AutoSetQuality")
		}
	};

	const UWidgetTree* LoadAuthoredWidgetTree(
		FAutomationTestBase& Test,
		const TCHAR* WidgetClassPath)
	{
		UClass* WidgetClass =
			LoadClass<UUserWidget>(nullptr, WidgetClassPath);
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("Localized widget class loads: %s"),
					WidgetClassPath),
				WidgetClass))
		{
			return nullptr;
		}

		const UWidgetBlueprintGeneratedClass* GeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT(
						"Localized widget owns a generated "
						"Widget Blueprint class: %s"),
					WidgetClassPath),
				GeneratedClass))
		{
			return nullptr;
		}

		const UWidgetTree* WidgetTree =
			GeneratedClass->GetWidgetTreeArchetype();
		Test.TestNotNull(
			*FString::Printf(
				TEXT("Localized widget owns an authored tree: %s"),
				WidgetClassPath),
			WidgetTree);
		return WidgetTree;
	}

	void TestStringTableText(
		FAutomationTestBase& Test,
		const FString& BindingLabel,
		const FText& TextValue,
		const TCHAR* ExpectedTableId,
		const TCHAR* ExpectedKey)
	{
		Test.TestFalse(
			*FString::Printf(
				TEXT("%s resolves to non-empty text"),
				*BindingLabel),
			TextValue.IsEmpty());

		FName ActualTableId;
		FString ActualKey;
		const bool bIsStringTableText =
			FTextInspector::GetTableIdAndKey(
				TextValue,
				ActualTableId,
				ActualKey);
		if (!Test.TestTrue(
				*FString::Printf(
					TEXT("%s is backed by a String Table"),
					*BindingLabel),
				bIsStringTableText))
		{
			return;
		}

		Test.TestEqual(
			*FString::Printf(
				TEXT("%s uses the moved String Table"),
				*BindingLabel),
			ActualTableId,
			FName(ExpectedTableId));
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s uses the intended localization key"),
				*BindingLabel),
			ActualKey,
			FString(ExpectedKey));
	}

	void TestLocalizedWidgetText(
		FAutomationTestBase& Test,
		const UWidgetTree& WidgetTree,
		const FLocalizedWidgetTextExpectation& Expected)
	{
		const FString BindingLabel = FString::Printf(
			TEXT("%s.%s.%s"),
			Expected.WidgetClassPath,
			Expected.WidgetName,
			Expected.PropertyName);
		const UWidget* Widget = WidgetTree.FindWidget(
			FName(Expected.WidgetName));
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s widget is authored"),
					*BindingLabel),
				Widget))
		{
			return;
		}

		const FTextProperty* TextProperty =
			FindFProperty<FTextProperty>(
				Widget->GetClass(),
				FName(Expected.PropertyName));
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s is an FText property"),
					*BindingLabel),
				TextProperty))
		{
			return;
		}

		TestStringTableText(
			Test,
			BindingLabel,
			TextProperty->GetPropertyValue_InContainer(Widget),
			Expected.TableId,
			Expected.Key);
	}

	void TestSettingsPageComposition(
		FAutomationTestBase& Test)
	{
		UClass* LazyPageClass = LoadClass<UCommonActivatableWidget>(
			nullptr,
			LazySettingsPageClassPath);
		const UWidgetTree* SettingsTree =
			LoadAuthoredWidgetTree(Test, SettingsMenuClassPath);
		if (!Test.TestNotNull(
				TEXT("Lazy Settings page inherits CommonActivatableWidget"),
				LazyPageClass) || !SettingsTree)
		{
			return;
		}

		const FSoftClassProperty* ContentClassProperty =
			FindFProperty<FSoftClassProperty>(LazyPageClass, TEXT("ContentClass"));
		const FNameProperty* TabIdProperty =
			FindFProperty<FNameProperty>(LazyPageClass, TEXT("TabId"));
		const FTextProperty* TabLabelProperty =
			FindFProperty<FTextProperty>(LazyPageClass, TEXT("TabLabel"));
		if (!Test.TestNotNull(
				TEXT("Lazy Settings page exposes a soft ContentClass"),
				ContentClassProperty) ||
			!Test.TestNotNull(
				TEXT("Lazy Settings page exposes a name TabId"),
				TabIdProperty) ||
			!Test.TestNotNull(
				TEXT("Lazy Settings page exposes an FText TabLabel"),
				TabLabelProperty))
		{
			return;
		}

		Test.TestTrue(
			TEXT("Lazy Settings page content accepts activatable widget classes"),
			ContentClassProperty->MetaClass &&
				ContentClassProperty->MetaClass->IsChildOf(
					UCommonActivatableWidget::StaticClass()));
		for (const FProperty* Property :
			{
				static_cast<const FProperty*>(ContentClassProperty),
				static_cast<const FProperty*>(TabIdProperty),
				static_cast<const FProperty*>(TabLabelProperty)
			})
		{
			Test.TestTrue(
				*FString::Printf(
					TEXT("Lazy Settings page %s is designer-editable Blueprint data"),
					*Property->GetName()),
				Property->HasAllPropertyFlags(CPF_Edit | CPF_BlueprintVisible) &&
					!Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
		}

		int32 PageCount = 0;
		TSet<FName> TabIds;
		bool bUsesLegacyTable = false;
		SettingsTree->ForEachWidget(
			[&](UWidget* Widget)
			{
				if (!Widget || !Widget->IsA(LazyPageClass))
				{
					return;
				}

				++PageCount;
				const FString PageLabel = Widget->GetName();
				const FName TabId = TabIdProperty->GetPropertyValue_InContainer(Widget);
				Test.TestFalse(
					*FString::Printf(TEXT("%s has a non-empty tab ID"), *PageLabel),
					TabId.IsNone());
				Test.TestFalse(
					*FString::Printf(TEXT("%s has a unique tab ID"), *PageLabel),
					TabIds.Contains(TabId));
				TabIds.Add(TabId);

				const FText TabLabel =
					TabLabelProperty->GetPropertyValue_InContainer(Widget);
				Test.TestFalse(
					*FString::Printf(TEXT("%s resolves to a non-empty tab label"), *PageLabel),
					TabLabel.IsEmpty());
				FName TableId;
				FString TableKey;
				if (FTextInspector::GetTableIdAndKey(TabLabel, TableId, TableKey))
				{
					bUsesLegacyTable |= TableId == FName(LegacySettingsTabListTableId);
				}

				const FSoftObjectPtr ContentClass =
					ContentClassProperty->GetPropertyValue_InContainer(Widget);
				UClass* PageClass = Cast<UClass>(ContentClass.ToSoftObjectPath().TryLoad());
				if (Test.TestNotNull(
						*FString::Printf(TEXT("%s soft page class loads"), *PageLabel),
						PageClass))
				{
					Test.TestTrue(
						*FString::Printf(TEXT("%s page is activatable"), *PageLabel),
						PageClass->IsChildOf(UCommonActivatableWidget::StaticClass()));
					Test.TestFalse(
						*FString::Printf(TEXT("%s page is concrete"), *PageLabel),
						PageClass->HasAnyClassFlags(CLASS_Abstract));
				}
			});
		Test.TestTrue(TEXT("Settings authors at least one lazy page"), PageCount > 0);

		if (bUsesLegacyTable)
		{
			TArray<FString> Redirects;
			GConfig->GetArray(
				TEXT("Core.StringTable"),
				TEXT("StringTableRedirects"),
				Redirects,
				GEngineIni);
			const bool bHasExpectedRedirect =
				Redirects.ContainsByPredicate(
					[](const FString& Redirect)
					{
						return Redirect.Contains(
							LegacySettingsTabListTableId) &&
							Redirect.Contains(
								SettingsTabListTableId);
					});
			Test.TestTrue(
				TEXT(
					"The legacy Settings table ID redirects to "
					"the moved table"),
				bHasExpectedRedirect);
		}
	}

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

	struct FPromotedScreenExpectation
	{
		const TCHAR* ScreenTag;
		const TCHAR* WidgetPackage;
		bool bModal = false;
		bool bPayloadReceiver = false;
	};

	const FPromotedScreenExpectation PromotedScreens[] = {
		{TEXT("UI.Screen.Play"), TEXT("/Game/SurvivalRpg/UI/PlayMenu/CUI_PlayMenu")},
		{TEXT("UI.Screen.Credits"), TEXT("/Game/SurvivalRpg/UI/Credits/CUI_Credits")},
		{TEXT("UI.Screen.LoadGame"), TEXT("/Game/SurvivalRpg/UI/Menus/LoadingMenu/CUI_LoadingMenu")},
		{TEXT("UI.Screen.NewGame.SinglePlayer"), TEXT("/Game/SurvivalRpg/UI/Menus/NewGameMenu/Singleplayer/CUI_NewGameSinglePlayer")},
		{TEXT("UI.Screen.NewGame.CreateSession"), TEXT("/Game/SurvivalRpg/UI/Menus/NewGameMenu/Multiplayer/CUI_NewGameCreateSession")},
		{TEXT("UI.Screen.ServerBrowser"), TEXT("/Game/SurvivalRpg/UI/PlayMenu/Sessions/CUI_ServerBrowser")},
		{TEXT("UI.Screen.LoadGame.SinglePlayer"), TEXT("/Game/SurvivalRpg/UI/Menus/LoadingMenu/Singleplayer/CUI_SinglePlayerLoading")},
		{TEXT("UI.Screen.LoadGame.Multiplayer"), TEXT("/Game/SurvivalRpg/UI/Menus/LoadingMenu/Multiplayer/CUI_MultiplayerLoading")},
		{TEXT("UI.Screen.LoadGame.SinglePlayer.Settings"), TEXT("/Game/SurvivalRpg/UI/Menus/LoadingMenu/Singleplayer/CUI_SinglePlayerLoadingSettings"), false, true},
		{TEXT("UI.Screen.LoadGame.Multiplayer.Settings"), TEXT("/Game/SurvivalRpg/UI/Menus/LoadingMenu/Multiplayer/CUI_MultiplayerLoadingSettings"), false, true},
		{TEXT("UI.Screen.ExitDesktop"), TEXT("/Game/SurvivalRpg/UI/Popup/CUI_ExitDesktop"), true},
		{TEXT("UI.Screen.BackToMainMenu"), TEXT("/Game/SurvivalRpg/UI/Popup/CUI_BackToMainMenu"), true}
	};

	TSet<const UEdGraphNode*> FindExecutableNodes(const UEdGraph& Graph)
	{
		TArray<const UEdGraphNode*> Pending;
		for (const UEdGraphNode* Node : Graph.Nodes)
		{
			const UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node);
			if (Node && (Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>() ||
				(Tunnel && Tunnel->bCanHaveOutputs && !Tunnel->bCanHaveInputs)))
			{
				Pending.Add(Node);
			}
		}
		TSet<const UEdGraphNode*> Executable;
		while (!Pending.IsEmpty())
		{
			const UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
			if (!Node || Executable.Contains(Node)) continue;
			Executable.Add(Node);
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					for (const UEdGraphPin* Linked : Pin->LinkedTo)
					{
						if (Linked) Pending.Add(Linked->GetOwningNode());
					}
				}
			}
		}
		return Executable;
	}

	bool IsNavigationFunction(const FName FunctionName)
	{
		for (const TCHAR* Candidate : NavigationFunctionNames)
		{
			if (FunctionName == Candidate)
			{
				return true;
			}
		}
		for (const TCHAR* Candidate :
			{TEXT("PushToMainStack"), TEXT("PushToOptionStack"), TEXT("PushToPopupStack"),
			 TEXT("PushToOption1Stack"), TEXT("PushToOption2Stack")})
		{
			if (FunctionName == Candidate) return true;
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
	UClass* SettingsScreenClass =
		LoadClass<URpgFrontendScreenWidget>(
			nullptr,
			SettingsMenuClassPath);
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
	const FGameplayTag SettingsTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.Screen.Settings"));
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
			TEXT("Settings inherits the frontend screen input contract"),
			SettingsScreenClass) ||
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
				MainStackClass),
			MakeTuple(
				SettingsTag,
				FString(SettingsMenuClassPath),
				SettingsScreenClass)
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

	TArray<UClass*> FrontendScreenClasses = {BootScreenClass, MainStackClass, SettingsScreenClass};
	for (const FPromotedScreenExpectation& Expected : PromotedScreens)
	{
		const FGameplayTag ScreenTag = FGameplayTag::RequestGameplayTag(Expected.ScreenTag, false);
		const FGameplayTag LayerTag = FGameplayTag::RequestGameplayTag(
			Expected.bModal ? TEXT("UI.Layer.Modal") : TEXT("UI.Layer.Menu"), false);
		TestTrue(*FString::Printf(TEXT("%s is a registered gameplay tag"), Expected.ScreenTag), ScreenTag.IsValid());
		int32 EntryCount = 0;
		for (const FRpgUIScreenRegistryEntry& Candidate : Registry->Screens)
			EntryCount += Candidate.ScreenTag == ScreenTag ? 1 : 0;
		TestEqual(*FString::Printf(TEXT("%s has exactly one registry entry"), Expected.ScreenTag), EntryCount, 1);
		FRpgUIScreenRegistryEntry Entry;
		if (!TestTrue(*FString::Printf(TEXT("%s resolves exactly"), Expected.ScreenTag), Registry->FindScreen(ScreenTag, Entry)))
			continue;
		const FString ClassPath = FString::Printf(TEXT("%s.%s_C"), Expected.WidgetPackage,
			*FPackageName::GetShortName(Expected.WidgetPackage));
		TestEqual(*FString::Printf(TEXT("%s targets its authored screen"), Expected.ScreenTag),
			Entry.WidgetClass.ToSoftObjectPath().ToString(), ClassPath);
		TestTrue(*FString::Printf(TEXT("%s uses its menu or modal layer"), Expected.ScreenTag), Entry.LayerTag == LayerTag);
		TestTrue(*FString::Printf(TEXT("%s suspends input while streaming"), Expected.ScreenTag), Entry.bSuspendInputUntilLoaded);
		TestTrue(*FString::Printf(TEXT("%s retains a single local-player instance"), Expected.ScreenTag), Entry.bSingleInstance);
		UClass* ScreenClass = Entry.WidgetClass.LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("%s soft class loads"), Expected.ScreenTag), ScreenClass)) continue;
		TestTrue(*FString::Printf(TEXT("%s inherits the frontend input contract"), Expected.ScreenTag),
			ScreenClass->IsChildOf(URpgFrontendScreenWidget::StaticClass()));
		TestFalse(*FString::Printf(TEXT("%s is concrete"), Expected.ScreenTag), ScreenClass->HasAnyClassFlags(CLASS_Abstract));
		const UBlueprint* ScreenBlueprint = Cast<UBlueprint>(ScreenClass->ClassGeneratedBy);
		if (TestNotNull(*FString::Printf(TEXT("%s is designer-authored"), Expected.ScreenTag), ScreenBlueprint))
		{
			TestTrue(*FString::Printf(TEXT("%s is compiled"), Expected.ScreenTag),
				ScreenBlueprint->Status == BS_UpToDate || ScreenBlueprint->Status == BS_UpToDateWithWarnings);
		}
		if (Expected.bPayloadReceiver)
		{
			TestTrue(*FString::Printf(TEXT("%s accepts the selected-save payload"), Expected.ScreenTag),
				ScreenClass->ImplementsInterface(URpgUIScreenPayloadReceiver::StaticClass()));
		}
		FrontendScreenClasses.AddUnique(ScreenClass);
	}

	for (UClass* FrontendScreenClass : FrontendScreenClasses)
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

	int32 ReachableLegacyPushCount = 0;
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
		bool bUsesScreenRouter = false;
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			// Disconnected editor leftovers cannot execute and do not define a route.
			for (const UEdGraphNode* Node : FindExecutableNodes(*Graph))
			{
				const UK2Node_CallFunction* CallNode =
					Cast<UK2Node_CallFunction>(Node);
				if (!CallNode || !CallNode->IsNodeEnabled())
				{
					continue;
				}
				if (IsNavigationFunction(CallNode->GetFunctionName()))
				{
					++ReachableLegacyPushCount;
					AddError(FString::Printf(TEXT("%s still executes legacy navigation %s"),
						ConsumerPath, *CallNode->GetFunctionName().ToString()));
					continue;
				}
				const bool bSubsystemCall = CallNode->GetFunctionName() ==
					GET_FUNCTION_NAME_CHECKED(URpgUIScreenSubsystem, OpenScreen);
				const bool bLibraryCall = CallNode->GetFunctionName() ==
					GET_FUNCTION_NAME_CHECKED(URpgUIScreenBlueprintLibrary, OpenUIScreen);
				if (!bSubsystemCall && !bLibraryCall)
					continue;
				const UFunction* TargetFunction =
					CallNode->GetTargetFunction();
				const bool bResolvedRouter = TargetFunction &&
					((bSubsystemCall && TargetFunction->GetOuterUClass() == URpgUIScreenSubsystem::StaticClass()) ||
					 (bLibraryCall && TargetFunction->GetOuterUClass() == URpgUIScreenBlueprintLibrary::StaticClass()));
				TestTrue(*FString::Printf(TEXT("%s screen-opening call resolves to the local-player router"), ConsumerPath), bResolvedRouter);
				bUsesScreenRouter |= bResolvedRouter;
				const UEdGraphPin* TagPin = CallNode->FindPin(TEXT("ScreenTag"), EGPD_Input);
				if (TagPin && TagPin->LinkedTo.IsEmpty())
				{
					FGameplayTag RouteTag;
					RouteTag.FromExportString(TagPin->DefaultValue);
					FRpgUIScreenRegistryEntry RouteEntry;
					TestTrue(*FString::Printf(TEXT("%s literal route %s resolves in the registry"), ConsumerPath, *RouteTag.ToString()),
						RouteTag.IsValid() && Registry->FindScreen(RouteTag, RouteEntry));
				}
			}
		}
		TestTrue(*FString::Printf(TEXT("%s opens screens through the local-player registry"), ConsumerPath), bUsesScreenRouter);
	}
	TestEqual(
		TEXT("Main Menu consumers have no reachable legacy stack pushes"),
		ReachableLegacyPushCount,
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFrontendLocalizedTextBindingsAssetTest,
	"SurvivalRpg.UI.Frontend.LocalizedTextBindings",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFrontendLocalizedTextBindingsAssetTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgFrontendCompositionAssetTests;

	TMap<FString, const UWidgetTree*> WidgetTrees;
	for (const FLocalizedWidgetTextExpectation& Expected :
		LocalizedTextBindings)
	{
		const FString WidgetClassPath(Expected.WidgetClassPath);
		if (!WidgetTrees.Contains(WidgetClassPath))
		{
			WidgetTrees.Add(
				WidgetClassPath,
				LoadAuthoredWidgetTree(
					*this,
					Expected.WidgetClassPath));
		}

		const UWidgetTree* WidgetTree =
			WidgetTrees.FindRef(WidgetClassPath);
		if (WidgetTree)
		{
			TestLocalizedWidgetText(
				*this,
				*WidgetTree,
				Expected);
		}
	}

	TestSettingsPageComposition(*this);

	return true;
}

#endif
