#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "RpgItemizationTypes.generated.h"

/** Supported Diablo-lite quality tiers for generated equipment. */
UENUM(BlueprintType)
enum class ERpgItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic
};

/** One generated numeric base-stat roll stored on a concrete item instance. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRolledItemStat
{
	GENERATED_BODY()

	/** Stable gameplay tag identifying the value consumed by equipment or abilities. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	FGameplayTag StatTag;

	/** Server-rolled numeric value. Units are defined by the referenced stat tag. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	float Value = 0.0f;

	friend bool operator==(const FRpgRolledItemStat& A, const FRpgRolledItemStat& B)
	{
		return A.StatTag == B.StatTag && A.Value == B.Value;
	}
};

/** One unique generated affix roll stored independently from an item's base stats. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRolledItemAffix
{
	GENERATED_BODY()

	/** Stable, pool-local identifier used to resolve display data without saving localized text. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	FName AffixId = NAME_None;

	/** Gameplay stat modified by this affix. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	FGameplayTag StatTag;

	/** Server-rolled numeric value. Units are defined by StatTag. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	float Value = 0.0f;

	friend bool operator==(const FRpgRolledItemAffix& A, const FRpgRolledItemAffix& B)
	{
		return A.AffixId == B.AffixId && A.StatTag == B.StatTag && A.Value == B.Value;
	}
};

/**
 * Replicated and saved random state of one generated equipment instance.
 * The server owns mutation; clients and UI may only read this structure.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemizationState
{
	GENERATED_BODY()

	/** Distinguishes deliberately generated equipment from legacy/static grants. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	bool bGenerated = false;

	/** Effective generation level, clamped by the item's profile; zero for legacy/unrolled items. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization", meta = (ClampMin = "0"))
	int32 ItemLevel = 0;

	/** Quality tier controlling the number of unique affixes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	ERpgItemRarity Rarity = ERpgItemRarity::Common;

	/** Definition-profile base values such as weapon damage, stagger, or armor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	TArray<FRpgRolledItemStat> BaseStats;

	/** Unique weighted affixes rolled from the definition's affix pool. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Itemization")
	TArray<FRpgRolledItemAffix> Affixes;

	/** Returns whether the state is safe to replicate, persist, and use for stack identity. */
	bool IsStructurallyValid() const;

	/** Returns the first matching base-stat value, or zero when the item has no such base roll. */
	float GetBaseValueForStat(FGameplayTag StatTag) const;

	/** Returns the sum of matching base and affix values for equipment/GAS integration. */
	float GetTotalValueForStat(FGameplayTag StatTag) const;

	friend bool operator==(const FRpgItemizationState& A, const FRpgItemizationState& B)
	{
		return A.bGenerated == B.bGenerated &&
			A.ItemLevel == B.ItemLevel &&
			A.Rarity == B.Rarity &&
			A.BaseStats == B.BaseStats &&
			A.Affixes == B.Affixes;
	}

	friend bool operator!=(const FRpgItemizationState& A, const FRpgItemizationState& B)
	{
		return !(A == B);
	}
};

/** Returns the fixed first-slice affix count for a rarity tier: 0/1/2/3. */
SURVIVALRPG_API int32 GetRpgAffixCountForRarity(ERpgItemRarity Rarity);
