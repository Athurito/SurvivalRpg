// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "RpgGaspLocomotionConfig.h"
#include "RpgGaspPresentationProfile.generated.h"

class UAnimationAsset;
class UAnimSequenceBase;
class UPoseSearchDatabase;

/**
 * Presentation category assigned to one curated GASP animation sequence.
 *
 * Categories are designer-authored static data. Runtime code converts them into an immutable
 * pointer-keyed trait cache before parallel animation work begins.
 */
UENUM(BlueprintType)
enum class ERpgGaspPresentationAssetCategory : uint8
{
	None,
	GroundMoving,
	JumpStart,
	BackwardJumpStart,
	AirborneFall,
	Landing,
};

/** One explicit sequence-to-presentation-category mapping owned by a GASP presentation profile. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgGaspPresentationAssetMembership
{
	GENERATED_BODY()

	/** Curated animation sequence kept alive by this static designer-authored profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Presentation")
	TObjectPtr<UAnimSequenceBase> Asset;

	/** Cosmetic procedural category consumed by the worker-safe AnimInstance lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Presentation")
	ERpgGaspPresentationAssetCategory Category = ERpgGaspPresentationAssetCategory::None;
};

/** Designer-authored point at which a completed footstep may release its cosmetic turn selection. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgTurnInPlaceClipTiming
{
	GENERATED_BODY()

	/** Non-looping member of the profile's StandTurnInPlace database; held alive by the profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Turn In Place")
	TObjectPtr<UAnimSequenceBase> Asset;

	/** Safe release point in animation seconds, after the authored turn and foot placement settle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Turn In Place", meta = (ClampMin = "0.0", Units = "s"))
	float ReentryTimeSeconds = 0.0f;
};

/** Value-only integrity result shared by editor validation and focused automation tests. */
struct SURVIVALRPG_API FRpgGaspPresentationProfileValidation
{
	bool bIsEmpty = false;
	bool bHasNullAsset = false;
	bool bHasDuplicateAsset = false;
	bool bHasUnassignedCategory = false;
	bool bHasLoopingJumpStart = false;
	bool bHasNonLoopingAirborneFall = false;
	bool bHasLoopingLanding = false;
	bool bHasEmptyRuntimeDatabases = false;
	bool bHasNullRuntimeDatabase = false;
	bool bHasDuplicateRuntimeDatabase = false;
	bool bHasRuntimeDatabaseWithoutAssets = false;
	bool bHasInvalidRuntimeDatabaseRoleTag = false;
	bool bHasDuplicateRuntimeDatabaseRole = false;
	bool bHasMissingRuntimeDatabaseRole = false;
	bool bHasGroundMovingCoverageMismatch = false;
	bool bHasAirborneCoverageMismatch = false;
	bool bHasLandingCoverageMismatch = false;
	bool bHasInvalidTuning = false;
	bool bHasInvalidTurnInPlaceTiming = false;
	bool bHasTurnInPlaceTimingCoverageMismatch = false;

	bool IsTurnInPlaceTimingValid() const
	{
		return !bHasInvalidTurnInPlaceTiming && !bHasTurnInPlaceTimingCoverageMismatch;
	}

	bool IsMembershipValid() const
	{
		return !bIsEmpty && !bHasNullAsset && !bHasDuplicateAsset &&
			!bHasUnassignedCategory && !bHasLoopingJumpStart &&
			!bHasNonLoopingAirborneFall && !bHasLoopingLanding;
	}

	bool IsRuntimeDatabaseConfigValid() const
	{
		return !bHasEmptyRuntimeDatabases && !bHasNullRuntimeDatabase &&
			!bHasDuplicateRuntimeDatabase && !bHasRuntimeDatabaseWithoutAssets &&
			!bHasInvalidRuntimeDatabaseRoleTag &&
			!bHasDuplicateRuntimeDatabaseRole && !bHasMissingRuntimeDatabaseRole &&
			!bHasGroundMovingCoverageMismatch && !bHasAirborneCoverageMismatch &&
			!bHasLandingCoverageMismatch && IsTurnInPlaceTimingValid();
	}

	bool IsValid() const
	{
		return IsMembershipValid() && IsRuntimeDatabaseConfigValid() && !bHasInvalidTuning;
	}
};

