// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "RpgCharacter.h"
#include "RpgHealthComponent.h"
#include "RpgMovementProfile.h"

/** Authorization, speeds, stance and facing remain local history; only the sprint request bit crosses the movement wire. */
class FSavedMove_RpgCharacter : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;
	virtual void Clear() override
	{
		Super::Clear();
		bBlocking = false;
		SpeedLimit = 0.f;
		ControlRotation = FRotator::ZeroRotator;
		bSprintAuthorized = false;
		SprintSpeed = 0.f;
		bEquipmentInHands = false;
		bFacingCommitted = false;
		FacingSteeringRate = 0.f;
	}

	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel,
		FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
		const URpgCharacterMovementComponent* Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		bBlocking = Movement->SampleBlockMovement(SpeedLimit);
		ControlRotation = Character->GetControlRotation();
		bSprintAuthorized = Movement->SampleSprintMovement(SprintSpeed);
		bEquipmentInHands = Movement->SampleEquipmentInHands();
		bFacingCommitted = Movement->SampleFacingCommitment(FacingSteeringRate);
	}

	virtual void PrepMoveFor(ACharacter* Character) override
	{
		Super::PrepMoveFor(Character);
		URpgCharacterMovementComponent* Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		Movement->bBlockMovementForMove = bBlocking;
		Movement->BlockMovementSpeedLimitForMove = SpeedLimit;
		Movement->ControlRotationForMove = ControlRotation;
		Movement->bSprintAuthorizedForMove = bSprintAuthorized;
		Movement->SprintSpeedForMove = SprintSpeed;
		Movement->bWantsSprint = bSprintAuthorized;
		Movement->bEquipmentInHandsForMove = bEquipmentInHands;
		Movement->bFacingCommittedForMove = bFacingCommitted;
		Movement->FacingSteeringRateForMove = FacingSteeringRate;
	}

	virtual uint8 GetCompressedFlags() const override
	{
		return Super::GetCompressedFlags() | (bSprintAuthorized ? FLAG_Custom_0 : 0);
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
	{
		const FSavedMove_RpgCharacter* Other = static_cast<const FSavedMove_RpgCharacter*>(NewMove.Get());
		return bBlocking == Other->bBlocking && SpeedLimit == Other->SpeedLimit
			&& bSprintAuthorized == Other->bSprintAuthorized && SprintSpeed == Other->SprintSpeed
			&& bEquipmentInHands == Other->bEquipmentInHands && bFacingCommitted == Other->bFacingCommitted
			&& FacingSteeringRate == Other->FacingSteeringRate && Super::CanCombineWith(NewMove, Character, MaxDelta);
	}

private:
	bool bBlocking = false;
	float SpeedLimit = 0.f;
	FRotator ControlRotation = FRotator::ZeroRotator;
	bool bSprintAuthorized = false;
	float SprintSpeed = 0.f;
	bool bEquipmentInHands = false;
	bool bFacingCommitted = false;
	float FacingSteeringRate = 0.f;
};

class FNetworkPredictionData_Client_RpgCharacter : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_RpgCharacter(const UCharacterMovementComponent& Movement)
		: FNetworkPredictionData_Client_Character(Movement) {}
	virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FSavedMove_RpgCharacter()); }
};


UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_MovementStopped, "Gameplay.MovementStopped");

namespace RpgCharacter
{
	static float GroundTraceDistance = 100000.0f;
	FAutoConsoleVariableRef CVar_GroundTraceDistance(TEXT("RpgCharacter.GroundTraceDistance"), GroundTraceDistance, TEXT("Distance to trace down when generating ground information."), ECVF_Cheat);
}

URpgCharacterMovementComponent::URpgCharacterMovementComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Character owns movement replication; this component adds simulated-proxy collision and effective-gait read models.
	SetIsReplicatedByDefault(true);
}

const URpgMovementProfile& URpgCharacterMovementComponent::GetProfileOrDefault() const
{
	return MovementProfile ? *MovementProfile : *GetDefault<URpgMovementProfile>();
}

float URpgCharacterMovementComponent::GetSprintSpeed() const
{
	return GetProfileOrDefault().SprintSpeed;
}

void URpgCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	ApplyMovementProfile();
}

