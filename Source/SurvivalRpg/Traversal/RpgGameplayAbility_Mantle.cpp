#include "RpgGameplayAbility_Mantle.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "AnimationWarpingLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/PackageMapClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "MoverDataModelTypes.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Misc/MemStack.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"
#include "SurvivalRpg/Core/Character/RpgMoverTraversalTypes.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Mantle)

bool FRpgMantleTargetData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	UObject* ColliderObject = HitComponent.Get();
	UObject* MontageObject = Montage.Get();
	const bool bColliderMapped = Map && Map->SerializeObject(Ar, UPrimitiveComponent::StaticClass(), ColliderObject);
	const bool bMontageMapped = Map && Map->SerializeObject(Ar, UAnimMontage::StaticClass(), MontageObject);
	if (Ar.IsLoading())
	{
		HitComponent = Cast<UPrimitiveComponent>(ColliderObject);
		Montage = Cast<UAnimMontage>(MontageObject);
	}
	Ar << StartTime;
	bOutSuccess = bColliderMapped && bMontageMapped && !Ar.IsError();
	return true;
}

namespace RpgMantle
{
	UCapsuleComponent* Capsule(const APawn& Pawn)
	{
		const ACharacter* Character = Cast<ACharacter>(&Pawn);
		return Character ? Character->GetCapsuleComponent() : Cast<UCapsuleComponent>(Pawn.GetRootComponent());
	}

	FCollisionShape CapsuleShape(const APawn& Pawn)
	{
		const UCapsuleComponent* PawnCapsule = Capsule(Pawn);
		return FCollisionShape::MakeCapsule(PawnCapsule->GetScaledCapsuleRadius(), PawnCapsule->GetScaledCapsuleHalfHeight());
	}

	bool IsWalkable(const APawn& Pawn, const FHitResult& Hit)
	{
		if (const ACharacter* Character = Cast<ACharacter>(&Pawn))
		{
			return Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsWalkable(Hit);
		}
		const URpgCharacterMoverComponent* Mover = Pawn.FindComponentByClass<URpgCharacterMoverComponent>();
		return Mover && Mover->IsTraversalWalkable(Hit);
	}

	bool IsIncapacitated(const AActor& Character)
	{
		const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(&Character);
		const URpgDownedComponent* Downed = URpgDownedComponent::FindDownedComponent(&Character);
		return (Health && Health->IsDeadOrDying()) || (Downed && Downed->IsDowned());
	}
}

URpgGameplayAbility_Mantle::URpgGameplayAbility_Mantle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
}

bool URpgGameplayAbility_Mantle::IsCharacterReady(const ACharacter& Character) const
{
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	const USkeletalMeshComponent* Mesh = Character.GetMesh();
	const UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const UMotionWarpingComponent* Warping = Character.FindComponentByClass<UMotionWarpingComponent>();
	const URpgCharacterMovementComponent* RpgMovement = Cast<URpgCharacterMovementComponent>(Movement);
	return Movement && Character.GetCapsuleComponent() && Character.GetCapsuleComponent()->IsQueryCollisionEnabled()
		&& (Movement->IsMovingOnGround() || Movement->IsFalling()) && !Character.bIsCrouched && !RpgMantle::IsIncapacitated(Character)
		&& MeshAsset && Character.FindComponentByClass<URpgTraversalQueryComponent>()
		&& Animation && Animation->RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly
		&& !Animation->IsSlotActive(FName(TEXT("DefaultSlot")))
		&& !WarpTargetName.IsNone() && FMath::IsFinite(LedgeVerticalOffset)
		&& FMath::IsFinite(CandidateSearchDistance) && CandidateSearchDistance > 0.0f
		&& FMath::IsFinite(PathClearance) && PathClearance >= 0.0f
		&& Warping && !Warping->FindWarpTarget(WarpTargetName) && RpgMovement && !RpgMovement->GetMantleCollisionComponent();
}

bool URpgGameplayAbility_Mantle::IsPawnReady(const APawn& Pawn) const
{
	if (const ACharacter* Character = Cast<ACharacter>(&Pawn)) return IsCharacterReady(*Character);
	const URpgCharacterMoverComponent* Mover = Pawn.FindComponentByClass<URpgCharacterMoverComponent>();
	const UCapsuleComponent* Capsule = RpgMantle::Capsule(Pawn);
	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(&Pawn);
	const UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const UMotionWarpingComponent* Warping = Pawn.FindComponentByClass<UMotionWarpingComponent>();
	return Mover && Capsule && Mover->GetUpdatedComponent() == Capsule && Capsule->IsQueryCollisionEnabled()
		&& Mover->IsOnGround() && !Mover->IsCrouching() && !RpgMantle::IsIncapacitated(Pawn)
		&& Mesh && Mesh->GetSkeletalMeshAsset() && Mover->GetPrimaryVisualComponent() == Mesh
		&& Animation && !Animation->IsSlotActive(TEXT("DefaultSlot"))
		&& Pawn.FindComponentByClass<URpgTraversalQueryComponent>() && !WarpTargetName.IsNone()
		&& FMath::IsFinite(LedgeVerticalOffset) && FMath::IsFinite(CandidateSearchDistance) && CandidateSearchDistance > 0.0f
		&& FMath::IsFinite(PathClearance) && PathClearance >= 0.0f
		&& Warping && !Warping->FindWarpTarget(WarpTargetName) && Mover->GetTraversalWarpingAdapter() && !Mover->HasTraversalLease();
}

bool URpgGameplayAbility_Mantle::GetMantleLandingLocation(const ACharacter& Character,
	const FRpgTraversalQueryResult& Result, FVector& OutLocation) const
{
	return GetMantleLandingLocation(static_cast<const APawn&>(Character), Result, OutLocation);
}

bool URpgGameplayAbility_Mantle::GetMantleLandingLocation(const APawn& Pawn,
	const FRpgTraversalQueryResult& Result, FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	const URpgTraversalQueryComponent* Query = Pawn.FindComponentByClass<URpgTraversalQueryComponent>();
	const FRpgTraversalAnimationEntry* Entry = Query ? Query->AllowedMantleAnimations.FindByPredicate(
		[&Result](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Result.ChosenMontage; }) : nullptr;
	return Entry && GetMantleLandingAtTime(Pawn, Result, Entry->HandoffTime, OutLocation);
}

bool URpgGameplayAbility_Mantle::GetMantleLandingAtTime(const APawn& Pawn, const FRpgTraversalQueryResult& Result,
	float HandoffTime, FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	const ACharacter* Character = Cast<ACharacter>(&Pawn);
	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(&Pawn);
	const URpgCharacterMoverComponent* Mover = Character ? nullptr : Pawn.FindComponentByClass<URpgCharacterMoverComponent>();
	const UMotionWarpingBaseAdapter* Adapter = Mover ? Mover->GetTraversalWarpingAdapter() : nullptr;
	if ((!Character && !Adapter) || !Result.ChosenMontage || !Mesh || !Mesh->GetAnimInstance()
		|| Result.FrontLedgeLocation.ContainsNaN() || Result.FrontLedgeNormal.ContainsNaN()) return false;
	if (!FMath::IsFinite(HandoffTime) || HandoffTime <= Result.StartTime
		|| HandoffTime > Result.ChosenMontage->GetPlayLength() + 0.001f) return false;
	HandoffTime = FMath::Min(HandoffTime, Result.ChosenMontage->GetPlayLength());
	TArray<FMotionWarpingWindowData> Windows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, WarpTargetName, Windows);
	const FMotionWarpingWindowData* Last = nullptr;
	for (const FMotionWarpingWindowData& Window : Windows)
	{
		if (Window.EndTime > Result.StartTime && (!Last || Window.EndTime > Last->EndTime)) Last = &Window;
	}
	const URootMotionModifier_SkewWarp* Modifier = Last && Last->AnimNotify
		? Cast<URootMotionModifier_SkewWarp>(Last->AnimNotify->RootMotionModifier) : nullptr;
	if (!Modifier || Last->EndTime > HandoffTime || !Modifier->bWarpTranslation || Modifier->bIgnoreZAxis || !Modifier->bWarpToFeetLocation
		|| !Modifier->bWarpRotation || Modifier->RotationType != EMotionWarpRotationType::Default) return false;
	FMemMark Mark(FMemStack::Get());
	FTransform Offset = FTransform::Identity;
	if (Modifier->WarpPointAnimProvider == EWarpPointAnimProvider::Bone)
	{
		if (Mesh->GetBoneIndex(Modifier->WarpPointAnimBoneName) == INDEX_NONE) return false;
		Offset = Character
			? UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*Character, Result.ChosenMontage, Last->EndTime, Modifier->WarpPointAnimBoneName)
			: UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*Adapter, Result.ChosenMontage, Last->EndTime, Modifier->WarpPointAnimBoneName);
	}
	else if (Modifier->WarpPointAnimProvider == EWarpPointAnimProvider::Static)
	{
		Offset = Character
			? UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*Character, Result.ChosenMontage, Last->EndTime, Modifier->WarpPointAnimTransform)
			: UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*Adapter, Result.ChosenMontage, Last->EndTime, Modifier->WarpPointAnimTransform);
	}
	Offset.ScaleTranslation(Mesh->GetComponentScale());
	const FQuat Facing = (-Result.FrontLedgeNormal.GetSafeNormal2D()).ToOrientationQuat();
	const FTransform RootAtWarp = Offset * FTransform(Facing, Result.FrontLedgeLocation + FVector(0, 0, LedgeVerticalOffset));
	// Running samples continue with a long locomotion tail after their forced blend-out notify releases traversal.
	// Validate the authored handoff, not the unused end of that root-motion tail.
	const FTransform Remaining = Modifier->bSubtractRemainingRootMotion ? FTransform::Identity
		: UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Result.ChosenMontage, Last->EndTime, HandoffTime);
	const FQuat ActorRotation = Modifier->AdditionalRotationOffset.Quaternion() * RootAtWarp.GetRotation();
	const FQuat VisualRotation = Character ? Character->GetBaseRotationOffset() : Mover->GetBaseVisualComponentTransform().GetRotation();
	OutLocation = RootAtWarp.GetLocation() + (ActorRotation * VisualRotation).RotateVector(Remaining.GetTranslation());
	return !OutLocation.ContainsNaN();
}

