// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Character.h"


UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_MovementStopped, "Gameplay.MovementStopped");

namespace RpgCharacter
{
	static float GroundTraceDistance = 100000.0f;
	FAutoConsoleVariableRef CVar_GroundTraceDistance(TEXT("RpgCharacter.GroundTraceDistance"), GroundTraceDistance, TEXT("Distance to trace down when generating ground information."), ECVF_Cheat);
}

URpgCharacterMovementComponent::URpgCharacterMovementComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

bool URpgCharacterMovementComponent::ApplyMovementProfile(
	const FRpgCharacterMovementProfile& Profile)
{
	if (!RpgCharacterMovementRuntime::IsProfileRuntimeValid(Profile))
	{
		return false;
	}

	MovementProfile = Profile;
	GroundGait = ERpgLocomotionGait::Idle;
	DesiredGait = ERpgLocomotionGait::Idle;
	bHasMoveIntent = false;
	bHasMovementInput = false;
	return true;
}

bool URpgCharacterMovementComponent::UsesStandingGroundMovementProfile() const
{
	return MovementProfile.bOverrideCharacterMovement &&
		IsMovingOnGround() &&
		!IsCrouching();
}

bool URpgCharacterMovementComponent::HasMovementStoppedTag() const
{
	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC && ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped);
}

void URpgCharacterMovementComponent::CalcVelocity(
	float DeltaTime,
	float Friction,
	bool bFluid,
	float BrakingDeceleration)
{
	if (!UsesStandingGroundMovementProfile())
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}

	// These engine properties are global across movement modes. Scope the profile values to
	// this synchronous standing-ground solve so crouch, falling, swimming, and custom modes
	// keep their Blueprint-authored response instead of inheriting GASP walking physics.
	// ControlledCharacterMove scales Acceleration before PerformMovement. A landing or
	// uncrouch may enter this standing solve later in that same movement tick, so rescale the
	// preserved raw analog magnitude against the standing profile instead of applying one
	// frame of the previous movement mode's acceleration.
	const float SafeAnalogInput = FMath::IsFinite(AnalogInputModifier)
		? FMath::Clamp(AnalogInputModifier, 0.0f, 1.0f)
		: 0.0f;
	Acceleration = Acceleration.GetSafeNormal() *
		(MovementProfile.MaxAcceleration * SafeAnalogInput);
	const bool bSavedUseSeparateBrakingFriction = bUseSeparateBrakingFriction;
	const float SavedBrakingFrictionFactor = BrakingFrictionFactor;
	const float SavedBrakingFriction = BrakingFriction;
	bUseSeparateBrakingFriction = MovementProfile.bUseSeparateBrakingFriction;
	BrakingFrictionFactor = MovementProfile.BrakingFrictionFactor;
	BrakingFriction = MovementProfile.BrakingFriction;
	Super::CalcVelocity(
		DeltaTime,
		MovementProfile.GroundFriction,
		bFluid,
		BrakingDeceleration);
	bUseSeparateBrakingFriction = bSavedUseSeparateBrakingFriction;
	BrakingFrictionFactor = SavedBrakingFrictionFactor;
	BrakingFriction = SavedBrakingFriction;
}

void URpgCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	RefreshLocomotionSnapshot();
}

void URpgCharacterMovementComponent::OnMovementUpdated(
	float DeltaSeconds,
	const FVector& OldLocation,
	const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	RefreshLocomotionSnapshot();
}

void URpgCharacterMovementComponent::RefreshLocomotionSnapshot()
{
	const float InputMagnitude = GetAnalogInputModifier();
	bHasMovementInput = FMath::IsFinite(InputMagnitude) &&
		InputMagnitude > UE_KINDA_SMALL_NUMBER;
	DesiredGait = RpgCharacterMovementRuntime::ResolveDesiredGait(
		InputMagnitude,
		MovementProfile);
	bHasMoveIntent = RpgCharacterMovementRuntime::HasMoveIntent(
		InputMagnitude,
		MovementProfile);
	GroundGait = RpgCharacterMovementRuntime::ResolveGroundGait(
		IsMovingOnGround(),
		Velocity.Size2D(),
		InputMagnitude,
		GroundGait,
		MovementProfile);
}

