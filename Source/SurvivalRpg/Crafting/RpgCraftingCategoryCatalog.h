#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RpgCraftingCategoryCatalog.generated.h"

class FDataValidationContext;
class UTexture2D;

/** Display data of one crafting category group or subcategory. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingCategoryDisplay
{
	GENERATED_BODY()

	/** Category tag: one level below Crafting.Category names a group, two levels a subcategory of that group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (Categories = "Crafting.Category"))
	FGameplayTag Category;

	/** Player-facing name. Empty falls back to the tag's last segment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FText DisplayName;

	/** Optional icon shown with the name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Order among siblings; lower values come first, ties sort by name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	int32 SortOrder = 0;
};

/** Display data of one recipe tier. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingTierDisplay
{
	GENERATED_BODY()

	/** Recipe tier this row names (URpgCraftingRecipeDefinition::RecipeTier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1"))
	int32 Tier = 1;

	/** Optional short name shown after the numeral, such as "Iron". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FText DisplayName;
};

/**
 * Presentation catalog of the crafting screen: names, icons and order of category groups and subcategories, plus
 * optional tier names. Static designer data read by the crafting view model; it neither grants nor gates recipes.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingCategoryCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Groups and subcategories with names and icons. Tags without a row fall back to their last segment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (TitleProperty = "Category"))
	TArray<FRpgCraftingCategoryDisplay> Categories;

	/** Optional tier names. Tiers without a row show only their numeral. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (TitleProperty = "Tier"))
	TArray<FRpgCraftingTierDisplay> Tiers;

	/** Returns the row of an exact category tag, or null. */
	const FRpgCraftingCategoryDisplay* FindCategory(const FGameplayTag& Category) const;

	/** Returns the row of a tier, or null. */
	const FRpgCraftingTierDisplay* FindTier(int32 Tier) const;

	/** Root tag every crafting category lives under (Crafting.Category). */
	static FGameplayTag GetCategoryRoot();

	/** Group of a recipe category: its ancestor one level below the root, or the tag itself at that level. */
	static FGameplayTag ResolveCategoryGroup(const FGameplayTag& Category);

	/** Roman numeral of a tier (I to XX); larger tiers fall back to the number. */
	static FText MakeTierNumeral(int32 Tier);

	/** Last segment of a tag, used when a category has no catalog row. */
	static FText MakeFallbackCategoryName(const FGameplayTag& Category);

#if WITH_EDITOR
	/** Rejects duplicate or non-category tags, categories deeper than group/subcategory and duplicate tiers. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