bool URpgGameplayAbility_Mantle::GetVaultExitLocation(const ACharacter& Character,
	const FRpgTraversalQueryResult& Result, FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	const URpgTraversalQueryComponent* Query = Character.FindComponentByClass<URpgTraversalQueryComponent>();
	const FRpgTraversalAnimationEntry* Entry = Query ? Query->AllowedVaultAnimations.FindByPredicate(
		[&Result](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Result.ChosenMontage; }) : nullptr;
	if (Result.ActionType != 2 || !Result.ChosenMontage || !Character.GetMesh() || !Entry
		|| !FMath::IsFinite(Entry->HandoffTime) || Entry->HandoffTime <= Result.StartTime
		|| Entry->HandoffTime > Result.ChosenMontage->GetPlayLength() + 0.001f
		|| Result.FrontLedgeLocation.ContainsNaN() || Result.FrontLedgeNormal.ContainsNaN()
		|| Result.BackLedgeLocation.ContainsNaN() || BackLedgeWarpTargetName.IsNone()
		|| BackLedgeWarpTargetName == WarpTargetName) return false;
	const float HandoffTime = FMath::Min(Entry->HandoffTime, Result.ChosenMontage->GetPlayLength());
	TArray<FMotionWarpingWindowData> FrontWindows;
	TArray<FMotionWarpingWindowData> BackWindows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, WarpTargetName, FrontWindows);
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackLedgeWarpTargetName, BackWindows);
	const FMotionWarpingWindowData* Front = nullptr;
	const FMotionWarpingWindowData* Back = nullptr;
	for (const FMotionWarpingWindowData& Window : FrontWindows)
	{
		if (Window.EndTime > Result.StartTime && (!Front || Window.EndTime > Front->EndTime)) Front = &Window;
	}
	for (const FMotionWarpingWindowData& Window : BackWindows)
	{
		if (Window.EndTime > Result.StartTime && (!Back || Window.EndTime > Back->EndTime)) Back = &Window;
	}
	const URootMotionModifier_SkewWarp* FrontModifier = Front && Front->AnimNotify
		? Cast<URootMotionModifier_SkewWarp>(Front->AnimNotify->RootMotionModifier) : nullptr;
	if (!FrontModifier || Front->EndTime > HandoffTime || !FrontModifier->bWarpTranslation || FrontModifier->bIgnoreZAxis
		|| !FrontModifier->bWarpToFeetLocation || !FrontModifier->bWarpRotation
		|| FrontModifier->RotationType != EMotionWarpRotationType::Default || FrontModifier->bSubtractRemainingRootMotion) return false;
	FMemMark Mark(FMemStack::Get());
	FTransform Offset = FTransform::Identity;
	if (FrontModifier->WarpPointAnimProvider == EWarpPointAnimProvider::Bone)
	{
		if (Character.GetMesh()->GetBoneIndex(FrontModifier->WarpPointAnimBoneName) == INDEX_NONE) return false;
		Offset = UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(Character, Result.ChosenMontage,
			Front->EndTime, FrontModifier->WarpPointAnimBoneName);
	}
	else if (FrontModifier->WarpPointAnimProvider == EWarpPointAnimProvider::Static)
	{
		Offset = UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(Character, Result.ChosenMontage,
			Front->EndTime, FrontModifier->WarpPointAnimTransform);
	}
	Offset.ScaleTranslation(Character.GetMesh()->GetComponentScale());
	const FQuat Facing = (-Result.FrontLedgeNormal.GetSafeNormal2D()).ToOrientationQuat();
	const FTransform RootAtFront = Offset * FTransform(Facing, Result.FrontLedgeLocation + FVector(0, 0, LedgeVerticalOffset));
	FVector FeetAtWarp = RootAtFront.GetLocation();
	FQuat ActorRotation = FrontModifier->AdditionalRotationOffset.Quaternion() * RootAtFront.GetRotation();
	float LastWarpEnd = Front->EndTime;
	if (Back)
	{
		const URootMotionModifier_SkewWarp* BackModifier = Back->AnimNotify
			? Cast<URootMotionModifier_SkewWarp>(Back->AnimNotify->RootMotionModifier) : nullptr;
		// Standing vault translates to the opposite ledge without rotating or applying an animated attach-bone offset.
		if (!BackModifier || Back->StartTime < Front->EndTime || Back->EndTime > HandoffTime
			|| !BackModifier->bWarpTranslation || BackModifier->bIgnoreZAxis || !BackModifier->bWarpToFeetLocation
			|| BackModifier->bWarpRotation || BackModifier->WarpPointAnimProvider != EWarpPointAnimProvider::None
			|| BackModifier->bSubtractRemainingRootMotion) return false;
		const FQuat MeshRotation = ActorRotation * Character.GetBaseRotationOffset();
		const FTransform Between = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Result.ChosenMontage, Front->EndTime, Back->EndTime);
		ActorRotation = MeshRotation * Between.GetRotation() * MeshRotation.Inverse() * ActorRotation;
		FeetAtWarp = Result.BackLedgeLocation;
		LastWarpEnd = Back->EndTime;
	}
	const FTransform Remaining = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Result.ChosenMontage, LastWarpEnd, HandoffTime);
	OutLocation = FeetAtWarp + (ActorRotation * Character.GetBaseRotationOffset()).RotateVector(Remaining.GetTranslation());
	return !OutLocation.ContainsNaN();
}

bool URpgGameplayAbility_Mantle::GetHurdleLandingLocation(const ACharacter& Character,
	const FRpgTraversalQueryResult& Result, FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	const URpgTraversalQueryComponent* Query = Character.FindComponentByClass<URpgTraversalQueryComponent>();
	const FRpgTraversalAnimationEntry* Entry = Query ? Query->AllowedHurdleAnimations.FindByPredicate(
		[&Result](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Result.ChosenMontage; }) : nullptr;
	FVector FloorTarget;
	return Entry && GetHurdleTargetsAndLanding(Character, Result, Entry->HandoffTime, FloorTarget, OutLocation);
}

bool URpgGameplayAbility_Mantle::GetHurdleTargetsAndLanding(const ACharacter& Character, const FRpgTraversalQueryResult& Result,
	float HandoffTime, FVector& OutFloorTarget, FVector& OutLocation) const
{
	OutFloorTarget = OutLocation = FVector::ZeroVector;
	if (Result.ActionType != 1 || !Result.ChosenMontage || !Character.GetMesh() || !Character.GetMesh()->GetAnimInstance()
		|| !FMath::IsFinite(HandoffTime) || HandoffTime <= Result.StartTime || HandoffTime > Result.ChosenMontage->GetPlayLength() + 0.001f
		|| Result.FrontLedgeLocation.ContainsNaN() || Result.FrontLedgeNormal.ContainsNaN() || Result.BackLedgeLocation.ContainsNaN()
		|| Result.BackLedgeNormal.ContainsNaN() || Result.BackFloorLocation.ContainsNaN() || BackFloorDistanceCurveName.IsNone()
		|| WarpTargetName.IsNone() || BackLedgeWarpTargetName.IsNone() || BackFloorWarpTargetName.IsNone()
		|| BackFloorWarpTargetName == WarpTargetName || BackFloorWarpTargetName == BackLedgeWarpTargetName
		|| BackLedgeWarpTargetName == WarpTargetName) return false;
	TArray<FMotionWarpingWindowData> FrontWindows, BackWindows, FloorWindows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, WarpTargetName, FrontWindows);
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackLedgeWarpTargetName, BackWindows);
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackFloorWarpTargetName, FloorWindows);
	if (FrontWindows.IsEmpty() || FloorWindows.IsEmpty()) return false;
	const FMotionWarpingWindowData* Front = nullptr;
	for (const FMotionWarpingWindowData& Window : FrontWindows)
	{
		if (Window.EndTime > Result.StartTime && (!Front || Window.EndTime > Front->EndTime)) Front = &Window;
	}
	const URootMotionModifier_SkewWarp* FrontModifier = Front && Front->AnimNotify
		? Cast<URootMotionModifier_SkewWarp>(Front->AnimNotify->RootMotionModifier) : nullptr;
	if (!FrontModifier || !FrontModifier->bWarpTranslation || FrontModifier->bIgnoreZAxis || !FrontModifier->bWarpToFeetLocation
		|| !FrontModifier->bWarpRotation || FrontModifier->RotationType != EMotionWarpRotationType::Default
		|| FrontModifier->bSubtractRemainingRootMotion || FrontModifier->WarpPointAnimProvider != EWarpPointAnimProvider::Bone
		|| Character.GetMesh()->GetBoneIndex(FrontModifier->WarpPointAnimBoneName) == INDEX_NONE) return false;
	float FinalFloorEnd = 0.0f;
	for (const TArray<FMotionWarpingWindowData>* Windows : { &BackWindows, &FloorWindows })
	{
		for (const FMotionWarpingWindowData& Window : *Windows)
		{
			const URootMotionModifier_SkewWarp* Modifier = Window.AnimNotify
				? Cast<URootMotionModifier_SkewWarp>(Window.AnimNotify->RootMotionModifier) : nullptr;
			if (!Modifier || !Modifier->bWarpTranslation || Modifier->bIgnoreZAxis || !Modifier->bWarpToFeetLocation
				|| Modifier->bWarpRotation || Modifier->WarpPointAnimProvider != EWarpPointAnimProvider::None
				|| Modifier->bSubtractRemainingRootMotion || Window.StartTime < Front->EndTime
				|| Window.EndTime <= Window.StartTime || Window.EndTime > HandoffTime) return false;
			if (Windows == &FloorWindows) FinalFloorEnd = FMath::Max(FinalFloorEnd, Window.EndTime);
		}
	}
	// Use the original first-window curve samples. Some narrow entries have no BackLedge window: their distance is zero.
	float BackDistance = 0.0f;
	float FloorDistance = 0.0f;
	if ((!BackWindows.IsEmpty() && !UAnimationWarpingLibrary::GetCurveValueFromAnimation(Result.ChosenMontage,
		BackFloorDistanceCurveName, BackWindows[0].EndTime, BackDistance))
		|| !UAnimationWarpingLibrary::GetCurveValueFromAnimation(Result.ChosenMontage,
			BackFloorDistanceCurveName, FloorWindows[0].EndTime, FloorDistance)
		|| !FMath::IsFinite(BackDistance) || !FMath::IsFinite(FloorDistance)) return false;
	OutFloorTarget = Result.BackLedgeLocation + Result.BackLedgeNormal * FMath::Abs(BackDistance - FloorDistance);
	OutFloorTarget.Z = Result.BackFloorLocation.Z;
	FMemMark Mark(FMemStack::Get());
	const FTransform Offset = UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(Character,
		Result.ChosenMontage, Front->EndTime, FrontModifier->WarpPointAnimBoneName);
	const FQuat Facing = (-Result.FrontLedgeNormal.GetSafeNormal2D()).ToOrientationQuat();
	const FQuat ActorAtFront = FrontModifier->AdditionalRotationOffset.Quaternion() * Facing * Offset.GetRotation();
	const FQuat MeshAtFront = ActorAtFront * Character.GetBaseRotationOffset();
	const FTransform Between = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Result.ChosenMontage, Front->EndTime, FinalFloorEnd);
	const FQuat ActorAtFloor = MeshAtFront * Between.GetRotation() * MeshAtFront.Inverse() * ActorAtFront;
	// BackFloor is the last absolute translation target. Preserve the source BackLedge/BackFloor overlap; neither rotates.
	const FTransform Remaining = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Result.ChosenMontage, FinalFloorEnd,
		FMath::Min(HandoffTime, Result.ChosenMontage->GetPlayLength()));
	OutLocation = OutFloorTarget + (ActorAtFloor * Character.GetBaseRotationOffset()).RotateVector(Remaining.GetTranslation());
	return !OutLocation.ContainsNaN() && !OutFloorTarget.ContainsNaN();
}

