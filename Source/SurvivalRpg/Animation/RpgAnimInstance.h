// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Animation/TrajectoryTypes.h"
#include "AnimationWarpingTypes.h"
#include "Engine/EngineTypes.h"
#include "GameplayEffectTypes.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "RpgFootPlacementTypes.h"
#include "SurvivalRpg/Core/Character/RpgCharacterRotationMode.h"
#include "RpgAnimInstance.generated.h"

class UAbilitySystemComponent;
class UAnimationAsset;
class UPoseSearchDatabase;
#if WITH_DEV_AUTOMATION_TESTS
class FRpgJumpPhaseRuntimeTest;
class FRpgLandingSelectionRuntimeTest;
class FRpgMotionMatchingDatabaseResolverTest;
class FRpgTrajectoryCollisionRuntimeTest;
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

/** Stable project roles for every Pose Search database used by the curated GASP CMC runtime. */
UENUM(BlueprintType)
enum class ERpgMotionMatchingDatabaseRole : uint8
{
	None,
	StandIdle,
	StandWalk,
	StandWalkStops,
	StandRunLoops,
	StandRunPivots,
	StandRunStarts,
	StandRunStops,
	StandSprint,
	StandSprintStops,
	Crouch,
	StandTurnInPlace,
	Jump,
	StandLightLanding,
	StandHeavyLanding,
	WalkLightLanding,
	WalkHeavyLanding,
	RunLightLanding,
	RunHeavyLanding,
	Count UMETA(Hidden),
};

