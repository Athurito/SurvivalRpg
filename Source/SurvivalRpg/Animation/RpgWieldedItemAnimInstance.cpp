#include "RpgWieldedItemAnimInstance.h"

#include "AbilitySystemGlobals.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWieldedItemAnimInstance)

void URpgWieldedItemAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	WielderBoneLocation = FVector::ZeroVector;
	bHasWielderBone = false;
	bWielderRangedDrawing = false;

	// Equipment attaches the item's root to the wielder's mesh; that mesh's owner is the wielder.
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const AActor* Item = GetOwningActor();
	const USceneComponent* ItemRoot = Item ? Item->GetRootComponent() : nullptr;
	const USkinnedMeshComponent* WielderMesh = ItemRoot ? Cast<USkinnedMeshComponent>(ItemRoot->GetAttachParent()) : nullptr;
	const AActor* Wielder = WielderMesh ? WielderMesh->GetOwner() : nullptr;
	if (!Mesh || !Wielder)
	{
		return;
	}

	if (!WielderBone.IsNone() && WielderMesh->DoesSocketExist(WielderBone))
	{
		FTransform LocalFrame = FTransform::Identity;
		const USkinnedAsset* Asset = Mesh->GetSkinnedAsset();
		const int32 FrameIndex = Asset && !LocalFrameBone.IsNone() ? Asset->GetRefSkeleton().FindBoneIndex(LocalFrameBone) : INDEX_NONE;
		if (FrameIndex != INDEX_NONE)
		{
			LocalFrame = FAnimationRuntime::GetComponentSpaceTransformRefPose(Asset->GetRefSkeleton(), FrameIndex);
		}
		const FVector ComponentLocation = Mesh->GetComponentTransform().InverseTransformPosition(WielderMesh->GetSocketLocation(WielderBone));
		WielderBoneLocation = LocalFrame.InverseTransformPosition(ComponentLocation);
		bHasWielderBone = true;
	}

	// Only the wielder's own ability system presents its draw, not one whose avatar moved on to another pawn.
	if (const URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Wielder)))
	{
		bWielderRangedDrawing = ASC->GetAvatarActor() == Wielder && ASC->IsRangedDrawingForPresentation();
	}
}