bool URpgGameplayAbility_Mantle::ValidateTraversal(const APawn& Pawn, const FRpgTraversalQueryResult& Result) const
{
	const ACharacter* Character = Cast<ACharacter>(&Pawn);
	URpgTraversalQueryComponent* Query = Pawn.FindComponentByClass<URpgTraversalQueryComponent>();
	UPrimitiveComponent* Collider = Result.HitComponent;
	const UCapsuleComponent* Capsule = RpgMantle::Capsule(Pawn);
	const UAnimMontage* SelectedMontage = Result.ChosenMontage;
	if (!IsPawnReady(Pawn) || !Query || (Result.ActionType != 3 && Result.ActionType != 2 && Result.ActionType != 1) || !Result.HasFrontLedge
		|| !IsValid(Collider) || !Collider->IsRegistered() || Collider->GetWorld() != Pawn.GetWorld()
		|| Collider->IsSimulatingPhysics() || !Collider->IsQueryCollisionEnabled()
		|| Collider->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block
		|| !SelectedMontage || !SelectedMontage->HasRootMotion()
		|| SelectedMontage->GetSkeleton() != URpgPawnExtensionComponent::FindGameplayMesh(&Pawn)->GetSkeletalMeshAsset()->GetSkeleton()
		|| !Query->IsAnimationAllowed(Pawn, Result) || Result.FrontLedgeLocation.ContainsNaN()
		|| Result.FrontLedgeNormal.ContainsNaN() || Result.FrontLedgeNormal.GetSafeNormal2D().IsNearlyZero()
		|| FMath::Abs(Result.FrontLedgeNormal.Z) > 0.1 || !FMath::IsFinite(Result.ObstacleDepth)
		|| Result.ObstacleDepth < 0.0
		|| !IsCapsuleClear(Pawn, Pawn.GetActorLocation())) return false;
	if (const URpgCharacterMoverComponent* Mover = Character ? nullptr : Pawn.FindComponentByClass<URpgCharacterMoverComponent>())
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(&Pawn);
		if (Result.ActionType != 3 || !Mover->IsOnGround()
			|| !Mover->CanPlayAbilityRootMotion(Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr,
				SelectedMontage, Result.PlayRate, NAME_None, Result.StartTime)) return false;
	}
	const FVector Normal = Result.FrontLedgeNormal.GetSafeNormal2D();
	const FVector Feet = Pawn.GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
	const double Height = Result.FrontLedgeLocation.Z - Feet.Z;
	if (!FMath::IsFinite(Height) || Height < -3.0 || Height > 280.0
		|| FMath::Abs(Height - Result.ObstacleHeight) > 5.0
		|| FVector::Dist2D(Pawn.GetActorLocation(), Result.FrontLedgeLocation) > CandidateSearchDistance
		|| FVector::DotProduct(Pawn.GetActorLocation() - Result.FrontLedgeLocation, Normal) <= 0.0) return false;
	if (Result.ActionType == 2) return Character && ValidateVaultTraversal(*Character, Result);
	if (Result.ActionType == 1) return Character && ValidateHurdleTraversal(*Character, Result);
	FVector Landing;
	if (!GetMantleLandingLocation(Pawn, Result, Landing)) return false;
	const UWorld* World = Pawn.GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgGaspMantleRoute), false, &Pawn);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	FHitResult Hit;
	// Query-authored ledges must lie on this exact physical face, including when the capsule is above a low catch edge.
	FVector Face = Result.FrontLedgeLocation - FVector(0, 0, FMath::Min(10.0, FMath::Max(2.0, Height * 0.5)));
	if (!World->LineTraceSingleByChannel(Hit, Face + Normal * (Capsule->GetScaledCapsuleRadius() + 10),
		Face - Normal * 10.0, Channel, Params, Responses) || Hit.GetComponent() != Collider) return false;
	const auto ValidateLanding = [&](const FVector& FeetLocation)
	{
		// Validate the actual supported capsule route for both natural and conditional source handoffs.
		if (!World->LineTraceSingleByChannel(Hit, FeetLocation + FVector(0, 0, 10), FeetLocation - FVector(0, 0, 10), Channel, Params, Responses)
			|| Hit.GetComponent() != Collider || !RpgMantle::IsWalkable(Pawn, Hit)
			|| FMath::Abs(Hit.ImpactPoint.Z - FeetLocation.Z) > 5.0) return false;
		const FVector Destination(FeetLocation.X, FeetLocation.Y, Hit.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
		if (!IsCapsuleClear(Pawn, Destination)) return false;
		const double RouteZ = FMath::Max(Pawn.GetActorLocation().Z, Destination.Z) + PathClearance;
		const FVector Route[] = { Pawn.GetActorLocation(), FVector(Pawn.GetActorLocation().X, Pawn.GetActorLocation().Y, RouteZ),
			FVector(Destination.X, Destination.Y, RouteZ), Destination };
		for (int32 Index = 1; Index < UE_ARRAY_COUNT(Route); ++Index)
		{
			if (World->SweepSingleByChannel(Hit, Route[Index - 1], Route[Index], Capsule->GetComponentQuat(), Channel,
				RpgMantle::CapsuleShape(Pawn), Params, Responses)) return false;
		}
		return true;
	};
	if (!ValidateLanding(Landing)) return false;
	if (!Character)
	{
		const FRpgTraversalAnimationEntry* Entry = Query->AllowedMantleAnimations.FindByPredicate(
			[&Result](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Result.ChosenMontage; });
		if (Entry && Entry->MovementInputHandoffTime > 0.0f)
		{
			FVector EarlyLanding;
			if (!GetMantleLandingAtTime(Pawn, Result, Entry->MovementInputHandoffTime, EarlyLanding) || !ValidateLanding(EarlyLanding)) return false;
		}
	}
	return true;
}

bool URpgGameplayAbility_Mantle::ValidateVaultTraversal(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const
{
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	// Source Vault is a thin ledge without a back floor; Hurdle validates supported ground separately.
	if (Result.HasBackFloor || !ValidateThinObstacleFaces(Character, Result)) return false;
	TArray<FMotionWarpingWindowData> BackWindows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackLedgeWarpTargetName, BackWindows);
	const UMotionWarpingComponent* Warping = Character.FindComponentByClass<UMotionWarpingComponent>();
	if (!BackWindows.IsEmpty() && (!Warping || Warping->FindWarpTarget(BackLedgeWarpTargetName))) return false;
	const FVector BackNormal = Result.BackLedgeNormal.GetSafeNormal2D();
	const UWorld* World = Character.GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgGaspVaultRoute), false, &Character);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	FHitResult Hit;
	FVector ExitFeet;
	if (!GetVaultExitLocation(Character, Result, ExitFeet)
		|| FVector::DotProduct(ExitFeet - Result.BackLedgeLocation, BackNormal) <= 0.0
		|| FVector::Dist(ExitFeet, Result.BackLedgeLocation) > CandidateSearchDistance) return false;
	const FVector Destination = ExitFeet + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
	if (!IsCapsuleClear(Character, Destination)) return false;
	const double RouteZ = FMath::Max3(Character.GetActorLocation().Z, Destination.Z,
		FMath::Max(Result.FrontLedgeLocation.Z, Result.BackLedgeLocation.Z) + Capsule->GetScaledCapsuleHalfHeight()) + PathClearance;
	const FVector Route[] = { Character.GetActorLocation(), FVector(Character.GetActorLocation().X, Character.GetActorLocation().Y, RouteZ),
		FVector(Destination.X, Destination.Y, RouteZ), Destination };
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Route); ++Index)
	{
		if (World->SweepSingleByChannel(Hit, Route[Index - 1], Route[Index], Capsule->GetComponentQuat(), Channel,
			RpgMantle::CapsuleShape(Character), Params, Responses)) return false;
	}
	return true;
}

