#include "RpgGameplayAbility_Mantle.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/PackageMapClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Misc/MemStack.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
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
	FCollisionShape CapsuleShape(const ACharacter& Character)
	{
		return FCollisionShape::MakeCapsule(Character.GetCapsuleComponent()->GetScaledCapsuleRadius(),
			Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	bool IsIncapacitated(const ACharacter& Character)
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

bool URpgGameplayAbility_Mantle::GetMantleLandingLocation(const ACharacter& Character,
	const FRpgTraversalQueryResult& Result, FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	if (!Result.ChosenMontage || !Character.GetMesh() || !Character.GetMesh()->GetAnimInstance()
		|| Result.FrontLedgeLocation.ContainsNaN() || Result.FrontLedgeNormal.ContainsNaN()) return false;
	const URpgTraversalQueryComponent* Query = Character.FindComponentByClass<URpgTraversalQueryComponent>();
	const FRpgTraversalAnimationEntry* Entry = Query ? Query->AllowedMantleAnimations.FindByPredicate(
		[&Result](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == Result.ChosenMontage; }) : nullptr;
	if (!Entry || !FMath::IsFinite(Entry->HandoffTime) || Entry->HandoffTime <= Result.StartTime
		|| Entry->HandoffTime > Result.ChosenMontage->GetPlayLength() + 0.001f) return false;
	const float HandoffTime = FMath::Min(Entry->HandoffTime, Result.ChosenMontage->GetPlayLength());
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
		if (Character.GetMesh()->GetBoneIndex(Modifier->WarpPointAnimBoneName) == INDEX_NONE) return false;
		Offset = UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(Character, Result.ChosenMontage,
			Last->EndTime, Modifier->WarpPointAnimBoneName);
	}
	else if (Modifier->WarpPointAnimProvider == EWarpPointAnimProvider::Static)
	{
		Offset = UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(Character, Result.ChosenMontage,
			Last->EndTime, Modifier->WarpPointAnimTransform);
	}
	Offset.ScaleTranslation(Character.GetMesh()->GetComponentScale());
	const FQuat Facing = (-Result.FrontLedgeNormal.GetSafeNormal2D()).ToOrientationQuat();
	const FTransform RootAtWarp = Offset * FTransform(Facing, Result.FrontLedgeLocation + FVector(0, 0, LedgeVerticalOffset));
	// Running samples continue with a long locomotion tail after their forced blend-out notify releases traversal.
	// Validate the authored handoff, not the unused end of that root-motion tail.
	const FTransform Remaining = Modifier->bSubtractRemainingRootMotion ? FTransform::Identity
		: UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Result.ChosenMontage, Last->EndTime, HandoffTime);
	const FQuat ActorRotation = Modifier->AdditionalRotationOffset.Quaternion() * RootAtWarp.GetRotation();
	OutLocation = RootAtWarp.GetLocation() + (ActorRotation * Character.GetBaseRotationOffset()).RotateVector(Remaining.GetTranslation());
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

bool URpgGameplayAbility_Mantle::ValidateTraversal(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const
{
	URpgTraversalQueryComponent* Query = Character.FindComponentByClass<URpgTraversalQueryComponent>();
	UPrimitiveComponent* Collider = Result.HitComponent;
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	const UAnimMontage* SelectedMontage = Result.ChosenMontage;
	if (!IsCharacterReady(Character) || !Query || (Result.ActionType != 3 && Result.ActionType != 2) || !Result.HasFrontLedge
		|| !IsValid(Collider) || !Collider->IsRegistered() || Collider->GetWorld() != Character.GetWorld()
		|| Collider->IsSimulatingPhysics() || !Collider->IsQueryCollisionEnabled()
		|| Collider->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block
		|| !SelectedMontage || !SelectedMontage->HasRootMotion()
		|| SelectedMontage->GetSkeleton() != Character.GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()
		|| !Query->IsAnimationAllowed(Character, Result) || Result.FrontLedgeLocation.ContainsNaN()
		|| Result.FrontLedgeNormal.ContainsNaN() || Result.FrontLedgeNormal.GetSafeNormal2D().IsNearlyZero()
		|| FMath::Abs(Result.FrontLedgeNormal.Z) > 0.1 || !FMath::IsFinite(Result.ObstacleDepth)
		|| Result.ObstacleDepth < 0.0
		|| !IsCapsuleClear(Character, Character.GetActorLocation())) return false;
	const FVector Normal = Result.FrontLedgeNormal.GetSafeNormal2D();
	const FVector Feet = Character.GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
	const double Height = Result.FrontLedgeLocation.Z - Feet.Z;
	if (!FMath::IsFinite(Height) || Height < -3.0 || Height > 280.0
		|| FMath::Abs(Height - Result.ObstacleHeight) > 5.0
		|| FVector::Dist2D(Character.GetActorLocation(), Result.FrontLedgeLocation) > CandidateSearchDistance
		|| FVector::DotProduct(Character.GetActorLocation() - Result.FrontLedgeLocation, Normal) <= 0.0) return false;
	if (Result.ActionType == 2) return ValidateVaultTraversal(Character, Result);
	FVector Landing;
	if (!GetMantleLandingLocation(Character, Result, Landing)) return false;
	const UWorld* World = Character.GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgGaspMantleRoute), false, &Character);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	FHitResult Hit;
	// Query-authored ledges must lie on this exact physical face, including when the capsule is above a low catch edge.
	FVector Face = Result.FrontLedgeLocation - FVector(0, 0, FMath::Min(10.0, FMath::Max(2.0, Height * 0.5)));
	if (!World->LineTraceSingleByChannel(Hit, Face + Normal * (Capsule->GetScaledCapsuleRadius() + 10),
		Face - Normal * 10.0, Channel, Params, Responses) || Hit.GetComponent() != Collider) return false;
	// Support and capsule clearance are measured at the selected animation's authored handoff.
	if (!World->LineTraceSingleByChannel(Hit, Landing + FVector(0, 0, 10), Landing - FVector(0, 0, 10), Channel, Params, Responses)
		|| Hit.GetComponent() != Collider || !Character.GetCharacterMovement()->IsWalkable(Hit)
		|| FMath::Abs(Hit.ImpactPoint.Z - Landing.Z) > 5.0) return false;
	const FVector Destination(Landing.X, Landing.Y, Hit.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
	if (!IsCapsuleClear(Character, Destination)) return false;
	const double RouteZ = FMath::Max(Character.GetActorLocation().Z, Destination.Z) + PathClearance;
	const FVector Route[] = { Character.GetActorLocation(), FVector(Character.GetActorLocation().X, Character.GetActorLocation().Y, RouteZ),
		FVector(Destination.X, Destination.Y, RouteZ), Destination };
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Route); ++Index)
	{
		if (World->SweepSingleByChannel(Hit, Route[Index - 1], Route[Index], Capsule->GetComponentQuat(), Channel,
			RpgMantle::CapsuleShape(Character), Params, Responses)) return false;
	}
	return true;
}

