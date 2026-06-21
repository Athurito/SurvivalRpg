#include "RpgCraftingViewModels.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgRecipeUnlockComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingViewModels)

namespace
{
	const URpgInventoryItemDefinition* GetItemDefinitionCDO(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		return ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
	}

	FText GetItemDisplayName(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		return ItemCDO ? ItemCDO->DisplayName : FText::GetEmpty();
	}

	TSoftObjectPtr<UTexture2D> GetItemIcon(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		const URpgInventoryFragment_UIData* UIData = ItemCDO ? Cast<URpgInventoryFragment_UIData>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_UIData::StaticClass())) : nullptr;
		return UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
	}

	FString NormalizeSearchString(const FText& Text)
	{
		return Text.ToString().ToLower();
	}

	FString GetRecipeSortName(const URpgCraftingRecipeDefinition* Recipe)
	{
		return Recipe ? Recipe->DisplayName.ToString() : FString();
	}

	FString GetCategorySortString(const URpgCraftingRecipeDefinition* Recipe)
	{
		return Recipe && Recipe->RecipeCategory.IsValid() ? Recipe->RecipeCategory.ToString() : FString();
	}

	bool IsRecipeCraftable(URpgCraftingStationComponent* Station, AActor* RequestingActor, const URpgCraftingRecipeDefinition* Recipe)
	{
		return Station && Station->CanCraftRecipe(RequestingActor, Recipe);
	}

	TSoftObjectPtr<UTexture2D> GetRecipeIcon(const URpgCraftingRecipeDefinition* Recipe)
	{
		if (!Recipe)
		{
			return TSoftObjectPtr<UTexture2D>();
		}

		if (!Recipe->Icon.IsNull())
		{
			return Recipe->Icon;
		}

		return Recipe->OutputItems.Num() > 0 ? GetItemIcon(Recipe->OutputItems[0].ItemDefinition) : TSoftObjectPtr<UTexture2D>();
	}

	float GetServerWorldTimeSeconds(const UObject* WorldContextObject)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
	}
}

void URpgCraftingIngredientViewModel::InitializeIngredient(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InRequiredCount, int32 InAvailableCount)
{
	ItemDefinition = InItemDefinition;
	DisplayName = GetItemDisplayName(ItemDefinition);
	Icon = GetItemIcon(ItemDefinition);
	RequiredCount = FMath::Max(0, InRequiredCount);
	AvailableCount = FMath::Max(0, InAvailableCount);
	MissingCount = FMath::Max(0, RequiredCount - AvailableCount);
	bHasEnough = MissingCount <= 0;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemDefinition);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RequiredCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AvailableCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MissingCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasEnough);
}

void URpgCraftingOutputViewModel::InitializeOutput(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InOutputCount)
{
	ItemDefinition = InItemDefinition;
	DisplayName = GetItemDisplayName(ItemDefinition);
	Icon = GetItemIcon(ItemDefinition);
	OutputCount = FMath::Max(0, InOutputCount);

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemDefinition);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputCount);
}

void URpgCraftingRecipeViewModel::InitializeRecipe(URpgCraftingStationComponent* InStation, AActor* InRequestingActor, URpgCraftingRecipeDefinition* InRecipe)
{
	RecipeDefinition = InRecipe;
	DisplayName = InRecipe ? InRecipe->DisplayName : FText::GetEmpty();
	Description = InRecipe ? InRecipe->Description : FText::GetEmpty();
	Icon = GetRecipeIcon(InRecipe);
	RecipeCategory = InRecipe ? InRecipe->RecipeCategory : FGameplayTag();
	RecipeTier = InRecipe ? InRecipe->RecipeTier : 1;
	CraftTime = InRecipe ? InRecipe->CraftTime : 0.0f;
	SortPriority = InRecipe ? InRecipe->SortPriority : 0;
	MaxCraftableQuantity = InStation && InRecipe ? InStation->GetMaxCraftableQuantity(InRequestingActor, InRecipe) : 0;
	bIsUnlocked = InStation && InRecipe ? InStation->IsRecipeUnlocked(InRecipe) : false;
	bCanCraftOne = InStation && InRecipe ? InStation->CanCraftRecipe(InRequestingActor, InRecipe) : false;
	bHasMissingResources = bIsUnlocked && MaxCraftableQuantity <= 0;

	if (InRecipe && InRecipe->OutputItems.Num() == 1)
	{
		const FRpgCraftingOutputItem& OutputItem = InRecipe->OutputItems[0];
		OutputSummary = FText::Format(
			NSLOCTEXT("RpgCrafting", "SingleOutputSummary", "{0}x {1}"),
			FText::AsNumber(OutputItem.Count),
			GetItemDisplayName(OutputItem.ItemDefinition));
	}
	else if (InRecipe && InRecipe->OutputItems.Num() > 1)
	{
		OutputSummary = FText::Format(
			NSLOCTEXT("RpgCrafting", "MultiOutputSummary", "{0} Outputs"),
			FText::AsNumber(InRecipe->OutputItems.Num()));
	}
	else
	{
		OutputSummary = FText::GetEmpty();
	}

	SearchString.Reset();
	SearchString += NormalizeSearchString(DisplayName);
	SearchString += TEXT(" ");
	SearchString += NormalizeSearchString(Description);
	SearchString += TEXT(" ");
	SearchString += OutputSummary.ToString().ToLower();
	if (InRecipe)
	{
		for (const FText& Keyword : InRecipe->SearchKeywords)
		{
			SearchString += TEXT(" ");
			SearchString += NormalizeSearchString(Keyword);
		}

		for (const FRpgCraftingOutputItem& OutputItem : InRecipe->OutputItems)
		{
			SearchString += TEXT(" ");
			SearchString += GetItemDisplayName(OutputItem.ItemDefinition).ToString().ToLower();
		}
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeDefinition);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeCategory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeTier);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftTime);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SortPriority);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxCraftableQuantity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsUnlocked);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanCraftOne);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasMissingResources);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputSummary);
}

