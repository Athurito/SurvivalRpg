#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/Combat/RpgCombatReactionProfile.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCombatReactionContractTest,
	"SurvivalRpg.Combat.ReactionProfile.MontagesMatchPawnSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatReactionContractTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();
	Registry.WaitForCompletion();

	FARFilter Filter;
	Filter.ClassPaths.Add(URpgPawnData::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> PawnDataAssets;
	Registry.GetAssets(Filter, PawnDataAssets);

	int32 CheckedMontages = 0;
	for (const FAssetData& Asset : PawnDataAssets)
	{
		// Only project content can opt in; skip engine and sample PawnData without loading them.
		if (!Asset.PackageName.ToString().StartsWith(TEXT("/Game/")) && !Asset.PackageName.ToString().StartsWith(TEXT("/GF_")))
		{
			continue;
		}
		const URpgPawnData* PawnData = Cast<URpgPawnData>(Asset.GetAsset());
		const URpgCombatReactionProfile* Profile = PawnData ? PawnData->CombatReactionProfile.Get() : nullptr;
		if (!Profile)
		{
			continue;
		}

		// Reactions play on GetMesh(), so their montages must target the skeleton of the pawn's gameplay mesh.
		const ACharacter* Character = PawnData->PawnClass ? Cast<ACharacter>(PawnData->PawnClass->GetDefaultObject()) : nullptr;
		const USkeletalMesh* Mesh = Character && Character->GetMesh() ? Character->GetMesh()->GetSkeletalMeshAsset() : nullptr;
		const USkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
		if (!TestNotNull(Asset.GetObjectPathString() + TEXT(" names a character whose gameplay mesh has a skeleton"), Skeleton))
		{
			continue;
		}

		const TPair<const TCHAR*, const FRpgDirectionalReactionMontages*> Reactions[] = {
			{ TEXT("Stagger"), &Profile->Stagger },
			{ TEXT("GuardBreak"), &Profile->GuardBreak },
			{ TEXT("Death"), &Profile->Death },
		};
		for (const TPair<const TCHAR*, const FRpgDirectionalReactionMontages*>& Reaction : Reactions)
		{
			for (const UAnimMontage* Montage : { Reaction.Value->Front.Get(), Reaction.Value->Back.Get() })
			{
				if (!Montage)
				{
					continue;
				}
				++CheckedMontages;
				TestTrue(FString::Printf(TEXT("%s %s montage %s targets %s"), *Profile->GetPathName(), Reaction.Key,
						*Montage->GetPathName(), *Skeleton->GetPathName()),
					Montage->GetSkeleton() == Skeleton);
			}
		}
	}

	AddInfo(FString::Printf(TEXT("Checked %d reaction montages."), CheckedMontages));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
