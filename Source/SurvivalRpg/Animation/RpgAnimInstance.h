// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Animation/TrajectoryTypes.h"
#include "AnimationWarpingTypes.h"
#include "GameplayEffectTypes.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "SurvivalRpg/Core/Character/RpgCharacterRotationMode.h"
#include "RpgAnimInstance.generated.h"

class UAbilitySystemComponent;
class UAnimationAsset;
class UPoseSearchDatabase;
#if WITH_DEV_AUTOMATION_TESTS
class FRpgTurnInPlaceStateMachineTest;
#endif

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

/** Cosmetic controller-facing turn-in-place lifecycle; it never changes authoritative actor rotation. */
UENUM(BlueprintType)
enum class ERpgTurnInPlaceState : uint8
{
	Inactive,
	Collecting,
	Active,
	Recovering,
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
	FVector LastNonZeroWorldVelocity = FVector::ZeroVector;
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
	float ActorYaw = 0.0f;
	float ActorYawDelta = 0.0f;
	FVector ActorLocation = FVector::ZeroVector;
	ERpgLocomotionGait Gait = ERpgLocomotionGait::Idle;
	ERpgLocomotionStance Stance = ERpgLocomotionStance::Standing;
	ERpgLocomotionMovementState MovementState = ERpgLocomotionMovementState::None;
	ERpgCharacterRotationMode RotationMode = ERpgCharacterRotationMode::Free;
	FPoseSearchTrajectoryData TrajectoryGenerationData;
	FTransformTrajectory TransformTrajectory;
	bool bHasVelocity = false;
	bool bHasAcceleration = false;
	bool bHasGroundedMoveIntent = false;
	bool bIsFalling = false;
	bool bIsMovingOnGround = false;
	bool bIsCrouching = false;
	bool bIsAnyMontagePlaying = false;
	bool bHasTurnInPlaceBlockingGameplayTag = false;
	bool bTurnInPlaceHardReset = true;

	// Previous-owner data is maintained and consumed only by game-thread PreUpdate.
	uint32 PreviousOwnerUniqueId = 0;
	uint8 PreviousLocalRole = 0;
	uint8 PreviousRemoteRole = 0;
	float PreviousActorYaw = 0.0f;
	FVector PreviousActorLocation = FVector::ZeroVector;
	bool bHasPreviousOwnerSnapshot = false;
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

	/** Returns the signed authored turn angle nearest to a request, or zero below the 30-degree activation threshold. */
	static float QuantizeTurnInPlaceAngle(float SignedAngle);

	/** Returns the synthetic facing horizon in seconds for an authored 45/90/135/180-degree turn. */
	static float GetTurnInPlaceFacingDuration(float QuantizedAngle);

	/** Calculates a wrap-safe signed actor-yaw delta in degrees for the cosmetic turn accumulator. */
	static float CalculateTurnInPlaceYawDelta(float PreviousActorYaw, float CurrentActorYaw);

	/**
	 * Builds a facing-only world-space trajectory whose facing advances monotonically through the requested authored turn.
	 * Source sample count, times, and positions are preserved exactly.
	 */
	static FTransformTrajectory MakeTurnInPlaceSyntheticTrajectory(
		const FTransformTrajectory& SourceTrajectory,
		float CurrentActorYaw,
		float AccumulatedYaw,
		float QuantizedAngle);

