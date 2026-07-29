#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "RpgLootTable.generated.h"

class AActor;
class FDataValidationContext;
class URpgInventoryItemDefinition;

/** Safety ceiling for one resolved row after base/yield quantity evaluation. */
inline constexpr int32 RpgLootMaximumQuantityPerRoll = 1000000;

/** How a loot group evaluates its entries. */
UENUM(BlueprintType)
enum class ERpgLootGroupMode : uint8
{
	/** Every entry performs its own independent chance check. */
	Independent,

	/** A configured number of entries is selected by weight without replacement. */
	WeightedPick
};

/** One item row in a reusable loot group. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgLootEntry
{
	GENERATED_BODY()

	/** Concrete item-definition class emitted by this row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Inclusive minimum base quantity before yield/HarvestPower scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", UIMin = "1"))
	int32 MinimumQuantity = 1;

	/** Inclusive maximum base quantity before yield/HarvestPower scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumQuantity = 1;

	/** Independent-mode base chance in percent. Ignored by WeightedPick groups. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", Units = "Percent"))
	float ChancePercent = 100.0f;

	/** WeightedPick-mode relative selection weight. Ignored by Independent groups. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Weight = 1.0f;

	/** Multiplies this row's independent chance by the authoritative RareFindMultiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bScaleChanceWithRareFind = false;

	/** Multiplies quantity by YieldMultiplier and HarvestPower, using deterministic stochastic rounding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bScaleQuantityWithYield = false;
};

/** One independently gated set of loot entries. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgLootGroup
{
	GENERATED_BODY()

	/** Determines whether entries roll independently or are selected by weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	ERpgLootGroupMode Mode = ERpgLootGroupMode::Independent;

	/** Chance for this entire group to run, evaluated once before any entry rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", Units = "Percent"))
	float GroupChancePercent = 100.0f;

	/** Number of unique weighted entries selected when Mode is WeightedPick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", UIMin = "1"))
	int32 WeightedRollCount = 1;

	/** Item rows evaluated in authored order for deterministic seeded results. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FRpgLootEntry> Entries;
};

/** Reusable server-resolved loot table for deaths, harvesting, containers, and rewards. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Ordered independent/weighted groups evaluated with one caller-provided deterministic stream. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FRpgLootGroup> Groups;

	/** Runtime-safe structural validation shared by rolling and editor validation. */
	bool HasValidConfiguration(FString* OutError = nullptr) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
