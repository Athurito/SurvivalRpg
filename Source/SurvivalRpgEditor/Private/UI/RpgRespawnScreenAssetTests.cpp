#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/UI/RpgRespawnScreenWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenRegistry.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace RpgRespawnScreenAssetTests
{
	constexpr TCHAR ScreenRegistryPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/"
			"DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry");
	constexpr TCHAR RespawnScreenClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Respawn/"
			"CUI_RespawnScreen.CUI_RespawnScreen_C");
	constexpr TCHAR RespawnScreenBlueprintPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Respawn/"
			"CUI_RespawnScreen.CUI_RespawnScreen");
	constexpr TCHAR PlayerControllerBlueprintPath[] =
		TEXT(
			"/Game/SurvivalRpg/Core/Player/"
			"BP_Rpg_PlayerController.BP_Rpg_PlayerController");
	constexpr TCHAR PlayerControllerPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Core/Player/"
			"BP_Rpg_PlayerController");
	constexpr TCHAR LegacyDeathScreenPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/DeathTest");

	bool HasBlueprintGraphLogic(const UWidgetBlueprint& Blueprint)
	{
		const auto HasNodes =
			[](const TArray<TObjectPtr<UEdGraph>>& Graphs)
			{
				return Graphs.ContainsByPredicate(
					[](const TObjectPtr<UEdGraph>& Graph)
					{
						return Graph && !Graph->Nodes.IsEmpty();
					});
			};

		return HasNodes(Blueprint.UbergraphPages)
			|| HasNodes(Blueprint.FunctionGraphs)
			|| HasNodes(Blueprint.MacroGraphs)
			|| HasNodes(Blueprint.DelegateSignatureGraphs);
	}

	bool IsCreateWidgetNode(const UEdGraphNode* Node)
	{
		return Node &&
			Node->GetClass()->GetName().Contains(
				TEXT("CreateWidget"));
	}

	bool IsForbiddenViewportFunction(const FName FunctionName)
	{
		return FunctionName == TEXT("AddToViewport")
			|| FunctionName == TEXT("AddToPlayerScreen")
			|| FunctionName == TEXT("RemoveFromParent")
			|| FunctionName.ToString().StartsWith(
				TEXT("SetInputMode"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRespawnScreenCommonUIAssetTest,
	"SurvivalRpg.UI.Respawn.CommonUIComposition",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgRespawnScreenCommonUIAssetTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgRespawnScreenAssetTests;

	const URpgUIScreenRegistry* Registry =
		LoadObject<URpgUIScreenRegistry>(
			nullptr,
			ScreenRegistryPath);
	const UWidgetBlueprint* RespawnBlueprint =
		LoadObject<UWidgetBlueprint>(
			nullptr,
			RespawnScreenBlueprintPath);
	UClass* RespawnScreenClass =
		LoadClass<URpgRespawnScreenWidget>(
			nullptr,
			RespawnScreenClassPath);
	UBlueprint* PlayerControllerBlueprint =
		LoadObject<UBlueprint>(
			nullptr,
			RpgRespawnScreenAssetTests::PlayerControllerBlueprintPath);
	const FGameplayTag RespawnScreenTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.Screen.Respawn"));
	const FGameplayTag ModalLayerTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.Layer.Modal"));
	if (!TestNotNull(
			TEXT("DA_RpgUIScreenRegistry loads"),
			Registry) ||
		!TestNotNull(
			TEXT("CUI_RespawnScreen Blueprint loads"),
			RespawnBlueprint) ||
		!TestNotNull(
			TEXT("CUI_RespawnScreen generated class loads"),
			RespawnScreenClass) ||
		!TestNotNull(
			TEXT("BP_Rpg_PlayerController loads"),
			PlayerControllerBlueprint) ||
		!TestTrue(
			TEXT("UI.Screen.Respawn gameplay tag resolves"),
			RespawnScreenTag.IsValid()) ||
		!TestTrue(
			TEXT("UI.Layer.Modal gameplay tag resolves"),
			ModalLayerTag.IsValid()))
	{
		return false;
	}

	int32 RespawnEntryCount = 0;
	for (const FRpgUIScreenRegistryEntry& Candidate :
		Registry->Screens)
	{
		RespawnEntryCount +=
			Candidate.ScreenTag ==
				RespawnScreenTag
				? 1
				: 0;
	}
	TestEqual(
		TEXT(
			"UI.Screen.Respawn is authored exactly once in the "
			"screen registry"),
		RespawnEntryCount,
		1);

	FRpgUIScreenRegistryEntry RespawnEntry;
	if (!TestTrue(
			TEXT("UI.Screen.Respawn registry entry resolves"),
			Registry->FindScreen(
				RespawnScreenTag,
				RespawnEntry)))
	{
		return false;
	}

	TestTrue(
		TEXT("Respawn opens on UI.Layer.Modal"),
		RespawnEntry.LayerTag ==
			ModalLayerTag);
	TestTrue(
		TEXT(
			"Respawn suspends owning-player input while its "
			"screen class streams"),
		RespawnEntry.bSuspendInputUntilLoaded);
	TestTrue(
		TEXT(
			"Respawn reuses its active CommonUI screen instance"),
		RespawnEntry.bSingleInstance);
	TestEqual(
		TEXT(
			"Respawn registry points at the authored CommonUI "
			"screen"),
		RespawnEntry.WidgetClass
			.ToSoftObjectPath()
			.ToString(),
		FString(RespawnScreenClassPath));
	TestTrue(
		TEXT(
			"Mapped Respawn class derives from the native "
			"Respawn presenter"),
		RespawnScreenClass->IsChildOf(
			URpgRespawnScreenWidget::StaticClass()));
	TestEqual(
		TEXT(
			"Respawn registry resolves to the expected generated "
			"class"),
		RespawnEntry.WidgetClass.LoadSynchronous(),
		RespawnScreenClass);
	TestEqual(
		TEXT(
			"Authored Respawn Blueprint owns the expected "
			"generated class"),
		RespawnBlueprint->GeneratedClass.Get(),
		RespawnScreenClass);

	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(
			RespawnScreenClass);
	const UWidgetTree* WidgetTree =
		GeneratedClass
			? GeneratedClass->GetWidgetTreeArchetype()
			: nullptr;
	if (!TestNotNull(
			TEXT(
				"CUI_RespawnScreen is an authored Widget "
				"Blueprint"),
			GeneratedClass) ||
		!TestNotNull(
			TEXT(
				"CUI_RespawnScreen owns an authored WidgetTree"),
			WidgetTree))
	{
		return false;
	}

	TestFalse(
		TEXT(
			"CUI_RespawnScreen owns no Blueprint graph logic"),
		HasBlueprintGraphLogic(*RespawnBlueprint));

	const UUserWidget* RespawnDefaults =
		Cast<UUserWidget>(
			RespawnScreenClass->GetDefaultObject());
	if (TestNotNull(
			TEXT("CUI_RespawnScreen defaults load"),
			RespawnDefaults))
	{
		TestTrue(
			TEXT("CUI_RespawnScreen never ticks"),
			RespawnDefaults->GetDesiredTickFrequency() ==
				EWidgetTickFrequency::Never);
	}

	const UOverlay* RootOverlay =
		Cast<UOverlay>(
			WidgetTree->FindWidget(TEXT("RootOverlay")));
	const UVerticalBox* RespawnPanel =
		Cast<UVerticalBox>(
			WidgetTree->FindWidget(TEXT("RespawnPanel")));
	const UTextBlock* RespawnTitleText =
		Cast<UTextBlock>(
			WidgetTree->FindWidget(
				TEXT("RespawnTitleText")));
	const UButton* RespawnButton =
		Cast<UButton>(
			WidgetTree->FindWidget(TEXT("RespawnButton")));
	const UTextBlock* RespawnButtonText =
		Cast<UTextBlock>(
			WidgetTree->FindWidget(
				TEXT("RespawnButtonText")));

	TestEqual(
		TEXT("RootOverlay is the authored WidgetTree root"),
		static_cast<const UWidget*>(
			WidgetTree->RootWidget.Get()),
		static_cast<const UWidget*>(RootOverlay));
	TestNotNull(
		TEXT("RootOverlay is an Overlay"),
		RootOverlay);
	TestNotNull(
		TEXT("RespawnPanel is a Vertical Box"),
		RespawnPanel);
	TestNotNull(
		TEXT("RespawnTitleText is a Text Block"),
		RespawnTitleText);
	TestNotNull(
		TEXT("RespawnButton is a Button"),
		RespawnButton);
	TestNotNull(
		TEXT("RespawnButtonText is a Text Block"),
		RespawnButtonText);
	if (RespawnPanel && RespawnTitleText)
	{
		TestEqual(
			TEXT(
				"RespawnTitleText is authored inside "
				"RespawnPanel"),
			static_cast<const UPanelWidget*>(
				RespawnTitleText->GetParent()),
			static_cast<const UPanelWidget*>(RespawnPanel));
	}
	if (RespawnPanel && RespawnButton)
	{
		TestEqual(
			TEXT(
				"RespawnButton is authored inside "
				"RespawnPanel"),
			static_cast<const UPanelWidget*>(
				RespawnButton->GetParent()),
			static_cast<const UPanelWidget*>(RespawnPanel));
	}
	if (RespawnButton && RespawnButtonText)
	{
		TestEqual(
			TEXT(
				"RespawnButtonText is authored inside "
				"RespawnButton"),
			static_cast<const UPanelWidget*>(
				RespawnButtonText->GetParent()),
			static_cast<const UPanelWidget*>(RespawnButton));
	}

	TestFalse(
		TEXT(
			"BP_Rpg_PlayerController no longer declares "
			"DeathWidget"),
		PlayerControllerBlueprint->NewVariables
			.ContainsByPredicate(
				[](const FBPVariableDescription& Variable)
				{
					return Variable.VarName ==
						TEXT("DeathWidget");
				}));
	TestNull(
		TEXT(
			"Compiled BP_Rpg_PlayerController has no stale "
			"DeathWidget property"),
		PlayerControllerBlueprint->GeneratedClass
			? FindFProperty<FProperty>(
				PlayerControllerBlueprint->GeneratedClass,
				TEXT("DeathWidget"))
			: nullptr);

	TArray<UEdGraph*> ControllerGraphs;
	PlayerControllerBlueprint->GetAllGraphs(
		ControllerGraphs);

	int32 RespawnEventNodeCount = 0;
	int32 CreateWidgetNodeCount = 0;
	int32 AddToViewportCallCount = 0;
	int32 AddToPlayerScreenCallCount = 0;
	int32 RemoveFromParentCallCount = 0;
	int32 SetInputModeCallCount = 0;
	for (const UEdGraph* Graph : ControllerGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_Event* EventNode =
				Cast<UK2Node_Event>(Node);
			RespawnEventNodeCount +=
				EventNode &&
					EventNode->GetFunctionName() ==
						TEXT("K2_OnRespawnStateChanged")
					? 1
					: 0;
			CreateWidgetNodeCount +=
				IsCreateWidgetNode(Node)
					? 1
					: 0;

			const UK2Node_CallFunction* CallNode =
				Cast<UK2Node_CallFunction>(Node);
			if (!CallNode)
			{
				continue;
			}

			const FName FunctionName =
				CallNode->GetFunctionName();
			if (!IsForbiddenViewportFunction(FunctionName))
			{
				continue;
			}

			AddToViewportCallCount +=
				FunctionName == TEXT("AddToViewport")
					? 1
					: 0;
			AddToPlayerScreenCallCount +=
				FunctionName == TEXT("AddToPlayerScreen")
					? 1
					: 0;
			RemoveFromParentCallCount +=
				FunctionName == TEXT("RemoveFromParent")
					? 1
					: 0;
			SetInputModeCallCount +=
				FunctionName.ToString().StartsWith(
					TEXT("SetInputMode"))
					? 1
					: 0;
		}
	}

	TestEqual(
		TEXT(
			"BP_Rpg_PlayerController has no "
			"K2_OnRespawnStateChanged event"),
		RespawnEventNodeCount,
		0);
	TestEqual(
		TEXT(
			"BP_Rpg_PlayerController has no CreateWidget node"),
		CreateWidgetNodeCount,
		0);
	TestEqual(
		TEXT(
			"BP_Rpg_PlayerController has no AddToViewport call"),
		AddToViewportCallCount,
		0);
	TestEqual(
		TEXT(
			"BP_Rpg_PlayerController has no AddToPlayerScreen "
			"call"),
		AddToPlayerScreenCallCount,
		0);
	TestEqual(
		TEXT(
			"BP_Rpg_PlayerController has no RemoveFromParent "
			"call"),
		RemoveFromParentCallCount,
		0);
	TestEqual(
		TEXT(
			"BP_Rpg_PlayerController has no manual "
			"SetInputMode call"),
		SetInputModeCallCount,
		0);

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<
			FAssetRegistryModule>(TEXT("AssetRegistry"))
			.Get();
	AssetRegistry.WaitForCompletion();
	TestFalse(
		TEXT(
			"BP_Rpg_PlayerController package no longer depends "
			"on DeathTest"),
		AssetRegistry.ContainsDependency(
			FName(PlayerControllerPackageName),
			FName(LegacyDeathScreenPackageName),
			UE::AssetRegistry::EDependencyCategory::Package));

	FString ExistingLegacyFilename;
	TestFalse(
		TEXT("DeathTest no longer exists on disk"),
		FPackageName::DoesPackageExist(
			LegacyDeathScreenPackageName,
			&ExistingLegacyFilename));

	TArray<FAssetData> LegacyDeathScreenAssets;
	AssetRegistry.GetAssetsByPackageName(
		FName(LegacyDeathScreenPackageName),
		LegacyDeathScreenAssets,
		/*bIncludeOnlyOnDiskAssets=*/ true,
		/*bSkipARFilteredAssets=*/ false);
	TestEqual(
		TEXT(
			"DeathTest has no AssetRegistry entry or redirector"),
		LegacyDeathScreenAssets.Num(),
		0);

	return true;
}

#endif
