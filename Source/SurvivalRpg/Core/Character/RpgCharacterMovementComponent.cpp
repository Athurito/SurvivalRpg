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

void FSavedMove_RpgCharacter::Clear()
{
	Super::Clear();
	bSavedRunGait = false;
}

void FSavedMove_RpgCharacter::SetMoveFor(
	ACharacter* Character,
	float InDeltaTime,
	const FVector& NewAcceleration,
	FNetworkPredictionData_Client_Character& ClientData)
{
	URpgCharacterMovementComponent* MovementComponent = Character
		? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	FVector AccelerationForSuper = NewAcceleration;
	if (MovementComponent)
	{
		const FVector CanonicalAcceleration =
			MovementComponent->RoundAcceleration(NewAcceleration);
		const float MaxAcceleration = MovementComponent->GetMaxAcceleration();
		const float CanonicalInputMagnitude = MaxAcceleration > UE_SMALL_NUMBER
			? FMath::Clamp(CanonicalAcceleration.Size() / MaxAcceleration, 0.0f, 1.0f)
			: 0.0f;
		const float PhysicalInputMagnitude =
			RpgCharacterMovementRuntime::ResolvePhysicalInputMagnitude(
				CanonicalInputMagnitude,
				MovementComponent->GetMovementProfile());
		if (MovementComponent->UsesStandingGroundMovementProfile() &&
			PhysicalInputMagnitude <= 0.0f)
		{
			AccelerationForSuper = FVector::ZeroVector;
		}
		MovementComponent->UpdatePredictedGaitFromInput(
			PhysicalInputMagnitude);
	}
	bSavedRunGait = MovementComponent &&
		MovementComponent->GetMovementProfile().bOverrideCharacterMovement &&
		MovementComponent->GetDesiredGait() == ERpgLocomotionGait::Run;

	// Super captures GetMaxSpeed here, so the component's predicted gait must already
	// represent this exact input move before entering the engine implementation. Preserve
	// UE's raw client-only AccelMag/AccelNormal combining data outside the physical deadzone.
	Super::SetMoveFor(Character, InDeltaTime, AccelerationForSuper, ClientData);
}

void FSavedMove_RpgCharacter::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	if (URpgCharacterMovementComponent* MovementComponent = Character
		? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr)
	{
		MovementComponent->RestorePredictedGaitFromSavedMove(bSavedRunGait);
	}
}

bool FSavedMove_RpgCharacter::CanCombineWith(
	const FSavedMovePtr& NewMove,
	ACharacter* Character,
	float MaxDelta) const
{
	const bool bNewMoveRunGait = NewMove.IsValid() &&
		(NewMove->GetCompressedFlags() & FLAG_Custom_0) != 0;
	if (!NewMove.IsValid() || bSavedRunGait != bNewMoveRunGait)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

uint8 FSavedMove_RpgCharacter::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedRunGait)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

FNetworkPredictionData_Client_RpgCharacter::FNetworkPredictionData_Client_RpgCharacter(
	const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_RpgCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_RpgCharacter());
}

URpgCharacterMovementComponent::URpgCharacterMovementComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

FNetworkPredictionData_Client* URpgCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		URpgCharacterMovementComponent* MutableThis =
			const_cast<URpgCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData =
			new FNetworkPredictionData_Client_RpgCharacter(*this);
	}

	return ClientPredictionData;
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

void URpgCharacterMovementComponent::ControlledCharacterMove(
	const FVector& InputVector,
	float DeltaSeconds)
{
	if (MovementProfile.bOverrideCharacterMovement)
	{
		const FVector ConstrainedInput = ConstrainInputAcceleration(InputVector);
		float InputMagnitude = ConstrainedInput.Size();
		if (UsesStandingGroundMovementProfile())
		{
			const float MaxAccelerationValue = GetMaxAcceleration();
			const FVector CanonicalAcceleration =
				ScaleInputAcceleration(ConstrainedInput);
			InputMagnitude = MaxAccelerationValue > UE_SMALL_NUMBER
				? FMath::Clamp(
					CanonicalAcceleration.Size() / MaxAccelerationValue,
					0.0f,
					1.0f)
				: 0.0f;
		}
		UpdatePredictedGaitFromInput(InputMagnitude);
	}

	Super::ControlledCharacterMove(InputVector, DeltaSeconds);
}