bool URpgGameplayAbility_Mantle::ValidateVaultTraversal(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const
{
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	// Source Vault is a thin ledge with no back floor. A same-floor hurdle is a distinct, still unsupported action.
	if (!Character.GetCharacterMovement()->IsMovingOnGround() || !Result.HasBackLedge || Result.HasBackFloor
		|| Result.ObstacleHeight < 0.0 || Result.ObstacleHeight > 125.0 || Result.ObstacleDepth > 59.0
		|| Result.BackLedgeLocation.ContainsNaN() || Result.BackLedgeNormal.ContainsNaN()
		|| !Result.FrontLedgeNormal.IsNormalized() || !Result.BackLedgeNormal.IsNormalized()
		|| FMath::Abs(Result.BackLedgeNormal.Z) > 0.1
		|| FVector::DotProduct(Result.FrontLedgeNormal, Result.BackLedgeNormal) > -0.95) return false;
	TArray<FMotionWarpingWindowData> BackWindows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackLedgeWarpTargetName, BackWindows);
	const UMotionWarpingComponent* Warping = Character.FindComponentByClass<UMotionWarpingComponent>();
	if (!BackWindows.IsEmpty() && (!Warping || Warping->FindWarpTarget(BackLedgeWarpTargetName))) return false;
	const FVector FrontNormal = Result.FrontLedgeNormal.GetSafeNormal2D();
	const FVector BackNormal = Result.BackLedgeNormal.GetSafeNormal2D();
	const FVector Across = Result.BackLedgeLocation - Result.FrontLedgeLocation;
	const double Depth = FVector::DotProduct(Across, -FrontNormal);
	if (Depth <= 0.0 || Depth > 59.0 || FMath::Abs(Depth - Result.ObstacleDepth) > 5.0
		|| FMath::Abs(Across.Z) > 5.0 || (Across + FrontNormal * Depth).Size2D() > 5.0) return false;
	const UWorld* World = Character.GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgGaspVaultRoute), false, &Character);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	FHitResult Hit;
	for (const TPair<FVector, FVector>& Face : { TPair<FVector, FVector>(Result.FrontLedgeLocation, FrontNormal),
		TPair<FVector, FVector>(Result.BackLedgeLocation, BackNormal) })
	{
		const FVector BelowLedge = Face.Key - FVector(0, 0, 10.0);
		if (!World->LineTraceSingleByChannel(Hit, BelowLedge + Face.Value * (Capsule->GetScaledCapsuleRadius() + 10.0),
			BelowLedge - Face.Value * 10.0, Channel, Params, Responses) || Hit.GetComponent() != Result.HitComponent
			|| FVector::DotProduct(Hit.ImpactNormal, Face.Value) < 0.95
			|| FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - BelowLedge, Face.Value)) > 2.0) return false;
	}
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

