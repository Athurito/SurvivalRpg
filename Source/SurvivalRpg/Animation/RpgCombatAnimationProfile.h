// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgCombatAnimationProfile.generated.h"

class UAnimSequence;
class USkeleton;

/**
 * One designer-owned upper-body presentation profile selected from equipped-item traits.
 *
 * Gameplay remains authoritative in Equipment, GAS, and Character rotation state. These values
 * select cosmetic animation assets and blend timings only; they are never replicated or saved.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCombatAnimationPoseProfile
{
	GENERATED_BODY()

	/** Stable diagnostic label for this presentation profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	FName ProfileName = NAME_None;

	/** Equipment traits that must all be present for this profile to match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	FGameplayTagContainer RequiredEquipmentTraits;

	/** Equipment traits that prevent this profile from matching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	FGameplayTagContainer BlockedEquipmentTraits;

	/** Looping upper-body pose used while the loadout is equipped in Free rotation mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	TObjectPtr<UAnimSequence> EquippedUpperBodyAnimation;

	/** Looping upper-body pose used while CombatStrafe or Aim rotation mode is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	TObjectPtr<UAnimSequence> CombatReadyUpperBodyAnimation;

	/** Seconds used to blend this upper-body profile in after equip or profile replacement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", Units = "s"))
	float EquipBlendInTime = 0.2f;

	/** Seconds used to blend this upper-body profile out before unequip or profile replacement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", Units = "s"))
	float EquipBlendOutTime = 0.15f;

	/** Seconds used by the AnimGraph to crossfade Free and combat-ready upper-body poses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", Units = "s"))
	float CombatModeBlendTime = 0.12f;

	/** Returns true when the supplied replicated-equipment trait projection satisfies this profile. */
	bool Matches(const FGameplayTagContainer& EquipmentTraits) const;

	/** Number of required traits used as the deterministic most-specific-match score. */
	int32 GetSpecificity() const { return RequiredEquipmentTraits.Num(); }
};

/** Value-only profile selection safe to copy from game-thread lookup state into the AnimInstance proxy. */
struct SURVIVALRPG_API FRpgResolvedCombatAnimationProfile
{
	FName ProfileName = NAME_None;
	UAnimSequence* EquippedUpperBodyAnimation = nullptr;
	UAnimSequence* CombatReadyUpperBodyAnimation = nullptr;
	float EquipBlendInTime = 0.0f;
	float EquipBlendOutTime = 0.0f;
	float CombatModeBlendTime = 0.0f;
	bool bIsFallback = true;

	/** True when this selection supplies a complete upper-body animation pair. */
	bool HasOverlay() const
	{
		return EquippedUpperBodyAnimation != nullptr && CombatReadyUpperBodyAnimation != nullptr;
	}

	/** Compares the immutable profile identity and animation assets, excluding current blend state. */
	bool IsSameProfile(const FRpgResolvedCombatAnimationProfile& Other) const
	{
		return ProfileName == Other.ProfileName &&
			EquippedUpperBodyAnimation == Other.EquippedUpperBodyAnimation &&
			CombatReadyUpperBodyAnimation == Other.CombatReadyUpperBodyAnimation &&
			bIsFallback == Other.bIsFallback;
	}
};

/** Value-only integrity result shared by editor validation and focused automation coverage. */
struct SURVIVALRPG_API FRpgCombatAnimationProfileValidation
{
	bool bHasMissingTargetSkeleton = false;
	bool bHasMissingUpperBodyBlendMask = false;
	bool bHasInvalidUpperBodyBlendMask = false;
	bool bHasMissingDefaultSlot = false;
	bool bHasInvalidFallback = false;
	bool bHasNoWeaponProfiles = false;
	bool bHasInvalidProfileName = false;
	bool bHasDuplicateProfileName = false;
	bool bHasEmptyRequiredTraits = false;
	bool bHasConflictingTraits = false;
	bool bHasAmbiguousProfiles = false;
	bool bHasMissingAnimation = false;
	bool bHasSkeletonMismatch = false;
	bool bHasAdditiveAnimation = false;
	bool bHasRootMotionAnimation = false;
	bool bHasNonLoopingAnimation = false;
	bool bHasInvalidAnimationLength = false;
	bool bHasForbiddenDependency = false;
	bool bHasInvalidBlendTime = false;

	bool IsValid() const
	{
		return !bHasMissingTargetSkeleton && !bHasMissingUpperBodyBlendMask &&
			!bHasInvalidUpperBodyBlendMask && !bHasMissingDefaultSlot &&
			!bHasInvalidFallback && !bHasNoWeaponProfiles &&
			!bHasInvalidProfileName && !bHasDuplicateProfileName &&
			!bHasEmptyRequiredTraits && !bHasConflictingTraits &&
			!bHasAmbiguousProfiles && !bHasMissingAnimation &&
			!bHasSkeletonMismatch && !bHasAdditiveAnimation &&
			!bHasRootMotionAnimation && !bHasNonLoopingAnimation &&
			!bHasInvalidAnimationLength &&
			!bHasForbiddenDependency &&
			!bHasInvalidBlendTime;
	}
};

/**
 * Project-local combat animation presentation contract.
 *
 * The asset hard-references every in-scope overlay sequence and maps generic equipment traits to
 * presentation. Unknown, invalid, or equally specific matches always fail closed to Unarmed.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgCombatAnimationProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Skeleton shared by every authored overlay and the AnimBP that consumes this profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	TObjectPtr<USkeleton> TargetSkeleton;

	/** Project-owned skeleton blend mask used by the GASP AnimGraph upper-body layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	FName UpperBodyBlendMaskName = TEXT("UpperBodyMask");

	/** Mandatory deterministic fallback. Its match traits must stay empty; animations are optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	FRpgCombatAnimationPoseProfile UnarmedFallback;

	/** Designer-authored weapon/loadout profiles resolved by most-specific matching trait set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Animation|Combat", meta = (TitleProperty = "ProfileName"))
	TArray<FRpgCombatAnimationPoseProfile> WeaponProfiles;

	/** Resolves the unique most-specific match, or Unarmed for unknown and ambiguous loadouts. */
	FRpgResolvedCombatAnimationProfile ResolveProfile(
		const FGameplayTagContainer& EquipmentTraits) const;

	/** Checks fallback, trait matching, animations, skeleton, blend mask, timings, and source ownership. */
	FRpgCombatAnimationProfileValidation ValidateProfile() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/**
 * Immutable-after-initialization lookup consumed by game-thread animation proxy snapshots.
 * Profile hard references retain every raw animation pointer for the AnimInstance lifetime.
 */
struct SURVIVALRPG_API FRpgCombatAnimationProfileLookup
{
	/** Builds a complete runtime copy; invalid profiles fail closed to an empty Unarmed fallback. */
	bool Build(const URpgCombatAnimationProfile* Profile);

	/** Clears every cached profile and restores the empty Unarmed fallback. */
	void Reset();

	/** Resolves without touching DataAsset arrays, equipment UObjects, paths, or animation metadata. */
	FRpgResolvedCombatAnimationProfile Resolve(
		const FGameplayTagContainer& EquipmentTraits) const;

	/** True only after a validated profile has populated the immutable runtime table. */
	bool IsEnabled() const { return !Entries.IsEmpty(); }

private:
	struct FRuntimeEntry
	{
		FGameplayTagContainer RequiredEquipmentTraits;
		FGameplayTagContainer BlockedEquipmentTraits;
		FRpgResolvedCombatAnimationProfile Profile;
	};

	FRpgResolvedCombatAnimationProfile FallbackProfile;
	TArray<FRuntimeEntry> Entries;
};
