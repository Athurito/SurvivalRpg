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

	bool AreRecipeOrdersEqual(
		const TArray<URpgCraftingRecipeDefinition*>& PreviousRecipes,
		const TArray<TObjectPtr<URpgCraftingRecipeViewModel>>& NewRecipes)
	{
		if (PreviousRecipes.Num() != NewRecipes.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < PreviousRecipes.Num(); ++Index)
		{
			const URpgCraftingRecipeDefinition* NewRecipe = NewRecipes[Index] ? NewRecipes[Index]->GetRecipeDefinition() : nullptr;
			if (PreviousRecipes[Index] != NewRecipe)
			{
				return false;
			}
		}

		return true;
	}

	bool AreJobOrdersEqual(const TArray<FGuid>& PreviousJobIds, const TArray<TObjectPtr<URpgCraftingJobViewModel>>& NewJobs)
	{
		if (PreviousJobIds.Num() != NewJobs.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < PreviousJobIds.Num(); ++Index)
		{
			const FGuid NewJobId = NewJobs[Index] ? NewJobs[Index]->GetJobId() : FGuid();
			if (PreviousJobIds[Index] != NewJobId)
			{
				return false;
			}
		}

		return true;
	}

	FText MakePauseResumeButtonText(bool bIsPaused)
	{
		return bIsPaused
			? NSLOCTEXT("RpgCrafting", "ResumeCraftingStationButton", "Resume")
			: NSLOCTEXT("RpgCrafting", "PauseCraftingStationButton", "Pause");
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
	CraftQuantity = 1;
	MaxSelectedCraftQuantity = 0;
	SelectedTotalCraftTime = 0.0f;
	bCanCraftSelectedRecipe = false;
	bStationPaused = false;
	PauseResumeButtonText = MakePauseResumeButtonText(bStationPaused);
	bCanAutoDepositCraftingOutputs = false;
	bAutoDepositCraftingOutputsEnabled = false;
	bShouldAutoDepositCraftingOutputs = false;
	RebuildActionAvailability();
	FilteredRecipes.Reset();
	SelectedIngredients.Reset();
	SelectedOutputs.Reset();
	Jobs.Reset();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedStation);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RequestingActor);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputInventory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxSelectedCraftQuantity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedTotalCraftTime);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanCraftSelectedRecipe);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
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
	RefreshStationState();
	RefreshRecipesAndDetails();
	RefreshJobs();
}

void URpgCraftingStationViewModel::RefreshStationState()
{
	RebuildStationState();
}

void URpgCraftingStationViewModel::RefreshRecipesAndDetails()
{
	RebuildRecipeList();
	RebuildSelectedRecipeDetails();
}

void URpgCraftingStationViewModel::RefreshSelectedRecipeDetails()
{
	RebuildSelectedRecipeDetails();
}

void URpgCraftingStationViewModel::RefreshJobs()
{
	RebuildJobs();
}

void URpgCraftingStationViewModel::SelectRecipe(URpgCraftingRecipeDefinition* RecipeDefinition)
{
	if (SelectedRecipe == RecipeDefinition)
	{
		return;
	}

	SelectedRecipe = RecipeDefinition;
	CraftQuantity = 1;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	RebuildSelectedRecipeDetails();
}

