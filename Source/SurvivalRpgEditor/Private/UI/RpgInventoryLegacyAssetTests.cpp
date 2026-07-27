#include "SurvivalRpg/System/RpgGameData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/UI/RpgInventorySpatialItemWidget.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/Blueprint.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryLegacyAssetRetirementTest,
	"SurvivalRpg.Inventory.UI.LegacyAssetRetirement",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryLegacyAssetRetirementTest::RunTest(
	const FString& Parameters)
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry"))
			.Get();
	AssetRegistry.WaitForCompletion();

	static const TCHAR* const RetiredPackages[] = {
		TEXT("/Game/SurvivalRpg/Inventory/UI/BP_DragOperation"),
		TEXT("/Game/SurvivalRpg/Inventory/UI/Hotbar/CUI_Hotbar_Old"),
		TEXT("/Game/SurvivalRpg/UI/CUI_ActionBar_Old"),
		TEXT("/Game/SurvivalRpg/UI/CUI_Storage"),
		TEXT("/Game/SurvivalRpg/UI/CUI_StorageContainer"),
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseTerminal"),
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseResourceList"),
		TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftingStation"),
		TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftingIngredientEntry"),
		TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftingJobEntry"),
		TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftingRecipeEntry"),
		TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftButtonBase"),
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_Inventory"),
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_InventorySlotEntry"),
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_AddressSlotEntry"),
		TEXT("/Game/SurvivalRpg/Equipment/Definitions/Test/EQ_TestShield"),
		TEXT("/Game/SurvivalRpg/Crafting/BP_TestDrop"),
		TEXT("/Game/SurvivalRpg/Inventory/Items/Test/ID_Cheese"),
		TEXT("/Game/SurvivalRpg/Inventory/Items/Test/ID_TestSword1"),
	};

	for (const TCHAR* PackageName : RetiredPackages)
	{
		FString ExistingFilename;
		TestFalse(
			*FString::Printf(
				TEXT("%s no longer exists on disk"),
				PackageName),
			FPackageName::DoesPackageExist(
				PackageName,
				&ExistingFilename));

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(
			FName(PackageName),
			Assets,
			true,
			false);
		TestEqual(
			*FString::Printf(
				TEXT("%s has no AssetRegistry entry or redirector"),
				PackageName),
			Assets.Num(),
			0);

		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(
			FName(PackageName),
			Referencers,
			UE::AssetRegistry::EDependencyCategory::Package);
		TestEqual(
			*FString::Printf(
				TEXT("%s has no serialized package referencers"),
				PackageName),
			Referencers.Num(),
			0);
	}

	struct FDependencyContract
	{
		const TCHAR* Owner;
		const TCHAR* Dependency;
	};

	static const FDependencyContract ActiveDependencies[] = {
		{
			TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry"),
			TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_PlayerInventory"),
		},
		{
			TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry"),
			TEXT("/Game/SurvivalRpg/UI/CUI_StorageSpatial"),
		},
		{
			TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry"),
			TEXT("/Game/SurvivalRpg/UI/CUI_BaseTerminalSpatial"),
		},
		{
			TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry"),
			TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftingStationSpatial"),
		},
		{
			TEXT("/Game/SurvivalRpg/Inventory/UI/SpatialInventory/CUI_CarrySlot"),
			TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_InventorySlotButtonStyle"),
		},
		{
			TEXT("/Game/SurvivalRpg/Inventory/UI/SpatialInventory/CUI_CarrySlot"),
			TEXT("/Game/SurvivalRpg/Inventory/UI/Inventory_Slot_Background"),
		},
		{
			TEXT("/Game/SurvivalRpg/Inventory/UI/SpatialInventory/CUI_SpatialInventoryGrid"),
			TEXT("/Game/SurvivalRpg/Inventory/UI/SpatialInventory/CUI_SpatialInventoryItem"),
		},
		{
			TEXT("/Game/SurvivalRpg/UI/CUI_BaseResourceListSpatial"),
			TEXT("/Game/SurvivalRpg/UI/CUI_BaseResourceEntry"),
		},
		{
			TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftingStationSpatial"),
			TEXT("/Game/SurvivalRpg/UI/CUI_ActionbarButton"),
		},
	};

	for (const FDependencyContract& Contract : ActiveDependencies)
	{
		TestTrue(
			*FString::Printf(
				TEXT("%s retains dependency on %s"),
				Contract.Owner,
				Contract.Dependency),
			AssetRegistry.ContainsDependency(
				FName(Contract.Owner),
				FName(Contract.Dependency),
				UE::AssetRegistry::EDependencyCategory::Package));
	}

	constexpr TCHAR SpatialItemObjectPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryItem.CUI_SpatialInventoryItem");
	UWidgetBlueprint* SpatialItemBlueprint =
		LoadObject<UWidgetBlueprint>(
			nullptr,
			SpatialItemObjectPath);
	if (!TestNotNull(
			TEXT("The active spatial-item Widget Blueprint loads"),
			SpatialItemBlueprint))
	{
		return false;
	}

	TestEqual(
		TEXT("The spatial-item asset keeps its exact native presenter"),
		SpatialItemBlueprint->ParentClass.Get(),
		URpgInventorySpatialItemWidget::StaticClass());
	TestTrue(
		TEXT("The cleaned spatial-item Blueprint is compiled"),
		SpatialItemBlueprint->Status == BS_UpToDate ||
			SpatialItemBlueprint->Status == BS_UpToDateWithWarnings);

	static const TSet<FName> RetiredSpatialItemMembers = {
		TEXT("InventoryAddressViewModel"),
		TEXT("InventoryEntryViewModel"),
	};
	for (const FBPVariableDescription& Variable :
		SpatialItemBlueprint->NewVariables)
	{
		TestFalse(
			*FString::Printf(
				TEXT("Spatial item no longer authors member %s"),
				*Variable.VarName.ToString()),
			RetiredSpatialItemMembers.Contains(Variable.VarName));
	}
	TestEqual(
		TEXT("Spatial item owns no Blueprint member-variable data path"),
		SpatialItemBlueprint->NewVariables.Num(),
		0);

	static const TSet<FName> RetiredSpatialItemImplementations = {
		TEXT("BP_OnSpatialAddressItemSet"),
		TEXT("BP_OnSpatialEntryItemSet"),
		TEXT("BP_OnSpatialItemDragDropStateChanged"),
	};
	TArray<UEdGraph*> AuthoredGraphs;
	SpatialItemBlueprint->GetAllGraphs(AuthoredGraphs);
	TestEqual(
		TEXT("Spatial item owns exactly one authored graph"),
		AuthoredGraphs.Num(),
		1);
	for (const UEdGraph* Graph : AuthoredGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TestFalse(
			*FString::Printf(
				TEXT("Spatial item no longer owns implementation graph %s"),
				*Graph->GetName()),
			RetiredSpatialItemImplementations.Contains(
				Graph->GetFName()));

		TestEqual(
			TEXT("Spatial item retains only its EventGraph"),
			Graph->GetFName(),
			FName(TEXT("EventGraph")));
		TestEqual(
			TEXT("Spatial item EventGraph retains exactly three audited nodes"),
			Graph->Nodes.Num(),
			3);

		int32 EventNodeCount = 0;
		int32 VariableGetNodeCount = 0;
		int32 UnexpectedNodeCount = 0;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_Event* EventNode =
				Cast<UK2Node_Event>(Node);
			if (EventNode)
			{
				++EventNodeCount;
				TestFalse(
					*FString::Printf(
						TEXT(
							"Spatial item graph %s no longer implements event %s"),
						*Graph->GetName(),
						*EventNode->GetFunctionName().ToString()),
					RetiredSpatialItemImplementations.Contains(
						EventNode->GetFunctionName()));
			}
			else if (Cast<UK2Node_VariableGet>(Node))
			{
				++VariableGetNodeCount;
			}
			else
			{
				++UnexpectedNodeCount;
			}
		}
		TestEqual(
			TEXT("Spatial item EventGraph retains three event nodes"),
			EventNodeCount,
			3);
		TestEqual(
			TEXT("Spatial item EventGraph retains no variable-get nodes"),
			VariableGetNodeCount,
			0);
		TestEqual(
			TEXT("Spatial item EventGraph has no other node classes"),
			UnexpectedNodeCount,
			0);
	}

	UClass* SpatialItemGeneratedClass =
		SpatialItemBlueprint->GeneratedClass;
	if (!TestNotNull(
			TEXT("The active spatial item has a generated class"),
			SpatialItemGeneratedClass))
	{
		return false;
	}
	for (TFieldIterator<FProperty> PropertyIt(
			SpatialItemGeneratedClass,
			EFieldIteratorFlags::ExcludeSuper);
		PropertyIt;
		++PropertyIt)
	{
		TestFalse(
			*FString::Printf(
				TEXT("Generated spatial item no longer owns property %s"),
				*PropertyIt->GetName()),
			RetiredSpatialItemMembers.Contains(
				PropertyIt->GetFName()));
	}

	for (const FName FunctionName : RetiredSpatialItemImplementations)
	{
		const UFunction* NativeFunction =
			URpgInventorySpatialItemWidget::StaticClass()->
				FindFunctionByName(FunctionName);
		const UFunction* ResolvedFunction =
			SpatialItemGeneratedClass->FindFunctionByName(FunctionName);
		if (TestNotNull(
				*FString::Printf(
					TEXT("Native spatial-item API %s remains available"),
					*FunctionName.ToString()),
				NativeFunction) &&
			TestNotNull(
				*FString::Printf(
					TEXT("Generated spatial item resolves native API %s"),
					*FunctionName.ToString()),
				ResolvedFunction))
		{
			TestTrue(
				*FString::Printf(
					TEXT("%s remains a Blueprint event seam"),
					*FunctionName.ToString()),
				NativeFunction->HasAnyFunctionFlags(
					FUNC_BlueprintEvent));
			TestEqual(
				*FString::Printf(
					TEXT("%s is inherited instead of graph-overridden"),
					*FunctionName.ToString()),
				ResolvedFunction->GetOwnerClass(),
				URpgInventorySpatialItemWidget::StaticClass());
		}
	}

	constexpr TCHAR GameDataPackage[] =
		TEXT("/Game/SurvivalRpg/System/DA_RpgGameData");
	constexpr TCHAR GameDataObjectPath[] =
		TEXT(
			"/Game/SurvivalRpg/System/"
			"DA_RpgGameData.DA_RpgGameData");
	constexpr TCHAR OldGameDataObjectPath[] =
		TEXT("/Game/System/DA_RpgGameData.DA_RpgGameData");
	const FSoftObjectPath ExpectedGameDataPath(GameDataObjectPath);

	const UAssetManagerSettings* Settings =
		GetDefault<UAssetManagerSettings>();
	const FPrimaryAssetTypeInfo* GameDataTypeInfo = nullptr;
	int32 MatchingTypeCount = 0;
	for (const FPrimaryAssetTypeInfo& TypeInfo :
		Settings->PrimaryAssetTypesToScan)
	{
		if (TypeInfo.PrimaryAssetType ==
			FName(TEXT("RpgGameData")))
		{
			++MatchingTypeCount;
			GameDataTypeInfo = &TypeInfo;
		}
	}

	TestEqual(
		TEXT("RpgGameData has exactly one AssetManager scan rule"),
		MatchingTypeCount,
		1);
	if (GameDataTypeInfo)
	{
		TestEqual(
			TEXT("RpgGameData scans exactly one explicit asset"),
			GameDataTypeInfo->GetSpecificAssets().Num(),
			1);
		TestTrue(
			TEXT("RpgGameData scans the corrected object path"),
			GameDataTypeInfo->GetSpecificAssets().Contains(
				ExpectedGameDataPath));
		TestFalse(
			TEXT("RpgGameData no longer scans the old object path"),
			GameDataTypeInfo->GetSpecificAssets().Contains(
				FSoftObjectPath(OldGameDataObjectPath)));
		TestEqual(
			TEXT("RpgGameData has no competing directory scan"),
			GameDataTypeInfo->GetDirectories().Num(),
			0);
		TestFalse(
			TEXT("RpgGameData assets are concrete UObjects"),
			GameDataTypeInfo->bHasBlueprintClasses);
		TestFalse(
			TEXT("RpgGameData is available at runtime"),
			GameDataTypeInfo->bIsEditorOnly);
		TestTrue(
			TEXT("RpgGameData is always cooked"),
			GameDataTypeInfo->Rules.CookRule ==
				EPrimaryAssetCookRule::AlwaysCook);
		TestEqual(
			TEXT("RpgGameData scans the exact native base class"),
			GameDataTypeInfo->GetAssetBaseClass()
				.ToSoftObjectPath()
				.ToString(),
			FString(TEXT("/Script/SurvivalRpg.RpgGameData")));
	}

	const URpgGameData* GameData =
		LoadObject<URpgGameData>(
			nullptr,
			GameDataObjectPath);
	TestNotNull(
		TEXT("DA_RpgGameData loads from the corrected path"),
		GameData);

	UAssetManager& AssetManager = UAssetManager::Get();
	const FPrimaryAssetId GameDataId =
		AssetManager.GetPrimaryAssetIdForPath(
			ExpectedGameDataPath);
	TestTrue(
		TEXT("DA_RpgGameData is registered as a primary asset"),
		GameDataId.IsValid());
	TestTrue(
		TEXT("DA_RpgGameData uses the RpgGameData type"),
		GameDataId.PrimaryAssetType ==
			FPrimaryAssetType(TEXT("RpgGameData")));
	TestTrue(
		TEXT("AssetManager resolves the corrected object path"),
		AssetManager.GetPrimaryAssetPath(GameDataId) ==
			ExpectedGameDataPath);
	TestTrue(
		TEXT("Effective GameData rule remains AlwaysCook"),
		AssetManager.GetPrimaryAssetRules(GameDataId).CookRule ==
			EPrimaryAssetCookRule::AlwaysCook);
	TestFalse(
		TEXT("The old GameData package does not exist"),
		FPackageName::DoesPackageExist(
			TEXT("/Game/System/DA_RpgGameData")));

	return true;
}

#endif
