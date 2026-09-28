// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

/** Local prediction history only: block/cap are not compressed client commands accepted by authority. */
class FSavedMove_RpgCharacter : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;
	virtual void Clear() override
	{
		Super::Clear();
		bBlocking = false;
		SpeedLimit = 0.f;
		BlockControlRotation = FRotator::ZeroRotator;
	}

	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel,
		FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
		const URpgCharacterMovementComponent* Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		bBlocking = Movement->SampleBlockMovement(SpeedLimit);
		BlockControlRotation = Character->GetControlRotation();
	}

	virtual void PrepMoveFor(ACharacter* Character) override
	{
		Super::PrepMoveFor(Character);
		URpgCharacterMovementComponent* Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		Movement->bBlockMovementForMove = bBlocking;
		Movement->BlockMovementSpeedLimitForMove = SpeedLimit;
		Movement->BlockControlRotationForMove = BlockControlRotation;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
	{
		const FSavedMove_RpgCharacter* Other = static_cast<const FSavedMove_RpgCharacter*>(NewMove.Get());
		return bBlocking == Other->bBlocking && SpeedLimit == Other->SpeedLimit && Super::CanCombineWith(NewMove, Character, MaxDelta);
	}

private:
	bool bBlocking = false;
	float SpeedLimit = 0.f;
	FRotator BlockControlRotation = FRotator::ZeroRotator;
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
	// Character still owns ordinary movement replication; this component adds only the simulated mantle collision lease.
	SetIsReplicatedByDefault(true);
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

void URpgCharacterMovementComponent::PerformMovement(float DeltaTime)
{
	// PrepMoveFor installed the original local policy during correction. A server movement RPC instead
	// samples the validated authority activation; no client-supplied cap can bypass or prolong its lease.
	if (!CharacterOwner || !CharacterOwner->bClientUpdating)
	{
		bBlockMovementForMove = SampleBlockMovement(BlockMovementSpeedLimitForMove);
	}
	TGuardValue<bool> BlockScope(bInBlockMovementScope, true);
	Super::PerformMovement(DeltaTime);
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
	if (IsBlockControllingRotation())
	{
		// Stock PostUpdate_Replay overwrites SavedControlRotation from today's controller. PrepMoveFor
		// installs our immutable original instead, including when the same move is corrected repeatedly.
		return CharacterOwner->bClientUpdating ? BlockControlRotationForMove : CharacterOwner->GetControlRotation();
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
	const float AuthoredSpeed = Super::GetMaxSpeed();
	return GetBlockMovementForMove(SpeedLimit) && FMath::IsFinite(SpeedLimit) && SpeedLimit > 0.f
		? FMath::Min(AuthoredSpeed, SpeedLimit) : AuthoredSpeed;
}

void URpgCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URpgCharacterMovementComponent, MantleCollisionComponent, COND_SimulatedOnly);
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
	OnRep_MantleCollisionComponent();
	Super::EndPlay(EndPlayReason);
}
