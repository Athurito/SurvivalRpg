// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgAnimInstance.h"
#include "RpgAnimationPlaybackRuntime.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationWarpingLibrary.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/MovementBaseInterface.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Animation/RpgCombatAnimationProfileProviderComponent.h"
#include "SurvivalRpg/Animation/RpgGaspPostureRuntime.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAnimInstance)

namespace
{
void ResetPoseSearchTrajectoryState(FRpgAnimInstanceProxy& Proxy)
{
	Proxy.RawTransformTrajectory.Samples.Reset();
	Proxy.TransformTrajectory.Samples.Reset();
	Proxy.TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
	Proxy.DesiredControllerYawLastUpdate = 0.0f;
}

FTransform GetPresentationActorTransform(const ARpgCharacter& Character)
{
	const bool bUsesNetworkSmoothedPresentation =
		Character.GetLocalRole() == ROLE_SimulatedProxy ||
		Character.GetRemoteRole() == ROLE_AutonomousProxy;
	const USkeletalMeshComponent* Mesh = Character.GetMesh();
	if (!bUsesNetworkSmoothedPresentation || !IsValid(Mesh))
	{
		return Character.GetActorTransform();
	}

	// Network smoothing lives on the mesh relative to the capsule. Remove only the
	// authored mesh offset so trajectory, local motion, and turn-in-place all observe
	// the same smoothed presentation frame without feeding skeletal pose into the result.
	FQuat PresentationRotation =
		Mesh->GetComponentQuat() * Character.GetBaseRotationOffset().Inverse();
	PresentationRotation.Normalize();
	const FVector PresentationLocation =
		Mesh->GetComponentLocation() -
		PresentationRotation.RotateVector(Character.GetBaseTranslationOffset());
	return FTransform(PresentationRotation, PresentationLocation);
}

struct FRpgFootPlacementTraceResult
{
	FVector GroundPointWorld = FVector::ZeroVector;
	FVector GroundNormalWorld = FVector::UpVector;
	FTransform HitComponentTransform = FTransform::Identity;
	uint32 HitComponentId = 0;
	float DistanceToGround = 0.0f;
	bool bWalkable = false;
};

void ResetFootPlacementLegState(FRpgAnimInstanceProxy::FFootPlacementLegState& State)
{
	State = FRpgAnimInstanceProxy::FFootPlacementLegState();
}

void ResetFootPlacementState(FRpgAnimInstanceProxy& Proxy)
{
	ResetFootPlacementLegState(Proxy.FootPlacementLegStates[0]);
	ResetFootPlacementLegState(Proxy.FootPlacementLegStates[1]);
	Proxy.FootPlacementAlpha = 0.0f;
}

void ResetFootPlacementInitializationState(FRpgAnimInstanceProxy& Proxy)
{
	ResetFootPlacementState(Proxy);
	Proxy.FootPlacementSnapshot = FRpgFootPlacementSnapshot();
	Proxy.PreviousFootPlacementComponentTransform = FTransform::Identity;
	Proxy.PreviousMovementBaseTransform = FTransform::Identity;
	Proxy.PreviousMovementBaseId = 0;
	Proxy.bHasPreviousFootPlacementComponentTransform = false;
	Proxy.bHasPreviousMovementBaseTransform = false;
	Proxy.bPreviousFootPlacementSourceEligible = false;
}

FGameplayTagContainer GatherCombatAnimationEquipmentTraits(
	const ARpgCharacter& Character)
{
	FGameplayTagContainer Result;
	const URpgEquipmentManagerComponent* EquipmentManager =
		Character.GetEquipmentManagerComponent();
	if (!EquipmentManager)
	{
		return Result;
	}

	static constexpr ERpgEquipmentSlot HandSlots[] = {
		ERpgEquipmentSlot::MainHand,
		ERpgEquipmentSlot::OffHand,
	};
	for (const ERpgEquipmentSlot Slot : HandSlots)
	{
		const URpgWeaponInstance* Weapon = Cast<URpgWeaponInstance>(
			EquipmentManager->GetEquipmentInstanceInSlot(Slot));
		if (Weapon)
		{
			Result.AppendTags(Weapon->GetEquipmentTraitTags());
		}
	}
	return Result;
}

void PublishCombatAnimationSnapshot(FRpgAnimInstanceProxy& Proxy)
{
	const FRpgResolvedCombatAnimationProfile& Active =
		Proxy.ActiveCombatAnimationProfile;
	Proxy.CombatEquippedUpperBodyAnimation = Active.EquippedUpperBodyAnimation;
	Proxy.CombatReadyUpperBodyAnimation = Active.CombatReadyUpperBodyAnimation;
	Proxy.CombatAnimationProfileName = Active.ProfileName;
	Proxy.CombatModeBlendTime = Active.CombatModeBlendTime;
	Proxy.bCombatAnimationProfileFallback = Active.bIsFallback;
}

void ResetCombatAnimationSnapshot(FRpgAnimInstanceProxy& Proxy)
{
	Proxy.ActiveCombatAnimationProfile = FRpgResolvedCombatAnimationProfile();
	Proxy.ActiveCombatAnimationProfile.ProfileName = TEXT("Unarmed");
	Proxy.PendingCombatAnimationProfile = FRpgResolvedCombatAnimationProfile();
	Proxy.bHasPendingCombatAnimationProfile = false;
	Proxy.CombatAnimationOverlayAlpha = 0.0f;
	Proxy.bCombatAnimationReady = false;
	PublishCombatAnimationSnapshot(Proxy);
}

float AdvanceLinearBlend(
	float CurrentAlpha,
	float TargetAlpha,
	float Duration,
	float DeltaSeconds)
{
	if (!FMath::IsFinite(Duration) || Duration <= UE_KINDA_SMALL_NUMBER)
	{
		return TargetAlpha;
	}
	return FMath::FInterpConstantTo(
		CurrentAlpha,
		TargetAlpha,
		FMath::Max(0.0f, DeltaSeconds),
		1.0f / Duration);
}

void AdvanceCombatAnimationSnapshot(
	FRpgAnimInstanceProxy& Proxy,
	const FRpgResolvedCombatAnimationProfile& DesiredProfile,
	bool bCombatReady,
	float DeltaSeconds)
{
	Proxy.bCombatAnimationReady = bCombatReady;
	if (Proxy.ActiveCombatAnimationProfile.IsSameProfile(DesiredProfile))
	{
		Proxy.bHasPendingCombatAnimationProfile = false;
	}
	else
	{
		Proxy.PendingCombatAnimationProfile = DesiredProfile;
		Proxy.bHasPendingCombatAnimationProfile = true;
	}

	if (Proxy.bHasPendingCombatAnimationProfile)
	{
		Proxy.CombatAnimationOverlayAlpha = AdvanceLinearBlend(
			Proxy.CombatAnimationOverlayAlpha,
			0.0f,
			Proxy.ActiveCombatAnimationProfile.EquipBlendOutTime,
			DeltaSeconds);
		if (Proxy.CombatAnimationOverlayAlpha <= UE_KINDA_SMALL_NUMBER)
		{
			Proxy.CombatAnimationOverlayAlpha = 0.0f;
			Proxy.ActiveCombatAnimationProfile =
				Proxy.PendingCombatAnimationProfile;
			Proxy.PendingCombatAnimationProfile =
				FRpgResolvedCombatAnimationProfile();
			Proxy.bHasPendingCombatAnimationProfile = false;
		}
	}

	if (!Proxy.bHasPendingCombatAnimationProfile)
	{
		const float TargetAlpha =
			Proxy.ActiveCombatAnimationProfile.HasOverlay() ? 1.0f : 0.0f;
		const float BlendDuration = TargetAlpha > Proxy.CombatAnimationOverlayAlpha
			? Proxy.ActiveCombatAnimationProfile.EquipBlendInTime
			: Proxy.ActiveCombatAnimationProfile.EquipBlendOutTime;
		Proxy.CombatAnimationOverlayAlpha = AdvanceLinearBlend(
			Proxy.CombatAnimationOverlayAlpha,
			TargetAlpha,
			BlendDuration,
			DeltaSeconds);
	}
	PublishCombatAnimationSnapshot(Proxy);
}

FRpgFootPlacementTraceResult TraceFootGround(
	const ARpgCharacter& Character,
	const URpgCharacterMovementComponent& MovementComponent,
	const FRpgFootPlacementSettings& Settings,
	const FVector& BallLocationWorld)
{
	check(IsInGameThread());
	FRpgFootPlacementTraceResult Result;
	UWorld* World = Character.GetWorld();
	if (!World)
	{
		return Result;
	}

	const FVector TraceStart = BallLocationWorld + FVector::UpVector * Settings.TraceStartHeight;
	const FVector TraceEnd = BallLocationWorld - FVector::UpVector * Settings.TraceEndDepth;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgFootPlacement), true, &Character);
	QueryParams.AddIgnoredActor(&Character);
	FHitResult HitResult;
	const bool bHit = World->SweepSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(FMath::Max(Settings.SweepRadius, 0.0f)),
		QueryParams);
	if (!bHit || !HitResult.bBlockingHit || !MovementComponent.IsWalkable(HitResult))
	{
		return Result;
	}

	Result.GroundPointWorld = HitResult.ImpactPoint;
	Result.GroundNormalWorld = HitResult.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	Result.DistanceToGround = FVector::DotProduct(
		BallLocationWorld - Result.GroundPointWorld,
		Result.GroundNormalWorld);
	if (const UPrimitiveComponent* HitComponent = HitResult.GetComponent())
	{
		Result.HitComponentId = HitComponent->GetUniqueID();
		Result.HitComponentTransform = HitComponent->GetComponentTransform();
	}
	Result.bWalkable = true;
	return Result;
}

bool TryGetMovementBaseSnapshot(
	const ARpgCharacter& Character,
	uint32& OutBaseId,
	FTransform& OutBaseTransform)
{
	check(IsInGameThread());
	OutBaseId = 0;
	OutBaseTransform = FTransform::Identity;
	const FMovementBaseInterfaceData* BaseData = Character.GetMovementBaseInterfaceData();
	if (!BaseData || !BaseData->IsValid())
	{
		return false;
	}

	UObject* BaseObject = BaseData->GetMovementBaseObject();
	FVector BaseLocation = FVector::ZeroVector;
	FQuat BaseRotation = FQuat::Identity;
	if (!BaseObject || !MovementBaseUtility::GetMovementBaseTransform(
		BaseData,
		Character.GetBasedMovement().BoneName,
		BaseLocation,
		BaseRotation))
	{
		return false;
	}

	OutBaseId = BaseObject->GetUniqueID();
	OutBaseTransform = FTransform(BaseRotation, BaseLocation);
	return true;
}

void UpdateFootPlacementLeg(
	FRpgAnimInstanceProxy::FFootPlacementLegState& State,
	FRpgFootPlacementLegSnapshot& Snapshot,
	const FRpgFootPlacementLegDefinition& Definition,
	const FRpgFootPlacementSettings& Settings,
	const URpgAnimInstance& AnimInstance,
	const ARpgCharacter& Character,
	const URpgCharacterMovementComponent& MovementComponent,
	const USkeletalMeshComponent& MeshComponent,
	const FTransform& ComponentToWorld,
	uint32 MovementBaseId,
	const FTransform& PreviousBaseTransform,
	const FTransform& CurrentBaseTransform,
	bool bHasBaseDelta,
	float DeltaSeconds)
{
	check(IsInGameThread());
	if (Definition.FKFootBone.IsNone() || Definition.IKFootBone.IsNone() || Definition.BallBone.IsNone() ||
		!MeshComponent.DoesSocketExist(Definition.FKFootBone) ||
		!MeshComponent.DoesSocketExist(Definition.IKFootBone) ||
		!MeshComponent.DoesSocketExist(Definition.BallBone))
	{
		ResetFootPlacementLegState(State);
		return;
	}

	const FTransform FKFootTransformWorld = MeshComponent.GetSocketTransform(
		Definition.FKFootBone,
		RTS_World);
	const FTransform AuthoredBallTransformWorld = MeshComponent.GetSocketTransform(
		Definition.BallBone,
		RTS_World);
	float ContactCurveValue = 0.0f;
	const bool bHasSpeedCurve =
		!Definition.SpeedCurveName.IsNone() &&
		AnimInstance.GetCurveValue(Definition.SpeedCurveName, ContactCurveValue);
	const float FootSpeed = bHasSpeedCurve
		? RpgFootPlacement::ConvertContactCurveToSpeed(ContactCurveValue)
		: MAX_flt;
	// Contact is the primary authored plant/release signal. Socket transforms are the previous
	// completed pose (including this cosmetic tail), so geometric bounds are defensive limits,
	// not a replacement for the curated contact curves.
	const FRpgFootPlacementTraceResult TraceResult = TraceFootGround(
		Character,
		MovementComponent,
		Settings,
		AuthoredBallTransformWorld.GetLocation());
	const bool bWantsToPlantNow = bHasSpeedCurve && RpgFootPlacement::ShouldPlantFoot(
		TraceResult.bWalkable,
		FootSpeed,
		TraceResult.DistanceToGround,
		Settings);
	bool bReleasedThisFrame = false;
	float AnchorDistance = MAX_flt;
	float GroundNormalDelta = 180.0f;
	FTransform CurrentAlignedFoot = FKFootTransformWorld;

	if (TraceResult.bWalkable)
	{
		if (State.bLocked && State.HitComponentId != 0 &&
			State.HitComponentId == TraceResult.HitComponentId &&
			State.bHasPreviousHitComponentTransform)
		{
			State.LockedFootTransformWorld = RpgFootPlacement::RebaseTransformThroughSurface(
				State.LockedFootTransformWorld,
				State.PreviousHitComponentTransform,
				TraceResult.HitComponentTransform);
			State.LockedGroundPointWorld = RpgFootPlacement::RebasePointThroughSurface(
				State.LockedGroundPointWorld,
				State.PreviousHitComponentTransform,
				TraceResult.HitComponentTransform);
			State.LockedGroundNormalWorld = RpgFootPlacement::RebaseNormalThroughSurface(
				State.LockedGroundNormalWorld,
				State.PreviousHitComponentTransform,
				TraceResult.HitComponentTransform);
		}

		CurrentAlignedFoot = RpgFootPlacement::AlignFootToGroundPlane(
			FKFootTransformWorld,
			AuthoredBallTransformWorld,
			TraceResult.GroundPointWorld,
			TraceResult.GroundNormalWorld,
			ComponentToWorld.GetUnitAxis(EAxis::Z),
			Settings.MaxFootTranslation,
			Settings.MaxFootAlignmentAngle);
		AnchorDistance = (
			CurrentAlignedFoot.GetLocation() - State.LockedFootTransformWorld.GetLocation()).Size2D();
		GroundNormalDelta = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(
				State.LockedGroundNormalWorld.GetSafeNormal(),
				TraceResult.GroundNormalWorld.GetSafeNormal()),
			-1.0f,
			1.0f)));
		if (State.bLocked && (State.HitComponentId != TraceResult.HitComponentId ||
			RpgFootPlacement::ShouldUnplantFoot(
				FootSpeed,
				AnchorDistance,
				GroundNormalDelta,
				Settings)))
		{
			State.bLocked = false;
			bReleasedThisFrame = true;
		}
		State.TraceMissElapsed = 0.0f;
	}
	else if (State.bLocked)
	{
		State.TraceMissElapsed += FMath::Max(DeltaSeconds, 0.0f);
		if (bHasBaseDelta && State.HitComponentId == MovementBaseId)
		{
			State.LockedFootTransformWorld = RpgFootPlacement::RebaseTransformThroughSurface(
				State.LockedFootTransformWorld,
				PreviousBaseTransform,
				CurrentBaseTransform);
			State.LockedGroundPointWorld = RpgFootPlacement::RebasePointThroughSurface(
				State.LockedGroundPointWorld,
				PreviousBaseTransform,
				CurrentBaseTransform);
			State.LockedGroundNormalWorld = RpgFootPlacement::RebaseNormalThroughSurface(
				State.LockedGroundNormalWorld,
				PreviousBaseTransform,
				CurrentBaseTransform);
			if (State.bHasRetainedGroundTarget)
			{
				State.RetainedGroundPointWorld = RpgFootPlacement::RebasePointThroughSurface(
					State.RetainedGroundPointWorld,
					PreviousBaseTransform,
					CurrentBaseTransform);
				State.RetainedGroundNormalWorld = RpgFootPlacement::RebaseNormalThroughSurface(
					State.RetainedGroundNormalWorld,
					PreviousBaseTransform,
					CurrentBaseTransform);
			}
		}
		if (State.TraceMissElapsed > Settings.TraceMissGracePeriod)
		{
			State.bLocked = false;
			bReleasedThisFrame = true;
		}
	}

	const bool bMayPlantFresh = !State.bWantedToPlantLastFrame;
	const bool bMayReplant =
		!bReleasedThisFrame &&
		RpgFootPlacement::ShouldReplantFoot(AnchorDistance, GroundNormalDelta, Settings);
	if (!State.bLocked && bWantsToPlantNow && (bMayPlantFresh || bMayReplant))
	{
		State.bLocked = true;
		State.LockedGroundPointWorld = TraceResult.GroundPointWorld;
		State.LockedGroundNormalWorld = TraceResult.GroundNormalWorld;
		State.LockedFootTransformWorld = CurrentAlignedFoot;
		State.HitComponentId = TraceResult.HitComponentId;
		State.TraceMissElapsed = 0.0f;
	}
	State.bWantedToPlantLastFrame = bWantsToPlantNow;

	if (TraceResult.bWalkable)
	{
		State.RetainedGroundPointWorld = TraceResult.GroundPointWorld;
		State.RetainedGroundNormalWorld = TraceResult.GroundNormalWorld;
		State.bHasRetainedGroundTarget = true;
		State.HitComponentId = TraceResult.HitComponentId;
		State.PreviousHitComponentTransform = TraceResult.HitComponentTransform;
		State.bHasPreviousHitComponentTransform = TraceResult.HitComponentId != 0;
	}
	else if (!State.bLocked)
	{
		State.HitComponentId = 0;
		State.bHasPreviousHitComponentTransform = false;
	}

	const bool bHasGround = TraceResult.bWalkable || State.bLocked;
	const float TargetWeight = bHasGround && bHasSpeedCurve
		? FMath::Clamp(ContactCurveValue, 0.0f, 1.0f) *
			RpgFootPlacement::CalculateAlignmentAlpha(FootSpeed, Settings)
		: 0.0f;
	const float WeightAlpha = RpgFootPlacement::CalculateHalfLifeAlpha(
		DeltaSeconds,
		Settings.WeightBlendHalfLife);
	State.Weight = FMath::Lerp(State.Weight, TargetWeight, WeightAlpha);
	if (!TraceResult.bWalkable && !State.bLocked && State.Weight <= UE_KINDA_SMALL_NUMBER)
	{
		State.bHasRetainedGroundTarget = false;
	}

	Snapshot.GroundPointWorld = State.bLocked
		? State.LockedGroundPointWorld
		: (TraceResult.bWalkable
			? TraceResult.GroundPointWorld
			: State.RetainedGroundPointWorld);
	Snapshot.GroundNormalWorld = State.bLocked
		? State.LockedGroundNormalWorld
		: (TraceResult.bWalkable
			? TraceResult.GroundNormalWorld
			: State.RetainedGroundNormalWorld);
	Snapshot.LockedFootTransformWorld = State.LockedFootTransformWorld;
	Snapshot.Weight = State.Weight;
	Snapshot.DistanceToGround = TraceResult.DistanceToGround;
	Snapshot.HitComponentId = State.HitComponentId;
	Snapshot.bHasWalkableGround = bHasGround ||
		(State.bHasRetainedGroundTarget && State.Weight > UE_KINDA_SMALL_NUMBER);
	Snapshot.bLocked = State.bLocked;
	Snapshot.bHasSpeedCurve = bHasSpeedCurve;
}

