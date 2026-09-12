// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"


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