void URpgCharacterMovementComponent::ApplyMovementProfile()
{
	// Static values only; values that depend on the move come from the overrides below, never from per-tick writes.
	if (!MovementProfile) { return; }
	MaxWalkSpeed = MovementProfile->RunSpeed;
	MaxWalkSpeedCrouched = MovementProfile->CrouchSpeed;
	MaxAcceleration = MovementProfile->Acceleration;
	BrakingDecelerationWalking = MovementProfile->BrakingDeceleration;
	bUseSeparateBrakingFriction = false;
	BrakingFrictionFactor = MovementProfile->BrakingFrictionFactor;
	// The Pose Search trajectory reads this field directly; it holds the run value, the sprint taper applies per move.
	GroundFriction = MovementProfile->GroundFriction;
	AirControl = MovementProfile->AirControl;
	BrakingDecelerationFalling = MovementProfile->FallingBrakingDeceleration;
	JumpZVelocity = MovementProfile->JumpZVelocity;
}

float URpgCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (MovementProfile && IsMovingOnGround())
	{
		return Acceleration.IsZero() ? MovementProfile->BrakingDeceleration : MovementProfile->BrakingDecelerationWithInput;
	}
	return Super::GetMaxBrakingDeceleration();
}

void URpgCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (!MovementProfile || bFluid || !IsMovingOnGround() || MovementProfile->Acceleration <= 0.f)
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}
	// Only the move's own velocity selects the taper, so the owner, saved-move replay and the server agree. Scaling
	// the input here, as the engine scales falling input, keeps GetMaxAcceleration and the analog strength unchanged.
	const float Speed = static_cast<float>(Velocity.Size2D());
	const float Scale = MovementProfile->GetAccelerationAtSpeed(Speed) / MovementProfile->Acceleration;
	TGuardValue<FVector> AccelerationScope(Acceleration, Acceleration * Scale);
	TGuardValue<float> ScaleScope(CalcVelocityAccelerationScale, Scale);
	Super::CalcVelocity(DeltaTime, MovementProfile->GetGroundFrictionAtSpeed(Speed), bFluid, BrakingDeceleration);
}

bool URpgCharacterMovementComponent::CanAttemptJump() const
{
	// Same as UCharacterMovementComponent's implementation but without the crouch check
	return IsJumpAllowed() &&
		(IsMovingOnGround() || IsFalling()); // Falling included for double-jump and non-zero jump hold time, but validated by character.
}

FNetworkPredictionData_Client* URpgCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		URpgCharacterMovementComponent* MutableThis = const_cast<URpgCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_RpgCharacter(*this);
	}
	return ClientPredictionData;
}

bool URpgCharacterMovementComponent::SampleBlockMovement(float& OutSpeedLimit) const
{
	OutSpeedLimit = 0.f;
	const URpgAbilitySystemComponent* AbilitySystem = Cast<URpgAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	if (!AbilitySystem || AbilitySystem->GetAvatarActor() != GetOwner() || !AbilitySystem->IsBlockMovementActive()) { return false; }
	OutSpeedLimit = AbilitySystem->GetBlockMovementSpeedLimit();
	return true;
}

bool URpgCharacterMovementComponent::GetBlockMovementForMove(float& OutSpeedLimit) const
{
	if (bInBlockMovementScope)
	{
		OutSpeedLimit = BlockMovementSpeedLimitForMove;
		return bBlockMovementForMove;
	}
	return SampleBlockMovement(OutSpeedLimit);
}

bool URpgCharacterMovementComponent::SampleEquipmentInHands() const
{
	const ARpgCharacter* Character = Cast<ARpgCharacter>(CharacterOwner);
	const URpgEquipmentManagerComponent* Equipment = Character ? Character->GetEquipmentManagerComponent() : nullptr;
	return Equipment && Equipment->HasEquipmentInHands();
}

bool URpgCharacterMovementComponent::SampleFacingCommitment(float& OutSteeringRate) const
{
	OutSteeringRate = 0.f;
	const URpgAbilitySystemComponent* AbilitySystem = Cast<URpgAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	return AbilitySystem && AbilitySystem->GetAvatarActor() == GetOwner() && AbilitySystem->GetFacingCommitment(OutSteeringRate);
}

bool URpgCharacterMovementComponent::IsCombatStanceForMove() const
{
	float BlockLimit = 0.f;
	return (bInBlockMovementScope ? bEquipmentInHandsForMove : SampleEquipmentInHands()) || GetBlockMovementForMove(BlockLimit);
}

bool URpgCharacterMovementComponent::GetFacingCommitmentForMove(float& OutSteeringRate) const
{
	if (bInBlockMovementScope)
	{
		OutSteeringRate = FacingSteeringRateForMove;
		return bFacingCommittedForMove;
	}
	return SampleFacingCommitment(OutSteeringRate);
}