/**
 * Static grounded Pose Search database groups consumed by the project-local locomotion selector.
 *
 * The fixed array shapes are part of the runtime contract: Idle stores one database, Walk and
 * Sprint each store Moving Aggregate and Stops, while Run stores Loops, Pivots, Starts, and Stops.
 * Runtime selection mirrors the relevant GASP chooser domains: moving Run can offer Starts, Loops,
 * and Pivots together, while logical Idle exposes overlapping stop rows from current ground speed.
 * Sprint Stops additionally require a real Sprint gait so a project-tuned 600 cm/s Run cannot
 * select GASP's forward-only Sprint Stops. The current input mapping intentionally produces only
 * Walk/Run, leaving this content dormant until gameplay supplies an explicit Sprint gait. The
 * values are designer-authored AnimBP defaults and must never be mutated during evaluation.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgGroundMotionMatchingDatabaseSets
{
	GENERATED_BODY()

	FRpgGroundMotionMatchingDatabaseSets()
	{
		Idle.SetNum(1);
		Walk.SetNum(2);
		Run.SetNum(4);
		Sprint.SetNum(2);
	}

	/** Single database searched for standing idle locomotion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, EditFixedSize, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> Idle;

	/** Two Walk roles: [0] is the Moving Aggregate and [1] is Stops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, EditFixedSize, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> Walk;

	/** Four Run role databases ordered exactly as Loops, Pivots, Starts, and Stops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, EditFixedSize, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> Run;

	/** Two Sprint roles: [0] is the Moving Aggregate and [1] is Stops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, EditFixedSize, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> Sprint;
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

/** Cosmetic jump lifecycle used to select airborne starts and one bounded curated landing pose. */
UENUM(BlueprintType)
enum class ERpgJumpPhase : uint8
{
	Grounded,
	Airborne,
	Landing,
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
 * Game-thread collision policy for the cosmetic Pose Search trajectory.
 *
 * The project resolves at most the configured number of future samples per update. None of
 * these bounded sweeps changes CharacterMovement, collision, replication, or touchdown state.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgTrajectoryCollisionSettings
{
	GENERATED_BODY()

	/** Enables game-thread gravity and world-collision correction for generated Pose Search trajectories. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	bool bEnabled = true;

	/** Height in centimeters retained between a corrected trajectory sample and the hit floor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "0.001", ClampMax = "10.0", UIMin = "0.001", UIMax = "2.0", Units = "cm"))
	float FloorOffset = 0.01f;

	/** Maximum vertical obstacle/floor search range per future sample, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "1.0", ClampMax = "1000.0", UIMin = "50.0", UIMax = "300.0", Units = "cm"))
	float MaxObstacleHeight = 150.0f;

	/** Radius of each bounded walkability sweep, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "0.1", ClampMax = "20.0", UIMin = "0.5", UIMax = "10.0", Units = "cm"))
	float SweepRadius = 2.0f;

	/** Maximum future samples swept per update; clamped to the generated 15-sample GASP horizon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (ClampMin = "1", ClampMax = "15", UIMin = "1", UIMax = "15"))
	int32 MaxPredictionSamples = 15;

	/** Collision channel used by cosmetic prediction sweeps; defaults to Visibility like GASP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Uses complex geometry for cosmetic prediction traces; disabled by default to keep the hot path bounded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	bool bTraceComplex = false;
};

/** Pointer-free, cosmetic prediction of the next walkable landing within the trajectory horizon. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgTrajectoryLandingPrediction
{
	GENERATED_BODY()

	/** Predicted world-space contact point from the first validated walkable trajectory collision. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	FVector LandingLocation = FVector::ZeroVector;

	/** Unit world-space normal at the predicted walkable contact. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	FVector LandingNormal = FVector::UpVector;

	/** Seconds until predicted contact; -1 means there is no valid airborne prediction. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision", meta = (Units = "s"))
	float TimeToLand = -1.0f;

	/** True only for a finite, walkable airborne hit inside the current prediction horizon. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Trajectory Collision")
	bool bIsValid = false;
};

/**
 * Pointer-free movement context frozen on the final airborne update before physical touchdown.
 *
 * CharacterMovement remains authoritative for the actual Falling-to-Grounded transition. This
 * snapshot is cosmetic-only and lets owner and simulated-proxy animation select the same bounded
 * Idle/Walk/Run and Light/Heavy landing domain after the grounded frame has cleared fall velocity.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgLandingSelectionSnapshot
{
	GENERATED_BODY()

	/** Horizontal pre-touchdown velocity in world space, in centimeters per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing", meta = (Units = "cm/s"))
	FVector HorizontalVelocity = FVector::ZeroVector;

	/** Magnitude of HorizontalVelocity, in centimeters per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing", meta = (Units = "cm/s"))
	float HorizontalSpeed = 0.0f;

	/** Latest signed world-Z velocity before touchdown; positive values move upward. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing", meta = (Units = "cm/s"))
	float VerticalVelocity = 0.0f;

	/** Strongest measured speed along gravity during this airborne epoch. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing", meta = (Units = "cm/s"))
	float MaximumDownwardSpeed = 0.0f;

	/** Downward speed projected to the current valid trajectory contact, or zero without one. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing", meta = (Units = "cm/s"))
	float PredictedImpactDownwardSpeed = 0.0f;

	/** Last explicit or safely reconstructed locomotion gait; Sprint is never inferred from speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing")
	ERpgLocomotionGait Gait = ERpgLocomotionGait::Idle;

	/** Final airborne collision prediction retained through the physical touchdown update. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing")
	FRpgTrajectoryLandingPrediction PredictedLanding;

	/** Monotonic cosmetic epoch used to distinguish a relaunch from an earlier fall. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing")
	int32 AirborneEpoch = 0;

	/** True when raw movement acceleration expressed intent before touchdown. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing")
	bool bHasMoveIntent = false;

	/** True only for finite movement data captured during the current airborne epoch. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Animation|Landing")
	bool bIsValid = false;
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

	/** Persistent value-only state owned and touched exclusively by game-thread PreUpdate. */
	struct FFootPlacementLegState
	{
		FTransform LockedFootTransformWorld = FTransform::Identity;
		FVector LockedGroundPointWorld = FVector::ZeroVector;
		FVector LockedGroundNormalWorld = FVector::UpVector;
		FVector RetainedGroundPointWorld = FVector::ZeroVector;
		FVector RetainedGroundNormalWorld = FVector::UpVector;
		FTransform PreviousHitComponentTransform = FTransform::Identity;
		uint32 HitComponentId = 0;
		float TraceMissElapsed = 0.0f;
		float Weight = 0.0f;
		bool bLocked = false;
		bool bWantedToPlantLastFrame = false;
		bool bHasRetainedGroundTarget = false;
		bool bHasPreviousHitComponentTransform = false;
	};

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
	float AirborneProceduralAlpha = 0.0f;
	float DesiredControllerYawLastUpdate = 0.0f;
	float ActorYaw = 0.0f;
	float ActorYawDelta = 0.0f;
	FVector ActorLocation = FVector::ZeroVector;
	ERpgLocomotionGait Gait = ERpgLocomotionGait::Idle;
	ERpgLocomotionStance Stance = ERpgLocomotionStance::Standing;
	ERpgLocomotionMovementState MovementState = ERpgLocomotionMovementState::None;
	ERpgCharacterRotationMode RotationMode = ERpgCharacterRotationMode::Free;
	FPoseSearchTrajectoryData TrajectoryGenerationData;
	/** Persistent raw trajectory history used only as the next game-thread generation input. */
	FTransformTrajectory RawTransformTrajectory;
	/** Gravity/collision-corrected trajectory published to the worker thread. */
	FTransformTrajectory TransformTrajectory;
	FRpgTrajectoryLandingPrediction TrajectoryLandingPrediction;
	FRpgLandingSelectionSnapshot LandingSelectionSnapshot;
	FRpgFootPlacementSnapshot FootPlacementSnapshot;
	float FootPlacementAlpha = 0.0f;
	bool bHasVelocity = false;
	bool bHasAcceleration = false;
	bool bHasGroundedMoveIntent = false;
	bool bIsFalling = false;
	bool bIsMovingOnGround = false;
	bool bIsCrouching = false;
	bool bIsAnyMontagePlaying = false;
	bool bHasTurnInPlaceBlockingGameplayTag = false;
	bool bTurnInPlaceHardReset = true;
	/** True when the game-thread snapshot crosses between free-facing and turn-in-place-capable rotation. */
	bool bTurnInPlaceSupportChanged = false;

	// Persistent pre-touchdown state is authored only by game-thread PreUpdate.
	ERpgLocomotionGait LastGroundedGait = ERpgLocomotionGait::Idle;
	int32 LandingAirborneEpoch = 0;
	bool bWasAirborneForLanding = false;

	// Previous-owner data is maintained and consumed only by game-thread PreUpdate.
	uint32 PreviousOwnerUniqueId = 0;
	uint8 PreviousLocalRole = 0;
	uint8 PreviousRemoteRole = 0;
	float PreviousActorYaw = 0.0f;
	FVector PreviousActorLocation = FVector::ZeroVector;
	ERpgCharacterRotationMode PreviousRotationMode = ERpgCharacterRotationMode::Free;
	bool bHasPreviousOwnerSnapshot = false;

	// Previous foot-placement data is maintained and consumed only by game-thread PreUpdate.
	FFootPlacementLegState FootPlacementLegStates[2];
	FTransform PreviousFootPlacementComponentTransform = FTransform::Identity;
	FTransform PreviousMovementBaseTransform = FTransform::Identity;
	uint32 PreviousMovementBaseId = 0;
	bool bHasPreviousFootPlacementComponentTransform = false;
	bool bHasPreviousMovementBaseTransform = false;
	bool bPreviousFootPlacementSourceEligible = false;
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

	/** Returns the immutable game-thread collision policy paired with generated trajectories. */
	const FRpgTrajectoryCollisionSettings& GetTrajectoryCollisionSettings() const
	{
		return TrajectoryCollisionSettings;
	}

	/**
	 * Supplies the active project-local Pose Search databases to a Motion Matching node.
	 * Bound as the node's thread-safe On Update function; it consumes proxy-owned snapshots and mutates only
	 * cosmetic update-thread selection state plus the referenced Motion Matching node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Motion Matching", meta = (BlueprintThreadSafe))
	void UpdateGaspMotionMatching(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	/**
	 * Captures the completed Pose Search result before the Motion Matching node blends to it.
	 * Bound as the node's thread-safe On Motion Matching State Updated function; it records only
	 * immutable database metadata and cosmetic selection latches.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Motion Matching", meta = (BlueprintThreadSafe))
	void UpdateGaspMotionMatchingPostSelection(
		const FAnimUpdateContext& Context,
		const FAnimNodeReference& Node);

	/** Returns the one project-owned role tag required on a runtime Pose Search database. */
	static FName GetMotionMatchingDatabaseRoleTag(ERpgMotionMatchingDatabaseRole Role);

	/** Returns the one project-owned locomotion-state tag required for a database role. */
	static FName GetMotionMatchingDatabaseStateTag(ERpgMotionMatchingDatabaseRole Role);

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
	 * assets plus values copied from the game-thread proxy. Each Blend Stack sample keeps gates based on its immutable
	 * asset category while it blends across a grounded/airborne boundary: moving ground and Jump/Starts samples receive
	 * authored moving corrections, while the airborne fall and Idle landing samples receive Reset Root only. A latched
	 * Walk/Run landing keeps moving corrections for its full request. Orientation Warping is additionally gated by the
	 * authored Enable_Warping curve.
	 *
	 * @param Node Blend Stack Input node reference whose current asset and playback time should be queried.
	 * @param CurrentAnimAsset Currently playing Blend Stack asset, or null when the node has no active asset.
	 * @param CurrentAnimAssetTime Current playback time in seconds for CurrentAnimAsset.
	 * @param ResetRootAlpha Alpha for Reset Root on moving ground, airborne jump, or the latched landing sample.
	 * @param OrientationWarpingAlpha Moving-sample alpha multiplied by the current animation's Enable_Warping curve.
	 * @param DesiredFacing World-space facing sampled from the current trajectory point.
	 * @param LocomotionDirection Last meaningful horizontal world-space velocity used by Orientation Warping.
	 * @param bEnableSteering True only for valid grounded movement or a contracted Jump/Starts asset with trajectory data.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Animation|Motion Matching", meta = (BlueprintThreadSafe))
	void GetGaspBlendStackInputs(
		const FAnimNodeReference& Node,
		UAnimationAsset*& CurrentAnimAsset,
		float& CurrentAnimAssetTime,
		float& ResetRootAlpha,
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

	/**
	 * Static game-thread trace and plant-lock policy for the project-local Foot Placement node.
	 * Disabled defaults keep non-GASP AnimBPs free of trace cost; runtime results are cosmetic only.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FRpgFootPlacementSettings FootPlacementSettings;

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
	 * Designer-authored game-thread collision policy for generated Pose Search trajectories.
	 * Runtime results are cosmetic and are never replicated or used as CharacterMovement truth.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	FRpgTrajectoryCollisionSettings TrajectoryCollisionSettings;

	/**
	 * Project-local grounded database groups resolved from the cosmetic gait snapshot.
	 * Static designer-authored defaults; runtime evaluation reads but never mutates these references.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	FRpgGroundMotionMatchingDatabaseSets GroundMotionMatchingDatabaseSets;

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

	/**
	 * Exclusive stand-idle light-landing database searched once per matching physical touchdown.
	 * This preserves the #66 serialized property and exact four-clip Idle-Light contract.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> LandingMotionMatchingDatabase;

	/** Exclusive stand-idle heavy-landing database selected at or above HeavyLandingSpeedThreshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> StandHeavyLandingMotionMatchingDatabase;

	/** Exclusive Walk light-landing database selected from frozen pre-touchdown movement context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> WalkLightLandingMotionMatchingDatabase;

	/** Exclusive Walk heavy-landing database selected at or above HeavyLandingSpeedThreshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> WalkHeavyLandingMotionMatchingDatabase;

	/** Exclusive Run light-landing database selected from frozen pre-touchdown movement context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> RunLightLandingMotionMatchingDatabase;

	/** Exclusive Run heavy-landing database selected at or above HeavyLandingSpeedThreshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> RunHeavyLandingMotionMatchingDatabase;

	/**
	 * Cosmetic impact-speed boundary between Light and Heavy landing presentation, in cm/s.
	 * GASP authors 700 cm/s; the comparison is inclusive and never affects fall damage or gameplay.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching", meta = (ClampMin = "1.0", UIMin = "300.0", UIMax = "1200.0", Units = "cm/s"))
	float HeavyLandingSpeedThreshold = 700.0f;

	/** Role of the most recently completed Pose Search result; cosmetic and worker-thread owned. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	ERpgMotionMatchingDatabaseRole CurrentMotionMatchingDatabaseRole =
		ERpgMotionMatchingDatabaseRole::None;

	/** True when the most recently completed search retained its Continuing Pose. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	bool bCurrentMotionMatchingResultIsContinuingPose = false;

	/** Interrupt mode paired with the most recently completed search; cosmetic diagnostics only. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	EPoseSearchInterruptMode CurrentMotionMatchingInterruptMode =
		EPoseSearchInterruptMode::DoNotInterrupt;

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

	/** Pointer-free game-thread trace/lock snapshot consumed by the parallel Foot Placement node. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Foot Placement")
	FRpgFootPlacementSnapshot FootPlacementSnapshot;

	/** Global cosmetic alpha shared by project-local Foot Placement and downstream Leg IK. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FootPlacementAlpha = 0.0f;

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

	/** Current cosmetic jump phase; it never changes CharacterMovement or authoritative movement state. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Jump")
	ERpgJumpPhase JumpPhase = ERpgJumpPhase::Grounded;

	/** Elapsed time in the current bounded landing request, in seconds. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Jump", Meta = (Units = "s"))
	float LandingStateElapsed = 0.0f;

	/** True after the first valid result from the exclusive landing database has been bound to the current request. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Jump")
	bool bLandingSelectionLatched = false;

	/** Immutable database role chosen for the active touchdown request; None outside Landing. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Jump")
	ERpgMotionMatchingDatabaseRole ActiveLandingDatabaseRole =
		ERpgMotionMatchingDatabaseRole::None;

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

	/**
	 * Pointer-free landing prediction copied from the game-thread proxy.
	 * Consumers must honor bIsValid; authoritative touchdown still belongs to CharacterMovement.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Jump")
	FRpgTrajectoryLandingPrediction TrajectoryLandingPrediction;

	/** Frozen final-airborne context used only for cosmetic landing selection and diagnostics. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Jump")
	FRpgLandingSelectionSnapshot PreTouchdownLandingSnapshot;

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

	/**
	 * Airborne procedural budget snapshotted independently from grounded locomotion.
	 * It is cosmetic-only and becomes zero for crouch or montage overrides; the active asset still decides each node gate.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Motion Matching")
	float AirborneProceduralAlpha = 0.0f;

	/** True when any montage is active in this AnimInstance; cosmetic-only and snapshotted on the game thread. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Montage")
	bool bIsAnyMontagePlaying = false;

private:
	/** Value-only integrity summary shared by editor validation and focused automation coverage. */
	struct FGroundMotionMatchingDatabaseSetValidation
	{
		bool bHasInvalidShape = false;
		bool bHasNullDatabase = false;
		bool bHasDuplicateDatabase = false;

		bool IsValid() const
		{
			return !bHasInvalidShape && !bHasNullDatabase && !bHasDuplicateDatabase;
		}
	};

	/** One expected project role and its designer-authored database reference. */
	struct FMotionMatchingDatabaseRoleContract
	{
		ERpgMotionMatchingDatabaseRole Role = ERpgMotionMatchingDatabaseRole::None;
		UPoseSearchDatabase* Database = nullptr;
	};

	/** Integrity summary for project role/state tags and role-to-database ownership. */
	struct FMotionMatchingDatabaseRoleValidation
	{
		bool bHasNullDatabase = false;
		bool bHasDuplicateDatabase = false;
		bool bHasMissingRole = false;
		bool bHasDuplicateRole = false;
		bool bHasMissingRoleTag = false;
		bool bHasDuplicateRoleTag = false;
		bool bHasWrongRoleTag = false;
		bool bHasMissingStateTag = false;
		bool bHasDuplicateStateTag = false;
		bool bHasWrongStateTag = false;

		bool IsValid() const
		{
			return !bHasNullDatabase && !bHasDuplicateDatabase &&
				!bHasMissingRole && !bHasDuplicateRole &&
				!bHasMissingRoleTag && !bHasDuplicateRoleTag && !bHasWrongRoleTag &&
				!bHasMissingStateTag && !bHasDuplicateStateTag && !bHasWrongStateTag;
		}
	};

	using FResolvedGroundMotionMatchingDatabases =
		TArray<UPoseSearchDatabase*, TInlineAllocator<4>>;
	using FResolvedMotionMatchingDatabaseRoles =
		TArray<ERpgMotionMatchingDatabaseRole, TInlineAllocator<4>>;
	using FMotionMatchingDatabaseRoleContracts =
		TArray<FMotionMatchingDatabaseRoleContract, TInlineAllocator<18>>;

	/**
	 * Pointer-free locomotion snapshot used to classify one bounded project database search.
	 * It deliberately contains no actor, controller, authority, or network-role state so identical
	 * movement snapshots resolve identically for local and simulated characters.
	 */
	struct FGroundMotionMatchingSelectionSnapshot
	{
		ERpgLocomotionGait Gait = ERpgLocomotionGait::Idle;
		ERpgLocomotionStance Stance = ERpgLocomotionStance::Standing;
		ERpgLocomotionMovementState MovementState = ERpgLocomotionMovementState::None;
		ERpgCharacterRotationMode RotationMode = ERpgCharacterRotationMode::Free;
		FVector WorldVelocity = FVector::ZeroVector;
		FVector WorldAcceleration = FVector::ZeroVector;
		FVector FutureVelocity = FVector::ZeroVector;
		float GroundSpeed = 0.0f;
		/** Role captured by the completed-search callback from the latest Motion Matching result. */
		ERpgMotionMatchingDatabaseRole CurrentDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
		bool bIsMovingOnGround = false;
	};

	/** Value-only PostSelection result shared by the worker-thread callback and focused tests. */
	struct FMotionMatchingPostSelectionState
	{
		ERpgMotionMatchingDatabaseRole CurrentDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
		EPoseSearchInterruptMode InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
		bool bIsContinuingPose = false;
		bool bShouldLatchTurnInPlace = false;
		bool bShouldLatchLanding = false;
	};

	/** High-level selector state used to preserve current poses across transient candidate-list changes. */
	struct FGroundMotionMatchingDomainState
	{
		ERpgLocomotionMovementState PhysicalMovementState = ERpgLocomotionMovementState::None;
		ERpgLocomotionGait Gait = ERpgLocomotionGait::Idle;
		ERpgLocomotionStance Stance = ERpgLocomotionStance::Standing;
		bool bChooserMoving = false;
	};

	// Source-aligned GASP Sparse chooser gates, expressed in project-native value-only state.
	static constexpr float ChooserVelocityTolerance = 0.1f;
	static constexpr float ChooserAccelerationTolerance = 0.0001f;
	static constexpr float WalkStopMinimumSpeed = 20.0f;
	static constexpr float RunStopMinimumSpeed = 100.0f;
	static constexpr float SprintStopMinimumSpeed = 550.0f;
	static constexpr float FreeRunPivotMinimumAngle = 45.0f;
	static constexpr float CombatStrafeRunPivotMinimumAngle = 30.0f;
	static constexpr float AimRunPivotMinimumAngle = 0.0f;
	static constexpr float RunStartMinimumFutureSpeedGain = 100.0f;
	static constexpr float RunStartFutureVelocityBeginTime = 0.4f;
	static constexpr float RunStartFutureVelocityEndTime = 0.5f;
	// Exact sampling arguments authored by GASP's Update_Trajectory function.
	static constexpr float TrajectoryHistorySamplingInterval = -1.0f;
	static constexpr int32 TrajectoryHistorySampleCount = 30;
	static constexpr float TrajectoryPredictionSamplingInterval = 0.1f;
	static constexpr int32 TrajectoryPredictionSampleCount = 15;

	/** Returns false for an empty trajectory or any non-finite time, position, or facing sample. */
	static bool IsTransformTrajectoryFinite(const FTransformTrajectory& Trajectory);
	/** Interpolates a bounded first-contact time between two ballistic samples and a floor plane. */
	static float CalculateTrajectoryLandingTime(
		float PreviousTime,
		float CurrentTime,
		const FVector& PreviousPosition,
		const FVector& CurrentPosition,
		const FVector& LandingLocation,
		const FVector& UpDirection);
	/** Sanitizes a walkable game-thread hit into the pointer-free airborne prediction contract. */
	static FRpgTrajectoryLandingPrediction MakeTrajectoryLandingPrediction(
		bool bCanPublishPrediction,
		bool bHasWalkableHit,
		bool bHardReset,
		float TimeToLand,
		float PredictionHorizon,
		const FVector& LandingLocation,
		const FVector& LandingNormal);
#if WITH_DEV_AUTOMATION_TESTS
	/** Runs the production collision resolver against deterministic query results for bounded automation coverage. */
	static FRpgTrajectoryLandingPrediction ResolveTrajectoryCollisionForTest(
		const FRpgTrajectoryCollisionSettings& Settings,
		const FVector& GravityAcceleration,
		bool bIsFalling,
		const FTransformTrajectory& RawTrajectory,
		const TArray<FHitResult>& QueryHits,
		const TArray<bool>& QueryWalkability,
		FTransformTrajectory& OutTrajectory,
		int32& OutWorldQueryCount,
		TArray<FVector>* OutTraceStarts = nullptr,
		TArray<FVector>* OutTraceEnds = nullptr);
#endif

	/** Mirrors GASP's logical Moving state from finite horizontal velocity and acceleration. */
	static bool IsGroundMotionMatchingChooserMoving(
		const FGroundMotionMatchingSelectionSnapshot& Snapshot);
	/** Resolves the source GASP pivot threshold for the active facing policy, in degrees. */
	static float GetRunPivotMinimumAngle(ERpgCharacterRotationMode RotationMode);
	/** Returns true only for source-level state changes that may interrupt the current continuing pose. */
	static bool ShouldInterruptGroundMotionMatching(
		bool bHasPreviousState,
		const FGroundMotionMatchingDomainState& PreviousState,
		const FGroundMotionMatchingDomainState& CurrentState);
	/** Mirrors the engine node's update-counter test and returns true after one or more missed updates. */
	static bool SynchronizeMotionMatchingNodeUpdateCounter(
		FGraphTraversalCounter& NodeUpdateCounter,
		const FGraphTraversalCounter& AnimInstanceUpdateCounter);

	/**
	 * Evaluates the pointer-free project chooser contract and returns ordered database roles.
	 * Airborne and crouching domains are resolved before the grounded Idle/Walk/Run/Sprint rows.
	 */
	static FResolvedMotionMatchingDatabaseRoles ResolveMotionMatchingDatabaseRoles(
		const FGroundMotionMatchingSelectionSnapshot& Snapshot);

	/**
	 * Selects one immutable GASP-like domain while invalid null or duplicate entries are safely omitted.
	 * Moving Run preserves source result order Starts, Loops, Pivots; logical Idle preserves the
	 * source's inclusive and overlapping Idle, Walk Stops, Run Stops, Sprint Stops row order.
	 */
	static FResolvedGroundMotionMatchingDatabases ResolveGroundMotionMatchingDatabases(
		const FGroundMotionMatchingSelectionSnapshot& Snapshot,
		const FRpgGroundMotionMatchingDatabaseSets& DatabaseSets);

	/** Checks the fixed 1/2/4/2 shape plus null and cross-set duplicate references without loading assets. */
	static FGroundMotionMatchingDatabaseSetValidation ValidateGroundMotionMatchingDatabaseSets(
		const FRpgGroundMotionMatchingDatabaseSets& DatabaseSets);

	/** Validates one unique configured database plus one expected project role/state tag per role. */
	static FMotionMatchingDatabaseRoleValidation ValidateMotionMatchingDatabaseRoleContracts(
		TConstArrayView<FMotionMatchingDatabaseRoleContract> Contracts);

	/** Resolves a runtime role only from the database's immutable project tag contract. */
	static ERpgMotionMatchingDatabaseRole ResolveMotionMatchingDatabaseRole(
		const UPoseSearchDatabase* Database);

	/** Central completed-search policy for role, Continuing Pose, interrupt, and exclusive latches. */
	static FMotionMatchingPostSelectionState ResolveMotionMatchingPostSelection(
		ERpgMotionMatchingDatabaseRole SelectedRole,
		bool bIsContinuingPose,
		EPoseSearchInterruptMode InterruptMode,
		bool bCanLatchTurnInPlace,
		bool bCanLatchLanding);

	/** Returns true for one of the six curated Idle/Walk/Run Light/Heavy landing roles. */
	static bool IsLandingDatabaseRole(ERpgMotionMatchingDatabaseRole Role);

	/** Releases a stationary landing when live grounded intent or speed no longer matches its frozen role. */
	static bool ShouldReleaseStationaryLanding(
		ERpgMotionMatchingDatabaseRole LandingRole,
		bool bHasGroundedMoveIntent,
		float GroundSpeed);

	/** Prevents a completed or cancelled landing database from surviving as an uninterruptible pose. */
	static bool ShouldInterruptLandingDatabaseExit(
		ERpgJumpPhase CurrentJumpPhase,
		bool bCompletionArmed,
		ERpgMotionMatchingDatabaseRole CurrentDatabaseRole);

	/**
	 * Resolves one requested landing role from a finite, pointer-free pre-touchdown snapshot.
	 * The 3 cm/s Idle boundary and Heavy threshold are inclusive; Sprint falls back to Run content
	 * until gameplay issue #62 supplies an authoritative Sprint state and dedicated landing assets.
	 */
	static ERpgMotionMatchingDatabaseRole ResolveLandingDatabaseRole(
		const FRpgLandingSelectionSnapshot& Snapshot,
		float HeavySpeedThreshold);

	/** Updates or resets the final-airborne snapshot from current value-only proxy inputs. */
	static void UpdateLandingSelectionSnapshot(
		FRpgAnimInstanceProxy& Proxy,
		float InputMagnitude,
		const FVector& GravityAcceleration);

	/** Applies Heavy-to-Light database fallback and returns None to resume normal gait locomotion. */
	ERpgMotionMatchingDatabaseRole ResolveAvailableLandingDatabaseRole(
		const FRpgLandingSelectionSnapshot& Snapshot) const;

	/** Builds the complete static role contract from the AnimBP defaults without loading assets. */
	FMotionMatchingDatabaseRoleContracts BuildMotionMatchingDatabaseRoleContracts() const;

	/** Returns the configured database for one role, or null when its fixed slot is invalid. */
	UPoseSearchDatabase* GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole Role) const;

	/** Value-only result used to keep Reset Root, Orientation Warping, and Steering gates independent. */
	struct FGaspProceduralGates
	{
		float ResetRootAlpha = 0.0f;
		float OrientationWarpingAlpha = 0.0f;
		bool bEnableSteering = false;
	};

	/** Database policy applied by the generic pre-update callback to the upcoming Motion Matching search. */
	enum class ETurnInPlaceSearchMode : uint8
	{
		NormalLocomotion,
		SearchRequestedTurn,
		ContinueSelectedTurn,
	};

	/** Detects a transition across the Free versus controller-facing turn-in-place policy boundary. */
	static bool DidTurnInPlaceSupportChange(
		bool bHasPreviousSnapshot,
		ERpgCharacterRotationMode PreviousMode,
		ERpgCharacterRotationMode CurrentMode);
	/** Rebases actor yaw when the owner snapshot is invalidated or the facing policy changes. */
	static float CalculateTurnInPlaceSnapshotYawDelta(
		float PreviousActorYaw,
		float CurrentActorYaw,
		bool bHardReset,
		bool bSupportChanged);
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
	/** Latches the first valid TIR SearchResult from the completed node search for the request serial. */
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

	/** Advances the pointer-free airborne/landing phase and creates at most one landing request per touchdown. */
	void UpdateJumpPhaseRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy);
	void BeginAirbornePhase(bool bAscendingTakeoff);
	void BeginLandingRequest(ERpgMotionMatchingDatabaseRole LandingRole);
	void ResetJumpPhaseRuntime();
	void ClearLandingSelection();
	void ClearBackwardJumpStartHold();
	/** Checks stable override/movement gates shared by request start and continuation. */
	bool IsLandingRuntimeEligible(const FRpgAnimInstanceProxy& Proxy) const;
	bool ConsumeLandingForceInterruptRequest();
	bool TryLatchLandingSelection(
		UAnimationAsset* SelectedAsset,
		const UPoseSearchDatabase* SelectedDatabase,
		float SelectedTime,
		bool bSelectedAssetLooping,
		uint32 SelectionRequestSerial);
	void UpdateLandingLatchedPlayback(
		UAnimationAsset* CurrentAsset,
		float CurrentAssetTime,
		float CurrentAssetLength,
		float CurrentAssetPlayRate,
		float DeltaSeconds);
	bool IsActiveLandingAsset(const UAnimationAsset* Asset) const;
	bool UpdateBackwardJumpStartHold(
		UAnimationAsset* CurrentAsset,
		float CurrentAssetTime,
		float CurrentAssetLength,
		float CurrentAssetPlayRate,
		float DeltaSeconds);
	/** Identifies curated Walk/Run/Sprint samples whose per-sample corrections must survive phase-boundary blending. */
	static bool IsGroundMovingAsset(const UAnimationAsset* Asset);
	/** Identifies every asset in the exclusive airborne database, including Jump/Starts and the looping fall. */
	static bool IsAirborneJumpAsset(const UAnimationAsset* Asset);
	/** Identifies the non-looping Jump/Starts subset that additionally receives authored OW and Steering. */
	static bool IsAirborneJumpStartAsset(const UAnimationAsset* Asset);
	/** Identifies the two backward Jump/Starts clips whose short transition block otherwise restarts mid-air. */
	static bool IsBackwardJumpStartAsset(const UAnimationAsset* Asset);
	/** Identifies the looping fall continuation, which must never search back into a directional start. */
	static bool IsLoopingAirborneFallAsset(const UAnimationAsset* Asset);
	/** Retains a fall loop only after this airborne phase actually used the bounded backward-start path. */
	static bool ShouldHoldLoopingAirborneFallPlayback(
		ERpgJumpPhase CurrentJumpPhase,
		bool bBackwardHoldWasArmed,
		float CurrentVerticalVelocity,
		bool bCurrentAssetIsLoopingFall);
	/** Bounds a continuing-pose-only backward start while retaining a fail-open path for genuine long falls. */
	static bool ShouldHoldBackwardJumpStartPlayback(
		ERpgJumpPhase CurrentJumpPhase,
		bool bCurrentAssetMatchesHeldSelection,
		float CurrentAssetTime,
		float CurrentAssetLength,
		float CurrentAssetPlayRate,
		float HoldElapsed);
	static FGaspProceduralGates ResolveGaspProceduralGates(
		bool bGroundMovingPose,
		float SupportedLocomotionAlpha,
		bool bAirborneJumpPose,
		bool bAirborneJumpStartPose,
		bool bLandingPose,
		float EnableWarpingCurveValue,
		bool bHasActiveBlendStackAsset,
		bool bHasTrajectory);