void URpgCraftingStationViewModel::SetSearchText(FText InSearchText)
{
	if (SearchText.ToString().Equals(InSearchText.ToString(), ESearchCase::CaseSensitive))
	{
		return;
	}

	SearchText = InSearchText;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SearchText);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::SetCategoryFilter(FGameplayTag InCategoryFilter)
{
	if (CategoryFilter == InCategoryFilter)
	{
		return;
	}

	CategoryFilter = InCategoryFilter;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CategoryFilter);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::SetTierFilter(int32 InTierFilter)
{
	const int32 NewTierFilter = FMath::Max(0, InTierFilter);
	if (TierFilter == NewTierFilter)
	{
		return;
	}

	TierFilter = NewTierFilter;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TierFilter);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::SetRecipeSortMode(ERpgCraftingRecipeSortMode InSortMode)
{
	if (RecipeSortMode == InSortMode)
	{
		return;
	}

	RecipeSortMode = InSortMode;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeSortMode);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::SetCraftQuantity(int32 InCraftQuantity)
{
	const int32 ClampedMax = FMath::Max(1, MaxSelectedCraftQuantity);
	const int32 NewCraftQuantity = FMath::Clamp(InCraftQuantity, 1, ClampedMax);
	if (CraftQuantity == NewCraftQuantity)
	{
		return;
	}

	CraftQuantity = NewCraftQuantity;
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

bool URpgCraftingStationViewModel::CanSetCraftQuantity(int32 InCraftQuantity) const
{
	if (InCraftQuantity == 1)
	{
		return bCanSetCraftQuantityToOne;
	}

	if (InCraftQuantity == 5)
	{
		return bCanSetCraftQuantityToFive;
	}

	if (InCraftQuantity == 10)
	{
		return bCanSetCraftQuantityToTen;
	}

	return InCraftQuantity > 0
		&& InCraftQuantity <= MaxSelectedCraftQuantity
		&& InCraftQuantity != CraftQuantity;
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
	PauseResumeButtonText = MakePauseResumeButtonText(bStationPaused);
	bCanAutoDepositCraftingOutputs = Station && Station->HasCraftingOutputAutoDepositAccess();
	bAutoDepositCraftingOutputsEnabled = Station && Station->IsCraftingOutputAutoDepositEnabled();
	bShouldAutoDepositCraftingOutputs = Station && Station->ShouldAutoDepositCraftingOutputs();
	RebuildActionAvailability();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputInventory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanAutoDepositCraftingOutputs);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAutoDepositCraftingOutputsEnabled);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bShouldAutoDepositCraftingOutputs);
}