bool URpgGameplayAbility_Mantle::FindTraversalCandidate(const ACharacter& Character, FRpgTraversalQueryResult& OutResult) const
{
	OutResult = FRpgTraversalQueryResult();
	if (!IsCharacterReady(Character)) return false;
	URpgTraversalQueryComponent* Query = Character.FindComponentByClass<URpgTraversalQueryComponent>();
	return Query && Query->QueryTraversal(OutResult) && ValidateTraversal(Character, OutResult);
}

bool URpgGameplayAbility_Mantle::IsCapsuleClear(const ACharacter& Character, const FVector& Location) const
{
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleClearance), false, &Character);
	return !Character.GetWorld()->OverlapBlockingTestByChannel(Location, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), RpgMantle::CapsuleShape(Character), Params,
		FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
}

bool URpgGameplayAbility_Mantle::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character || !IsCharacterReady(*Character)) return false;
	// The remote server validates the correlated proposal after receiving it; it never substitutes another entry.
	FRpgTraversalQueryResult Result;
	return !ActorInfo->IsLocallyControlled() && ActorInfo->IsNetAuthority() ? true : FindTraversalCandidate(*Character, Result);
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
	FinalWarpEndTime = 0.0f;
	SourceHandoffTime = 0.0f;
	ActiveActionType = 0;
	OwnedWarpTargets.Reset();
	ActiveCharacter = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ActiveCharacter.IsValid()) { CancelCurrentMantle(); return; }
	if (ActorInfo->IsLocallyControlled())
	{
		FRpgTraversalQueryResult Result;
		if (!FindTraversalCandidate(*ActiveCharacter, Result)) { CancelCurrentMantle(); return; }
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
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	if (LocalData.Num() != 1 || !LocalData.Get(0)
		|| LocalData.Get(0)->GetScriptStruct() != FRpgMantleTargetData::StaticStruct())
	{
		UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle rejected malformed target data: ability=%s count=%d"), *GetPathName(), LocalData.Num());
		CancelCurrentMantle(); return;
	}
	const FRpgMantleTargetData* Proposal = static_cast<const FRpgMantleTargetData*>(LocalData.Get(0));
	FRpgTraversalQueryResult Result;
	URpgTraversalQueryComponent* Query = ActiveCharacter.IsValid()
		? ActiveCharacter->FindComponentByClass<URpgTraversalQueryComponent>() : nullptr;
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
	if (!ActiveCharacter.IsValid() || !ValidateTraversal(*ActiveCharacter, Result))
	{
		LogTraversalRejection(TEXT("geometry_or_animation"), Result); CancelCurrentMantle(); return;
	}
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		LogTraversalRejection(TEXT("commit"), Result); CancelCurrentMantle(); return;
	}
	// Authored costs/effects can synchronously kill, unpossess or cancel the avatar, including deferred EndAbility calls.
	if (!IsActive() || bEnding || !ActiveCharacter.IsValid()
		|| !ValidateTraversal(*ActiveCharacter, Result)) { LogTraversalRejection(TEXT("post_commit"), Result); CancelCurrentMantle(); return; }
	BeginMantle(Result);
}

