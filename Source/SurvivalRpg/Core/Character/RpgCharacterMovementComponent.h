// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	//~End of UMovementComponent interface
protected:
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

	// Cached ground info for the character.  Do not access this directly!  It's only updated when accessed via GetGroundInfo().
	FRpgCharacterGroundInfo CachedGroundInfo;

	/** Local-only edge consumed once by the animation game-thread snapshot. */
	uint32 AnimationDiscontinuitySerial = 0;

	/** Frame used to coalesce a replicated teleport and its accompanying hard movement correction. */
	uint64 LastAnimationDiscontinuityFrame = MAX_uint64;

	/** True until the pending server correction and its saved moves have been evaluated together. */
	bool bHasPendingAnimationCorrection = false;

	/** True when any correction in the pending batch exceeds UE's large-correction threshold. */
	bool bPendingAnimationCorrectionDiscontinuity = false;
};