void URpgCharacterMovementComponent::OnTeleported()
{
	Super::OnTeleported();
	MarkAnimationDiscontinuity();
}

void URpgCharacterMovementComponent::NotifyReplicatedAnimationTeleport()
{
	MarkAnimationDiscontinuity();
}

void URpgCharacterMovementComponent::OnClientCorrectionReceived(
	FNetworkPredictionData_Client_Character& ClientData,
	float TimeStamp,
	FVector NewLocation,
	FVector NewVelocity,
	FMovementBaseInterfaceData* NewMovementBaseInterfaceData,
	FName NewBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition,
	uint8 ServerMovementMode,
	FVector ServerGravityDirection)
{
	// NewLocation is already converted to world space by ClientAdjustPosition. Compare
	// it with the acknowledged move from the same timestamp, not the client's current
	// location which may be many unacknowledged moves ahead under latency. Also retain
	// the live pre-correction location so replay can detect a real presentation jump when
	// an earlier correction prunes the saved move that originally contained the error.
	if (UpdatedComponent)
	{
		if (!bHasPendingAnimationCorrection)
		{
			PendingAnimationCorrectionStartLocation =
				UpdatedComponent->GetComponentLocation();
		}
		const FVector ClientLocationAtCorrectedMove =
			ClientData.LastAckedMove.IsValid()
				? ClientData.LastAckedMove->SavedLocation
				: UpdatedComponent->GetComponentLocation();
		bHasPendingAnimationCorrection = true;
		bPendingAnimationCorrectionDiscontinuity |= FVector::DistSquared(
			ClientLocationAtCorrectedMove,
			NewLocation) > FMath::Square(NetworkLargeClientCorrectionDistance);
	}

	Super::OnClientCorrectionReceived(
		ClientData,
		TimeStamp,
		NewLocation,
		NewVelocity,
		NewMovementBaseInterfaceData,
		NewBaseBoneName,
		bHasBase,
		bBaseRelativePosition,
		ServerMovementMode,
		ServerGravityDirection);
}

bool URpgCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	const bool bHadPendingAnimationCorrection = bHasPendingAnimationCorrection;
	const bool bReplayedMoves = Super::ClientUpdatePositionAfterServerUpdate();

	if (bHadPendingAnimationCorrection)
	{
		const bool bLivePresentationDiscontinuity = UpdatedComponent &&
			FVector::DistSquared(
				PendingAnimationCorrectionStartLocation,
				UpdatedComponent->GetComponentLocation()) >
				FMath::Square(NetworkLargeClientCorrectionDistance);
		const bool bShouldMarkAnimationDiscontinuity =
			bPendingAnimationCorrectionDiscontinuity ||
			bLivePresentationDiscontinuity;
		bHasPendingAnimationCorrection = false;
		PendingAnimationCorrectionStartLocation = FVector::ZeroVector;
		bPendingAnimationCorrectionDiscontinuity = false;
		if (bShouldMarkAnimationDiscontinuity)
		{
			MarkAnimationDiscontinuity();
		}
	}

	return bReplayedMoves;
}

void URpgCharacterMovementComponent::SmoothCorrection(
	const FVector& OldLocation,
	const FQuat& OldRotation,
	const FVector& NewLocation,
	const FQuat& NewRotation)
{
	// Ordinary simulated-proxy updates are intentionally left to network smoothing.
	// Only an untagged correction beyond UE's actual no-smoothing distance is a hard fallback.
	const FNetworkPredictionData_Client_Character* ClientData =
		GetPredictionData_Client_Character();
	const float NoSmoothDistance = ClientData
		? ClientData->NoSmoothNetUpdateDist
		: NetworkNoSmoothUpdateDistance;
	const bool bHardSimulatedProxyCorrection =
		CharacterOwner &&
		CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy &&
		FVector::DistSquared(OldLocation, NewLocation) >
			FMath::Square(FMath::Max(NoSmoothDistance, 0.0f));

	Super::SmoothCorrection(OldLocation, OldRotation, NewLocation, NewRotation);
	if (bHardSimulatedProxyCorrection)
	{
		MarkAnimationDiscontinuity();
	}
}