void URpgGameplayAbility_Mantle::LogTraversalRejection(const TCHAR* Stage, const FRpgTraversalQueryResult& Result) const
{
	// Inspect only the existing result; diagnostics must not rerun the stateful designer query.
	if (!UE_LOG_ACTIVE(LogRpgAbilitySystem, Verbose)) return;
	const ACharacter* Character = ActiveCharacter.Get();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const UAnimInstance* Animation = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	const URpgTraversalQueryComponent* Query = Character ? Character->FindComponentByClass<URpgTraversalQueryComponent>() : nullptr;
	FVector Landing = FVector::ZeroVector;
	const bool bLandingValid = Character && (Result.ActionType == 2 ? GetVaultExitLocation(*Character, Result, Landing)
		: GetMantleLandingLocation(*Character, Result, Landing));
	UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle rejected: stage=%s ability=%s pawn=%s role=%d active=%d ending=%d ready=%d allowed=%d landingValid=%d landing=%s action=%u height=%.3f depth=%.3f front=%s normal=%s collider=%s montage=%s start=%.6f mode=%d speed=%.3f slotActive=%d animMontage=%s"),
		Stage, *GetPathName(), *GetPathNameSafe(Character), Character ? static_cast<int32>(Character->GetLocalRole()) : -1,
		IsActive(), bEnding, Character && IsCharacterReady(*Character), Query && Character && Query->IsAnimationAllowed(*Character, Result),
		bLandingValid, *Landing.ToCompactString(), static_cast<uint32>(Result.ActionType), Result.ObstacleHeight, Result.ObstacleDepth,
		*Result.FrontLedgeLocation.ToCompactString(), *Result.FrontLedgeNormal.ToCompactString(), *GetPathNameSafe(Result.HitComponent),
		*GetPathNameSafe(Result.ChosenMontage), Result.StartTime, Movement ? static_cast<int32>(Movement->MovementMode) : -1,
		Character ? Character->GetVelocity().Size2D() : 0.0, Animation && Animation->IsSlotActive(TEXT("DefaultSlot")),
		*GetPathNameSafe(Animation ? Animation->GetCurrentActiveMontage() : nullptr));
}

void URpgGameplayAbility_Mantle::BeginMantle(const FRpgTraversalQueryResult& Result)
{
	ACharacter* Character = ActiveCharacter.Get();
	UMotionWarpingComponent* Warping = Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	URpgCharacterMovementComponent* Movement = Character ? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	TArray<FMotionWarpingWindowData> BackWindows;
	if (Result.ActionType == 2)
	{
		UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Result.ChosenMontage, BackLedgeWarpTargetName, BackWindows);
	}
	if (!Character || !Warping || Warping->FindWarpTarget(WarpTargetName) || !Movement
		|| !(Result.ActionType == 2 ? GetVaultExitLocation(*Character, Result, LandingAtActivation)
			: GetMantleLandingLocation(*Character, Result, LandingAtActivation))
		|| (!BackWindows.IsEmpty() && Warping->FindWarpTarget(BackLedgeWarpTargetName))
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

void URpgGameplayAbility_Mantle::HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	if (bEnding || !bOwnsMovement) return;
	ACharacter* Character = ActiveCharacter.Get();
	if (!Character || !IgnoredComponent.IsValid() || !IgnoredComponent->IsRegistered()
		|| IgnoredComponent->IsSimulatingPhysics() || !IgnoredComponent->IsQueryCollisionEnabled()
		|| IgnoredComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block
		|| !IgnoredComponent->GetComponentTransform().Equals(ColliderAtActivation, 0.01)
		|| !IsCapsuleClear(*Character, LandingAtActivation + FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			+ (ActiveActionType == 2 ? 0.0f : 2.0f)))
		|| RpgMantle::IsIncapacitated(*Character))
	{
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
	if (IsActive() && !bEnding) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
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
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleRecovery), false, &Character);
	// The capsule can be inside only the explicitly traversed surface. Recovery still sweeps every other obstacle.
	if (IgnoredComponent.IsValid()) Params.AddIgnoredComponent(IgnoredComponent.Get());
	for (const FVector& Candidate : {LastClearLocation, EntryLocation})
	{
		if (!IsCapsuleClear(Character, Candidate)) continue;
		FHitResult Hit;
		if (!Character.GetWorld()->SweepSingleByChannel(Hit, Character.GetActorLocation(), Candidate, Capsule->GetComponentQuat(),
			Capsule->GetCollisionObjectType(), RpgMantle::CapsuleShape(Character), Params,
			FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())))
		{
			// This is rollback along a checked route to a previously clear position, never a teleport to the ledge destination.
			Character.SetActorLocation(Candidate, false, nullptr, ETeleportType::TeleportPhysics);
			return true;
		}
	}
	return false;
}

