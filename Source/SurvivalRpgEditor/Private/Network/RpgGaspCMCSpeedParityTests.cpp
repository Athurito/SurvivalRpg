#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgMovementProfile.h"

namespace RpgGaspCMCSpeedParityTests
{
	constexpr TCHAR PawnClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/BP_RpgGasp_CMC.BP_RpgGasp_CMC_C");
	constexpr TCHAR AnimBlueprintPackage[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/ABP_RpgGasp_CMC");
	constexpr TCHAR DatabaseChooserPackage[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Choosers/CHT_RpgGasp_CMC_PoseSearchDatabases");
	constexpr TCHAR AnimationRoot[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/");

	// Capsule speed over authored loop speed; outside this band Motion Matching visibly slides the feet.
	constexpr double MinPlayRate = 0.85;
	constexpr double MaxPlayRate = 1.15;

	enum class EGaitSpeed : uint8 { Walk, Run, Sprint, Crouch };

	struct FReferenceLoop
	{
		EGaitSpeed Gait;
		const TCHAR* Path;
	};

	/** The main loop per direction that the CMC database chooser selects for each gait; crouch reuses the shared Neutral loops. */
	const FReferenceLoop ReferenceLoops[] =
	{
		{EGaitSpeed::Walk, TEXT("Walk/M_Relaxed_Walk_Loop_F")},
		{EGaitSpeed::Walk, TEXT("Walk/M_Relaxed_Walk_Loop_B")},
		{EGaitSpeed::Walk, TEXT("Walk/M_Relaxed_Walk_Loop_LL")},
		{EGaitSpeed::Walk, TEXT("Walk/M_Relaxed_Walk_Loop_RL")},
		{EGaitSpeed::Run, TEXT("Run/M_Relaxed_Run_Loop_F")},
		{EGaitSpeed::Run, TEXT("Run/M_Relaxed_Run_Loop_B")},
		{EGaitSpeed::Run, TEXT("Run/M_Relaxed_Run_Loop_LL")},
		{EGaitSpeed::Run, TEXT("Run/M_Relaxed_Run_Loop_RL")},
		{EGaitSpeed::Sprint, TEXT("Sprint/M_Relaxed_Sprint_Loop_F")},
		{EGaitSpeed::Crouch, TEXT("Crouch/M_Neutral_Crouch_Loop_F")},
		{EGaitSpeed::Crouch, TEXT("Crouch/M_Neutral_Crouch_Loop_B")},
		{EGaitSpeed::Crouch, TEXT("Crouch/M_Neutral_Crouch_Loop_LL")},
		{EGaitSpeed::Crouch, TEXT("Crouch/M_Neutral_Crouch_Loop_RL")},
	};

	double TargetSpeed(const URpgMovementProfile& Profile, EGaitSpeed Gait)
	{
		switch (Gait)
		{
		case EGaitSpeed::Walk: return Profile.WalkSpeed;
		case EGaitSpeed::Sprint: return Profile.SprintSpeed;
		case EGaitSpeed::Crouch: return Profile.CrouchSpeed;
		default: return Profile.RunSpeed;
		}
	}

	/** Horizontal root speed over the whole authored loop, in cm/s. */
	double RootSpeed(const UAnimSequence& Sequence)
	{
		const double Length = Sequence.GetPlayLength();
		const FTransform Motion = Sequence.ExtractRootMotionFromRange(0.0, Length, FAnimExtractContext());
		return Length > UE_SMALL_NUMBER ? Motion.GetTranslation().Size2D() / Length : 0.0;
	}

	/** True when one of the chooser's Pose Search databases contains the clip, so the gait can actually play it. */
	bool IsSelectableBy(const IAssetRegistry& Registry, FName ChooserPackage, FName ClipPackage)
	{
		TArray<FName> Databases;
		Registry.GetDependencies(ChooserPackage, Databases);
		for (const FName Database : Databases)
		{
			TArray<FName> Clips;
			Registry.GetDependencies(Database, Clips);
			if (Clips.Contains(ClipPackage))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgGaspCMCSpeedParityTest,
	"SurvivalRpg.GASP.CMC.SpeedParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGaspCMCSpeedParityTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspCMCSpeedParityTests;
	const UClass* PawnClass = LoadClass<ARpgCharacter>(nullptr, PawnClassPath);
	const URpgCharacterMovementComponent* Movement = PawnClass
		? Cast<URpgCharacterMovementComponent>(PawnClass->GetDefaultObject<ARpgCharacter>()->GetCharacterMovement()) : nullptr;
	if (!TestNotNull(TEXT("CMC pawn keeps the RPG movement component"), Movement))
	{
		return false;
	}
	const URpgMovementProfile* Profile = Movement->MovementProfile;
	FText ProfileError;
	if (!TestNotNull(TEXT("CMC pawn takes its movement from a shared profile"), Profile)
		|| !TestTrue(FString::Printf(TEXT("Shared movement profile validates: %s"), *ProfileError.ToString()), Profile->ValidateConfiguration(ProfileError)))
	{
		return false;
	}

	const IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	// Every pawn derived from the CMC pawn (traversal and pilot variants) moves with the same profile.
	TSet<FTopLevelAssetPath> DerivedClasses;
	Registry.GetDerivedClassNames({FTopLevelAssetPath(PawnClass)}, {}, DerivedClasses);
	TestTrue(TEXT("The CMC pawn has derived variants to check"), DerivedClasses.Num() > 1);
	for (const FTopLevelAssetPath& DerivedPath : DerivedClasses)
	{
		const UClass* Derived = LoadClass<ARpgCharacter>(nullptr, *DerivedPath.ToString());
		const URpgCharacterMovementComponent* DerivedMovement = Derived
			? Cast<URpgCharacterMovementComponent>(Derived->GetDefaultObject<ARpgCharacter>()->GetCharacterMovement()) : nullptr;
		TestTrue(FString::Printf(TEXT("%s inherits the shared movement profile"), *DerivedPath.ToString()),
			DerivedMovement && DerivedMovement->MovementProfile == Profile);
	}
	TArray<FName> AnimDependencies;
	Registry.GetDependencies(AnimBlueprintPackage, AnimDependencies);
	TestTrue(TEXT("CMC AnimBP selects its Motion Matching databases through the CMC chooser"),
		AnimDependencies.Contains(FName(DatabaseChooserPackage)));

	for (const FReferenceLoop& Loop : ReferenceLoops)
	{
		const FString Package = FString(AnimationRoot) + Loop.Path;
		const FString Name = FPackageName::GetShortName(Package);
		const UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *(Package + TEXT(".") + Name));
		if (!TestNotNull(FString::Printf(TEXT("Reference loop loads: %s"), *Name), Sequence))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("CMC chooser can select %s"), *Name), IsSelectableBy(Registry, DatabaseChooserPackage, FName(*Package)));
		const double Authored = RootSpeed(*Sequence);
		const double Target = TargetSpeed(*Profile, Loop.Gait);
		const double PlayRate = Authored > UE_SMALL_NUMBER ? Target / Authored : 0.0;
		TestTrue(FString::Printf(TEXT("%s: %.0f cm/s against %.0f cm/s authored is a %.2fx play rate inside %.2f-%.2f"),
			*Name, Target, Authored, PlayRate, MinPlayRate, MaxPlayRate), PlayRate >= MinPlayRate && PlayRate <= MaxPlayRate);
	}
	return true;
}

#endif
