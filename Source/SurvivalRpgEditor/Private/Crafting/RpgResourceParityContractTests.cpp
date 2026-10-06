#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

namespace RpgResourceParityContractTests
{
	const TCHAR* ParityBenchClassPath =
		TEXT("/GF_Dev_Sandbox/ResourceParity/BP_CraftingStation_ParityBench.BP_CraftingStation_ParityBench_C");
	const TCHAR* KilnRecipeSetPath = TEXT("/GF_Harvesting_Magic/Crafting/DA_RecipeSet_Kiln.DA_RecipeSet_Kiln");

	// The small ingredient pool that every combat style and the build target draw from.
	const TCHAR* PoolMaterialPaths[] = {
		TEXT("/GF_Harvesting_Magic/Items/Materials/ID_Wood.ID_Wood_C"),
		TEXT("/GF_Harvesting_Magic/Items/Materials/ID_Ore.ID_Ore_C"),
		TEXT("/GF_Harvesting_Magic/Items/Materials/ID_Charcoal.ID_Charcoal_C"),
		TEXT("/GF_Harvesting_Magic/Items/Materials/ID_Stone.ID_Stone_C"),
		TEXT("/GF_Harvesting_Magic/Items/Materials/ID_Sticks.ID_Sticks_C"),
	};

	// One weapon per combat style, the ranged style's running supply and the build target.
	const TCHAR* RequiredOutputPaths[] = {
		TEXT("/GF_Combat_Core/Items/Weapons/ID_BasicSword.ID_BasicSword_C"),
		TEXT("/GF_Dev_Sandbox/ResourceParity/Items/ID_Placeholder_Bow.ID_Placeholder_Bow_C"),
		TEXT("/GF_Dev_Sandbox/ResourceParity/Items/ID_Placeholder_Arrow.ID_Placeholder_Arrow_C"),
		TEXT("/GF_Dev_Sandbox/ResourceParity/Items/ID_Placeholder_SpellFocus.ID_Placeholder_SpellFocus_C"),
		TEXT("/GF_Dev_Sandbox/ResourceParity/Items/ID_Placeholder_KilnKit.ID_Placeholder_KilnKit_C"),
	};

	/** Harvest profiles in the project and its content plugins. */
	TArray<const URpgHarvestProfile*> LoadHarvestProfiles()
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.WaitForCompletion();

		FARFilter Filter;
		Filter.ClassPaths.Add(URpgHarvestProfile::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(TEXT("/Game"));
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			if (Plugin->IsMounted() && Plugin->GetType() == EPluginType::Project)
			{
				FString Root = Plugin->GetMountedAssetPath();
				Root.RemoveFromEnd(TEXT("/"));
				Filter.PackagePaths.AddUnique(FName(*Root));
			}
		}

		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		TArray<const URpgHarvestProfile*> Profiles;
		for (const FAssetData& Asset : Assets)
		{
			if (const URpgHarvestProfile* Profile = Cast<URpgHarvestProfile>(Asset.GetAsset()))
			{
				Profiles.Add(Profile);
			}
		}
		return Profiles;
	}

	bool IsHarvested(const TArray<const URpgHarvestProfile*>& Profiles, const UClass* Material)
	{
		for (const URpgHarvestProfile* Profile : Profiles)
		{
			if (!Profile->LootTable)
			{
				continue;
			}
			for (const FRpgLootGroup& Group : Profile->LootTable->Groups)
			{
				for (const FRpgLootEntry& Entry : Group.Entries)
				{
					if (Entry.ItemDefinition == Material)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	bool ProducesFrom(const URpgCraftingRecipeSet* RecipeSet, const UClass* Material, const TSet<const UClass*>& Inputs)
	{
		if (!RecipeSet)
		{
			return false;
		}
		for (const URpgCraftingRecipeDefinition* Recipe : RecipeSet->Recipes)
		{
			if (!Recipe || !Recipe->OutputItems.ContainsByPredicate([Material](const FRpgCraftingOutputItem& Output)
			{
				return Output.ItemDefinition == Material;
			}))
			{
				continue;
			}
			const bool bFromInputs = !Recipe->RequiredResources.ContainsByPredicate([&Inputs](const FRpgCraftingResourceCost& Cost)
			{
				return !Inputs.Contains(Cost.ItemDefinition.Get());
			});
			if (bFromInputs)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgResourceParityPoolContractTest,
	"SurvivalRpg.Harvesting.Content.ResourceParityPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgResourceParityPoolContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgResourceParityContractTests;

	TSet<const UClass*> Pool;
	for (const TCHAR* Path : PoolMaterialPaths)
	{
		const UClass* Material = LoadClass<URpgInventoryItemDefinition>(nullptr, Path);
		if (TestNotNull(*FString::Printf(TEXT("Pool material %s loads"), Path), Material))
		{
			Pool.Add(Material);
		}
	}

	// The parity bench offers its recipes through the station's own offer rules; no count is fixed here.
	const UClass* BenchClass = LoadClass<ARpgCraftingStationActor>(nullptr, ParityBenchClassPath);
	const ARpgCraftingStationActor* Bench = BenchClass ? GetDefault<ARpgCraftingStationActor>(BenchClass) : nullptr;
	const URpgCraftingStationComponent* Station = Bench ? Bench->GetCraftingStationComponent() : nullptr;
	if (!TestNotNull(TEXT("The parity bench has a crafting station"), Station))
	{
		return false;
	}
	const TArray<URpgCraftingRecipeDefinition*> Recipes = Station->GetAvailableRecipes();
	TestTrue(TEXT("The parity bench offers recipes"), Recipes.Num() > 0);

	TSet<const UClass*> Outputs;
	for (const URpgCraftingRecipeDefinition* Recipe : Recipes)
	{
		const FString Name = GetNameSafe(Recipe);
		TestTrue(*FString::Printf(TEXT("%s costs materials"), *Name), Recipe->RequiredResources.Num() > 0);
		for (const FRpgCraftingResourceCost& Cost : Recipe->RequiredResources)
		{
			TestTrue(
				*FString::Printf(TEXT("%s draws %s from the shared pool"), *Name, *GetNameSafe(Cost.ItemDefinition.Get())),
				Pool.Contains(Cost.ItemDefinition.Get()));
		}
		for (const FRpgCraftingOutputItem& Output : Recipe->OutputItems)
		{
			Outputs.Add(Output.ItemDefinition.Get());
		}
	}
	for (const TCHAR* Path : RequiredOutputPaths)
	{
		const UClass* Output = LoadClass<URpgInventoryItemDefinition>(nullptr, Path);
		TestTrue(*FString::Printf(TEXT("The parity bench crafts %s"), Path), Output && Outputs.Contains(Output));
	}

	// Every pool material has a source: a harvest, or a recipe that refines other pool materials.
	const TArray<const URpgHarvestProfile*> Profiles = LoadHarvestProfiles();
	const URpgCraftingRecipeSet* KilnRecipes = LoadObject<URpgCraftingRecipeSet>(nullptr, KilnRecipeSetPath);
	TestNotNull(TEXT("The kiln recipe set loads"), KilnRecipes);
	for (const UClass* Material : Pool)
	{
		TestTrue(
			*FString::Printf(TEXT("%s is harvested or refined from the pool"), *GetNameSafe(Material)),
			IsHarvested(Profiles, Material) || ProducesFrom(KilnRecipes, Material, Pool));
	}
	return true;
}

#endif
