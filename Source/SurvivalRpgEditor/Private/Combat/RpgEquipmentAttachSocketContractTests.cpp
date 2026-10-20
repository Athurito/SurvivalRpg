#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

namespace RpgEquipmentAttachSocketContractTests
{
	struct FGameplaySkeleton
	{
		const TCHAR* Description;
		const TCHAR* ObjectPath;
	};

	// Skeletons of the gameplay meshes that equipment actors attach to: Lyra Manny for the CMC pawns
	// and Manny-native enemies, UEFN for the Mover pawns. Equipment definitions stay skeleton-free,
	// so every attach name they author must resolve on both.
	constexpr FGameplaySkeleton GameplaySkeletons[] = {
		{
			TEXT("Lyra Manny skeleton"),
			TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"),
		},
		{
			TEXT("UEFN skeleton"),
			TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/"
				"SK_UEFN_Mannequin.SK_UEFN_Mannequin"),
		},
	};

	TArray<FAssetData> FindProjectBlueprints(IAssetRegistry& Registry)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
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

	bool ResolvesOnSkeleton(const USkeleton& Skeleton, FName AttachName)
	{
		// Attachment accepts a skeleton socket or a bone of the same name.
		return Skeleton.FindSocket(AttachName) != nullptr
			|| Skeleton.GetReferenceSkeleton().FindBoneIndex(AttachName) != INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgEquipmentAttachSocketContractTest,
	"SurvivalRpg.Combat.EquipmentAttachSocketContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentAttachSocketContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAttachSocketContractTests;

	TArray<const USkeleton*, TInlineAllocator<2>> Skeletons;
	for (const FGameplaySkeleton& Entry : GameplaySkeletons)
	{
		const USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, Entry.ObjectPath);
		if (!TestNotNull(FString::Printf(TEXT("The %s loads"), Entry.Description), Skeleton))
		{
			return false;
		}
		Skeletons.Add(Skeleton);
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();
	Registry.WaitForCompletion();
	TSet<FTopLevelAssetPath> DefinitionClassPaths;
	Registry.GetDerivedClassNames({ URpgEquipmentDefinition::StaticClass()->GetClassPathName() }, {}, DefinitionClassPaths);

	int32 CheckedAttachNames = 0;
	for (const FAssetData& Asset : FindProjectBlueprints(Registry))
	{
		// Inspect the generated class tag first so unrelated Blueprints stay unloaded.
		const FTopLevelAssetPath ClassPath(FPackageName::ExportTextPathToObjectPath(
			Asset.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath)));
		if (!DefinitionClassPaths.Contains(ClassPath))
		{
			continue;
		}

		const UClass* DefinitionClass = LoadObject<UClass>(nullptr, *ClassPath.ToString());
		const URpgEquipmentDefinition* Definition = DefinitionClass
			? Cast<URpgEquipmentDefinition>(DefinitionClass->GetDefaultObject())
			: nullptr;
		if (!TestNotNull(Asset.GetObjectPathString() + TEXT(" loads its equipment definition defaults"), Definition))
		{
			continue;
		}

		for (int32 Index = 0; Index < Definition->ActorsToSpawn.Num(); ++Index)
		{
			const FRpgEquipmentActorToSpawn& Spawn = Definition->ActorsToSpawn[Index];
			for (const FName AttachName : { Spawn.AttachSocket, Spawn.MainHandAttachSocket, Spawn.OffHandAttachSocket })
			{
				if (AttachName.IsNone())
				{
					continue;
				}
				++CheckedAttachNames;
				for (int32 SkeletonIndex = 0; SkeletonIndex < Skeletons.Num(); ++SkeletonIndex)
				{
					TestTrue(FString::Printf(TEXT("%s actor %d attach name %s resolves on the %s"),
							*Asset.GetObjectPathString(), Index, *AttachName.ToString(),
							GameplaySkeletons[SkeletonIndex].Description),
						ResolvesOnSkeleton(*Skeletons[SkeletonIndex], AttachName));
				}
			}
		}
	}

	TestTrue(TEXT("The registry finds equipment definitions with attach names"), CheckedAttachNames > 0);
	AddInfo(FString::Printf(TEXT("Checked %d authored attach names on %d gameplay skeletons."),
		CheckedAttachNames, Skeletons.Num()));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
