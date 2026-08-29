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
#include "RpgCombatAnimationProfile.h"
#include "RpgFootPlacementTypes.h"
#include "RpgGaspPresentationProfile.h"
#include "RpgJumpRuntime.h"
#include "RpgLandingRuntime.h"
#include "RpgMotionMatchingRuntime.h"
#include "RpgPoseSearchTrajectory.h"
#include "RpgTurnInPlaceRuntime.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementProfile.h"
#include "SurvivalRpg/Core/Character/RpgCharacterRotationMode.h"
#include "RpgAnimInstance.generated.h"

class UAbilitySystemComponent;
class UAnimationAsset;
class UAnimSequence;
class UPoseSearchDatabase;
#if WITH_DEV_AUTOMATION_TESTS
class FRpgJumpPhaseRuntimeTest;
class FRpgLandingSelectionRuntimeTest;
class FRpgMotionMatchingDatabaseResolverTest;
class FRpgTrajectoryCollisionRuntimeTest;
class FRpgTurnInPlaceStateMachineTest;
#endif

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

	/** Raw pre-touchdown intent used to capture desired gait; physical horizontal speed still owns the landing domain. */
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
	/** Active immutable upper-body asset selected from replicated equipment traits on the game thread. */
	UAnimSequence* CombatEquippedUpperBodyAnimation = nullptr;
	/** Combat-ready counterpart selected from the same immutable profile. */
	UAnimSequence* CombatReadyUpperBodyAnimation = nullptr;
	/** Stable profile identity exposed only for cosmetic diagnostics and network tests. */
	FName CombatAnimationProfileName = NAME_None;
	/** Current equip/profile blend weight advanced exclusively by game-thread PreUpdate. */
	float CombatAnimationOverlayAlpha = 0.0f;
	/** Designer-authored Free-to-combat-ready crossfade duration. */
	float CombatModeBlendTime = 0.0f;
	/** Replicated rotation-mode projection; true for CombatStrafe and Aim. */
	bool bCombatAnimationReady = false;
	/** True when unknown, ambiguous, or empty equipment resolved to the Unarmed fallback. */
	bool bCombatAnimationProfileFallback = true;
	bool bHasTurnInPlaceBlockingGameplayTag = false;
	bool bTurnInPlaceHardReset = true;
	/** Number of unified presentation-history resets emitted by game-thread snapshots. */
	int32 AnimationHistoryResetCount = 0;
	/** True when the game-thread snapshot crosses between free-facing and turn-in-place-capable rotation. */
	bool bTurnInPlaceSupportChanged = false;

	// Persistent pre-touchdown state is authored only by game-thread PreUpdate.
	ERpgLocomotionGait LastGroundedGait = ERpgLocomotionGait::Idle;
	int32 LandingAirborneEpoch = 0;
	bool bWasAirborneForLanding = false;
	/** Game-thread acknowledgement gate that prevents repeated PreUpdate calls from erasing touchdown context. */
	bool bLandingTouchdownPendingConsumption = false;

	// Previous-owner data is maintained and consumed only by game-thread PreUpdate.
	uint32 PreviousOwnerUniqueId = 0;
	uint8 PreviousLocalRole = 0;
	uint8 PreviousRemoteRole = 0;
	float PreviousActorYaw = 0.0f;
	FVector PreviousActorLocation = FVector::ZeroVector;
	uint32 PreviousAnimationDiscontinuitySerial = 0;
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

	// Persistent combat-presentation transition state is authored only by game-thread PreUpdate.
	FRpgResolvedCombatAnimationProfile ActiveCombatAnimationProfile;
	FRpgResolvedCombatAnimationProfile PendingCombatAnimationProfile;
	bool bHasPendingCombatAnimationProfile = false;
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

	/** Returns the project-owned role tag resolved once while building a profile database cache. */
	static FName GetMotionMatchingDatabaseRoleTag(ERpgMotionMatchingDatabaseRole Role);

	/** Returns legacy state metadata retained for compatibility diagnostics, not runtime selection. */
	static FName GetMotionMatchingDatabaseStateTag(ERpgMotionMatchingDatabaseRole Role);

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
	 * Legacy serialized grounded database fallback used only when the bound profile has no database set.
	 * Profile-bound runtimes never merge or read these references during parallel evaluation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	FRpgGroundMotionMatchingDatabaseSets GroundMotionMatchingDatabaseSets;

	/**
	 * Legacy serialized crouching database fallback used only when profile database mapping is absent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> CrouchingMotionMatchingDatabase;

	/**
	 * Legacy serialized turn-in-place database fallback used only when profile mapping is absent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> TurnInPlaceMotionMatchingDatabase;

	/**
	 * Legacy serialized airborne database fallback used only when profile mapping is absent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> AirborneMotionMatchingDatabases;

	/**
	 * Designer-owned GASP presentation membership, complete runtime database set, and cosmetic feel.
	 * Static profile data is snapshotted into worker-safe lookups and values during initialization.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Presentation")
	TObjectPtr<URpgGaspPresentationProfile> GaspPresentationProfile;

	/**
	 * Designer-owned combat upper-body profile resolved from replicated MainHand/OffHand traits.
	 * The profile is cosmetic only; Equipment, GAS, and Character rotation remain authoritative.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	TObjectPtr<URpgCombatAnimationProfile> CombatAnimationProfile;

	/**
	 * Legacy serialized stand-idle Light landing fallback. This preserves the #66 property name and
	 * exact four-clip contract while a valid profile is the active runtime source.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> LandingMotionMatchingDatabase;

	/** Legacy serialized stand-idle Heavy landing fallback retained for AnimBP compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> StandHeavyLandingMotionMatchingDatabase;

	/** Legacy serialized Walk Light landing fallback retained for AnimBP compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> WalkLightLandingMotionMatchingDatabase;

	/** Legacy serialized Walk Heavy landing fallback retained for AnimBP compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> WalkHeavyLandingMotionMatchingDatabase;

	/** Legacy serialized Run Light landing fallback retained for AnimBP compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> RunLightLandingMotionMatchingDatabase;

	/** Legacy serialized Run Heavy landing fallback retained for AnimBP compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TObjectPtr<UPoseSearchDatabase> RunHeavyLandingMotionMatchingDatabase;

	/**
	 * Legacy serialized Heavy-landing boundary used only by the whole-legacy fallback.
	 * GASP authors 700 cm/s; the comparison remains inclusive and cosmetic.
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

	/**
	 * Monotonic local diagnostic count for unified trajectory, landing, foot-placement, and turn-in-place history resets.
	 * It is presentation-only and is not authoritative or replicated.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Diagnostics")
	int32 AnimationHistoryResetCount = 0;

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
	 * Signed movement angle relative to the network-smoothed presentation forward axis.
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

	/** Current landing role; a stationary role may hand off once to same-severity Walk/Run presentation. */
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

	/** Equipped/relaxed upper-body loop selected on the game thread from immutable profile data. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Combat")
	TObjectPtr<UAnimSequence> CombatEquippedUpperBodyAnimation;

	/** CombatStrafe/Aim upper-body loop paired with CombatEquippedUpperBodyAnimation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Combat")
	TObjectPtr<UAnimSequence> CombatReadyUpperBodyAnimation;

	/** Stable name of the selected cosmetic profile; useful for authority/proxy/late-join diagnostics. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Combat")
	FName CombatAnimationProfileName = NAME_None;

	/** Smoothed weight applied to the masked upper-body overlay. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CombatAnimationOverlayAlpha = 0.0f;

	/** Current designer-authored Free-to-combat-ready pose crossfade duration, in seconds. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Combat", meta = (ClampMin = "0.0", Units = "s"))
	float CombatModeBlendTime = 0.0f;

	/** True when replicated rotation policy is CombatStrafe or Aim. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Combat")
	bool bCombatAnimationReady = false;

	/** True when the deterministic Unarmed fallback owns the current presentation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Combat")
	bool bCombatAnimationProfileFallback = true;

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

	/** Integrity summary for the whole-legacy role/state-tag compatibility configuration. */
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
	using FMotionMatchingDatabaseRoleContracts =
		TArray<FMotionMatchingDatabaseRoleContract, TInlineAllocator<18>>;

	/** Mirrors the engine node's update-counter test and returns true after one or more missed updates. */
	static bool SynchronizeMotionMatchingNodeUpdateCounter(
		FGraphTraversalCounter& NodeUpdateCounter,
		const FGraphTraversalCounter& AnimInstanceUpdateCounter);

	/**
	 * Selects one immutable GASP-like domain while invalid null or duplicate entries are safely omitted.
	 * Moving Run preserves source result order Starts, Loops, Pivots; logical Idle preserves the
	 * source's inclusive and overlapping Idle, Walk Stops, Run Stops, Sprint Stops row order.
	 */
	static FResolvedGroundMotionMatchingDatabases ResolveGroundMotionMatchingDatabases(
		const FRpgGroundMotionMatchingSelectionSnapshot& Snapshot,
		const FRpgGroundMotionMatchingDatabaseSets& DatabaseSets);

	/** Checks the fixed 1/2/4/2 shape plus null and cross-set duplicate references without loading assets. */
	static FGroundMotionMatchingDatabaseSetValidation ValidateGroundMotionMatchingDatabaseSets(
		const FRpgGroundMotionMatchingDatabaseSets& DatabaseSets);

	/** Validates the legacy fixed slots plus their historical role/state metadata. */
	static FMotionMatchingDatabaseRoleValidation ValidateMotionMatchingDatabaseRoleContracts(
		TConstArrayView<FMotionMatchingDatabaseRoleContract> Contracts);

	/** Resolves one role tag for whole-legacy validation and focused compatibility diagnostics. */
	static ERpgMotionMatchingDatabaseRole ResolveMotionMatchingDatabaseRole(
		const UPoseSearchDatabase* Database);

	/** Resolves only databases active in the selected whole-profile or whole-legacy configuration. */
	ERpgMotionMatchingDatabaseRole ResolveConfiguredMotionMatchingDatabaseRole(
		const UPoseSearchDatabase* Database) const;

	/** Updates or resets the final-airborne snapshot from current value-only proxy inputs. */
	static void UpdateLandingSelectionSnapshot(
		FRpgAnimInstanceProxy& Proxy,
		ERpgLocomotionGait DesiredGait,
		const FVector& GravityAcceleration,
		const FRpgGaspLocomotionTuning& Tuning = FRpgGaspLocomotionTuning());

	/** Applies Heavy-to-Light database fallback to an already resolved landing role. */
	ERpgMotionMatchingDatabaseRole ResolveAvailableLandingDatabaseRole(
		ERpgMotionMatchingDatabaseRole RequestedRole) const;

	/** Resolves a snapshot, applies Heavy-to-Light fallback, or returns None for normal locomotion. */
	ERpgMotionMatchingDatabaseRole ResolveAvailableLandingDatabaseRole(
		const FRpgLandingSelectionSnapshot& Snapshot) const;

	/** Builds the complete static role contract from the AnimBP defaults without loading assets. */
	FMotionMatchingDatabaseRoleContracts BuildMotionMatchingDatabaseRoleContracts() const;

	/** Returns one reflected whole-legacy database slot without consulting runtime cache state. */
	UPoseSearchDatabase* GetLegacyMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole Role) const;

	/** Returns the configured database for one role from the immutable runtime cache. */
	UPoseSearchDatabase* GetMotionMatchingDatabaseForRole(
		ERpgMotionMatchingDatabaseRole Role) const;

	/** True when a non-empty profile database set selects the all-profile configuration mode. */
	bool UsesProfileRuntimeConfiguration() const;

	/** Builds immutable presentation/database caches and selects the atomic profile or legacy mode. */
	void InitializeGaspRuntimeConfiguration();

	/** Value-only result used to keep Reset Root, Orientation Warping, and Steering gates independent. */
	struct FGaspProceduralGates
	{
		float ResetRootAlpha = 0.0f;
		float OrientationWarpingAlpha = 0.0f;
		bool bEnableSteering = false;
	};

	/** Captures the flat compatibility fields for one pointer-free turn-in-place decision. */
	FRpgTurnInPlaceRuntimeState CaptureTurnInPlaceRuntimeState() const;
	/** Applies one value result without moving the GC-tracked selection out of this facade. */
	void ApplyTurnInPlaceRuntimeResult(const FRpgTurnInPlaceUpdateResult& Result);
	void UpdateTurnInPlaceRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy);
	void BeginTurnInPlaceRequest(float QuantizedAngle);
	void BeginTurnInPlaceRecovery(bool bHardResetOffset);
	void ResetTurnInPlaceRuntime(bool bHardResetOffset);
	/** Clears only the cosmetic selection/playback latch; request serials remain monotonic. */
	void ClearTurnInPlaceSelection();
	bool ConsumeTurnInPlaceForceInterruptRequest();
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
	/** Advances landing state and permits only the initial request plus one bounded stationary-to-moving handoff. */
	void UpdateJumpPhaseRuntime(float DeltaSeconds, const FRpgAnimInstanceProxy& Proxy);
	void BeginAirbornePhase(bool bAscendingTakeoff);
	void BeginLandingRequest(
		ERpgMotionMatchingDatabaseRole LandingRole,
		bool bForceInterrupt = true);
	void ResetJumpPhaseRuntime();
	void ClearLandingSelection();
	void ClearBackwardJumpStartHold();
	/** Captures the flat compatible landing facade for one pointer-free lifecycle decision. */
	FRpgLandingRuntimeState CaptureLandingRuntimeState() const;
	/** Applies value state and explicit phase/GC cleanup actions returned by the landing runtime. */
	void ApplyLandingRuntimeResult(const FRpgLandingRuntimeResult& Result);
	/** Snapshots the six fixed database pointers as pointer-free availability flags. */
	FRpgLandingDatabaseAvailability BuildLandingDatabaseAvailability() const;
	/** Copies stable grounded presentation gates without exposing proxy or UObject state. */
	static FRpgLandingEligibilitySnapshot BuildLandingEligibilitySnapshot(
		const FRpgAnimInstanceProxy& Proxy);
	/** Captures the pointer-free backward-start state while the held asset remains GC-owned here. */
	FRpgBackwardJumpStartHoldState CaptureBackwardJumpStartHoldState() const;
	/** Applies value state and explicit capture/release actions around the held-asset pointer. */
	void ApplyBackwardJumpStartHoldResult(
		const FRpgBackwardJumpStartHoldResult& Result,
		UAnimationAsset* CurrentAsset);
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
	/** Identifies curated Landing samples independently of request lifetime so outgoing blends keep Reset Root. */
	bool IsLandingAsset(const UAnimationAsset* Asset) const;
	bool UpdateBackwardJumpStartHold(
		UAnimationAsset* CurrentAsset,
		float CurrentAssetTime,
		float CurrentAssetLength,
		float CurrentAssetPlayRate,
		float DeltaSeconds);
	/** Identifies curated Walk/Run/Sprint samples whose per-sample corrections must survive phase-boundary blending. */
	bool IsGroundMovingAsset(const UAnimationAsset* Asset) const;
	/** Identifies every asset in the exclusive airborne database, including Jump/Starts and the looping fall. */
	bool IsAirborneJumpAsset(const UAnimationAsset* Asset) const;
	/** Identifies the non-looping Jump/Starts subset that additionally receives authored OW and Steering. */
	bool IsAirborneJumpStartAsset(const UAnimationAsset* Asset) const;
	/** Identifies the two backward Jump/Starts clips whose short transition block otherwise restarts mid-air. */
	bool IsBackwardJumpStartAsset(const UAnimationAsset* Asset) const;
	/** Identifies the looping fall continuation, which must never search back into a directional start. */
	bool IsLoopingAirborneFallAsset(const UAnimationAsset* Asset) const;
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
	/** Wall-clock age of the physical touchdown, preserved across one Stand-to-moving landing handoff. */
	float LandingTouchdownElapsed = 0.0f;
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
	FRpgGroundMotionMatchingDomainState PreviousGroundMotionMatchingDomainState;
	bool bHasPreviousGroundMotionMatchingDomainState = false;
	/** Interrupt mode supplied during pre-selection and captured with the completed result. */
	EPoseSearchInterruptMode PendingMotionMatchingInterruptMode =
		EPoseSearchInterruptMode::DoNotInterrupt;
	/** Last AnimInstance update seen by the Motion Matching callback, used to mirror node relevancy resets. */
	FGraphTraversalCounter MotionMatchingNodeUpdateCounter;

	/** Immutable presentation traits built on the game thread from GaspPresentationProfile. */
	FRpgGaspPresentationAssetLookup GaspPresentationAssetLookup;
	/** Immutable combat overlay selection copied from CombatAnimationProfile on initialization. */
	FRpgCombatAnimationProfileLookup CombatAnimationProfileLookup;
	/** Immutable bidirectional database mapping built from the selected profile or legacy facade. */
	FRpgGaspMotionMatchingDatabaseLookup GaspMotionMatchingDatabaseLookup;
	/** Pointer-free cosmetic feel copied once for game-thread proxy and worker-thread runtime use. */
	FRpgGaspLocomotionTuning RuntimeGaspLocomotionTuning;
	/** Whole-profile versus whole-legacy mode selected once before parallel animation updates. */
	bool bUseProfileRuntimeConfiguration = false;

	friend struct FRpgAnimInstanceProxy;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgJumpPhaseRuntimeTest;
	friend class FRpgLandingSelectionRuntimeTest;
	friend class FRpgMotionMatchingDatabaseResolverTest;
	friend class FRpgTrajectoryCollisionRuntimeTest;
	friend class FRpgTurnInPlaceStateMachineTest;
#endif
};
