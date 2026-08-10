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
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "UObject/Package.h"

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
constexpr float BackwardJumpStartHoldTimeout = 1.25f;
constexpr float BackwardJumpStartReleaseLeadTime = 0.2f;

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
		TransformTrajectory.Samples.Reset();
		DesiredControllerYawLastUpdate = 0.0f;
		bTurnInPlaceHardReset = true;
		PreviousOwnerUniqueId = 0;
		bHasPreviousOwnerSnapshot = false;
		return;
	}
	RotationMode = Character->GetRotationMode();

	URpgCharacterMovementComponent* MovementComponent = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!MovementComponent)
	{
		LastNonZeroWorldVelocity = FVector::ZeroVector;
		TransformTrajectory.Samples.Reset();
		DesiredControllerYawLastUpdate = 0.0f;
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

	if (RpgAnimInstance->ShouldGeneratePoseSearchTrajectory())
	{
		// This controller-facing character turns its component directly. Extrapolating a one-frame mouse
		// or replicated yaw delta across the prediction horizon exaggerates small turns and selects false
		// 90/180-degree poses. Current mesh facing and trajectory history already carry the real rotation.
		TrajectoryGenerationData.RotateTowardsMovementSpeed = 0.0f;
		TrajectoryGenerationData.BendVelocityTowardsAcceleration = 0.0f;
		TrajectoryGenerationData.MaxControllerYawRate = 0.0f;

		// Avoid interpreting the initial world yaw as a one-frame controller turn.
		if (TransformTrajectory.Samples.IsEmpty())
		{
			DesiredControllerYawLastUpdate = Character->GetViewRotation().Yaw;
		}

		FTransformTrajectory GeneratedTrajectory;
		UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
			RpgAnimInstance,
			TrajectoryGenerationData,
			DeltaSeconds,
			TransformTrajectory,
			DesiredControllerYawLastUpdate,
			GeneratedTrajectory,
			URpgAnimInstance::TrajectoryHistorySamplingInterval,
			URpgAnimInstance::TrajectoryHistorySampleCount,
			URpgAnimInstance::TrajectoryPredictionSamplingInterval,
			URpgAnimInstance::TrajectoryPredictionSampleCount);
		TransformTrajectory = MoveTemp(GeneratedTrajectory);
	}
	else
	{
		TransformTrajectory.Samples.Reset();
		DesiredControllerYawLastUpdate = 0.0f;
	}
}

void URpgAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