void UpdateFootPlacementSnapshot(
	FRpgAnimInstanceProxy& Proxy,
	const FRpgFootPlacementSettings& Settings,
	const URpgAnimInstance& AnimInstance,
	const ARpgCharacter& Character,
	const URpgCharacterMovementComponent& MovementComponent,
	float DeltaSeconds)
{
	check(IsInGameThread());
	Proxy.FootPlacementSnapshot = FRpgFootPlacementSnapshot();
	FRpgFootPlacementSnapshot& Snapshot = Proxy.FootPlacementSnapshot;
	Snapshot.OwnerId = Character.GetUniqueID();
	Snapshot.LocalRole = static_cast<uint8>(Character.GetLocalRole());
	Snapshot.RemoteRole = static_cast<uint8>(Character.GetRemoteRole());
	Snapshot.VelocityWorld = MovementComponent.Velocity;
	Snapshot.bGrounded = Proxy.MovementState == ERpgLocomotionMovementState::Grounded &&
		Proxy.bIsMovingOnGround;

	const USkeletalMeshComponent* MeshComponent = AnimInstance.GetSkelMeshComponent();
	if (!MeshComponent)
	{
		Snapshot.bReset = true;
		ResetFootPlacementState(Proxy);
		Proxy.bPreviousFootPlacementSourceEligible = false;
		return;
	}

	const FTransform ComponentToWorld = MeshComponent->GetComponentTransform();
	const bool bHadPreviousComponentTransform = Proxy.bHasPreviousFootPlacementComponentTransform;
	const bool bHadPreviousBaseTransform = Proxy.bHasPreviousMovementBaseTransform;
	const uint32 PreviousMovementBaseId = Proxy.PreviousMovementBaseId;
	const FTransform PreviousMovementBaseTransform = Proxy.PreviousMovementBaseTransform;
	Snapshot.ComponentToWorld = ComponentToWorld;
	if (bHadPreviousComponentTransform)
	{
		Snapshot.ComponentDeltaWorld =
			ComponentToWorld.GetLocation() - Proxy.PreviousFootPlacementComponentTransform.GetLocation();
	}

	uint32 MovementBaseId = 0;
	FTransform CurrentBaseTransform = FTransform::Identity;
	const bool bHasCurrentBase = TryGetMovementBaseSnapshot(
		Character,
		MovementBaseId,
		CurrentBaseTransform);
	Snapshot.MovementBaseId = MovementBaseId;
	const bool bHasBaseDelta =
		bHasCurrentBase &&
		bHadPreviousBaseTransform &&
		MovementBaseId != 0 &&
		MovementBaseId == PreviousMovementBaseId;
	if (bHasBaseDelta)
	{
		Snapshot.BaseDeltaTranslationWorld =
			CurrentBaseTransform.GetLocation() - PreviousMovementBaseTransform.GetLocation();
		Snapshot.BaseDeltaRotationWorld = (
			CurrentBaseTransform.GetRotation() *
			PreviousMovementBaseTransform.GetRotation().Inverse()).GetNormalized();
	}

	if (MovementComponent.CurrentFloor.bBlockingHit)
	{
		Snapshot.FloorPointWorld = MovementComponent.CurrentFloor.HitResult.ImpactPoint;
		Snapshot.FloorNormalWorld = MovementComponent.CurrentFloor.HitResult.ImpactNormal.GetSafeNormal(
			UE_SMALL_NUMBER,
			FVector::UpVector);
	}

	const bool bBaseIdentityChanged =
		bHadPreviousComponentTransform &&
		PreviousMovementBaseId != MovementBaseId;
	const bool bSourceEligible =
		Settings.bEnabled &&
		Snapshot.bGrounded &&
		(!Proxy.bIsCrouching || Settings.bApplyWhileCrouching) &&
		!Proxy.bIsAnyMontagePlaying &&
		!Proxy.bHasTurnInPlaceBlockingGameplayTag;
	const bool bSourceBecameEligible =
		bSourceEligible && !Proxy.bPreviousFootPlacementSourceEligible;
	Snapshot.bReset =
		Proxy.bTurnInPlaceHardReset ||
		bBaseIdentityChanged ||
		bSourceBecameEligible ||
		!bSourceEligible;

	Proxy.PreviousFootPlacementComponentTransform = ComponentToWorld;
	Proxy.bHasPreviousFootPlacementComponentTransform = true;
	Proxy.PreviousMovementBaseId = MovementBaseId;
	Proxy.PreviousMovementBaseTransform = CurrentBaseTransform;
	Proxy.bHasPreviousMovementBaseTransform = bHasCurrentBase;
	Proxy.bPreviousFootPlacementSourceEligible = bSourceEligible;

	if (Snapshot.bReset)
	{
		ResetFootPlacementState(Proxy);
		return;
	}

	UpdateFootPlacementLeg(
		Proxy.FootPlacementLegStates[0],
		Snapshot.LeftFoot,
		Settings.LeftLeg,
		Settings,
		AnimInstance,
		Character,
		MovementComponent,
		*MeshComponent,
		ComponentToWorld,
		MovementBaseId,
		PreviousMovementBaseTransform,
		CurrentBaseTransform,
		bHasBaseDelta,
		DeltaSeconds);
	UpdateFootPlacementLeg(
		Proxy.FootPlacementLegStates[1],
		Snapshot.RightFoot,
		Settings.RightLeg,
		Settings,
		AnimInstance,
		Character,
		MovementComponent,
		*MeshComponent,
		ComponentToWorld,
		MovementBaseId,
		PreviousMovementBaseTransform,
		CurrentBaseTransform,
		bHasBaseDelta,
		DeltaSeconds);
	Snapshot.bValid = true;
	Proxy.FootPlacementAlpha = 1.0f;
}
}


URpgAnimInstance::URpgAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URpgAnimInstance::CanRunParallelWork() const
{
	if (!Super::CanRunParallelWork())
	{
		return false;
	}

	const ARpgCharacter* Character = Cast<ARpgCharacter>(TryGetPawnOwner());
	const USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
	const UWorld* World = GetWorld();

	const bool bIsRemoteAutonomousMoveOnServer =
		World &&
		(World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer) &&
		Character &&
		Character->GetLocalRole() == ROLE_Authority &&
		Character->GetRemoteRole() == ROLE_AutonomousProxy &&
		MeshComponent &&
		MeshComponent->bOnlyAllowAutonomousTickPose &&
		MeshComponent->bIsAutonomousTickPose;

	// Parallel updates collapse several autonomous move pose ticks into the last move delta.
	// CMC runs this path on listen and dedicated servers. Montages already advance per move;
	// the graph must also consume each delta before the next move overwrites the proxy snapshot.
	return !bIsRemoteAutonomousMoveOnServer;
}

void FRpgAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	Super::PreUpdate(InAnimInstance, DeltaSeconds);

	WorldVelocity = FVector::ZeroVector;
	LocalVelocity = FVector::ZeroVector;
	WorldAcceleration = FVector::ZeroVector;
	LocalAcceleration = FVector::ZeroVector;
	GroundSpeed = 0.0f;
	VerticalVelocity = 0.0f;
	GroundDistance = -1.0f;
	AimYaw = 0.0f;
	AimPitch = 0.0f;
	LocomotionAngle = 0.0f;
	ProceduralLocomotionAlpha = 0.0f;
	AirborneProceduralAlpha = 0.0f;
	Gait = ERpgLocomotionGait::Idle;
	Stance = ERpgLocomotionStance::Standing;
	MovementState = ERpgLocomotionMovementState::None;
	RotationMode = ERpgCharacterRotationMode::Free;
	bHasVelocity = false;
	bHasAcceleration = false;
	bHasGroundedMoveIntent = false;
	bIsFalling = false;
	bIsMovingOnGround = false;
	bIsCrouching = false;
	bIsAnyMontagePlaying = false;
	bHasTurnInPlaceBlockingGameplayTag = false;
	bTurnInPlaceHardReset = false;
	bTurnInPlaceSupportChanged = false;
	bLandingTouchdownPendingConsumption = false;
	ActorYaw = 0.0f;
	ActorYawDelta = 0.0f;
	MeshBasisRotation = FQuat::Identity;
	ActorLocation = FVector::ZeroVector;
	TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
	FootPlacementSnapshot = FRpgFootPlacementSnapshot();
	FootPlacementAlpha = 0.0f;

	URpgAnimInstance* RpgAnimInstance = Cast<URpgAnimInstance>(InAnimInstance);
	bHasTurnInPlaceBlockingGameplayTag =
		RpgAnimInstance &&
		(RpgAnimInstance->bGameplayMovementStopped ||
		 RpgAnimInstance->bStateBlocking ||
		 RpgAnimInstance->bStateDead ||
		 RpgAnimInstance->bStateStaggered ||
		 RpgAnimInstance->bStateGuardBroken);
	const ARpgCharacter* Character = RpgAnimInstance ? Cast<ARpgCharacter>(RpgAnimInstance->TryGetPawnOwner()) : nullptr;
	if (RpgAnimInstance)
	{
		RpgAnimInstance->SynchronizeCombatAnimationProfileProvider(Character, *this);
	}
	if (Character)
	{
		RotationMode = Character->GetRotationMode();
	}
	if (RpgAnimInstance)
	{
		const FGameplayTagContainer CombatEquipmentTraits = Character
			? GatherCombatAnimationEquipmentTraits(*Character)
			: FGameplayTagContainer();
		const bool bHasCombatAnimationProvider =
			RpgAnimInstance->CombatAnimationProfileLookup.IsEnabled();
		AdvanceCombatAnimationSnapshot(
			*this,
			RpgAnimInstance->CombatAnimationProfileLookup.Resolve(
				CombatEquipmentTraits),
			bHasCombatAnimationProvider && Character &&
				RotationMode != ERpgCharacterRotationMode::Free,
			DeltaSeconds);
	}
	if (!Character)
	{
		LastNonZeroWorldVelocity = FVector::ZeroVector;
		ResetPoseSearchTrajectoryState(*this);
		LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		LastGroundedGait = ERpgLocomotionGait::Idle;
		LandingAirborneEpoch = 0;
		bWasAirborneForLanding = false;
		bTurnInPlaceHardReset = true;
		PreviousOwnerUniqueId = 0;
		PreviousAnimationDiscontinuitySerial = 0;
		bHasPreviousOwnerSnapshot = false;
		return;
	}
	URpgCharacterMovementComponent* MovementComponent = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!MovementComponent || !Character->GetWorld())
	{
		LastNonZeroWorldVelocity = FVector::ZeroVector;
		ResetPoseSearchTrajectoryState(*this);
		LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		LastGroundedGait = ERpgLocomotionGait::Idle;
		LandingAirborneEpoch = 0;
		bWasAirborneForLanding = false;
		bTurnInPlaceHardReset = true;
		PreviousOwnerUniqueId = 0;
		PreviousAnimationDiscontinuitySerial = 0;
		bHasPreviousOwnerSnapshot = false;
		return;
	}

	const FTransform PresentationActorTransform = GetPresentationActorTransform(*Character);
	const FQuat ActorRotation = PresentationActorTransform.GetRotation();
	ActorYaw = ActorRotation.Rotator().Yaw;
	MeshBasisRotation = Character->GetBaseRotationOffset().GetNormalized();
	ActorLocation = PresentationActorTransform.GetLocation();
	const uint32 OwnerUniqueId = Character->GetUniqueID();
	const uint8 LocalRole = static_cast<uint8>(Character->GetLocalRole());
	const uint8 RemoteRole = static_cast<uint8>(Character->GetRemoteRole());
	const bool bOwnerOrRoleChanged =
		!bHasPreviousOwnerSnapshot ||
		PreviousOwnerUniqueId != OwnerUniqueId ||
		PreviousLocalRole != LocalRole ||
		PreviousRemoteRole != RemoteRole;
	bTurnInPlaceSupportChanged = RpgTurnInPlaceRuntime::DidSupportChange(
		bHasPreviousOwnerSnapshot,
		PreviousRotationMode,
		RotationMode);
	const uint32 AnimationDiscontinuitySerial =
		MovementComponent->GetAnimationDiscontinuitySerial();
	const bool bAnimationDiscontinuity =
		bHasPreviousOwnerSnapshot &&
		AnimationDiscontinuitySerial != PreviousAnimationDiscontinuitySerial;
	bTurnInPlaceHardReset =
		bOwnerOrRoleChanged || bAnimationDiscontinuity;
	if (bTurnInPlaceHardReset)
	{
		ResetPoseSearchTrajectoryState(*this);
		++AnimationHistoryResetCount;
	}
	ActorYawDelta = RpgTurnInPlaceRuntime::CalculateSnapshotYawDelta(
		PreviousActorYaw,
		ActorYaw,
		bTurnInPlaceHardReset,
		bTurnInPlaceSupportChanged);

	PreviousOwnerUniqueId = OwnerUniqueId;
	PreviousLocalRole = LocalRole;
	PreviousRemoteRole = RemoteRole;
	PreviousActorYaw = ActorYaw;
	PreviousActorLocation = ActorLocation;
	PreviousAnimationDiscontinuitySerial = AnimationDiscontinuitySerial;
	PreviousRotationMode = RotationMode;
	bHasPreviousOwnerSnapshot = true;

	WorldVelocity = MovementComponent->Velocity;
	const FVector HorizontalWorldVelocity(WorldVelocity.X, WorldVelocity.Y, 0.0f);
	const FRpgGaspLocomotionTuning& LocomotionTuning =
		RpgAnimInstance->RuntimeGaspLocomotionTuning;
	if (HorizontalWorldVelocity.SizeSquared() >=
		FMath::Square(LocomotionTuning.LastMeaningfulVelocityThreshold))
	{
		LastNonZeroWorldVelocity = HorizontalWorldVelocity;
	}
	LocalVelocity = ActorRotation.UnrotateVector(WorldVelocity);
	WorldAcceleration = MovementComponent->GetCurrentAcceleration();
	LocalAcceleration = ActorRotation.UnrotateVector(WorldAcceleration);
	GroundSpeed = WorldVelocity.Size2D();
	VerticalVelocity = WorldVelocity.Z;
	GroundDistance = MovementComponent->GetGroundInfo().GroundDistance;
	bHasVelocity = !WorldVelocity.IsNearlyZero();
	bHasAcceleration = !WorldAcceleration.IsNearlyZero();
	bIsFalling = MovementComponent->IsFalling();
	bIsMovingOnGround = MovementComponent->IsMovingOnGround();
	bIsCrouching = Character->bIsCrouched;
	bIsAnyMontagePlaying = RpgAnimInstance->IsAnyMontagePlaying();
	Stance = bIsCrouching ? ERpgLocomotionStance::Crouching : ERpgLocomotionStance::Standing;

	switch (MovementComponent->MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		MovementState = ERpgLocomotionMovementState::Grounded;
		break;
	case MOVE_Falling:
		MovementState = ERpgLocomotionMovementState::Airborne;
		break;
	case MOVE_Swimming:
		MovementState = ERpgLocomotionMovementState::Swimming;
		break;
	case MOVE_Flying:
		MovementState = ERpgLocomotionMovementState::Flying;
		break;
	case MOVE_Custom:
		MovementState = ERpgLocomotionMovementState::Custom;
		break;
	default:
		MovementState = ERpgLocomotionMovementState::None;
		break;
	}

	bHasGroundedMoveIntent = bIsMovingOnGround && MovementComponent->HasMoveIntent();
	Gait = MovementComponent->GetGroundGait();

	ProceduralLocomotionAlpha =
		bIsMovingOnGround && !bIsCrouching && !bIsAnyMontagePlaying ? 1.0f : 0.0f;
	AirborneProceduralAlpha =
		bIsFalling && !bIsCrouching && !bIsAnyMontagePlaying ? 1.0f : 0.0f;

	UpdateFootPlacementSnapshot(
		*this,
		RpgAnimInstance->FootPlacementSettings,
		*RpgAnimInstance,
		*Character,
		*MovementComponent,
		DeltaSeconds);

	const FRotator AimDelta =
		(Character->GetBaseAimRotation() - ActorRotation.Rotator()).GetNormalized();
	AimYaw = AimDelta.Yaw;
	AimPitch = AimDelta.Pitch;
	LocomotionAngle = bHasVelocity
		? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X))
		: 0.0f;

	const bool bSupportsPoseSearchTrajectory =
		MovementState == ERpgLocomotionMovementState::Grounded ||
		MovementState == ERpgLocomotionMovementState::Airborne;
	if (RpgAnimInstance->ShouldGeneratePoseSearchTrajectory() && bSupportsPoseSearchTrajectory)
	{
		// This controller-facing character turns its component directly. Extrapolating a one-frame mouse
		// or replicated yaw delta across the prediction horizon exaggerates small turns and selects false
		// 90/180-degree poses. Current mesh facing and trajectory history already carry the real rotation.
		TrajectoryGenerationData.RotateTowardsMovementSpeed = 0.0f;
		TrajectoryGenerationData.BendVelocityTowardsAcceleration = 0.0f;
		TrajectoryGenerationData.MaxControllerYawRate = 0.0f;

		// Avoid interpreting the initial world yaw as a one-frame controller turn.
		if (RawTransformTrajectory.Samples.IsEmpty())
		{
			DesiredControllerYawLastUpdate = Character->GetViewRotation().Yaw;
		}

		FTransformTrajectory GeneratedRawTrajectory;
		UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
			RpgAnimInstance,
			TrajectoryGenerationData,
			DeltaSeconds,
			RawTransformTrajectory,
			DesiredControllerYawLastUpdate,
			GeneratedRawTrajectory,
			RpgPoseSearchTrajectory::HistorySamplingInterval,
			RpgPoseSearchTrajectory::HistorySampleCount,
			RpgPoseSearchTrajectory::PredictionSamplingInterval,
			RpgPoseSearchTrajectory::PredictionSampleCount);
		RawTransformTrajectory = MoveTemp(GeneratedRawTrajectory);
		if (RpgPoseSearchTrajectory::IsTransformTrajectoryFinite(RawTransformTrajectory))
		{
			TransformTrajectory = RawTransformTrajectory;
			if (RpgAnimInstance->GetTrajectoryCollisionSettings().bEnabled)
			{
				RpgPoseSearchTrajectory::FCollisionResult CollisionResult =
					RpgPoseSearchTrajectory::ResolveWorldCollision(
						*Character,
						*MovementComponent,
						RpgAnimInstance->GetTrajectoryCollisionSettings(),
						RawTransformTrajectory,
						bIsFalling && MovementState == ERpgLocomotionMovementState::Airborne,
						bTurnInPlaceHardReset);
				if (RpgPoseSearchTrajectory::IsTransformTrajectoryFinite(
					CollisionResult.CorrectedTrajectory))
				{
					TransformTrajectory = MoveTemp(CollisionResult.CorrectedTrajectory);
				}
				TrajectoryLandingPrediction = CollisionResult.LandingPrediction;
			}
		}
		else
		{
			ResetPoseSearchTrajectoryState(*this);
		}
	}
	else
	{
		ResetPoseSearchTrajectoryState(*this);
	}

	const FVector GravityAcceleration =
		MovementComponent->GetGravityDirection() * -MovementComponent->GetGravityZ();
	bLandingTouchdownPendingConsumption =
		RpgAnimInstance->JumpPhase == ERpgJumpPhase::Airborne;
	URpgAnimInstance::UpdateLandingSelectionSnapshot(
		*this,
		MovementComponent->GetDesiredGait(),
		GravityAcceleration,
		RpgAnimInstance->RuntimeGaspLocomotionTuning);
}

void URpgAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

FName URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(
	ERpgMotionMatchingDatabaseRole Role)
{
	return RpgGaspLocomotionConfig::GetDatabaseRoleTag(Role);
}

FName URpgAnimInstance::GetMotionMatchingDatabaseStateTag(
	ERpgMotionMatchingDatabaseRole Role)
{
	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandIdle:
	case ERpgMotionMatchingDatabaseRole::StandWalk:
	case ERpgMotionMatchingDatabaseRole::StandWalkStops:
	case ERpgMotionMatchingDatabaseRole::StandRunLoops:
	case ERpgMotionMatchingDatabaseRole::StandRunPivots:
	case ERpgMotionMatchingDatabaseRole::StandRunStarts:
	case ERpgMotionMatchingDatabaseRole::StandRunStops:
	case ERpgMotionMatchingDatabaseRole::StandSprint:
	case ERpgMotionMatchingDatabaseRole::StandSprintStops:
		return TEXT("Rpg.MotionMatching.State.Grounded");
	case ERpgMotionMatchingDatabaseRole::Crouch:
		return TEXT("Rpg.MotionMatching.State.Crouching");
	case ERpgMotionMatchingDatabaseRole::StandTurnInPlace:
		return TEXT("Rpg.MotionMatching.State.TurnInPlace");
	case ERpgMotionMatchingDatabaseRole::Jump:
		return TEXT("Rpg.MotionMatching.State.Airborne");
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return TEXT("Rpg.MotionMatching.State.Landing");
	default:
		return NAME_None;
	}
}

void URpgAnimInstance::UpdateLandingSelectionSnapshot(
	FRpgAnimInstanceProxy& Proxy,
	ERpgLocomotionGait DesiredGait,
	const FVector& GravityAcceleration,
	const FRpgGaspLocomotionTuning& Tuning)
{
	FRpgLandingCaptureState State;
	State.LastGroundedGait = Proxy.LastGroundedGait;
	State.AirborneEpoch = Proxy.LandingAirborneEpoch;
	State.bWasAirborne = Proxy.bWasAirborneForLanding;

	FRpgLandingCaptureSnapshot Snapshot;
	Snapshot.WorldVelocity = Proxy.WorldVelocity;
	Snapshot.GravityAcceleration = GravityAcceleration;
	Snapshot.TrajectoryPrediction = Proxy.TrajectoryLandingPrediction;
	Snapshot.Gait = Proxy.Gait;
	Snapshot.DesiredGait = DesiredGait;
	Snapshot.MovementState = Proxy.MovementState;
	Snapshot.VerticalVelocity = Proxy.VerticalVelocity;
	Snapshot.bHasMoveIntent =
		DesiredGait == ERpgLocomotionGait::Walk ||
		DesiredGait == ERpgLocomotionGait::Run;
	Snapshot.bIsFalling = Proxy.bIsFalling;
	Snapshot.bIsMovingOnGround = Proxy.bIsMovingOnGround;
	Snapshot.bAwaitingTouchdownConsumption =
		Proxy.bLandingTouchdownPendingConsumption;
	Snapshot.bHardReset = Proxy.bTurnInPlaceHardReset;
	RpgLandingRuntime::UpdateSelectionSnapshot(
		Proxy.LandingSelectionSnapshot,
		State,
		Snapshot,
		Tuning);

	Proxy.LastGroundedGait = State.LastGroundedGait;
	Proxy.LandingAirborneEpoch = State.AirborneEpoch;
	Proxy.bWasAirborneForLanding = State.bWasAirborne;
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveAvailableLandingDatabaseRole(
	ERpgMotionMatchingDatabaseRole RequestedRole) const
{
	return RpgLandingRuntime::ResolveAvailableRole(
		RequestedRole,
		BuildLandingDatabaseAvailability());
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveAvailableLandingDatabaseRole(
	const FRpgLandingSelectionSnapshot& Snapshot) const
{
	return ResolveAvailableLandingDatabaseRole(
		RpgLandingRuntime::ResolveDatabaseRole(Snapshot, RuntimeGaspLocomotionTuning));
}

bool URpgAnimInstance::SynchronizeMotionMatchingNodeUpdateCounter(
	FGraphTraversalCounter& NodeUpdateCounter,
	const FGraphTraversalCounter& AnimInstanceUpdateCounter)
{
	const bool bBecameRelevant =
		NodeUpdateCounter.HasEverBeenUpdated() &&
		!NodeUpdateCounter.WasSynchronizedCounter(AnimInstanceUpdateCounter);
	NodeUpdateCounter.SynchronizeWith(AnimInstanceUpdateCounter);
	return bBecameRelevant;
}
URpgAnimInstance::FResolvedGroundMotionMatchingDatabases
URpgAnimInstance::ResolveGroundMotionMatchingDatabases(
	const FRpgGroundMotionMatchingSelectionSnapshot& Snapshot,
	const FRpgGroundMotionMatchingDatabaseSets& DatabaseSets)
{
	FResolvedGroundMotionMatchingDatabases ResolvedDatabases;
	auto AddDatabase = [&ResolvedDatabases](UPoseSearchDatabase* Database)
	{
		if (Database && !ResolvedDatabases.Contains(Database))
		{
			ResolvedDatabases.Add(Database);
		}
	};
	auto GetDatabaseAtIndex = [](
		const TArray<TObjectPtr<UPoseSearchDatabase>>& Databases,
		int32 Index) -> UPoseSearchDatabase*
	{
		return Databases.IsValidIndex(Index) ? Databases[Index].Get() : nullptr;
	};

	for (const ERpgMotionMatchingDatabaseRole Role :
		RpgMotionMatchingRuntime::ResolveDatabaseRoles(Snapshot))
	{
		switch (Role)
		{
		case ERpgMotionMatchingDatabaseRole::StandIdle:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Idle, 0));
			break;
		case ERpgMotionMatchingDatabaseRole::StandWalk:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Walk, 0));
			break;
		case ERpgMotionMatchingDatabaseRole::StandWalkStops:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Walk, 1));
			break;
		case ERpgMotionMatchingDatabaseRole::StandRunLoops:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Run, 0));
			break;
		case ERpgMotionMatchingDatabaseRole::StandRunPivots:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Run, 1));
			break;
		case ERpgMotionMatchingDatabaseRole::StandRunStarts:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Run, 2));
			break;
		case ERpgMotionMatchingDatabaseRole::StandRunStops:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Run, 3));
			break;
		case ERpgMotionMatchingDatabaseRole::StandSprint:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Sprint, 0));
			break;
		case ERpgMotionMatchingDatabaseRole::StandSprintStops:
			AddDatabase(GetDatabaseAtIndex(DatabaseSets.Sprint, 1));
			break;
		default:
			break;
		}
	}
	return ResolvedDatabases;
}

