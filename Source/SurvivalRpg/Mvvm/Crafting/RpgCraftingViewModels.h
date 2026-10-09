#pragma once

#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"
#include "UObject/ObjectKey.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgCraftingViewModels.generated.h"

class UTexture2D;
class URpgCraftingCategoryCatalog;
class URpgCraftingRecipeDefinition;
class URpgInventoryManagerComponent;
class URpgInventoryItemDefinition;
class URpgWorldStorageKnowledgeComponent;

/** Presentation state of one recipe row, derived from unlock state and the connected chests. UI read-only. */
UENUM(BlueprintType)
enum class ERpgCraftingRecipeState : uint8
{
	/** The connected chests hold the materials of at least one unit. */
	Craftable,

	/** Unlocked, but the connected chests lack the materials of one unit. */
	MissingResources,

	/** Not unlocked yet; the row stays visible so players see what the station offers. */
	Locked
};

/** Row kind in the crafting category list. */
UENUM(BlueprintType)
enum class ERpgCraftingCategoryRowKind : uint8
{
	/** "All recipes" at the top. */
	All,

	/** Collapsible group, one tag level below Crafting.Category. */
	Group,

	/** Subcategory inside a group. */
	Subcategory
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgCraftingViewModelListChanged);

/**
 * One material row of the selected recipe: the cost of one unit, the cost of the chosen quantity and the count in the
 * connected chests.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingIngredientViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this row from one recipe cost, the chosen quantity and the count in the connected chests. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void InitializeIngredient(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InPerUnitCount, int32 InQuantity, int32 InAvailableCount);

protected:
	/** Material definition consumed by the recipe. Static definition data; UI read-only. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Player-facing material name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Optional icon read from item UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Count one unit consumes. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	int32 PerUnitCount = 0;

	/** Count the chosen quantity consumes in total. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	int32 RequiredCount = 0;

	/** Count in the connected chests that orders may consume. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	int32 AvailableCount = 0;

	/** Missing count for the chosen quantity, or zero. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	int32 MissingCount = 0;

	/** True when the chests cover the chosen quantity. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	bool bHasEnough = false;

	/** True when the chests cover at least one unit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	bool bHasEnoughForOneUnit = false;
};

/**
 * One output row projected for the chosen quantity.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingOutputViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this UI row from one recipe output and selected quantity. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void InitializeOutput(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InOutputCount);

protected:
	/** Item definition produced by the recipe. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Output", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Player-facing output name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Output", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Optional icon read from item UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Output", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Total output count after multiplying by selected craft quantity. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Output", meta = (AllowPrivateAccess = "true"))
	int32 OutputCount = 0;
};

/**
 * One recipe list row. Widgets can bind this to a CommonButtonBase entry.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingRecipeViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this row from one recipe of a station and the units its connected chests can pay for. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void InitializeRecipe(URpgCraftingStationComponent* InStation, URpgCraftingRecipeDefinition* InRecipe, int32 InAffordableUnitCount);

	/** Static recipe represented by this row. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	URpgCraftingRecipeDefinition* GetRecipeDefinition() const { return RecipeDefinition.Get(); }

	/** Returns true if this row matches the current search text. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	bool MatchesSearchText(const FText& SearchText) const;

	/** Presentation state used to style this row. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	ERpgCraftingRecipeState GetRecipeState() const { return RecipeState; }

	/** Units one craft yields, as "×20", or empty. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FText GetYieldText() const { return YieldText; }

	/** Second row line, such as "2 per run · 3 s", "2 × 4 cells" or "Locked". */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FText GetDetailText() const { return DetailText; }

