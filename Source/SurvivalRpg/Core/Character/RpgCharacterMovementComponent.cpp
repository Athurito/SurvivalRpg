// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Character.h"
#include "RpgCharacter.h"


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

void URpgCharacterMovementComponent::MoveAutonomous(
	float ClientTimeStamp,
	float DeltaTime,
	uint8 CompressedFlags,
	const FVector& NewAccel)
{
	TGuardValue<bool> SavedMoveScope(bResolvingSavedMove, true);
	TGuardValue<bool> RunRequestScope(
		bCurrentSavedMoveRunGait,
		(CompressedFlags & FSavedMove_Character::FLAG_Custom_0) != 0);
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

bool URpgCharacterMovementComponent::ForcePositionUpdate(float DeltaTime)
{
	if (CharacterOwner && CharacterOwner->HasAuthority() &&
		CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
	{
		// UE extrapolates the previous acceleration directly through PerformMovement here,
		// bypassing MoveAutonomous. Preserve its already validated gait, never infer a new request.
		TGuardValue<bool> SavedMoveScope(bResolvingSavedMove, true);
		TGuardValue<bool> RunRequestScope(
			bCurrentSavedMoveRunGait, DesiredGait == ERpgLocomotionGait::Run);
		return Super::ForcePositionUpdate(DeltaTime);
	}
	return Super::ForcePositionUpdate(DeltaTime);
}

float URpgCharacterMovementComponent::GetCurrentOwnerMoveTimeStamp() const
{
	if (!CharacterOwner || CharacterOwner->GetLocalRole() != ROLE_AutonomousProxy ||
		!CharacterOwner->IsLocallyControlled())
	{
		return -1.0f;
	}
	const FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	return ClientData && FMath::IsFinite(ClientData->CurrentTimeStamp)
		? ClientData->CurrentTimeStamp
		: -1.0f;
}

void URpgCharacterMovementComponent::RequestOwnerRotationSynchronization(float OwnerAppliedMoveTimeStamp)
{
	if (CharacterOwner &&
		FMath::IsFinite(OwnerAppliedMoveTimeStamp) && OwnerAppliedMoveTimeStamp >= 0.0f)
	{
		OwnerRotationSynchronizationTimeStamp = OwnerAppliedMoveTimeStamp;
	}
}

void URpgCharacterMovementComponent::CancelOwnerRotationSynchronization()
{
	OwnerRotationSynchronizationTimeStamp = -1.0f;
}

bool URpgCharacterMovementComponent::IsOwnerRotationSynchronizationCorrection(float TimeStamp) const
{
	const float Delta = TimeStamp - OwnerRotationSynchronizationTimeStamp;
	const float HalfResetPeriod = MinTimeBetweenTimeStampResets * 0.5f;
	return OwnerRotationSynchronizationTimeStamp >= 0.0f && FMath::IsFinite(TimeStamp) &&
		TimeStamp >= 0.0f &&
		((Delta > 0.0f && Delta <= HalfResetPeriod) || Delta < -HalfResetPeriod);
}

bool URpgCharacterMovementComponent::ShouldCorrectRotation() const
{
	return bOrientRotationToMovement || Super::ShouldCorrectRotation();
}

void URpgCharacterMovementComponent::ServerMoveHandleClientError(
	float ClientTimeStamp,
	float DeltaTime,
	const FVector& Accel,
	const FVector& RelativeClientLocation,
	FMovementBaseInterfaceData* ClientMovementBaseInterfaceData,
	FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	if (bOrientRotationToMovement && IsOwnerRotationSynchronizationCorrection(ClientTimeStamp))
	{
		// Keep retrying through normal CMC response throttling until the owner confirms receipt.
		// Move responses are unreliable; consuming the request here would lose yaw-only corrections.
		// ForceClientAdjustment only clears the send timer. Rotation-only divergence also
		// needs an explicit correction request because UE's position error check ignores yaw.
		GetPredictionData_Server_Character()->bForceClientUpdate = true;
		ForceClientAdjustment();
	}
	Super::ServerMoveHandleClientError(
		ClientTimeStamp, DeltaTime, Accel, RelativeClientLocation,
		ClientMovementBaseInterfaceData, ClientBaseBoneName, ClientMovementMode);
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
		DesiredGait = bResolvingSavedMove
			? RpgCharacterMovementRuntime::ResolveSavedMoveDesiredGait(
				InputMagnitude, bCurrentSavedMoveRunGait, GetMaxAcceleration(), MovementProfile)
			: RpgCharacterMovementRuntime::ResolveDesiredGait(
				InputMagnitude, DesiredGait, MovementProfile);
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

void URpgCharacterMovementComponent::OnMovementModeChanged(
	EMovementMode PreviousMovementMode,
	uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (!UsesStandingGroundMovementProfile())
	{
		ClearGroundGaitState();
	}
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
	if (MovementProfile.bOverrideCharacterMovement)
	{
		UpdatePredictedGaitFromInput(GaitInputMagnitude);
	}
	else
	{
		DesiredGait = RpgCharacterMovementRuntime::ResolveDesiredGait(
			GaitInputMagnitude, ERpgLocomotionGait::Idle, MovementProfile);
	}
	bHasMoveIntent = RpgCharacterMovementRuntime::HasMoveIntent(
		GaitInputMagnitude,
		MovementProfile);
	const bool bUseAuthoritativeGait = bUsesStandingGroundProfile && CharacterOwner &&
		CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy &&
		(ReplicatedGroundMovementGait == ERpgLocomotionGait::Walk ||
		 ReplicatedGroundMovementGait == ERpgLocomotionGait::Run);
	if (bUseAuthoritativeGait && bHasMoveIntent)
	{
		DesiredGait = ReplicatedGroundMovementGait;
	}
	GroundGait = RpgCharacterMovementRuntime::ResolveGroundGait(
		IsMovingOnGround(),
		Velocity.Size2D(),
		PhysicalInputMagnitude,
		DesiredGait,
		GroundGait,
		bUseAuthoritativeGait
			? ReplicatedGroundMovementGait
			: ERpgLocomotionGait::Idle,
		MovementProfile);

	if (ARpgCharacter* RpgCharacter = Cast<ARpgCharacter>(CharacterOwner);
		RpgCharacter && RpgCharacter->HasAuthority())
	{
		const ERpgLocomotionGait AuthoritativeGait =
			MovementProfile.bOverrideCharacterMovement &&
			UsesStandingGroundMovementProfile() &&
			(GroundGait == ERpgLocomotionGait::Walk ||
			 GroundGait == ERpgLocomotionGait::Run)
				? GroundGait
				: ERpgLocomotionGait::Idle;
		RpgCharacter->SetAuthoritativeGroundMovementGait(AuthoritativeGait);
	}
}

void URpgCharacterMovementComponent::OnTeleported()
{
	Super::OnTeleported();
	MarkAnimationDiscontinuity();
}

void URpgCharacterMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	ClearGroundGaitState();
}

void URpgCharacterMovementComponent::ClearGroundGaitState()
{
	GroundGait = ERpgLocomotionGait::Idle;
	if (ARpgCharacter* RpgCharacter = Cast<ARpgCharacter>(CharacterOwner);
		RpgCharacter && RpgCharacter->HasAuthority())
	{
		RpgCharacter->SetAuthoritativeGroundMovementGait(
			ERpgLocomotionGait::Idle);
	}
}

void URpgCharacterMovementComponent::NotifyReplicatedAnimationTeleport()
{
	MarkAnimationDiscontinuity();
}

void URpgCharacterMovementComponent::NotifyReplicatedGroundMovementGait(
	ERpgLocomotionGait NewCoastGait)
{
	ReplicatedGroundMovementGait =
		NewCoastGait == ERpgLocomotionGait::Walk ||
		NewCoastGait == ERpgLocomotionGait::Run
			? NewCoastGait
			: ERpgLocomotionGait::Idle;
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
	const bool bLargeAcknowledgedAnimationCorrection =
		UpdatedComponent &&
		IsLargeAcknowledgedAnimationCorrection(
			ClientData.LastAckedMove.Get(),
			NewLocation,
			NewMovementBaseInterfaceData,
			NewBaseBoneName,
			bHasBase,
			bBaseRelativePosition);

#if WITH_DEV_AUTOMATION_TESTS
	++ClientCorrectionReceivedCountForTests;
	if (bLargeAcknowledgedAnimationCorrection)
	{
		++LargeClientCorrectionReceivedCountForTests;
		LastLargeClientCorrectionTimeStampForTests = TimeStamp;
	}
	LastClientCorrectionMovementBaseForTests.Clear();
	if (MovementBaseUtility::IsMovementBaseDataValid(
		NewMovementBaseInterfaceData))
	{
		LastClientCorrectionMovementBaseForTests =
			*NewMovementBaseInterfaceData;
	}
	LastClientCorrectionBaseBoneNameForTests = NewBaseBoneName;
	bLastClientCorrectionHadBaseForTests = bHasBase;
	bLastClientCorrectionWasBaseRelativeForTests = bBaseRelativePosition;
#endif

	// NewLocation is already converted to world space by ClientAdjustPosition. Compare
	// it with the acknowledged move from the same timestamp, not the client's current
	// location which may be many unacknowledged moves ahead under latency. Dynamic-base
	// corrections must compare the move and server location in their shared base frame;
	// historical world locations also contain legitimate base translation and rotation.
	if (UpdatedComponent)
	{
		if (!bHasPendingAnimationCorrection)
		{
			CapturePendingAnimationCorrectionStart();
		}
		bHasPendingAnimationCorrection = true;
		bPendingAnimationCorrectionDiscontinuity |=
			bLargeAcknowledgedAnimationCorrection;
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
	if (ARpgCharacter* RpgCharacter = Cast<ARpgCharacter>(CharacterOwner))
	{
		RpgCharacter->NotifyOwnerRotationCorrectionReceived(TimeStamp);
	}
}

bool URpgCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	const bool bHadPendingAnimationCorrection = bHasPendingAnimationCorrection;
	const bool bReplayedMoves = Super::ClientUpdatePositionAfterServerUpdate();

	if (bHadPendingAnimationCorrection)
	{
		const bool bLivePresentationDiscontinuity =
			IsLargePendingAnimationCorrection();
		const bool bShouldMarkAnimationDiscontinuity =
			bPendingAnimationCorrectionDiscontinuity ||
			bLivePresentationDiscontinuity;
		ResetPendingAnimationCorrectionState();
		if (bShouldMarkAnimationDiscontinuity)
		{
			MarkAnimationDiscontinuity();
		}
	}

	return bReplayedMoves;
}

#if WITH_DEV_AUTOMATION_TESTS
bool URpgCharacterMovementComponent::WasLastClientCorrectionBaseRelativeForTests(
	const FMovementBaseInterfaceData* ExpectedMovementBaseInterfaceData,
	FName ExpectedBaseBoneName) const
{
	return bLastClientCorrectionHadBaseForTests &&
		bLastClientCorrectionWasBaseRelativeForTests &&
		MovementBaseUtility::IsMovementBaseDataValid(
			ExpectedMovementBaseInterfaceData) &&
		MovementBaseUtility::IsMovementBaseDataValid(
			&LastClientCorrectionMovementBaseForTests) &&
		MovementBaseUtility::DoesMovementBaseDataMatch(
			ExpectedMovementBaseInterfaceData,
			&LastClientCorrectionMovementBaseForTests) &&
		LastClientCorrectionBaseBoneNameForTests == ExpectedBaseBoneName;
}
#endif

void URpgCharacterMovementComponent::CapturePendingAnimationCorrectionStart()
{
	PendingAnimationCorrectionStartLocation = UpdatedComponent
		? UpdatedComponent->GetComponentLocation()
		: FVector::ZeroVector;
	PendingAnimationCorrectionStartRelativeLocation = FVector::ZeroVector;
	PendingAnimationCorrectionStartMovementBase.Clear();
	PendingAnimationCorrectionStartBaseBoneName = NAME_None;
	bHasPendingAnimationCorrectionStartRelativeLocation = false;

	if (CharacterOwner)
	{
		const FBasedMovementInfo& BasedMovement =
			CharacterOwner->GetBasedMovement();
		if (BasedMovement.HasRelativeLocation() &&
			MovementBaseUtility::IsMovementBaseDataValid(
				&BasedMovement.MovementBaseInterfaceData))
		{
			// Use UE's cached relative location. A correction RPC may arrive before
			// the capsule has followed a base that already ticked this frame.
			PendingAnimationCorrectionStartRelativeLocation = BasedMovement.Location;
			PendingAnimationCorrectionStartMovementBase =
				BasedMovement.MovementBaseInterfaceData;
			PendingAnimationCorrectionStartBaseBoneName = BasedMovement.BoneName;
			bHasPendingAnimationCorrectionStartRelativeLocation = true;
		}
	}
}

bool URpgCharacterMovementComponent::IsLargeAcknowledgedAnimationCorrection(
	const FSavedMove_Character* AcknowledgedMove,
	const FVector& NewLocation,
	const FMovementBaseInterfaceData* NewMovementBaseInterfaceData,
	FName NewBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition) const
{
	FVector ClientLocation = AcknowledgedMove
		? AcknowledgedMove->SavedLocation
		: UpdatedComponent
			? UpdatedComponent->GetComponentLocation()
			: NewLocation;
	FVector ServerLocation = NewLocation;

	const bool bCanCompareRelative =
		AcknowledgedMove &&
		bHasBase &&
		bBaseRelativePosition &&
		MovementBaseUtility::IsMovementBaseDataValid(
			NewMovementBaseInterfaceData) &&
		MovementBaseUtility::UseRelativeLocation(
			NewMovementBaseInterfaceData) &&
		MovementBaseUtility::UseRelativeLocation(
			&AcknowledgedMove->EndMovementBaseInterfaceData) &&
		MovementBaseUtility::DoesMovementBaseDataMatch(
			&AcknowledgedMove->EndMovementBaseInterfaceData,
			NewMovementBaseInterfaceData) &&
		AcknowledgedMove->EndBoneName == NewBaseBoneName;
	if (bCanCompareRelative)
	{
		FVector ServerRelativeLocation = FVector::ZeroVector;
		if (MovementBaseUtility::TransformLocationToLocal(
			NewMovementBaseInterfaceData,
			NewBaseBoneName,
			NewLocation,
			ServerRelativeLocation))
		{
			ClientLocation = AcknowledgedMove->SavedRelativeLocation;
			ServerLocation = ServerRelativeLocation;
		}
	}

	return FVector::DistSquared(ClientLocation, ServerLocation) >
		FMath::Square(NetworkLargeClientCorrectionDistance);
}

bool URpgCharacterMovementComponent::IsLargePendingAnimationCorrection() const
{
	if (!UpdatedComponent)
	{
		return false;
	}

	FVector StartLocation = PendingAnimationCorrectionStartLocation;
	FVector EndLocation = UpdatedComponent->GetComponentLocation();
	const FBasedMovementInfo* CurrentBasedMovement = CharacterOwner
		? &CharacterOwner->GetBasedMovement()
		: nullptr;
	const bool bCanCompareRelative =
		bHasPendingAnimationCorrectionStartRelativeLocation &&
		CurrentBasedMovement &&
		CurrentBasedMovement->HasRelativeLocation() &&
		MovementBaseUtility::IsMovementBaseDataValid(
			&CurrentBasedMovement->MovementBaseInterfaceData) &&
		MovementBaseUtility::DoesMovementBaseDataMatch(
			&PendingAnimationCorrectionStartMovementBase,
			&CurrentBasedMovement->MovementBaseInterfaceData) &&
		PendingAnimationCorrectionStartBaseBoneName ==
			CurrentBasedMovement->BoneName;
	if (bCanCompareRelative)
	{
		StartLocation = PendingAnimationCorrectionStartRelativeLocation;
		EndLocation = CurrentBasedMovement->Location;
	}

	return FVector::DistSquared(StartLocation, EndLocation) >
		FMath::Square(NetworkLargeClientCorrectionDistance);
}

void URpgCharacterMovementComponent::ResetPendingAnimationCorrectionState()
{
	bHasPendingAnimationCorrection = false;
	PendingAnimationCorrectionStartLocation = FVector::ZeroVector;
	PendingAnimationCorrectionStartRelativeLocation = FVector::ZeroVector;
	PendingAnimationCorrectionStartMovementBase.Clear();
	PendingAnimationCorrectionStartBaseBoneName = NAME_None;
	bHasPendingAnimationCorrectionStartRelativeLocation = false;
	bPendingAnimationCorrectionDiscontinuity = false;
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
