#include "RpgRuntimeRetargetProfile.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "Rig/IKRigProcessor.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRuntimeRetargetProfile)

namespace RpgRuntimeRetarget
{
	bool RigSupportsMesh(const UIKRigDefinition* Rig, const USkeletalMesh* Mesh)
	{
		if (!Rig || !Mesh || !Mesh->GetSkeleton()
			|| !FIKRigProcessor::IsIKRigCompatibleWithSkeleton(Rig, FIKRigInputSkeleton(Mesh), nullptr)) return false;
		const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
		// Optional, unmapped retarget chains may be absent. Validate engine-required solver/goal bones and the rig roots.
		for (const FName Bone : {Rig->GetRoot(), Rig->GetPelvis()})
		{
			if (!Bone.IsNone() && Skeleton.FindBoneIndex(Bone) == INDEX_NONE) return false;
		}
		return true;
	}
}

bool URpgRuntimeRetargetProfile::ValidateConfiguration(const USkeletalMesh* SourceMesh, FText& OutError) const
{
	OutError = FText::GetEmpty();
	if (!TargetMesh) return true;
	if (!Retargeter || !RetargetAnimClass || RetargetAnimClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		OutError = NSLOCTEXT("RpgRuntimeRetarget", "MissingAssets", "An enabled retarget profile requires a retargeter and a usable presentation AnimBP.");
		return false;
	}
	if (!RelativeTransform.IsValid() || RelativeTransform.GetScale3D().GetMin() <= UE_SMALL_NUMBER)
	{
		OutError = NSLOCTEXT("RpgRuntimeRetarget", "InvalidTransform", "The cosmetic mesh transform must be finite with normalized rotation and positive scale.");
		return false;
	}
	const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(RetargetAnimClass.Get());
	const USkeleton* AnimSkeleton = AnimClass ? AnimClass->GetTargetSkeleton() : nullptr;
	const bool bContainsRetargetNode = AnimClass && AnimClass->GetAnimNodeProperties().ContainsByPredicate(
		[](const FStructProperty* Property)
		{
			return Property && Property->Struct == FAnimNode_RetargetPoseFromMesh::StaticStruct();
		});
	if (!TargetMesh->GetSkeleton() || !bContainsRetargetNode || (AnimSkeleton && !AnimSkeleton->IsCompatibleMesh(TargetMesh, false)))
	{
		OutError = NSLOCTEXT("RpgRuntimeRetarget", "InvalidAnimBlueprint", "The presentation AnimBP must contain Retarget Pose From Mesh and support the target skeleton.");
		return false;
	}
	const UIKRigDefinition* SourceRig = Retargeter->GetIKRig(ERetargetSourceOrTarget::Source);
	const UIKRigDefinition* TargetRig = Retargeter->GetIKRig(ERetargetSourceOrTarget::Target);
	if (!SourceRig || !RpgRuntimeRetarget::RigSupportsMesh(TargetRig, TargetMesh)
		|| (SourceMesh && !RpgRuntimeRetarget::RigSupportsMesh(SourceRig, SourceMesh)))
	{
		OutError = NSLOCTEXT("RpgRuntimeRetarget", "InvalidRigs", "The retargeter's source and target rigs must support the selected meshes and their required bones.");
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult URpgRuntimeRetargetProfile::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FText Error;
	if (!ValidateConfiguration(nullptr, Error))
	{
		Context.AddError(Error);
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
