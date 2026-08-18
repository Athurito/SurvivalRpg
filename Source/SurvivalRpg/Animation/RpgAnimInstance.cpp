// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgAnimInstance.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAnimInstance)

namespace
{
constexpr float TurnInPlaceIdleSpeedThreshold = 3.0f;
constexpr float TurnInPlaceCollectThreshold = 20.0f;
constexpr float TurnInPlaceActivationThreshold = 30.0f;
constexpr float TurnInPlaceCancelThreshold = 10.0f;
constexpr float TurnInPlaceInactiveYawRateThreshold = 6.0f;
constexpr float TurnInPlaceStableYawRateThreshold = 60.0f;
constexpr float TurnInPlaceStabilityDuration = 0.08f;
constexpr float TurnInPlaceCollectionTimeout = 0.2f;
constexpr float TurnInPlaceRecoveryDuration = 0.15f;
constexpr float TurnInPlaceSelectionTimeout = 0.25f;
constexpr float TurnInPlaceActiveTimeout = 1.75f;
constexpr float TurnInPlacePlaybackWatchdogSafetyMargin = 0.1f;
constexpr float TurnInPlaceInactiveAccumulatorTimeout = 0.2f;
constexpr float TurnInPlaceFinishedTimeTolerance = 0.05f;
constexpr float TurnInPlaceLargePositionDelta = 200.0f;
constexpr float LandingSelectionTimeout = 0.25f;
constexpr float LandingActiveTimeout = 1.25f;
constexpr float LandingPlaybackWatchdogSafetyMargin = 0.1f;
constexpr float LandingFinishedTimeTolerance = 0.05f;
constexpr float LightLandingIdleSpeedThreshold = 3.0f;
constexpr float LandingMovementHandoffWindow = 0.3f;
constexpr float BackwardJumpStartHoldTimeout = 1.25f;
constexpr float BackwardJumpStartReleaseLeadTime = 0.2f;
void ResetPoseSearchTrajectoryState(FRpgAnimInstanceProxy& Proxy)
{
	Proxy.RawTransformTrajectory.Samples.Reset();
	Proxy.TransformTrajectory.Samples.Reset();
	Proxy.TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
	Proxy.DesiredControllerYawLastUpdate = 0.0f;
}

enum class ETurnInPlaceHardResetReason : uint8
{
	ProxySnapshot = 1 << 0,
	UnsupportedRotationMode = 1 << 1,
	BlockingGameplayTag = 1 << 2,
	Montage = 1 << 3,
	Crouch = 1 << 4,
	UnsupportedMovementState = 1 << 5,
};

constexpr uint8 ToReasonMask(ETurnInPlaceHardResetReason Reason)
{
	return static_cast<uint8>(Reason);
}

constexpr bool SupportsTurnInPlace(ERpgCharacterRotationMode RotationMode)
{
	return RotationMode == ERpgCharacterRotationMode::CombatStrafe ||
		RotationMode == ERpgCharacterRotationMode::Aim;
}

float CalculateTurnInPlacePlaybackWatchdogDuration(
	float RemainingAnimationTime,
	float PlayRate,
	bool bLooping)
{
	if (bLooping || !FMath::IsFinite(PlayRate) || FMath::Abs(PlayRate) <= UE_SMALL_NUMBER)
	{
		return TurnInPlaceActiveTimeout;
	}

	return FMath::Max(
		TurnInPlaceActiveTimeout,
		FMath::Max(RemainingAnimationTime, 0.0f) / FMath::Abs(PlayRate) +
			TurnInPlacePlaybackWatchdogSafetyMargin);
}

float CalculateLandingPlaybackWatchdogDuration(
	float RemainingAnimationTime,
	float PlayRate,
	bool bLooping)
{
	if (bLooping || !FMath::IsFinite(PlayRate) || FMath::Abs(PlayRate) <= UE_SMALL_NUMBER)
	{
		return LandingActiveTimeout;
	}

	return FMath::Clamp(
		FMath::Max(RemainingAnimationTime, 0.0f) / FMath::Abs(PlayRate) +
			LandingPlaybackWatchdogSafetyMargin,
		LandingPlaybackWatchdogSafetyMargin,
		LandingActiveTimeout);
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

	const bool bLargeComponentJump =
		bHadPreviousComponentTransform &&
		Snapshot.ComponentDeltaWorld.SizeSquared() > FMath::Square(TurnInPlaceLargePositionDelta);
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
		bLargeComponentJump ||
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

float URpgAnimInstance::QuantizeTurnInPlaceAngle(float SignedAngle)
{
	const float AbsoluteAngle = FMath::Min(FMath::Abs(SignedAngle), 180.0f);
	if (AbsoluteAngle < TurnInPlaceActivationThreshold)
	{
		return 0.0f;
	}

	float QuantizedMagnitude = 180.0f;
	if (AbsoluteAngle < 67.5f)
	{
		QuantizedMagnitude = 45.0f;
	}
	else if (AbsoluteAngle < 112.5f)
	{
		QuantizedMagnitude = 90.0f;
	}
	else if (AbsoluteAngle < 157.5f)
	{
		QuantizedMagnitude = 135.0f;
	}

	return SignedAngle < 0.0f ? -QuantizedMagnitude : QuantizedMagnitude;
}

float URpgAnimInstance::GetTurnInPlaceFacingDuration(float QuantizedAngle)
{
	const float AbsoluteAngle = FMath::Abs(QuantizedAngle);
	if (AbsoluteAngle <= 45.0f)
	{
		return 0.45f;
	}
	if (AbsoluteAngle <= 90.0f)
	{
		return 0.65f;
	}
	if (AbsoluteAngle <= 135.0f)
	{
		return 0.85f;
	}
	return 1.0f;
}

float URpgAnimInstance::CalculateTurnInPlaceYawDelta(float PreviousActorYaw, float CurrentActorYaw)
{
	return FMath::FindDeltaAngleDegrees(PreviousActorYaw, CurrentActorYaw);
}

bool URpgAnimInstance::DidTurnInPlaceSupportChange(
	bool bHasPreviousSnapshot,
	ERpgCharacterRotationMode PreviousMode,
	ERpgCharacterRotationMode CurrentMode)
{
	return bHasPreviousSnapshot && SupportsTurnInPlace(PreviousMode) != SupportsTurnInPlace(CurrentMode);
}

float URpgAnimInstance::CalculateTurnInPlaceSnapshotYawDelta(
	float PreviousActorYaw,
	float CurrentActorYaw,
	bool bHardReset,
	bool bSupportChanged)
{
	return bHardReset || bSupportChanged
		? 0.0f
		: CalculateTurnInPlaceYawDelta(PreviousActorYaw, CurrentActorYaw);
}

FTransformTrajectory URpgAnimInstance::MakeTurnInPlaceSyntheticTrajectory(
	const FTransformTrajectory& SourceTrajectory,
	float CurrentActorYaw,
	float AccumulatedYaw,
	float QuantizedAngle)
{
	FTransformTrajectory Result;
	const float FacingDuration = GetTurnInPlaceFacingDuration(QuantizedAngle);
	const float StartYaw = CurrentActorYaw - AccumulatedYaw;

	Result.Samples.Reserve(SourceTrajectory.Samples.Num());
	for (const FTransformTrajectorySample& SourceSample : SourceTrajectory.Samples)
	{
		FTransformTrajectorySample& Sample = Result.Samples.Add_GetRef(SourceSample);
		const float FacingAlpha = Sample.TimeInSeconds <= 0.0f
			? 0.0f
			: FMath::Clamp(Sample.TimeInSeconds / FacingDuration, 0.0f, 1.0f);
		Sample.Facing = FRotator(0.0f, StartYaw + QuantizedAngle * FacingAlpha, 0.0f).Quaternion();
	}
	return Result;
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

	const bool bIsRemoteAutonomousMoveOnListenServer =
		World &&
		World->GetNetMode() == NM_ListenServer &&
		Character &&
		Character->GetLocalRole() == ROLE_Authority &&
		Character->GetRemoteRole() == ROLE_AutonomousProxy &&
		MeshComponent &&
		MeshComponent->bOnlyAllowAutonomousTickPose &&
		MeshComponent->bIsAutonomousTickPose;

	// Parallel updates collapse several autonomous move pose ticks into the last move delta.
	// Updating this narrow path immediately preserves the full animation time and notify order.
	return !bIsRemoteAutonomousMoveOnListenServer;
}

void FRpgAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	Super::PreUpdate(InAnimInstance, DeltaSeconds);

	const ERpgLocomotionGait PreviousGait = Gait;
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
	ActorYaw = 0.0f;
	ActorYawDelta = 0.0f;
	ActorLocation = FVector::ZeroVector;
	TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
	FootPlacementSnapshot = FRpgFootPlacementSnapshot();
	FootPlacementAlpha = 0.0f;

	const URpgAnimInstance* RpgAnimInstance = Cast<URpgAnimInstance>(InAnimInstance);
	bHasTurnInPlaceBlockingGameplayTag =
		RpgAnimInstance &&
		(RpgAnimInstance->bGameplayMovementStopped ||
		 RpgAnimInstance->bStateBlocking ||
		 RpgAnimInstance->bStateDead ||
		 RpgAnimInstance->bStateStaggered ||
		 RpgAnimInstance->bStateGuardBroken);
	const ARpgCharacter* Character = RpgAnimInstance ? Cast<ARpgCharacter>(RpgAnimInstance->TryGetPawnOwner()) : nullptr;
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
		bHasPreviousOwnerSnapshot = false;
		return;
	}
	RotationMode = Character->GetRotationMode();

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
		bHasPreviousOwnerSnapshot = false;
		return;
	}

	const FQuat ActorRotation = Character->GetActorQuat();
	ActorYaw = Character->GetActorRotation().Yaw;
	ActorLocation = Character->GetActorLocation();
	const uint32 OwnerUniqueId = Character->GetUniqueID();
	const uint8 LocalRole = static_cast<uint8>(Character->GetLocalRole());
	const uint8 RemoteRole = static_cast<uint8>(Character->GetRemoteRole());
	const bool bOwnerOrRoleChanged =
		!bHasPreviousOwnerSnapshot ||
		PreviousOwnerUniqueId != OwnerUniqueId ||
		PreviousLocalRole != LocalRole ||
		PreviousRemoteRole != RemoteRole;
	bTurnInPlaceSupportChanged = URpgAnimInstance::DidTurnInPlaceSupportChange(
		bHasPreviousOwnerSnapshot,
		PreviousRotationMode,
		RotationMode);
	const bool bLargePositionJump =
		bHasPreviousOwnerSnapshot &&
		FVector::DistSquared(PreviousActorLocation, ActorLocation) > FMath::Square(TurnInPlaceLargePositionDelta);
	bTurnInPlaceHardReset = bOwnerOrRoleChanged || bLargePositionJump || MovementComponent->bJustTeleported;
	if (bTurnInPlaceHardReset)
	{
		RawTransformTrajectory.Samples.Reset();
		TransformTrajectory.Samples.Reset();
		TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
		DesiredControllerYawLastUpdate = 0.0f;
	}
	ActorYawDelta = URpgAnimInstance::CalculateTurnInPlaceSnapshotYawDelta(
		PreviousActorYaw,
		ActorYaw,
		bTurnInPlaceHardReset,
		bTurnInPlaceSupportChanged);

	PreviousOwnerUniqueId = OwnerUniqueId;
	PreviousLocalRole = LocalRole;
	PreviousRemoteRole = RemoteRole;
	PreviousActorYaw = ActorYaw;
	PreviousActorLocation = ActorLocation;
	PreviousRotationMode = RotationMode;
	bHasPreviousOwnerSnapshot = true;

	WorldVelocity = MovementComponent->Velocity;
	const FVector HorizontalWorldVelocity(WorldVelocity.X, WorldVelocity.Y, 0.0f);
	constexpr float LastNonZeroVelocityThreshold = 5.0f;
	if (HorizontalWorldVelocity.SizeSquared() >= FMath::Square(LastNonZeroVelocityThreshold))
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

	constexpr float IdleSpeedThreshold = 3.0f;
	const float MaxAcceleration = FMath::Max(MovementComponent->GetMaxAcceleration(), 1.0f);
	const float InputMagnitude = WorldAcceleration.Size2D() / MaxAcceleration;
	bHasGroundedMoveIntent = bIsMovingOnGround && InputMagnitude > 0.1f;
	if (!bIsMovingOnGround || (GroundSpeed < IdleSpeedThreshold && !bHasGroundedMoveIntent))
	{
		Gait = ERpgLocomotionGait::Idle;
	}
	else if (bHasGroundedMoveIntent)
	{
		Gait = InputMagnitude < 0.65f
			? ERpgLocomotionGait::Walk
			: ERpgLocomotionGait::Run;
	}
	else
	{
		// Keep the last moving database through deceleration so a stop pose is not interrupted
		// just because input acceleration reached zero before capsule velocity did.
		Gait = PreviousGait == ERpgLocomotionGait::Walk
			? ERpgLocomotionGait::Walk
			: ERpgLocomotionGait::Run;
	}

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

	const FRotator AimDelta = (Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
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
	URpgAnimInstance::UpdateLandingSelectionSnapshot(
		*this,
		InputMagnitude,
		GravityAcceleration);
}

void URpgAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

FName URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(
	ERpgMotionMatchingDatabaseRole Role)
{
	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandIdle:
		return TEXT("Rpg.MotionMatching.Role.StandIdle");
	case ERpgMotionMatchingDatabaseRole::StandWalk:
		return TEXT("Rpg.MotionMatching.Role.StandWalk");
	case ERpgMotionMatchingDatabaseRole::StandWalkStops:
		return TEXT("Rpg.MotionMatching.Role.StandWalkStops");
	case ERpgMotionMatchingDatabaseRole::StandRunLoops:
		return TEXT("Rpg.MotionMatching.Role.StandRunLoops");
	case ERpgMotionMatchingDatabaseRole::StandRunPivots:
		return TEXT("Rpg.MotionMatching.Role.StandRunPivots");
	case ERpgMotionMatchingDatabaseRole::StandRunStarts:
		return TEXT("Rpg.MotionMatching.Role.StandRunStarts");
	case ERpgMotionMatchingDatabaseRole::StandRunStops:
		return TEXT("Rpg.MotionMatching.Role.StandRunStops");
	case ERpgMotionMatchingDatabaseRole::StandSprint:
		return TEXT("Rpg.MotionMatching.Role.StandSprint");
	case ERpgMotionMatchingDatabaseRole::StandSprintStops:
		return TEXT("Rpg.MotionMatching.Role.StandSprintStops");
	case ERpgMotionMatchingDatabaseRole::Crouch:
		return TEXT("Rpg.MotionMatching.Role.Crouch");
	case ERpgMotionMatchingDatabaseRole::StandTurnInPlace:
		return TEXT("Rpg.MotionMatching.Role.StandTurnInPlace");
	case ERpgMotionMatchingDatabaseRole::Jump:
		return TEXT("Rpg.MotionMatching.Role.Jump");
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
		return TEXT("Rpg.MotionMatching.Role.StandLightLanding");
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
		return TEXT("Rpg.MotionMatching.Role.StandHeavyLanding");
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
		return TEXT("Rpg.MotionMatching.Role.WalkLightLanding");
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
		return TEXT("Rpg.MotionMatching.Role.WalkHeavyLanding");
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
		return TEXT("Rpg.MotionMatching.Role.RunLightLanding");
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return TEXT("Rpg.MotionMatching.Role.RunHeavyLanding");
	default:
		return NAME_None;
	}
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