void URpgCharacterMovementComponent::MarkAnimationDiscontinuity()
{
	if (LastAnimationDiscontinuityFrame == GFrameCounter)
	{
		return;
	}

	LastAnimationDiscontinuityFrame = GFrameCounter;
	++AnimationDiscontinuitySerial;
	if (AnimationDiscontinuitySerial == 0)
	{
		++AnimationDiscontinuitySerial;
	}
}

bool URpgCharacterMovementComponent::CanAttemptJump() const
{
	// Same as UCharacterMovementComponent's implementation but without the crouch check
	return IsJumpAllowed() &&
		(IsMovingOnGround() || IsFalling()); // Falling included for double-jump and non-zero jump hold time, but validated by character.
}

const FRpgCharacterGroundInfo& URpgCharacterMovementComponent::GetGroundInfo()
{
	if (!CharacterOwner || (GFrameCounter == CachedGroundInfo.LastUpdateFrame))
	{
		return CachedGroundInfo;
	}

	if (MovementMode == MOVE_Walking)
	{
		CachedGroundInfo.GroundHitResult = CurrentFloor.HitResult;
		CachedGroundInfo.GroundDistance = 0.0f;
	}
	else
	{
		const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = (UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn);
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, (TraceStart.Z - RpgCharacter::GroundTraceDistance - CapsuleHalfHeight));

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParam);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = RpgCharacter::GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking)
		{
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit)
		{
			CachedGroundInfo.GroundDistance = FMath::Max((HitResult.Distance - CapsuleHalfHeight), 0.0f);
		}
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;

	return CachedGroundInfo;
}

FRotator URpgCharacterMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	if (HasMovementStoppedTag())
	{
		return FRotator::ZeroRotator;
	}

	return Super::GetDeltaRotation(DeltaTime);
}

float URpgCharacterMovementComponent::GetMaxSpeed() const
{
	if (HasMovementStoppedTag())
	{
		return 0.0f;
	}

	if (UsesStandingGroundMovementProfile())
	{
		// FSavedMove captures MaxSpeed before PerformMovement refreshes the presentation
		// snapshot. Resolve the physical cap from CharacterMovement's current analog value
		// so Walk/Run boundary moves are not combined with a one-frame-old gait cap.
		const ERpgLocomotionGait PhysicalGait =
			RpgCharacterMovementRuntime::ResolveDesiredGait(
				GetAnalogInputModifier(),
				MovementProfile);
		return RpgCharacterMovementRuntime::ResolveGroundSpeedCap(
			PhysicalGait == ERpgLocomotionGait::Run
				? ERpgLocomotionGait::Run
				: ERpgLocomotionGait::Walk,
			false,
			0.0f,
			MovementProfile);
	}

	return Super::GetMaxSpeed();
}

float URpgCharacterMovementComponent::GetMinAnalogSpeed() const
{
	// CharacterMovement clamps any non-zero input up to this floor. It must therefore
	// participate in the same GAS stop contract as MaxSpeed or held input can still
	// drive the GASP profile at MinAnalogGroundSpeed while movement is disabled.
	if (HasMovementStoppedTag())
	{
		return 0.0f;
	}

	return UsesStandingGroundMovementProfile()
		? MovementProfile.MinAnalogGroundSpeed
		: Super::GetMinAnalogSpeed();
}

float URpgCharacterMovementComponent::GetMaxAcceleration() const
{
	return UsesStandingGroundMovementProfile()
		? MovementProfile.MaxAcceleration
		: Super::GetMaxAcceleration();
}

float URpgCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (UsesStandingGroundMovementProfile())
	{
		return RpgCharacterMovementRuntime::ResolveGroundBrakingDeceleration(
			bHasMovementInput,
			MovementProfile);
	}

	return Super::GetMaxBrakingDeceleration();
}
