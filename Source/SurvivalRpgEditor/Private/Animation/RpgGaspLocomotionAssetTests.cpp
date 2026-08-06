#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/DataTable.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

namespace RpgGaspLocomotionAssetTests
{
	constexpr TCHAR PluginRoot[] = TEXT("/RpgGaspLocomotion");
	constexpr TCHAR AnimationRoot[] = TEXT("/RpgGaspLocomotion/Animations");
	constexpr TCHAR TargetSkeletonPath[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	constexpr TCHAR TargetSkeletonPackage[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin");
	constexpr TCHAR TargetMeshPackage[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SKM_Manny_Simple");
	constexpr TCHAR NormalizationPackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/NormalizationSets/PSN_Rpg_Locomotion");

	bool IsForbiddenDependency(const FString& PackageName)
	{
		const FString LowerPackageName = PackageName.ToLower();
		static const TCHAR* const ForbiddenMarkers[] = {
			TEXT("/game/blueprints/"),
			TEXT("/game/audio/"),
			TEXT("/game/characters/uefn_mannequin/"),
			TEXT("/game/characters/ue5_mannequins/"),
			TEXT("experimentalstatemachine"),
			TEXT("psd_sm_"),
			TEXT("/traversal/"),
			TEXT("/locomotor/"),
			TEXT("/mover/"),
			TEXT("networkprediction"),
			TEXT("metasound"),
		};

		for (const TCHAR* Marker : ForbiddenMarkers)
		{
			if (LowerPackageName.Contains(Marker))
			{
				return true;
			}
		}

		return PackageName.StartsWith(TEXT("/Game/")) &&
			!PackageName.StartsWith(TargetSkeletonPackage) &&
			!PackageName.StartsWith(TargetMeshPackage);
	}

	struct FDatabaseContract
	{
		const TCHAR* PackageName;
		const TCHAR* AnimationPrefix;
		const TCHAR* SchemaPackage;
		int32 ExpectedAnimationCount;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspLocomotionContentContractTest,
	"SurvivalRpg.Animation.Gasp.ContentContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspLocomotionContentContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspLocomotionAssetTests;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RpgGaspLocomotion"));
	if (!TestTrue(TEXT("RpgGaspLocomotion is registered"), Plugin.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("RpgGaspLocomotion is enabled"), Plugin->IsEnabled());
	TestTrue(TEXT("RpgGaspLocomotion can contain content"), Plugin->CanContainContent());
	TestEqual(TEXT("RpgGaspLocomotion has no code modules"), Plugin->GetDescriptor().Modules.Num(), 0);

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous({ FString(PluginRoot) }, true, false);
	AssetRegistry.WaitForCompletion();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(
		FName(PluginRoot),
		Assets,
		true,
		true);
	TestEqual(TEXT("The curated plugin contains exactly 85 assets"), Assets.Num(), 85);

	TMap<FString, int32> ClassCounts;
	int32 AnimationCount = 0;
	for (const FAssetData& AssetData : Assets)
	{
		const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		++ClassCounts.FindOrAdd(ClassName);
		TestNotEqual(
			*FString::Printf(TEXT("%s is not a redirector"), *AssetData.PackageName.ToString()),
			ClassName,
			FString(TEXT("ObjectRedirector")));

		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(
			AssetData.PackageName,
			Dependencies,
			UE::AssetRegistry::EDependencyCategory::Package);
		for (const FName Dependency : Dependencies)
		{
			const FString DependencyString = Dependency.ToString();
			TestFalse(
				*FString::Printf(
					TEXT("%s has no forbidden dependency on %s"),
					*AssetData.PackageName.ToString(),
					*DependencyString),
				IsForbiddenDependency(DependencyString));
		}

		if (ClassName != TEXT("AnimSequence"))
		{
			continue;
		}

		++AnimationCount;
		UAnimSequence* Animation = Cast<UAnimSequence>(AssetData.GetAsset());
		if (!TestNotNull(
			*FString::Printf(TEXT("%s loads as an AnimSequence"), *AssetData.PackageName.ToString()),
			Animation))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s uses the authoritative skeleton"), *Animation->GetName()),
			GetPathNameSafe(Animation->GetSkeleton()),
			FString(TargetSkeletonPath));
		TestTrue(*FString::Printf(TEXT("%s keeps root motion enabled"), *Animation->GetName()), Animation->bEnableRootMotion);
		TestEqual(
			*FString::Printf(TEXT("%s keeps RefPose root locking"), *Animation->GetName()),
			Animation->RootMotionRootLock.GetValue(),
			ERootMotionRootLock::RefPose);
		TestTrue(*FString::Printf(TEXT("%s keeps force-root-lock"), *Animation->GetName()), Animation->bForceRootLock);
		TestTrue(
			*FString::Printf(TEXT("%s keeps normalized root-motion scale"), *Animation->GetName()),
			Animation->bUseNormalizedRootMotionScale);

		const TArray<UAssetUserData*>* AssetUserData = Animation->GetAssetUserDataArray();
		TestTrue(
			*FString::Printf(TEXT("%s has no migrated sample asset user data"), *Animation->GetName()),
			!AssetUserData || AssetUserData->IsEmpty());

		for (const FAnimNotifyEvent& NotifyEvent : Animation->Notifies)
		{
			const UObject* NotifyObject = NotifyEvent.Notify
				? static_cast<const UObject*>(NotifyEvent.Notify.Get())
				: static_cast<const UObject*>(NotifyEvent.NotifyStateClass.Get());
			if (!NotifyObject)
			{
				continue;
			}

			const FString NotifyClassPath = NotifyObject->GetClass()->GetPathName();
			TestFalse(
				*FString::Printf(TEXT("%s has no sample Blueprint notify %s"), *Animation->GetName(), *NotifyClassPath),
				NotifyClassPath.StartsWith(TEXT("/Game/")) ||
					NotifyClassPath.Contains(TEXT("Foley")) ||
					NotifyClassPath.Contains(TEXT("EarlyTransition")) ||
					NotifyClassPath.Contains(TEXT("BranchIn")));
		}
	}

	TestEqual(TEXT("Exactly 75 curated AnimSequences are present"), AnimationCount, 75);
	TestEqual(TEXT("Exactly one ChooserTable is present"), ClassCounts.FindRef(TEXT("ChooserTable")), 1);
	TestEqual(TEXT("Exactly one MirrorDataTable is present"), ClassCounts.FindRef(TEXT("MirrorDataTable")), 1);
	TestEqual(TEXT("Exactly five PoseSearchDatabases are present"), ClassCounts.FindRef(TEXT("PoseSearchDatabase")), 5);
	TestEqual(TEXT("Exactly one PoseSearchNormalizationSet is present"), ClassCounts.FindRef(TEXT("PoseSearchNormalizationSet")), 1);
	TestEqual(TEXT("Exactly two PoseSearchSchemas are present"), ClassCounts.FindRef(TEXT("PoseSearchSchema")), 2);

	static const FDatabaseContract DatabaseContracts[] = {
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"), TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 5 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk"), TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 29 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 20 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint"), TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 10 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump"), TEXT("/RpgGaspLocomotion/Animations/Jump/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Jump"), 11 },
	};

	for (const FDatabaseContract& Contract : DatabaseContracts)
	{
		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(
			FName(Contract.PackageName),
			Dependencies,
			UE::AssetRegistry::EDependencyCategory::Package);
		int32 AnimationDependencyCount = 0;
		for (const FName Dependency : Dependencies)
		{
			if (Dependency.ToString().StartsWith(Contract.AnimationPrefix))
			{
				++AnimationDependencyCount;
			}
		}
		TestEqual(
			*FString::Printf(TEXT("%s owns the expected animation group"), Contract.PackageName),
			AnimationDependencyCount,
			Contract.ExpectedAnimationCount);
		TestTrue(
			*FString::Printf(TEXT("%s references its local schema"), Contract.PackageName),
			Dependencies.Contains(FName(Contract.SchemaPackage)));
		TestTrue(
			*FString::Printf(TEXT("%s references the shared normalization set"), Contract.PackageName),
			Dependencies.Contains(FName(NormalizationPackage)));
	}

	static const TCHAR* const DatabasePackages[] = {
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump"),
	};
	constexpr TCHAR ChooserPackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Choosers/CHT_Rpg_LocomotionDatabases");
	for (const TCHAR* DatabasePackage : DatabasePackages)
	{
		TestTrue(
			*FString::Printf(TEXT("The chooser references %s"), DatabasePackage),
			AssetRegistry.ContainsDependency(
				FName(ChooserPackage),
				FName(DatabasePackage),
				UE::AssetRegistry::EDependencyCategory::Package));
		TestTrue(
			*FString::Printf(TEXT("The normalization set references %s"), DatabasePackage),
			AssetRegistry.ContainsDependency(
				FName(NormalizationPackage),
				FName(DatabasePackage),
				UE::AssetRegistry::EDependencyCategory::Package));
	}

	UDataTable* MirrorTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/MirrorTables/MDT_Rpg_Mannequin.MDT_Rpg_Mannequin"));
	if (TestNotNull(TEXT("The project-local mirror table loads"), MirrorTable))
	{
		TestEqual(TEXT("The mirror table uses FMirrorTableRow"), GetNameSafe(MirrorTable->GetRowStruct()), FString(TEXT("MirrorTableRow")));
		TestTrue(TEXT("The mirror table contains generated pairs"), MirrorTable->GetRowMap().Num() > 0);
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