bool URpgAnimInstance::IsLandingDatabaseRole(ERpgMotionMatchingDatabaseRole Role)
{
	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return true;
	default:
		return false;
	}
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveStationaryLandingRole(
	ERpgMotionMatchingDatabaseRole LandingRole)
{
	switch (LandingRole)
	{
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
		return ERpgMotionMatchingDatabaseRole::StandLightLanding;
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return ERpgMotionMatchingDatabaseRole::StandHeavyLanding;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
}

bool URpgAnimInstance::ShouldReleaseStationaryLanding(
	ERpgMotionMatchingDatabaseRole LandingRole,
	bool bChooserMoving,
	float GroundSpeed)
{
	const bool bStationaryLanding =
		LandingRole == ERpgMotionMatchingDatabaseRole::StandLightLanding ||
		LandingRole == ERpgMotionMatchingDatabaseRole::StandHeavyLanding;
	return bStationaryLanding &&
		(bChooserMoving ||
		 !FMath::IsFinite(GroundSpeed) ||
		 GroundSpeed > LightLandingIdleSpeedThreshold);
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveStationaryLandingMovementRole(
	ERpgMotionMatchingDatabaseRole LandingRole,
	ERpgLocomotionGait LiveGait)
{
	const bool bLight = LandingRole == ERpgMotionMatchingDatabaseRole::StandLightLanding;
	const bool bHeavy = LandingRole == ERpgMotionMatchingDatabaseRole::StandHeavyLanding;
	if (!bLight && !bHeavy)
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	switch (LiveGait)
	{
	case ERpgLocomotionGait::Walk:
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::WalkHeavyLanding
			: ERpgMotionMatchingDatabaseRole::WalkLightLanding;
	case ERpgLocomotionGait::Run:
	case ERpgLocomotionGait::Sprint:
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::RunHeavyLanding
			: ERpgMotionMatchingDatabaseRole::RunLightLanding;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
}

bool URpgAnimInstance::ShouldInterruptLandingDatabaseExit(
	ERpgJumpPhase CurrentJumpPhase,
	bool bCompletionArmed,
	ERpgMotionMatchingDatabaseRole CurrentDatabaseRole)
{
	return IsLandingDatabaseRole(CurrentDatabaseRole) &&
		(CurrentJumpPhase != ERpgJumpPhase::Landing || bCompletionArmed);
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveLandingDatabaseRole(
	const FRpgLandingSelectionSnapshot& Snapshot,
	float HeavySpeedThreshold)
{
	const bool bFiniteSnapshot =
		Snapshot.bIsValid &&
		Snapshot.AirborneEpoch != 0 &&
		!Snapshot.HorizontalVelocity.ContainsNaN() &&
		FMath::IsFinite(Snapshot.HorizontalSpeed) && Snapshot.HorizontalSpeed >= 0.0f &&
		FMath::IsFinite(Snapshot.VerticalVelocity) &&
		FMath::IsFinite(Snapshot.MaximumDownwardSpeed) &&
		Snapshot.MaximumDownwardSpeed >= 0.0f &&
		FMath::IsFinite(Snapshot.PredictedImpactDownwardSpeed) &&
		Snapshot.PredictedImpactDownwardSpeed >= 0.0f &&
		FMath::IsFinite(HeavySpeedThreshold) && HeavySpeedThreshold > 0.0f;
	if (!bFiniteSnapshot)
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	if (Snapshot.PredictedLanding.bIsValid &&
		(!FMath::IsFinite(Snapshot.PredictedLanding.TimeToLand) ||
		 Snapshot.PredictedLanding.TimeToLand < 0.0f ||
		 Snapshot.PredictedLanding.LandingLocation.ContainsNaN() ||
		 Snapshot.PredictedLanding.LandingNormal.ContainsNaN()))
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	const bool bHeavy =
		FMath::Max(
			Snapshot.MaximumDownwardSpeed,
			Snapshot.PredictedImpactDownwardSpeed) >= HeavySpeedThreshold;
	// Match GASP's physical MovementState boundary: airborne input may capture the desired
	// gait, but it cannot select a moving landing before speed leaves the inclusive Idle band.
	const bool bIdle = Snapshot.HorizontalSpeed <= LightLandingIdleSpeedThreshold;
	if (bIdle)
	{
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::StandHeavyLanding
			: ERpgMotionMatchingDatabaseRole::StandLightLanding;
	}

	switch (Snapshot.Gait)
	{
	case ERpgLocomotionGait::Walk:
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::WalkHeavyLanding
			: ERpgMotionMatchingDatabaseRole::WalkLightLanding;
	case ERpgLocomotionGait::Run:
	case ERpgLocomotionGait::Sprint:
		// Sprint is never inferred. Until #62 owns that gameplay state and its dedicated
		// landing content, an explicit Sprint snapshot uses the closest curated Run domain.
		return bHeavy
			? ERpgMotionMatchingDatabaseRole::RunHeavyLanding
			: ERpgMotionMatchingDatabaseRole::RunLightLanding;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
}

void URpgAnimInstance::UpdateLandingSelectionSnapshot(
	FRpgAnimInstanceProxy& Proxy,
	float InputMagnitude,
	const FVector& GravityAcceleration)
{
	const bool bAirborne =
		Proxy.bIsFalling ||
		Proxy.MovementState == ERpgLocomotionMovementState::Airborne;
	const bool bGrounded =
		Proxy.MovementState == ERpgLocomotionMovementState::Grounded &&
		Proxy.bIsMovingOnGround;
	if (Proxy.bTurnInPlaceHardReset || (!bAirborne && !bGrounded))
	{
		Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		Proxy.LastGroundedGait = ERpgLocomotionGait::Idle;
		Proxy.LandingAirborneEpoch = 0;
		Proxy.bWasAirborneForLanding = false;
		return;
	}

	if (bGrounded)
	{
		// Keep the final airborne values for exactly this physical touchdown update. A
		// second grounded update clears them so floor transitions cannot create requests.
		if (!Proxy.bWasAirborneForLanding)
		{
			Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		}
		Proxy.LastGroundedGait = Proxy.Gait;
		Proxy.bWasAirborneForLanding = false;
		return;
	}

	const bool bUpwardRelaunch =
		Proxy.bWasAirborneForLanding &&
		Proxy.LandingSelectionSnapshot.bIsValid &&
		Proxy.LandingSelectionSnapshot.VerticalVelocity <= UE_KINDA_SMALL_NUMBER &&
		Proxy.VerticalVelocity > UE_KINDA_SMALL_NUMBER;
	if (!Proxy.bWasAirborneForLanding || bUpwardRelaunch)
	{
		Proxy.LandingAirborneEpoch =
			Proxy.LandingAirborneEpoch >= MAX_int32
				? 1
				: Proxy.LandingAirborneEpoch + 1;
		Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		Proxy.LandingSelectionSnapshot.AirborneEpoch = Proxy.LandingAirborneEpoch;
	}
	Proxy.bWasAirborneForLanding = true;

	FVector GravityDirection = FVector::ZeroVector;
	float GravityMagnitude = 0.0f;
	GravityAcceleration.ToDirectionAndLength(GravityDirection, GravityMagnitude);
	const FVector HorizontalVelocity(
		Proxy.WorldVelocity.X,
		Proxy.WorldVelocity.Y,
		0.0f);
	const float HorizontalSpeed = HorizontalVelocity.Size();
	const bool bFiniteInputs =
		!Proxy.WorldVelocity.ContainsNaN() &&
		!HorizontalVelocity.ContainsNaN() &&
		FMath::IsFinite(HorizontalSpeed) &&
		FMath::IsFinite(Proxy.VerticalVelocity) &&
		FMath::IsFinite(InputMagnitude) && InputMagnitude >= 0.0f &&
		!GravityAcceleration.ContainsNaN() &&
		!GravityDirection.ContainsNaN() &&
		FMath::IsFinite(GravityMagnitude) && GravityMagnitude > UE_SMALL_NUMBER;
	if (!bFiniteInputs)
	{
		const int32 AirborneEpoch = Proxy.LandingAirborneEpoch;
		Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		Proxy.LandingSelectionSnapshot.AirborneEpoch = AirborneEpoch;
		return;
	}

	const bool bHasMoveIntent = InputMagnitude > 0.1f;
	ERpgLocomotionGait CapturedGait = Proxy.LastGroundedGait;
	if (CapturedGait != ERpgLocomotionGait::Sprint && bHasMoveIntent)
	{
		CapturedGait = InputMagnitude < 0.65f
			? ERpgLocomotionGait::Walk
			: ERpgLocomotionGait::Run;
	}
	else if (CapturedGait == ERpgLocomotionGait::Idle &&
		HorizontalSpeed > LightLandingIdleSpeedThreshold)
	{
		// A late-joining proxy may have no grounded history. Preserve the existing project
		// deceleration policy by choosing Run rather than inventing Sprint from velocity.
		CapturedGait = ERpgLocomotionGait::Run;
	}

	const float CurrentDownwardSpeed = FMath::Max(
		FVector::DotProduct(Proxy.WorldVelocity, GravityDirection),
		0.0f);
	FRpgLandingSelectionSnapshot& Snapshot = Proxy.LandingSelectionSnapshot;
	Snapshot.HorizontalVelocity = HorizontalVelocity;
	Snapshot.HorizontalSpeed = HorizontalSpeed;
	Snapshot.VerticalVelocity = Proxy.VerticalVelocity;
	Snapshot.MaximumDownwardSpeed = FMath::Max(
		Snapshot.MaximumDownwardSpeed,
		CurrentDownwardSpeed);
	Snapshot.Gait = CapturedGait;
	Snapshot.bHasMoveIntent = bHasMoveIntent;
	Snapshot.PredictedLanding = FRpgTrajectoryLandingPrediction();
	Snapshot.PredictedImpactDownwardSpeed = 0.0f;
	if (Proxy.TrajectoryLandingPrediction.bIsValid &&
		FMath::IsFinite(Proxy.TrajectoryLandingPrediction.TimeToLand) &&
		Proxy.TrajectoryLandingPrediction.TimeToLand >= 0.0f &&
		!Proxy.TrajectoryLandingPrediction.LandingLocation.ContainsNaN() &&
		!Proxy.TrajectoryLandingPrediction.LandingNormal.ContainsNaN())
	{
		Snapshot.PredictedLanding = Proxy.TrajectoryLandingPrediction;
		Snapshot.PredictedImpactDownwardSpeed = FMath::Max(
			FVector::DotProduct(Proxy.WorldVelocity, GravityDirection) +
				GravityMagnitude * Proxy.TrajectoryLandingPrediction.TimeToLand,
			0.0f);
	}
	Snapshot.bIsValid =
		FMath::IsFinite(Snapshot.MaximumDownwardSpeed) &&
		FMath::IsFinite(Snapshot.PredictedImpactDownwardSpeed);
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveAvailableLandingDatabaseRole(
	ERpgMotionMatchingDatabaseRole RequestedRole) const
{
	ERpgMotionMatchingDatabaseRole Role = RequestedRole;
	if (GetMotionMatchingDatabaseForRole(Role))
	{
		return Role;
	}

	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
		Role = ERpgMotionMatchingDatabaseRole::StandLightLanding;
		break;
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
		Role = ERpgMotionMatchingDatabaseRole::WalkLightLanding;
		break;
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		Role = ERpgMotionMatchingDatabaseRole::RunLightLanding;
		break;
	default:
		return ERpgMotionMatchingDatabaseRole::None;
	}
	return GetMotionMatchingDatabaseForRole(Role)
		? Role
		: ERpgMotionMatchingDatabaseRole::None;
}

ERpgMotionMatchingDatabaseRole URpgAnimInstance::ResolveAvailableLandingDatabaseRole(
	const FRpgLandingSelectionSnapshot& Snapshot) const
{
	return ResolveAvailableLandingDatabaseRole(
		ResolveLandingDatabaseRole(Snapshot, HeavyLandingSpeedThreshold));
}

bool URpgAnimInstance::IsGroundMotionMatchingChooserMoving(
	const FGroundMotionMatchingSelectionSnapshot& Snapshot)
{
	return Snapshot.bIsMovingOnGround &&
		!Snapshot.WorldVelocity.ContainsNaN() &&
		!Snapshot.WorldAcceleration.ContainsNaN() &&
		Snapshot.WorldVelocity.SizeSquared2D() > FMath::Square(ChooserVelocityTolerance) &&
		Snapshot.WorldAcceleration.SizeSquared2D() > FMath::Square(ChooserAccelerationTolerance);
}

float URpgAnimInstance::GetRunPivotMinimumAngle(ERpgCharacterRotationMode RotationMode)
{
	switch (RotationMode)
	{
	case ERpgCharacterRotationMode::Free:
		return FreeRunPivotMinimumAngle;
	case ERpgCharacterRotationMode::CombatStrafe:
		return CombatStrafeRunPivotMinimumAngle;
	case ERpgCharacterRotationMode::Aim:
		return AimRunPivotMinimumAngle;
	default:
		return FreeRunPivotMinimumAngle;
	}
}

bool URpgAnimInstance::ShouldInterruptGroundMotionMatching(
	bool bHasPreviousState,
	const FGroundMotionMatchingDomainState& PreviousState,
	const FGroundMotionMatchingDomainState& CurrentState)
{
	if (!bHasPreviousState ||
		PreviousState.PhysicalMovementState != CurrentState.PhysicalMovementState)
	{
		return true;
	}

	if (CurrentState.PhysicalMovementState != ERpgLocomotionMovementState::Grounded)
	{
		return false;
	}

	return PreviousState.bChooserMoving != CurrentState.bChooserMoving ||
		PreviousState.Stance != CurrentState.Stance ||
		(CurrentState.bChooserMoving && PreviousState.Gait != CurrentState.Gait);
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

URpgAnimInstance::FResolvedMotionMatchingDatabaseRoles
URpgAnimInstance::ResolveMotionMatchingDatabaseRoles(
	const FGroundMotionMatchingSelectionSnapshot& Snapshot)
{
	FResolvedMotionMatchingDatabaseRoles ResolvedRoles;
	if (Snapshot.MovementState == ERpgLocomotionMovementState::Airborne)
	{
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::Jump);
		return ResolvedRoles;
	}

	if (Snapshot.MovementState != ERpgLocomotionMovementState::Grounded ||
		!Snapshot.bIsMovingOnGround)
	{
		return ResolvedRoles;
	}

	if (Snapshot.Stance == ERpgLocomotionStance::Crouching)
	{
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::Crouch);
		return ResolvedRoles;
	}

	if (Snapshot.Stance != ERpgLocomotionStance::Standing)
	{
		return ResolvedRoles;
	}

	const float SafeGroundSpeed = FMath::IsFinite(Snapshot.GroundSpeed)
		? FMath::Max(Snapshot.GroundSpeed, 0.0f)
		: 0.0f;
	const bool bChooserMoving = IsGroundMotionMatchingChooserMoving(Snapshot);
	if (!bChooserMoving)
	{
		// GASP's logical Idle rows are inclusive and intentionally overlap. Preserve their source
		// order so exact boundaries expose both adjacent roles to the Pose Search cost comparison.
		if (SafeGroundSpeed <= WalkStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandIdle);
		}
		if (SafeGroundSpeed >= WalkStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandWalkStops);
		}
		if (SafeGroundSpeed >= RunStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunStops);
		}
		// Project Run reaches 600 cm/s, so source's speed-only Sprint Stop row also requires the
		// explicit cosmetic Sprint gait. This keeps ordinary Run Stops out of forward-only clips.
		if (Snapshot.Gait == ERpgLocomotionGait::Sprint &&
			SafeGroundSpeed >= SprintStopMinimumSpeed)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandSprintStops);
		}
		return ResolvedRoles;
	}

	switch (Snapshot.Gait)
	{
	case ERpgLocomotionGait::Idle:
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandIdle);
		break;
	case ERpgLocomotionGait::Walk:
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandWalk);
		break;
	case ERpgLocomotionGait::Run:
	{
		float SafeFutureGroundSpeed = Snapshot.FutureVelocity.Size2D();
		if (!FMath::IsFinite(SafeFutureGroundSpeed))
		{
			SafeFutureGroundSpeed = 0.0f;
		}
		const bool bSearchStarts =
			Snapshot.CurrentDatabaseRole != ERpgMotionMatchingDatabaseRole::StandRunPivots &&
			SafeFutureGroundSpeed >= SafeGroundSpeed + RunStartMinimumFutureSpeedGain;

		const FVector HorizontalVelocity(Snapshot.WorldVelocity.X, Snapshot.WorldVelocity.Y, 0.0f);
		const FVector HorizontalAcceleration(Snapshot.WorldAcceleration.X, Snapshot.WorldAcceleration.Y, 0.0f);
		const float TurnAngle = FMath::Abs(FMath::FindDeltaAngleDegrees(
			HorizontalVelocity.Rotation().Yaw,
			HorizontalAcceleration.Rotation().Yaw));
		const bool bSearchPivots = TurnAngle >= GetRunPivotMinimumAngle(Snapshot.RotationMode);

		ResolvedRoles.Reserve(3);
		if (bSearchStarts)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunStarts);
		}
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunLoops);
		if (bSearchPivots)
		{
			ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandRunPivots);
		}
		break;
	}
	case ERpgLocomotionGait::Sprint:
		ResolvedRoles.Add(ERpgMotionMatchingDatabaseRole::StandSprint);
		break;
	default:
		break;
	}
	return ResolvedRoles;
}