bool URpgGameplayAbility_Mantle::ValidateThinObstacleFaces(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const
{
	if (!Character.GetCharacterMovement()->IsMovingOnGround() || !Result.HasBackLedge
		|| Result.ObstacleHeight < 0.0 || Result.ObstacleHeight > 125.0 || Result.ObstacleDepth > 59.0
		|| Result.BackLedgeLocation.ContainsNaN() || Result.BackLedgeNormal.ContainsNaN()
		|| !Result.FrontLedgeNormal.IsNormalized() || !Result.BackLedgeNormal.IsNormalized()
		|| FMath::Abs(Result.BackLedgeNormal.Z) > 0.1
		|| FVector::DotProduct(Result.FrontLedgeNormal, Result.BackLedgeNormal) > -0.95) return false;
	const FVector FrontNormal = Result.FrontLedgeNormal.GetSafeNormal2D();
	const FVector BackNormal = Result.BackLedgeNormal.GetSafeNormal2D();
	const FVector Across = Result.BackLedgeLocation - Result.FrontLedgeLocation;
	const double Depth = FVector::DotProduct(Across, -FrontNormal);
	if (Depth <= 0.0 || Depth > 59.0 || FMath::Abs(Depth - Result.ObstacleDepth) > 5.0
		|| FMath::Abs(Across.Z) > 5.0 || (Across + FrontNormal * Depth).Size2D() > 5.0) return false;
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgThinObstacleFaces), false, &Character);
	for (const TPair<FVector, FVector>& Face : { TPair<FVector, FVector>(Result.FrontLedgeLocation, FrontNormal),
		TPair<FVector, FVector>(Result.BackLedgeLocation, BackNormal) })
	{
		FHitResult Hit;
		const FVector BelowLedge = Face.Key - FVector(0, 0, 10.0);
		if (!Character.GetWorld()->LineTraceSingleByChannel(Hit, BelowLedge + Face.Value * (Capsule->GetScaledCapsuleRadius() + 10.0),
			BelowLedge - Face.Value * 10.0, Capsule->GetCollisionObjectType(), Params,
			FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())) || Hit.GetComponent() != Result.HitComponent
			|| FVector::DotProduct(Hit.ImpactNormal, Face.Value) < 0.95
			|| FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - BelowLedge, Face.Value)) > 2.0) return false;
	}
	return true;
}

bool URpgGameplayAbility_Mantle::FindHurdleSupport(const ACharacter& Character, const FVector& Feet, FHitResult& OutHit) const
{
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgHurdleSupport), false, &Character);
	if (!Character.GetWorld()->LineTraceSingleByChannel(OutHit, Feet + FVector(0, 0, 10), Feet - FVector(0, 0, 10),
		Capsule->GetCollisionObjectType(), Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))) return false;
	const UPrimitiveComponent* Support = OutHit.GetComponent();
	return IsValid(Support) && Support->IsRegistered() && !Support->IsSimulatingPhysics() && Support->IsQueryCollisionEnabled()
		&& Character.GetCharacterMovement()->IsWalkable(OutHit) && FMath::Abs(OutHit.ImpactPoint.Z - Feet.Z) <= 5.0;
}

bool URpgGameplayAbility_Mantle::ValidateHurdleTraversal(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const
{
	if (!Result.HasBackFloor || !ValidateThinObstacleFaces(Character, Result) || Result.BackFloorLocation.ContainsNaN()
		|| !FMath::IsFinite(Result.BackLedgeHeight) || Result.BackLedgeHeight < 50.0
		|| Result.BackFloorLocation.Z >= Result.BackLedgeLocation.Z
		|| FMath::Abs(Result.BackLedgeLocation.Z - Result.BackFloorLocation.Z - Result.BackLedgeHeight) > 5.0) return false;
	const URpgTraversalQueryComponent* Query = Character.FindComponentByClass<URpgTraversalQueryComponent>();
	const FRpgTraversalAnimationEntry* Entry = Query->AllowedHurdleAnimations.FindByPredicate(
		[&Result](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Result.ChosenMontage; });
	if (!Entry) return false;
	const UMotionWarpingComponent* Warping = Character.FindComponentByClass<UMotionWarpingComponent>();
	if (!Warping || Warping->FindWarpTarget(BackFloorWarpTargetName)) return false;
	TArray<FMotionWarpingWindowData> BackWindows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackLedgeWarpTargetName, BackWindows);
	if (!BackWindows.IsEmpty() && Warping->FindWarpTarget(BackLedgeWarpTargetName)) return false;
	FHitResult QueriedSupport;
	if (!FindHurdleSupport(Character, Result.BackFloorLocation, QueriedSupport) || QueriedSupport.GetComponent() == Result.HitComponent) return false;
	FVector FloorTarget, Landing;
	if (!GetHurdleTargetsAndLanding(Character, Result, Entry->HandoffTime, FloorTarget, Landing)) return false;
	FVector EarlyLanding = Landing;
	if (Entry->MovementInputHandoffTime > 0.0f
		&& !GetHurdleTargetsAndLanding(Character, Result, Entry->MovementInputHandoffTime, FloorTarget, EarlyLanding)) return false;
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgHurdleRoute), false, &Character);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	FVector PreviousDestination = FVector::ZeroVector;
	for (const FVector& Feet : {FloorTarget, EarlyLanding, Landing})
	{
		FHitResult Support;
		if (FVector::DotProduct(Feet - Result.BackLedgeLocation, Result.BackLedgeNormal) <= 0.0
			|| FVector::Dist(Feet, Result.BackLedgeLocation) > CandidateSearchDistance
			|| !FindHurdleSupport(Character, Feet, Support) || Support.GetComponent() != QueriedSupport.GetComponent()) return false;
		const FVector Destination(Feet.X, Feet.Y, Support.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
		if (!IsCapsuleClear(Character, Destination)) return false;
		FHitResult Hit;
		if (!PreviousDestination.IsZero() && Character.GetWorld()->SweepSingleByChannel(Hit, PreviousDestination, Destination,
			Capsule->GetComponentQuat(), Channel, RpgMantle::CapsuleShape(Character), Params, Responses)) return false;
		PreviousDestination = Destination;
	}
	const double RouteZ = FMath::Max(Character.GetActorLocation().Z,
		FMath::Max(Result.FrontLedgeLocation.Z, Result.BackLedgeLocation.Z) + Capsule->GetScaledCapsuleHalfHeight()) + PathClearance;
	const FVector Destination = FloorTarget + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
	const FVector Route[] = {Character.GetActorLocation(), FVector(Character.GetActorLocation().X, Character.GetActorLocation().Y, RouteZ),
		FVector(Destination.X, Destination.Y, RouteZ), Destination};
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Route); ++Index)
	{
		FHitResult Hit;
		if (Character.GetWorld()->SweepSingleByChannel(Hit, Route[Index - 1], Route[Index], Capsule->GetComponentQuat(), Channel,
			RpgMantle::CapsuleShape(Character), Params, Responses)) return false;
	}
	return true;
}

bool URpgGameplayAbility_Mantle::FindTraversalCandidate(const ACharacter& Character, FRpgTraversalQueryResult& OutResult) const
{
	return FindTraversalCandidate(static_cast<const APawn&>(Character), OutResult);
}

bool URpgGameplayAbility_Mantle::FindTraversalCandidate(const APawn& Pawn, FRpgTraversalQueryResult& OutResult) const
{
	OutResult = FRpgTraversalQueryResult();
	if (!IsPawnReady(Pawn)) return false;
	URpgTraversalQueryComponent* Query = Pawn.FindComponentByClass<URpgTraversalQueryComponent>();
	return Query && Query->QueryTraversal(OutResult) && ValidateTraversal(Pawn, OutResult);
}

bool URpgGameplayAbility_Mantle::IsCapsuleClear(const APawn& Pawn, const FVector& Location) const
{
	const UCapsuleComponent* Capsule = RpgMantle::Capsule(Pawn);
	if (!Capsule || !Pawn.GetWorld() || Location.ContainsNaN()) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleClearance), false, &Pawn);
	return !Pawn.GetWorld()->OverlapBlockingTestByChannel(Location, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), RpgMantle::CapsuleShape(Pawn), Params,
		FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
}

bool URpgGameplayAbility_Mantle::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Pawn || !IsPawnReady(*Pawn)) return false;
	// The remote server validates the correlated proposal after receiving it; it never substitutes another entry.
	FRpgTraversalQueryResult Result;
	return !ActorInfo->IsLocallyControlled() && ActorInfo->IsNetAuthority() ? true : FindTraversalCandidate(*Pawn, Result);
}