bool URpgCraftingRecipeViewModel::MatchesSearchText(const FText& InSearchText) const
{
	const FString Query = NormalizeSearchString(InSearchText).TrimStartAndEnd();
	return Query.IsEmpty() || SearchString.Contains(Query);
}

void URpgCraftingJobViewModel::InitializeJob(const FRpgCraftingJobEntry& Job, float ServerWorldTime)
{
	JobId = Job.JobId;
	RecipeDefinition = Job.Recipe;
	DisplayName = Job.Recipe ? Job.Recipe->DisplayName : FText::GetEmpty();
	Icon = GetRecipeIcon(Job.Recipe);
	QuantityTotal = Job.QuantityTotal;
	QuantityCompleted = Job.QuantityCompleted;
	State = Job.State;

	const float UnitDuration = FMath::Max(0.0f, Job.FinishServerTime - Job.StartServerTime);
	if (Job.State == ERpgCraftingJobState::Active && UnitDuration > 0.0f)
	{
		Progress = FMath::Clamp((ServerWorldTime - Job.StartServerTime) / UnitDuration, 0.0f, 1.0f);
		RemainingSeconds = FMath::Max(0.0f, Job.FinishServerTime - ServerWorldTime);
	}
	else if (Job.State == ERpgCraftingJobState::Paused)
	{
		Progress = UnitDuration > 0.0f ? FMath::Clamp(1.0f - Job.PausedRemainingTime / UnitDuration, 0.0f, 1.0f) : 0.0f;
		RemainingSeconds = Job.PausedRemainingTime;
	}
	else
	{
		Progress = Job.State == ERpgCraftingJobState::Completed ? 1.0f : 0.0f;
		RemainingSeconds = 0.0f;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(JobId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeDefinition);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(QuantityTotal);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(QuantityCompleted);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(State);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Progress);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RemainingSeconds);
}

void URpgCraftingStationViewModel::BeginDestroy()
{
	UnbindCraftingStation();
	Super::BeginDestroy();
}

void URpgCraftingStationViewModel::BindCraftingStation(URpgCraftingStationComponent* InStation, AActor* InRequestingActor)
{
	if (ObservedStation == InStation && RequestingActor == InRequestingActor)
	{
		Refresh();
		return;
	}

	UnbindCraftingStation();
	ObservedStation = InStation;
	RequestingActor = InRequestingActor;
	RegisterMessageListeners();
	Refresh();
}

void URpgCraftingStationViewModel::UnbindCraftingStation()
{
	UnregisterMessageListeners();
	ObservedStation = nullptr;
	RequestingActor = nullptr;
	OutputInventory = nullptr;
	SelectedRecipe = nullptr;
	bStationPaused = false;
	bCanAutoDepositCraftingOutputs = false;
	bAutoDepositCraftingOutputsEnabled = false;
	bShouldAutoDepositCraftingOutputs = false;
	FilteredRecipes.Reset();
	SelectedIngredients.Reset();
	SelectedOutputs.Reset();
	Jobs.Reset();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedStation);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RequestingActor);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputInventory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanAutoDepositCraftingOutputs);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAutoDepositCraftingOutputsEnabled);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bShouldAutoDepositCraftingOutputs);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedIngredients);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedOutputs);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Jobs);
	OnRecipesChanged.Broadcast();
	OnSelectedRecipeDetailsChanged.Broadcast();
	OnJobsChanged.Broadcast();
}

