// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RpgCharacterMovementProfile.h"
#include "RpgCharacterMovementComponent.generated.h"

class FSavedMove_RpgCharacter;
class FNetworkPredictionData_Client_RpgCharacter;

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

	/** Returns the predicted input gait restored by SavedMove replay and consumed by presentation. */
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

#if WITH_DEV_AUTOMATION_TESTS
	/** Returns how many server corrections reached this local component during the current test lifetime. */
	uint32 GetClientCorrectionReceivedCountForTests() const
	{
		return ClientCorrectionReceivedCountForTests;
	}

	/** Returns whether the latest observed correction used the expected resolved relative base frame. */
	bool WasLastClientCorrectionBaseRelativeForTests(
		const FMovementBaseInterfaceData* ExpectedMovementBaseInterfaceData,
		FName ExpectedBaseBoneName) const;
#endif

	/** Applies the server's semantic teleport edge on a simulated proxy. */
	void NotifyReplicatedAnimationTeleport();

	/** Stores the authority's current Walk/Run coast classification for late join and relevancy return. */
	void NotifyReplicatedGroundCoastGait(ERpgLocomotionGait NewCoastGait);

	/** Returns the local simulated-proxy coast hint; Idle means no authority coast is active. */
	ERpgLocomotionGait GetReplicatedGroundCoastGait() const { return ReplicatedGroundCoastGait; }

	//~UMovementComponent interface
	virtual void StopMovementImmediately() override;
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
	virtual void ControlledCharacterMove(
		const FVector& InputVector,
		float DeltaSeconds) override;
	virtual FVector ScaleInputAcceleration(
		const FVector& InputAcceleration) const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void CalcVelocity(
		float DeltaTime,
		float Friction,
		bool bFluid,
		float BrakingDeceleration) override;
	virtual void OnMovementModeChanged(
		EMovementMode PreviousMovementMode,
		uint8 PreviousCustomMode) override;
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

	/** Captures the live pre-correction position in world and, when possible, movement-base space. */
	void CapturePendingAnimationCorrectionStart();

	/** Returns whether the acknowledged move differs materially from the server in a comparable space. */
	bool IsLargeAcknowledgedAnimationCorrection(
		const FSavedMove_Character* AcknowledgedMove,
		const FVector& NewLocation,
		const FMovementBaseInterfaceData* NewMovementBaseInterfaceData,
		FName NewBaseBoneName,
		bool bHasBase,
		bool bBaseRelativePosition) const;

	/** Returns whether correction replay caused a material presentation jump beyond movement-base motion. */
	bool IsLargePendingAnimationCorrection() const;

	/** Clears every transient value owned by the pending correction batch. */
	void ResetPendingAnimationCorrectionState();

	/** Rebuilds the value-only move-intent and gait snapshot after CharacterMovement updates. */
	void RefreshLocomotionSnapshot();

	/** Clears local ground presentation and the authority transport without discarding a received proxy hint. */
	void ClearGroundCoastState();

	/** Restores the physical Run latch encoded by a server move or client replay. */
	void RestorePredictedGaitFromSavedMove(bool bSavedRunGait);

	/** Resolves this exact input move's deadzone and prediction-owned gait before it is saved. */
	void UpdatePredictedGaitFromInput(float InputMagnitude);

	/** Returns whether this frame may consume the standing-only GASP physical response profile. */
	bool UsesStandingGroundMovementProfile() const;

	/** Returns whether GAS currently suppresses all character translation and movement-driven rotation. */
	bool HasMovementStoppedTag() const;

	/** Static PawnData profile copied locally on every role; it is never runtime-mutated by animation. */
	FRpgCharacterMovementProfile MovementProfile;

	/** Stable grounded gait reconstructed from CMC input, speed, and profile hysteresis. */
	ERpgLocomotionGait GroundGait = ERpgLocomotionGait::Idle;

	/** Prediction-owned input gait encoded in SavedMoves for Run hysteresis. */
	ERpgLocomotionGait DesiredGait = ERpgLocomotionGait::Idle;

	/** Current profile-thresholded input intent, including while airborne. */
	bool bHasMoveIntent = false;

	/** Current deadzone-resolved physical CMC input used by the GASP braking contract. */
	bool bHasMovementInput = false;

	/** Simulated-proxy-only semantic coast hint received from the authoritative character. */
	ERpgLocomotionGait ReplicatedGroundCoastGait = ERpgLocomotionGait::Idle;

	// Cached ground info for the character.  Do not access this directly!  It's only updated when accessed via GetGroundInfo().
	FRpgCharacterGroundInfo CachedGroundInfo;

	/** Local-only edge consumed once by the animation game-thread snapshot. */
	uint32 AnimationDiscontinuitySerial = 0;

	/** Frame used to coalesce a replicated teleport and its accompanying hard movement correction. */
	uint64 LastAnimationDiscontinuityFrame = MAX_uint64;

