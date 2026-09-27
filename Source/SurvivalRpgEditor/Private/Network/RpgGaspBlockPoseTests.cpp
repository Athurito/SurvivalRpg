// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "AnimPose.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgGaspBlockArmPoseContract,
	"SurvivalRpg.GASP.MovingBlock.SourceClipsPreserveUEFNArmJointTranslationsAndScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGaspBlockArmPoseContract::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USkeletalMesh> Target(LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin")));
	if (!TestNotNull(TEXT("The actual UEFN gameplay mesh loads"), Target.Get())) return false;
	const FName ArmBones[] = { TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
		TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r") };
	// The combat phases reproduce the compatible-skeleton fault; native GASP locomotion, turning,
	// traversal and getup exercise the other consumers of this target skeleton's retarget policy.
	const TCHAR* Sequences[] = {
		TEXT("/GF_Combat_Core/Animations/Sword_and_Shield/Animations/Sequence2/08_Hit/12_Block/Block_Start_Seq.Block_Start_Seq"),
		TEXT("/GF_Combat_Core/Animations/Sword_and_Shield/Animations/Sequence2/08_Hit/12_Block/Block_Loop_Seq.Block_Loop_Seq"),
		TEXT("/GF_Combat_Core/Animations/Sword_and_Shield/Animations/Sequence2/08_Hit/12_Block/Block_End_Seq.Block_End_Seq"),
		TEXT("/GF_Combat_Core/Animations/Sword_and_Shield/Animations/Sequence2/08_Hit/12_Block/Block_Hit_Seq.Block_Hit_Seq"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_RR.M_Neutral_Run_Loop_RR"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Walk/M_Neutral_Walk_Loop_F.M_Neutral_Walk_Loop_F"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Turn_090_L.M_Neutral_Stand_Turn_090_L"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Traversal/Mantle/M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot.M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Traversal/Vault/M_Neutral_Traversal_Vault_1_0_run_F_Lfoot.M_Neutral_Traversal_Vault_1_0_run_F_Lfoot"),
		TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/Ragdoll/Characters/UEFN_Mannequin/Animations/Ragdoll/M_ragdoll_getup_stand_F.M_ragdoll_getup_stand_F")
	};
	FAnimPoseEvaluationOptions Options;
	Options.EvaluationType = EAnimDataEvalType::Compressed;
	Options.bShouldRetarget = true;
	Options.bExtractRootMotion = false;
	Options.bIncorporateRootMotionIntoPose = false;
	Options.OptionalSkeletalMesh = Target.Get();
	Options.bRetrieveAdditiveAsFullPose = true;
	Options.bEvaluateCurves = true;
	for (const TCHAR* Path : Sequences)
	{
		TStrongObjectPtr<UAnimSequence> Sequence(LoadObject<UAnimSequence>(nullptr, Path));
		if (!TestNotNull(FString::Printf(TEXT("Sequence loads: %s"), Path), Sequence.Get())) continue;
		if (!TestTrue(TEXT("Evaluated sequence has a finite positive duration"), FMath::IsFinite(Sequence->GetPlayLength()) && Sequence->GetPlayLength() > 0.0f)) continue;
		float MaximumOffset = 0.0f, MaximumScaleError = 0.0f;
		FName WorstBone;
		double WorstTime = 0.0;
		int32 VerifiedBones = 0;
		for (int32 Sample = 0; Sample < 17; ++Sample)
		{
			const double Time = Sequence->GetPlayLength() * Sample / 16.0;
			FAnimPose Pose;
			UAnimPoseExtensions::GetAnimPoseAtTime(Sequence.Get(), Time, Options, Pose);
			if (!TestTrue(FString::Printf(TEXT("Compressed target-mesh evaluation is valid: %s t=%.4f"), *Sequence->GetName(), Time), Pose.IsValid())) continue;
			TArray<FName> PoseBones;
			UAnimPoseExtensions::GetBoneNames(Pose, PoseBones);
			for (const FName Bone : ArmBones)
			{
				const int32 ReferenceIndex = Target->GetRefSkeleton().FindBoneIndex(Bone);
				if (!TestTrue(FString::Printf(TEXT("The evaluated and actual target reference poses both contain %s"), *Bone.ToString()),
					PoseBones.Contains(Bone) && ReferenceIndex != INDEX_NONE)) continue;
				const FTransform& Local = UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::Local);
				const FTransform& Reference = Target->GetRefSkeleton().GetRefBonePose()[ReferenceIndex];
				if (!TestFalse(TEXT("Evaluated joint transform is finite"), Local.ContainsNaN())) continue;
				++VerifiedBones;
				// Check the entire translation vector: the original failure preserved bone lengths while
				// moving elbow/wrist offsets by more than 6 cm. No joint rotations are prescribed here.
				const float Offset = static_cast<float>(FVector::Distance(Local.GetTranslation(), Reference.GetTranslation()));
				if (Offset > MaximumOffset) { MaximumOffset = Offset; WorstBone = Bone; WorstTime = Time; }
				MaximumScaleError = FMath::Max(MaximumScaleError, static_cast<float>((Local.GetScale3D() - Reference.GetScale3D()).GetAbsMax()));
			}
		}
		AddInfo(FString::Printf(TEXT("ArmJointContract clip=%s samples=17 bones=%d maxTranslationCm=%.6f bone=%s time=%.4f maxScaleError=%.8f"),
			*Sequence->GetPathName(), VerifiedBones, MaximumOffset, *WorstBone.ToString(), WorstTime, MaximumScaleError));
		TestEqual(TEXT("Every requested joint was evaluated at all 17 sample times"), VerifiedBones, static_cast<int32>(17 * UE_ARRAY_COUNT(ArmBones)));
		TestTrue(FString::Printf(TEXT("%s preserves target arm joint offsets within 0.01 cm"), *Sequence->GetName()), MaximumOffset <= 0.01f);
		TestTrue(FString::Printf(TEXT("%s preserves target arm joint scale"), *Sequence->GetName()), MaximumScaleError <= UE_KINDA_SMALL_NUMBER);
	}
	return !HasAnyErrors();
}

#endif