URpgAnimInstance::FResolvedGroundMotionMatchingDatabases
URpgAnimInstance::ResolveGroundMotionMatchingDatabases(
	const FGroundMotionMatchingSelectionSnapshot& Snapshot,
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
		ResolveMotionMatchingDatabaseRoles(Snapshot))
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
	if (!Database)
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	ERpgMotionMatchingDatabaseRole ResolvedRole = ERpgMotionMatchingDatabaseRole::None;
	int32 MatchedTagCount = 0;
	for (uint8 RoleValue = static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None) + 1;
		RoleValue < static_cast<uint8>(ERpgMotionMatchingDatabaseRole::Count);
		++RoleValue)
	{
		const ERpgMotionMatchingDatabaseRole Role =
			static_cast<ERpgMotionMatchingDatabaseRole>(RoleValue);
		const FName RoleTag = GetMotionMatchingDatabaseRoleTag(Role);
		int32 RoleTagCount = 0;
		for (const FName DatabaseTag : Database->Tags)
		{
			RoleTagCount += DatabaseTag == RoleTag;
		}
		if (RoleTagCount > 0)
		{
			ResolvedRole = Role;
			MatchedTagCount += RoleTagCount;
		}
	}
	return MatchedTagCount == 1
		? ResolvedRole
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

URpgAnimInstance::FMotionMatchingPostSelectionState
URpgAnimInstance::ResolveMotionMatchingPostSelection(
	ERpgMotionMatchingDatabaseRole SelectedRole,
	bool bIsContinuingPose,
	EPoseSearchInterruptMode InterruptMode,
	bool bCanLatchTurnInPlace,
	bool bCanLatchLanding)
{
	FMotionMatchingPostSelectionState State;
	State.CurrentDatabaseRole = SelectedRole;
	State.InterruptMode = InterruptMode;
	State.bIsContinuingPose = bIsContinuingPose;
	State.bShouldLatchTurnInPlace =
		!bIsContinuingPose && bCanLatchTurnInPlace &&
		SelectedRole == ERpgMotionMatchingDatabaseRole::StandTurnInPlace;
	State.bShouldLatchLanding =
		!bIsContinuingPose && bCanLatchLanding &&
		IsLandingDatabaseRole(SelectedRole);
	return State;
}

