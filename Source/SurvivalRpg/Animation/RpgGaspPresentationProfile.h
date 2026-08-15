// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "RpgGaspPresentationProfile.generated.h"

class UAnimationAsset;
class UAnimSequenceBase;

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

	bool IsValid() const
	{
		return !bIsEmpty && !bHasNullAsset && !bHasDuplicateAsset &&
			!bHasUnassignedCategory && !bHasLoopingJumpStart &&
			!bHasNonLoopingAirborneFall && !bHasLoopingLanding;
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

	/** Checks empty data, nulls, duplicates, unassigned categories, and loop invariants. */
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

	/** Clears every cached trait before owner/profile rebinding. */
	void Reset();

	/** Tests a precomputed trait without touching profile data, paths, packages, or sequence metadata. */
	bool HasTrait(
		const UAnimationAsset* Asset,
		ERpgGaspPresentationAssetTrait Trait) const;

private:
	TMap<const UAnimationAsset*, ERpgGaspPresentationAssetTrait> AssetTraits;
};
