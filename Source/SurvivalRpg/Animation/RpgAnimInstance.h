// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Animation/TrajectoryTypes.h"
#include "GameplayEffectTypes.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "RpgAnimInstance.generated.h"

class UAbilitySystemComponent;
class UPoseSearchDatabase;

/** Cosmetic locomotion speed band derived from the authoritative movement-component snapshot. */
UENUM(BlueprintType)
enum class ERpgLocomotionGait : uint8
{
	Idle,
	Walk,
	Run,
	Sprint,
};

/** Cosmetic stance mirrored from the replicated character crouch state. */
UENUM(BlueprintType)
enum class ERpgLocomotionStance : uint8
{
	Standing,
	Crouching,
};

/** Animation-facing movement state derived from CharacterMovement without exposing the component to worker threads. */
UENUM(BlueprintType)
enum class ERpgLocomotionMovementState : uint8
{
	None,
	Grounded,
	Airborne,
	Swimming,
	Flying,
	Custom,
};

/**
 * Game-thread snapshot consumed by URpgAnimInstance during parallel animation updates.
 *
 * UObject and movement-component access is deliberately confined to PreUpdate; the animation
 * worker thread only reads the plain values stored here.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FRpgAnimInstanceProxy() = default;
	explicit FRpgAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;

	FVector WorldVelocity = FVector::ZeroVector;
	FVector LocalVelocity = FVector::ZeroVector;
	FVector WorldAcceleration = FVector::ZeroVector;
	FVector LocalAcceleration = FVector::ZeroVector;
	float GroundSpeed = 0.0f;
	float VerticalVelocity = 0.0f;
	float GroundDistance = -1.0f;
	float AimYaw = 0.0f;
	float AimPitch = 0.0f;
	float LocomotionAngle = 0.0f;
	float ProceduralLocomotionAlpha = 0.0f;
	float DesiredControllerYawLastUpdate = 0.0f;
	ERpgLocomotionGait Gait = ERpgLocomotionGait::Idle;
	ERpgLocomotionStance Stance = ERpgLocomotionStance::Standing;
	ERpgLocomotionMovementState MovementState = ERpgLocomotionMovementState::None;
	FPoseSearchTrajectoryData TrajectoryGenerationData;
	FTransformTrajectory TransformTrajectory;
	bool bHasVelocity = false;
	bool bHasAcceleration = false;
	bool bIsFalling = false;
	bool bIsMovingOnGround = false;
	bool bIsCrouching = false;
	bool bIsAnyMontagePlaying = false;
};


/**
 * URpgAnimInstance
 *
 * Thread-safe base animation instance shared by RPG locomotion and montage-capable character AnimBPs.
 */
UCLASS(Config = Game)
class SURVIVALRPG_API URpgAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	explicit URpgAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

	/** Returns whether this AnimBP opts into game-thread trajectory generation for Motion Matching. */
	bool ShouldGeneratePoseSearchTrajectory() const { return bGeneratePoseSearchTrajectory; }

	/**
	 * Supplies the active project-local Pose Search databases to a Motion Matching node.
	 * Bound as the node's thread-safe On Update function; it reads only immutable defaults and proxy snapshots.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Motion Matching", meta = (BlueprintThreadSafe))
	void UpdateGaspMotionMatching(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	/**
	 * Keeps listen-server copies of remote autonomous characters time-correct when several
	 * client moves tick their pose in one server frame; all regular animation work stays parallel.
	 */
	virtual bool CanRunParallelWork() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	/** Gameplay tags mirrored from the owning ASC into AnimBP variables; do not query the ASC from worker-thread graph logic. */
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	/**
	 * Enables Pose Search trajectory generation in the game-thread proxy snapshot.
	 * This is static AnimBP configuration: keep it disabled for non-Motion-Matching graphs.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	bool bGeneratePoseSearchTrajectory = false;

	/**
	 * Pose Search databases used while grounded by Motion Matching AnimBPs.
	 * Static designer-authored defaults; never mutate this array while an AnimInstance is evaluating.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> GroundMotionMatchingDatabases;

	/**
	 * Pose Search databases used while airborne by Motion Matching AnimBPs.
	 * Static designer-authored defaults; never mutate this array while an AnimInstance is evaluating.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> AirborneMotionMatchingDatabases;

	/** Character velocity in world space, snapshotted on the game thread and read-only to AnimBPs. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector WorldVelocity = FVector::ZeroVector;

	/** Character velocity transformed into character-local space. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector LocalVelocity = FVector::ZeroVector;

	/** Current movement input acceleration in world space, including replicated data for simulated proxies. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector WorldAcceleration = FVector::ZeroVector;

	/** Current movement input acceleration transformed into character-local space. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector LocalAcceleration = FVector::ZeroVector;

	/** Horizontal speed in centimeters per second. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion", Meta = (Units = "cm/s"))
	float LocomotionGroundSpeed = 0.0f;

	/** Vertical velocity in centimeters per second; positive values move upward. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion", Meta = (Units = "cm/s"))
	float VerticalVelocity = 0.0f;

	/** Distance from the capsule bottom to the ground in centimeters; -1 means no valid owner snapshot. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion", Meta = (Units = "cm"))
	float GroundDistance = -1.0f;

	/** Controller aim yaw relative to the character in degrees, normalized to [-180, 180]. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Aim", Meta = (Units = "deg"))
	float AimYaw = 0.0f;

	/** Controller aim pitch relative to the character in degrees, normalized to [-180, 180]. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Aim", Meta = (Units = "deg"))
	float AimPitch = 0.0f;

	/**
	 * Signed movement angle relative to the owning actor's forward axis.
	 * The graph-driven Orientation Warping node compares it with animation root motion; cosmetic-only, in degrees.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching", Meta = (Units = "deg"))
	float LocomotionAngle = 0.0f;

	/** True when the velocity snapshot is non-zero. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bHasVelocity = false;

	/** True when movement input acceleration is present. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bHasAcceleration = false;

	/** True while CharacterMovement is in the falling movement mode. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bLocomotionIsFalling = false;

	/** True while CharacterMovement considers the character grounded. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bIsMovingOnGround = false;

	/** Replicated crouch state owned by the character. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bIsCrouching = false;

	/** Cosmetic gait band derived from current speed relative to CharacterMovement's authoritative max speed. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	ERpgLocomotionGait LocomotionGait = ERpgLocomotionGait::Idle;

	/** Replicated standing/crouching state translated into an animation-facing enum. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	ERpgLocomotionStance LocomotionStance = ERpgLocomotionStance::Standing;

	/** CharacterMovement mode translated into a worker-thread-safe animation state. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	ERpgLocomotionMovementState LocomotionMovementState = ERpgLocomotionMovementState::None;

	/**
	 * Game-thread-generated world-space trajectory consumed read-only by the Motion Matching history collector.
	 * The value is transient and never authoritative gameplay state.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	FTransformTrajectory LocomotionTrajectory;

	/**
	 * Alpha for procedural locomotion nodes; zero while airborne, crouched, or a gameplay montage is active.
	 * This prevents Orientation Warping from influencing combat, harvesting, hit, or death transitions.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	float ProceduralLocomotionAlpha = 0.0f;

	/** True when any montage is active in this AnimInstance; cosmetic-only and snapshotted on the game thread. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Montage")
	bool bIsAnyMontagePlaying = false;
};