FRotator URpgCharacterMovementComponent::GetControlRotationForMove() const
{
	// Stock PostUpdate_Replay overwrites SavedControlRotation from today's controller. PrepMoveFor
	// installs our immutable original instead, including when the same move is corrected repeatedly.
	if (!CharacterOwner) { return FRotator::ZeroRotator; }
	return CharacterOwner->bClientUpdating ? ControlRotationForMove : CharacterOwner->GetControlRotation();
}

bool URpgCharacterMovementComponent::IsSprintDirectionAllowedForMove() const
{
	// The stance faces the camera, so the move's control yaw is the facing on owner, replay and server alike.
	const FVector Direction = Acceleration.GetSafeNormal2D();
	if (Direction.IsNearlyZero() || !FMath::IsFinite(CombatStanceSprintAngle)) { return false; }
	const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(GetControlRotationForMove().Yaw, Direction.Rotation().Yaw));
	return Delta <= FMath::Clamp(CombatStanceSprintAngle, 0.f, 180.f);
}

void URpgCharacterMovementComponent::RefreshRotationModeFlags()
{
	if (!bUseRotationModes) { return; }
	// Without these, the trajectory predicts an orient-to-movement turn while the stance strafes, and Motion
	// Matching selects reface starts instead of strafe and backpedal clips.
	const bool bCombatStance = IsInCombatStance();
	bOrientRotationToMovement = !bCombatStance;
	bUseControllerDesiredRotation = bCombatStance;
}

void URpgCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Simulated proxies and idle frames refresh here; moves refresh again from their own recorded stance.
	RefreshRotationModeFlags();
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool URpgCharacterMovementComponent::IsInCombatStance() const
{
	// Simulated proxies sample the replicated equipment list and, through the block read model, the replicated blocking tag.
	return bUseRotationModes && CharacterOwner && IsCombatStanceForMove();
}

bool URpgCharacterMovementComponent::SampleSprintMovement(float& OutSprintSpeed) const
{
	OutSprintSpeed = 0.f;
	const URpgAbilitySystemComponent* AbilitySystem = Cast<URpgAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	if (!bEnableGASSprint || !AbilitySystem || AbilitySystem->GetAvatarActor() != GetOwner()
		|| !AbilitySystem->IsSprintMovementActive()) { return false; }
	OutSprintSpeed = AbilitySystem->GetSprintMovementSpeed();
	return FMath::IsFinite(OutSprintSpeed) && OutSprintSpeed > 0.f;
}

bool URpgCharacterMovementComponent::CanSprintForMove(float& OutSprintSpeed) const
{
	OutSprintSpeed = 0.f;
	if (!bEnableGASSprint || !CharacterOwner || CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy
		|| !IsMovingOnGround() || IsCrouching() || bWantsToCrouch || Acceleration.SizeSquared2D() <= UE_SMALL_NUMBER
		|| IsMantleControllingRotation() || HasAnimRootMotion() || CurrentRootMotion.HasActiveRootMotionSources()) { return false; }
	const float MaximumAcceleration = GetMaxAcceleration();
	if (!FMath::IsFinite(MinimumSprintInput) || MinimumSprintInput < 0.f || MinimumSprintInput > 1.f
		|| !FMath::IsFinite(MaximumAcceleration) || MaximumAcceleration <= 0.f || CalcVelocityAccelerationScale <= 0.f) { return false; }
	// MoveAutonomous restores the original constrained acceleration before replay, while the server
	// constrains the received acceleration itself. Never derive analog strength from today's input device.
	const float InputStrength = static_cast<float>(Acceleration.Size2D() / (MaximumAcceleration * CalcVelocityAccelerationScale));
	if (!FMath::IsFinite(InputStrength) || InputStrength < MinimumSprintInput) { return false; }
	float BlockLimit = 0.f;
	if (GetBlockMovementForMove(BlockLimit)) { return false; }
	if (bUseRotationModes && IsCombatStanceForMove() && !IsSprintDirectionAllowedForMove()) { return false; }
	const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(CharacterOwner);
	if (Health && Health->IsDeadOrDying()) { return false; }
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)) { return false; }
	}
	if (bInBlockMovementScope)
	{
		OutSprintSpeed = SprintSpeedForMove;
		return bWantsSprint && bSprintAuthorizedForMove && FMath::IsFinite(OutSprintSpeed) && OutSprintSpeed > 0.f;
	}
	// Local speed queries before movement may precede the first compressed command. A remote server pawn
	// still requires its received request; possession/replication never grants sprint from the bit alone.
	const bool bNeedsRemoteRequest = CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled()
		&& CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy;
	return (!bNeedsRemoteRequest || bWantsSprint) && SampleSprintMovement(OutSprintSpeed);
}

void URpgCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void URpgCharacterMovementComponent::PerformMovement(float DeltaTime)
{
	// PrepMoveFor installed the original local policy during correction. A server movement RPC instead
	// samples the validated authority activation; no client-supplied cap can bypass or prolong its lease.
	if (!CharacterOwner || !CharacterOwner->bClientUpdating)
	{
		bBlockMovementForMove = SampleBlockMovement(BlockMovementSpeedLimitForMove);
		bSprintAuthorizedForMove = SampleSprintMovement(SprintSpeedForMove);
		bEquipmentInHandsForMove = SampleEquipmentInHands();
		bFacingCommittedForMove = SampleFacingCommitment(FacingSteeringRateForMove);
		const bool bUsesRemoteRequest = CharacterOwner && CharacterOwner->HasAuthority()
			&& !CharacterOwner->IsLocallyControlled() && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy;
		if (!bUsesRemoteRequest) { bWantsSprint = bSprintAuthorizedForMove; }
	}
	TGuardValue<bool> BlockScope(bInBlockMovementScope, true);
	bRootMotionOwnedSprintMove = false;
	RefreshRotationModeFlags();
	Super::PerformMovement(DeltaTime);
}

void URpgCharacterMovementComponent::UpdateVelocityBeforeMovement(float DeltaSeconds)
{
	Super::UpdateVelocityBeforeMovement(DeltaSeconds);
	bRootMotionOwnedSprintMove = HasAnimRootMotion() || CurrentRootMotion.HasActiveRootMotionSources();
}

void URpgCharacterMovementComponent::SetIsSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting) { return; }
	bIsSprinting = bNewSprinting;
	if (CharacterOwner && CharacterOwner->HasAuthority()) { CharacterOwner->ForceNetUpdate(); }
}

void URpgCharacterMovementComponent::RefreshSprintState()
{
	float AuthorizedSpeed = 0.f;
	if (!SampleSprintMovement(AuthorizedSpeed)) { SetIsSprinting(false); }
}

bool URpgCharacterMovementComponent::IsSprinting() const
{
	if (!bIsSprinting || !CharacterOwner) { return false; }
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy || bInBlockMovementScope) { return true; }
	float EffectiveSprintSpeed = 0.f;
	// A replay may finish with an older sprinting move after today's lease has already ended.
	// Keep that historical result available inside its movement delegate, never as today's local gait.
	return CanSprintForMove(EffectiveSprintSpeed);
}

void URpgCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		float EffectiveSprintSpeed = 0.f;
		SetIsSprinting(!bRootMotionOwnedSprintMove && CanSprintForMove(EffectiveSprintSpeed)
			&& Velocity.SizeSquared2D() > UE_SMALL_NUMBER);
	}
}

void URpgCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy && !IsMovingOnGround()) { SetIsSprinting(false); }
}

bool URpgCharacterMovementComponent::IsBlockControllingRotation() const
{
	float SpeedLimit = 0.f;
	return CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy && MovementMode != MOVE_None &&
		!IsMantleControllingRotation() && !HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources() &&
		GetBlockMovementForMove(SpeedLimit);
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

void URpgCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	// Pawn::FaceRotation is gated separately: controller updates and saved-move replay can call it outside CMC physics.
	// Reuse the ability's existing lease so cancellation, rejection, death and replication release both policies together.
	if (IsMantleControllingRotation()) { return; }
	if (bUseRotationModes && CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Movement is the single yaw writer: an ability's facing commitment first, then the stance or free movement.
		float SteeringRate = 0.f;
		const bool bFacingCommitted = GetFacingCommitmentForMove(SteeringRate);
		if (bFacingCommitted && SteeringRate <= 0.f) { return; }
		const bool bCombatStance = IsCombatStanceForMove();
		const URpgMovementProfile& Profile = GetProfileOrDefault();
		float YawRate = 0.f; // The combat stance turns at once on the ground.
		float SprintSpeedForRotation = 0.f;
		if (bFacingCommitted) { YawRate = SteeringRate; }
		else if (IsFalling()) { YawRate = Profile.AirRotationRate; }
		else if (!bCombatStance) { YawRate = CanSprintForMove(SprintSpeedForRotation) ? Profile.SprintRotationRate : Profile.GroundRotationRate; }
		const bool bSavedOrientToMovement = bOrientRotationToMovement;
		const bool bSavedControllerDesired = bUseControllerDesiredRotation;
		bOrientRotationToMovement = true;
		bUseControllerDesiredRotation = false;
		{
			// A negative engine rate turns at once; ComputeOrientToMovementRotation supplies the camera target.
			TGuardValue<FRotator> RateScope(RotationRate, FRotator(0.f, FMath::IsFinite(YawRate) && YawRate > 0.f ? YawRate : -1.f, 0.f));
			TGuardValue<bool> TargetScope(bFaceControlRotationInPhysics, bFacingCommitted || bCombatStance);
			Super::PhysicsRotation(DeltaTime);
		}
		bOrientRotationToMovement = bSavedOrientToMovement;
		bUseControllerDesiredRotation = bSavedControllerDesired;
		return;
	}
	if (IsBlockControllingRotation())
	{
		const bool bSavedOrientToMovement = bOrientRotationToMovement;
		const bool bSavedControllerDesired = bUseControllerDesiredRotation;
		bOrientRotationToMovement = true;
		bUseControllerDesiredRotation = false;
		Super::PhysicsRotation(DeltaTime);
		bOrientRotationToMovement = bSavedOrientToMovement;
		bUseControllerDesiredRotation = bSavedControllerDesired;
	}
	else { Super::PhysicsRotation(DeltaTime); }
}

