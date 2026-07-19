#include "SurvivalRpg/System/RpgGameData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/AssetManagerTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

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