URpgAnimInstance::FGroundMotionMatchingDatabaseSetValidation
URpgAnimInstance::ValidateGroundMotionMatchingDatabaseSets(
	const FRpgGroundMotionMatchingDatabaseSets& DatabaseSets)
{
	struct FDatabaseSetContract
	{
		const TArray<TObjectPtr<UPoseSearchDatabase>>* Databases = nullptr;
		int32 ExpectedNum = 0;
	};
	const FDatabaseSetContract Contracts[] = {
		{ &DatabaseSets.Idle, 1 },
		{ &DatabaseSets.Walk, 2 },
		{ &DatabaseSets.Run, 4 },
		{ &DatabaseSets.Sprint, 2 },
	};

	FGroundMotionMatchingDatabaseSetValidation Validation;
	TArray<UPoseSearchDatabase*, TInlineAllocator<9>> SeenDatabases;
	for (const FDatabaseSetContract& Contract : Contracts)
	{
		check(Contract.Databases);
		Validation.bHasInvalidShape |= Contract.Databases->Num() != Contract.ExpectedNum;
		for (UPoseSearchDatabase* Database : *Contract.Databases)
		{
			if (!Database)
			{
				Validation.bHasNullDatabase = true;
			}
			else if (SeenDatabases.Contains(Database))
			{
				Validation.bHasDuplicateDatabase = true;
			}
			else
			{
				SeenDatabases.Add(Database);
			}
		}
	}
	return Validation;
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveMotionMatchingDatabaseRole(
	const UPoseSearchDatabase* Database)
{
	return Database
		? RpgGaspLocomotionConfig::ResolveDatabaseRoleTag(Database->Tags)
		: ERpgMotionMatchingDatabaseRole::None;
}

URpgAnimInstance::FMotionMatchingDatabaseRoleValidation
URpgAnimInstance::ValidateMotionMatchingDatabaseRoleContracts(
	TConstArrayView<FMotionMatchingDatabaseRoleContract> Contracts)
{
	FMotionMatchingDatabaseRoleValidation Validation;
	constexpr int32 RoleCount = static_cast<int32>(ERpgMotionMatchingDatabaseRole::Count);
	int32 ContractCountsByRole[RoleCount] = {};
	TSet<const UPoseSearchDatabase*> SeenDatabases;

	auto IsProjectRoleTag = [](FName Tag)
	{
		return Tag.ToString().StartsWith(
			TEXT("Rpg.MotionMatching.Role."),
			ESearchCase::CaseSensitive);
	};
	auto IsProjectStateTag = [](FName Tag)
	{
		return Tag.ToString().StartsWith(
			TEXT("Rpg.MotionMatching.State."),
			ESearchCase::CaseSensitive);
	};

	for (const FMotionMatchingDatabaseRoleContract& Contract : Contracts)
	{
		const int32 RoleIndex = static_cast<int32>(Contract.Role);
		if (RoleIndex <= static_cast<int32>(ERpgMotionMatchingDatabaseRole::None) ||
			RoleIndex >= RoleCount)
		{
			Validation.bHasMissingRole = true;
			continue;
		}
		++ContractCountsByRole[RoleIndex];

		if (!Contract.Database)
		{
			Validation.bHasNullDatabase = true;
			continue;
		}
		if (SeenDatabases.Contains(Contract.Database))
		{
			Validation.bHasDuplicateDatabase = true;
		}
		else
		{
			SeenDatabases.Add(Contract.Database);
		}

		const FName ExpectedRoleTag = GetMotionMatchingDatabaseRoleTag(Contract.Role);
		const FName ExpectedStateTag = GetMotionMatchingDatabaseStateTag(Contract.Role);
		int32 ExpectedRoleTagCount = 0;
		int32 ProjectRoleTagCount = 0;
		int32 ExpectedStateTagCount = 0;
		int32 ProjectStateTagCount = 0;
		for (const FName Tag : Contract.Database->Tags)
		{
			ExpectedRoleTagCount += Tag == ExpectedRoleTag;
			ExpectedStateTagCount += Tag == ExpectedStateTag;
			if (IsProjectRoleTag(Tag))
			{
				++ProjectRoleTagCount;
				Validation.bHasWrongRoleTag |= Tag != ExpectedRoleTag;
			}
			if (IsProjectStateTag(Tag))
			{
				++ProjectStateTagCount;
				Validation.bHasWrongStateTag |= Tag != ExpectedStateTag;
			}
		}

		Validation.bHasMissingRoleTag |= ExpectedRoleTagCount == 0;
		Validation.bHasDuplicateRoleTag |=
			ExpectedRoleTagCount > 1 || ProjectRoleTagCount > 1;
		Validation.bHasMissingStateTag |= ExpectedStateTagCount == 0;
		Validation.bHasDuplicateStateTag |=
			ExpectedStateTagCount > 1 || ProjectStateTagCount > 1;
	}

	for (int32 RoleIndex = static_cast<int32>(ERpgMotionMatchingDatabaseRole::None) + 1;
		RoleIndex < RoleCount;
		++RoleIndex)
	{
		Validation.bHasMissingRole |= ContractCountsByRole[RoleIndex] == 0;
		Validation.bHasDuplicateRole |= ContractCountsByRole[RoleIndex] > 1;
	}
	return Validation;
}

UPoseSearchDatabase* URpgAnimInstance::GetLegacyMotionMatchingDatabaseForRole(
	ERpgMotionMatchingDatabaseRole Role) const
{
	auto GetDatabaseAtIndex = [](
		const TArray<TObjectPtr<UPoseSearchDatabase>>& Databases,
		int32 Index) -> UPoseSearchDatabase*
	{
		return Databases.IsValidIndex(Index) ? Databases[Index].Get() : nullptr;
	};

	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandIdle:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Idle, 0);
	case ERpgMotionMatchingDatabaseRole::StandWalk:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Walk, 0);
	case ERpgMotionMatchingDatabaseRole::StandWalkStops:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Walk, 1);
	case ERpgMotionMatchingDatabaseRole::StandRunLoops:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Run, 0);
	case ERpgMotionMatchingDatabaseRole::StandRunPivots:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Run, 1);
	case ERpgMotionMatchingDatabaseRole::StandRunStarts:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Run, 2);
	case ERpgMotionMatchingDatabaseRole::StandRunStops:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Run, 3);
	case ERpgMotionMatchingDatabaseRole::StandSprint:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Sprint, 0);
	case ERpgMotionMatchingDatabaseRole::StandSprintStops:
		return GetDatabaseAtIndex(GroundMotionMatchingDatabaseSets.Sprint, 1);
	case ERpgMotionMatchingDatabaseRole::Crouch:
		return CrouchingMotionMatchingDatabase.Get();
	case ERpgMotionMatchingDatabaseRole::StandTurnInPlace:
		return TurnInPlaceMotionMatchingDatabase.Get();
	case ERpgMotionMatchingDatabaseRole::Jump:
		return AirborneMotionMatchingDatabases.IsValidIndex(0)
			? AirborneMotionMatchingDatabases[0].Get()
			: nullptr;
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
		return LandingMotionMatchingDatabase.Get();
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
		return StandHeavyLandingMotionMatchingDatabase.Get();
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
		return WalkLightLandingMotionMatchingDatabase.Get();
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
		return WalkHeavyLandingMotionMatchingDatabase.Get();
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
		return RunLightLandingMotionMatchingDatabase.Get();
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return RunHeavyLandingMotionMatchingDatabase.Get();
	default:
		return nullptr;
	}
}

UPoseSearchDatabase* URpgAnimInstance::GetMotionMatchingDatabaseForRole(
	ERpgMotionMatchingDatabaseRole Role) const
{
	return GaspMotionMatchingDatabaseLookup.FindDatabase(Role);
}

URpgAnimInstance::FMotionMatchingDatabaseRoleContracts
URpgAnimInstance::BuildMotionMatchingDatabaseRoleContracts() const
{
	FMotionMatchingDatabaseRoleContracts Contracts;
	auto AddContract = [&Contracts, this](ERpgMotionMatchingDatabaseRole Role)
	{
		Contracts.Add({Role, GetLegacyMotionMatchingDatabaseForRole(Role)});
	};

	AddContract(ERpgMotionMatchingDatabaseRole::StandIdle);
	AddContract(ERpgMotionMatchingDatabaseRole::StandWalk);
	AddContract(ERpgMotionMatchingDatabaseRole::StandWalkStops);
	AddContract(ERpgMotionMatchingDatabaseRole::StandRunLoops);
	AddContract(ERpgMotionMatchingDatabaseRole::StandRunPivots);
	AddContract(ERpgMotionMatchingDatabaseRole::StandRunStarts);
	AddContract(ERpgMotionMatchingDatabaseRole::StandRunStops);
	AddContract(ERpgMotionMatchingDatabaseRole::StandSprint);
	AddContract(ERpgMotionMatchingDatabaseRole::StandSprintStops);
	AddContract(ERpgMotionMatchingDatabaseRole::Crouch);
	AddContract(ERpgMotionMatchingDatabaseRole::StandTurnInPlace);
	if (AirborneMotionMatchingDatabases.IsEmpty())
	{
		AddContract(ERpgMotionMatchingDatabaseRole::Jump);
	}
	else
	{
		for (UPoseSearchDatabase* Database : AirborneMotionMatchingDatabases)
		{
			Contracts.Add({ERpgMotionMatchingDatabaseRole::Jump, Database});
		}
	}
	AddContract(ERpgMotionMatchingDatabaseRole::StandLightLanding);
	AddContract(ERpgMotionMatchingDatabaseRole::StandHeavyLanding);
	AddContract(ERpgMotionMatchingDatabaseRole::WalkLightLanding);
	AddContract(ERpgMotionMatchingDatabaseRole::WalkHeavyLanding);
	AddContract(ERpgMotionMatchingDatabaseRole::RunLightLanding);
	AddContract(ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	return Contracts;
}

#if WITH_EDITOR
EDataValidationResult URpgAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);
	if (bGeneratePoseSearchTrajectory)
	{
		if (TrajectoryCollisionSettings.bEnabled &&
			!RpgPoseSearchTrajectory::IsCollisionSettingsRuntimeValid(
				TrajectoryCollisionSettings))
		{
			Context.AddError(FText::FromString(
				TEXT("Trajectory collision requires FloorOffset 0.001-10 cm, MaxObstacleHeight 1-1000 cm, SweepRadius 0.1-20 cm, 1-15 samples, and a serialized collision channel below ECC_OverlapAll_Deprecated.")));
		}

		const bool bHasProfileRuntimeConfiguration =
			GaspPresentationProfile &&
			!GaspPresentationProfile->RuntimeMotionMatchingDatabases.IsEmpty();
		if (!bHasProfileRuntimeConfiguration)
		{
			const FGroundMotionMatchingDatabaseSetValidation GroundDatabaseValidation =
				ValidateGroundMotionMatchingDatabaseSets(GroundMotionMatchingDatabaseSets);
			if (GroundDatabaseValidation.bHasInvalidShape)
			{
				Context.AddError(FText::FromString(
					TEXT("Ground Motion Matching database sets must contain Idle 1, Walk 2, Run 4, and Sprint 2 entries; Walk and Sprint are ordered Moving Aggregate, Stops, while Run is ordered Loops, Pivots, Starts, Stops.")));
			}
			if (GroundDatabaseValidation.bHasNullDatabase)
			{
				Context.AddError(FText::FromString(
					TEXT("Ground Motion Matching database sets contain at least one null entry.")));
			}
			if (GroundDatabaseValidation.bHasDuplicateDatabase)
			{
				Context.AddError(FText::FromString(
					TEXT("Ground Motion Matching database sets must not reuse a database within or across gait groups.")));
			}

			const FMotionMatchingDatabaseRoleValidation RoleValidation =
				ValidateMotionMatchingDatabaseRoleContracts(
					BuildMotionMatchingDatabaseRoleContracts());
			if (RoleValidation.bHasMissingRole || RoleValidation.bHasDuplicateRole)
			{
				Context.AddError(FText::FromString(
					TEXT("Motion Matching must configure exactly one database for every project runtime role.")));
			}
			if (RoleValidation.bHasNullDatabase)
			{
				Context.AddError(FText::FromString(
					TEXT("At least one project Motion Matching database role resolves to a null database.")));
			}
			if (RoleValidation.bHasDuplicateDatabase)
			{
				Context.AddError(FText::FromString(
					TEXT("A Pose Search database must not own more than one project Motion Matching role.")));
			}
			if (RoleValidation.bHasMissingRoleTag || RoleValidation.bHasDuplicateRoleTag ||
				RoleValidation.bHasWrongRoleTag)
			{
				Context.AddError(FText::FromString(
					TEXT("Every runtime Pose Search database must contain exactly its expected Rpg.MotionMatching.Role tag and no other project role tag.")));
			}
			if (RoleValidation.bHasMissingStateTag || RoleValidation.bHasDuplicateStateTag ||
				RoleValidation.bHasWrongStateTag)
			{
				Context.AddError(FText::FromString(
					TEXT("Every runtime Pose Search database must contain exactly its expected Rpg.MotionMatching.State tag and no other project state tag.")));
			}
			if (AirborneMotionMatchingDatabases.IsEmpty())
			{
				Context.AddError(FText::FromString(
					TEXT("Motion Matching is enabled, but no airborne Pose Search database is configured.")));
			}
			if (!FMath::IsFinite(HeavyLandingSpeedThreshold) ||
				HeavyLandingSpeedThreshold <= 0.0f)
			{
				Context.AddError(FText::FromString(
					TEXT("Heavy Landing Speed Threshold must be a finite positive speed in cm/s.")));
			}
			if (!LandingMotionMatchingDatabase ||
				!StandHeavyLandingMotionMatchingDatabase ||
				!WalkLightLandingMotionMatchingDatabase ||
				!WalkHeavyLandingMotionMatchingDatabase ||
				!RunLightLandingMotionMatchingDatabase ||
				!RunHeavyLandingMotionMatchingDatabase)
			{
				Context.AddError(FText::FromString(
					TEXT("Motion Matching requires all six curated Idle/Walk/Run Light/Heavy landing Pose Search databases.")));
			}
			if (!CrouchingMotionMatchingDatabase)
			{
				Context.AddError(FText::FromString(
					TEXT("Motion Matching is enabled, but no crouching Pose Search database is configured.")));
			}
			if (!TurnInPlaceMotionMatchingDatabase)
			{
				Context.AddError(FText::FromString(
					TEXT("Motion Matching is enabled, but no turn-in-place Pose Search database is configured.")));
			}
		}
		if (!GaspPresentationProfile)
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no GASP presentation profile is configured.")));
		}
		else
		{
			const FRpgGaspPresentationProfileValidation ProfileValidation =
				GaspPresentationProfile->ValidateProfile();
			const bool bProfileValid = bHasProfileRuntimeConfiguration
				? ProfileValidation.IsValid()
				: ProfileValidation.IsMembershipValid();
			if (!bProfileValid)
			{
				Context.AddError(FText::FromString(bHasProfileRuntimeConfiguration
					? TEXT("The configured GASP presentation profile has invalid membership, runtime databases, coverage, or tuning.")
					: TEXT("The configured GASP presentation profile has invalid presentation membership.")));
			}
			else if (!bHasProfileRuntimeConfiguration)
			{
				FRpgGaspPresentationAssetLookup PresentationLookup;
				const bool bBuiltPresentationLookup =
					PresentationLookup.Build(GaspPresentationProfile);
				check(bBuiltPresentationLookup);

				const auto DatabaseHasTrait = [&PresentationLookup](
					const UPoseSearchDatabase* Database,
					ERpgGaspPresentationAssetTrait RequiredTrait)
				{
					if (!Database)
					{
						return true;
					}

					for (int32 AssetIndex = 0;
						AssetIndex < Database->GetNumAnimationAssets();
						++AssetIndex)
					{
						const UAnimationAsset* Asset =
							Cast<UAnimationAsset>(Database->GetAnimationAsset(AssetIndex));
						if (!Asset || !PresentationLookup.HasTrait(Asset, RequiredTrait))
						{
							return false;
						}
					}
					return true;
				};

				static constexpr ERpgMotionMatchingDatabaseRole GroundMovingRoles[] = {
					ERpgMotionMatchingDatabaseRole::StandWalk,
					ERpgMotionMatchingDatabaseRole::StandWalkStops,
					ERpgMotionMatchingDatabaseRole::StandRunLoops,
					ERpgMotionMatchingDatabaseRole::StandRunPivots,
					ERpgMotionMatchingDatabaseRole::StandRunStarts,
					ERpgMotionMatchingDatabaseRole::StandRunStops,
					ERpgMotionMatchingDatabaseRole::StandSprint,
					ERpgMotionMatchingDatabaseRole::StandSprintStops,
				};
				bool bGroundMovingCoverageValid = true;
				for (const ERpgMotionMatchingDatabaseRole Role : GroundMovingRoles)
				{
					bGroundMovingCoverageValid &= DatabaseHasTrait(
						GetLegacyMotionMatchingDatabaseForRole(Role),
						ERpgGaspPresentationAssetTrait::GroundMoving);
				}
				if (!bGroundMovingCoverageValid)
				{
					Context.AddError(FText::FromString(
						TEXT("Every Walk, Run, and Sprint legacy runtime database asset must have GroundMoving presentation membership.")));
				}

				bool bAirborneCoverageValid = !AirborneMotionMatchingDatabases.IsEmpty();
				for (const UPoseSearchDatabase* Database : AirborneMotionMatchingDatabases)
				{
					bAirborneCoverageValid &= DatabaseHasTrait(
						Database,
						ERpgGaspPresentationAssetTrait::Airborne);
				}
				if (!bAirborneCoverageValid)
				{
					Context.AddError(FText::FromString(
						TEXT("Every legacy Airborne runtime database asset must have JumpStart, BackwardJumpStart, or AirborneFall presentation membership.")));
				}

				static constexpr ERpgMotionMatchingDatabaseRole LandingRoles[] = {
					ERpgMotionMatchingDatabaseRole::StandLightLanding,
					ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
					ERpgMotionMatchingDatabaseRole::WalkLightLanding,
					ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
					ERpgMotionMatchingDatabaseRole::RunLightLanding,
					ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
				};
				bool bLandingCoverageValid = true;
				for (const ERpgMotionMatchingDatabaseRole Role : LandingRoles)
				{
					bLandingCoverageValid &= DatabaseHasTrait(
						GetLegacyMotionMatchingDatabaseForRole(Role),
						ERpgGaspPresentationAssetTrait::Landing);
				}
				if (!bLandingCoverageValid)
				{
					Context.AddError(FText::FromString(
						TEXT("Every curated legacy runtime landing database asset must have Landing presentation membership.")));
				}
			}
		}
		if (FootPlacementSettings.bEnabled)
		{
			const FRpgFootPlacementLegDefinition* LegDefinitions[] = {
				&FootPlacementSettings.LeftLeg,
				&FootPlacementSettings.RightLeg,
			};
			for (int32 LegIndex = 0; LegIndex < UE_ARRAY_COUNT(LegDefinitions); ++LegIndex)
			{
				const FRpgFootPlacementLegDefinition& Leg = *LegDefinitions[LegIndex];
				if (Leg.FKFootBone.IsNone() || Leg.IKFootBone.IsNone() ||
					Leg.BallBone.IsNone() || Leg.SpeedCurveName.IsNone())
				{
					Context.AddError(FText::FromString(FString::Printf(
						TEXT("Foot Placement leg %d requires FK foot, IK foot, ball, and speed-curve names."),
						LegIndex)));
				}
			}
		}

		if (!bHasProfileRuntimeConfiguration)
		{
			const auto ValidateDatabases = [&Context](
				const TArray<TObjectPtr<UPoseSearchDatabase>>& Databases,
				const TCHAR* GroupName)
			{
				for (int32 Index = 0; Index < Databases.Num(); ++Index)
				{
					if (!Databases[Index])
					{
						Context.AddError(FText::FromString(FString::Printf(
							TEXT("%s Pose Search database entry %d is null."),
							GroupName,
							Index)));
					}
				}
			};
			ValidateDatabases(AirborneMotionMatchingDatabases, TEXT("Airborne"));
		}
	}

	return ((Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // WITH_EDITOR

void URpgAnimInstance::ResetPublishedCombatAnimationState()
{
	CombatEquippedUpperBodyAnimation = nullptr;
	CombatReadyUpperBodyAnimation = nullptr;
	CombatAnimationProfileName = TEXT("Unarmed");
	CombatAnimationOverlayAlpha = 0.0f;
	CombatModeBlendTime = 0.0f;
	bCombatAnimationReady = false;
	bCombatAnimationProfileFallback = true;
}

void URpgAnimInstance::SynchronizeCombatAnimationProfileProvider(
	const AActor* OwningActor,
	FRpgAnimInstanceProxy& Proxy)
{
	check(IsInGameThread());
	const URpgCombatAnimationProfileProviderComponent* DesiredProvider =
		URpgCombatAnimationProfileProviderComponent::FindForActor(OwningActor);
	URpgCombatAnimationProfile* DesiredProfile = DesiredProvider
		? DesiredProvider->GetCombatAnimationProfile()
		: nullptr;
	if (CombatAnimationProfileProvider.Get() == DesiredProvider &&
		ActiveCombatAnimationProfileSource == DesiredProfile)
	{
		return;
	}

	ClearCombatAnimationProfileProvider(Proxy);
	if (DesiredProvider && CombatAnimationProfileLookup.Build(DesiredProfile))
	{
		CombatAnimationProfileProvider =
			const_cast<URpgCombatAnimationProfileProviderComponent*>(DesiredProvider);
		ActiveCombatAnimationProfileSource = DesiredProfile;
	}
}

void URpgAnimInstance::HandleCombatAnimationProfileProviderUnregistering(
	const URpgCombatAnimationProfileProviderComponent* Provider)
{
	check(IsInGameThread());
	if (!Provider || CombatAnimationProfileProvider.Get() != Provider)
	{
		return;
	}

	FRpgAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRpgAnimInstanceProxy>();
	ClearCombatAnimationProfileProvider(Proxy);
}

void URpgAnimInstance::ClearCombatAnimationProfileProvider(
	FRpgAnimInstanceProxy& Proxy)
{
	// The profile keeps the raw lookup and proxy animation pointers alive. Clear every consumer
	// first, then release the weak provider and GC-strong profile references last.
	ResetCombatAnimationSnapshot(Proxy);
	ResetPublishedCombatAnimationState();
	CombatAnimationProfileLookup.Reset();
	CombatAnimationProfileProvider.Reset();
	ActiveCombatAnimationProfileSource = nullptr;
}

void URpgAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	InitializeGaspRuntimeConfiguration();
	CombatAnimationProfileLookup.Reset();
	CombatAnimationProfileProvider.Reset();
	ActiveCombatAnimationProfileSource = nullptr;
	FRpgAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRpgAnimInstanceProxy>();
	ResetFootPlacementInitializationState(Proxy);
	ResetCombatAnimationSnapshot(Proxy);
	SynchronizeCombatAnimationProfileProvider(TryGetPawnOwner(), Proxy);
	ResetPoseSearchTrajectoryState(Proxy);
	Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
	Proxy.LastGroundedGait = ERpgLocomotionGait::Idle;
	Proxy.LandingAirborneEpoch = 0;
	Proxy.bWasAirborneForLanding = false;
	TurnInPlaceRequestSerial = 0;
	TurnInPlaceInterruptedRequestSerial = 0;
	TurnInPlaceHardResetReasonsLastFrame = 0;
	bResetOffsetRootEveryFrame = false;
	bTurnInPlaceInitializationResetPending = true;
	FootPlacementSnapshot = FRpgFootPlacementSnapshot();
	FootPlacementAlpha = 0.0f;
	LocomotionTrajectory.Samples.Reset();
	TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
	PreTouchdownLandingSnapshot = FRpgLandingSelectionSnapshot();
	AirborneProceduralAlpha = 0.0f;
	UnarmedUpperBodyPostureCorrection = FRotator::ZeroRotator;
	ResetPublishedCombatAnimationState();
	LandingRequestSerial = 0;
	LandingInterruptedRequestSerial = 0;
	PreviousGroundMotionMatchingDomainState = FRpgGroundMotionMatchingDomainState();
	bHasPreviousGroundMotionMatchingDomainState = false;
	CurrentMotionMatchingDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
	bCurrentMotionMatchingResultIsContinuingPose = false;
	CurrentMotionMatchingInterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	PendingMotionMatchingInterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	MotionMatchingNodeUpdateCounter.Reset();
	ResetJumpPhaseRuntime();
	ResetTurnInPlaceRuntime(false);

	if (AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
		}
	}
}