void URpgCraftingStationViewModel::Refresh()
{
	RebuildStationState();
	RebuildRecipeList();
	RebuildSelectedRecipeDetails();
	RebuildJobs();
}

void URpgCraftingStationViewModel::SelectRecipe(URpgCraftingRecipeDefinition* RecipeDefinition)
{
	SelectedRecipe = RecipeDefinition;
	CraftQuantity = 1;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	RebuildSelectedRecipeDetails();
}

void URpgCraftingStationViewModel::SetSearchText(FText InSearchText)
{
	SearchText = InSearchText;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SearchText);
	RebuildRecipeList();
}

void URpgCraftingStationViewModel::SetCategoryFilter(FGameplayTag InCategoryFilter)
{
	CategoryFilter = InCategoryFilter;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CategoryFilter);
	RebuildRecipeList();
}

void URpgCraftingStationViewModel::SetTierFilter(int32 InTierFilter)
{
	TierFilter = FMath::Max(0, InTierFilter);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TierFilter);
	RebuildRecipeList();
}

void URpgCraftingStationViewModel::SetRecipeSortMode(ERpgCraftingRecipeSortMode InSortMode)
{
	RecipeSortMode = InSortMode;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeSortMode);
	RebuildRecipeList();
}

void URpgCraftingStationViewModel::SetCraftQuantity(int32 InCraftQuantity)
{
	const int32 ClampedMax = FMath::Max(1, MaxSelectedCraftQuantity);
	CraftQuantity = FMath::Clamp(InCraftQuantity, 1, ClampedMax);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	RebuildSelectedRecipeDetails();
}

void URpgCraftingStationViewModel::IncreaseCraftQuantity(int32 Delta)
{
	SetCraftQuantity(CraftQuantity + Delta);
}

void URpgCraftingStationViewModel::SetCraftQuantityToMax()
{
	SetCraftQuantity(FMath::Max(1, MaxSelectedCraftQuantity));
}

TArray<URpgCraftingRecipeViewModel*> URpgCraftingStationViewModel::GetFilteredRecipes() const
{
	TArray<URpgCraftingRecipeViewModel*> Result;
	Result.Reserve(FilteredRecipes.Num());
	for (URpgCraftingRecipeViewModel* Recipe : FilteredRecipes)
	{
		Result.Add(Recipe);
	}
	return Result;
}

TArray<URpgCraftingIngredientViewModel*> URpgCraftingStationViewModel::GetSelectedIngredients() const
{
	TArray<URpgCraftingIngredientViewModel*> Result;
	Result.Reserve(SelectedIngredients.Num());
	for (URpgCraftingIngredientViewModel* Ingredient : SelectedIngredients)
	{
		Result.Add(Ingredient);
	}
	return Result;
}

TArray<URpgCraftingOutputViewModel*> URpgCraftingStationViewModel::GetSelectedOutputs() const
{
	TArray<URpgCraftingOutputViewModel*> Result;
	Result.Reserve(SelectedOutputs.Num());
	for (URpgCraftingOutputViewModel* Output : SelectedOutputs)
	{
		Result.Add(Output);
	}
	return Result;
}

TArray<URpgCraftingJobViewModel*> URpgCraftingStationViewModel::GetJobs() const
{
	TArray<URpgCraftingJobViewModel*> Result;
	Result.Reserve(Jobs.Num());
	for (URpgCraftingJobViewModel* Job : Jobs)
	{
		Result.Add(Job);
	}
	return Result;
}

void URpgCraftingStationViewModel::RegisterMessageListeners()
{
	UnregisterMessageListeners();
	UWorld* World = ObservedStation ? ObservedStation->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	CraftingStationChangedHandle = MessageSubsystem.RegisterListener<FRpgCraftingStationChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Crafting.Message.StationChanged")),
		this,
		&ThisClass::HandleCraftingStationChanged);
	RecipeUnlockChangedHandle = MessageSubsystem.RegisterListener<FRpgRecipeUnlockChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Crafting.Message.RecipeUnlockChanged")),
		this,
		&ThisClass::HandleRecipeUnlockChanged);
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandleInventoryChanged);
	BaseStorageChangedHandle = MessageSubsystem.RegisterListener<FRpgBaseResourceChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.BaseStorage.Message.Changed")),
		this,
		&ThisClass::HandleBaseStorageChanged);
}