protected:
	/** Static recipe definition used by server commands. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCraftingRecipeDefinition> RecipeDefinition = nullptr;

	/** Recipe name shown in the list and details. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Recipe description for the details panel and search. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** Recipe or first-output icon. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Category tag used by the category list. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FGameplayTag RecipeCategory;

	/** UI tier used for the tier sections and filter. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 RecipeTier = 1;

	/** Seconds per produced unit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	float CraftTime = 0.0f;

	/** Designer sort priority copied from the recipe. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 SortPriority = 0;

	/** Units the connected chests can pay for right now. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 AffordableUnitCount = 0;

	/** True when globally unlocked or unlocked by default. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bIsUnlocked = false;

	/** True when unlocked and the chests pay for at least one unit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bCanCraftOne = false;

	/** True when unlocked but missing the materials of one unit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bHasMissingResources = false;

	/** Single presentation state for row styling: Locked before unlock, then Craftable or MissingResources. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	ERpgCraftingRecipeState RecipeState = ERpgCraftingRecipeState::Locked;

	/** Compact text such as "2x Plank". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText OutputSummary;

	/** Units one craft yields, as "×20", for a single-output recipe; empty otherwise. Static recipe data; UI read-only. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText YieldText;

	/** Second row line: "2 per run · 3 s" for stackable outputs, "2 × 4 cells" for single items, "Locked" before unlock. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText DetailText;

	/** Lowercase text blob used for local UI search. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FString SearchString;
};

/** Non-selectable header above the recipes of one tier in the recipe list. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingTierSectionViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes the header of one tier. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void InitializeSection(int32 InTier, const FText& InTitleText, int32 InRecipeCount);

	/** Tier this header introduces. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	int32 GetTier() const { return Tier; }

protected:
	/** Tier this header introduces. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Tier", meta = (AllowPrivateAccess = "true"))
	int32 Tier = 1;

	/** Header text, such as "Tier II" or "II · Iron" when the catalog names the tier. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Tier", meta = (AllowPrivateAccess = "true"))
	FText TitleText;

	/** Visible recipes under this header. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Tier", meta = (AllowPrivateAccess = "true"))
	int32 RecipeCount = 0;
};

/** One row of the category list: All, a collapsible group or a subcategory. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingCategoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this row. */
	void InitializeCategory(const FGameplayTag& InCategoryTag, ERpgCraftingCategoryRowKind InKind, const FText& InDisplayName,
		const TSoftObjectPtr<UTexture2D>& InIcon, int32 InRecipeCount, bool bInExpanded, bool bInSelected);

	/** Category of this row; invalid for All. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FGameplayTag GetCategoryTag() const { return CategoryTag; }

	/** Kind of this row. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	ERpgCraftingCategoryRowKind GetKind() const { return Kind; }

protected:
	/** Category of this row; invalid for All. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Category", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CategoryTag;

	/** All, group or subcategory; drives the row style. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Category", meta = (AllowPrivateAccess = "true"))
	ERpgCraftingCategoryRowKind Kind = ERpgCraftingCategoryRowKind::All;

	/** Catalog name, or the tag's last segment. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Category", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Catalog icon, or null. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Category", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Offered recipes in this row's category, independent of search and tier filters. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Category", meta = (AllowPrivateAccess = "true"))
	int32 RecipeCount = 0;

	/** Groups only: true while the subcategories show. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Category", meta = (AllowPrivateAccess = "true"))
	bool bExpanded = false;

	/** True when this row is the active category filter. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Category", meta = (AllowPrivateAccess = "true"))
	bool bSelected = false;
};

/** One option of the tier filter. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingTierOptionViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this option; tier 0 stands for all tiers. */
	void InitializeOption(int32 InTier, const FText& InLabel, bool bInSelected);

	/** Tier filtered by this option; 0 for all tiers. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	int32 GetTier() const { return Tier; }

protected:
	/** Tier filtered by this option; 0 for all tiers. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Tier", meta = (AllowPrivateAccess = "true"))
	int32 Tier = 0;

	/** Option label, such as "All tiers" or "Tier II". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Tier", meta = (AllowPrivateAccess = "true"))
	FText Label;

	/** True for the active filter. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Tier", meta = (AllowPrivateAccess = "true"))
	bool bSelected = false;
};

/** One connected chest offered as target storage, with its room for the recipe in context. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingStorageOptionViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this option. */
	void InitializeOption(FName InContainerId, const FText& InDisplayName, int32 InFreeCells, int32 InTotalCells,
		int32 InUnitsThatFit, const FText& InCapacityText, bool bInSelected, bool bInSuggested);

	/** Persistent container id of the chest. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FName GetContainerId() const { return ContainerId; }

protected:
	/** Persistent container id of the chest. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FName ContainerId;

	/** Chest name, numbered when several chests share a name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Free cells of the chest's grid. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	int32 FreeCells = 0;

	/** Cells of the chest's grid. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	int32 TotalCells = 0;

	/** Units of the recipe in context whose outputs fit now. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	int32 UnitsThatFit = 0;

	/** Short room line, such as "12 cells free". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FText CapacityText;

	/** True for the chosen target. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	bool bSelected = false;

	/** True for the chest the station suggests for the recipe in context. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	bool bSuggested = false;
};

/** One key value of the recipe preview, such as "Yield per run: 2 pieces" or "Damage: 48–54". */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgCraftingDetailRowViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this row. */
	void InitializeRow(const FText& InLabel, const FText& InValueText, bool bInEmphasized);

