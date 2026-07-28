#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "RpgItemizationTypes.h"

#include "RpgItemizationProfile.generated.h"

class FDataValidationContext;

/** Designer-authored numeric range for one generated base stat. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemStatRollDefinition
{
	GENERATED_BODY()

	/** Gameplay stat written into the concrete item instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization", meta = (Categories = "Item.Stat"))
	FGameplayTag StatTag;

	/** Inclusive lower roll bound evaluated at the generated item level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FScalableFloat MinimumValue;

	/** Inclusive upper roll bound evaluated at the generated item level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FScalableFloat MaximumValue;
};

/** One weighted, item-tag-filtered numeric affix available to generated equipment. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemAffixDefinition
{
	GENERATED_BODY()

	/** Stable pool-local id persisted on generated items; changing it invalidates display lookup for old items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FName AffixId = NAME_None;

	/** Localized label used by inventory details and tooltips. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FText DisplayName;

	/** Gameplay stat modified by the rolled value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization", meta = (Categories = "Item.Stat"))
	FGameplayTag StatTag;

	/** Relative selection weight among currently eligible affixes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Weight = 1.0f;

	/** Inclusive lower roll bound evaluated at the generated item level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FScalableFloat MinimumValue;

	/** Inclusive upper roll bound evaluated at the generated item level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FScalableFloat MaximumValue;

	/** All of these static profile tags must be present for this affix to roll. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FGameplayTagContainer RequiredItemTags;

	/** Any matching static profile tag prevents this affix from rolling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FGameplayTagContainer BlockedItemTags;

	/** Returns whether this affix is eligible for an item profile's static tags. */
	bool IsEligibleFor(const FGameplayTagContainer& ItemTags) const;
};

/** Reusable weighted catalog of numeric equipment affixes. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemAffixPool : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Affixes selected without replacement; AffixId values must be unique within the pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	TArray<FRpgItemAffixDefinition> Affixes;

	/** Collects valid affixes compatible with the supplied profile tags. */
	void GetEligibleAffixes(
		const FGameplayTagContainer& ItemTags,
		TArray<const FRpgItemAffixDefinition*>& OutAffixes) const;

	/** Runtime-safe structural validation shared by generation and editor validation. */
	bool HasValidConfiguration(FString* OutError = nullptr) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Relative probability of one supported item rarity. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemRarityWeight
{
	GENERATED_BODY()

	/** Rarity selected by this row. Keep one row per supported tier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	ERpgItemRarity Rarity = ERpgItemRarity::Common;

	/** Relative selection weight; defaults across all rows are 60/28/10/2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Weight = 0.0f;
};

/** Static generation rules referenced by an item-definition fragment. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemizationProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	explicit URpgItemizationProfile(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Static classification used to filter compatible affixes, such as weapon, armor, or sword. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	FGameplayTagContainer ItemTags;

	/** Base values always rolled once, such as weapon damage/stagger or armor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	TArray<FRpgItemStatRollDefinition> BaseStats;

	/** Weighted affix catalog; required whenever a non-Common rarity has non-zero weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	TObjectPtr<URpgItemAffixPool> AffixPool;

	/** Tier weights. The default profile uses Common 60, Uncommon 28, Rare 10, Epic 2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	TArray<FRpgItemRarityWeight> RarityWeights;

	/** Lowest item level this profile may generate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MinimumItemLevel = 1;

	/** Highest item level this profile may generate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MaximumItemLevel = 100;

	/** Designer offset applied to the authoritative source level before clamping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	int32 ItemLevelOffset = 0;

	/** Resolves the source level and offset into this profile's supported range. */
	int32 ResolveItemLevel(int32 SourceLevel) const;

	/** Runtime-safe structural validation shared by generation and editor validation. */
	bool HasValidConfiguration(FString* OutError = nullptr) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
