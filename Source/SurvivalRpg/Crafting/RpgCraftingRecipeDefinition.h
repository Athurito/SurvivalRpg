#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgCraftingStationComponent.h"

#include "RpgCraftingRecipeDefinition.generated.h"

class UTexture2D;

/**
 * Data-driven crafting recipe consumed by a crafting station.
 *
 * V1 recipes are instant. CraftTime is kept for UI/upgrade readiness but is not queued yet.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing recipe name shown in crafting UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting|Display")
	FText DisplayName;

	/** Optional description explaining the produced item or station requirement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting|Display", meta = (MultiLine = true))
	FText Description;

	/** Optional icon used by recipe lists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting|Display", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Optional category tag for UI grouping such as Crafting.Category.Refining. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (Categories = "Crafting.Category"))
	FGameplayTag RecipeCategory;

	/** Station tags that must all be present on the crafting station. Empty means any station can craft it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (Categories = "Crafting.Station"))
	FGameplayTagContainer RequiredStationTags;

	/** Base or progression unlock tags required before this recipe appears or can be crafted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (Categories = "Base,Recipe,Crafting"))
	FGameplayTagContainer RequiredUnlockTags;

	/** Resource costs consumed from player inventory and/or linked base storage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<FRpgCraftingResourceCost> RequiredResources;

	/** Item stacks created by this recipe. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<FRpgCraftingOutputItem> OutputItems;

	/** Future queue/timer duration in seconds. V1 treats values <= 0 as instant craft. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0", UIMin = "0", Units = "s"))
	float CraftTime = 0.0f;
};

/** Small recipe collection assigned to a station or terminal UI. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingRecipeSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Recipes this station may offer, filtered again by station tags and unlock tags at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<TObjectPtr<URpgCraftingRecipeDefinition>> Recipes;
};