protected:
	/** Value name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Preview", meta = (AllowPrivateAccess = "true"))
	FText Label;

	/** Formatted value. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Preview", meta = (AllowPrivateAccess = "true"))
	FText ValueText;

	/** True for the leading value, shown larger. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Preview", meta = (AllowPrivateAccess = "true"))
	bool bEmphasized = false;
};

/**
 * Crafting screen model that observes one station and projects categories, recipes, the selected recipe, target chests
 * and the station's order for widgets. It reads replicated state only; commands go through the screen widget.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingStationViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Starts observing a crafting station for one local requesting actor, usually the controlled pawn. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void BindCraftingStation(URpgCraftingStationComponent* InStation, AActor* InRequestingActor);

	/** Clears station bindings and UI lists. Filters and the catalog stay. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void UnbindCraftingStation();

	/** Names, icons and tier names of the category list; null falls back to tag names and numerals. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetPresentationCatalog(URpgCraftingCategoryCatalog* InCatalog);

	/** Rebuilds every projection from current gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void Refresh();

	/** Refreshes station identity, connected chests and the order strip. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void RefreshStationState();

	/** Refreshes categories, recipe rows and the selected recipe after filters, unlocks or chest contents changed. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void RefreshRecipesAndDetails();

	/** Refreshes the selected recipe's details, targets and plan. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void RefreshSelectedRecipeDetails();

	/** Refreshes the order strip's progress. Call from UI progress timers; it also notices chests that came or went. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void RefreshOrderProgress();

	/** Selects a recipe for the details panel. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SelectRecipe(URpgCraftingRecipeDefinition* RecipeDefinition);

	/** Sets local search text used to filter recipe list rows. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetSearchText(FText InSearchText);

	/** Sets the category filter. Invalid tag means all categories; a group includes its subcategories. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetCategoryFilter(FGameplayTag InCategoryFilter);

	/** Activates a category row: a group toggles its subcategories and filters the whole group; other rows filter. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void ActivateCategoryRow(URpgCraftingCategoryViewModel* Row);

	/** Expands or collapses one category group. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void ToggleCategoryGroup(FGameplayTag GroupTag);

	/** Sets the tier filter. Values <= 0 mean all tiers. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetTierFilter(int32 InTierFilter);

	/** Moves the tier filter to the next (Direction > 0) or previous option, wrapping around. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void CycleTierFilter(int32 Direction);

	/** Flips the tier order of the recipe list. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void ToggleTierSortDirection();

	/** Sets the chosen quantity, clamped to 1..MaxOrderQuantity. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetCraftQuantity(int32 InCraftQuantity);

	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void IncreaseCraftQuantity(int32 Delta);

	/** Sets the quantity to what the chests can pay for and the target can hold now, at least 1. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetCraftQuantityToMax();

	/** Chooses the target chest shown in the details. While an order runs, the screen also sends it to the server. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SelectTargetStorage(FName ContainerId);

	/** Moves the target to the next (Direction > 0) or previous connected chest, wrapping around. Returns the new id. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	FName CycleTargetStorage(int32 Direction);

	/** Header title for the observed station. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FText GetStationDisplayName() const { return StationDisplayName; }

	/** Header icon for the observed station, or null. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TSoftObjectPtr<UTexture2D> GetStationIcon() const { return StationIcon; }

	/** True when the current filters leave at least one recipe row. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	bool HasFilteredRecipes() const { return bHasFilteredRecipes; }

	/** Large preview image of the selected recipe. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TSoftObjectPtr<UTexture2D> GetPreviewIcon() const { return PreviewIcon; }

	/** Static recipe currently selected by the details panel. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	URpgCraftingRecipeDefinition* GetSelectedRecipe() const { return SelectedRecipe.Get(); }

	/** Chosen quantity. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	int32 GetCraftQuantity() const { return CraftQuantity; }

	/** Chosen target chest; None without a connected chest. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FName GetSelectedTargetContainerId() const { return SelectedTargetContainerId; }

	/** True when the selection may be started as an order. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	bool CanStartOrder() const { return bCanStartOrder; }

	/** Label of the main action. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FText GetStartActionText() const { return StartActionText; }

	/** Id of the station's order; invalid while idle. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FGuid GetActiveOrderId() const { return ActiveOrderId; }

	/** True while the station has an order. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	bool HasActiveOrder() const { return bHasActiveOrder; }

	/** Active category filter; invalid for all categories. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FGameplayTag GetCategoryFilter() const { return CategoryFilter; }

	/** Active tier filter; 0 for all tiers. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	int32 GetTierFilter() const { return TierFilter; }

	/** Label of the active tier filter, such as "All tiers". */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FText GetTierFilterText() const { return TierFilterText; }

	/** Label of the tier sort toggle. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FText GetTierSortText() const { return TierSortText; }

	/** Name of the chosen target chest. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FText GetSelectedTargetName() const { return SelectedTargetName; }

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingCategoryViewModel*> GetCategoryRows() const;

	/** Recipe list items in display order: tier section headers followed by their recipe rows. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<UObject*> GetRecipeListItems() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingRecipeViewModel*> GetFilteredRecipes() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingIngredientViewModel*> GetSelectedIngredients() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingOutputViewModel*> GetSelectedOutputs() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingDetailRowViewModel*> GetPreviewRows() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingTierOptionViewModel*> GetTierOptions() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingStorageOptionViewModel*> GetTargetStorageOptions() const;

	/** Fired when the recipe list items change order or membership. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting|ViewModel")
	FRpgCraftingViewModelListChanged OnRecipesChanged;

	/** Fired when the category rows change order or membership. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting|ViewModel")
	FRpgCraftingViewModelListChanged OnCategoriesChanged;

	/** Fired when the tier filter options change. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting|ViewModel")
	FRpgCraftingViewModelListChanged OnTierOptionsChanged;

	/** Fired whenever details panel rows (materials, preview values, target chests) refresh. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting|ViewModel")
	FRpgCraftingViewModelListChanged OnSelectedRecipeDetailsChanged;

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCraftingStationComponent> ObservedStation = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> RequestingActor = nullptr;

	/** Header title: the station's authored name, a generic title when it has none, or empty while unbound. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	FText StationDisplayName;

	/** Header icon authored on the observed station, or null. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> StationIcon;

	/** Chests the station draws from and delivers into. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	int32 ConnectedStorageCount = 0;

	/** Header line, such as "Materials from 3 connected chests" or "No connected chest". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	FText ConnectedStorageText;

	/** Names of the connected chests joined by " · ". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	FText ConnectedStorageNamesText;

	/** Category rows in display order; collapsed groups hide their subcategories. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingCategoryViewModel>> CategoryRows;

	/** Title of the recipe list: the active category's name or "All recipes". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	FText RecipeListTitleText;

	/** Recipe rows left by the current filters. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	int32 FilteredRecipeCount = 0;

	/** True when the current filters leave at least one recipe row; false drives the empty-list hint. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	bool bHasFilteredRecipes = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	FText SearchText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CategoryFilter;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	int32 TierFilter = 0;

	/** Tier filter options: "All tiers" and every tier the station offers. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingTierOptionViewModel>> TierOptions;

	/** Label of the active tier filter, such as "All tiers". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	FText TierFilterText;

	/** True when the list shows low tiers first. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	bool bTierSortAscending = true;

	/** Label of the sort toggle, "Tier ↑" or "Tier ↓". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	FText TierSortText;

	/** Recipe rows left by the filters, in display order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingRecipeViewModel>> FilteredRecipes;

	/** List items: tier section headers followed by their recipe rows. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UObject>> RecipeListItems;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCraftingRecipeDefinition> SelectedRecipe = nullptr;

	/** Selected recipe's name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText SelectedRecipeName;

	/** Selected recipe's description. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText SelectedRecipeDescription;

	/** Large preview image: the recipe icon or its first output's icon. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> PreviewIcon;

	/** Path above the name, such as "Materials / Fuel · Tier I". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText BreadcrumbText;

	/** Kind line under the name: "Single item · individual stats", "Stackable material" or "Single item". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText OutputKindText;

	/** Space one output piece needs, such as "Space per item 2 × 4 cells". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText OutputSizeText;

	/** Selected recipe's tier, such as "Tier II". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText SelectedTierText;

	/** Label of the formula row, such as "One run uses". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText FormulaLabelText;

	/** Inputs of one unit, such as "4 Iron Ore + 1 Coal". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText FormulaInputsText;

	/** Outputs of one unit, such as "2 Iron Bars". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText FormulaOutputText;

	/** Status line under the preview, such as "Kiln ready" or "Missing materials". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText SelectedStatusText;

	/** True when the status line reports a problem. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bSelectedStatusIsWarning = false;

	/** Explanation under the status line. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText SelectedStatusHint;

	/** Key values next to the preview image. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingDetailRowViewModel>> PreviewRows;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingIngredientViewModel>> SelectedIngredients;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingOutputViewModel>> SelectedOutputs;

	/** Units the connected chests can pay for now. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 AffordableUnitCount = 0;

	/** Line under the materials, such as "From connected chests · enough for 12 pieces now". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText MaterialsSummaryText;

	/** Connected chests offered as target. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingStorageOptionViewModel>> TargetStorageOptions;

	/** Chosen target chest; the active order's target while one runs. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FName SelectedTargetContainerId;

	/** Chosen target chest's name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FText SelectedTargetName;

	/** Label above the target, "Store in" or "Target · current order". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FText TargetLabelText;

	/** Room line, such as "Room for 40 of 40 Iron Bars" or "No room for Iron Sword". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FText TargetCapacityText;

	/** Detail line, such as "44 present · 1 cell free · 50 per stack" or "64 / 64 cells used · 2 × 4 each". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	FText TargetDetailText;

	/** True when the target holds every wanted unit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	bool bTargetHasRoom = false;

	/** True when at least one connected chest exists. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Storage", meta = (AllowPrivateAccess = "true"))
	bool bHasConnectedStorage = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 CraftQuantity = 1;

	/** What Max sets: the smaller of affordable and fitting units, at least 1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 MaxSelectedCraftQuantity = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	float SelectedTotalCraftTime = 0.0f;

	/** Plan line, such as "20 runs → 40 Iron Bars". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText PlanSummaryText;

	/** Plan detail, such as "1 min 00 s pure time" or "20 items · 160 cells at 2 × 4 each". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText PlanDetailText;

	/** Main action label: the station's start text, or "An order is active" while one runs. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText StartActionText;

	/** True when the selection may be started as an order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bCanStartOrder = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bCanDecreaseCraftQuantity = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bCanIncreaseCraftQuantity = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bCanSetCraftQuantityToMax = false;

	/** Id of the station's order; invalid while idle. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FGuid ActiveOrderId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	bool bHasActiveOrder = false;

	/** Label of the order strip, such as "Smelting". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FText OrderLabelText;

	/** Order strip title: the order's recipe, or "Kiln ready" while idle. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FText OrderTitleText;

	/** Order strip counts, such as "6 / 20 pieces crafted", or the selection's plan while idle. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FText OrderCountsText;

	/** Order status, such as "Running · 12 s left" or "Waiting: Shared Chest full". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FText OrderStatusText;

	/** Hint under the progress bar. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FText OrderHintText;

	/** Progress of the whole order, 0..1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	float OrderProgress = 0.0f;

	/** True while the order waits for materials, room or its target. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	bool bOrderWaiting = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	bool bStationPaused = false;

	/** Live text for the pause/resume action. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FText PauseResumeButtonText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	bool bCanToggleCraftingPause = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	bool bCanStopOrder = false;

private:
	void RegisterMessageListeners();
	void UnregisterMessageListeners();
	void BindWorldKnowledgeListener();
	void UnbindWorldKnowledgeListener();
	void RequestRefresh(uint8 RefreshDomains);
	void ExecuteQueuedRefresh();
	void FlushPendingRefreshes();
	void CancelQueuedRefresh();
	void SatisfyPendingRefresh(uint8 RefreshDomains);
	bool ResolveConnectedStorage();
	TArray<URpgInventoryManagerComponent*> GetCachedConnectedStorage() const;
	void RebuildStationState();
	void RebuildRecipeList();
	void RebuildSelectedRecipeDetails();
	void RebuildOrderStrip();
	void RebuildActionAvailability();
	void BroadcastFields(TConstArrayView<UE::FieldNotification::FFieldId> Fields);
	void HandleCraftingStationChanged(FGameplayTag Channel, const FRpgCraftingStationChangeMessage& Message);
	void HandleRecipeUnlockChanged(FGameplayTag Channel, const struct FRpgRecipeUnlockChangeMessage& Message);
	void HandleInventoryChanged(FGameplayTag Channel, const struct FRpgInventoryChangeMessage& Message);

	/** Coalesces replicated world-knowledge changes into one deferred recipe/details refresh. */
	UFUNCTION()
	void HandleWorldKnowledgeChanged(FGameplayTag KnowledgeTag, bool bIsKnown);

	/** Presentation catalog set by the screen. */
	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingCategoryCatalog> PresentationCatalog = nullptr;

	FGameplayMessageListenerHandle CraftingStationChangedHandle;
	FGameplayMessageListenerHandle RecipeUnlockChangedHandle;
	FGameplayMessageListenerHandle InventoryChangedHandle;
	TWeakObjectPtr<URpgWorldStorageKnowledgeComponent> ObservedWorldKnowledge;
	FRpgViewModelInvalidationQueue RefreshQueue;
	uint8 PendingRefreshDomains = 0;

	/** Connected chests resolved at the last station refresh; inventory messages from them refresh the details. */
	TArray<TWeakObjectPtr<URpgInventoryManagerComponent>> ConnectedStorage;
	double LastConnectedStorageResolveTime = 0.0;

	/** Groups the player collapsed; groups start expanded. */
	TSet<FGameplayTag> CollapsedGroups;

	/** Target picked per recipe in this session. */
	TMap<FObjectKey, FName> TargetPickByRecipe;
};