void URpgCraftingStationViewModel::RebuildRecipeList()
{
	TArray<URpgCraftingRecipeDefinition*> PreviousRecipeOrder;
	PreviousRecipeOrder.Reserve(FilteredRecipes.Num());

	TMap<URpgCraftingRecipeDefinition*, URpgCraftingRecipeViewModel*> PreviousViewModelsByRecipe;
	for (URpgCraftingRecipeViewModel* ExistingViewModel : FilteredRecipes)
	{
		URpgCraftingRecipeDefinition* ExistingRecipe = ExistingViewModel ? ExistingViewModel->GetRecipeDefinition() : nullptr;
		PreviousRecipeOrder.Add(ExistingRecipe);
		if (ExistingRecipe && ExistingViewModel)
		{
			PreviousViewModelsByRecipe.Add(ExistingRecipe, ExistingViewModel);
		}
	}

	TArray<TObjectPtr<URpgCraftingRecipeViewModel>> NewFilteredRecipes;
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	URpgCraftingRecipeDefinition* PreviousSelectedRecipe = SelectedRecipe.Get();
	if (!Station)
	{
		FilteredRecipes.Reset();
		SelectedRecipe = nullptr;
		CraftQuantity = 1;

		if (PreviousSelectedRecipe)
		{
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
		}

		if (PreviousRecipeOrder.Num() > 0)
		{
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
			OnRecipesChanged.Broadcast();
		}
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

		URpgCraftingRecipeViewModel* RecipeViewModel = nullptr;
		if (URpgCraftingRecipeViewModel** ExistingViewModel = PreviousViewModelsByRecipe.Find(Recipe))
		{
			RecipeViewModel = *ExistingViewModel;
		}

		if (!RecipeViewModel)
		{
			RecipeViewModel = NewObject<URpgCraftingRecipeViewModel>(this);
		}

		RecipeViewModel->InitializeRecipe(Station, Actor, Recipe);
		if (!RecipeViewModel->MatchesSearchText(SearchText))
		{
			continue;
		}

		NewFilteredRecipes.Add(RecipeViewModel);
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

	NewFilteredRecipes.Sort([this, Station, Actor, &CompareDefault](const URpgCraftingRecipeViewModel& A, const URpgCraftingRecipeViewModel& B)
	{
		const URpgCraftingRecipeDefinition* RecipeA = A.GetRecipeDefinition();
		const URpgCraftingRecipeDefinition* RecipeB = B.GetRecipeDefinition();
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

	const bool bSelectedRecipeStillVisible = SelectedRecipe && NewFilteredRecipes.ContainsByPredicate([this](const TObjectPtr<URpgCraftingRecipeViewModel>& RecipeViewModel)
	{
		return RecipeViewModel && RecipeViewModel->GetRecipeDefinition() == SelectedRecipe;
	});

	if (!bSelectedRecipeStillVisible)
	{
		SelectedRecipe = NewFilteredRecipes.Num() > 0 ? NewFilteredRecipes[0]->GetRecipeDefinition() : nullptr;
		CraftQuantity = 1;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}

	const bool bRecipeListChanged = !AreRecipeOrdersEqual(PreviousRecipeOrder, NewFilteredRecipes);
	FilteredRecipes = MoveTemp(NewFilteredRecipes);
	if (bRecipeListChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
		OnRecipesChanged.Broadcast();
	}
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
	PauseResumeButtonText = MakePauseResumeButtonText(bStationPaused);
	RebuildActionAvailability();

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
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedIngredients);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedOutputs);
	OnSelectedRecipeDetailsChanged.Broadcast();
}

void URpgCraftingStationViewModel::RebuildJobs()
{
	TArray<FGuid> PreviousJobOrder;
	PreviousJobOrder.Reserve(Jobs.Num());

	TMap<FGuid, URpgCraftingJobViewModel*> PreviousViewModelsByJobId;
	for (URpgCraftingJobViewModel* ExistingViewModel : Jobs)
	{
		const FGuid ExistingJobId = ExistingViewModel ? ExistingViewModel->GetJobId() : FGuid();
		PreviousJobOrder.Add(ExistingJobId);
		if (ExistingJobId.IsValid() && ExistingViewModel)
		{
			PreviousViewModelsByJobId.Add(ExistingJobId, ExistingViewModel);
		}
	}

	TArray<TObjectPtr<URpgCraftingJobViewModel>> NewJobs;
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	const bool bPreviousStationPaused = bStationPaused;
	if (Station)
	{
		const float ServerTime = GetServerWorldTimeSeconds(Station);
		for (const FRpgCraftingJobEntry& Job : Station->GetCraftingJobs())
		{
			URpgCraftingJobViewModel* JobViewModel = nullptr;
			if (URpgCraftingJobViewModel** ExistingViewModel = PreviousViewModelsByJobId.Find(Job.JobId))
			{
				JobViewModel = *ExistingViewModel;
			}

			if (!JobViewModel)
			{
				JobViewModel = NewObject<URpgCraftingJobViewModel>(this);
			}

			JobViewModel->InitializeJob(Job, ServerTime);
			NewJobs.Add(JobViewModel);
		}

		bStationPaused = Station->IsCraftingPaused();
	}
	else
	{
		bStationPaused = false;
	}

	const bool bJobListChanged = !AreJobOrdersEqual(PreviousJobOrder, NewJobs);
	Jobs = MoveTemp(NewJobs);
	if (bJobListChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Jobs);
	}

	if (bJobListChanged || PreviousJobOrder.Num() > 0 || Jobs.Num() > 0)
	{
		OnJobsChanged.Broadcast();
	}

	if (bPreviousStationPaused != bStationPaused)
	{
		PauseResumeButtonText = MakePauseResumeButtonText(bStationPaused);
		RebuildActionAvailability();
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
	}
}

