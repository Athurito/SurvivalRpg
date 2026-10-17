// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetProfile.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"

namespace RpgRuntimeRetargetTests
{
	constexpr TCHAR MoverPawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/DA_PawnData_GaspMover.DA_PawnData_GaspMover");
	constexpr TCHAR CmcPawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/DA_PawnData_GaspCMC.DA_PawnData_GaspCMC");
	constexpr TCHAR MantlePawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/DA_PawnData_GaspMantle.DA_PawnData_GaspMantle");
	constexpr TCHAR MannyMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Manny/Meshes/SKM_Manny_Simple_GaspLook.SKM_Manny_Simple_GaspLook");

	/** True when the class or a Blueprint parent adds a runtime retarget follower component. */
	bool AddsRetargetComponent(const UClass* Class)
	{
		for (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class); Generated;
			Generated = Cast<UBlueprintGeneratedClass>(Generated->GetSuperClass()))
		{
			const USimpleConstructionScript* Script = Generated->SimpleConstructionScript;
			for (const USCS_Node* Node : Script ? Script->GetAllNodes() : TArray<USCS_Node*>())
			{
				if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf<URpgRuntimeRetargetComponent>()) return true;
			}
		}
		return false;
	}
}

/**
 * The optional runtime retarget follower stays a seam of the UEFN Mover pawn. The CMC pawns play the Manny skeleton
 * natively (GASP-15), so neither their PawnData nor their classes compose a follower.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRuntimeRetargetCompositionTest,
	"SurvivalRpg.GASP.RuntimeRetarget.OptionalProfileComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuntimeRetargetCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuntimeRetargetTests;
	const URpgPawnData* MoverPawnData = LoadObject<URpgPawnData>(nullptr, MoverPawnDataPath);
	if (!TestNotNull(TEXT("Mover PawnData loads"), MoverPawnData)
		|| !TestNotNull(TEXT("The Mover composes the optional designer profile"), MoverPawnData->RuntimeRetargetProfile.Get())) return false;
	const URpgRuntimeRetargetProfile* Profile = MoverPawnData->RuntimeRetargetProfile;
	TestNull(TEXT("UEFN remains the Mover's default visible character"), Profile->TargetMesh.Get());
	TestNull(TEXT("No target skeleton is implicitly selected"), Profile->Retargeter.Get());
	TestNotNull(TEXT("Optional retarget presentation has a configured AnimBP"), Profile->RetargetAnimClass.Get());

	for (const TCHAR* Path : { CmcPawnDataPath, MantlePawnDataPath })
	{
		const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, Path);
		if (!TestNotNull(FString::Printf(TEXT("CMC PawnData loads: %s"), Path), PawnData)
			|| !TestNotNull(TEXT("CMC PawnData selects a pawn"), PawnData->PawnClass.Get())) continue;
		TestNull(FString::Printf(TEXT("%s composes no follower profile"), Path), PawnData->RuntimeRetargetProfile.Get());
		TestFalse(FString::Printf(TEXT("%s adds no follower component"), *PawnData->PawnClass->GetPathName()),
			AddsRetargetComponent(PawnData->PawnClass.Get()));
		const ARpgCharacter* Pawn = Cast<ARpgCharacter>(PawnData->PawnClass->GetDefaultObject());
		TestTrue(TEXT("The CMC gameplay mesh is the GASP-look Manny"), Pawn && Pawn->GetMesh()->GetSkeletalMeshAsset()
			&& Pawn->GetMesh()->GetSkeletalMeshAsset()->GetPathName() == MannyMeshPath);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