FRotator URpgCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation,
	float DeltaTime, FRotator& DeltaRotation) const
{
	if (bFaceControlRotationInPhysics || IsBlockControllingRotation())
	{
		return GetControlRotationForMove();
	}
	return Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);
}

FRotator URpgCharacterMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped))
		{
			return FRotator(0,0,0);
		}
	}

	return Super::GetDeltaRotation(DeltaTime);
}

float URpgCharacterMovementComponent::GetMaxSpeed() const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped))
		{
			return 0;
		}
	}

	float SpeedLimit = 0.f;
	float EffectiveSprintSpeed = 0.f;
	const float AuthoredSpeed = CanSprintForMove(EffectiveSprintSpeed) ? EffectiveSprintSpeed : Super::GetMaxSpeed();
	return GetBlockMovementForMove(SpeedLimit) && FMath::IsFinite(SpeedLimit) && SpeedLimit > 0.f
		? FMath::Min(AuthoredSpeed, SpeedLimit) : AuthoredSpeed;
}

void URpgCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URpgCharacterMovementComponent, MantleCollisionComponent, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(URpgCharacterMovementComponent, bIsSprinting, COND_SimulatedOnly);
}

bool URpgCharacterMovementComponent::BeginMantleCollisionIgnore(UPrimitiveComponent* Component)
{
	if (!CharacterOwner || (!CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled())
		|| !IsValid(Component) || MantleCollisionComponent) return false;
	MantleCollisionComponent = Component;
	OnRep_MantleCollisionComponent();
	return true;
}

void URpgCharacterMovementComponent::EndMantleCollisionIgnore(UPrimitiveComponent* ExpectedComponent)
{
	// A destroyed obstacle disappears from the ability's weak reference before this replicated pointer is collected.
	// Permit that stale lease to clear, but never release a valid replacement belonging to another activation.
	if (MantleCollisionComponent != ExpectedComponent && (ExpectedComponent || IsValid(MantleCollisionComponent))) return;
	MantleCollisionComponent = nullptr;
	OnRep_MantleCollisionComponent();
}

void URpgCharacterMovementComponent::OnRep_MantleCollisionComponent()
{
	UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (Capsule && bAddedMantleCollisionIgnore && AppliedMantleCollisionComponent.IsValid())
	{
		Capsule->IgnoreComponentWhenMoving(AppliedMantleCollisionComponent.Get(), false);
	}
	AppliedMantleCollisionComponent = MantleCollisionComponent;
	bAddedMantleCollisionIgnore = Capsule && MantleCollisionComponent
		&& !Capsule->GetMoveIgnoreComponents().Contains(MantleCollisionComponent);
	if (bAddedMantleCollisionIgnore) Capsule->IgnoreComponentWhenMoving(MantleCollisionComponent, true);
}

void URpgCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MantleCollisionComponent = nullptr;
	bIsSprinting = false;
	bWantsSprint = false;
	bSprintAuthorizedForMove = false;
	SprintSpeedForMove = 0.f;
	OnRep_MantleCollisionComponent();
	Super::EndPlay(EndPlayReason);
}