void URpgCraftingStationViewModel::UnregisterMessageListeners()
{
	if (CraftingStationChangedHandle.IsValid())
	{
		CraftingStationChangedHandle.Unregister();
	}

	if (RecipeUnlockChangedHandle.IsValid())
	{
		RecipeUnlockChangedHandle.Unregister();
	}

	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}

	if (BaseStorageChangedHandle.IsValid())
	{
		BaseStorageChangedHandle.Unregister();
	}
}

void URpgCraftingStationViewModel::RebuildStationState()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	OutputInventory = Station ? Station->GetOutputInventory() : nullptr;
	bStationPaused = Station && Station->IsCraftingPaused();
	bCanAutoDepositCraftingOutputs = Station && Station->HasCraftingOutputAutoDepositAccess();
	bAutoDepositCraftingOutputsEnabled = Station && Station->IsCraftingOutputAutoDepositEnabled();
	bShouldAutoDepositCraftingOutputs = Station && Station->ShouldAutoDepositCraftingOutputs();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputInventory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanAutoDepositCraftingOutputs);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAutoDepositCraftingOutputsEnabled);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bShouldAutoDepositCraftingOutputs);
}

void URpgCraftingStationViewModel::RebuildRecipeList()
{
	FilteredRecipes.Reset();
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	if (!Station)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
		OnRecipesChanged.Broadcast();
		return;
	}

	for (URpgCraftingRecipeDefinition* Recipe : Station->GetAvailableRecipes())
	{
		if (!Recipe)
		{
			continue;
		}

		if (CategoryFilter.IsValid() && Recipe->RecipeCategory != CategoryFilter)
		{
			continue;
		}

		if (TierFilter > 0 && Recipe->RecipeTier != TierFilter)
		{
			continue;
		}

		URpgCraftingRecipeViewModel* RecipeViewModel = NewObject<URpgCraftingRecipeViewModel>(this);
		RecipeViewModel->InitializeRecipe(Station, Actor, Recipe);
		if (!RecipeViewModel->MatchesSearchText(SearchText))
		{
			continue;
		}

		FilteredRecipes.Add(RecipeViewModel);
	}

	auto CompareDefault = [Station, Actor](const URpgCraftingRecipeDefinition* A, const URpgCraftingRecipeDefinition* B)
	{
		const bool bCraftableA = IsRecipeCraftable(Station, Actor, A);
		const bool bCraftableB = IsRecipeCraftable(Station, Actor, B);
		if (bCraftableA != bCraftableB)
		{
			return bCraftableA;
		}

		if (A && B && A->RecipeTier != B->RecipeTier)
		{
			return A->RecipeTier < B->RecipeTier;
		}

		if (A && B && A->SortPriority != B->SortPriority)
		{
			return A->SortPriority < B->SortPriority;
		}

		return GetRecipeSortName(A).Compare(GetRecipeSortName(B), ESearchCase::IgnoreCase) < 0;
	};

	FilteredRecipes.Sort([this, Station, Actor, &CompareDefault](const TObjectPtr<URpgCraftingRecipeViewModel>& A, const TObjectPtr<URpgCraftingRecipeViewModel>& B)
	{
		const URpgCraftingRecipeDefinition* RecipeA = A ? A->GetRecipeDefinition() : nullptr;
		const URpgCraftingRecipeDefinition* RecipeB = B ? B->GetRecipeDefinition() : nullptr;
		switch (RecipeSortMode)
		{
		case ERpgCraftingRecipeSortMode::Tier:
			if (RecipeA && RecipeB && RecipeA->RecipeTier != RecipeB->RecipeTier)
			{
				return RecipeA->RecipeTier < RecipeB->RecipeTier;
			}
			break;

		case ERpgCraftingRecipeSortMode::Category:
			{
				const int32 CategoryCompare = GetCategorySortString(RecipeA).Compare(GetCategorySortString(RecipeB), ESearchCase::IgnoreCase);
				if (CategoryCompare != 0)
				{
					return CategoryCompare < 0;
				}
			}
			break;

		case ERpgCraftingRecipeSortMode::Name:
			return GetRecipeSortName(RecipeA).Compare(GetRecipeSortName(RecipeB), ESearchCase::IgnoreCase) < 0;

		case ERpgCraftingRecipeSortMode::Craftable:
			{
				const bool bCraftableA = IsRecipeCraftable(Station, Actor, RecipeA);
				const bool bCraftableB = IsRecipeCraftable(Station, Actor, RecipeB);
				if (bCraftableA != bCraftableB)
				{
					return bCraftableA;
				}
			}
			break;

		case ERpgCraftingRecipeSortMode::RecentUnlocked:
			{
				const bool bUnlockedA = Station && Station->IsRecipeUnlocked(RecipeA);
				const bool bUnlockedB = Station && Station->IsRecipeUnlocked(RecipeB);
				if (bUnlockedA != bUnlockedB)
				{
					return bUnlockedA;
				}
			}
			break;

		case ERpgCraftingRecipeSortMode::Default:
			break;
		}

		return CompareDefault(RecipeA, RecipeB);
	});

	if (!SelectedRecipe && FilteredRecipes.Num() > 0)
	{
		SelectedRecipe = FilteredRecipes[0]->GetRecipeDefinition();
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
	OnRecipesChanged.Broadcast();
}