void URpgAnimInstance::NativeUninitializeAnimation()
{
	FRpgAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRpgAnimInstanceProxy>();
	ClearCombatAnimationProfileProvider(Proxy);
	Super::NativeUninitializeAnimation();
}

FRpgTurnInPlaceRuntimeState URpgAnimInstance::CaptureTurnInPlaceRuntimeState() const
{
	FRpgTurnInPlaceRuntimeState State;
	State.State = TurnInPlaceState;
	State.QueryAngle = TurnInPlaceQueryAngle;
	State.AccumulatedYaw = TurnInPlaceAccumulatedYaw;
	State.StateElapsed = TurnInPlaceStateElapsed;
	State.StableElapsed = TurnInPlaceStableElapsed;
	State.SelectionElapsed = TurnInPlaceSelectionElapsed;
	State.PlaybackWatchdogDuration = TurnInPlacePlaybackWatchdogDuration;
	State.RequestAccumulatedYaw = TurnInPlaceRequestAccumulatedYaw;
	State.RequestSerial = TurnInPlaceRequestSerial;
	State.InterruptedRequestSerial = TurnInPlaceInterruptedRequestSerial;
	State.HardResetReasonsLastFrame = TurnInPlaceHardResetReasonsLastFrame;
	return State;
}

void URpgAnimInstance::ApplyTurnInPlaceRuntimeResult(
	const FRpgTurnInPlaceUpdateResult& Result)
{
	TurnInPlaceState = Result.State.State;
	TurnInPlaceQueryAngle = Result.State.QueryAngle;
	TurnInPlaceAccumulatedYaw = Result.State.AccumulatedYaw;
	TurnInPlaceStateElapsed = Result.State.StateElapsed;
	TurnInPlaceStableElapsed = Result.State.StableElapsed;
	TurnInPlaceSelectionElapsed = Result.State.SelectionElapsed;
	TurnInPlacePlaybackWatchdogDuration = Result.State.PlaybackWatchdogDuration;
	TurnInPlaceRequestAccumulatedYaw = Result.State.RequestAccumulatedYaw;
	TurnInPlaceRequestSerial = Result.State.RequestSerial;
	TurnInPlaceInterruptedRequestSerial = Result.State.InterruptedRequestSerial;
	TurnInPlaceHardResetReasonsLastFrame = Result.State.HardResetReasonsLastFrame;
	OffsetRootRotationMode = Result.OffsetRootRotationMode;
	bResetOffsetRootEveryFrame |= Result.bResetOffsetRootEveryFrame;
	if (Result.bClearSelection)
	{
		ClearTurnInPlaceSelection();
	}
}

bool URpgAnimInstance::UsesProfileRuntimeConfiguration() const
{
	return bUseProfileRuntimeConfiguration;
}

void URpgAnimInstance::InitializeGaspRuntimeConfiguration()
{
	const FRpgGaspPresentationProfileValidation Validation = GaspPresentationProfile
		? GaspPresentationProfile->ValidateProfile()
		: FRpgGaspPresentationProfileValidation();
	GaspPresentationAssetLookup.BuildValidated(GaspPresentationProfile, Validation);
	GaspMotionMatchingDatabaseLookup.Reset();
	RuntimeGaspLocomotionTuning = FRpgGaspLocomotionTuning();
	bUseProfileRuntimeConfiguration = GaspPresentationProfile &&
		!GaspPresentationProfile->RuntimeMotionMatchingDatabases.IsEmpty();

	if (bUseProfileRuntimeConfiguration)
	{
		if (Validation.IsValid() &&
			GaspMotionMatchingDatabaseLookup.BuildValidated(
				GaspPresentationProfile,
				Validation))
		{
			RuntimeGaspLocomotionTuning = GaspPresentationProfile->LocomotionTuning;
		}
		return;
	}

	// Legacy serialization remains reversible, but workers receive no partial configuration from an
	// already invalid CDO: all 18 unique non-null slots enter the cache together or the cache stays empty.
	const FMotionMatchingDatabaseRoleContracts LegacyContracts =
		BuildMotionMatchingDatabaseRoleContracts();
	bool bBuiltCompleteLegacyLookup =
		LegacyContracts.Num() ==
			static_cast<int32>(ERpgMotionMatchingDatabaseRole::Count) - 1;
	for (const FMotionMatchingDatabaseRoleContract& Contract : LegacyContracts)
	{
		if (!bBuiltCompleteLegacyLookup ||
			!GaspMotionMatchingDatabaseLookup.AddResolvedBinding(
				Contract.Role,
				Contract.Database))
		{
			bBuiltCompleteLegacyLookup = false;
			break;
		}
	}
	if (!bBuiltCompleteLegacyLookup)
	{
		GaspMotionMatchingDatabaseLookup.Reset();
	}

	// Whole-legacy compatibility mode retains the one historical AnimBP-authored feel value.
	RuntimeGaspLocomotionTuning.HeavyLandingSpeedThreshold = HeavyLandingSpeedThreshold;
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveConfiguredMotionMatchingDatabaseRole(
	const UPoseSearchDatabase* Database) const
{
	return GaspMotionMatchingDatabaseLookup.FindRole(Database);
}

void URpgAnimInstance::ClearTurnInPlaceSelection()
{
	TurnInPlaceSelectedAsset = nullptr;
	TurnInPlaceSelectedAssetStartTime = 0.0f;
	TurnInPlaceSelectedAssetReentryTime = -1.0f;
	TurnInPlaceSelectedAssetRemainingTime = MAX_flt;
	TurnInPlaceSelectedRequestSerial = 0;
	TurnInPlacePlaybackWatchdogDuration = RuntimeGaspLocomotionTuning.TurnActiveTimeout;
	bTurnInPlacePoseSelected = false;
	bTurnInPlaceSelectedAssetLooping = false;
	bTurnInPlaceSelectionLatched = false;
	bTurnInPlacePlaybackObserved = false;
	bTurnInPlaceCompletionArmed = false;
}

void URpgAnimInstance::ResetTurnInPlaceRuntime(bool bHardResetOffset)
{
	ApplyTurnInPlaceRuntimeResult(RpgTurnInPlaceRuntime::Reset(
		CaptureTurnInPlaceRuntimeState(),
		bHardResetOffset,
		RuntimeGaspLocomotionTuning));
	TurnInPlaceSyntheticTrajectory.Samples.Reset();
}

void URpgAnimInstance::BeginTurnInPlaceRecovery(bool bHardResetOffset)
{
	ApplyTurnInPlaceRuntimeResult(RpgTurnInPlaceRuntime::BeginRecovery(
		CaptureTurnInPlaceRuntimeState(),
		bHardResetOffset,
		RuntimeGaspLocomotionTuning));
	TurnInPlaceSyntheticTrajectory.Samples.Reset();
}

void URpgAnimInstance::BeginTurnInPlaceRequest(float QuantizedAngle)
{
	ApplyTurnInPlaceRuntimeResult(RpgTurnInPlaceRuntime::BeginRequest(
		CaptureTurnInPlaceRuntimeState(),
		QuantizedAngle,
		RuntimeGaspLocomotionTuning));
}

void URpgAnimInstance::UpdateTurnInPlaceRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy)
{
	bResetOffsetRootEveryFrame = bTurnInPlaceInitializationResetPending;
	bTurnInPlaceInitializationResetPending = false;

	FRpgTurnInPlaceEligibilitySnapshot Eligibility;
	Eligibility.RotationMode = Proxy.RotationMode;
	Eligibility.MovementState = Proxy.MovementState;
	Eligibility.GroundSpeed = Proxy.GroundSpeed;
	Eligibility.bHasTurnDatabase = GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole::StandTurnInPlace) != nullptr;
	Eligibility.bJumpPhaseGrounded = JumpPhase == ERpgJumpPhase::Grounded;
	Eligibility.bIsMovingOnGround = Proxy.bIsMovingOnGround;
	Eligibility.bIsCrouching = Proxy.bIsCrouching;
	Eligibility.bIsAnyMontagePlaying = Proxy.bIsAnyMontagePlaying;
	Eligibility.bHasBlockingGameplayTag = Proxy.bHasTurnInPlaceBlockingGameplayTag;
	Eligibility.bHasGroundedMoveIntent = Proxy.bHasGroundedMoveIntent;
	Eligibility.bHasTrajectory = !Proxy.TransformTrajectory.Samples.IsEmpty();

	FRpgTurnInPlaceUpdateSnapshot Snapshot;
	Snapshot.RotationMode = Proxy.RotationMode;
	Snapshot.MovementState = Proxy.MovementState;
	Snapshot.ActorYawDelta = Proxy.ActorYawDelta;
	Snapshot.bEligible = RpgTurnInPlaceRuntime::IsEligible(
		Eligibility,
		RuntimeGaspLocomotionTuning);
	Snapshot.bProxyHardReset = Proxy.bTurnInPlaceHardReset;
	Snapshot.bSupportChanged = Proxy.bTurnInPlaceSupportChanged;
	Snapshot.bHasBlockingGameplayTag = Proxy.bHasTurnInPlaceBlockingGameplayTag;
	Snapshot.bIsAnyMontagePlaying = Proxy.bIsAnyMontagePlaying;
	Snapshot.bIsCrouching = Proxy.bIsCrouching;
	Snapshot.bSelectionLatched = bTurnInPlaceSelectionLatched;
	Snapshot.bPlaybackObserved = bTurnInPlacePlaybackObserved;
	Snapshot.bPoseSelected = bTurnInPlacePoseSelected;
	Snapshot.bCompletionArmed = bTurnInPlaceCompletionArmed;

	const FRpgTurnInPlaceUpdateResult Result = RpgTurnInPlaceRuntime::Update(
		CaptureTurnInPlaceRuntimeState(),
		Snapshot,
		DeltaSeconds,
		RuntimeGaspLocomotionTuning);
	ApplyTurnInPlaceRuntimeResult(Result);

	if (Result.bUseSyntheticTrajectory)
	{
		TurnInPlaceSyntheticTrajectory = RpgTurnInPlaceRuntime::MakeSyntheticTrajectory(
			Proxy.TransformTrajectory,
			Proxy.ActorYaw,
			TurnInPlaceAccumulatedYaw,
			TurnInPlaceQueryAngle,
			Proxy.MeshBasisRotation,
			RuntimeGaspLocomotionTuning);
		LocomotionTrajectory = TurnInPlaceSyntheticTrajectory;
	}
	else
	{
		TurnInPlaceSyntheticTrajectory.Samples.Reset();
		LocomotionTrajectory = Proxy.TransformTrajectory;
	}
}