void URpgGameplayAbility_Mantle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle activation: ability=%s pawn=%s authority=%d prediction=%s"), *GetPathName(),
		*GetPathNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), ActorInfo && ActorInfo->IsNetAuthority(),
		*ActivationInfo.GetActivationPredictionKey().ToString());
	bEnding = false;
	bReceivedTargetData = false;
	bGameplayCancellationRequested = false;
	ActiveMontageInstanceId = INDEX_NONE;
	MoverLeaseSequence = 0;
	FinalWarpEndTime = 0.0f;
	SourceHandoffTime = 0.0f;
	MovementInputHandoffTime = 0.0f;
	HurdleSupportComponent.Reset();
	ActiveActionType = 0;
	OwnedWarpTargets.Reset();
	ActivePawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	ActiveCharacter = Cast<ACharacter>(ActivePawn.Get());
	ActiveMover = ActivePawn.IsValid() && !ActiveCharacter.IsValid() ? ActivePawn->FindComponentByClass<URpgCharacterMoverComponent>() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ActivePawn.IsValid()) { CancelCurrentMantle(); return; }
	if (ActorInfo->IsLocallyControlled())
	{
		FRpgTraversalQueryResult Result;
		if (!FindTraversalCandidate(*ActivePawn, Result)) { CancelCurrentMantle(); return; }
		FRpgMantleTargetData* Proposal = new FRpgMantleTargetData();
		Proposal->HitComponent = Result.HitComponent;
		Proposal->Montage = Result.ChosenMontage;
		Proposal->StartTime = Result.StartTime;
		FGameplayAbilityTargetDataHandle Data(Proposal);
		bReceivedTargetData = true;
		FScopedPredictionWindow Prediction(ASC, !ActorInfo->IsNetAuthority());
		if (!ActorInfo->IsNetAuthority())
		{
			ASC->CallServerSetReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey(), Data, FGameplayTag(), ASC->ScopedPredictionKey);
		}
		// The owner just sampled this proposal. Repeating the stateful pose query here can reject that same sample.
		// Authority receives only animation/component identity and independently queries its own world in HandleTargetData.
		ResolveTraversal(Result);
	}
	else
	{
		TargetDataDelegate = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &ThisClass::HandleTargetData);
		GetWorld()->GetTimerManager().SetTimer(TimeoutHandle, this, &ThisClass::CancelCurrentMantle, FMath::Max(0.1f, TargetDataTimeout));
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
	}
}

void URpgGameplayAbility_Mantle::HandleTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (!IsActive() || bEnding || bReceivedTargetData) return;
	bReceivedTargetData = true;
	// Hold our own handle while consuming the ASC's cached entry and invoking gameplay callbacks.
	const FGameplayAbilityTargetDataHandle LocalData = Data;
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ActivePawn.IsValid() || CurrentActorInfo->AvatarActor.Get() != ActivePawn.Get())
	{
		CancelCurrentMantle(); return;
	}
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	if (LocalData.Num() != 1 || !LocalData.Get(0)
		|| LocalData.Get(0)->GetScriptStruct() != FRpgMantleTargetData::StaticStruct())
	{
		UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle rejected malformed target data: ability=%s count=%d"), *GetPathName(), LocalData.Num());
		CancelCurrentMantle(); return;
	}
	const FRpgMantleTargetData* Proposal = static_cast<const FRpgMantleTargetData*>(LocalData.Get(0));
	FRpgTraversalQueryResult Result;
	URpgTraversalQueryComponent* Query = ActivePawn.IsValid()
		? ActivePawn->FindComponentByClass<URpgTraversalQueryComponent>() : nullptr;
	const bool bQuerySucceeded = Query && Query->QueryTraversal(Result);
	if (!bQuerySucceeded || Result.HitComponent != Proposal->HitComponent)
	{
		UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle authority query failed: ability=%s query=%d sameCollider=%d proposedCollider=%s proposedMontage=%s proposedStart=%.6f"),
			*GetPathName(), bQuerySucceeded, Result.HitComponent == Proposal->HitComponent, *GetPathNameSafe(Proposal->HitComponent),
			*GetPathNameSafe(Proposal->Montage), Proposal->StartTime);
		LogTraversalRejection(TEXT("query_or_collider"), Result);
		CancelCurrentMantle(); return;
	}
	// Pose histories can select different feet on different machines. Only the allowed montage and entry time are proposed;
	// every ledge, surface and destination comes from the server's fresh query and native physical checks.
	Result.ChosenMontage = Proposal->Montage;
	Result.StartTime = Proposal->StartTime;
	Result.PlayRate = 1.0;
	ResolveTraversal(Result);
}

void URpgGameplayAbility_Mantle::ResolveTraversal(const FRpgTraversalQueryResult& Result)
{
	if (!IsActive() || bEnding) return;
	if (!ActivePawn.IsValid() || !CurrentActorInfo || CurrentActorInfo->AvatarActor.Get() != ActivePawn.Get()
		|| !ValidateTraversal(*ActivePawn, Result))
	{
		LogTraversalRejection(TEXT("geometry_or_animation"), Result); CancelCurrentMantle(); return;
	}
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		LogTraversalRejection(TEXT("commit"), Result); CancelCurrentMantle(); return;
	}
	// Authored costs/effects can synchronously kill, unpossess or cancel the avatar, including deferred EndAbility calls.
	if (!IsActive() || bEnding || !ActivePawn.IsValid()
		|| !CurrentActorInfo || CurrentActorInfo->AvatarActor.Get() != ActivePawn.Get()
		|| !ValidateTraversal(*ActivePawn, Result)) { LogTraversalRejection(TEXT("post_commit"), Result); CancelCurrentMantle(); return; }
	BeginMantle(Result);
}

void URpgGameplayAbility_Mantle::LogTraversalRejection(const TCHAR* Stage, const FRpgTraversalQueryResult& Result) const
{
	// Inspect only the existing result; diagnostics must not rerun the stateful designer query.
	if (!UE_LOG_ACTIVE(LogRpgAbilitySystem, Verbose)) return;
	const APawn* Pawn = ActivePawn.Get();
	const ACharacter* Character = ActiveCharacter.Get();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(Pawn);
	const UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const URpgTraversalQueryComponent* Query = Pawn ? Pawn->FindComponentByClass<URpgTraversalQueryComponent>() : nullptr;
	FVector Landing = FVector::ZeroVector;
	const bool bLandingValid = Pawn && (Result.ActionType == 2 ? Character && GetVaultExitLocation(*Character, Result, Landing)
		: Result.ActionType == 1 ? Character && GetHurdleLandingLocation(*Character, Result, Landing) : GetMantleLandingLocation(*Pawn, Result, Landing));
	UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle rejected: stage=%s ability=%s pawn=%s role=%d active=%d ending=%d ready=%d allowed=%d landingValid=%d landing=%s action=%u height=%.3f depth=%.3f front=%s normal=%s collider=%s montage=%s start=%.6f mode=%d speed=%.3f slotActive=%d animMontage=%s"),
		Stage, *GetPathName(), *GetPathNameSafe(Pawn), Pawn ? static_cast<int32>(Pawn->GetLocalRole()) : -1,
		IsActive(), bEnding, Pawn && IsPawnReady(*Pawn), Query && Pawn && Query->IsAnimationAllowed(*Pawn, Result),
		bLandingValid, *Landing.ToCompactString(), static_cast<uint32>(Result.ActionType), Result.ObstacleHeight, Result.ObstacleDepth,
		*Result.FrontLedgeLocation.ToCompactString(), *Result.FrontLedgeNormal.ToCompactString(), *GetPathNameSafe(Result.HitComponent),
		*GetPathNameSafe(Result.ChosenMontage), Result.StartTime, Movement ? static_cast<int32>(Movement->MovementMode) : -1,
		Pawn ? Pawn->GetVelocity().Size2D() : 0.0, Animation && Animation->IsSlotActive(TEXT("DefaultSlot")),
		*GetPathNameSafe(Animation ? Animation->GetCurrentActiveMontage() : nullptr));
}

