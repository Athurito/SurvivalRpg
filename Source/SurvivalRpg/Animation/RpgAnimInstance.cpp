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
			0.04f,
			10,
			0.2f,
			8);
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

#if WITH_EDITOR
EDataValidationResult URpgAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);
	if (bGeneratePoseSearchTrajectory)
	{
		if (GroundMotionMatchingDatabases.IsEmpty())
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no grounded Pose Search database is configured.")));
		}
		else if (GroundMotionMatchingDatabases.Num() != 4)
		{
			Context.AddError(FText::FromString(
				TEXT("Ground Motion Matching databases must be ordered as exactly Idle, Walk, Run, and Sprint.")));
		}
		if (AirborneMotionMatchingDatabases.IsEmpty())
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no airborne Pose Search database is configured.")));
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
		ValidateDatabases(GroundMotionMatchingDatabases, TEXT("Grounded"));
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
	bTurnInPlaceHardResetConditionLastFrame = false;
	bResetOffsetRootEveryFrame = false;
	bTurnInPlaceInitializationResetPending = true;
	FootPlacementSnapshot = FRpgFootPlacementSnapshot();
	FootPlacementAlpha = 0.0f;
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

	const bool bHardResetCondition =
		Proxy.bTurnInPlaceHardReset ||
		!SupportsTurnInPlace(Proxy.RotationMode) ||
		Proxy.bHasTurnInPlaceBlockingGameplayTag ||
		Proxy.bIsAnyMontagePlaying ||
		Proxy.bIsCrouching ||
		Proxy.MovementState != ERpgLocomotionMovementState::Grounded;
	if (bHardResetCondition)
	{
		const bool bHasTurnStateToClear =
			TurnInPlaceState != ERpgTurnInPlaceState::Inactive ||
			FMath::Abs(TurnInPlaceAccumulatedYaw) > UE_KINDA_SMALL_NUMBER;
		const bool bPulseHardReset = !bTurnInPlaceHardResetConditionLastFrame || bHasTurnStateToClear;
		ResetTurnInPlaceRuntime(bPulseHardReset);
		bTurnInPlaceHardResetConditionLastFrame = true;
		LocomotionTrajectory = Proxy.TransformTrajectory;
		return;
	}
	bTurnInPlaceHardResetConditionLastFrame = false;

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
	bIsAnyMontagePlaying = Proxy.bIsAnyMontagePlaying;
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

	if (TurnInPlaceState == ERpgTurnInPlaceState::Active && bTurnInPlaceSelectionLatched)
	{
		const FAnimationUpdateContext* AnimationContext = Context.GetContext();
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
	else if (bLocomotionIsFalling)
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
		const int32 GaitDatabaseIndex = static_cast<int32>(LocomotionGait);
		if (GroundMotionMatchingDatabases.IsValidIndex(GaitDatabaseIndex))
		{
			if (UPoseSearchDatabase* Database = GroundMotionMatchingDatabases[GaitDatabaseIndex])
			{
				DatabasesToSearch.Add(Database);
			}
		}
	}

	MotionMatchingNode->SetDatabasesToSearch(
		MakeArrayView(DatabasesToSearch),
		InterruptMode);
}

void URpgAnimInstance::GetGaspBlendStackInputs(
	const FAnimNodeReference& Node,
	UAnimationAsset*& CurrentAnimAsset,
	float& CurrentAnimAssetTime,
	float& MovingAlpha,
	float& OrientationWarpingAlpha,
	FQuat& DesiredFacing,
	FVector& LocomotionDirection,
	bool& bEnableSteering) const
{
	CurrentAnimAsset = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(Node);
	CurrentAnimAssetTime = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAssetTime(Node);

	const bool bIsBoundedMovingPose =
		bIsMovingOnGround &&
		!bLocomotionIsFalling &&
		!bIsCrouching &&
		!bIsAnyMontagePlaying &&
		AllowsMovingProceduralNodes() &&
		LocomotionGait != ERpgLocomotionGait::Idle;
	MovingAlpha = bIsBoundedMovingPose
		? FMath::Clamp(ProceduralLocomotionAlpha, 0.0f, 1.0f)
		: 0.0f;

	OrientationWarpingAlpha = 0.0f;
	if (MovingAlpha > UE_KINDA_SMALL_NUMBER)
	{
		if (const UAnimSequenceBase* CurrentSequence = Cast<UAnimSequenceBase>(CurrentAnimAsset))
		{
			static const FName EnableWarpingCurveName(TEXT("Enable_Warping"));
			float EnableWarpingCurveValue = 0.0f;
			if (UAnimationWarpingLibrary::GetCurveValueFromAnimation(
				CurrentSequence,
				EnableWarpingCurveName,
				CurrentAnimAssetTime,
				EnableWarpingCurveValue))
			{
				OrientationWarpingAlpha =
					MovingAlpha * FMath::Clamp(EnableWarpingCurveValue, 0.0f, 1.0f);
			}
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
	bEnableSteering =
		MovingAlpha > UE_KINDA_SMALL_NUMBER &&
		bHasActiveBlendStackAsset &&
		bHasTrajectory;
}

FAnimInstanceProxy* URpgAnimInstance::CreateAnimInstanceProxy()
{
	return new FRpgAnimInstanceProxy(this);
}

void URpgAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FRpgAnimInstanceProxy*>(InProxy);
}