	float TurnInPlaceStableElapsed = 0.0f;
	float TurnInPlaceSelectionElapsed = 0.0f;
	/** Remaining full-sequence playback time for the latched cosmetic turn, in seconds. */
	float TurnInPlaceSelectedAssetRemainingTime = 0.0f;
	/** PoseSearch-selected start time copied from the completed PostSelection result, in seconds. */
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
	/** Per-reason mask used to emit one reset for each newly active hard condition without airborne masking. */
	uint8 TurnInPlaceHardResetReasonsLastFrame = 0;
	bool bTurnInPlaceInitializationResetPending = false;

	/** Exact cosmetic landing chosen for the active request; Pose Search and Blend Stack retain ownership. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> LandingSelectedAsset;

	float LandingSelectedAssetStartTime = 0.0f;
	float LandingSelectedAssetRemainingTime = 0.0f;
	float LandingPlaybackWatchdogDuration = 0.0f;
	uint32 LandingRequestSerial = 0;
	uint32 LandingInterruptedRequestSerial = 0;
	uint32 LandingSelectedRequestSerial = 0;
	bool bLandingSelectedAssetLooping = false;
	bool bLandingPlaybackObserved = false;
	bool bLandingCompletionArmed = false;

	/** Exact initial backward Jump/Starts asset retained as a Continuing Pose for an ordinary short jump. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> BackwardJumpStartHeldAsset;

	/** Wall-clock duration of the bounded backward-start hold, in seconds. */
	float BackwardJumpStartHoldElapsed = 0.0f;
	/** Prevents a released backward start from being latched again during the same airborne phase. */
	bool bBackwardJumpStartHoldOpportunityConsumed = false;
	/** Takeoff provenance captured on the movement-mode edge, before a delayed MM result is observed. */
	bool bBackwardJumpStartHoldEligible = false;
	/** Scopes fall-loop continuation to an airborne phase that actually held a backward start. */
	bool bBackwardJumpStartHoldWasArmed = false;

	/** Previous source-level selector domain, owned and mutated only by the animation update thread. */
	FGroundMotionMatchingDomainState PreviousGroundMotionMatchingDomainState;
	bool bHasPreviousGroundMotionMatchingDomainState = false;
	/** Interrupt mode supplied during pre-selection and captured with the completed result. */
	EPoseSearchInterruptMode PendingMotionMatchingInterruptMode =
		EPoseSearchInterruptMode::DoNotInterrupt;
	/** Last AnimInstance update seen by the Motion Matching callback, used to mirror node relevancy resets. */
	FGraphTraversalCounter MotionMatchingNodeUpdateCounter;

	friend struct FRpgAnimInstanceProxy;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgJumpPhaseRuntimeTest;
	friend class FRpgLandingSelectionRuntimeTest;
	friend class FRpgMotionMatchingDatabaseResolverTest;
	friend class FRpgTrajectoryCollisionRuntimeTest;
	friend class FRpgTurnInPlaceStateMachineTest;
#endif
};
