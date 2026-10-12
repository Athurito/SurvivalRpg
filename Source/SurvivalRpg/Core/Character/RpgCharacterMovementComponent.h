// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RpgCharacterMovementComponent.generated.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace RpgCmcSprintTests { class FFixture; }
namespace RpgCmcRotationModeTests { class FFixture; }
#endif

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

	/** Camera-facing block locomotion yields to traversal and root-motion rotation; never changes the authored rotation flags. */
	bool IsBlockControllingRotation() const;
	/** Game-thread presentation snapshot: effective saved policy inside movement/replay, otherwise the current valid GAS lease. */
	bool GetBlockMovementForMove(float& OutSpeedLimit) const;

	/** Designer opt-in for GAS-authorized sprint. Existing characters keep their ordinary MaxWalkSpeed when disabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Sprint")
	bool bEnableGASSprint = false;

	/** Designer sprint speed in cm/s, copied into each GAS activation; MaxWalkSpeed remains the normal running speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Sprint", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintSpeed = 585.f;

	/** Minimum normalized movement input (0..1) for effective sprint and stamina cost. Partial input retains the held GAS lease. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Sprint", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumSprintInput = 0.7f;

	/** Effective last performed gait, available before OnCharacterMovementUpdated; simulated proxies receive authority state. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Sprint")
	bool IsSprinting() const;

	/** Retires the presentation state when GAS authorization ends, including when movement has stopped ticking. Does not alter saved moves. */
	void RefreshSprintState();

	/**
	 * Designer opt-in for the GASP rotation modes; disabled characters keep their authored rotation flags.
	 * When enabled, movement owns yaw and ignores the pawn's controller-yaw and the orient/desired-rotation flags:
	 * free movement turns toward the movement input, and the combat stance (equipment in hands or an active block)
	 * faces the camera. An ability's facing commitment and mantle override both.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Rotation")
	bool bUseRotationModes = false;

	/** Designer yaw rate in deg/s on the ground for both rotation modes. Zero or less turns at once, as GASP does. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Rotation", meta = (EditCondition = "bUseRotationModes", Units = "DegreesPerSecond"))
	float GroundRotationRate = 0.f;

	/** Designer yaw rate in deg/s while falling. Zero or less turns at once; GASP uses 200. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Rotation", meta = (EditCondition = "bUseRotationModes", Units = "DegreesPerSecond"))
	float AirRotationRate = 200.f;

	/**
	 * Designer limit in degrees between movement input and the camera yaw within which the combat stance may sprint.
	 * Larger angles, such as backward sprinting, keep the run speed and cost no stamina. GASP uses 50.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Rotation", meta = (EditCondition = "bUseRotationModes", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float CombatStanceSprintAngle = 50.f;

	/**
	 * Presentation read model for the AnimBP, false without rotation modes: true while the combat stance faces the
	 * camera. Owners and authority read their effective move state; simulated proxies derive it from the replicated
	 * equipment list and blocking tag, so late joiners see the same stance.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Rotation")
	bool IsInCombatStance() const;

	/** Mirrors the rotation mode into the stock rotation flags before movement, on every role. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Captures activation-scoped block policy in local saved moves; authority resolves its own GAS lease. */
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void PerformMovement(float DeltaTime) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	/** Acquires one validated mantle obstacle ignore; server state replicates to simulated proxies only. */
	bool BeginMantleCollisionIgnore(UPrimitiveComponent* Component);

	/** Releases only the matching mantle obstacle; existing unrelated collision ignores are preserved. */
	void EndMantleCollisionIgnore(UPrimitiveComponent* ExpectedComponent);

	/** Current obstacle for this component's mantle collision lease, or null while ordinary movement owns collision. */
	UPrimitiveComponent* GetMantleCollisionComponent() const { return MantleCollisionComponent; }

	/** Root-motion rotation owns the same validated mantle lifetime on the predicting owner, server and simulated proxies. */
	bool IsMantleControllingRotation() const { return MantleCollisionComponent != nullptr; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Returns the current ground info.  Calling this will update the ground info if it's out of date.
	UFUNCTION(BlueprintCallable, Category = "Rpg|CharacterMovement")
	const FRpgCharacterGroundInfo& GetGroundInfo();

	//~UMovementComponent interface
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual float GetMaxSpeed() const override;
	//~End of UMovementComponent interface
protected:
	/** Captures root-motion ownership before the engine consumes/clears its parameters at the end of physics. */
	virtual void UpdateVelocityBeforeMovement(float DeltaSeconds) override;
	/** Publishes the completed sprint state before the Character movement delegate used by authority stamina tasks. */
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** Preserves authored rotation warping during mantle even when ordinary locomotion enables physics rotation. */
	virtual void PhysicsRotation(float DeltaTime) override;
	/** Uses saved control yaw during replay without changing the player's current camera rotation. */
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;

	// Cached ground info for the character.  Do not access this directly!  It's only updated when accessed via GetGroundInfo().
	FRpgCharacterGroundInfo CachedGroundInfo;

private:
	friend class FSavedMove_RpgCharacter;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgBlockRepeatedSavedMoveYawTest;
	friend class FRpgSprintSavedMoveTest;
	friend class RpgCmcSprintTests::FFixture;
	friend class RpgCmcRotationModeTests::FFixture;
#endif
	bool SampleBlockMovement(float& OutSpeedLimit) const;
	bool SampleEquipmentInHands() const;
	bool SampleFacingCommitment(float& OutSteeringRate) const;
	/** Effective combat stance of the current move: saved history during replay, otherwise today's sample. */
	bool IsCombatStanceForMove() const;
	/** Effective ability facing commitment of the current move; OutSteeringRate is zero while the facing is held. */
	bool GetFacingCommitmentForMove(float& OutSteeringRate) const;
	/** Control rotation of the current move: the recorded original during replay, otherwise the live controller value. */
	FRotator GetControlRotationForMove() const;
	bool IsSprintDirectionAllowedForMove() const;
	/**
	 * Presentation mirror of the mode in the stock flags, as GASP sets them per tick: the Pose Search trajectory
	 * predictor, moving bases and correction fallbacks read them. PhysicsRotation still applies the policy itself.
	 */
	void RefreshRotationModeFlags();
	bool SampleSprintMovement(float& OutSprintSpeed) const;
	bool CanSprintForMove(float& OutSprintSpeed) const;
	void SetIsSprinting(bool bNewSprinting);

	// Only desire is compressed. Authorization/speed are immutable local replay history or a fresh server GAS sample.
	bool bWantsSprint = false;
	bool bSprintAuthorizedForMove = false;
	float SprintSpeedForMove = 0.f;
	bool bRootMotionOwnedSprintMove = false;

	/** Authority's effective gait for remote animation and late join. Owners predict from their own saved moves. */
	UPROPERTY(Transient, Replicated)
	bool bIsSprinting = false;

	// Immutable local saved-move values are applied only during replay; they are never client authority on the server.
	bool bBlockMovementForMove = false;
	float BlockMovementSpeedLimitForMove = 0.f;
	FRotator ControlRotationForMove = FRotator::ZeroRotator;
	bool bInBlockMovementScope = false;
	bool bEquipmentInHandsForMove = false;
	bool bFacingCommittedForMove = false;
	float FacingSteeringRateForMove = 0.f;
	// Set only around Super::PhysicsRotation so ComputeOrientToMovementRotation targets the camera instead of the input.
	bool bFaceControlRotationInPhysics = false;

	UFUNCTION()
	void OnRep_MantleCollisionComponent();

	/** Server-selected collision exception for simulated root-motion physics; the owning client predicts its own lease. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_MantleCollisionComponent)
	TObjectPtr<UPrimitiveComponent> MantleCollisionComponent;

	TWeakObjectPtr<UPrimitiveComponent> AppliedMantleCollisionComponent;
	bool bAddedMantleCollisionIgnore = false;
};