bool URpgAnimInstance::ConsumeTurnInPlaceForceInterruptRequest()
{
	FRpgTurnInPlaceRuntimeState State = CaptureTurnInPlaceRuntimeState();
	const bool bConsumed = RpgTurnInPlaceRuntime::ConsumeForceInterrupt(
		GetMotionMatchingDatabaseForRole(
			ERpgMotionMatchingDatabaseRole::StandTurnInPlace) != nullptr,
		State);
	TurnInPlaceInterruptedRequestSerial = State.InterruptedRequestSerial;
	return bConsumed;
}

bool URpgAnimInstance::TryLatchTurnInPlaceSelection(
	UAnimationAsset* SelectedAsset,
	const UPoseSearchDatabase* SelectedDatabase,
	float SelectedTime,
	bool bSelectedAssetLooping,
	uint32 SelectionRequestSerial)
{
	if (TurnInPlaceState != ERpgTurnInPlaceState::Active ||
		bTurnInPlaceSelectionLatched ||
		!SelectedAsset ||
		SelectedDatabase != GetMotionMatchingDatabaseForRole(
			ERpgMotionMatchingDatabaseRole::StandTurnInPlace) ||
		SelectionRequestSerial == 0 ||
		SelectionRequestSerial != TurnInPlaceRequestSerial)
	{
		return false;
	}

	TurnInPlaceSelectedAsset = SelectedAsset;
	TurnInPlaceSelectedAssetStartTime = FMath::Max(SelectedTime, 0.0f);
	TurnInPlaceSelectedAssetReentryTime = -1.0f;
	GaspPresentationAssetLookup.FindTurnInPlaceReentryTime(
		SelectedAsset, TurnInPlaceSelectedAssetReentryTime);
	TurnInPlaceSelectedAssetRemainingTime = FMath::Max(
		SelectedAsset->GetPlayLength() - TurnInPlaceSelectedAssetStartTime,
		0.0f);
	TurnInPlaceSelectedRequestSerial = SelectionRequestSerial;
	TurnInPlacePlaybackWatchdogDuration = RpgTurnInPlaceRuntime::CalculatePlaybackWatchdogDuration(
		TurnInPlaceSelectedAssetRemainingTime,
		1.0f,
		bSelectedAssetLooping,
		RuntimeGaspLocomotionTuning);
	TurnInPlaceStateElapsed = 0.0f;
	bTurnInPlacePoseSelected = true;
	bTurnInPlaceSelectedAssetLooping = bSelectedAssetLooping;
	bTurnInPlaceSelectionLatched = true;
	bTurnInPlacePlaybackObserved = false;
	bTurnInPlaceCompletionArmed = false;
	return true;
}

void URpgAnimInstance::UpdateTurnInPlaceLatchedPlayback(
	UAnimationAsset* CurrentAsset,
	float CurrentAssetTime,
	float CurrentAssetLength,
	float CurrentAssetPlayRate,
	float DeltaSeconds)
{
	if (TurnInPlaceState != ERpgTurnInPlaceState::Active ||
		!bTurnInPlaceSelectionLatched ||
		TurnInPlaceSelectedRequestSerial != TurnInPlaceRequestSerial ||
		!TurnInPlaceSelectedAsset)
	{
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	const float CompletionLeadTime =
		RpgTurnInPlaceRuntime::FinishedTimeTolerance + SafeDeltaSeconds;
	if (CurrentAsset == TurnInPlaceSelectedAsset.Get())
	{
		const bool bFirstPlaybackObservation = !bTurnInPlacePlaybackObserved;
		bTurnInPlacePlaybackObserved = true;
		bTurnInPlacePoseSelected = true;
		const float EffectiveAssetLength = CurrentAssetLength > UE_SMALL_NUMBER
			? CurrentAssetLength
			: TurnInPlaceSelectedAsset->GetPlayLength();
		const float ClampedAssetTime = FMath::Clamp(CurrentAssetTime, 0.0f, EffectiveAssetLength);
		TurnInPlaceSelectedAssetRemainingTime = CurrentAssetPlayRate < 0.0f
			? ClampedAssetTime : EffectiveAssetLength - ClampedAssetTime;
		if (bFirstPlaybackObservation)
		{
			TurnInPlacePlaybackWatchdogDuration =
				RpgTurnInPlaceRuntime::CalculatePlaybackWatchdogDuration(
				TurnInPlaceSelectedAssetRemainingTime,
				CurrentAssetPlayRate,
				bTurnInPlaceSelectedAssetLooping,
				RuntimeGaspLocomotionTuning);
			TurnInPlaceStateElapsed = 0.0f;
		}
		const bool bHasForwardReentryMarker =
			TurnInPlaceSelectedAssetReentryTime > 0.0f &&
			FMath::IsFinite(CurrentAssetPlayRate) && CurrentAssetPlayRate > UE_SMALL_NUMBER;
		// Authored contact safety is an exact asset-time boundary, never a frame-early completion hint.
		const bool bPlaybackFinished = bHasForwardReentryMarker
			? FMath::IsFinite(CurrentAssetTime) && CurrentAssetTime >= TurnInPlaceSelectedAssetReentryTime
			: RpgAnimationPlaybackRuntime::RemainingPlaybackSeconds(
				CurrentAssetTime, EffectiveAssetLength, CurrentAssetPlayRate) <= CompletionLeadTime;
		if (!bTurnInPlaceSelectedAssetLooping && bPlaybackFinished)
		{
			bTurnInPlaceCompletionArmed = true;
		}
		return;
	}

	if (!bTurnInPlacePlaybackObserved)
	{
		// The completed-search callback owns this selection. Keep that latch authoritative through a
		// transient Blend Stack mismatch until the exact asset is observed.
		bTurnInPlacePoseSelected = true;
		return;
	}

	if (bTurnInPlaceCompletionArmed)
	{
		bTurnInPlacePoseSelected = true;
		bTurnInPlaceCompletionArmed = true;
		return;
	}

	bTurnInPlacePoseSelected = false;
}

void URpgAnimInstance::ClearLandingSelection()
{
	LandingSelectedAsset = nullptr;
	LandingSelectedAssetStartTime = 0.0f;
	LandingSelectedAssetRemainingTime = MAX_flt;
	LandingPlaybackWatchdogDuration = RuntimeGaspLocomotionTuning.LandingActiveTimeout;
	LandingSelectedRequestSerial = 0;
	bLandingSelectedAssetLooping = false;
	bLandingSelectionLatched = false;
	bLandingPlaybackObserved = false;
	bLandingCompletionArmed = false;
}

void URpgAnimInstance::ClearBackwardJumpStartHold()
{
	ApplyBackwardJumpStartHoldResult(
		RpgJumpRuntime::ResetBackwardJumpStartHold(),
		nullptr);
}

FRpgBackwardJumpStartHoldState URpgAnimInstance::CaptureBackwardJumpStartHoldState() const
{
	FRpgBackwardJumpStartHoldState State;
	State.HoldElapsed = BackwardJumpStartHoldElapsed;
	State.bOpportunityConsumed = bBackwardJumpStartHoldOpportunityConsumed;
	State.bHoldEligible = bBackwardJumpStartHoldEligible;
	State.bHoldWasArmed = bBackwardJumpStartHoldWasArmed;
	return State;
}

void URpgAnimInstance::ApplyBackwardJumpStartHoldResult(
	const FRpgBackwardJumpStartHoldResult& Result,
	UAnimationAsset* CurrentAsset)
{
	BackwardJumpStartHoldElapsed = Result.State.HoldElapsed;
	bBackwardJumpStartHoldOpportunityConsumed = Result.State.bOpportunityConsumed;
	bBackwardJumpStartHoldEligible = Result.State.bHoldEligible;
	bBackwardJumpStartHoldWasArmed = Result.State.bHoldWasArmed;
	if (Result.bCaptureCurrentAsset)
	{
		BackwardJumpStartHeldAsset = CurrentAsset;
	}
	if (Result.bClearHeldAsset)
	{
		// A first observed backward start may already be past the bounded hold window.
		// Match the previous capture-then-release order so no stale GC pointer survives.
		BackwardJumpStartHeldAsset = nullptr;
	}
}

FRpgLandingRuntimeState URpgAnimInstance::CaptureLandingRuntimeState() const
{
	FRpgLandingRuntimeState State;
	State.ActiveRole = ActiveLandingDatabaseRole;
	State.StateElapsed = LandingStateElapsed;
	State.TouchdownElapsed = LandingTouchdownElapsed;
	State.PlaybackWatchdogDuration = LandingPlaybackWatchdogDuration;
	State.RequestSerial = LandingRequestSerial;
	State.InterruptedRequestSerial = LandingInterruptedRequestSerial;
	State.bSelectionLatched = bLandingSelectionLatched;
	State.bCompletionArmed = bLandingCompletionArmed;
	return State;
}

void URpgAnimInstance::ApplyLandingRuntimeResult(const FRpgLandingRuntimeResult& Result)
{
	ActiveLandingDatabaseRole = Result.State.ActiveRole;
	LandingStateElapsed = Result.State.StateElapsed;
	LandingTouchdownElapsed = Result.State.TouchdownElapsed;
	LandingPlaybackWatchdogDuration = Result.State.PlaybackWatchdogDuration;
	LandingRequestSerial = Result.State.RequestSerial;
	LandingInterruptedRequestSerial = Result.State.InterruptedRequestSerial;
	bLandingSelectionLatched = Result.State.bSelectionLatched;
	bLandingCompletionArmed = Result.State.bCompletionArmed;

	switch (Result.Transition)
	{
	case ERpgLandingRuntimeTransition::BeginLanding:
		JumpPhase = ERpgJumpPhase::Landing;
		break;
	case ERpgLandingRuntimeTransition::ResetGrounded:
		JumpPhase = ERpgJumpPhase::Grounded;
		break;
	case ERpgLandingRuntimeTransition::None:
		break;
	}
	if (Result.bClearSelection)
	{
		ClearLandingSelection();
	}
	if (Result.bClearBackwardHold)
	{
		ClearBackwardJumpStartHold();
	}
}

FRpgLandingDatabaseAvailability URpgAnimInstance::BuildLandingDatabaseAvailability() const
{
	FRpgLandingDatabaseAvailability Availability;
	Availability.bStandLight = GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole::StandLightLanding) != nullptr;
	Availability.bStandHeavy = GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding) != nullptr;
	Availability.bWalkLight = GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole::WalkLightLanding) != nullptr;
	Availability.bWalkHeavy = GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding) != nullptr;
	Availability.bRunLight = GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole::RunLightLanding) != nullptr;
	Availability.bRunHeavy = GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding) != nullptr;
	return Availability;
}

FRpgLandingEligibilitySnapshot URpgAnimInstance::BuildLandingEligibilitySnapshot(
	const FRpgAnimInstanceProxy& Proxy)
{
	FRpgLandingEligibilitySnapshot Snapshot;
	Snapshot.MovementState = Proxy.MovementState;
	Snapshot.bIsMovingOnGround = Proxy.bIsMovingOnGround;
	Snapshot.bIsCrouching = Proxy.bIsCrouching;
	Snapshot.bIsAnyMontagePlaying = Proxy.bIsAnyMontagePlaying;
	Snapshot.bHasBlockingGameplayTag = Proxy.bHasTurnInPlaceBlockingGameplayTag;
	return Snapshot;
}

void URpgAnimInstance::ResetJumpPhaseRuntime()
{
	ApplyLandingRuntimeResult(RpgLandingRuntime::Reset(
		CaptureLandingRuntimeState(),
		RuntimeGaspLocomotionTuning));
}

void URpgAnimInstance::BeginAirbornePhase(bool bAscendingTakeoff)
{
	ApplyLandingRuntimeResult(RpgLandingRuntime::Reset(
		CaptureLandingRuntimeState(),
		RuntimeGaspLocomotionTuning));
	JumpPhase = ERpgJumpPhase::Airborne;
	bBackwardJumpStartHoldEligible = bAscendingTakeoff;
}

void URpgAnimInstance::BeginLandingRequest(
	ERpgMotionMatchingDatabaseRole LandingRole,
	bool bForceInterrupt)
{
	check(RpgMotionMatchingRuntime::IsLandingDatabaseRole(LandingRole));
	check(GetMotionMatchingDatabaseForRole(LandingRole));
	ApplyLandingRuntimeResult(RpgLandingRuntime::BeginRequest(
		CaptureLandingRuntimeState(),
		LandingRole,
		bForceInterrupt,
		RuntimeGaspLocomotionTuning));
}