UPoseSearchDatabase* URpgAnimInstance::GetMotionMatchingDatabaseForRole(
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

URpgAnimInstance::FMotionMatchingDatabaseRoleContracts
URpgAnimInstance::BuildMotionMatchingDatabaseRoleContracts() const
{
	FMotionMatchingDatabaseRoleContracts Contracts;
	auto AddContract = [&Contracts, this](ERpgMotionMatchingDatabaseRole Role)
	{
		Contracts.Add({Role, GetMotionMatchingDatabaseForRole(Role)});
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
		if (!GaspPresentationProfile)
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no GASP presentation profile is configured.")));
		}
		else if (!GaspPresentationProfile->ValidateProfile().IsValid())
		{
			Context.AddError(FText::FromString(
				TEXT("The configured GASP presentation profile has invalid membership or loop invariants.")));
		}
		else
		{
			FRpgGaspPresentationAssetLookup PresentationLookup;
			const bool bBuiltPresentationLookup = PresentationLookup.Build(GaspPresentationProfile);
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
					GetMotionMatchingDatabaseForRole(Role),
					ERpgGaspPresentationAssetTrait::GroundMoving);
			}
			if (!bGroundMovingCoverageValid)
			{
				Context.AddError(FText::FromString(
					TEXT("Every Walk, Run, and Sprint runtime database asset must have GroundMoving presentation membership.")));
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
					TEXT("Every Airborne runtime database asset must have JumpStart, BackwardJumpStart, or AirborneFall presentation membership.")));
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
					GetMotionMatchingDatabaseForRole(Role),
					ERpgGaspPresentationAssetTrait::Landing);
			}
			if (!bLandingCoverageValid)
			{
				Context.AddError(FText::FromString(
					TEXT("Every curated runtime landing database asset must have Landing presentation membership.")));
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

	return ((Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // WITH_EDITOR

void URpgAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	GaspPresentationAssetLookup.Build(GaspPresentationProfile);
	FRpgAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRpgAnimInstanceProxy>();
	ResetFootPlacementInitializationState(Proxy);
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
	LandingRequestSerial = 0;
	LandingInterruptedRequestSerial = 0;
	PreviousGroundMotionMatchingDomainState = FGroundMotionMatchingDomainState();
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

bool URpgAnimInstance::IsTurnInPlaceEligible(const FRpgAnimInstanceProxy& Proxy) const
{
	return
		TurnInPlaceMotionMatchingDatabase != nullptr &&
		JumpPhase == ERpgJumpPhase::Grounded &&
		SupportsTurnInPlace(Proxy.RotationMode) &&
		Proxy.MovementState == ERpgLocomotionMovementState::Grounded &&
		Proxy.bIsMovingOnGround &&
		!Proxy.bIsCrouching &&
		!Proxy.bIsAnyMontagePlaying &&
		!Proxy.bHasTurnInPlaceBlockingGameplayTag &&
		Proxy.GroundSpeed <= TurnInPlaceIdleSpeedThreshold &&
		!Proxy.bHasGroundedMoveIntent &&
		!Proxy.TransformTrajectory.Samples.IsEmpty();
}

void URpgAnimInstance::ClearTurnInPlaceSelection()
{
	TurnInPlaceSelectedAsset = nullptr;
	TurnInPlaceSelectedAssetStartTime = 0.0f;
	TurnInPlaceSelectedAssetRemainingTime = MAX_flt;
	TurnInPlaceSelectedRequestSerial = 0;
	TurnInPlacePlaybackWatchdogDuration = TurnInPlaceActiveTimeout;
	bTurnInPlacePoseSelected = false;
	bTurnInPlaceSelectedAssetLooping = false;
	bTurnInPlaceSelectionLatched = false;
	bTurnInPlacePlaybackObserved = false;
	bTurnInPlaceCompletionArmed = false;
}

void URpgAnimInstance::ResetTurnInPlaceRuntime(bool bHardResetOffset)
{
	TurnInPlaceState = ERpgTurnInPlaceState::Inactive;
	TurnInPlaceQueryAngle = 0.0f;
	TurnInPlaceAccumulatedYaw = 0.0f;
	TurnInPlaceStateElapsed = 0.0f;
	TurnInPlaceStableElapsed = 0.0f;
	TurnInPlaceSelectionElapsed = 0.0f;
	TurnInPlaceRequestAccumulatedYaw = 0.0f;
	ClearTurnInPlaceSelection();
	TurnInPlaceSyntheticTrajectory.Samples.Reset();
	OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
	bResetOffsetRootEveryFrame |= bHardResetOffset;
}

void URpgAnimInstance::BeginTurnInPlaceRecovery(bool bHardResetOffset)
{
	TurnInPlaceState = ERpgTurnInPlaceState::Recovering;
	TurnInPlaceStateElapsed = 0.0f;
	TurnInPlaceStableElapsed = 0.0f;
	TurnInPlaceSelectionElapsed = 0.0f;
	ClearTurnInPlaceSelection();
	TurnInPlaceSyntheticTrajectory.Samples.Reset();
	OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
	bResetOffsetRootEveryFrame |= bHardResetOffset;
}

void URpgAnimInstance::BeginTurnInPlaceRequest(float QuantizedAngle)
{
	TurnInPlaceState = ERpgTurnInPlaceState::Active;
	TurnInPlaceQueryAngle = QuantizedAngle;
	TurnInPlaceStateElapsed = 0.0f;
	TurnInPlaceStableElapsed = 0.0f;
	TurnInPlaceSelectionElapsed = 0.0f;
	TurnInPlaceRequestAccumulatedYaw = TurnInPlaceAccumulatedYaw;
	ClearTurnInPlaceSelection();
	++TurnInPlaceRequestSerial;
	if (TurnInPlaceRequestSerial == 0)
	{
		++TurnInPlaceRequestSerial;
	}
	OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
}

void URpgAnimInstance::UpdateTurnInPlaceRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy)
{
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	bResetOffsetRootEveryFrame = bTurnInPlaceInitializationResetPending;
	bTurnInPlaceInitializationResetPending = false;
	const float AbsoluteActorYawRate = SafeDeltaSeconds > UE_SMALL_NUMBER
		? FMath::Abs(Proxy.ActorYawDelta) / SafeDeltaSeconds
		: (FMath::IsNearlyZero(Proxy.ActorYawDelta) ? 0.0f : MAX_flt);

	const bool bAirborneCancelsTurnInPlace =
		Proxy.MovementState == ERpgLocomotionMovementState::Airborne;
	uint8 HardResetReasons = 0;
	const auto AddHardResetReason = [&HardResetReasons](
		bool bCondition,
		ETurnInPlaceHardResetReason Reason)
	{
		if (bCondition)
		{
			HardResetReasons |= ToReasonMask(Reason);
		}
	};
	AddHardResetReason(Proxy.bTurnInPlaceHardReset, ETurnInPlaceHardResetReason::ProxySnapshot);
	AddHardResetReason(!SupportsTurnInPlace(Proxy.RotationMode), ETurnInPlaceHardResetReason::UnsupportedRotationMode);
	AddHardResetReason(Proxy.bHasTurnInPlaceBlockingGameplayTag, ETurnInPlaceHardResetReason::BlockingGameplayTag);
	AddHardResetReason(Proxy.bIsAnyMontagePlaying, ETurnInPlaceHardResetReason::Montage);
	AddHardResetReason(Proxy.bIsCrouching, ETurnInPlaceHardResetReason::Crouch);
	AddHardResetReason(
		!bAirborneCancelsTurnInPlace &&
			Proxy.MovementState != ERpgLocomotionMovementState::Grounded,
		ETurnInPlaceHardResetReason::UnsupportedMovementState);
	if (HardResetReasons != 0 || bAirborneCancelsTurnInPlace)
	{
		const bool bHasTurnStateToClear =
			TurnInPlaceState != ERpgTurnInPlaceState::Inactive ||
			FMath::Abs(TurnInPlaceAccumulatedYaw) > UE_KINDA_SMALL_NUMBER;
		const bool bHasNewHardResetReason =
			(HardResetReasons & ~TurnInPlaceHardResetReasonsLastFrame) != 0;
		// Entering the air always cancels TIR ownership, but a regular moving jump must retain
		// Offset Root's interpolated locomotion offset. Only real TIR state/yaw needs a pulse;
		// every newly added teleport, montage, stance, tag, or unsupported-policy reason gets
		// its own one-shot edge even while another reason remains active. A rotation-policy
		// transition is also an event so an early airborne/override return cannot consume it.
		const bool bPulseHardReset =
			bHasTurnStateToClear ||
			bHasNewHardResetReason ||
			Proxy.bTurnInPlaceSupportChanged;
		ResetTurnInPlaceRuntime(bPulseHardReset);
		// Do not let a clean airborne-only cancel mask a later montage/tag/policy reset in the air.
		TurnInPlaceHardResetReasonsLastFrame = HardResetReasons;
		LocomotionTrajectory = Proxy.TransformTrajectory;
		return;
	}
	TurnInPlaceHardResetReasonsLastFrame = 0;

	if (Proxy.bTurnInPlaceSupportChanged)
	{
		// Entering a controller-facing mode can rotate the capsule from an arbitrary free-camera
		// heading in one frame. Treat that policy transition as a reset, not authored turn intent.
		// Recovery also absorbs the short network-smoothing tail on simulated proxies.
		BeginTurnInPlaceRecovery(true);
		LocomotionTrajectory = Proxy.TransformTrajectory;
		return;
	}

	const bool bEligible = IsTurnInPlaceEligible(Proxy);
	if (!bEligible)
	{
		if (TurnInPlaceState == ERpgTurnInPlaceState::Collecting ||
			TurnInPlaceState == ERpgTurnInPlaceState::Active)
		{
			BeginTurnInPlaceRecovery(false);
		}
		else if (TurnInPlaceState == ERpgTurnInPlaceState::Recovering)
		{
			TurnInPlaceStateElapsed += SafeDeltaSeconds;
			if (TurnInPlaceStateElapsed >= TurnInPlaceRecoveryDuration)
			{
				ResetTurnInPlaceRuntime(false);
			}
		}
		else
		{
			ResetTurnInPlaceRuntime(false);
		}

		LocomotionTrajectory = Proxy.TransformTrajectory;
		return;
	}

	TurnInPlaceStateElapsed += SafeDeltaSeconds;
	if (TurnInPlaceState != ERpgTurnInPlaceState::Recovering)
	{
		TurnInPlaceAccumulatedYaw = FMath::Clamp(
			TurnInPlaceAccumulatedYaw + Proxy.ActorYawDelta,
			-180.0f,
			180.0f);
	}

	switch (TurnInPlaceState)
	{
	case ERpgTurnInPlaceState::Inactive:
		OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
		if (AbsoluteActorYawRate <= TurnInPlaceInactiveYawRateThreshold)
		{
			TurnInPlaceStableElapsed += SafeDeltaSeconds;
			if (TurnInPlaceStableElapsed >= TurnInPlaceInactiveAccumulatorTimeout)
			{
				TurnInPlaceAccumulatedYaw = 0.0f;
			}
		}
		else
		{
			TurnInPlaceStableElapsed = 0.0f;
		}

		if (FMath::Abs(TurnInPlaceAccumulatedYaw) >= TurnInPlaceCollectThreshold)
		{
			TurnInPlaceState = ERpgTurnInPlaceState::Collecting;
			TurnInPlaceStateElapsed = 0.0f;
			TurnInPlaceStableElapsed = 0.0f;
			OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
		}
		break;

	case ERpgTurnInPlaceState::Collecting:
		OffsetRootRotationMode = EOffsetRootBoneMode::Accumulate;
		if (FMath::Abs(TurnInPlaceAccumulatedYaw) < TurnInPlaceCancelThreshold)
		{
			BeginTurnInPlaceRecovery(false);
			break;
		}

		TurnInPlaceStableElapsed = AbsoluteActorYawRate <= TurnInPlaceStableYawRateThreshold
			? TurnInPlaceStableElapsed + SafeDeltaSeconds
			: 0.0f;
		if (FMath::Abs(TurnInPlaceAccumulatedYaw) >= TurnInPlaceActivationThreshold &&
			(TurnInPlaceStableElapsed >= TurnInPlaceStabilityDuration ||
			 TurnInPlaceStateElapsed >= TurnInPlaceCollectionTimeout))
		{
			BeginTurnInPlaceRequest(QuantizeTurnInPlaceAngle(TurnInPlaceAccumulatedYaw));
		}
		else if (TurnInPlaceStateElapsed >= TurnInPlaceCollectionTimeout)
		{
			BeginTurnInPlaceRecovery(false);
		}
		break;

	case ERpgTurnInPlaceState::Active:
	{
		OffsetRootRotationMode = bTurnInPlaceSelectionLatched
			? EOffsetRootBoneMode::LockOffsetIncreaseAndConsumeAnimation
			: EOffsetRootBoneMode::Accumulate;
		if (FMath::Abs(TurnInPlaceAccumulatedYaw) < TurnInPlaceCancelThreshold)
		{
			BeginTurnInPlaceRecovery(false);
			break;
		}

		if (CanRetargetTurnInPlaceRequest())
		{
			const float UpdatedQueryAngle = QuantizeTurnInPlaceAngle(TurnInPlaceAccumulatedYaw);
			const float AdditionalYaw = TurnInPlaceAccumulatedYaw - TurnInPlaceRequestAccumulatedYaw;
			const bool bDirectionChanged =
				!FMath::IsNearlyZero(UpdatedQueryAngle) &&
				FMath::Sign(UpdatedQueryAngle) != FMath::Sign(TurnInPlaceQueryAngle);
			const bool bQuantizedBucketChanged =
				!FMath::IsNearlyZero(UpdatedQueryAngle) &&
				!FMath::IsNearlyEqual(UpdatedQueryAngle, TurnInPlaceQueryAngle);
			if (FMath::Abs(AdditionalYaw) >= TurnInPlaceActivationThreshold &&
				(bDirectionChanged || bQuantizedBucketChanged))
			{
				BeginTurnInPlaceRequest(UpdatedQueryAngle);
				break;
			}
		}

		if (!bTurnInPlaceSelectionLatched)
		{
			TurnInPlaceSelectionElapsed += SafeDeltaSeconds;
			if (TurnInPlaceSelectionElapsed >= TurnInPlaceSelectionTimeout)
			{
				BeginTurnInPlaceRecovery(true);
				break;
			}
		}
		else if (bTurnInPlaceCompletionArmed)
		{
			BeginTurnInPlaceRecovery(false);
			break;
		}
		else if (bTurnInPlacePlaybackObserved && !bTurnInPlacePoseSelected)
		{
			BeginTurnInPlaceRecovery(true);
			break;
		}

		if (TurnInPlaceStateElapsed >= TurnInPlacePlaybackWatchdogDuration)
		{
			BeginTurnInPlaceRecovery(true);
		}
		break;
	}

	case ERpgTurnInPlaceState::Recovering:
		OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;
		if (TurnInPlaceStateElapsed >= TurnInPlaceRecoveryDuration)
		{
			ResetTurnInPlaceRuntime(false);
		}
		break;
	}

	if (TurnInPlaceState == ERpgTurnInPlaceState::Active)
	{
		TurnInPlaceSyntheticTrajectory = MakeTurnInPlaceSyntheticTrajectory(
			Proxy.TransformTrajectory,
			Proxy.ActorYaw,
			TurnInPlaceAccumulatedYaw,
			TurnInPlaceQueryAngle);
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
	if (TurnInPlaceState != ERpgTurnInPlaceState::Active ||
		!TurnInPlaceMotionMatchingDatabase ||
		TurnInPlaceInterruptedRequestSerial == TurnInPlaceRequestSerial)
	{
		return false;
	}

	TurnInPlaceInterruptedRequestSerial = TurnInPlaceRequestSerial;
	return true;
}

bool URpgAnimInstance::CanRetargetTurnInPlaceRequest() const
{
	return TurnInPlaceState == ERpgTurnInPlaceState::Active &&
		!bTurnInPlaceSelectionLatched &&
		TurnInPlaceInterruptedRequestSerial != TurnInPlaceRequestSerial;
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
		SelectedDatabase != TurnInPlaceMotionMatchingDatabase.Get() ||
		SelectionRequestSerial == 0 ||
		SelectionRequestSerial != TurnInPlaceRequestSerial)
	{
		return false;
	}

	TurnInPlaceSelectedAsset = SelectedAsset;
	TurnInPlaceSelectedAssetStartTime = FMath::Max(SelectedTime, 0.0f);
	TurnInPlaceSelectedAssetRemainingTime = FMath::Max(
		SelectedAsset->GetPlayLength() - TurnInPlaceSelectedAssetStartTime,
		0.0f);
	TurnInPlaceSelectedRequestSerial = SelectionRequestSerial;
	TurnInPlacePlaybackWatchdogDuration = CalculateTurnInPlacePlaybackWatchdogDuration(
		TurnInPlaceSelectedAssetRemainingTime,
		1.0f,
		bSelectedAssetLooping);
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
	const float CompletionLeadTime = TurnInPlaceFinishedTimeTolerance + SafeDeltaSeconds;
	if (CurrentAsset == TurnInPlaceSelectedAsset.Get())
	{
		const bool bFirstPlaybackObservation = !bTurnInPlacePlaybackObserved;
		bTurnInPlacePlaybackObserved = true;
		bTurnInPlacePoseSelected = true;
		const float EffectiveAssetLength = CurrentAssetLength > UE_SMALL_NUMBER
			? CurrentAssetLength
			: TurnInPlaceSelectedAsset->GetPlayLength();
		TurnInPlaceSelectedAssetRemainingTime = FMath::Max(
			EffectiveAssetLength - FMath::Max(CurrentAssetTime, 0.0f),
			0.0f);
		if (bFirstPlaybackObservation)
		{
			TurnInPlacePlaybackWatchdogDuration = CalculateTurnInPlacePlaybackWatchdogDuration(
				TurnInPlaceSelectedAssetRemainingTime,
				CurrentAssetPlayRate,
				bTurnInPlaceSelectedAssetLooping);
			TurnInPlaceStateElapsed = 0.0f;
		}
		if (!bTurnInPlaceSelectedAssetLooping &&
			TurnInPlaceSelectedAssetRemainingTime <= CompletionLeadTime)
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

	if (!bTurnInPlaceSelectedAssetLooping &&
		TurnInPlaceSelectedAssetRemainingTime <= CompletionLeadTime)
	{
		bTurnInPlacePoseSelected = true;
		bTurnInPlaceCompletionArmed = true;
		return;
	}

	bTurnInPlacePoseSelected = false;
}

URpgAnimInstance::ETurnInPlaceSearchMode URpgAnimInstance::ResolveTurnInPlaceSearchMode(
	bool bForceNewRequest) const
{
	if (bForceNewRequest &&
		TurnInPlaceState == ERpgTurnInPlaceState::Active &&
		TurnInPlaceMotionMatchingDatabase)
	{
		return ETurnInPlaceSearchMode::SearchRequestedTurn;
	}

	if (TurnInPlaceState == ERpgTurnInPlaceState::Active &&
		bTurnInPlaceSelectionLatched &&
		!bTurnInPlaceCompletionArmed)
	{
		return ETurnInPlaceSearchMode::ContinueSelectedTurn;
	}

	return ETurnInPlaceSearchMode::NormalLocomotion;
}

bool URpgAnimInstance::AllowsMovingProceduralNodes() const
{
	return TurnInPlaceState == ERpgTurnInPlaceState::Inactive ||
		TurnInPlaceState == ERpgTurnInPlaceState::Recovering;
}

void URpgAnimInstance::ClearLandingSelection()
{
	LandingSelectedAsset = nullptr;
	LandingSelectedAssetStartTime = 0.0f;
	LandingSelectedAssetRemainingTime = MAX_flt;
	LandingPlaybackWatchdogDuration = LandingActiveTimeout;
	LandingSelectedRequestSerial = 0;
	bLandingSelectedAssetLooping = false;
	bLandingSelectionLatched = false;
	bLandingPlaybackObserved = false;
	bLandingCompletionArmed = false;
}

void URpgAnimInstance::ClearBackwardJumpStartHold()
{
	BackwardJumpStartHeldAsset = nullptr;
	BackwardJumpStartHoldElapsed = 0.0f;
	bBackwardJumpStartHoldOpportunityConsumed = false;
	bBackwardJumpStartHoldEligible = false;
	bBackwardJumpStartHoldWasArmed = false;
}

void URpgAnimInstance::ResetJumpPhaseRuntime()
{
	JumpPhase = ERpgJumpPhase::Grounded;
	LandingStateElapsed = 0.0f;
	LandingTouchdownElapsed = 0.0f;
	ActiveLandingDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
	ClearLandingSelection();
	ClearBackwardJumpStartHold();
}

void URpgAnimInstance::BeginAirbornePhase(bool bAscendingTakeoff)
{
	JumpPhase = ERpgJumpPhase::Airborne;
	LandingStateElapsed = 0.0f;
	LandingTouchdownElapsed = 0.0f;
	ActiveLandingDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
	ClearLandingSelection();
	ClearBackwardJumpStartHold();
	bBackwardJumpStartHoldEligible = bAscendingTakeoff;
}

void URpgAnimInstance::BeginLandingRequest(
	ERpgMotionMatchingDatabaseRole LandingRole,
	bool bForceInterrupt)
{
	check(IsLandingDatabaseRole(LandingRole));
	check(GetMotionMatchingDatabaseForRole(LandingRole));
	JumpPhase = ERpgJumpPhase::Landing;
	LandingStateElapsed = 0.0f;
	ActiveLandingDatabaseRole = LandingRole;
	ClearLandingSelection();
	ClearBackwardJumpStartHold();
	++LandingRequestSerial;
	if (LandingRequestSerial == 0)
	{
		++LandingRequestSerial;
	}
	if (!bForceInterrupt)
	{
		// The outgoing stationary landing database is absent from the new moving landing set,
		// so GASP's database-change interrupt is sufficient and preserves the current pose as query context.
		LandingInterruptedRequestSerial = LandingRequestSerial;
	}
	else
	{
		LandingTouchdownElapsed = 0.0f;
	}
}

bool URpgAnimInstance::IsLandingRuntimeEligible(const FRpgAnimInstanceProxy& Proxy) const
{
	return Proxy.MovementState == ERpgLocomotionMovementState::Grounded &&
		Proxy.bIsMovingOnGround &&
		!Proxy.bIsCrouching &&
		!Proxy.bIsAnyMontagePlaying &&
		!Proxy.bHasTurnInPlaceBlockingGameplayTag;
}

void URpgAnimInstance::UpdateJumpPhaseRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy)
{
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	const bool bAirborneSnapshot =
		Proxy.bIsFalling || Proxy.MovementState == ERpgLocomotionMovementState::Airborne;

	if (Proxy.bTurnInPlaceHardReset)
	{
		if (bAirborneSnapshot)
		{
			BeginAirbornePhase(Proxy.VerticalVelocity > UE_KINDA_SMALL_NUMBER);
		}
		else
		{
			ResetJumpPhaseRuntime();
		}
		return;
	}

	if (bAirborneSnapshot)
	{
		if (JumpPhase != ERpgJumpPhase::Airborne)
		{
			BeginAirbornePhase(Proxy.VerticalVelocity > UE_KINDA_SMALL_NUMBER);
		}
		return;
	}

	FGroundMotionMatchingSelectionSnapshot LiveGroundSnapshot;
	LiveGroundSnapshot.Gait = Proxy.Gait;
	LiveGroundSnapshot.Stance = Proxy.Stance;
	LiveGroundSnapshot.MovementState = Proxy.MovementState;
	LiveGroundSnapshot.RotationMode = Proxy.RotationMode;
	LiveGroundSnapshot.WorldVelocity = Proxy.WorldVelocity;
	LiveGroundSnapshot.WorldAcceleration = Proxy.WorldAcceleration;
	LiveGroundSnapshot.GroundSpeed = Proxy.GroundSpeed;
	LiveGroundSnapshot.bIsMovingOnGround = Proxy.bIsMovingOnGround;
	const bool bChooserMoving = IsGroundMotionMatchingChooserMoving(LiveGroundSnapshot);

	if (JumpPhase == ERpgJumpPhase::Airborne)
	{
		ERpgMotionMatchingDatabaseRole LandingRole = ERpgMotionMatchingDatabaseRole::None;
		if (IsLandingRuntimeEligible(Proxy))
		{
			LandingRole = ResolveLandingDatabaseRole(
				Proxy.LandingSelectionSnapshot,
				HeavyLandingSpeedThreshold);
			if (FMath::IsFinite(Proxy.GroundSpeed) &&
				Proxy.GroundSpeed <= LightLandingIdleSpeedThreshold)
			{
				// CMC owns the physical touchdown. Rebase stale airborne momentum into the
				// live stationary domain before database fallback, while preserving severity.
				LandingRole = ResolveStationaryLandingRole(LandingRole);
			}
			LandingRole = ResolveAvailableLandingDatabaseRole(LandingRole);
		}
		if (ShouldReleaseStationaryLanding(LandingRole, bChooserMoving, Proxy.GroundSpeed))
		{
			LandingRole = bChooserMoving
				? ResolveAvailableLandingDatabaseRole(
					ResolveStationaryLandingMovementRole(LandingRole, Proxy.Gait))
				: ERpgMotionMatchingDatabaseRole::None;
		}
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

	if (!IsLandingRuntimeEligible(Proxy) ||
		!IsLandingDatabaseRole(ActiveLandingDatabaseRole) ||
		!GetMotionMatchingDatabaseForRole(ActiveLandingDatabaseRole))
	{
		ResetJumpPhaseRuntime();
		return;
	}
	LandingTouchdownElapsed += SafeDeltaSeconds;

	if (ShouldReleaseStationaryLanding(
			ActiveLandingDatabaseRole,
			bChooserMoving,
			Proxy.GroundSpeed))
	{
		const ERpgMotionMatchingDatabaseRole HandoffRole =
			bChooserMoving && LandingTouchdownElapsed <= LandingMovementHandoffWindow
				? ResolveAvailableLandingDatabaseRole(
					ResolveStationaryLandingMovementRole(ActiveLandingDatabaseRole, Proxy.Gait))
				: ERpgMotionMatchingDatabaseRole::None;
		if (HandoffRole != ERpgMotionMatchingDatabaseRole::None)
		{
			// GASP re-evaluates the gait while JustLanded is active. Preserve severity and perform
			// one database-change handoff rather than exposing a normal Run loop at touchdown speed.
			BeginLandingRequest(HandoffRole, false);
		}
		else
		{
			ResetJumpPhaseRuntime();
		}
		return;
	}

	LandingStateElapsed += SafeDeltaSeconds;
	const bool bSelectionTimedOut =
		!bLandingSelectionLatched && LandingStateElapsed >= LandingSelectionTimeout;
	const bool bPlaybackTimedOut =
		bLandingSelectionLatched && LandingStateElapsed >= LandingPlaybackWatchdogDuration;
	if (bLandingCompletionArmed || bSelectionTimedOut || bPlaybackTimedOut)
	{
		ResetJumpPhaseRuntime();
	}
}

bool URpgAnimInstance::ConsumeLandingForceInterruptRequest()
{
	if (JumpPhase != ERpgJumpPhase::Landing ||
		!GetMotionMatchingDatabaseForRole(ActiveLandingDatabaseRole) ||
		LandingInterruptedRequestSerial == LandingRequestSerial)
	{
		return false;
	}

	LandingInterruptedRequestSerial = LandingRequestSerial;
	return true;
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
	LandingPlaybackWatchdogDuration = CalculateLandingPlaybackWatchdogDuration(
		LandingSelectedAssetRemainingTime,
		1.0f,
		bLandingSelectedAssetLooping);
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
	const float CompletionLeadTime = LandingFinishedTimeTolerance + SafeDeltaSeconds;
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
			LandingPlaybackWatchdogDuration = CalculateLandingPlaybackWatchdogDuration(
				LandingSelectedAssetRemainingTime,
				CurrentAssetPlayRate,
				bLandingSelectedAssetLooping);
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

bool URpgAnimInstance::ShouldHoldLoopingAirborneFallPlayback(
	ERpgJumpPhase CurrentJumpPhase,
	bool bBackwardHoldWasArmed,
	float CurrentVerticalVelocity,
	bool bCurrentAssetIsLoopingFall)
{
	return CurrentJumpPhase == ERpgJumpPhase::Airborne &&
		bBackwardHoldWasArmed &&
		CurrentVerticalVelocity <= UE_KINDA_SMALL_NUMBER &&
		bCurrentAssetIsLoopingFall;
}

bool URpgAnimInstance::ShouldHoldBackwardJumpStartPlayback(
	ERpgJumpPhase CurrentJumpPhase,
	bool bCurrentAssetMatchesHeldSelection,
	float CurrentAssetTime,
	float CurrentAssetLength,
	float CurrentAssetPlayRate,
	float HoldElapsed)
{
	if (CurrentJumpPhase != ERpgJumpPhase::Airborne ||
		!bCurrentAssetMatchesHeldSelection ||
		!FMath::IsFinite(CurrentAssetTime) ||
		!FMath::IsFinite(CurrentAssetLength) ||
		!FMath::IsFinite(CurrentAssetPlayRate) ||
		!FMath::IsFinite(HoldElapsed) ||
		CurrentAssetTime < 0.0f ||
		CurrentAssetLength <= 0.0f ||
		CurrentAssetTime >= CurrentAssetLength ||
		HoldElapsed < 0.0f ||
		HoldElapsed >= BackwardJumpStartHoldTimeout)
	{
		return false;
	}

	const float AbsolutePlayRate = FMath::Abs(CurrentAssetPlayRate);
	if (AbsolutePlayRate <= UE_KINDA_SMALL_NUMBER)
	{
		// A paused cosmetic player remains bounded by HoldElapsed rather than forcing a new search immediately.
		return true;
	}

	const float RemainingPlaybackTime =
		(CurrentAssetLength - CurrentAssetTime) / AbsolutePlayRate;
	return RemainingPlaybackTime > BackwardJumpStartReleaseLeadTime;
}

bool URpgAnimInstance::UpdateBackwardJumpStartHold(
	UAnimationAsset* CurrentAsset,
	float CurrentAssetTime,
	float CurrentAssetLength,
	float CurrentAssetPlayRate,
	float DeltaSeconds)
{
	if (JumpPhase != ERpgJumpPhase::Airborne)
	{
		ClearBackwardJumpStartHold();
		return false;
	}

	if (!bBackwardJumpStartHoldOpportunityConsumed)
	{
		if (!IsAirborneJumpAsset(CurrentAsset))
		{
			// The outgoing grounded sample may remain current during the initial Ground-to-Air blend.
			return false;
		}

		bBackwardJumpStartHoldOpportunityConsumed = true;
		if (bBackwardJumpStartHoldEligible && IsBackwardJumpStartAsset(CurrentAsset))
		{
			BackwardJumpStartHeldAsset = CurrentAsset;
			BackwardJumpStartHoldElapsed = 0.0f;
			bBackwardJumpStartHoldWasArmed = true;
		}
	}

	if (!BackwardJumpStartHeldAsset)
	{
		return false;
	}

	BackwardJumpStartHoldElapsed += FMath::Max(DeltaSeconds, 0.0f);
	const bool bShouldHold = ShouldHoldBackwardJumpStartPlayback(
		JumpPhase,
		CurrentAsset == BackwardJumpStartHeldAsset.Get(),
		CurrentAssetTime,
		CurrentAssetLength,
		CurrentAssetPlayRate,
		BackwardJumpStartHoldElapsed);
	if (!bShouldHold)
	{
		// Keep the opportunity consumed so a later search cannot re-arm the same start in this jump.
		BackwardJumpStartHeldAsset = nullptr;
		BackwardJumpStartHoldElapsed = 0.0f;
	}
	return bShouldHold;
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
	CharacterRotationMode = Proxy.RotationMode;
	LocomotionTrajectory = Proxy.TransformTrajectory;
	TrajectoryLandingPrediction = Proxy.TrajectoryLandingPrediction;
	PreTouchdownLandingSnapshot = Proxy.LandingSelectionSnapshot;
	ProceduralLocomotionAlpha = Proxy.ProceduralLocomotionAlpha;
	AirborneProceduralAlpha = Proxy.AirborneProceduralAlpha;
	bIsAnyMontagePlaying = Proxy.bIsAnyMontagePlaying;
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
		: ResolveMotionMatchingDatabaseRole(
			MotionMatchingNode->GetMotionMatchingState().SearchResult.SelectedDatabase.Get());
	if (CurrentMotionMatchingDatabaseRole == ERpgMotionMatchingDatabaseRole::None)
	{
		bCurrentMotionMatchingResultIsContinuingPose = false;
		CurrentMotionMatchingInterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	}

	const FRpgAnimInstanceProxy& Proxy = GetProxyOnAnyThread<FRpgAnimInstanceProxy>();
	FGroundMotionMatchingSelectionSnapshot DomainSnapshot;
	DomainSnapshot.Gait = Proxy.Gait;
	DomainSnapshot.Stance = Proxy.Stance;
	DomainSnapshot.MovementState = Proxy.MovementState;
	DomainSnapshot.RotationMode = Proxy.RotationMode;
	DomainSnapshot.WorldVelocity = Proxy.WorldVelocity;
	DomainSnapshot.WorldAcceleration = Proxy.WorldAcceleration;
	DomainSnapshot.GroundSpeed = Proxy.GroundSpeed;
	DomainSnapshot.CurrentDatabaseRole = CurrentMotionMatchingDatabaseRole;
	DomainSnapshot.bIsMovingOnGround = Proxy.bIsMovingOnGround;

	FGroundMotionMatchingDomainState CurrentGroundDomainState;
	CurrentGroundDomainState.PhysicalMovementState = Proxy.MovementState;
	CurrentGroundDomainState.Gait = Proxy.Gait;
	CurrentGroundDomainState.Stance = Proxy.Stance;
	CurrentGroundDomainState.bChooserMoving =
		IsGroundMotionMatchingChooserMoving(DomainSnapshot);
	const bool bInterruptGroundDomain = ShouldInterruptGroundMotionMatching(
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

	const ETurnInPlaceSearchMode SearchMode =
		ResolveTurnInPlaceSearchMode(bForceNewTurnRequest);
	const bool bForceNewLandingRequest =
		SearchMode == ETurnInPlaceSearchMode::NormalLocomotion &&
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
	const bool bContinueLoopingAirborneFall = ShouldHoldLoopingAirborneFallPlayback(
		JumpPhase,
		bBackwardJumpStartHoldWasArmed,
		VerticalVelocity,
		IsLoopingAirborneFallAsset(CurrentMotionMatchingAsset));
	const bool bInterruptLandingDatabaseExit = ShouldInterruptLandingDatabaseExit(
		JumpPhase,
		bLandingCompletionArmed,
		CurrentMotionMatchingDatabaseRole);

	TArray<UPoseSearchDatabase*, TInlineAllocator<5>> DatabasesToSearch;
	EPoseSearchInterruptMode InterruptMode = EPoseSearchInterruptMode::InterruptOnDatabaseChange;
	if (SearchMode == ETurnInPlaceSearchMode::SearchRequestedTurn)
	{
		DatabasesToSearch.Add(TurnInPlaceMotionMatchingDatabase);
		InterruptMode = EPoseSearchInterruptMode::ForceInterrupt;
	}
	else if (SearchMode == ETurnInPlaceSearchMode::ContinueSelectedTurn)
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
				RunStartFutureVelocityBeginTime,
				RunStartFutureVelocityEndTime,
				FutureTrajectoryVelocity);
		}

		FGroundMotionMatchingSelectionSnapshot SelectionSnapshot = DomainSnapshot;
		SelectionSnapshot.FutureVelocity = FutureTrajectoryVelocity;
		if (JumpPhase == ERpgJumpPhase::Airborne || bLocomotionIsFalling)
		{
			SelectionSnapshot.MovementState = ERpgLocomotionMovementState::Airborne;
		}

		for (const ERpgMotionMatchingDatabaseRole Role :
			ResolveMotionMatchingDatabaseRoles(SelectionSnapshot))
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
		ResolveMotionMatchingDatabaseRole(SearchResult.SelectedDatabase.Get());
	const FMotionMatchingPostSelectionState PostSelection =
		ResolveMotionMatchingPostSelection(
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
		AllowsMovingProceduralNodes();

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