bool URpgAnimInstance::IsGroundMotionMatchingChooserMoving(
	const FGroundMotionMatchingSelectionSnapshot& Snapshot)
{
	return Snapshot.bIsMovingOnGround &&
		!Snapshot.WorldVelocity.ContainsNaN() &&
		!Snapshot.WorldAcceleration.ContainsNaN() &&
		Snapshot.WorldVelocity.SizeSquared() > FMath::Square(ChooserVelocityTolerance) &&
		Snapshot.WorldAcceleration.SizeSquared() > FMath::Square(ChooserAccelerationTolerance);
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

URpgAnimInstance::FResolvedGroundMotionMatchingDatabases
URpgAnimInstance::ResolveGroundMotionMatchingDatabases(
	const FGroundMotionMatchingSelectionSnapshot& Snapshot,
	const FRpgGroundMotionMatchingDatabaseSets& DatabaseSets)
{
	FResolvedGroundMotionMatchingDatabases ResolvedDatabases;
	if (!Snapshot.bIsMovingOnGround)
	{
		return ResolvedDatabases;
	}

	auto AddDatabase = [&ResolvedDatabases](UPoseSearchDatabase* Database)
	{
		if (Database && !ResolvedDatabases.Contains(Database))
		{
			ResolvedDatabases.Add(Database);
		}
	};
	auto AddDatabaseAtIndex = [&AddDatabase](
		const TArray<TObjectPtr<UPoseSearchDatabase>>& Databases,
		int32 Index)
	{
		if (Databases.IsValidIndex(Index))
		{
			AddDatabase(Databases[Index].Get());
		}
	};

	const float SafeGroundSpeed = FMath::IsFinite(Snapshot.GroundSpeed)
		? FMath::Max(Snapshot.GroundSpeed, 0.0f)
		: 0.0f;
	const bool bChooserMoving = IsGroundMotionMatchingChooserMoving(Snapshot);
	if (!bChooserMoving)
	{
		// GASP's logical Idle rows are inclusive and intentionally overlap. Preserve their source
		// order so exact boundaries expose both adjacent rows to the Pose Search cost comparison.
		if (SafeGroundSpeed <= WalkStopMinimumSpeed)
		{
			AddDatabaseAtIndex(DatabaseSets.Idle, 0);
		}
		if (SafeGroundSpeed >= WalkStopMinimumSpeed)
		{
			AddDatabaseAtIndex(DatabaseSets.Walk, 1);
		}
		if (SafeGroundSpeed >= RunStopMinimumSpeed)
		{
			AddDatabaseAtIndex(DatabaseSets.Run, 3);
		}
		// GASP's speed-only 550 cm/s row assumes its Run speed remains below that boundary.
		// This project tunes normal Run to 600 cm/s, so require the preserved gait as well;
		// otherwise its forward-only Sprint clips can abruptly replace an ordinary Run Stop.
		if (Snapshot.Gait == ERpgLocomotionGait::Sprint &&
			SafeGroundSpeed >= SprintStopMinimumSpeed)
		{
			AddDatabaseAtIndex(DatabaseSets.Sprint, 1);
		}
		return ResolvedDatabases;
	}

	switch (Snapshot.Gait)
	{
	case ERpgLocomotionGait::Idle:
		AddDatabaseAtIndex(DatabaseSets.Idle, 0);
		break;
	case ERpgLocomotionGait::Walk:
		AddDatabaseAtIndex(DatabaseSets.Walk, 0);
		break;
	case ERpgLocomotionGait::Run:
	{
		const float SafeFutureGroundSpeed = FMath::IsFinite(Snapshot.FutureGroundSpeed)
			? FMath::Max(Snapshot.FutureGroundSpeed, 0.0f)
			: 0.0f;
		const bool bSearchStarts =
			!Snapshot.bCurrentDatabaseIsRunPivot &&
			SafeFutureGroundSpeed >= SafeGroundSpeed + RunStartMinimumFutureSpeedGain;

		const FVector HorizontalVelocity(Snapshot.WorldVelocity.X, Snapshot.WorldVelocity.Y, 0.0f);
		const FVector HorizontalAcceleration(Snapshot.WorldAcceleration.X, Snapshot.WorldAcceleration.Y, 0.0f);
		const float TurnAngle = FMath::Abs(FMath::FindDeltaAngleDegrees(
			HorizontalVelocity.Rotation().Yaw,
			HorizontalAcceleration.Rotation().Yaw));
		const bool bSearchPivots = TurnAngle >= GetRunPivotMinimumAngle(Snapshot.RotationMode);

		ResolvedDatabases.Reserve(3);
		// Preserve the source Sparse chooser result order: Starts, Loops, then Pivots.
		if (bSearchStarts)
		{
			AddDatabaseAtIndex(DatabaseSets.Run, 2);
		}
		AddDatabaseAtIndex(DatabaseSets.Run, 0);
		if (bSearchPivots)
		{
			AddDatabaseAtIndex(DatabaseSets.Run, 1);
		}
		break;
	}
	case ERpgLocomotionGait::Sprint:
		AddDatabaseAtIndex(DatabaseSets.Sprint, 0);
		break;
	default:
		break;
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

#if WITH_EDITOR
EDataValidationResult URpgAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);
	if (bGeneratePoseSearchTrajectory)
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
		if (AirborneMotionMatchingDatabases.IsEmpty())
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no airborne Pose Search database is configured.")));
		}
		if (!LandingMotionMatchingDatabase)
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no exclusive landing Pose Search database is configured.")));
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
	ResetFootPlacementInitializationState(GetProxyOnGameThread<FRpgAnimInstanceProxy>());
	TurnInPlaceRequestSerial = 0;
	TurnInPlaceInterruptedRequestSerial = 0;
	TurnInPlaceHardResetReasonsLastFrame = 0;
	bResetOffsetRootEveryFrame = false;
	bTurnInPlaceInitializationResetPending = true;
	FootPlacementSnapshot = FRpgFootPlacementSnapshot();
	FootPlacementAlpha = 0.0f;
	AirborneProceduralAlpha = 0.0f;
	LandingRequestSerial = 0;
	LandingInterruptedRequestSerial = 0;
	PreviousGroundMotionMatchingDomainState = FGroundMotionMatchingDomainState();
	bHasPreviousGroundMotionMatchingDomainState = false;
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
		// The generic pre-update callback reads the previous completed SearchResult. Keep that latch
		// authoritative through a transient Blend Stack mismatch until the exact asset is observed.
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
	ClearLandingSelection();
	ClearBackwardJumpStartHold();
}

void URpgAnimInstance::BeginAirbornePhase(bool bAscendingTakeoff)
{
	JumpPhase = ERpgJumpPhase::Airborne;
	LandingStateElapsed = 0.0f;
	ClearLandingSelection();
	ClearBackwardJumpStartHold();
	bBackwardJumpStartHoldEligible = bAscendingTakeoff;
}

