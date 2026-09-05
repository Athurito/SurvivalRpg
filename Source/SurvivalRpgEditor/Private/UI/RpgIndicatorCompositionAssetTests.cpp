#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Game/Experience/RpgExperienceActionSet.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddComponents.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddWidgets.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorLayer.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorManagerComponent.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"

namespace RpgIndicatorCompositionAssetTests
{
	constexpr TCHAR HudLayoutClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Hud/"
			"CUI_RpgHudLayout.CUI_RpgHudLayout_C");
	constexpr TCHAR PrototypeExperiencePath[] =
		TEXT(
			"/Game/SurvivalRpg/System/Experiences/"
			"RpgPrototypeExperience.RpgPrototypeExperience");
	constexpr TCHAR StandardUiActionSetPath[] =
		TEXT(
			"/Game/SurvivalRpg/System/Experiences/ActionSets/"
			"LAS_Rpg_StandardUI.LAS_Rpg_StandardUI");
	constexpr TCHAR StandardUiActionSetDirectory[] =
		TEXT(
			"/Game/SurvivalRpg/System/Experiences/ActionSets");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgIndicatorHudCompositionAssetTest,
	"SurvivalRpg.UI.Indicator.CompositionAssets",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgIndicatorHudCompositionAssetTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgIndicatorCompositionAssetTests;

	const UAssetManagerSettings* AssetManagerSettings =
		GetDefault<UAssetManagerSettings>();
	const FPrimaryAssetTypeInfo* ActionSetTypeInfo = nullptr;
	int32 ActionSetTypeCount = 0;
	for (const FPrimaryAssetTypeInfo& TypeInfo :
		AssetManagerSettings->PrimaryAssetTypesToScan)
	{
		if (TypeInfo.PrimaryAssetType ==
			FName(TEXT("RpgExperienceActionSet")))
		{
			++ActionSetTypeCount;
			ActionSetTypeInfo = &TypeInfo;
		}
	}

	TestEqual(
		TEXT(
			"RpgExperienceActionSet has exactly one AssetManager "
			"scan rule"),
		ActionSetTypeCount,
		1);
	if (ActionSetTypeInfo)
	{
		const TArray<FDirectoryPath>& ActionSetDirectories =
			ActionSetTypeInfo->GetDirectories();
		TestEqual(
			TEXT(
				"RpgExperienceActionSet scans exactly one "
				"content directory"),
			ActionSetDirectories.Num(),
			1);
		if (ActionSetDirectories.Num() == 1)
		{
			TestEqual(
				TEXT(
					"RpgExperienceActionSet scans the shared "
					"ActionSets directory"),
				ActionSetDirectories[0].Path,
				FString(StandardUiActionSetDirectory));
		}
		TestEqual(
			TEXT(
				"RpgExperienceActionSet has no competing "
				"explicit asset list"),
			ActionSetTypeInfo->GetSpecificAssets().Num(),
			0);
		TestFalse(
			TEXT(
				"RpgExperienceActionSet assets are concrete "
				"UObjects"),
			ActionSetTypeInfo->bHasBlueprintClasses);
		TestFalse(
			TEXT(
				"RpgExperienceActionSet is available at "
				"runtime"),
			ActionSetTypeInfo->bIsEditorOnly);
		TestTrue(
			TEXT("RpgExperienceActionSet assets are always cooked"),
			ActionSetTypeInfo->Rules.CookRule ==
				EPrimaryAssetCookRule::AlwaysCook);
		TestEqual(
			TEXT(
				"RpgExperienceActionSet scans the exact native "
				"base class"),
			ActionSetTypeInfo->GetAssetBaseClass()
				.ToSoftObjectPath()
				.ToString(),
			FString(
				TEXT(
					"/Script/SurvivalRpg."
					"RpgExperienceActionSet")));
	}

	UClass* HudLayoutClass =
		LoadClass<UUserWidget>(
			nullptr,
			HudLayoutClassPath);
	const UBlueprint* PrototypeExperienceBlueprint =
		LoadObject<UBlueprint>(
			nullptr,
			RpgIndicatorCompositionAssetTests::PrototypeExperiencePath);
	const URpgExperienceActionSet* StandardUiActionSet =
		LoadObject<URpgExperienceActionSet>(
			nullptr,
			StandardUiActionSetPath);
	if (!TestNotNull(
			TEXT("CUI_RpgHudLayout class loads"),
			HudLayoutClass) ||
		!TestNotNull(
			TEXT("RpgPrototypeExperience Blueprint loads"),
			PrototypeExperienceBlueprint) ||
		!TestNotNull(
			TEXT("LAS_Rpg_StandardUI loads"),
			StandardUiActionSet))
	{
		return false;
	}

	const FPrimaryAssetId StandardUiActionSetId =
		StandardUiActionSet->GetPrimaryAssetId();
	const FSoftObjectPath ExpectedActionSetPath(
		StandardUiActionSetPath);
	TestTrue(
		TEXT("LAS_Rpg_StandardUI has a valid primary asset id"),
		StandardUiActionSetId.IsValid());
	TestTrue(
		TEXT(
			"LAS_Rpg_StandardUI uses the "
			"RpgExperienceActionSet type"),
		StandardUiActionSetId.PrimaryAssetType ==
			FPrimaryAssetType(TEXT("RpgExperienceActionSet")));
	TestTrue(
		TEXT(
			"AssetManager resolves LAS_Rpg_StandardUI by its "
			"primary asset id"),
		UAssetManager::Get().GetPrimaryAssetPath(
			StandardUiActionSetId) == ExpectedActionSetPath);
	TestTrue(
		TEXT("LAS_Rpg_StandardUI remains AlwaysCook"),
		UAssetManager::Get()
			.GetPrimaryAssetRules(StandardUiActionSetId)
			.CookRule == EPrimaryAssetCookRule::AlwaysCook);

	const UWidgetBlueprintGeneratedClass* HudGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(
			HudLayoutClass);
	const UWidgetTree* HudTree =
		HudGeneratedClass
			? HudGeneratedClass->GetWidgetTreeArchetype()
			: nullptr;
	const UOverlay* HudRootOverlay =
		HudTree
			? Cast<UOverlay>(HudTree->RootWidget)
			: nullptr;
	if (!TestNotNull(
			TEXT("HUD layout is an authored Widget Blueprint"),
			HudGeneratedClass) ||
		!TestNotNull(
			TEXT("HUD layout owns an authored WidgetTree"),
			HudTree) ||
		!TestNotNull(
			TEXT("HUD layout root is an Overlay"),
			HudRootOverlay))
	{
		return false;
	}

	int32 IndicatorLayerCount = 0;
	const URpgIndicatorLayer* IndicatorLayer = nullptr;
	HudTree->ForEachWidget(
		[&IndicatorLayerCount, &IndicatorLayer](
			UWidget* Widget)
		{
			if (const URpgIndicatorLayer* Candidate =
					Cast<URpgIndicatorLayer>(Widget))
			{
				++IndicatorLayerCount;
				IndicatorLayer = Candidate;
			}
		});

	TestEqual(
		TEXT(
			"HUD layout owns exactly one authored indicator "
			"layer"),
		IndicatorLayerCount,
		1);
	if (!TestNotNull(
			TEXT("Authored indicator layer resolves"),
			IndicatorLayer))
	{
		return false;
	}

	const UWidget* BackMostHudChild =
		HudRootOverlay->GetChildAt(0);
	TestEqual(
		TEXT(
			"Indicator layer is the back-most direct child of "
			"the HUD root"),
		BackMostHudChild,
		static_cast<const UWidget*>(IndicatorLayer));
	TestEqual(
		TEXT("Indicator layer is named explicitly in the Designer"),
		IndicatorLayer->GetFName(),
		FName(TEXT("IndicatorLayer")));
	TestTrue(
		TEXT("Indicator layer is hit-test invisible"),
		IndicatorLayer->GetVisibility() ==
			ESlateVisibility::HitTestInvisible);

	const UOverlaySlot* IndicatorSlot =
		Cast<UOverlaySlot>(IndicatorLayer->Slot);
	if (TestNotNull(
			TEXT("Indicator layer owns an Overlay slot"),
			IndicatorSlot))
	{
		TestTrue(
			TEXT("Indicator layer fills the HUD horizontally"),
			IndicatorSlot->GetHorizontalAlignment() ==
				HAlign_Fill);
		TestTrue(
			TEXT("Indicator layer fills the HUD vertically"),
			IndicatorSlot->GetVerticalAlignment() ==
				VAlign_Fill);
	}

	const URpgExperienceDefinition* PrototypeExperience =
		PrototypeExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(
				PrototypeExperienceBlueprint
					->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (!TestNotNull(
			TEXT(
				"RpgPrototypeExperience generated defaults "
				"load"),
			PrototypeExperience))
	{
		return false;
	}

	const FSoftObjectPath ExpectedHudLayoutPath(
		HudLayoutClass);
	const FGameplayTag GameLayerTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.Layer.Game"),
			/*ErrorIfNotFound=*/ false);
	TestTrue(
		TEXT("UI.Layer.Game is registered"),
		GameLayerTag.IsValid());
	int32 HudLayoutRegistrationCount = 0;
	for (const UGameFeatureAction* Action :
		PrototypeExperience->Actions)
	{
		const URpgGameFeatureAction_AddWidgets*
			AddWidgetsAction =
				Cast<URpgGameFeatureAction_AddWidgets>(
					Action);
		if (!AddWidgetsAction)
		{
			continue;
		}

		for (const FRpgGameFeatureWidgetLayoutEntry& Entry :
			AddWidgetsAction->Layouts)
		{
			if (Entry.LayoutClass.ToSoftObjectPath() !=
				ExpectedHudLayoutPath)
			{
				continue;
			}

			++HudLayoutRegistrationCount;
			TestTrue(
				TEXT(
					"CUI_RpgHudLayout is pushed to "
					"UI.Layer.Game"),
				Entry.LayerTag == GameLayerTag);
		}
	}
	TestEqual(
		TEXT(
			"RpgPrototypeExperience registers "
			"CUI_RpgHudLayout exactly once"),
		HudLayoutRegistrationCount,
		1);

	int32 StandardUiActionSetCount = 0;
	for (const URpgExperienceActionSet* ActionSet :
		PrototypeExperience->ActionSets)
	{
		StandardUiActionSetCount +=
			ActionSet == StandardUiActionSet
				? 1
				: 0;
	}
	TestEqual(
		TEXT(
			"RpgPrototypeExperience composes "
			"LAS_Rpg_StandardUI exactly once"),
		StandardUiActionSetCount,
		1);

	const FSoftObjectPath ExpectedManagerPath(
		URpgIndicatorManagerComponent::StaticClass());
	const FSoftObjectPath ExpectedControllerPath(
		ARpgPlayerController::StaticClass());
	int32 IndicatorManagerRegistrationCount = 0;
	for (const UGameFeatureAction* Action :
		StandardUiActionSet->Actions)
	{
		const URpgGameFeatureAction_AddComponents*
			AddComponentsAction =
				Cast<URpgGameFeatureAction_AddComponents>(
					Action);
		if (!AddComponentsAction)
		{
			continue;
		}

		for (const FRpgGameFeatureComponentEntry& Entry :
			AddComponentsAction->ComponentList)
		{
			if (Entry.ComponentClass.ToSoftObjectPath() !=
				ExpectedManagerPath)
			{
				continue;
			}

			++IndicatorManagerRegistrationCount;
			TestTrue(
				TEXT(
					"Indicator manager targets the RPG player "
					"controller"),
				Entry.ActorClass.ToSoftObjectPath() ==
					ExpectedControllerPath);
			TestTrue(
				TEXT(
					"Indicator manager is available on local "
					"clients"),
				Entry.bClientComponent);
		}
	}
	TestEqual(
		TEXT(
			"LAS_Rpg_StandardUI registers the indicator manager "
			"exactly once"),
		IndicatorManagerRegistrationCount,
		1);

	return true;
}

#endif