void URpgGameplayAbility_Mantle::BeginMantle(const FRpgTraversalQueryResult& Result)
{
	if (ActiveMover.IsValid()) { BeginMoverMantle(Result); return; }
	ACharacter* Character = ActiveCharacter.Get();
	UMotionWarpingComponent* Warping = Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	URpgCharacterMovementComponent* Movement = Character ? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	TArray<FMotionWarpingWindowData> BackWindows;
	if (Result.ActionType == 2 || Result.ActionType == 1)
	{
		UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackLedgeWarpTargetName, BackWindows);
	}
	FVector HurdleFloorTarget;
	FHitResult HurdleSupport;
	const FRpgTraversalAnimationEntry* HurdleEntry = nullptr;
	if (Result.ActionType == 1 && Character)
	{
		const URpgTraversalQueryComponent* Query = Character->FindComponentByClass<URpgTraversalQueryComponent>();
		HurdleEntry = Query ? Query->AllowedHurdleAnimations.FindByPredicate(
			[&Result](const FRpgTraversalAnimationEntry& Entry) { return Entry.Montage == Result.ChosenMontage; }) : nullptr;
		if (!HurdleEntry || !GetHurdleTargetsAndLanding(*Character, Result, HurdleEntry->HandoffTime, HurdleFloorTarget, LandingAtActivation)
			|| !FindHurdleSupport(*Character, LandingAtActivation, HurdleSupport)) { CancelCurrentMantle(); return; }
		HurdleEarlyLanding = LandingAtActivation;
		if (HurdleEntry->MovementInputHandoffTime > 0.0f && !GetHurdleTargetsAndLanding(*Character, Result,
			HurdleEntry->MovementInputHandoffTime, HurdleFloorTarget, HurdleEarlyLanding)) { CancelCurrentMantle(); return; }
	}
	if (!Character || !Warping || Warping->FindWarpTarget(WarpTargetName) || !Movement
		|| !(Result.ActionType == 2 ? GetVaultExitLocation(*Character, Result, LandingAtActivation)
			: Result.ActionType == 1 ? HurdleEntry != nullptr : GetMantleLandingLocation(*Character, Result, LandingAtActivation))
		|| (!BackWindows.IsEmpty() && Warping->FindWarpTarget(BackLedgeWarpTargetName))
		|| (Result.ActionType == 1 && Warping->FindWarpTarget(BackFloorWarpTargetName))
		|| !Movement->BeginMantleCollisionIgnore(Result.HitComponent)) { CancelCurrentMantle(); return; }
	ActiveActionType = Result.ActionType;
	Montage = Result.ChosenMontage;
	StartTimeSeconds = Result.StartTime;
	PlayRate = Result.PlayRate;
	TArray<FMotionWarpingWindowData> Windows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, WarpTargetName, Windows);
	for (const FMotionWarpingWindowData& Window : Windows)
	{
		FinalWarpEndTime = FMath::Max(FinalWarpEndTime, Window.EndTime);
	}
	if (ActiveActionType == 2)
	{
		for (const FMotionWarpingWindowData& Window : BackWindows) FinalWarpEndTime = FMath::Max(FinalWarpEndTime, Window.EndTime);
		const URpgTraversalQueryComponent* Query = Character->FindComponentByClass<URpgTraversalQueryComponent>();
		const FRpgTraversalAnimationEntry* Entry = Query->AllowedVaultAnimations.FindByPredicate(
			[this](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Montage; });
		SourceHandoffTime = Entry->HandoffTime;
	}
	if (HurdleEntry)
	{
		SourceHandoffTime = HurdleEntry->HandoffTime;
		MovementInputHandoffTime = HurdleEntry->MovementInputHandoffTime;
		HurdleSupportComponent = HurdleSupport.GetComponent();
		HurdleSupportAtActivation = HurdleSupportComponent->GetComponentTransform();
		TArray<FMotionWarpingWindowData> FloorWindows;
		UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, BackFloorWarpTargetName, FloorWindows);
		for (const FMotionWarpingWindowData& Window : BackWindows) FinalWarpEndTime = FMath::Max(FinalWarpEndTime, Window.EndTime);
		for (const FMotionWarpingWindowData& Window : FloorWindows) FinalWarpEndTime = FMath::Max(FinalWarpEndTime, Window.EndTime);
	}
	ActiveWarping = Warping;
	IgnoredComponent = Result.HitComponent;
	ColliderAtActivation = IgnoredComponent->GetComponentTransform();
	EntryLocation = LastClearLocation = Character->GetActorLocation();
	// UE's animated SkewWarp branch measures from the cached mesh base, while the landing contract measures capsule feet.
	// Keep the gameplay mesh unchanged and compensate their vertical separation (e.g. half-height 90 + mesh Z -88 = 2 cm).
	const double MeshAboveCapsuleFeet = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		+ Character->GetBaseTranslationOffset().Z;
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName,
		Result.FrontLedgeLocation + FVector(0, 0, LedgeVerticalOffset + MeshAboveCapsuleFeet),
		(-Result.FrontLedgeNormal.GetSafeNormal2D()).Rotation());
	OwnedWarpTargets.Add(WarpTargetName);
	if (!BackWindows.IsEmpty())
	{
		// Unlike FrontLedge, source BackLedge has no hand-height offset and its translation-only warp does not rotate the pawn.
		Warping->AddOrUpdateWarpTargetFromLocationAndRotation(BackLedgeWarpTargetName,
			Result.BackLedgeLocation + FVector(0, 0, MeshAboveCapsuleFeet), FRotator::ZeroRotator);
		OwnedWarpTargets.Add(BackLedgeWarpTargetName);
	}
	if (HurdleEntry)
	{
		Warping->AddOrUpdateWarpTargetFromLocationAndRotation(BackFloorWarpTargetName,
			HurdleFloorTarget + FVector(0, 0, MeshAboveCapsuleFeet), FRotator::ZeroRotator);
		OwnedWarpTargets.Add(BackFloorWarpTargetName);
	}
	Character->StopJumping();
	// Preserve the sampled approach velocity and input, as source GASP does when handing movement to root motion.
	bOwnsMovement = true;
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Character->OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::HandleMovementUpdated);
	Character->MovementModeChangedDelegate.AddDynamic(this, &ThisClass::HandleMovementModeChanged);
	const float MaximumDuration = (Montage->GetPlayLength() - StartTimeSeconds) / PlayRate + 1.0f;
	GetWorld()->GetTimerManager().SetTimer(TimeoutHandle, this, &ThisClass::CancelCurrentMantle, MaximumDuration);
	// Concrete Blueprint content starts its GAS montage task only after the native movement lease and commit succeed.
	Super::ActivateAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr);
	if (bOwnsMovement && IsActive() && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
	{
		if (const FAnimMontageInstance* Instance = Character->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(Montage))
		{
			ActiveMontageInstanceId = Instance->GetInstanceID();
		}
	}
}

void URpgGameplayAbility_Mantle::BeginMoverMantle(const FRpgTraversalQueryResult& Result)
{
	APawn* Pawn = ActivePawn.Get();
	URpgCharacterMoverComponent* Mover = ActiveMover.Get();
	const UCapsuleComponent* Capsule = Pawn ? RpgMantle::Capsule(*Pawn) : nullptr;
	const URpgTraversalQueryComponent* Query = Pawn ? Pawn->FindComponentByClass<URpgTraversalQueryComponent>() : nullptr;
	const FRpgTraversalAnimationEntry* Entry = Query ? Query->AllowedMantleAnimations.FindByPredicate(
		[&Result](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Result.ChosenMontage && !Candidate.bAirborne; }) : nullptr;
	if (!Pawn || !Mover || !Capsule || !Entry || Result.ActionType != 3
		|| !GetMantleLandingLocation(*Pawn, Result, LandingAtActivation)) { CancelCurrentMantle(); return; }

	Montage = Result.ChosenMontage;
	StartTimeSeconds = Result.StartTime;
	PlayRate = Result.PlayRate;
	SourceHandoffTime = Entry->HandoffTime;
	MovementInputHandoffTime = Entry->MovementInputHandoffTime;
	ConditionalMantleLanding = LandingAtActivation;
	if (MovementInputHandoffTime > 0.0f
		&& !GetMantleLandingAtTime(*Pawn, Result, MovementInputHandoffTime, ConditionalMantleLanding)) { CancelCurrentMantle(); return; }
	TArray<FMotionWarpingWindowData> Windows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, WarpTargetName, Windows);
	for (const FMotionWarpingWindowData& Window : Windows) FinalWarpEndTime = FMath::Max(FinalWarpEndTime, Window.EndTime);
	IgnoredComponent = Result.HitComponent;
	ColliderAtActivation = Result.HitComponent->GetComponentTransform();
	EntryLocation = LastClearLocation = Pawn->GetActorLocation();
	ActiveActionType = 3;

	FRpgMoverTraversalRequest Request;
	Request.Collider = Result.HitComponent;
	Request.ColliderTransform = ColliderAtActivation;
	Request.EntryCapsuleLocation = EntryLocation;
	Request.LandingCapsuleLocation = LandingAtActivation + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
	// The adapter measures animated warp points from Mover's saved visual base, never its smoothed live mesh.
	const double VisualBaseAboveFeet = Capsule->GetScaledCapsuleHalfHeight() + Mover->GetBaseVisualComponentTransform().GetTranslation().Z;
	Request.FrontLedgeTarget = FTransform((-Result.FrontLedgeNormal.GetSafeNormal2D()).Rotation(),
		Result.FrontLedgeLocation + FVector(0, 0, LedgeVerticalOffset + VisualBaseAboveFeet));
	Request.WarpTargetName = WarpTargetName;
	Request.Montage = Montage;
	Request.StartTimeSeconds = StartTimeSeconds;
	Request.PlayRate = PlayRate;
	Request.HandoffTimeSeconds = SourceHandoffTime;
	MoverLeaseSequence = Mover->BeginTraversal(this, CurrentActivationInfo.GetActivationPredictionKey(), Request);
	if (!MoverLeaseSequence) { CancelCurrentMantle(); return; }
	bOwnsMovement = true;
	Mover->OnPostFinalize.AddDynamic(this, &ThisClass::HandleMoverPostFinalize);
	const float MaximumDuration = (Montage->GetPlayLength() - StartTimeSeconds) / PlayRate + 1.0f;
	GetWorld()->GetTimerManager().SetTimer(TimeoutHandle, this, &ThisClass::CancelCurrentMantle, MaximumDuration);
	// The existing ASC bridge adopts the Blueprint's real GAS montage once the predicted movement lease exists.
	Super::ActivateAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr);
	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(Pawn);
	if (bOwnsMovement && IsActive() && Mesh && Mesh->GetAnimInstance())
	{
		if (const FAnimMontageInstance* Instance = Mesh->GetAnimInstance()->GetActiveInstanceForMontage(Montage))
		{
			ActiveMontageInstanceId = Instance->GetInstanceID();
		}
	}
}