void URpgAnimInstance::BeginLandingRequest()
{
	JumpPhase = ERpgJumpPhase::Landing;
	LandingStateElapsed = 0.0f;
	ClearLandingSelection();
	ClearBackwardJumpStartHold();
	++LandingRequestSerial;
	if (LandingRequestSerial == 0)
	{
		++LandingRequestSerial;
	}
}

bool URpgAnimInstance::IsLandingEligible(const FRpgAnimInstanceProxy& Proxy) const
{
	return LandingMotionMatchingDatabase != nullptr &&
		Proxy.MovementState == ERpgLocomotionMovementState::Grounded &&
		Proxy.bIsMovingOnGround &&
		Proxy.GroundSpeed <= LightLandingIdleSpeedThreshold &&
		!Proxy.bHasGroundedMoveIntent &&
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

	if (JumpPhase == ERpgJumpPhase::Airborne)
	{
		if (IsLandingEligible(Proxy))
		{
			BeginLandingRequest();
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

	if (!IsLandingEligible(Proxy))
	{
		ResetJumpPhaseRuntime();
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
		!LandingMotionMatchingDatabase ||
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
		SelectedDatabase != LandingMotionMatchingDatabase.Get() ||
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

bool URpgAnimInstance::IsAirborneJumpStartAsset(const UAnimationAsset* Asset)
{
	const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Asset);
	if (!Sequence || Sequence->bLoop)
	{
		return false;
	}

	TStringBuilder<256> PackageName;
	Sequence->GetOutermost()->GetPathName(nullptr, PackageName);
	return PackageName.ToView().StartsWith(
		TEXTVIEW("/RpgGaspLocomotion/Animations/Jump/Starts/"));
}

bool URpgAnimInstance::IsGroundMovingAsset(const UAnimationAsset* Asset)
{
	const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Asset);
	if (!Sequence)
	{
		return false;
	}

	static constexpr FStringView GroundMovingPackagePrefixes[] = {
		TEXTVIEW("/RpgGaspLocomotion/Animations/Stand/Walk/"),
		TEXTVIEW("/RpgGaspLocomotion/Animations/Stand/Run/"),
		TEXTVIEW("/RpgGaspLocomotion/Animations/Stand/Sprint/"),
	};
	TStringBuilder<256> PackageName;
	Sequence->GetOutermost()->GetPathName(nullptr, PackageName);
	for (const FStringView PackagePrefix : GroundMovingPackagePrefixes)
	{
		if (PackageName.ToView().StartsWith(PackagePrefix))
		{
			return true;
		}
	}
	return false;
}

bool URpgAnimInstance::IsAirborneJumpAsset(const UAnimationAsset* Asset)
{
	const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Asset);
	if (!Sequence)
	{
		return false;
	}

	TStringBuilder<256> PackageName;
	Sequence->GetOutermost()->GetPathName(nullptr, PackageName);
	return PackageName.ToView().StartsWith(
			TEXTVIEW("/RpgGaspLocomotion/Animations/Jump/Starts/")) ||
		PackageName.ToView().StartsWith(
			TEXTVIEW("/RpgGaspLocomotion/Animations/Jump/Airborne/"));
}

bool URpgAnimInstance::IsBackwardJumpStartAsset(const UAnimationAsset* Asset)
{
	if (!IsAirborneJumpStartAsset(Asset))
	{
		return false;
	}

	TStringBuilder<256> PackageName;
	Asset->GetOutermost()->GetPathName(nullptr, PackageName);
	return PackageName.ToView().StartsWith(
		TEXTVIEW("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_B_"));
}

bool URpgAnimInstance::IsLoopingAirborneFallAsset(const UAnimationAsset* Asset)
{
	const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Asset);
	if (!Sequence || !Sequence->bLoop)
	{
		return false;
	}

	TStringBuilder<256> PackageName;
	Sequence->GetOutermost()->GetPathName(nullptr, PackageName);
	return PackageName.ToView().StartsWith(
		TEXTVIEW("/RpgGaspLocomotion/Animations/Jump/Airborne/"));
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

	const FRpgAnimInstanceProxy& Proxy = GetProxyOnAnyThread<FRpgAnimInstanceProxy>();
	FGroundMotionMatchingSelectionSnapshot DomainSnapshot;
	DomainSnapshot.Gait = Proxy.Gait;
	DomainSnapshot.RotationMode = Proxy.RotationMode;
	DomainSnapshot.WorldVelocity = Proxy.WorldVelocity;
	DomainSnapshot.WorldAcceleration = Proxy.WorldAcceleration;
	DomainSnapshot.GroundSpeed = Proxy.GroundSpeed;
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
	if (!bForceNewTurnRequest &&
		TurnInPlaceState == ERpgTurnInPlaceState::Active &&
		!bTurnInPlaceSelectionLatched &&
		TurnInPlaceMotionMatchingDatabase)
	{
		// The pilot binds this function to the generic node UpdateFunction, which runs before the
		// current search. SearchResult therefore belongs to the previous completed node update.
		const FPoseSearchBlueprintResult& SearchResult =
			MotionMatchingNode->GetMotionMatchingState().SearchResult;
		if (SearchResult.SelectedDatabase.Get() == TurnInPlaceMotionMatchingDatabase.Get())
		{
			TryLatchTurnInPlaceSelection(
				Cast<UAnimationAsset>(SearchResult.SelectedAnim.Get()),
				SearchResult.SelectedDatabase.Get(),
				SearchResult.SelectedTime,
				SearchResult.bLoop,
				TurnInPlaceRequestSerial);
		}
	}

	const FAnimationUpdateContext* AnimationContext = Context.GetContext();
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
	if (SearchMode == ETurnInPlaceSearchMode::NormalLocomotion &&
		!bForceNewLandingRequest &&
		JumpPhase == ERpgJumpPhase::Landing &&
		!bLandingSelectionLatched &&
		LandingMotionMatchingDatabase)
	{
		// As with TIR, SearchResult belongs to the previous completed Motion Matching update.
		const FPoseSearchBlueprintResult& SearchResult =
			MotionMatchingNode->GetMotionMatchingState().SearchResult;
		if (SearchResult.SelectedDatabase.Get() == LandingMotionMatchingDatabase.Get())
		{
			TryLatchLandingSelection(
				Cast<UAnimationAsset>(SearchResult.SelectedAnim.Get()),
				SearchResult.SelectedDatabase.Get(),
				SearchResult.SelectedTime,
				SearchResult.bLoop,
				LandingRequestSerial);
		}
	}

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
		else if (LandingMotionMatchingDatabase)
		{
			DatabasesToSearch.Add(LandingMotionMatchingDatabase);
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
	else if (JumpPhase == ERpgJumpPhase::Airborne || bLocomotionIsFalling)
	{
		DatabasesToSearch.Reserve(AirborneMotionMatchingDatabases.Num());
		for (UPoseSearchDatabase* Database : AirborneMotionMatchingDatabases)
		{
			if (Database)
			{
				DatabasesToSearch.Add(Database);
			}
		}
	}
	else if (LocomotionStance == ERpgLocomotionStance::Crouching)
	{
		if (CrouchingMotionMatchingDatabase)
		{
			DatabasesToSearch.Add(CrouchingMotionMatchingDatabase);
		}
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
		SelectionSnapshot.FutureGroundSpeed = FutureTrajectoryVelocity.Size2D();
		SelectionSnapshot.bCurrentDatabaseIsRunPivot =
			GroundMotionMatchingDatabaseSets.Run.IsValidIndex(1) &&
			MotionMatchingNode->GetMotionMatchingState().SearchResult.SelectedDatabase.Get() ==
				GroundMotionMatchingDatabaseSets.Run[1].Get();
		const FResolvedGroundMotionMatchingDatabases GroundDatabases =
			ResolveGroundMotionMatchingDatabases(
				SelectionSnapshot,
				GroundMotionMatchingDatabaseSets);
		for (UPoseSearchDatabase* Database : GroundDatabases)
		{
			DatabasesToSearch.Add(Database);
		}
		InterruptMode = bInterruptGroundDomain
			? EPoseSearchInterruptMode::InterruptOnDatabaseChange
			: EPoseSearchInterruptMode::DoNotInterrupt;
	}

	PreviousGroundMotionMatchingDomainState = CurrentGroundDomainState;
	bHasPreviousGroundMotionMatchingDomainState = true;

	MotionMatchingNode->SetDatabasesToSearch(
		MakeArrayView(DatabasesToSearch),
		InterruptMode);
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
		IsGroundMovingAsset(CurrentAnimAsset);
	const bool bIsAirborneJumpPose =
		bAllowsSampleProceduralNodes &&
		IsAirborneJumpAsset(CurrentAnimAsset);
	const bool bIsAirborneJumpStartPose =
		bIsAirborneJumpPose &&
		IsAirborneJumpStartAsset(CurrentAnimAsset);
	const FGaspProceduralGates Gates = ResolveGaspProceduralGates(
		bIsGroundMovingPose,
		SupportedLocomotionAlpha,
		bIsAirborneJumpPose,
		bIsAirborneJumpStartPose,
		IsActiveLandingAsset(CurrentAnimAsset),
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

