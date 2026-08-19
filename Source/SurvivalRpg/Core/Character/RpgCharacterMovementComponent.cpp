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
	// location which may be many unacknowledged moves ahead under latency. Classify the
	// adjustment before replay, then fold historical responses into one tick batch.
	if (UpdatedComponent)
	{
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
	const bool bShouldMarkAnimationDiscontinuity =
		bPendingAnimationCorrectionDiscontinuity;
	const bool bReplayedMoves = Super::ClientUpdatePositionAfterServerUpdate();

	if (bHadPendingAnimationCorrection)
	{
		bHasPendingAnimationCorrection = false;
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

	return Super::GetMaxSpeed();
}