void URpgGameplayAbility_Mantle::HandleMoverPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (bEnding || !bOwnsMovement) return;
	APawn* Pawn = ActivePawn.Get();
	URpgCharacterMoverComponent* Mover = ActiveMover.Get();
	const UCapsuleComponent* Capsule = Pawn ? RpgMantle::Capsule(*Pawn) : nullptr;
	if (!Pawn || !Mover || !Capsule || !CurrentActorInfo || CurrentActorInfo->AvatarActor.Get() != Pawn
		|| !Mover->OwnsTraversalLease(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), MoverLeaseSequence)
		|| RpgMantle::IsIncapacitated(*Pawn) || !IgnoredComponent.IsValid() || !IgnoredComponent->IsRegistered()
		|| IgnoredComponent->IsSimulatingPhysics() || !IgnoredComponent->IsQueryCollisionEnabled()
		|| IgnoredComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block
		|| !IgnoredComponent->GetComponentTransform().Equals(ColliderAtActivation, 0.01))
	{
		CancelCurrentMantle(); return;
	}
	FHitResult Support;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMoverMantleActiveSupport), false, Pawn);
	for (const FVector& Feet : {LandingAtActivation, ConditionalMantleLanding})
	{
		if (!IsCapsuleClear(*Pawn, Feet + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2.0f))
			|| !Pawn->GetWorld()->LineTraceSingleByChannel(Support, Feet + FVector(0, 0, 10),
				Feet - FVector(0, 0, 10), Capsule->GetCollisionObjectType(), Params,
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))
			|| Support.GetComponent() != IgnoredComponent.Get() || !Mover->IsTraversalWalkable(Support)
			|| FMath::Abs(Support.ImpactPoint.Z - Feet.Z) > 5.0)
		{
			CancelCurrentMantle(); return;
		}
	}
	if (IsCapsuleClear(*Pawn, Pawn->GetActorLocation())) LastClearLocation = Pawn->GetActorLocation();
}

void URpgGameplayAbility_Mantle::HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	if (bEnding || !bOwnsMovement) return;
	ACharacter* Character = ActiveCharacter.Get();
	const bool bColliderValid = Character && IgnoredComponent.IsValid() && IgnoredComponent->IsRegistered()
		&& !IgnoredComponent->IsSimulatingPhysics() && IgnoredComponent->IsQueryCollisionEnabled()
		&& IgnoredComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block
		&& IgnoredComponent->GetComponentTransform().Equals(ColliderAtActivation, 0.01);
	// Hurdle uses the freshly traced support plane below, exactly as activation validation does. Its source animation
	// may retain a small vertical root offset at handoff; that offset is checked against the floor, not added to capsule clearance.
	const bool bEndpointClear = Character && (ActiveActionType == 1 || IsCapsuleClear(*Character,
		LandingAtActivation + FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			+ (ActiveActionType == 2 ? 0.0f : 2.0f))));
	const bool bIncapacitated = Character && RpgMantle::IsIncapacitated(*Character);
	if (!bColliderValid || !bEndpointClear || bIncapacitated)
	{
		UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Traversal active validation failed: pawn=%s action=%u colliderValid=%d endpointClear=%d incapacitated=%d landing=%s montage=%s"),
			*GetPathNameSafe(Character), static_cast<uint32>(ActiveActionType), bColliderValid, bEndpointClear, bIncapacitated,
			*LandingAtActivation.ToCompactString(), *GetPathNameSafe(Montage));
		CancelCurrentMantle(); return;
	}
	FHitResult Support;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleActiveSupport), false, Character);
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (ActiveActionType == 3 && (!Character->GetWorld()->LineTraceSingleByChannel(Support, LandingAtActivation + FVector(0, 0, 10),
		LandingAtActivation - FVector(0, 0, 10), Capsule->GetCollisionObjectType(), Params,
		FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))
		|| Support.GetComponent() != IgnoredComponent.Get() || !Character->GetCharacterMovement()->IsWalkable(Support)
		|| FMath::Abs(Support.ImpactPoint.Z - LandingAtActivation.Z) > 5.0))
	{
		CancelCurrentMantle(); return;
	}
	if (ActiveActionType == 1)
	{
		if (!HurdleSupportComponent.IsValid() || !HurdleSupportComponent->IsRegistered()
			|| !HurdleSupportComponent->GetComponentTransform().Equals(HurdleSupportAtActivation, 0.01))
		{
			UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Hurdle active support changed: pawn=%s support=%s landing=%s"),
				*GetPathNameSafe(Character), *GetPathNameSafe(HurdleSupportComponent.Get()), *LandingAtActivation.ToCompactString());
			CancelCurrentMantle(); return;
		}
		for (const FVector& Feet : {HurdleEarlyLanding, LandingAtActivation})
		{
			const bool bHasSupport = FindHurdleSupport(*Character, Feet, Support);
			const bool bSameSupport = bHasSupport && Support.GetComponent() == HurdleSupportComponent.Get();
			const FVector SupportedCapsule(Feet.X, Feet.Y, Support.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
			const bool bClear = bSameSupport && IsCapsuleClear(*Character, SupportedCapsule);
			if (!bClear)
			{
				UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Hurdle active landing rejected: pawn=%s supportValid=%d sameSupport=%d capsuleClear=%d feet=%s supportPoint=%s supportedCapsule=%s expectedSupport=%s actualSupport=%s montage=%s"),
					*GetPathNameSafe(Character), bHasSupport, bSameSupport, bClear, *Feet.ToCompactString(),
					*Support.ImpactPoint.ToCompactString(), *SupportedCapsule.ToCompactString(),
					*GetPathNameSafe(HurdleSupportComponent.Get()), *GetPathNameSafe(Support.GetComponent()), *GetPathNameSafe(Montage));
				CancelCurrentMantle(); return;
			}
		}
	}
	if (IsCapsuleClear(*Character, Character->GetActorLocation())) LastClearLocation = Character->GetActorLocation();
}

void URpgGameplayAbility_Mantle::HandleMovementModeChanged(ACharacter* Character, EMovementMode PreviousMode, uint8 PreviousCustomMode)
{
	if (!bEnding && bOwnsMovement && Character && Character->GetCharacterMovement()->MovementMode != MOVE_Flying)
	{
		CancelCurrentMantle();
	}
}

void URpgGameplayAbility_Mantle::CancelCurrentMantle()
{
	if (!IsActive() || bEnding) return;
	// Invalid geometry and timeouts must release traversal even if content temporarily blocked gameplay cancellation.
	// GAS EndAbility replicates a normal end regardless of bWasCancelled; CancelAbility sends the cancellation RPC.
	SetCanBeCanceled(true);
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void URpgGameplayAbility_Mantle::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	// GAS broadcasts cancellation before EndAbility; PlayMontageAndWait may synchronously turn it into OnInterrupted.
	if (IsActive() && CanBeCanceled()) bGameplayCancellationRequested = true;
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

bool URpgGameplayAbility_Mantle::RestoreSafeCapsuleLocation(ACharacter& Character) const
{
	if (IsCapsuleClear(Character, Character.GetActorLocation())) return true;
	if (const TOptional<FVector> Recovery = FindSafeRecoveryLocation(Character); Recovery.IsSet())
	{
		// CMC owns its actor transform; Mover applies this checked recovery through its simulation instead.
		Character.SetActorLocation(Recovery.GetValue(), false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}
	return false;
}

TOptional<FVector> URpgGameplayAbility_Mantle::FindSafeRecoveryLocation(const APawn& Pawn) const
{
	const UCapsuleComponent* Capsule = RpgMantle::Capsule(Pawn);
	if (!Capsule || !Pawn.GetWorld()) return {};
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleRecovery), false, &Pawn);
	// The capsule can be inside only the explicitly traversed surface. Recovery still sweeps every other obstacle.
	if (IgnoredComponent.IsValid()) Params.AddIgnoredComponent(IgnoredComponent.Get());
	for (const FVector& Candidate : {LastClearLocation, EntryLocation})
	{
		if (!IsCapsuleClear(Pawn, Candidate)) continue;
		FHitResult Hit;
		if (!Pawn.GetWorld()->SweepSingleByChannel(Hit, Pawn.GetActorLocation(), Candidate, Capsule->GetComponentQuat(),
			Capsule->GetCollisionObjectType(), RpgMantle::CapsuleShape(Pawn), Params,
			FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())))
		{
			// This is rollback along a checked route to a previously clear position, never a teleport to the ledge destination.
			return Candidate;
		}
	}
	return {};
}

float URpgGameplayAbility_Mantle::GetAdmissibleSourceHandoffTime() const
{
	if (ActiveMover.IsValid() && MovementInputHandoffTime > 0.0f)
	{
		// The original Mover animation interface exposes the post-simulation input vector as InputAcceleration.
		// Its notify compares that vector with zero at 0.1 tolerance; this is input intent, not root-motion velocity.
		const FCharacterDefaultInputs* Inputs = ActiveMover->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
		if (Inputs && !Inputs->GetMoveInput().Equals(FVector::ZeroVector, 0.1)) return MovementInputHandoffTime;
	}
	const UCharacterMovementComponent* Movement = ActiveCharacter.IsValid() ? ActiveCharacter->GetCharacterMovement() : nullptr;
	// Match the source notify's NotEqual_VectorVector(InputAcceleration, Zero, 0.1) predicate on each machine.
	return ActiveActionType == 1 && MovementInputHandoffTime > 0.0f && Movement
		&& !Movement->GetCurrentAcceleration().Equals(FVector::ZeroVector, 0.1)
		? MovementInputHandoffTime : SourceHandoffTime;
}