FVector URpgCharacterMovementComponent::ScaleInputAcceleration(
	const FVector& InputAcceleration) const
{
	const FVector ScaledAcceleration =
		Super::ScaleInputAcceleration(InputAcceleration);
	if (!UsesStandingGroundMovementProfile())
	{
		return ScaledAcceleration;
	}

	// Local authority and standalone movement must use the same NetQuantize10 boundary
	// decisions as autonomous SavedMoves and the server that receives them.
	const FVector CanonicalAcceleration = RoundAcceleration(ScaledAcceleration);
	const float MaxAccelerationValue = GetMaxAcceleration();
	const float CanonicalInputMagnitude = MaxAccelerationValue > UE_SMALL_NUMBER
		? FMath::Clamp(
			CanonicalAcceleration.Size() / MaxAccelerationValue,
			0.0f,
			1.0f)
		: 0.0f;
	if (RpgCharacterMovementRuntime::ResolvePhysicalInputMagnitude(
			CanonicalInputMagnitude,
			MovementProfile) <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	return CanonicalAcceleration;
}

void URpgCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	RestorePredictedGaitFromSavedMove(
		(Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
}

void URpgCharacterMovementComponent::RestorePredictedGaitFromSavedMove(
	bool bSavedRunGait)
{
	if (MovementProfile.bOverrideCharacterMovement)
	{
		DesiredGait = bSavedRunGait
			? ERpgLocomotionGait::Run
			: ERpgLocomotionGait::Walk;
	}
}

void URpgCharacterMovementComponent::UpdatePredictedGaitFromInput(
	float InputMagnitude)
{
	if (MovementProfile.bOverrideCharacterMovement)
	{
		DesiredGait = RpgCharacterMovementRuntime::ResolveDesiredGait(
			InputMagnitude,
			DesiredGait,
			MovementProfile);
	}
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
	// uncrouch may enter this standing solve later in that same movement tick, so rebuild and
	// network-canonicalize the preserved analog magnitude against the standing profile instead
	// of applying one frame of the previous movement mode's acceleration.
	const float RawAnalogInput = FMath::IsFinite(AnalogInputModifier)
		? FMath::Clamp(AnalogInputModifier, 0.0f, 1.0f)
		: 0.0f;
	const FVector StandingAcceleration = RoundAcceleration(
		Acceleration.GetSafeNormal() *
		(MovementProfile.MaxAcceleration * RawAnalogInput));
	const float CanonicalInputMagnitude = MovementProfile.MaxAcceleration > UE_SMALL_NUMBER
		? FMath::Clamp(
			StandingAcceleration.Size() / MovementProfile.MaxAcceleration,
			0.0f,
			1.0f)
		: 0.0f;
	const float PhysicalInputMagnitude =
		RpgCharacterMovementRuntime::ResolvePhysicalInputMagnitude(
			CanonicalInputMagnitude,
			MovementProfile);
	AnalogInputModifier = PhysicalInputMagnitude;
	Acceleration = PhysicalInputMagnitude > 0.0f
		? StandingAcceleration
		: FVector::ZeroVector;
	bHasMovementInput = PhysicalInputMagnitude > UE_KINDA_SMALL_NUMBER;
	bHasMoveIntent = bHasMovementInput;
	UpdatePredictedGaitFromInput(PhysicalInputMagnitude);
	const bool bSavedUseSeparateBrakingFriction = bUseSeparateBrakingFriction;
	const float SavedBrakingFrictionFactor = BrakingFrictionFactor;
	const float SavedBrakingFriction = BrakingFriction;
	bUseSeparateBrakingFriction = MovementProfile.bUseSeparateBrakingFriction;
	BrakingFrictionFactor = MovementProfile.BrakingFrictionFactor;
	BrakingFriction = MovementProfile.BrakingFriction;
	const float CanonicalBrakingDeceleration =
		RpgCharacterMovementRuntime::ResolveGroundBrakingDeceleration(
			bHasMovementInput,
			MovementProfile);
	Super::CalcVelocity(
		DeltaTime,
		MovementProfile.GroundFriction,
		bFluid,
		CanonicalBrakingDeceleration);
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
	const float RawInputMagnitude = GetAnalogInputModifier();
	const bool bUsesStandingGroundProfile = UsesStandingGroundMovementProfile();
	const float PhysicalInputMagnitude = bUsesStandingGroundProfile
		? RpgCharacterMovementRuntime::ResolvePhysicalInputMagnitude(
			RawInputMagnitude,
			MovementProfile)
		: RawInputMagnitude;
	if (bUsesStandingGroundProfile)
	{
		// Canonicalize transitions that enter standing physics after acceleration was
		// prepared in another movement mode during this same CharacterMovement tick.
		AnalogInputModifier = PhysicalInputMagnitude;
	}
	bHasMovementInput = FMath::IsFinite(PhysicalInputMagnitude) &&
		PhysicalInputMagnitude > UE_KINDA_SMALL_NUMBER;
	float GaitInputMagnitude = PhysicalInputMagnitude;
	if (MovementProfile.bOverrideCharacterMovement)
	{
		const float AccelerationMagnitude = Acceleration.Size();
		float SourceMaxAcceleration = GetMaxAcceleration();
		if (FMath::IsFinite(RawInputMagnitude) &&
			RawInputMagnitude > UE_SMALL_NUMBER &&
			AccelerationMagnitude > UE_SMALL_NUMBER)
		{
			// Crouch and movement-mode transitions can change GetMaxAcceleration after
			// this frame's acceleration was scaled. Recover the source cap from the paired
			// analog snapshot so gait classification keeps the move's original magnitude.
			SourceMaxAcceleration = AccelerationMagnitude / RawInputMagnitude;
		}
		GaitInputMagnitude = SourceMaxAcceleration > UE_SMALL_NUMBER
			? FMath::Clamp(
				RoundAcceleration(Acceleration).Size() / SourceMaxAcceleration,
				0.0f,
				1.0f)
			: 0.0f;
	}
	DesiredGait = RpgCharacterMovementRuntime::ResolveDesiredGait(
		GaitInputMagnitude,
		MovementProfile.bOverrideCharacterMovement
			? DesiredGait
			: ERpgLocomotionGait::Idle,
		MovementProfile);
	bHasMoveIntent = RpgCharacterMovementRuntime::HasMoveIntent(
		GaitInputMagnitude,
		MovementProfile);
	GroundGait = RpgCharacterMovementRuntime::ResolveGroundGait(
		IsMovingOnGround(),
		Velocity.Size2D(),
		PhysicalInputMagnitude,
		DesiredGait,
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
		// ControlledCharacterMove or UpdateFromCompressedFlags resolves this move's physical
		// gait before FSavedMove captures MaxSpeed and before PerformMovement begins.
		return RpgCharacterMovementRuntime::ResolveGroundSpeedCap(
			DesiredGait == ERpgLocomotionGait::Run
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
		const bool bHasCurrentPhysicalInput =
			RpgCharacterMovementRuntime::ResolvePhysicalInputMagnitude(
				AnalogInputModifier,
				MovementProfile) > UE_KINDA_SMALL_NUMBER;
		return RpgCharacterMovementRuntime::ResolveGroundBrakingDeceleration(
			bHasCurrentPhysicalInput,
			MovementProfile);
	}

	return Super::GetMaxBrakingDeceleration();
}