void URpgCraftingStationViewModel::RebuildActionAvailability()
{
	const bool bNewCanSubmitSelectedRecipe = bCanCraftSelectedRecipe;
	if (bCanSubmitSelectedRecipe != bNewCanSubmitSelectedRecipe)
	{
		bCanSubmitSelectedRecipe = bNewCanSubmitSelectedRecipe;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSubmitSelectedRecipe);
	}

	const bool bNewCanDecreaseCraftQuantity = CraftQuantity > 1;
	if (bCanDecreaseCraftQuantity != bNewCanDecreaseCraftQuantity)
	{
		bCanDecreaseCraftQuantity = bNewCanDecreaseCraftQuantity;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanDecreaseCraftQuantity);
	}

	const bool bNewCanIncreaseCraftQuantity = CraftQuantity < MaxSelectedCraftQuantity;
	if (bCanIncreaseCraftQuantity != bNewCanIncreaseCraftQuantity)
	{
		bCanIncreaseCraftQuantity = bNewCanIncreaseCraftQuantity;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanIncreaseCraftQuantity);
	}

	const bool bNewCanSetCraftQuantityToOne = MaxSelectedCraftQuantity >= 1 && CraftQuantity != 1;
	if (bCanSetCraftQuantityToOne != bNewCanSetCraftQuantityToOne)
	{
		bCanSetCraftQuantityToOne = bNewCanSetCraftQuantityToOne;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToOne);
	}

	const bool bNewCanSetCraftQuantityToFive = MaxSelectedCraftQuantity >= 5 && CraftQuantity != 5;
	if (bCanSetCraftQuantityToFive != bNewCanSetCraftQuantityToFive)
	{
		bCanSetCraftQuantityToFive = bNewCanSetCraftQuantityToFive;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToFive);
	}

	const bool bNewCanSetCraftQuantityToTen = MaxSelectedCraftQuantity >= 10 && CraftQuantity != 10;
	if (bCanSetCraftQuantityToTen != bNewCanSetCraftQuantityToTen)
	{
		bCanSetCraftQuantityToTen = bNewCanSetCraftQuantityToTen;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToTen);
	}

	const bool bNewCanSetCraftQuantityToMax = MaxSelectedCraftQuantity > 1 && CraftQuantity < MaxSelectedCraftQuantity;
	if (bCanSetCraftQuantityToMax != bNewCanSetCraftQuantityToMax)
	{
		bCanSetCraftQuantityToMax = bNewCanSetCraftQuantityToMax;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToMax);
	}

	const bool bNewHasCraftQuantityOptions = MaxSelectedCraftQuantity > 1;
	if (bHasCraftQuantityOptions != bNewHasCraftQuantityOptions)
	{
		bHasCraftQuantityOptions = bNewHasCraftQuantityOptions;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasCraftQuantityOptions);
	}

	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	const bool bNewCanToggleCraftingPause = Station && Actor && Station->CanActorAccess(Actor);
	if (bCanToggleCraftingPause != bNewCanToggleCraftingPause)
	{
		bCanToggleCraftingPause = bNewCanToggleCraftingPause;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanToggleCraftingPause);
	}

	const bool bNewCanPauseCraftingStation = bCanToggleCraftingPause && !bStationPaused;
	if (bCanPauseCraftingStation != bNewCanPauseCraftingStation)
	{
		bCanPauseCraftingStation = bNewCanPauseCraftingStation;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanPauseCraftingStation);
	}

	const bool bNewCanResumeCraftingStation = bCanToggleCraftingPause && bStationPaused;
	if (bCanResumeCraftingStation != bNewCanResumeCraftingStation)
	{
		bCanResumeCraftingStation = bNewCanResumeCraftingStation;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanResumeCraftingStation);
	}
}

void URpgCraftingStationViewModel::HandleCraftingStationChanged(FGameplayTag Channel, const FRpgCraftingStationChangeMessage& Message)
{
	if (ObservedStation.Get() == Message.Station)
	{
		RefreshStationState();
		RefreshJobs();
	}
}

void URpgCraftingStationViewModel::HandleRecipeUnlockChanged(FGameplayTag Channel, const FRpgRecipeUnlockChangeMessage& Message)
{
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	if (!Station)
	{
		return;
	}

	URpgInventoryManagerComponent* ChangedInventory = Cast<URpgInventoryManagerComponent>(Message.InventoryOwner.Get());
	if (!ChangedInventory)
	{
		return;
	}

	if (ChangedInventory == Station->GetOutputInventory())
	{
		RefreshStationState();
		RefreshJobs();
		return;
	}

	const TArray<URpgInventoryManagerComponent*> ResourceInventories = Station->GetResourceInventories(RequestingActor.Get());
	if (ResourceInventories.Contains(ChangedInventory))
	{
		RefreshRecipesAndDetails();
	}
}

void URpgCraftingStationViewModel::HandleBaseStorageChanged(FGameplayTag Channel, const FRpgBaseResourceChangeMessage& Message)
{
	RefreshRecipesAndDetails();
}