	/**
	 * Resolves the current Blend Stack playback state and the bounded moving-locomotion inputs used by GASP nodes.
	 *
	 * The helper is safe for parallel AnimGraph evaluation: it reads only the Blend Stack node and immutable animation
	 * assets plus values copied from the game-thread proxy. Moving and procedural-warping alphas are forced to zero for
	 * idle, crouched, airborne, and montage-driven poses.
	 *
	 * @param Node Blend Stack Input node reference whose current asset and playback time should be queried.
	 * @param CurrentAnimAsset Currently playing Blend Stack asset, or null when the node has no active asset.
	 * @param CurrentAnimAssetTime Current playback time in seconds for CurrentAnimAsset.
	 * @param MovingAlpha Alpha for moving-only procedural nodes in the range [0, 1].
	 * @param OrientationWarpingAlpha MovingAlpha multiplied by the current animation's Enable_Warping curve.
	 * @param DesiredFacing World-space facing sampled from the current trajectory point.
	 * @param LocomotionDirection Last meaningful horizontal world-space velocity used by Orientation Warping.
	 * @param bEnableSteering True only when the bounded moving slice has a valid asset and trajectory.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Animation|Motion Matching", meta = (BlueprintThreadSafe))
	void GetGaspBlendStackInputs(
		const FAnimNodeReference& Node,
		UAnimationAsset*& CurrentAnimAsset,
		float& CurrentAnimAssetTime,
		float& MovingAlpha,
		float& OrientationWarpingAlpha,
		FQuat& DesiredFacing,
		FVector& LocomotionDirection,
		bool& bEnableSteering) const;

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

	/** Mirrored `Gameplay.MovementStopped` state; game-thread authored and snapshotted before parallel evaluation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Gameplay Tags")
	bool bGameplayMovementStopped = false;

	/** Mirrored `State.Blocking` state; game-thread authored and snapshotted before parallel evaluation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Gameplay Tags")
	bool bStateBlocking = false;

	/** Mirrored `State.Dead` state; game-thread authored and snapshotted before parallel evaluation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Gameplay Tags")
	bool bStateDead = false;

	/** Mirrored `State.Staggered` state; game-thread authored and snapshotted before parallel evaluation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Gameplay Tags")
	bool bStateStaggered = false;

	/** Mirrored `State.GuardBroken` state; game-thread authored and snapshotted before parallel evaluation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Gameplay Tags")
	bool bStateGuardBroken = false;

	/**
	 * Enables Pose Search trajectory generation in the game-thread proxy snapshot.
	 * This is static AnimBP configuration: keep it disabled for non-Motion-Matching graphs.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	bool bGeneratePoseSearchTrajectory = false;

	/**
	 * Pose Search databases used while grounded, ordered as Idle, Walk, Run, and Sprint.
	 * Static designer-authored defaults; never mutate this array while an AnimInstance is evaluating.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> GroundMotionMatchingDatabases;

	/**
	 * Pose Search database used for grounded crouching locomotion.
	 * This is static designer-authored configuration and must not be mutated while an AnimInstance is evaluating.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> CrouchingMotionMatchingDatabase;

	/**
	 * Exclusive Pose Search database for authored standing turn-in-place clips.
	 * Static designer configuration; runtime selection remains cosmetic and never rotates the owning actor.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> TurnInPlaceMotionMatchingDatabase;

	/**
	 * Pose Search databases used while airborne by Motion Matching AnimBPs.
	 * Static designer-authored defaults; never mutate this array while an AnimInstance is evaluating.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> AirborneMotionMatchingDatabases;

	/** Character velocity in world space, snapshotted on the game thread and read-only to AnimBPs. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector WorldVelocity = FVector::ZeroVector;

	/**
	 * Last meaningful horizontal character velocity in world space.
	 * Persisted by the game-thread proxy through zero-velocity frames and copied read-only for parallel AnimGraph use.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector LastNonZeroWorldVelocity = FVector::ZeroVector;

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

	/** Cosmetic gait selected from grounded movement intent; Sprint is reserved until gameplay exposes an explicit sprint state. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	ERpgLocomotionGait LocomotionGait = ERpgLocomotionGait::Idle;

	/** Replicated standing/crouching state translated into an animation-facing enum. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	ERpgLocomotionStance LocomotionStance = ERpgLocomotionStance::Standing;

	/** CharacterMovement mode translated into a worker-thread-safe animation state. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	ERpgLocomotionMovementState LocomotionMovementState = ERpgLocomotionMovementState::None;

	/**
	 * Replicated character rotation policy copied from the game-thread proxy for AnimBP debugging.
	 * This transient cosmetic snapshot is read-only and never owns authoritative rotation state.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	ERpgCharacterRotationMode CharacterRotationMode = ERpgCharacterRotationMode::Free;

	/**
	 * Game-thread-generated world-space trajectory consumed read-only by the Motion Matching history collector.
	 * The value is transient and never authoritative gameplay state.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	FTransformTrajectory LocomotionTrajectory;

	/** Current cosmetic turn-in-place lifecycle, derived entirely from game-thread proxy snapshots. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Turn In Place")
	ERpgTurnInPlaceState TurnInPlaceState = ERpgTurnInPlaceState::Inactive;

	/** Signed authored query angle in degrees; zero while no turn request is active. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Turn In Place", Meta = (Units = "deg"))
	float TurnInPlaceQueryAngle = 0.0f;

	/** Wrap-safe actor-yaw delta accumulated for the current cosmetic turn request, in degrees. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Turn In Place", Meta = (Units = "deg"))
	float TurnInPlaceAccumulatedYaw = 0.0f;

	/** Elapsed time in the current turn-in-place lifecycle state, in seconds. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Turn In Place", Meta = (Units = "s"))
	float TurnInPlaceStateElapsed = 0.0f;

	/** Stationary facing-only query used while the exclusive turn-in-place database is active. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Turn In Place")
	FTransformTrajectory TurnInPlaceSyntheticTrajectory;

	/** Runtime rotation behavior wired to the graph's Offset Root Bone node; translation remains graph-authored. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Turn In Place")
	EOffsetRootBoneMode OffsetRootRotationMode = EOffsetRootBoneMode::Interpolate;

	/** One-frame pulse that clears stale Offset Root Bone state after a hard turn-in-place reset. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Turn In Place")
	bool bResetOffsetRootEveryFrame = false;

	/**
	 * Alpha for procedural locomotion nodes; zero while airborne, crouched, or a gameplay montage is active.
	 * This prevents Orientation Warping from influencing combat, harvesting, hit, or death transitions.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	float ProceduralLocomotionAlpha = 0.0f;

	/** True when any montage is active in this AnimInstance; cosmetic-only and snapshotted on the game thread. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Montage")
	bool bIsAnyMontagePlaying = false;

private:
	/** Database policy applied by the generic pre-update callback to the upcoming Motion Matching search. */
	enum class ETurnInPlaceSearchMode : uint8
	{
		NormalLocomotion,
		SearchRequestedTurn,
		ContinueSelectedTurn,
	};

	void UpdateTurnInPlaceRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy);
	void BeginTurnInPlaceRequest(float QuantizedAngle);
	void BeginTurnInPlaceRecovery(bool bHardResetOffset);
	void ResetTurnInPlaceRuntime(bool bHardResetOffset);
	/** Clears only the cosmetic selection/playback latch; request serials remain monotonic. */
	void ClearTurnInPlaceSelection();
	bool IsTurnInPlaceEligible(const FRpgAnimInstanceProxy& Proxy) const;
	bool ConsumeTurnInPlaceForceInterruptRequest();
	/** Allows request retargeting only until the generic pre-update callback dispatches its first TIR search. */
	bool CanRetargetTurnInPlaceRequest() const;
	/** Latches the first valid TIR SearchResult from the previous completed node update for the request serial. */
	bool TryLatchTurnInPlaceSelection(
		UAnimationAsset* SelectedAsset,
		const UPoseSearchDatabase* SelectedDatabase,
		float SelectedTime,
		bool bSelectedAssetLooping,
		uint32 SelectionRequestSerial);
	/** Tracks the latched asset and its actual Blend Stack play rate independently of later PoseSearch-result validity. */
	void UpdateTurnInPlaceLatchedPlayback(
		UAnimationAsset* CurrentAsset,
		float CurrentAssetTime,
		float CurrentAssetLength,
		float CurrentAssetPlayRate,
		float DeltaSeconds);
	/** Resolves the upcoming search policy without issuing more than one full TIR search per request. */
	ETurnInPlaceSearchMode ResolveTurnInPlaceSearchMode(bool bForceNewRequest) const;
	/** Allows moving procedural nodes during a movement-driven recovery, but never during collection or playback. */
	bool AllowsMovingProceduralNodes() const;

	float TurnInPlaceStableElapsed = 0.0f;
	float TurnInPlaceSelectionElapsed = 0.0f;
	/** Remaining full-sequence playback time for the latched cosmetic turn, in seconds. */
	float TurnInPlaceSelectedAssetRemainingTime = 0.0f;
	/** PoseSearch-selected start time copied from the previous completed search result, in seconds. */
	float TurnInPlaceSelectedAssetStartTime = 0.0f;
	/** Wall-clock watchdog budget, restarted on playback observation and scaled by the actual non-looping play rate. */
	float TurnInPlacePlaybackWatchdogDuration = 0.0f;
	float TurnInPlaceRequestAccumulatedYaw = 0.0f;
	uint32 TurnInPlaceRequestSerial = 0;
	uint32 TurnInPlaceInterruptedRequestSerial = 0;
	/** Request serial owning TurnInPlaceSelectedAsset; zero means no selection is latched. */
	uint32 TurnInPlaceSelectedRequestSerial = 0;

	/** Exact non-authoritative animation chosen for the active request; the database and Blend Stack retain ownership. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> TurnInPlaceSelectedAsset;

	bool bTurnInPlacePoseSelected = false;
	bool bTurnInPlaceSelectedAssetLooping = false;
	/** True after the first valid SearchResult has been bound to the active request. */
	bool bTurnInPlaceSelectionLatched = false;
	/** True after the latched asset has become the Blend Stack's actual current asset. */
	bool bTurnInPlacePlaybackObserved = false;
	/** True when pre-update selects normal locomotion for the upcoming search at natural completion. */
	bool bTurnInPlaceCompletionArmed = false;
	bool bTurnInPlaceHardResetConditionLastFrame = false;
	bool bTurnInPlaceInitializationResetPending = false;

	friend struct FRpgAnimInstanceProxy;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgTurnInPlaceStateMachineTest;
#endif
};