#if WITH_DEV_AUTOMATION_TESTS
	/** Editor-test observation only; it is neither replicated nor consumed by gameplay. */
	uint32 ClientCorrectionReceivedCountForTests = 0;

	/** Last correction's base identity, retained only so PIE tests can prove the RPC contract. */
	FMovementBaseInterfaceData LastClientCorrectionMovementBaseForTests;

	/** Last correction's base bone, paired with the editor-only base identity. */
	FName LastClientCorrectionBaseBoneNameForTests = NAME_None;

	/** True when the latest editor-observed correction declared a movement base. */
	bool bLastClientCorrectionHadBaseForTests = false;

	/** True when the latest editor-observed correction declared its position base-relative. */
	bool bLastClientCorrectionWasBaseRelativeForTests = false;
#endif

	/** True until the pending server correction and its saved moves have been evaluated together. */
	bool bHasPendingAnimationCorrection = false;

	/** Live autonomous-pawn location before the pending correction batch and saved-move replay. */
	FVector PendingAnimationCorrectionStartLocation = FVector::ZeroVector;

	/** Base-relative live location captured before replay when the client stands on a dynamic base. */
	FVector PendingAnimationCorrectionStartRelativeLocation = FVector::ZeroVector;

	/** Weak movement-base identity paired with the pre-replay relative location. */
	FMovementBaseInterfaceData PendingAnimationCorrectionStartMovementBase;

	/** Bone/socket frame paired with the pre-replay movement-base identity. */
	FName PendingAnimationCorrectionStartBaseBoneName = NAME_None;

	/** True when the pending live comparison can use the captured movement-base frame. */
	bool bHasPendingAnimationCorrectionStartRelativeLocation = false;

	/** True when any correction in the pending batch exceeds UE's large-correction threshold. */
	bool bPendingAnimationCorrectionDiscontinuity = false;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgCharacterMovementProfileTest;
	friend class FRpgCharacterMovementSavedMoveTest;
#endif
	friend class FSavedMove_RpgCharacter;
};

/** Client move that records the GASP pilot's prediction-owned Run latch. */
class SURVIVALRPG_API FSavedMove_RpgCharacter : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	virtual void Clear() override;
	virtual void SetMoveFor(
		ACharacter* Character,
		float InDeltaTime,
		const FVector& NewAcceleration,
		FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
	virtual bool CanCombineWith(
		const FSavedMovePtr& NewMove,
		ACharacter* Character,
		float MaxDelta) const override;
	virtual uint8 GetCompressedFlags() const override;

	/** Predicted physical Run state for this exact move; false represents Idle or Walk. */
	bool bSavedRunGait = false;
};

/** Allocates project SavedMoves for the RPG CharacterMovement prediction path. */
class SURVIVALRPG_API FNetworkPredictionData_Client_RpgCharacter
	: public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_RpgCharacter(
		const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