void URpgGameplayAbility_Mantle::CleanupMovement(bool bWasCancelled)
{
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
				const bool bPreserveMomentum = bSuccessfulVault || (ActiveActionType == 3 && !bWasCancelled && !bGameplayCancellationRequested && bCapsuleWasClear
					&& bFinishedWarp && bSupported && Floor.HitResult.GetComponent() == IgnoredComponent.Get());
				UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Mantle movement handoff: pawn=%s preserveMomentum=%d cancelled=%d gameplayCancel=%d capsuleClear=%d finishedWarp=%d supported=%d floorDistance=%.3f speed=%.3f acceleration=%.3f action=%u vaultHandoff=%d instance=%d montagePosition=%.6f previousPosition=%.6f sourceHandoff=%.6f finalWarpEnd=%.6f remoteEnded=%d"),
					*GetPathNameSafe(Character), bPreserveMomentum, bWasCancelled, bGameplayCancellationRequested, bCapsuleWasClear,
					bFinishedWarp, bSupported, Floor.FloorDist, Movement->Velocity.Size2D(), Movement->GetCurrentAcceleration().Size2D(),
					static_cast<uint32>(ActiveActionType), bReachedVaultHandoff, ActiveMontageInstanceId,
					Instance ? Instance->GetPosition() : -1.0f, Instance ? Instance->GetPreviousPosition() : -1.0f,
					SourceHandoffTime, FinalWarpEndTime, RemoteInstanceEnded);
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
	ActiveCharacter.Reset();
	IgnoredComponent.Reset();
	ActiveWarping.Reset();
	bOwnsMovement = false;
	ActiveMontageInstanceId = INDEX_NONE;
	FinalWarpEndTime = 0.0f;
	SourceHandoffTime = 0.0f;
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
	if (!bWasCancelled && !bReplicateEndAbility && RemoteInstanceEnded && ActiveActionType == 2 && bOwnsMovement
		&& ActorInfo && ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled() && ActiveCharacter.IsValid())
	{
		const USkeletalMeshComponent* Mesh = ActiveCharacter->GetMesh();
		UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
		const FAnimMontageInstance* Instance = Animation ? Animation->GetMontageInstanceForID(ActiveMontageInstanceId) : nullptr;
		if (Instance && Instance->Montage == Montage && Instance->IsPlaying() && !Instance->IsStopped()
			&& SourceHandoffTime > 0.0f && Instance->GetPosition() < SourceHandoffTime)
		{
			// GAS's reliable normal-end RPC can overtake the owner's final movement/pose update. Do not stop the
			// authority's montage before its own source notify and then mistake that artificial stop for an interruption.
			// The existing montage task, geometry checks and duration timeout remain active; cancellation is never deferred.
			UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Vault deferred remote normal end: pawn=%s instance=%d montagePosition=%.6f previousPosition=%.6f sourceHandoff=%.6f finalWarpEnd=%.6f"),
				*GetPathNameSafe(ActiveCharacter.Get()), ActiveMontageInstanceId, Instance->GetPosition(), Instance->GetPreviousPosition(),
				SourceHandoffTime, FinalWarpEndTime);
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