void URpgCraftingStationViewModel::RebuildSelectedRecipeDetails()
{
	SelectedIngredients.Reset();
	SelectedOutputs.Reset();
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	MaxSelectedCraftQuantity = Station && SelectedRecipe ? Station->GetMaxCraftableQuantity(Actor, SelectedRecipe) : 0;
	if (CraftQuantity > FMath::Max(1, MaxSelectedCraftQuantity))
	{
		CraftQuantity = FMath::Max(1, MaxSelectedCraftQuantity);
	}

	bCanCraftSelectedRecipe = Station && SelectedRecipe && Station->CanCraftRecipeQuantity(Actor, SelectedRecipe, CraftQuantity);
	SelectedTotalCraftTime = SelectedRecipe ? FMath::Max(0.0f, SelectedRecipe->CraftTime) * CraftQuantity : 0.0f;
	bStationPaused = Station && Station->IsCraftingPaused();

	if (Station && SelectedRecipe)
	{
		for (const FRpgCraftingResourceCost& RequiredResource : SelectedRecipe->RequiredResources)
		{
			URpgCraftingIngredientViewModel* IngredientViewModel = NewObject<URpgCraftingIngredientViewModel>(this);
			IngredientViewModel->InitializeIngredient(
				RequiredResource.ItemDefinition,
				RequiredResource.Count * CraftQuantity,
				Station->GetAvailableResourceCount(Actor, RequiredResource.ItemDefinition));
			SelectedIngredients.Add(IngredientViewModel);
		}

		for (const FRpgCraftingOutputItem& OutputItem : SelectedRecipe->OutputItems)
		{
			URpgCraftingOutputViewModel* OutputViewModel = NewObject<URpgCraftingOutputViewModel>(this);
			OutputViewModel->InitializeOutput(OutputItem.ItemDefinition, OutputItem.Count * CraftQuantity);
			SelectedOutputs.Add(OutputViewModel);
		}
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxSelectedCraftQuantity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedTotalCraftTime);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanCraftSelectedRecipe);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedIngredients);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedOutputs);
	OnSelectedRecipeDetailsChanged.Broadcast();
}

void URpgCraftingStationViewModel::RebuildJobs()
{
	Jobs.Reset();
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	if (Station)
	{
		const float ServerTime = GetServerWorldTimeSeconds(Station);
		for (const FRpgCraftingJobEntry& Job : Station->GetCraftingJobs())
		{
			URpgCraftingJobViewModel* JobViewModel = NewObject<URpgCraftingJobViewModel>(this);
			JobViewModel->InitializeJob(Job, ServerTime);
			Jobs.Add(JobViewModel);
		}

		bStationPaused = Station->IsCraftingPaused();
	}
	else
	{
		bStationPaused = false;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Jobs);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	OnJobsChanged.Broadcast();
}

void URpgCraftingStationViewModel::HandleCraftingStationChanged(FGameplayTag Channel, const FRpgCraftingStationChangeMessage& Message)
{
	if (ObservedStation.Get() == Message.Station)
	{
		Refresh();
	}
}

void URpgCraftingStationViewModel::HandleRecipeUnlockChanged(FGameplayTag Channel, const FRpgRecipeUnlockChangeMessage& Message)
{
	Refresh();
}

void URpgCraftingStationViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	RebuildRecipeList();
	RebuildSelectedRecipeDetails();
}

void URpgCraftingStationViewModel::HandleBaseStorageChanged(FGameplayTag Channel, const FRpgBaseResourceChangeMessage& Message)
{
	RebuildRecipeList();
	RebuildSelectedRecipeDetails();
}