void URpgGameplayAbility_Mantle::CleanupMoverMovement(bool bWasCancelled)
{
	URpgCharacterMoverComponent* Mover = ActiveMover.Get();
	if (!Mover) return;
	Mover->OnPostFinalize.RemoveDynamic(this, &ThisClass::HandleMoverPostFinalize);
	APawn* Pawn = ActivePawn.Get();
	if (!Pawn || !bOwnsMovement || !Mover->OwnsTraversalLease(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), MoverLeaseSequence)) return;

	const bool bAlive = !RpgMantle::IsIncapacitated(*Pawn);
	const bool bCapsuleClear = IsCapsuleClear(*Pawn, Pawn->GetActorLocation());
	const TOptional<FVector> Recovery = bAlive && !bCapsuleClear ? FindSafeRecoveryLocation(*Pawn) : TOptional<FVector>();
	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(Pawn);
	UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* Instance = Animation ? Animation->GetMontageInstanceForID(ActiveMontageInstanceId) : nullptr;
	const float AdmissibleHandoffTime = GetAdmissibleSourceHandoffTime();
	// Natural completion clears the montage pointer before dispatching GAS OnCompleted, but deletes the instance
	// afterward. The captured instance ID still identifies our exact play throughout that terminal callback.
	const bool bReachedHandoff = Instance && (Instance->Montage == Montage || Instance->Montage == nullptr) && Instance->IsStopped()
		&& FinalWarpEndTime > 0.0f && Instance->GetPosition() >= FinalWarpEndTime
		&& AdmissibleHandoffTime > 0.0f && Instance->GetPosition() >= AdmissibleHandoffTime - 0.001f;
	bool bSupported = false;
	if (bAlive && bCapsuleClear && bReachedHandoff)
	{
		const UCapsuleComponent* Capsule = RpgMantle::Capsule(*Pawn);
		const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMoverMantleHandoff), false, Pawn);
		bSupported = Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0, 0, 5), Feet - FVector(0, 0, 10),
			Capsule->GetCollisionObjectType(), Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))
			&& Hit.GetComponent() == IgnoredComponent.Get() && Mover->IsTraversalWalkable(Hit);
	}
	const bool bPreserveMomentum = !bWasCancelled && !bGameplayCancellationRequested && bAlive && bCapsuleClear && bReachedHandoff && bSupported;
	// The simulation owns collision restoration, safe recovery and movement handoff. Death or a newer lease wins.
	Mover->EndTraversal(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), MoverLeaseSequence, bPreserveMomentum, Recovery);
}

void URpgGameplayAbility_Mantle::CleanupMovement(bool bWasCancelled)
{
	CleanupMoverMovement(bWasCancelled);
	ACharacter* Character = ActiveCharacter.Get();
	if (Character)
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
		Character->MovementModeChangedDelegate.RemoveDynamic(this, &ThisClass::HandleMovementModeChanged);
		if (bOwnsMovement)
		{
			const bool bCapsuleWasClear = IsCapsuleClear(*Character, Character->GetActorLocation());
			if (!bCapsuleWasClear) RestoreSafeCapsuleLocation(*Character);
			UAnimInstance* Animation = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
			const FAnimMontageInstance* Instance = Animation ? Animation->GetMontageInstanceForID(ActiveMontageInstanceId) : nullptr;
			const bool bFinishedWarp = Instance && Instance->Montage == Montage && Instance->IsStopped()
				&& FinalWarpEndTime > 0.0f && Instance->GetPosition() >= FinalWarpEndTime;
			const bool bReachedVaultHandoff = bFinishedWarp && ActiveActionType == 2 && SourceHandoffTime > 0.0f
				&& Instance->GetPosition() >= SourceHandoffTime - 0.001f;
			const float AdmissibleHandoffTime = GetAdmissibleSourceHandoffTime();
			const bool bReachedHurdleHandoff = bFinishedWarp && ActiveActionType == 1 && AdmissibleHandoffTime > 0.0f
				&& Instance->GetPosition() >= AdmissibleHandoffTime - 0.001f;
			if (URpgCharacterMovementComponent* RpgMovement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				RpgMovement->EndMantleCollisionIgnore(IgnoredComponent.Get());
			}
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			// Death/downed or a newer movement owner may have changed the mode before cancelling this ability.
			if (Movement->MovementMode == MOVE_Flying && !RpgMantle::IsIncapacitated(*Character))
			{
				FFindFloorResult Floor;
				Movement->FindFloor(Character->GetActorLocation(), Floor, false);
				const bool bSupported = Floor.IsWalkableFloor() && Floor.FloorDist <= UCharacterMovementComponent::MAX_FLOOR_DIST;
				// A source notify is reported as a montage interruption. Preserve an action's validated handoff without
				// preserving momentum for gameplay cancellation, interrupted warps or capsule recovery.
				const bool bSuccessfulVault = !bWasCancelled && !bGameplayCancellationRequested && bCapsuleWasClear && bReachedVaultHandoff;
				const bool bSuccessfulHurdle = !bWasCancelled && !bGameplayCancellationRequested && bCapsuleWasClear && bReachedHurdleHandoff
					&& bSupported && Floor.HitResult.GetComponent() == HurdleSupportComponent.Get();
				const bool bPreserveMomentum = bSuccessfulVault || bSuccessfulHurdle || (ActiveActionType == 3 && !bWasCancelled && !bGameplayCancellationRequested && bCapsuleWasClear
					&& bFinishedWarp && bSupported && Floor.HitResult.GetComponent() == IgnoredComponent.Get());
				UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle movement handoff: pawn=%s preserveMomentum=%d cancelled=%d gameplayCancel=%d capsuleClear=%d finishedWarp=%d supported=%d floorDistance=%.3f speed=%.3f acceleration=%.3f action=%u vaultHandoff=%d instance=%d montagePosition=%.6f previousPosition=%.6f sourceHandoff=%.6f finalWarpEnd=%.6f remoteEnded=%d hurdleHandoff=%d admissibleHandoff=%.6f sameHurdleSupport=%d"),
					*GetPathNameSafe(Character), bPreserveMomentum, bWasCancelled, bGameplayCancellationRequested, bCapsuleWasClear,
					bFinishedWarp, bSupported, Floor.FloorDist, Movement->Velocity.Size2D(), Movement->GetCurrentAcceleration().Size2D(),
					static_cast<uint32>(ActiveActionType), bReachedVaultHandoff, ActiveMontageInstanceId,
					Instance ? Instance->GetPosition() : -1.0f, Instance ? Instance->GetPreviousPosition() : -1.0f,
					SourceHandoffTime, FinalWarpEndTime, RemoteInstanceEnded, bReachedHurdleHandoff, AdmissibleHandoffTime,
					Floor.HitResult.GetComponent() == HurdleSupportComponent.Get());
				if (!bPreserveMomentum) Movement->StopMovementImmediately();
				// Source Vault hands an unsupported exit to CMC gravity; floor contact and landing remain ordinary movement behavior.
				Movement->SetMovementMode(bSuccessfulVault ? MOVE_Falling : bSupported ? MOVE_Walking : MOVE_Falling);
			}
		}
	}
	if (ActiveWarping.IsValid())
	{
		for (const FName Target : OwnedWarpTargets) ActiveWarping->RemoveWarpTarget(Target);
	}
	OwnedWarpTargets.Reset();
	ActivePawn.Reset();
	ActiveCharacter.Reset();
	ActiveMover.Reset();
	MoverLeaseSequence = 0;
	IgnoredComponent.Reset();
	HurdleSupportComponent.Reset();
	ActiveWarping.Reset();
	bOwnsMovement = false;
	ActiveMontageInstanceId = INDEX_NONE;
	FinalWarpEndTime = 0.0f;
	SourceHandoffTime = 0.0f;
	MovementInputHandoffTime = 0.0f;
	ActiveActionType = 0;
}

void URpgGameplayAbility_Mantle::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo) || bEnding) return;
	// A montage task can call ordinary EndAbility during GAS cancellation's OnInterrupted callback.
	// Preserve the original gameplay reason for both movement cleanup and GAS end observers.
	bWasCancelled |= bGameplayCancellationRequested;
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	if (!bWasCancelled && !bReplicateEndAbility && RemoteInstanceEnded && (ActiveActionType == 2 || ActiveActionType == 1 || ActiveMover.IsValid()) && bOwnsMovement
		&& ActorInfo && ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled() && ActivePawn.IsValid())
	{
		const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(ActivePawn.Get());
		UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
		const FAnimMontageInstance* Instance = Animation ? Animation->GetMontageInstanceForID(ActiveMontageInstanceId) : nullptr;
		const float AdmissibleHandoffTime = GetAdmissibleSourceHandoffTime();
		if (Instance && Instance->Montage == Montage && Instance->IsPlaying() && !Instance->IsStopped()
			&& AdmissibleHandoffTime > 0.0f && Instance->GetPosition() < AdmissibleHandoffTime)
		{
			// GAS's reliable normal-end RPC can overtake the owner's final movement/pose update. Do not stop the
			// authority's montage before its own source notify and then mistake that artificial stop for an interruption.
			// The existing montage task, geometry checks and duration timeout remain active; cancellation is never deferred.
			UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Traversal deferred remote normal end: pawn=%s instance=%d montagePosition=%.6f previousPosition=%.6f sourceHandoff=%.6f finalWarpEnd=%.6f action=%u admissibleHandoff=%.6f"),
				*GetPathNameSafe(ActivePawn.Get()), ActiveMontageInstanceId, Instance->GetPosition(), Instance->GetPreviousPosition(),
				SourceHandoffTime, FinalWarpEndTime, static_cast<uint32>(ActiveActionType), AdmissibleHandoffTime);
			return;
		}
	}
	UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle ending: ability=%s pawn=%s authority=%d prediction=%s cancelled=%d replicateEnd=%d ownsMovement=%d"),
		*GetPathName(), *GetPathNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), ActorInfo && ActorInfo->IsNetAuthority(),
		*ActivationInfo.GetActivationPredictionKey().ToString(), bWasCancelled, bReplicateEndAbility, bOwnsMovement);
	bEnding = true;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(TimeoutHandle);
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataDelegate);
		ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
		if (ASC->GetAnimatingAbility() == this) ASC->CurrentMontageStop();
	}
	TargetDataDelegate.Reset();
	// Stop our montage and release movement before EndAbility broadcasts can activate the next action.
	CleanupMovement(bWasCancelled);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