void URpgAnimInstance::UpdateJumpPhaseRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy)
{
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	FRpgJumpPhysicalSnapshot PhysicalSnapshot;
	PhysicalSnapshot.MovementState = Proxy.MovementState;
	PhysicalSnapshot.VerticalVelocity = Proxy.VerticalVelocity;
	PhysicalSnapshot.bIsFalling = Proxy.bIsFalling;
	PhysicalSnapshot.bHardReset = Proxy.bTurnInPlaceHardReset;
	const FRpgJumpPhysicalTransitionResult PhysicalTransition =
		RpgJumpRuntime::ResolvePhysicalTransition(JumpPhase, PhysicalSnapshot);
	switch (PhysicalTransition.Transition)
	{
	case ERpgJumpPhysicalTransition::EnterAirborne:
		BeginAirbornePhase(PhysicalTransition.bAscendingTakeoff);
		return;
	case ERpgJumpPhysicalTransition::ResetGrounded:
		ResetJumpPhaseRuntime();
		return;
	case ERpgJumpPhysicalTransition::None:
		if (JumpPhase == ERpgJumpPhase::Airborne)
		{
			return;
		}
		break;
	case ERpgJumpPhysicalTransition::Touchdown:
		break;
	}

	FRpgGroundMotionMatchingSelectionSnapshot LiveGroundSnapshot;
	LiveGroundSnapshot.Gait = Proxy.Gait;
	LiveGroundSnapshot.Stance = Proxy.Stance;
	LiveGroundSnapshot.MovementState = Proxy.MovementState;
	LiveGroundSnapshot.RotationMode = Proxy.RotationMode;
	LiveGroundSnapshot.WorldVelocity = Proxy.WorldVelocity;
	LiveGroundSnapshot.WorldAcceleration = Proxy.WorldAcceleration;
	LiveGroundSnapshot.GroundSpeed = Proxy.GroundSpeed;
	LiveGroundSnapshot.bIsMovingOnGround = Proxy.bIsMovingOnGround;
	const bool bChooserMoving = RpgMotionMatchingRuntime::IsChooserMoving(
		LiveGroundSnapshot,
		RuntimeGaspLocomotionTuning);

	if (PhysicalTransition.Transition == ERpgJumpPhysicalTransition::Touchdown)
	{
		const ERpgMotionMatchingDatabaseRole LandingRole =
			RpgLandingRuntime::ResolveTouchdownRole(
				Proxy.LandingSelectionSnapshot,
				BuildLandingEligibilitySnapshot(Proxy),
				BuildLandingDatabaseAvailability(),
				Proxy.Gait,
				Proxy.GroundSpeed,
				bChooserMoving,
				RuntimeGaspLocomotionTuning);
		if (LandingRole != ERpgMotionMatchingDatabaseRole::None)
		{
			BeginLandingRequest(LandingRole);
		}
		else
		{
			ResetJumpPhaseRuntime();
		}
		return;
	}

	if (JumpPhase != ERpgJumpPhase::Landing)
	{
		ResetJumpPhaseRuntime();
		return;
	}

	FRpgLandingActiveSnapshot ActiveSnapshot;
	ActiveSnapshot.Eligibility = BuildLandingEligibilitySnapshot(Proxy);
	ActiveSnapshot.Availability = BuildLandingDatabaseAvailability();
	ActiveSnapshot.LiveGait = Proxy.Gait;
	ActiveSnapshot.GroundSpeed = Proxy.GroundSpeed;
	ActiveSnapshot.bChooserMoving = bChooserMoving;
	ApplyLandingRuntimeResult(RpgLandingRuntime::UpdateActive(
		CaptureLandingRuntimeState(),
		ActiveSnapshot,
		SafeDeltaSeconds,
		RuntimeGaspLocomotionTuning));
}

bool URpgAnimInstance::ConsumeLandingForceInterruptRequest()
{
	FRpgLandingRuntimeState State = CaptureLandingRuntimeState();
	const bool bConsumed = RpgLandingRuntime::ConsumeForceInterrupt(
		JumpPhase == ERpgJumpPhase::Landing,
		GetMotionMatchingDatabaseForRole(ActiveLandingDatabaseRole) != nullptr,
		State);
	LandingInterruptedRequestSerial = State.InterruptedRequestSerial;
	return bConsumed;
}

bool URpgAnimInstance::TryLatchLandingSelection(
	UAnimationAsset* SelectedAsset,
	const UPoseSearchDatabase* SelectedDatabase,
	float SelectedTime,
	bool bSelectedAssetLooping,
	uint32 SelectionRequestSerial)
{
	if (JumpPhase != ERpgJumpPhase::Landing ||
		bLandingSelectionLatched ||
		!SelectedAsset ||
		SelectedDatabase != GetMotionMatchingDatabaseForRole(ActiveLandingDatabaseRole) ||
		SelectionRequestSerial == 0 ||
		SelectionRequestSerial != LandingRequestSerial)
	{
		return false;
	}

	LandingSelectedAsset = SelectedAsset;
	LandingSelectedAssetStartTime = FMath::Max(SelectedTime, 0.0f);
	LandingSelectedAssetRemainingTime = FMath::Max(
		SelectedAsset->GetPlayLength() - LandingSelectedAssetStartTime,
		0.0f);
	LandingSelectedRequestSerial = SelectionRequestSerial;
	bLandingSelectedAssetLooping = bSelectedAssetLooping;
	LandingPlaybackWatchdogDuration = RpgLandingRuntime::CalculatePlaybackWatchdogDuration(
		LandingSelectedAssetRemainingTime,
		1.0f,
		bLandingSelectedAssetLooping,
		RuntimeGaspLocomotionTuning);
	LandingStateElapsed = 0.0f;
	bLandingSelectionLatched = true;
	bLandingPlaybackObserved = false;
	bLandingCompletionArmed = false;
	return true;
}

void URpgAnimInstance::UpdateLandingLatchedPlayback(
	UAnimationAsset* CurrentAsset,
	float CurrentAssetTime,
	float CurrentAssetLength,
	float CurrentAssetPlayRate,
	float DeltaSeconds)
{
	if (JumpPhase != ERpgJumpPhase::Landing ||
		!bLandingSelectionLatched ||
		LandingSelectedRequestSerial != LandingRequestSerial ||
		!LandingSelectedAsset)
	{
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	const float CompletionLeadTime =
		RpgLandingRuntime::FinishedTimeTolerance + SafeDeltaSeconds;
	if (CurrentAsset == LandingSelectedAsset.Get())
	{
		const bool bFirstPlaybackObservation = !bLandingPlaybackObserved;
		bLandingPlaybackObserved = true;
		const float EffectiveAssetLength = CurrentAssetLength > UE_SMALL_NUMBER
			? CurrentAssetLength
			: LandingSelectedAsset->GetPlayLength();
		LandingSelectedAssetRemainingTime = FMath::Max(
			EffectiveAssetLength - FMath::Max(CurrentAssetTime, 0.0f),
			0.0f);
		if (bFirstPlaybackObservation)
		{
			LandingPlaybackWatchdogDuration =
				RpgLandingRuntime::CalculatePlaybackWatchdogDuration(
				LandingSelectedAssetRemainingTime,
				CurrentAssetPlayRate,
				bLandingSelectedAssetLooping,
				RuntimeGaspLocomotionTuning);
			LandingStateElapsed = 0.0f;
		}
		if (!bLandingSelectedAssetLooping &&
			LandingSelectedAssetRemainingTime <= CompletionLeadTime)
		{
			bLandingCompletionArmed = true;
		}
		return;
	}

	if (bLandingPlaybackObserved)
	{
		// Once the exact latched clip has played, an unexpected Blend Stack replacement ends the cosmetic hold.
		bLandingCompletionArmed = true;
	}
}

bool URpgAnimInstance::IsActiveLandingAsset(const UAnimationAsset* Asset) const
{
	return JumpPhase == ERpgJumpPhase::Landing &&
		bLandingSelectionLatched &&
		!bLandingCompletionArmed &&
		Asset &&
		Asset == LandingSelectedAsset.Get();
}

bool URpgAnimInstance::IsLandingAsset(const UAnimationAsset* Asset) const
{
	return GaspPresentationAssetLookup.HasTrait(
		Asset,
		ERpgGaspPresentationAssetTrait::Landing);
}

bool URpgAnimInstance::IsAirborneJumpStartAsset(const UAnimationAsset* Asset) const
{
	return GaspPresentationAssetLookup.HasTrait(
		Asset,
		ERpgGaspPresentationAssetTrait::JumpStart);
}

bool URpgAnimInstance::IsGroundMovingAsset(const UAnimationAsset* Asset) const
{
	return GaspPresentationAssetLookup.HasTrait(
		Asset,
		ERpgGaspPresentationAssetTrait::GroundMoving);
}

bool URpgAnimInstance::IsAirborneJumpAsset(const UAnimationAsset* Asset) const
{
	return GaspPresentationAssetLookup.HasTrait(
		Asset,
		ERpgGaspPresentationAssetTrait::Airborne);
}

bool URpgAnimInstance::IsBackwardJumpStartAsset(const UAnimationAsset* Asset) const
{
	return GaspPresentationAssetLookup.HasTrait(
		Asset,
		ERpgGaspPresentationAssetTrait::BackwardJumpStart);
}

bool URpgAnimInstance::IsLoopingAirborneFallAsset(const UAnimationAsset* Asset) const
{
	return GaspPresentationAssetLookup.HasTrait(
		Asset,
		ERpgGaspPresentationAssetTrait::AirborneFall);
}

bool URpgAnimInstance::UpdateBackwardJumpStartHold(
	UAnimationAsset* CurrentAsset,
	float CurrentAssetTime,
	float CurrentAssetLength,
	float CurrentAssetPlayRate,
	float DeltaSeconds)
{
	const FRpgBackwardJumpStartHoldState State = CaptureBackwardJumpStartHoldState();
	FRpgBackwardJumpStartPlaybackSnapshot Snapshot;
	Snapshot.JumpPhase = JumpPhase;
	Snapshot.CurrentAssetTime = CurrentAssetTime;
	Snapshot.CurrentAssetLength = CurrentAssetLength;
	Snapshot.CurrentAssetPlayRate = CurrentAssetPlayRate;
	Snapshot.DeltaSeconds = DeltaSeconds;
	if (JumpPhase == ERpgJumpPhase::Airborne && !State.bOpportunityConsumed)
	{
		Snapshot.bCurrentAssetIsAirborne = IsAirborneJumpAsset(CurrentAsset);
		Snapshot.bCurrentAssetIsBackwardStart =
			State.bHoldEligible &&
			Snapshot.bCurrentAssetIsAirborne &&
			IsBackwardJumpStartAsset(CurrentAsset);
	}
	Snapshot.bHasHeldAsset = BackwardJumpStartHeldAsset != nullptr;
	Snapshot.bCurrentAssetMatchesHeld =
		CurrentAsset && CurrentAsset == BackwardJumpStartHeldAsset.Get();
	const FRpgBackwardJumpStartHoldResult Result =
		RpgJumpRuntime::UpdateBackwardJumpStartHold(
			State,
			Snapshot,
			RuntimeGaspLocomotionTuning);
	ApplyBackwardJumpStartHoldResult(Result, CurrentAsset);
	return Result.bHoldContinuingPose;
}

URpgAnimInstance::FGaspProceduralGates URpgAnimInstance::ResolveGaspProceduralGates(
	bool bGroundMovingPose,
	float SupportedLocomotionAlpha,
	bool bAirborneJumpPose,
	bool bAirborneJumpStartPose,
	bool bLandingPose,
	float EnableWarpingCurveValue,
	bool bHasActiveBlendStackAsset,
	bool bHasTrajectory)
{
	const float SafeSupportedAlpha = FMath::Clamp(SupportedLocomotionAlpha, 0.0f, 1.0f);
	const float SafeGroundAlpha = bGroundMovingPose
		? SafeSupportedAlpha
		: 0.0f;
	const float SafeAirborneResetAlpha = bAirborneJumpPose
		? SafeSupportedAlpha
		: 0.0f;
	const float SafeAirborneMovingAlpha = bAirborneJumpStartPose
		? SafeSupportedAlpha
		: 0.0f;
	const float LandingAlpha = bLandingPose ? 1.0f : 0.0f;
	const float MovingCorrectionAlpha = FMath::Max(SafeGroundAlpha, SafeAirborneMovingAlpha);

	FGaspProceduralGates Result;
	Result.ResetRootAlpha = FMath::Max3(SafeGroundAlpha, SafeAirborneResetAlpha, LandingAlpha);
	Result.OrientationWarpingAlpha =
		MovingCorrectionAlpha * FMath::Clamp(EnableWarpingCurveValue, 0.0f, 1.0f);
	Result.bEnableSteering =
		MovingCorrectionAlpha > UE_KINDA_SMALL_NUMBER &&
		bHasActiveBlendStackAsset &&
		bHasTrajectory;
	return Result;
}

void URpgAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	const FRpgAnimInstanceProxy& Proxy = GetProxyOnAnyThread<FRpgAnimInstanceProxy>();
	WorldVelocity = Proxy.WorldVelocity;
	LastNonZeroWorldVelocity = Proxy.LastNonZeroWorldVelocity;
	LocalVelocity = Proxy.LocalVelocity;
	WorldAcceleration = Proxy.WorldAcceleration;
	LocalAcceleration = Proxy.LocalAcceleration;
	AnimationHistoryResetCount = Proxy.AnimationHistoryResetCount;
	LocomotionGroundSpeed = Proxy.GroundSpeed;
	VerticalVelocity = Proxy.VerticalVelocity;
	GroundDistance = Proxy.GroundDistance;
	FootPlacementSnapshot = Proxy.FootPlacementSnapshot;
	FootPlacementAlpha = Proxy.FootPlacementAlpha;
	AimYaw = Proxy.AimYaw;
	AimPitch = Proxy.AimPitch;
	LocomotionAngle = Proxy.LocomotionAngle;
	bHasVelocity = Proxy.bHasVelocity;
	bHasAcceleration = Proxy.bHasAcceleration;
	bLocomotionIsFalling = Proxy.bIsFalling;
	bIsMovingOnGround = Proxy.bIsMovingOnGround;
	bIsCrouching = Proxy.bIsCrouching;
	LocomotionGait = Proxy.Gait;
	LocomotionStance = Proxy.Stance;
	LocomotionMovementState = Proxy.MovementState;
	const bool bAllowRelaxedPostureCorrection =
		RpgGaspPostureRuntime::ShouldApplyCorrection(
			Proxy.RotationMode,
			Proxy.bCombatAnimationProfileFallback);
	UnarmedUpperBodyPostureCorrection = FRotator(
		0.0f,
		0.0f,
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			UnarmedUpperBodyPostureCorrection.Roll,
			LocomotionGait,
			bAllowRelaxedPostureCorrection,
			DeltaSeconds,
			RuntimeGaspLocomotionTuning));
	CharacterRotationMode = Proxy.RotationMode;
	LocomotionTrajectory = Proxy.TransformTrajectory;
	TrajectoryLandingPrediction = Proxy.TrajectoryLandingPrediction;
	PreTouchdownLandingSnapshot = Proxy.LandingSelectionSnapshot;
	ProceduralLocomotionAlpha = Proxy.ProceduralLocomotionAlpha;
	AirborneProceduralAlpha = Proxy.AirborneProceduralAlpha;
	bIsAnyMontagePlaying = Proxy.bIsAnyMontagePlaying;
	CombatEquippedUpperBodyAnimation =
		Proxy.CombatEquippedUpperBodyAnimation;
	CombatReadyUpperBodyAnimation =
		Proxy.CombatReadyUpperBodyAnimation;
	CombatAnimationProfileName = Proxy.CombatAnimationProfileName;
	CombatAnimationOverlayAlpha = Proxy.CombatAnimationOverlayAlpha;
	CombatModeBlendTime = Proxy.CombatModeBlendTime;
	bCombatAnimationReady = Proxy.bCombatAnimationReady;
	bCombatAnimationProfileFallback =
		Proxy.bCombatAnimationProfileFallback;
	UpdateJumpPhaseRuntime(DeltaSeconds, Proxy);
	UpdateTurnInPlaceRuntime(DeltaSeconds, Proxy);
}

