// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RpgCharacterMovementProfile.h"
#include "RpgCharacterMovementComponent.generated.h"


/**
 * FRpgCharacterGroundInfo
 *
 *	Information about the ground under the character.  It only gets updated as needed.
 */
USTRUCT(BlueprintType)
struct FRpgCharacterGroundInfo
{
	GENERATED_BODY()

	FRpgCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};


UCLASS(Config = Game)
class SURVIVALRPG_API URpgCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	explicit URpgCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** Applies one validated PawnData movement profile to this role's predicted CMC state. */
	bool ApplyMovementProfile(const FRpgCharacterMovementProfile& Profile);

	/** Returns the immutable-after-PawnData movement profile copied into this component. */
	const FRpgCharacterMovementProfile& GetMovementProfile() const { return MovementProfile; }

	/** Returns the stable CMC-owned grounded gait consumed by animation presentation. */
	ERpgLocomotionGait GetGroundGait() const { return GroundGait; }

	/** Returns the latest stateless input gait snapshot used by presentation and landing intent. */
	ERpgLocomotionGait GetDesiredGait() const { return DesiredGait; }

	/** Returns current normalized move intent reconstructed by CharacterMovement on this role. */
	bool HasMoveIntent() const { return bHasMoveIntent; }
	
	virtual bool CanAttemptJump() const override;

	/** Returns cached ground data, updating its game-thread trace at most once per frame. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|CharacterMovement")
	const FRpgCharacterGroundInfo& GetGroundInfo();

	/**
	 * Returns the local monotonic signal for presentation-history discontinuities.
	 * It is not replicated; each network role advances it from the correction or teleport it actually applies.
	 */
	uint32 GetAnimationDiscontinuitySerial() const { return AnimationDiscontinuitySerial; }

	/** Applies the server's semantic teleport edge on a simulated proxy. */
	void NotifyReplicatedAnimationTeleport();

	//~UMovementComponent interface
	virtual void OnTeleported() override;
	//~End of UMovementComponent interface

	//~UMovementComponent interface
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMinAnalogSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	//~End of UMovementComponent interface
protected:
	virtual void CalcVelocity(
		float DeltaTime,
		float Friction,
		bool bFluid,
		float BrakingDeceleration) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementUpdated(
		float DeltaSeconds,
		const FVector& OldLocation,
		const FVector& OldVelocity) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	virtual void OnClientCorrectionReceived(
		FNetworkPredictionData_Client_Character& ClientData,
		float TimeStamp,
		FVector NewLocation,
		FVector NewVelocity,
		FMovementBaseInterfaceData* NewMovementBaseInterfaceData,
		FName NewBaseBoneName,
		bool bHasBase,
		bool bBaseRelativePosition,
		uint8 ServerMovementMode,
		FVector ServerGravityDirection) override;
	virtual void SmoothCorrection(
		const FVector& OldLocation,
		const FQuat& OldRotation,
		const FVector& NewLocation,
		const FQuat& NewRotation) override;

	/** Advances the local history-reset edge without adding another replicated movement contract. */
	void MarkAnimationDiscontinuity();

	/** Rebuilds the value-only move-intent and gait snapshot after CharacterMovement updates. */
	void RefreshLocomotionSnapshot();

	/** Returns whether this frame may consume the standing-only GASP physical response profile. */
	bool UsesStandingGroundMovementProfile() const;

	/** Returns whether GAS currently suppresses all character translation and movement-driven rotation. */
	bool HasMovementStoppedTag() const;

	/** Static PawnData profile copied locally on every role; it is never runtime-mutated by animation. */
	FRpgCharacterMovementProfile MovementProfile;

	/** Stable grounded gait reconstructed from CMC input, speed, and profile hysteresis. */
	ERpgLocomotionGait GroundGait = ERpgLocomotionGait::Idle;

	/** Latest stateless input gait snapshot used by presentation and airborne landing capture. */
	ERpgLocomotionGait DesiredGait = ERpgLocomotionGait::Idle;

	/** Current profile-thresholded input intent, including while airborne. */
	bool bHasMoveIntent = false;

	/** Current non-zero CMC input used by the GASP braking contract independently of gait deadzones. */
	bool bHasMovementInput = false;

	// Cached ground info for the character.  Do not access this directly!  It's only updated when accessed via GetGroundInfo().
	FRpgCharacterGroundInfo CachedGroundInfo;

	/** Local-only edge consumed once by the animation game-thread snapshot. */
	uint32 AnimationDiscontinuitySerial = 0;

	/** Frame used to coalesce a replicated teleport and its accompanying hard movement correction. */
	uint64 LastAnimationDiscontinuityFrame = MAX_uint64;

	/** True until the pending server correction and its saved moves have been evaluated together. */
	bool bHasPendingAnimationCorrection = false;

	/** Live autonomous-pawn location before the pending correction batch and saved-move replay. */
	FVector PendingAnimationCorrectionStartLocation = FVector::ZeroVector;

	/** True when any correction in the pending batch exceeds UE's large-correction threshold. */
	bool bPendingAnimationCorrectionDiscontinuity = false;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgCharacterMovementProfileTest;
#endif
};
