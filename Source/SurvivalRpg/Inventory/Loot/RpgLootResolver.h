#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgLootTable.h"
#include "SurvivalRpg/Inventory/IPickupable.h"

#include "RpgLootResolver.generated.h"

class AActor;

/** Server-authored inputs shared by enemy, harvesting, container, and scripted reward rolls. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgLootRollContext
{
	GENERATED_BODY()

	/** Optional authoritative actor that produced the reward. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot")
	TObjectPtr<AActor> SourceActor;

	/** Optional authoritative actor receiving or harvesting the reward. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot")
	TObjectPtr<AActor> RecipientActor;

	/** Semantic source tags used by higher-level table selection and diagnostics. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot")
	FGameplayTagContainer SourceTags;

	/** Authoritative encounter/resource level used as generated equipment item level. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "1", UIMin = "1"))
	int32 SourceLevel = 1;

	/** Optional tag-driven gathering/crafting skill responsible for the reward. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot")
	FGameplayTag SkillId;

	/** Current authoritative skill level, retained for downstream reward policies. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0", UIMin = "0"))
	int32 SkillLevel = 0;

	/** Ability/tool contribution multiplied into yield-sensitive entry quantities. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HarvestPower = 1.0f;

	/** Skill/effect quantity multiplier applied only to yield-sensitive entries. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float YieldMultiplier = 1.0f;

	/** Multiplicative chance modifier applied only to rare-find-sensitive independent entries. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RareFindMultiplier = 1.0f;

	/** Explicit deterministic seed. Production callers generate it on the server; tests may supply a fixed value. */
	UPROPERTY(BlueprintReadWrite, Category = "Loot")
	int32 Seed = 0;

	/** Returns whether numeric server context can be evaluated without NaN, overflow, or negative multipliers. */
	bool IsValid() const;
};

/** One successful table row before conversion to templates or concrete generated instances. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgLootItemRoll
{
	GENERATED_BODY()

	/** Item-definition class selected by the table. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Final whole-item quantity after optional yield scaling and stochastic rounding. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	int32 Quantity = 0;

	/** Source level captured for deterministic itemization during later materialization. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	int32 SourceLevel = 1;

	/** Per-row deterministic sub-seed used only when the selected definition has an Itemization fragment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	int32 ItemizationSeed = 0;
};

/** Deterministic table output independent from inventory capacity or world-delivery policy. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgLootRollResult
{
	GENERATED_BODY()

	/** Seed used to produce this result, retained for diagnostics and replay tests. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	int32 Seed = 0;

	/** Successful rows in deterministic group/entry order. Empty is a valid no-drop result. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FRpgLootItemRoll> Items;

	/** Converts this roll into stack templates and generated concrete instances as one all-or-nothing operation. */
	bool ToInventoryPickup(UObject* InstanceOuter, FInventoryPickup& OutPickup) const;
};

/** Stateless deterministic loot evaluator and item-instance materializer. */
struct SURVIVALRPG_API FRpgLootResolver
{
	/** Evaluates every table group using only the supplied deterministic context. */
	static bool RollLoot(
		const URpgLootTable* LootTable,
		const FRpgLootRollContext& Context,
		FRpgLootRollResult& OutResult);

	/** Converts a prior roll into normal templates and itemized concrete instances. */
	static bool MaterializeLoot(
		UObject* InstanceOuter,
		const FRpgLootRollResult& RollResult,
		FInventoryPickup& OutPickup);

	/** Rolls and materializes in one call; OutRollResult is optional diagnostic/replay output. */
	static bool RollAndMaterialize(
		const URpgLootTable* LootTable,
		const FRpgLootRollContext& Context,
		UObject* InstanceOuter,
		FInventoryPickup& OutPickup,
		FRpgLootRollResult* OutRollResult = nullptr);
};
