// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RpgFootPlacementTypes.generated.h"

/** Static per-leg names used by the game-thread foot-placement sampler. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgFootPlacementLegDefinition
{
	GENERATED_BODY()

	/** FK ankle bone used by the downstream Leg IK solver. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FName FKFootBone = NAME_None;

	/** IK target bone moved by the project-local foot-placement node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FName IKFootBone = NAME_None;

	/** Toe/ball bone whose previous evaluated world position seeds the ground sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FName BallBone = NAME_None;

	/** Previous-frame GASP contact curve; the game thread remaps it to the pseudo-speed used for planting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FName SpeedCurveName = NAME_None;
};

/**
 * Designer-tuned game-thread trace and lock policy for project-local foot placement.
 *
 * The settings are static AnimBP defaults. Runtime traces remain cosmetic and never
 * influence authoritative character movement or collision.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgFootPlacementSettings
{
	GENERATED_BODY()

	FRpgFootPlacementSettings();

	/** Enables game-thread ground sampling for this AnimBP; disabled by default for legacy graphs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bEnabled = false;

	/** Opts crouch clips into procedural placement; false preserves the validated authored crouch pose. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bApplyWhileCrouching = false;

	/** Left mannequin leg sampled on the game thread. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FRpgFootPlacementLegDefinition LeftLeg;

	/** Right mannequin leg sampled on the game thread. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FRpgFootPlacementLegDefinition RightLeg;

	/** Height above the ball bone at which each downward sweep starts, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceStartHeight = 75.0f;

	/** Distance below the ball bone covered by each downward sweep, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceEndDepth = 100.0f;

	/** Radius of the game-thread sphere sweep, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float SweepRadius = 5.0f;

	/** Foot speed below which a nearby walkable surface may establish a plant lock, in centimeters per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PlantSpeedThreshold = 60.0f;

	/** Foot speed above which ground-alignment weight is fully released, in centimeters per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float UnalignmentSpeedThreshold = 200.0f;

	/** Maximum ball-to-plane distance that may establish a plant lock, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float PlantDistanceThreshold = 10.0f;

	/** Maximum distance between a planted target and the animated foot before the lock releases, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float UnplantRadius = 20.0f;

	/** Fraction of UnplantRadius below which an already-requested contact may replant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReplantRadiusRatio = 0.2f;

	/** Maximum ground-normal change retained by a plant lock, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float UnplantAngle = 60.0f;

	/** Fraction of UnplantAngle below which an already-requested contact may replant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReplantAngleRatio = 0.2f;

	/** Maximum slope-alignment rotation authored into a locked IK target, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float MaxFootAlignmentAngle = 60.0f;

	/** Maximum world-space correction authored into a locked IK target, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxFootTranslation = 50.0f;

	/** Maximum time a planted foot keeps its last plane across a transient trace miss, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", Units = "s"))
	float TraceMissGracePeriod = 0.10f;

	/** Half-life used to blend per-foot placement weights after valid grounded sampling begins, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.001", Units = "s"))
	float WeightBlendHalfLife = 0.08f;
};

/** Pointer-free result of one game-thread foot sweep and plant-lock update. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgFootPlacementLegSnapshot
{
	GENERATED_BODY()

	/** Walkable plane point in world space. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FVector GroundPointWorld = FVector::ZeroVector;

	/** Normalized walkable plane normal in world space. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FVector GroundNormalWorld = FVector::UpVector;

	/** World-space IK target retained while this foot is planted. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FTransform LockedFootTransformWorld = FTransform::Identity;

	/** Per-leg placement weight in the range [0, 1]. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Weight = 0.0f;

	/** Distance from the animated ball bone to the sampled plane, in centimeters. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement", meta = (Units = "cm"))
	float DistanceToGround = 0.0f;

	/** Stable runtime identifier of the hit component; zero means no component was retained. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	int64 HitComponentId = 0;

	/** True while a walkable surface, planted grace plane, or fading last-valid target remains usable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bHasWalkableGround = false;

	/** True when the foot owns a world-space plant lock. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bLocked = false;

	/** True when the active animation supplied the authored speed curve required for safe locking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bHasSpeedCurve = false;
};

/**
 * Immutable game-thread foot-placement snapshot consumed by parallel animation evaluation.
 *
 * This value deliberately contains no UObject, FHitResult, world, actor, or movement-component
 * references. Authority, autonomous proxies, and simulated proxies each build a cosmetic local copy.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgFootPlacementSnapshot
{
	GENERATED_BODY()

	/** Mesh component transform used to convert the stored world-space planes during this update. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FTransform ComponentToWorld = FTransform::Identity;

	/** Mesh component translation delta since the previous valid game-thread sample. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FVector ComponentDeltaWorld = FVector::ZeroVector;

	/** Translation contributed by the current movement base since the previous sample. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FVector BaseDeltaTranslationWorld = FVector::ZeroVector;

	/** Rotation contributed by the current movement base since the previous sample. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FQuat BaseDeltaRotationWorld = FQuat::Identity;

	/** Character velocity captured on the game thread for debugging and deterministic tests. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FVector VelocityWorld = FVector::ZeroVector;

	/** Current CharacterMovement floor point, flattened from CurrentFloor on the game thread. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FVector FloorPointWorld = FVector::ZeroVector;

	/** Current CharacterMovement floor normal, flattened from CurrentFloor on the game thread. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FVector FloorNormalWorld = FVector::UpVector;

	/** Left-foot ground sample. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FRpgFootPlacementLegSnapshot LeftFoot;

	/** Right-foot ground sample. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	FRpgFootPlacementLegSnapshot RightFoot;

	/** Owner identifier used to diagnose role/possession rebases without carrying an object pointer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	int64 OwnerId = 0;

	/** Movement-base identifier used to diagnose lock rebases without carrying an object pointer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	int64 MovementBaseId = 0;

	/** Local role flattened on the game thread for network-role validation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	uint8 LocalRole = 0;

	/** Remote role flattened on the game thread for network-role validation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	uint8 RemoteRole = 0;

	/** True while CharacterMovement has a valid grounded walking state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bGrounded = false;

	/** True when teleport, owner/role, base, or large-transform changes require lock state to be discarded. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bReset = true;

	/** True when both the static configuration and current game-thread owner snapshot are usable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Foot Placement")
	bool bValid = false;
};

namespace RpgFootPlacement
{
	/** Converts a normalized GASP contact curve into the pseudo-speed expected by the plant policy. */
	SURVIVALRPG_API float ConvertContactCurveToSpeed(float ContactCurveValue);

	/** Returns an exponential interpolation alpha for the supplied half-life. */
	SURVIVALRPG_API float CalculateHalfLifeAlpha(float DeltaSeconds, float HalfLifeSeconds);

	/** Maps the pseudo-speed range to the procedural alignment weight used by Epic's Foot Placement policy. */
	SURVIVALRPG_API float CalculateAlignmentAlpha(
		float FootSpeed,
		const FRpgFootPlacementSettings& Settings);

	/** Returns whether a walkable sample is close and slow enough to establish a foot lock. */
	SURVIVALRPG_API bool ShouldPlantFoot(
		bool bHasWalkableGround,
		float FootSpeed,
		float DistanceToGround,
		const FRpgFootPlacementSettings& Settings);

	/** Returns whether an existing lock must release because the animated foot or ground changed too far. */
	SURVIVALRPG_API bool ShouldUnplantFoot(
		float FootSpeed,
		float AnchorDistance,
		float GroundNormalDeltaDegrees,
		const FRpgFootPlacementSettings& Settings);

	/** Returns whether an uninterrupted plant request has returned inside the tighter replant bounds. */
	SURVIVALRPG_API bool ShouldReplantFoot(
		float AnchorDistance,
		float GroundNormalDeltaDegrees,
		const FRpgFootPlacementSettings& Settings);

	/** Rebases a world transform through a moving surface's previous and current transforms. */
	SURVIVALRPG_API FTransform RebaseTransformThroughSurface(
		const FTransform& TransformWorld,
		const FTransform& PreviousSurfaceTransform,
		const FTransform& CurrentSurfaceTransform);

	/** Rebases a world point through a moving surface's previous and current transforms. */
	SURVIVALRPG_API FVector RebasePointThroughSurface(
		const FVector& PointWorld,
		const FTransform& PreviousSurfaceTransform,
		const FTransform& CurrentSurfaceTransform);

	/** Rebases a world normal through a moving surface's previous and current transforms. */
	SURVIVALRPG_API FVector RebaseNormalThroughSurface(
		const FVector& NormalWorld,
		const FTransform& PreviousSurfaceTransform,
		const FTransform& CurrentSurfaceTransform);

	/** Aligns an IK-foot transform to a plane while pivoting around the current ball transform. */
	SURVIVALRPG_API FTransform AlignFootToGroundPlane(
		const FTransform& IKFootTransformWorld,
		const FTransform& BallTransformWorld,
		const FVector& GroundPointWorld,
		const FVector& GroundNormalWorld,
		const FVector& ComponentUpWorld,
		float MaxTranslationOffset,
		float MaxRotationDegrees);

	/**
	 * Reconstructs the ball transform that belongs to an IK-foot target.
	 *
	 * The authored FK-foot-to-ball relationship is transferred onto the IK foot, matching
	 * Epic's Foot Placement input contract without assuming that FK and IK bones overlap.
	 */
	SURVIVALRPG_API FTransform DeriveIKBallTransform(
		const FTransform& FKFootTransform,
		const FTransform& BallTransform,
		const FTransform& IKFootTransform);

	/** Rebuilds a planted IK foot while retaining the authored pivot around its current ball. */
	SURVIVALRPG_API FTransform PivotFootAroundBall(
		const FTransform& IKFootTransform,
		const FTransform& IKBallTransform,
		const FTransform& LockedFootTransform);

	/**
	 * Returns the stateless raw-pose gate shared by IK output and pelvis contribution.
	 *
	 * Ball distance is always enforced. Planar drift is additionally enforced for a lock.
	 * Each bound stays fully weighted through its inner half and smoothly reaches zero at
	 * the configured GASP limit; values at or beyond that limit are guaranteed to return zero.
	 */
	SURVIVALRPG_API float CalculateGeometryWeight(
		float BallDistanceToPlane,
		float PlanarLockDrift,
		bool bLocked,
		float PlantDistanceThreshold,
		float UnplantRadius);

	/** Combines per-leg availability, snapshot weight, and raw-pose geometry into a safe [0, 1] weight. */
	SURVIVALRPG_API float CalculateEffectivePlacementWeight(
		bool bHasWalkableGround,
		float SnapshotWeight,
		float GeometryWeight);

	/**
	 * Resolves the IK target consumed by stock Leg IK from the same-frame FK ankle baseline.
	 *
	 * A zero placement weight returns the FK ankle exactly, so a downstream global-alpha Leg IK
	 * can never pin a swing leg to an unrelated or static authored IK track.
	 */
	SURVIVALRPG_API FTransform ResolveIKFootTarget(
		const FTransform& FKFootTransform,
		const FTransform& ProceduralTargetTransform,
		float EffectivePlacementWeight);

	/** Chooses the bounded downward pelvis correction required by the sampled feet. */
	SURVIVALRPG_API float CalculatePelvisOffset(float LeftOffset, float RightOffset, float MaxDownwardOffset);

	/** Smooths a pelvis target with a frame-rate-independent half-life and a hard speed bound. */
	SURVIVALRPG_API float SmoothPelvisOffset(
		float CurrentOffset,
		float TargetOffset,
		float DeltaSeconds,
		float HalfLifeSeconds,
		float MaxSpeed);
}