/**
 * Designer-owned membership for the project-curated GASP procedural presentation contract.
 *
 * The profile is a hard-referenced content boundary: it neither owns gameplay state nor mutates at
 * runtime. `URpgAnimInstance` validates and snapshots it on the game thread, then worker-thread
 * callbacks consume only the resulting immutable pointer-to-traits cache.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgGaspPresentationProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Complete curated sequence membership used by Reset Root, Orientation Warping, Steering, and
	 * the bounded backward-jump hold. Entries must be unique and use loop-compatible categories.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Presentation")
	TArray<FRpgGaspPresentationAssetMembership> AssetMemberships;

	/**
	 * Complete unordered runtime Pose Search database set for this profile.
	 * Each database must carry exactly one unique `Rpg.MotionMatching.Role.*` tag; hard references
	 * make the profile the deterministic load/cook root for the worker-safe runtime cache.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> RuntimeMotionMatchingDatabases;

	/**
	 * Optional complete timing set for the StandTurnInPlace database, copied before worker updates.
	 * An empty set preserves full-asset completion for legacy profiles; a non-empty set must cover
	 * every turn asset exactly once. These times are cosmetic and never authorize gameplay actions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Turn In Place")
	TArray<FRpgTurnInPlaceClipTiming> TurnInPlaceClipTimings;

	/** Cosmetic gait, Motion Matching, turn, jump, and landing feel copied at initialization. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Motion Matching")
	FRpgGaspLocomotionTuning LocomotionTuning;

	/** Checks membership, all database roles/coverage, optional turn timings, and finite ordered tuning on the game thread. */
	FRpgGaspPresentationProfileValidation ValidateProfile() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/** Flattened presentation traits read by worker-thread animation callbacks. */
enum class ERpgGaspPresentationAssetTrait : uint8
{
	None = 0,
	GroundMoving = 1 << 0,
	Airborne = 1 << 1,
	JumpStart = 1 << 2,
	BackwardJumpStart = 1 << 3,
	AirborneFall = 1 << 4,
	Landing = 1 << 5,
};
ENUM_CLASS_FLAGS(ERpgGaspPresentationAssetTrait);

/**
 * Immutable-after-initialization lookup copied from a hard-referenced presentation profile.
 * Raw asset keys are safe for the cache lifetime because the profile owns hard references to every
 * entry and the AnimInstance owns the profile.
 */
struct SURVIVALRPG_API FRpgGaspPresentationAssetLookup
{
	/** Rebuilds the complete cache on the game thread; invalid profiles fail closed to an empty map. */
	bool Build(const URpgGaspPresentationProfile* Profile);

	/** Clears every cached trait and turn release time before owner/profile rebinding. */
	void Reset();

	/** Tests a precomputed trait without touching profile data, paths, packages, or sequence metadata. */
	bool HasTrait(
		const UAnimationAsset* Asset,
		ERpgGaspPresentationAssetTrait Trait) const;

	/** Reads an immutable animation-time release point; false requests the full-asset legacy fallback. */
	bool FindTurnInPlaceReentryTime(const UAnimationAsset* Asset, float& OutSeconds) const;

private:
	/** Populates from a validation result computed for the same immutable profile. */
	bool BuildValidated(
		const URpgGaspPresentationProfile* Profile,
		const FRpgGaspPresentationProfileValidation& Validation);

	TMap<const UAnimationAsset*, ERpgGaspPresentationAssetTrait> AssetTraits;
	TMap<const UAnimationAsset*, float> TurnInPlaceReentryTimes;

	friend class URpgAnimInstance;
};

/**
 * Immutable-after-initialization bidirectional database-role cache.
 * The active profile or reflected legacy facade owns hard references to every database while
 * worker callbacks read raw pointer keys and the fixed native role enum only; no profile array,
 * legacy slot, or database tag is touched after game-thread initialization.
 */
struct SURVIVALRPG_API FRpgGaspMotionMatchingDatabaseLookup
{
	/** Builds the complete 18-role cache; invalid or partial non-empty mappings fail closed. */
	bool Build(const URpgGaspPresentationProfile* Profile);

	/** Clears both lookup directions before profile rebinding or a failed rebuild. */
	void Reset();

	/** Returns the configured database for one role, or null for None, Count, or an invalid cache. */
	UPoseSearchDatabase* FindDatabase(ERpgMotionMatchingDatabaseRole Role) const;

	/** Returns the configured role for one exact database pointer, or None when it is not active. */
	ERpgMotionMatchingDatabaseRole FindRole(const UPoseSearchDatabase* Database) const;

private:
	/** Adds one already resolved role binding while rejecting null, sentinel, or duplicate entries. */
	bool AddResolvedBinding(
		ERpgMotionMatchingDatabaseRole Role,
		UPoseSearchDatabase* Database);

	/** Populates from a validation result computed for the same immutable profile. */
	bool BuildValidated(
		const URpgGaspPresentationProfile* Profile,
		const FRpgGaspPresentationProfileValidation& Validation);

	TMap<ERpgMotionMatchingDatabaseRole, UPoseSearchDatabase*> DatabaseByRole;
	TMap<const UPoseSearchDatabase*, ERpgMotionMatchingDatabaseRole> RoleByDatabase;

	friend class URpgAnimInstance;
};
