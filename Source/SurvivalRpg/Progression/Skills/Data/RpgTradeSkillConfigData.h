// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgTradeSkillConfigData.generated.h"

class UCurveFloat;

/** Legacy trade-skill ids retained only as a Blueprint migration adapter. */
UENUM(BlueprintType)
enum class ETradeSkill : uint8
{
	Blacksmithing,
	Woodworking,
	Mining,
	Harvesting,
	Logging,

	MAX UMETA(Hidden)
};

/** Optional per-skill tuning; missing curves use the project's level 1-100 defaults. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FTradeSkillConfig
{
	GENERATED_BODY()

	/** Optional XP cost by current level; unset entries use round(100 * Level^1.35). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills")
	TObjectPtr<UCurveFloat> XPToNextLevel = nullptr;

	/** Highest attainable skill level. Runtime values are always capped at the supported maximum of 100. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MaxLevel = 100;

	/** Optional crafting-station unlock threshold consumed by feature-specific progression content. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
	int32 StationTierUnlockLevel = 0;

	/** Optional absolute gathering quantity multiplier by skill level; unset uses a linear 1.0-1.5 curve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills")
	TObjectPtr<UCurveFloat> YieldMultiplierByLevel = nullptr;

	/** Optional multiplicative rare-find modifier by skill level; unset uses a linear 1.0-2.0 curve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills")
	TObjectPtr<UCurveFloat> RareFindMultiplierByLevel = nullptr;
};

/** Designer-authored overrides for tag-addressed trade-skill progression. */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgTradeSkillConfigData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/** Preferred tag-keyed tuning map. Missing core skills use the documented project defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills", meta = (Categories = "Skill"))
	TMap<FGameplayTag, FTradeSkillConfig> TaggedSkillConfigs;

	/** Legacy enum-keyed tuning preserved while existing Blueprint assets migrate to TaggedSkillConfigs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills", meta = (DeprecatedProperty, DeprecationMessage = "Use TaggedSkillConfigs with Skill.* gameplay tags."))
	TMap<ETradeSkill, FTradeSkillConfig> SkillConfigs;

#if WITH_EDITOR
	/** Rejects invalid skill tags, ranges, and curve values before the asset reaches gameplay. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
