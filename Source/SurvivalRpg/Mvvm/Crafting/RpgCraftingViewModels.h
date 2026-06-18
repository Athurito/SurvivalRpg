#pragma once

#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgCraftingViewModels.generated.h"

class UTexture2D;
class URpgCraftingRecipeDefinition;
class URpgInventoryItemDefinition;

/** Local UI sort modes for crafting recipe lists. They never mutate server gameplay state. */
UENUM(BlueprintType)
enum class ERpgCraftingRecipeSortMode : uint8
{
	/** Craftable first, then tier, designer priority, and name. */
	Default,

	/** Sort by RecipeTier, then designer priority and name. */
	Tier,

	/** Sort by RecipeCategory, then tier and name. */
	Category,

	/** Sort alphabetically by recipe display name. */
	Name,

	/** Sort craftable recipes before blocked or locked recipes. */
	Craftable,

	/** Sort globally unlocked recipes before locked recipes, then default order. */
	RecentUnlocked
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgCraftingViewModelListChanged);

/**
 * One required ingredient row projected for the currently selected quantity.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingIngredientViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this UI row from one recipe cost and the current resource availability. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void InitializeIngredient(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InRequiredCount, int32 InAvailableCount);

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

	/** Total required count after multiplying by selected craft quantity. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	int32 RequiredCount = 0;

	/** Currently available count from player inventory plus station resource sources. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	int32 AvailableCount = 0;

	/** Missing count, or zero when enough resources exist. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	int32 MissingCount = 0;

	/** True when AvailableCount is at least RequiredCount. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Ingredient", meta = (AllowPrivateAccess = "true"))
	bool bHasEnough = false;
};

/**
 * One output preview row projected for the currently selected quantity.
 */
UCLASS(BlueprintType)
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
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingRecipeViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this row from one recipe for a specific station and requesting actor. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void InitializeRecipe(URpgCraftingStationComponent* InStation, AActor* InRequestingActor, URpgCraftingRecipeDefinition* InRecipe);

	/** Static recipe represented by this row. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	URpgCraftingRecipeDefinition* GetRecipeDefinition() const { return RecipeDefinition.Get(); }

	/** Returns true if this row matches the current search text. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	bool MatchesSearchText(const FText& SearchText) const;

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

	/** Primary UI category tag for tabs/filtering. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FGameplayTag RecipeCategory;

	/** UI tier used for filtering and display badges. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 RecipeTier = 1;

	/** Seconds per produced unit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	float CraftTime = 0.0f;

	/** Designer sort priority copied from the recipe. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 SortPriority = 0;

	/** Maximum quantity craftable from current known resources. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 MaxCraftableQuantity = 0;

	/** True when globally unlocked or unlocked by default. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bIsUnlocked = false;

	/** True when at least one unit can currently be queued. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bCanCraftOne = false;

	/** True when the row is blocked because one or more ingredients are missing. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bHasMissingResources = false;

	/** Compact text such as "2x Plank" for recipe list rows. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FText OutputSummary;

	/** Lowercase text blob used for local UI search. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	FString SearchString;
};

/**
 * One active or queued crafting job row.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingJobViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this UI row from replicated station job data. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void InitializeJob(const FRpgCraftingJobEntry& Job, float ServerWorldTime);

	/** Job id used by cancel commands. */
	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	FGuid GetJobId() const { return JobId; }

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	FGuid JobId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCraftingRecipeDefinition> RecipeDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	int32 QuantityTotal = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	int32 QuantityCompleted = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	ERpgCraftingJobState State = ERpgCraftingJobState::Queued;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Job", meta = (AllowPrivateAccess = "true"))
	float RemainingSeconds = 0.0f;
};

/**
 * Crafting screen model that observes one station and projects recipe/filter/job state for widgets.
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

	/** Clears station bindings and UI lists. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void UnbindCraftingStation();

	/** Rebuilds recipe, ingredient, output, and job projections from current gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void Refresh();

	/** Selects a recipe for the details panel. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SelectRecipe(URpgCraftingRecipeDefinition* RecipeDefinition);

	/** Sets local search text used to filter recipe list rows. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetSearchText(FText InSearchText);

	/** Sets the category filter. Invalid tag means all categories. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetCategoryFilter(FGameplayTag InCategoryFilter);

	/** Sets the tier filter. Values <= 0 mean all tiers. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetTierFilter(int32 InTierFilter);

	/** Sets local recipe sort mode. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetRecipeSortMode(ERpgCraftingRecipeSortMode InSortMode);

	/** Sets desired craft quantity and clamps details against currently craftable maximum. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetCraftQuantity(int32 InCraftQuantity);

	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void IncreaseCraftQuantity(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
	void SetCraftQuantityToMax();

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingRecipeViewModel*> GetFilteredRecipes() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingIngredientViewModel*> GetSelectedIngredients() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingOutputViewModel*> GetSelectedOutputs() const;

	UFUNCTION(BlueprintPure, Category = "Crafting|ViewModel")
	TArray<URpgCraftingJobViewModel*> GetJobs() const;

	/** Fired whenever recipe list widgets should call SetListItems/RequestRefresh. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting|ViewModel")
	FRpgCraftingViewModelListChanged OnRecipesChanged;

	/** Fired whenever details panel rows should refresh. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting|ViewModel")
	FRpgCraftingViewModelListChanged OnSelectedRecipeDetailsChanged;

	/** Fired whenever queue list widgets should refresh. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting|ViewModel")
	FRpgCraftingViewModelListChanged OnJobsChanged;

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCraftingStationComponent> ObservedStation = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> RequestingActor = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCraftingRecipeDefinition> SelectedRecipe = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 CraftQuantity = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	int32 MaxSelectedCraftQuantity = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	float SelectedTotalCraftTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	bool bCanCraftSelectedRecipe = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	bool bStationPaused = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	FText SearchText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CategoryFilter;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	int32 TierFilter = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Filters", meta = (AllowPrivateAccess = "true"))
	ERpgCraftingRecipeSortMode RecipeSortMode = ERpgCraftingRecipeSortMode::Default;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingRecipeViewModel>> FilteredRecipes;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingIngredientViewModel>> SelectedIngredients;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Recipe", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingOutputViewModel>> SelectedOutputs;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Crafting|Jobs", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgCraftingJobViewModel>> Jobs;

private:
	void RegisterMessageListeners();
	void UnregisterMessageListeners();
	void RebuildRecipeList();
	void RebuildSelectedRecipeDetails();
	void RebuildJobs();
	void HandleCraftingStationChanged(FGameplayTag Channel, const FRpgCraftingStationChangeMessage& Message);
	void HandleRecipeUnlockChanged(FGameplayTag Channel, const struct FRpgRecipeUnlockChangeMessage& Message);
	void HandleInventoryChanged(FGameplayTag Channel, const struct FRpgInventoryChangeMessage& Message);
	void HandleBaseStorageChanged(FGameplayTag Channel, const struct FRpgBaseResourceChangeMessage& Message);

	FGameplayMessageListenerHandle CraftingStationChangedHandle;
	FGameplayMessageListenerHandle RecipeUnlockChangedHandle;
	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle BaseStorageChangedHandle;
};