void URpgAnimInstance::UpdateGaspMotionMatching(
	const FAnimUpdateContext& Context,
	const FAnimNodeReference& Node)
{
	FAnimNode_MotionMatching* MotionMatchingNode = Node.GetAnimNodePtr<FAnimNode_MotionMatching>();
	if (!MotionMatchingNode)
	{
		return;
	}

	const FAnimationUpdateContext* AnimationContext = Context.GetContext();
	const bool bNodeBecameRelevant =
		AnimationContext && AnimationContext->AnimInstanceProxy &&
		SynchronizeMotionMatchingNodeUpdateCounter(
			MotionMatchingNodeUpdateCounter,
			AnimationContext->AnimInstanceProxy->GetUpdateCounter());

	// The bound UpdateFunction runs before UE applies bResetOnBecomingRelevant. Mirror the node's
	// traversal-counter gate so a stale Pivot result cannot suppress the first Run Start after re-entry.
	CurrentMotionMatchingDatabaseRole = bNodeBecameRelevant
		? ERpgMotionMatchingDatabaseRole::None
		: ResolveConfiguredMotionMatchingDatabaseRole(
			MotionMatchingNode->GetMotionMatchingState().SearchResult.SelectedDatabase.Get());
	if (CurrentMotionMatchingDatabaseRole == ERpgMotionMatchingDatabaseRole::None)
	{
		bCurrentMotionMatchingResultIsContinuingPose = false;
		CurrentMotionMatchingInterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	}

	const FRpgAnimInstanceProxy& Proxy = GetProxyOnAnyThread<FRpgAnimInstanceProxy>();
	FRpgGroundMotionMatchingSelectionSnapshot DomainSnapshot;
	DomainSnapshot.Gait = Proxy.Gait;
	DomainSnapshot.Stance = Proxy.Stance;
	DomainSnapshot.MovementState = Proxy.MovementState;
	DomainSnapshot.RotationMode = Proxy.RotationMode;
	DomainSnapshot.WorldVelocity = Proxy.WorldVelocity;
	DomainSnapshot.WorldAcceleration = Proxy.WorldAcceleration;
	DomainSnapshot.GroundSpeed = Proxy.GroundSpeed;
	DomainSnapshot.CurrentDatabaseRole = CurrentMotionMatchingDatabaseRole;
	DomainSnapshot.bIsMovingOnGround = Proxy.bIsMovingOnGround;

	FRpgGroundMotionMatchingDomainState CurrentGroundDomainState;
	CurrentGroundDomainState.PhysicalMovementState = Proxy.MovementState;
	CurrentGroundDomainState.Gait = Proxy.Gait;
	CurrentGroundDomainState.Stance = Proxy.Stance;
	CurrentGroundDomainState.bChooserMoving =
		RpgMotionMatchingRuntime::IsChooserMoving(
			DomainSnapshot,
			RuntimeGaspLocomotionTuning);
	const bool bInterruptGroundDomain = RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching(
		bHasPreviousGroundMotionMatchingDomainState,
		PreviousGroundMotionMatchingDomainState,
		CurrentGroundDomainState);

	const bool bForceNewTurnRequest = ConsumeTurnInPlaceForceInterruptRequest();

	if (TurnInPlaceState == ERpgTurnInPlaceState::Active && bTurnInPlaceSelectionLatched)
	{
		UpdateTurnInPlaceLatchedPlayback(
			MotionMatchingNode->GetAnimAsset(),
			MotionMatchingNode->GetCurrentAssetTime(),
			MotionMatchingNode->GetCurrentAssetLength(),
			MotionMatchingNode->AnimPlayers.IsEmpty()
				? 1.0f
				: MotionMatchingNode->AnimPlayers[0].GetPlayRate(),
			AnimationContext ? AnimationContext->GetDeltaTime() : 0.0f);
	}

	const ERpgTurnInPlaceSearchMode SearchMode =
		RpgTurnInPlaceRuntime::ResolveSearchMode(
			CaptureTurnInPlaceRuntimeState(),
			bForceNewTurnRequest,
			GetMotionMatchingDatabaseForRole(
				ERpgMotionMatchingDatabaseRole::StandTurnInPlace) != nullptr,
			bTurnInPlaceSelectionLatched,
			bTurnInPlaceCompletionArmed);
	const bool bForceNewLandingRequest =
		SearchMode == ERpgTurnInPlaceSearchMode::NormalLocomotion &&
		ConsumeLandingForceInterruptRequest();

	if (JumpPhase == ERpgJumpPhase::Landing && bLandingSelectionLatched)
	{
		UpdateLandingLatchedPlayback(
			MotionMatchingNode->GetAnimAsset(),
			MotionMatchingNode->GetCurrentAssetTime(),
			MotionMatchingNode->GetCurrentAssetLength(),
			MotionMatchingNode->AnimPlayers.IsEmpty()
				? 1.0f
				: MotionMatchingNode->AnimPlayers[0].GetPlayRate(),
			AnimationContext ? AnimationContext->GetDeltaTime() : 0.0f);
	}

	const UAnimationAsset* CurrentMotionMatchingAsset = MotionMatchingNode->GetAnimAsset();
	const float CurrentMotionMatchingPlayRate = MotionMatchingNode->AnimPlayers.IsEmpty()
		? 1.0f
		: MotionMatchingNode->AnimPlayers[0].GetPlayRate();
	const bool bHoldBackwardJumpStart = UpdateBackwardJumpStartHold(
		MotionMatchingNode->GetAnimAsset(),
		MotionMatchingNode->GetCurrentAssetTime(),
		MotionMatchingNode->GetCurrentAssetLength(),
		CurrentMotionMatchingPlayRate,
		AnimationContext ? AnimationContext->GetDeltaTime() : 0.0f);
	const bool bContinueLoopingAirborneFall = RpgJumpRuntime::ShouldHoldLoopingAirborneFallPlayback(
		JumpPhase,
		bBackwardJumpStartHoldWasArmed,
		VerticalVelocity,
		IsLoopingAirborneFallAsset(CurrentMotionMatchingAsset));
	const bool bInterruptLandingDatabaseExit = RpgLandingRuntime::ShouldInterruptDatabaseExit(
		JumpPhase,
		bLandingCompletionArmed,
		CurrentMotionMatchingDatabaseRole);

	TArray<UPoseSearchDatabase*, TInlineAllocator<5>> DatabasesToSearch;
	EPoseSearchInterruptMode InterruptMode = EPoseSearchInterruptMode::InterruptOnDatabaseChange;
	if (SearchMode == ERpgTurnInPlaceSearchMode::SearchRequestedTurn)
	{
		DatabasesToSearch.Add(GetMotionMatchingDatabaseForRole(
			ERpgMotionMatchingDatabaseRole::StandTurnInPlace));
		InterruptMode = EPoseSearchInterruptMode::ForceInterrupt;
	}
	else if (SearchMode == ERpgTurnInPlaceSearchMode::ContinueSelectedTurn)
	{
		// An empty searchable set with DoNotInterrupt keeps only the current database's Continuing Pose.
		// It prevents a full TIR-database search from selecting another clip when the indexed range ends.
		InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	}
	else if (JumpPhase == ERpgJumpPhase::Landing && !bLandingCompletionArmed)
	{
		if (bLandingSelectionLatched)
		{
			// Preserve only the continuing pose; a full landing-database search must happen once per touchdown.
			InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
		}
		else if (UPoseSearchDatabase* LandingDatabase =
			GetMotionMatchingDatabaseForRole(ActiveLandingDatabaseRole))
		{
			DatabasesToSearch.Add(LandingDatabase);
			InterruptMode = bForceNewLandingRequest
				? EPoseSearchInterruptMode::ForceInterrupt
				: EPoseSearchInterruptMode::InterruptOnDatabaseChange;
		}
	}
	else if (bHoldBackwardJumpStart || bContinueLoopingAirborneFall)
	{
		// Continuing-pose-only playback prevents the full Jump DB from restarting a leg phase mid-air.
		// The start fails open near clip end or at its watchdog; the fall releases on landing or upward relaunch.
		InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	}
	else
	{
		FVector FutureTrajectoryVelocity = FVector::ZeroVector;
		if (!Proxy.TransformTrajectory.Samples.IsEmpty())
		{
			// This is the exact future-speed window used by GASP's IsStarting function.
			UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(
				Proxy.TransformTrajectory,
				RuntimeGaspLocomotionTuning.RunStartFutureVelocityBeginTime,
				RuntimeGaspLocomotionTuning.RunStartFutureVelocityEndTime,
				FutureTrajectoryVelocity);
		}

		FRpgGroundMotionMatchingSelectionSnapshot SelectionSnapshot = DomainSnapshot;
		SelectionSnapshot.FutureVelocity = FutureTrajectoryVelocity;
		if (JumpPhase == ERpgJumpPhase::Airborne || bLocomotionIsFalling)
		{
			SelectionSnapshot.MovementState = ERpgLocomotionMovementState::Airborne;
		}

		for (const ERpgMotionMatchingDatabaseRole Role :
			RpgMotionMatchingRuntime::ResolveDatabaseRoles(
				SelectionSnapshot,
				RuntimeGaspLocomotionTuning))
		{
			if (UPoseSearchDatabase* Database = GetMotionMatchingDatabaseForRole(Role))
			{
				DatabasesToSearch.AddUnique(Database);
			}
		}

		if (SelectionSnapshot.MovementState == ERpgLocomotionMovementState::Grounded &&
			SelectionSnapshot.Stance == ERpgLocomotionStance::Standing)
		{
			InterruptMode = bInterruptGroundDomain || bInterruptLandingDatabaseExit
				? EPoseSearchInterruptMode::InterruptOnDatabaseChange
				: EPoseSearchInterruptMode::DoNotInterrupt;
		}
	}

	if (Proxy.bTurnInPlaceHardReset)
	{
		// A discontinuity invalidates the current Continuing Pose even when the resolved
		// database set happens to remain unchanged across the correction or teleport.
		InterruptMode = EPoseSearchInterruptMode::ForceInterruptAndInvalidateContinuingPose;
		CurrentMotionMatchingDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
		bCurrentMotionMatchingResultIsContinuingPose = false;
	}

	PreviousGroundMotionMatchingDomainState = CurrentGroundDomainState;
	bHasPreviousGroundMotionMatchingDomainState = true;
	PendingMotionMatchingInterruptMode = InterruptMode;

	MotionMatchingNode->SetDatabasesToSearch(
		MakeArrayView(DatabasesToSearch),
		InterruptMode);
}

void URpgAnimInstance::UpdateGaspMotionMatchingPostSelection(
	const FAnimUpdateContext& Context,
	const FAnimNodeReference& Node)
{
	(void)Context;
	FAnimNode_MotionMatching* MotionMatchingNode = Node.GetAnimNodePtr<FAnimNode_MotionMatching>();
	if (!MotionMatchingNode)
	{
		return;
	}

	const FPoseSearchBlueprintResult& SearchResult =
		MotionMatchingNode->GetMotionMatchingState().SearchResult;
	const ERpgMotionMatchingDatabaseRole SelectedRole =
		ResolveConfiguredMotionMatchingDatabaseRole(SearchResult.SelectedDatabase.Get());
	const FRpgMotionMatchingPostSelectionState PostSelection =
		RpgMotionMatchingRuntime::ResolvePostSelection(
			SelectedRole,
			SearchResult.bIsContinuingPoseSearch,
			PendingMotionMatchingInterruptMode,
			TurnInPlaceState == ERpgTurnInPlaceState::Active &&
				!bTurnInPlaceSelectionLatched,
			JumpPhase == ERpgJumpPhase::Landing &&
				!bLandingSelectionLatched &&
				SelectedRole == ActiveLandingDatabaseRole);

	CurrentMotionMatchingDatabaseRole = PostSelection.CurrentDatabaseRole;
	bCurrentMotionMatchingResultIsContinuingPose = PostSelection.bIsContinuingPose;
	CurrentMotionMatchingInterruptMode = PostSelection.InterruptMode;
	if (PostSelection.bShouldLatchTurnInPlace)
	{
		TryLatchTurnInPlaceSelection(
			Cast<UAnimationAsset>(SearchResult.SelectedAnim.Get()),
			SearchResult.SelectedDatabase.Get(),
			SearchResult.SelectedTime,
			SearchResult.bLoop,
			TurnInPlaceRequestSerial);
	}
	if (PostSelection.bShouldLatchLanding)
	{
		TryLatchLandingSelection(
			Cast<UAnimationAsset>(SearchResult.SelectedAnim.Get()),
			SearchResult.SelectedDatabase.Get(),
			SearchResult.SelectedTime,
			SearchResult.bLoop,
			LandingRequestSerial);
	}
}

void URpgAnimInstance::GetGaspBlendStackInputs(
	const FAnimNodeReference& Node,
	UAnimationAsset*& CurrentAnimAsset,
	float& CurrentAnimAssetTime,
	float& ResetRootAlpha,
	float& OrientationWarpingAlpha,
	FQuat& DesiredFacing,
	FVector& LocomotionDirection,
	bool& bEnableSteering) const
{
	CurrentAnimAsset = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(Node);
	CurrentAnimAssetTime = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAssetTime(Node);

	// Each proxy budget is binary for its supported CMC state (ground includes idle). Taking the
	// maximum keeps outgoing ground/fall samples corrected across both sides of the transition.
	const float SupportedLocomotionAlpha = FMath::Max(
		ProceduralLocomotionAlpha,
		AirborneProceduralAlpha);
	const bool bAllowsSampleProceduralNodes =
		!bIsCrouching &&
		!bIsAnyMontagePlaying &&
		RpgTurnInPlaceRuntime::AllowsMovingProceduralNodes(TurnInPlaceState);

	float EnableWarpingCurveValue = 0.0f;
	if (const UAnimSequenceBase* CurrentSequence = Cast<UAnimSequenceBase>(CurrentAnimAsset))
	{
		static const FName EnableWarpingCurveName(TEXT("Enable_Warping"));
		float AuthoredCurveValue = 0.0f;
		if (UAnimationWarpingLibrary::GetCurveValueFromAnimation(
			CurrentSequence,
			EnableWarpingCurveName,
			CurrentAnimAssetTime,
			AuthoredCurveValue))
		{
			EnableWarpingCurveValue = FMath::Clamp(AuthoredCurveValue, 0.0f, 1.0f);
		}
	}

	LocomotionDirection = LastNonZeroWorldVelocity;
	const bool bHasTrajectory = !LocomotionTrajectory.Samples.IsEmpty();
	const bool bHasActiveBlendStackAsset =
		CurrentAnimAsset != nullptr &&
		UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimIsActive(Node);
	DesiredFacing = bHasTrajectory
		? LocomotionTrajectory.GetSampleAtTime(0.0f).Facing
		: FQuat::Identity;

	const bool bIsGroundMovingPose =
		bAllowsSampleProceduralNodes &&
		(IsGroundMovingAsset(CurrentAnimAsset) ||
		 (IsActiveLandingAsset(CurrentAnimAsset) &&
		  (ActiveLandingDatabaseRole == ERpgMotionMatchingDatabaseRole::WalkLightLanding ||
		   ActiveLandingDatabaseRole == ERpgMotionMatchingDatabaseRole::WalkHeavyLanding ||
		   ActiveLandingDatabaseRole == ERpgMotionMatchingDatabaseRole::RunLightLanding ||
		   ActiveLandingDatabaseRole == ERpgMotionMatchingDatabaseRole::RunHeavyLanding)));
	const bool bIsAirborneJumpPose =
		bAllowsSampleProceduralNodes &&
		IsAirborneJumpAsset(CurrentAnimAsset);
	const bool bIsAirborneJumpStartPose =
		bIsAirborneJumpPose &&
		IsAirborneJumpStartAsset(CurrentAnimAsset);
	const bool bIsLandingPose = IsLandingAsset(CurrentAnimAsset);
	const FGaspProceduralGates Gates = ResolveGaspProceduralGates(
		bIsGroundMovingPose,
		SupportedLocomotionAlpha,
		bIsAirborneJumpPose,
		bIsAirborneJumpStartPose,
		bIsLandingPose,
		EnableWarpingCurveValue,
		bHasActiveBlendStackAsset,
		bHasTrajectory);
	ResetRootAlpha = Gates.ResetRootAlpha;
	OrientationWarpingAlpha = Gates.OrientationWarpingAlpha;
	bEnableSteering = Gates.bEnableSteering;
}

FAnimInstanceProxy* URpgAnimInstance::CreateAnimInstanceProxy()
{
	return new FRpgAnimInstanceProxy(this);
}

void URpgAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FRpgAnimInstanceProxy*>(InProxy);
}

