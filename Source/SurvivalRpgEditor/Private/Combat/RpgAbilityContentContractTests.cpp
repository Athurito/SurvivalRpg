#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/AssetManager.h"
#include "Engine/Blueprint.h"
#include "GameFeatureData.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddAbilities.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/System/RpgGameData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace RpgAbilityContentContractTests
{
	// These are reusable native mechanisms, not a manifest of concrete designer abilities.
	constexpr const TCHAR* NativeMechanisms[] = {
		TEXT("ApplyItemEffects"), TEXT("BasicWeaponAttack"), TEXT("Block"),
		TEXT("Death"), TEXT("Dodge"), TEXT("FromEquipment"), TEXT("Revive"),
		TEXT("SelfRevive"), TEXT("Stagger"), TEXT("Collect"),
		TEXT("ExecuteInteraction"), TEXT("OpenBaseStorageStation"),
		TEXT("OpenCraftingStation"), TEXT("OpenStorageContainer"),
	};

	UClass* FindNativeMechanism(const TCHAR* Name)
	{
		return FindObject<UClass>(nullptr,
			*FString::Printf(TEXT("/Script/SurvivalRpg.RpgGameplayAbility_%s"), Name));
	}

	IAssetRegistry& GetRegistry()
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
		Registry.WaitForCompletion();
		return Registry;
	}

	TArray<FAssetData> FindProjectAssets(IAssetRegistry& Registry, const UClass* AssetClass)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(AssetClass->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;
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
		Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(Right.PackageName);
		});
		return Assets;
	}

	void ValidateAbilityClass(FAutomationTestBase& Test, UClass* AbilityClass,
		const FString& Context, bool bRequireBlueprint = false)
	{
		if (!Test.TestNotNull(Context + TEXT(" resolves an ability class"), AbilityClass))
		{
			return;
		}
		Test.TestTrue(Context + TEXT(" derives from the project gameplay ability foundation"),
			AbilityClass->IsChildOf(URpgGameplayAbility::StaticClass()));
		Test.TestFalse(Context + TEXT(" grants an instantiable current class"),
			AbilityClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists));

		for (const TCHAR* Name : NativeMechanisms)
		{
			if (UClass* Mechanism = FindNativeMechanism(Name))
			{
				bRequireBlueprint |= AbilityClass->IsChildOf(Mechanism);
			}
		}
		const UBlueprint* Blueprint = Cast<UBlueprint>(AbilityClass->ClassGeneratedBy);
		if (bRequireBlueprint)
		{
			Test.TestNotNull(Context + TEXT(" selects designer-owned Blueprint content"), Blueprint);
		}
		if (Blueprint)
		{
			Test.TestTrue(Context + TEXT(" has a valid project ability parent"),
				Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(URpgGameplayAbility::StaticClass()));
			Test.TestTrue(Context + TEXT(" uses its Blueprint's current generated class"),
				Blueprint->GeneratedClass == AbilityClass);
			Test.TestTrue(Context + TEXT(" is compiled successfully"),
				Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
			Test.TestTrue(Context + TEXT(" has a saved content package"),
				FPackageName::DoesPackageExist(Blueprint->GetOutermost()->GetName()));
		}
	}

	TSet<FTopLevelAssetPath> GetDerivedClassPaths(IAssetRegistry& Registry, const UClass* BaseClass)
	{
		TSet<FTopLevelAssetPath> Paths;
		Registry.GetDerivedClassNames({ BaseClass->GetClassPathName() }, {}, Paths);
		return Paths;
	}

	FTopLevelAssetPath GetBlueprintClassPath(const FAssetData& Asset)
	{
		return FTopLevelAssetPath(FPackageName::ExportTextPathToObjectPath(
			Asset.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgAbilityContentNativeBoundaryTest,
	"SurvivalRpg.Combat.AbilityContent.NativeBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilityContentNativeBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityContentContractTests;
	for (const TCHAR* Name : NativeMechanisms)
	{
		const UClass* Mechanism = FindNativeMechanism(Name);
		if (TestNotNull(FString::Printf(TEXT("Native mechanism %s exists"), Name), Mechanism))
		{
			TestTrue(FString::Printf(TEXT("Native mechanism %s is abstract"), Name),
				Mechanism->HasAnyClassFlags(CLASS_Abstract));
		}
	}

	const FString RetiredHitReactionPath(TEXT("/Script/SurvivalRpg.RpgGameplayAbility_HitReaction"));
	TestNull(TEXT("Hit reaction has no content-only native leaf"),
		FindObject<UClass>(nullptr, *RetiredHitReactionPath));

	IAssetRegistry& Registry = GetRegistry();
	const TSet<FTopLevelAssetPath> AbilityPaths = GetDerivedClassPaths(Registry, URpgGameplayAbility::StaticClass());
	int32 AbilityCount = 0;
	for (const FAssetData& Asset : FindProjectAssets(Registry, UBlueprint::StaticClass()))
	{
		// Inspect tags before loading: unrelated actors, animations and GASP assets stay unloaded.
		for (const FName ParentTag : { FBlueprintTags::ParentClassPath, FBlueprintTags::NativeParentClassPath })
		{
			TestFalse(Asset.GetObjectPathString() + TEXT(" has no retired hit-reaction parent reference"),
				FPackageName::ExportTextPathToObjectPath(Asset.GetTagValueRef<FString>(ParentTag)) == RetiredHitReactionPath);
		}
		const FTopLevelAssetPath ClassPath = GetBlueprintClassPath(Asset);
		if (!AbilityPaths.Contains(ClassPath))
		{
			continue;
		}
		UClass* AbilityClass = LoadObject<UClass>(nullptr, *ClassPath.ToString());
		if (AbilityClass && AbilityClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue; // Abstract Blueprint family bases are valid content composition seams.
		}
		ValidateAbilityClass(*this, AbilityClass, Asset.GetObjectPathString(), true);
		++AbilityCount;
	}
	TestTrue(TEXT("The registry finds concrete project ability content"), AbilityCount > 0);

	UClass* HitReaction = LoadClass<URpgGameplayAbility>(nullptr,
		TEXT("/GF_Combat_Core/GAS/Abilities/GA_Combat_HitReaction.GA_Combat_HitReaction_C"));
	ValidateAbilityClass(*this, HitReaction, TEXT("Combat hit-reaction content"), true);
	if (HitReaction)
	{
		TestTrue(TEXT("Hit-reaction flow is owned directly by Blueprint content"),
			HitReaction->GetSuperClass() == URpgGameplayAbility::StaticClass());
	}
	AddInfo(FString::Printf(TEXT("Validated %d concrete project ability Blueprints."), AbilityCount));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgAbilityContentGrantTest,
	"SurvivalRpg.Combat.AbilityContent.Grants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilityContentGrantTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityContentContractTests;
	IAssetRegistry& Registry = GetRegistry();
	const FArrayProperty* GrantsProperty = FindFProperty<FArrayProperty>(
		URpgAbilitySet::StaticClass(), TEXT("GrantedGameplayAbilities"));
	const FStructProperty* GrantStruct = GrantsProperty ? CastField<FStructProperty>(GrantsProperty->Inner) : nullptr;
	if (!TestTrue(TEXT("Ability sets expose the reusable native grant schema"),
		GrantStruct && GrantStruct->Struct == FRpgAbilitySet_GameplayAbility::StaticStruct()))
	{
		return false;
	}

	int32 GrantCount = 0;
	const TArray<FAssetData> AbilitySets = FindProjectAssets(Registry, URpgAbilitySet::StaticClass());
	TestTrue(TEXT("The registry finds project ability sets"), !AbilitySets.IsEmpty());
	for (const FAssetData& Asset : AbilitySets)
	{
		const URpgAbilitySet* AbilitySet = Cast<URpgAbilitySet>(Asset.GetAsset());
		if (!TestNotNull(Asset.GetObjectPathString() + TEXT(" loads"), AbilitySet))
		{
			continue;
		}
		const TArray<FRpgAbilitySet_GameplayAbility>& Grants =
			*GrantsProperty->ContainerPtrToValuePtr<TArray<FRpgAbilitySet_GameplayAbility>>(AbilitySet);
		for (int32 Index = 0; Index < Grants.Num(); ++Index)
		{
			ValidateAbilityClass(*this, Grants[Index].Ability.Get(),
				FString::Printf(TEXT("%s grant %d"), *Asset.GetObjectPathString(), Index));
			++GrantCount;
		}
	}

	for (const FAssetData& Asset : FindProjectAssets(Registry, UGameFeatureData::StaticClass()))
	{
		const UGameFeatureData* Feature = Cast<UGameFeatureData>(Asset.GetAsset());
		if (!TestNotNull(Asset.GetObjectPathString() + TEXT(" loads"), Feature))
		{
			continue;
		}
		for (const UGameFeatureAction* Action : Feature->GetActions())
		{
			const URpgGameFeatureAction_AddAbilities* Grants = Cast<URpgGameFeatureAction_AddAbilities>(Action);
			if (!Grants)
			{
				continue;
			}
			for (const FRpgGameFeatureAbilitiesEntry& Entry : Grants->AbilitiesList)
			{
				for (const FRpgGameFeatureAbilityGrant& Grant : Entry.GrantedAbilities)
				{
					ValidateAbilityClass(*this, Grant.AbilityType.LoadSynchronous(), Asset.GetObjectPathString());
					++GrantCount;
				}
			}
		}
	}

	const TSet<FTopLevelAssetPath> ItemPaths = GetDerivedClassPaths(Registry, URpgInventoryItemDefinition::StaticClass());
	for (const FAssetData& Asset : FindProjectAssets(Registry, UBlueprint::StaticClass()))
	{
		const FTopLevelAssetPath ClassPath = GetBlueprintClassPath(Asset);
		if (!ItemPaths.Contains(ClassPath))
		{
			continue;
		}
		UClass* ItemClass = LoadObject<UClass>(nullptr, *ClassPath.ToString());
		if (!TestNotNull(Asset.GetObjectPathString() + TEXT(" generated class loads"), ItemClass)
			|| ItemClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		const URpgInventoryItemDefinition* Item = ItemClass->GetDefaultObject<URpgInventoryItemDefinition>();
		for (const URpgInventoryItemFragment* Fragment : Item->Fragments)
		{
			const URpgInventoryFragment_UsableItem* Usable = Cast<URpgInventoryFragment_UsableItem>(Fragment);
			if (Usable && Usable->UseAbility)
			{
				ValidateAbilityClass(*this, Usable->UseAbility.Get(), Asset.GetObjectPathString() + TEXT(" item use"));
				++GrantCount;
			}
		}
	}
	TestTrue(TEXT("Project content supplies concrete ability grants"), GrantCount > 0);
	AddInfo(FString::Printf(TEXT("Validated %d ability grants across %d ability sets, GameFeatures and item definitions."),
		GrantCount, AbilitySets.Num()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgAbilityContentInteractionDefaultsTest,
	"SurvivalRpg.Combat.AbilityContent.InteractionDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilityContentInteractionDefaultsTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityContentContractTests;
	IAssetRegistry& Registry = GetRegistry();
	const URpgGameData& GameData = URpgGameData::Get();
	const FPrimaryAssetRules CookRules = UAssetManager::Get().GetPrimaryAssetRules(GameData.GetPrimaryAssetId());
	TestTrue(TEXT("Project GameData is always cooked with recursive dependencies"),
		CookRules.CookRule == EPrimaryAssetCookRule::AlwaysCook && CookRules.bApplyRecursively);

	struct FInteractionDefault
	{
		const TCHAR* Field;
		const TCHAR* Mechanism;
	};
	constexpr FInteractionDefault Defaults[] = {
		{ TEXT("CollectInteractionAbility"), TEXT("Collect") },
		{ TEXT("ExecuteInteractionAbility"), TEXT("ExecuteInteraction") },
		{ TEXT("OpenStorageInteractionAbility"), TEXT("OpenStorageContainer") },
		{ TEXT("OpenCraftingInteractionAbility"), TEXT("OpenCraftingStation") },
		{ TEXT("OpenBaseStorageInteractionAbility"), TEXT("OpenBaseStorageStation") },
		{ TEXT("ReviveInteractionAbility"), TEXT("Revive") },
	};
	for (const FInteractionDefault& Default : Defaults)
	{
		const FString Context = FString::Printf(TEXT("GameData.%s"), Default.Field);
		const FSoftClassProperty* Property = FindFProperty<FSoftClassProperty>(GameData.GetClass(), Default.Field);
		if (!TestNotNull(Context + TEXT(" exposes a soft ability class reference"), Property))
		{
			continue;
		}
		const FSoftObjectPtr& Reference = *Property->ContainerPtrToValuePtr<FSoftObjectPtr>(&GameData);
		UClass* AbilityClass = Cast<UClass>(Reference.LoadSynchronous());
		ValidateAbilityClass(*this, AbilityClass, Context, true);
		if (AbilityClass)
		{
			const UClass* Mechanism = FindNativeMechanism(Default.Mechanism);
			TestTrue(Context + TEXT(" uses its intended native interaction mechanism"),
				Mechanism && AbilityClass->IsChildOf(Mechanism));
			TestTrue(Context + TEXT(" is a saved package dependency of cooked GameData"),
				Registry.ContainsDependency(GameData.GetOutermost()->GetFName(),
					AbilityClass->GetOutermost()->GetFName(), UE::AssetRegistry::EDependencyCategory::Package));
		}
	}
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
