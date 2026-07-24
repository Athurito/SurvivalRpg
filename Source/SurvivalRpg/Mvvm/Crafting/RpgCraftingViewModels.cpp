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
	constexpr ETextIdenticalModeFlags CraftingTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	namespace CraftingRefreshDomains
	{
		constexpr uint8 Station = 1 << 0;
		constexpr uint8 RecipesAndDetails = 1 << 1;
		constexpr uint8 Jobs = 1 << 2;
		constexpr uint8 All = Station | RecipesAndDetails | Jobs;
	}

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

	template <typename ViewModelType>
	bool AreViewModelArraysEqual(
		const TArray<TObjectPtr<ViewModelType>>& A,
		const TArray<TObjectPtr<ViewModelType>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Get() != B[Index].Get())
			{
				return false;
			}
		}

		return true;
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
			const URpgCraftingRecipeDefinition* NewRecipe =
				NewRecipes[Index]
					? NewRecipes[Index]->GetRecipeDefinition()
					: nullptr;
			if (PreviousRecipes[Index] != NewRecipe)
			{
				return false;
			}
		}

		return true;
	}

	bool AreJobOrdersEqual(
		const TArray<FGuid>& PreviousJobIds,
		const TArray<TObjectPtr<URpgCraftingJobViewModel>>& NewJobs)
	{
		if (PreviousJobIds.Num() != NewJobs.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < PreviousJobIds.Num(); ++Index)
		{
			const FGuid NewJobId =
				NewJobs[Index] ? NewJobs[Index]->GetJobId() : FGuid();
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

	bool CanCancelJobState(ERpgCraftingJobState State)
	{
		return State == ERpgCraftingJobState::Queued
			|| State == ERpgCraftingJobState::Active
			|| State == ERpgCraftingJobState::Paused
			|| State == ERpgCraftingJobState::BlockedOutput;
	}
}

void URpgCraftingIngredientViewModel::InitializeIngredient(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InRequiredCount, int32 InAvailableCount)
{
	const TSubclassOf<URpgInventoryItemDefinition> NewItemDefinition = InItemDefinition;
	const FText NewDisplayName = GetItemDisplayName(NewItemDefinition);
	const TSoftObjectPtr<UTexture2D> NewIcon = GetItemIcon(NewItemDefinition);
	const int32 NewRequiredCount = FMath::Max(0, InRequiredCount);
	const int32 NewAvailableCount = FMath::Max(0, InAvailableCount);
	const int32 NewMissingCount = FMath::Max(0, NewRequiredCount - NewAvailableCount);
	const bool bNewHasEnough = NewMissingCount <= 0;

	const bool bItemDefinitionChanged = ItemDefinition != NewItemDefinition;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, CraftingTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bRequiredCountChanged = RequiredCount != NewRequiredCount;
	const bool bAvailableCountChanged = AvailableCount != NewAvailableCount;
	const bool bMissingCountChanged = MissingCount != NewMissingCount;
	const bool bHasEnoughChanged = bHasEnough != bNewHasEnough;

	ItemDefinition = NewItemDefinition;
	DisplayName = NewDisplayName;
	Icon = NewIcon;
	RequiredCount = NewRequiredCount;
	AvailableCount = NewAvailableCount;
	MissingCount = NewMissingCount;
	bHasEnough = bNewHasEnough;

	if (bItemDefinitionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemDefinition);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bRequiredCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RequiredCount);
	}
	if (bAvailableCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AvailableCount);
	}
	if (bMissingCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MissingCount);
	}
	if (bHasEnoughChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasEnough);
	}
}

void URpgCraftingOutputViewModel::InitializeOutput(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InOutputCount)
{
	const TSubclassOf<URpgInventoryItemDefinition> NewItemDefinition = InItemDefinition;
	const FText NewDisplayName = GetItemDisplayName(NewItemDefinition);
	const TSoftObjectPtr<UTexture2D> NewIcon = GetItemIcon(NewItemDefinition);
	const int32 NewOutputCount = FMath::Max(0, InOutputCount);

	const bool bItemDefinitionChanged = ItemDefinition != NewItemDefinition;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, CraftingTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bOutputCountChanged = OutputCount != NewOutputCount;

	ItemDefinition = NewItemDefinition;
	DisplayName = NewDisplayName;
	Icon = NewIcon;
	OutputCount = NewOutputCount;

	if (bItemDefinitionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemDefinition);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bOutputCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputCount);
	}
}

void URpgCraftingRecipeViewModel::InitializeRecipe(URpgCraftingStationComponent* InStation, AActor* InRequestingActor, URpgCraftingRecipeDefinition* InRecipe)
{
	const TObjectPtr<URpgCraftingRecipeDefinition> NewRecipeDefinition = InRecipe;
	const FText NewDisplayName = InRecipe ? InRecipe->DisplayName : FText::GetEmpty();
	const FText NewDescription = InRecipe ? InRecipe->Description : FText::GetEmpty();
	const TSoftObjectPtr<UTexture2D> NewIcon = GetRecipeIcon(InRecipe);
	const FGameplayTag NewRecipeCategory = InRecipe ? InRecipe->RecipeCategory : FGameplayTag();
	const int32 NewRecipeTier = InRecipe ? InRecipe->RecipeTier : 1;
	const float NewCraftTime = InRecipe ? InRecipe->CraftTime : 0.0f;
	const int32 NewSortPriority = InRecipe ? InRecipe->SortPriority : 0;
	const int32 NewMaxCraftableQuantity =
		InStation && InRecipe ? InStation->GetMaxCraftableQuantity(InRequestingActor, InRecipe) : 0;
	const bool bNewIsUnlocked = InStation && InRecipe ? InStation->IsRecipeUnlocked(InRecipe) : false;
	const bool bNewCanCraftOne = InStation && InRecipe
		? InStation->CanCraftRecipe(InRequestingActor, InRecipe)
		: false;
	const bool bNewHasMissingResources = bNewIsUnlocked && NewMaxCraftableQuantity <= 0;

	FText NewOutputSummary;
	if (InRecipe && InRecipe->OutputItems.Num() == 1)
	{
		const FRpgCraftingOutputItem& OutputItem = InRecipe->OutputItems[0];
		NewOutputSummary = FText::Format(
			NSLOCTEXT("RpgCrafting", "SingleOutputSummary", "{0}x {1}"),
			FText::AsNumber(OutputItem.Count),
			GetItemDisplayName(OutputItem.ItemDefinition));
	}
	else if (InRecipe && InRecipe->OutputItems.Num() > 1)
	{
		NewOutputSummary = FText::Format(
			NSLOCTEXT("RpgCrafting", "MultiOutputSummary", "{0} Outputs"),
			FText::AsNumber(InRecipe->OutputItems.Num()));
	}
	else
	{
		NewOutputSummary = FText::GetEmpty();
	}

	FString NewSearchString;
	NewSearchString += NormalizeSearchString(NewDisplayName);
	NewSearchString += TEXT(" ");
	NewSearchString += NormalizeSearchString(NewDescription);
	NewSearchString += TEXT(" ");
	NewSearchString += NewOutputSummary.ToString().ToLower();
	if (InRecipe)
	{
		for (const FText& Keyword : InRecipe->SearchKeywords)
		{
			NewSearchString += TEXT(" ");
			NewSearchString += NormalizeSearchString(Keyword);
		}

		for (const FRpgCraftingOutputItem& OutputItem : InRecipe->OutputItems)
		{
			NewSearchString += TEXT(" ");
			NewSearchString += GetItemDisplayName(OutputItem.ItemDefinition).ToString().ToLower();
		}
	}

	const bool bRecipeDefinitionChanged = RecipeDefinition != NewRecipeDefinition;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, CraftingTextIdentityFlags);
	const bool bDescriptionChanged =
		!Description.IdenticalTo(NewDescription, CraftingTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bRecipeCategoryChanged = RecipeCategory != NewRecipeCategory;
	const bool bRecipeTierChanged = RecipeTier != NewRecipeTier;
	const bool bCraftTimeChanged = CraftTime != NewCraftTime;
	const bool bSortPriorityChanged = SortPriority != NewSortPriority;
	const bool bMaxCraftableQuantityChanged = MaxCraftableQuantity != NewMaxCraftableQuantity;
	const bool bIsUnlockedChanged = bIsUnlocked != bNewIsUnlocked;
	const bool bCanCraftOneChanged = bCanCraftOne != bNewCanCraftOne;
	const bool bHasMissingResourcesChanged = bHasMissingResources != bNewHasMissingResources;
	const bool bOutputSummaryChanged =
		!OutputSummary.IdenticalTo(NewOutputSummary, CraftingTextIdentityFlags);

	RecipeDefinition = NewRecipeDefinition;
	DisplayName = NewDisplayName;
	Description = NewDescription;
	Icon = NewIcon;
	RecipeCategory = NewRecipeCategory;
	RecipeTier = NewRecipeTier;
	CraftTime = NewCraftTime;
	SortPriority = NewSortPriority;
	MaxCraftableQuantity = NewMaxCraftableQuantity;
	bIsUnlocked = bNewIsUnlocked;
	bCanCraftOne = bNewCanCraftOne;
	bHasMissingResources = bNewHasMissingResources;
	OutputSummary = NewOutputSummary;
	SearchString = MoveTemp(NewSearchString);

	if (bRecipeDefinitionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeDefinition);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bDescriptionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bRecipeCategoryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeCategory);
	}
	if (bRecipeTierChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeTier);
	}
	if (bCraftTimeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftTime);
	}
	if (bSortPriorityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SortPriority);
	}
	if (bMaxCraftableQuantityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxCraftableQuantity);
	}
	if (bIsUnlockedChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsUnlocked);
	}
	if (bCanCraftOneChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanCraftOne);
	}
	if (bHasMissingResourcesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasMissingResources);
	}
	if (bOutputSummaryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputSummary);
	}
}

bool URpgCraftingRecipeViewModel::MatchesSearchText(const FText& InSearchText) const
{
	const FString Query = NormalizeSearchString(InSearchText).TrimStartAndEnd();
	return Query.IsEmpty() || SearchString.Contains(Query);
}

void URpgCraftingJobViewModel::InitializeJob(const FRpgCraftingJobEntry& Job, float ServerWorldTime)
{
	InitializeJobForStation(nullptr, Job, ServerWorldTime);
}

void URpgCraftingJobViewModel::InitializeJobForStation(URpgCraftingStationComponent* InCraftingStation, const FRpgCraftingJobEntry& Job, float ServerWorldTime)
{
	const TObjectPtr<URpgCraftingStationComponent> NewCraftingStation = InCraftingStation;
	const FGuid NewJobId = Job.JobId;
	const TObjectPtr<URpgCraftingRecipeDefinition> NewRecipeDefinition = Job.Recipe;
	const FText NewDisplayName = Job.Recipe ? Job.Recipe->DisplayName : FText::GetEmpty();
	const TSoftObjectPtr<UTexture2D> NewIcon = GetRecipeIcon(Job.Recipe);
	const int32 NewQuantityTotal = Job.QuantityTotal;
	const int32 NewQuantityCompleted = Job.QuantityCompleted;
	const ERpgCraftingJobState NewState = Job.State;
	const bool bNewCanCancelJob =
		NewCraftingStation && NewJobId.IsValid() && CanCancelJobState(NewState);

	const float UnitDuration = FMath::Max(0.0f, Job.FinishServerTime - Job.StartServerTime);
	float NewProgress = 0.0f;
	float NewRemainingSeconds = 0.0f;
	if (Job.State == ERpgCraftingJobState::Active && UnitDuration > 0.0f)
	{
		NewProgress = FMath::Clamp((ServerWorldTime - Job.StartServerTime) / UnitDuration, 0.0f, 1.0f);
		NewRemainingSeconds = FMath::Max(0.0f, Job.FinishServerTime - ServerWorldTime);
	}
	else if (Job.State == ERpgCraftingJobState::Paused)
	{
		NewProgress = UnitDuration > 0.0f
			? FMath::Clamp(1.0f - Job.PausedRemainingTime / UnitDuration, 0.0f, 1.0f)
			: 0.0f;
		NewRemainingSeconds = Job.PausedRemainingTime;
	}
	else
	{
		NewProgress = Job.State == ERpgCraftingJobState::Completed ? 1.0f : 0.0f;
	}

	const bool bCraftingStationChanged = CraftingStation != NewCraftingStation;
	const bool bJobIdChanged = JobId != NewJobId;
	const bool bRecipeDefinitionChanged = RecipeDefinition != NewRecipeDefinition;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, CraftingTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bQuantityTotalChanged = QuantityTotal != NewQuantityTotal;
	const bool bQuantityCompletedChanged = QuantityCompleted != NewQuantityCompleted;
	const bool bStateChanged = State != NewState;
	const bool bCanCancelJobChanged = bCanCancelJob != bNewCanCancelJob;
	const bool bProgressChanged = Progress != NewProgress;
	const bool bRemainingSecondsChanged = RemainingSeconds != NewRemainingSeconds;

	CraftingStation = NewCraftingStation;
	JobId = NewJobId;
	RecipeDefinition = NewRecipeDefinition;
	DisplayName = NewDisplayName;
	Icon = NewIcon;
	QuantityTotal = NewQuantityTotal;
	QuantityCompleted = NewQuantityCompleted;
	State = NewState;
	bCanCancelJob = bNewCanCancelJob;
	Progress = NewProgress;
	RemainingSeconds = NewRemainingSeconds;

	if (bCraftingStationChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftingStation);
	}
	if (bJobIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(JobId);
	}
	if (bRecipeDefinitionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecipeDefinition);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bQuantityTotalChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(QuantityTotal);
	}
	if (bQuantityCompletedChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(QuantityCompleted);
	}
	if (bStateChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(State);
	}
	if (bCanCancelJobChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanCancelJob);
	}
	if (bProgressChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Progress);
	}
	if (bRemainingSecondsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RemainingSeconds);
	}
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

	const TObjectPtr<URpgCraftingStationComponent> NewObservedStation = InStation;
	const TObjectPtr<AActor> NewRequestingActor = InRequestingActor;
	const bool bObservedStationChanged = ObservedStation != NewObservedStation;
	const bool bRequestingActorChanged = RequestingActor != NewRequestingActor;

	UnregisterMessageListeners();
	ObservedStation = NewObservedStation;
	RequestingActor = NewRequestingActor;
	RegisterMessageListeners();
	Refresh();

	if (bObservedStationChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedStation);
	}
	if (bRequestingActorChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RequestingActor);
	}
}

void URpgCraftingStationViewModel::UnbindCraftingStation()
{
	UnregisterMessageListeners();
	CancelQueuedRefresh();

	const FText NewPauseResumeButtonText = MakePauseResumeButtonText(false);
	const bool bObservedStationChanged = ObservedStation != nullptr;
	const bool bRequestingActorChanged = RequestingActor != nullptr;
	const bool bOutputInventoryChanged = OutputInventory != nullptr;
	const bool bSelectedRecipeChanged = SelectedRecipe != nullptr;
	const bool bCraftQuantityChanged = CraftQuantity != 1;
	const bool bMaxSelectedCraftQuantityChanged = MaxSelectedCraftQuantity != 0;
	const bool bSelectedTotalCraftTimeChanged = SelectedTotalCraftTime != 0.0f;
	const bool bCanCraftSelectedRecipeChanged = bCanCraftSelectedRecipe;
	const bool bStationPausedChanged = bStationPaused;
	const bool bPauseResumeButtonTextChanged =
		!PauseResumeButtonText.IdenticalTo(
			NewPauseResumeButtonText,
			CraftingTextIdentityFlags);
	const bool bCanAutoDepositCraftingOutputsChanged = bCanAutoDepositCraftingOutputs;
	const bool bAutoDepositCraftingOutputsEnabledChanged =
		bAutoDepositCraftingOutputsEnabled;
	const bool bShouldAutoDepositCraftingOutputsChanged =
		bShouldAutoDepositCraftingOutputs;
	const bool bFilteredRecipesChanged = !FilteredRecipes.IsEmpty();
	const bool bSelectedIngredientsChanged = !SelectedIngredients.IsEmpty();
	const bool bSelectedOutputsChanged = !SelectedOutputs.IsEmpty();
	const bool bJobsChanged = !Jobs.IsEmpty();

	ObservedStation = nullptr;
	RequestingActor = nullptr;
	OutputInventory = nullptr;
	SelectedRecipe = nullptr;
	CraftQuantity = 1;
	MaxSelectedCraftQuantity = 0;
	SelectedTotalCraftTime = 0.0f;
	bCanCraftSelectedRecipe = false;
	bStationPaused = false;
	PauseResumeButtonText = NewPauseResumeButtonText;
	bCanAutoDepositCraftingOutputs = false;
	bAutoDepositCraftingOutputsEnabled = false;
	bShouldAutoDepositCraftingOutputs = false;
	FilteredRecipes.Reset();
	SelectedIngredients.Reset();
	SelectedOutputs.Reset();
	Jobs.Reset();

	RebuildActionAvailability();

	if (bObservedStationChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedStation);
	}
	if (bRequestingActorChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RequestingActor);
	}
	if (bOutputInventoryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputInventory);
	}
	if (bSelectedRecipeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	}
	if (bCraftQuantityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
	if (bMaxSelectedCraftQuantityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxSelectedCraftQuantity);
	}
	if (bSelectedTotalCraftTimeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedTotalCraftTime);
	}
	if (bCanCraftSelectedRecipeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanCraftSelectedRecipe);
	}
	if (bStationPausedChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	}
	if (bPauseResumeButtonTextChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
	}
	if (bCanAutoDepositCraftingOutputsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanAutoDepositCraftingOutputs);
	}
	if (bAutoDepositCraftingOutputsEnabledChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAutoDepositCraftingOutputsEnabled);
	}
	if (bShouldAutoDepositCraftingOutputsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bShouldAutoDepositCraftingOutputs);
	}
	if (bFilteredRecipesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
	}
	if (bSelectedIngredientsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedIngredients);
	}
	if (bSelectedOutputsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedOutputs);
	}
	if (bJobsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Jobs);
	}
	OnRecipesChanged.Broadcast();
	OnSelectedRecipeDetailsChanged.Broadcast();
	OnJobsChanged.Broadcast();
}

void URpgCraftingStationViewModel::Refresh()
{
	CancelQueuedRefresh();
	RefreshStationState();
	RefreshRecipesAndDetails();
	RefreshJobs();
}

void URpgCraftingStationViewModel::RefreshStationState()
{
	SatisfyPendingRefresh(CraftingRefreshDomains::Station);
	RebuildStationState();
}

void URpgCraftingStationViewModel::RefreshRecipesAndDetails()
{
	SatisfyPendingRefresh(CraftingRefreshDomains::RecipesAndDetails);
	URpgCraftingRecipeDefinition* PreviousSelectedRecipe = SelectedRecipe.Get();
	const int32 PreviousCraftQuantity = CraftQuantity;
	RebuildRecipeList();
	RebuildSelectedRecipeDetails();

	if (PreviousSelectedRecipe != SelectedRecipe.Get())
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	}
	if (PreviousCraftQuantity != CraftQuantity)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
}

void URpgCraftingStationViewModel::RefreshSelectedRecipeDetails()
{
	const int32 PreviousCraftQuantity = CraftQuantity;
	RebuildSelectedRecipeDetails();
	if (PreviousCraftQuantity != CraftQuantity)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
}

void URpgCraftingStationViewModel::RefreshJobs()
{
	SatisfyPendingRefresh(CraftingRefreshDomains::Jobs);
	RebuildJobs();
}

void URpgCraftingStationViewModel::SelectRecipe(URpgCraftingRecipeDefinition* RecipeDefinition)
{
	if (SelectedRecipe == RecipeDefinition)
	{
		return;
	}

	URpgCraftingRecipeDefinition* PreviousSelectedRecipe = SelectedRecipe.Get();
	const int32 PreviousCraftQuantity = CraftQuantity;
	SelectedRecipe = RecipeDefinition;
	CraftQuantity = 1;
	RebuildSelectedRecipeDetails();

	if (PreviousSelectedRecipe != SelectedRecipe.Get())
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	}
	if (PreviousCraftQuantity != CraftQuantity)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
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

	const int32 PreviousCraftQuantity = CraftQuantity;
	CraftQuantity = NewCraftQuantity;
	RebuildSelectedRecipeDetails();
	if (PreviousCraftQuantity != CraftQuantity)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
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

void URpgCraftingStationViewModel::RequestRefresh(uint8 RefreshDomains)
{
	PendingRefreshDomains |= RefreshDomains;

	UWorld* World = ObservedStation ? ObservedStation->GetWorld() : nullptr;
	if (!World)
	{
		FlushPendingRefreshes();
		return;
	}

	RefreshQueue.Queue(
		World,
		this,
		&ThisClass::ExecuteQueuedRefresh);
}

void URpgCraftingStationViewModel::ExecuteQueuedRefresh()
{
	if (!RefreshQueue.Consume())
	{
		return;
	}

	FlushPendingRefreshes();
}

void URpgCraftingStationViewModel::FlushPendingRefreshes()
{
	const uint8 RefreshDomains = PendingRefreshDomains;
	PendingRefreshDomains = 0;

	if ((RefreshDomains & CraftingRefreshDomains::Station) != 0)
	{
		RefreshStationState();
	}
	if ((RefreshDomains & CraftingRefreshDomains::RecipesAndDetails) != 0)
	{
		RefreshRecipesAndDetails();
	}
	if ((RefreshDomains & CraftingRefreshDomains::Jobs) != 0)
	{
		RefreshJobs();
	}
}

void URpgCraftingStationViewModel::CancelQueuedRefresh()
{
	RefreshQueue.Cancel();
	PendingRefreshDomains = 0;
}

void URpgCraftingStationViewModel::SatisfyPendingRefresh(uint8 RefreshDomains)
{
	if ((PendingRefreshDomains & RefreshDomains) == 0)
	{
		return;
	}

	PendingRefreshDomains &= ~RefreshDomains;
	if (PendingRefreshDomains == 0)
	{
		RefreshQueue.Cancel();
	}
}

void URpgCraftingStationViewModel::RebuildStationState()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	URpgInventoryManagerComponent* NewOutputInventory =
		Station ? Station->GetOutputInventory() : nullptr;
	const bool bNewStationPaused = Station && Station->IsCraftingPaused();
	const FText NewPauseResumeButtonText =
		MakePauseResumeButtonText(bNewStationPaused);
	const bool bNewCanAutoDepositCraftingOutputs =
		Station && Station->HasCraftingOutputAutoDepositAccess();
	const bool bNewAutoDepositCraftingOutputsEnabled =
		Station && Station->IsCraftingOutputAutoDepositEnabled();
	const bool bNewShouldAutoDepositCraftingOutputs =
		Station && Station->ShouldAutoDepositCraftingOutputs();

	const bool bOutputInventoryChanged = OutputInventory != NewOutputInventory;
	const bool bStationPausedChanged = bStationPaused != bNewStationPaused;
	const bool bPauseResumeButtonTextChanged =
		!PauseResumeButtonText.IdenticalTo(
			NewPauseResumeButtonText,
			CraftingTextIdentityFlags);
	const bool bCanAutoDepositCraftingOutputsChanged =
		bCanAutoDepositCraftingOutputs != bNewCanAutoDepositCraftingOutputs;
	const bool bAutoDepositCraftingOutputsEnabledChanged =
		bAutoDepositCraftingOutputsEnabled !=
		bNewAutoDepositCraftingOutputsEnabled;
	const bool bShouldAutoDepositCraftingOutputsChanged =
		bShouldAutoDepositCraftingOutputs !=
		bNewShouldAutoDepositCraftingOutputs;

	OutputInventory = NewOutputInventory;
	bStationPaused = bNewStationPaused;
	PauseResumeButtonText = NewPauseResumeButtonText;
	bCanAutoDepositCraftingOutputs = bNewCanAutoDepositCraftingOutputs;
	bAutoDepositCraftingOutputsEnabled =
		bNewAutoDepositCraftingOutputsEnabled;
	bShouldAutoDepositCraftingOutputs =
		bNewShouldAutoDepositCraftingOutputs;

	RebuildActionAvailability();

	if (bOutputInventoryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OutputInventory);
	}
	if (bStationPausedChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	}
	if (bPauseResumeButtonTextChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
	}
	if (bCanAutoDepositCraftingOutputsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanAutoDepositCraftingOutputs);
	}
	if (bAutoDepositCraftingOutputsEnabledChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAutoDepositCraftingOutputsEnabled);
	}
	if (bShouldAutoDepositCraftingOutputsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bShouldAutoDepositCraftingOutputs);
	}
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
	if (!Station)
	{
		const bool bRecipeListChanged = !FilteredRecipes.IsEmpty();
		const bool bRecipeOrderChanged = !PreviousRecipeOrder.IsEmpty();
		FilteredRecipes.Reset();
		SelectedRecipe = nullptr;
		CraftQuantity = 1;

		if (bRecipeListChanged)
		{
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
		}
		if (bRecipeOrderChanged)
		{
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

	URpgCraftingRecipeDefinition* NewSelectedRecipe = SelectedRecipe.Get();
	int32 NewCraftQuantity = CraftQuantity;
	const bool bSelectedRecipeStillVisible =
		NewSelectedRecipe &&
		NewFilteredRecipes.ContainsByPredicate([NewSelectedRecipe](const TObjectPtr<URpgCraftingRecipeViewModel>& RecipeViewModel)
	{
		return RecipeViewModel &&
			RecipeViewModel->GetRecipeDefinition() == NewSelectedRecipe;
	});

	if (!bSelectedRecipeStillVisible)
	{
		NewSelectedRecipe = NewFilteredRecipes.Num() > 0
			? NewFilteredRecipes[0]->GetRecipeDefinition()
			: nullptr;
		NewCraftQuantity = 1;
	}

	const bool bRecipeListChanged =
		!AreViewModelArraysEqual(FilteredRecipes, NewFilteredRecipes);
	const bool bRecipeOrderChanged =
		!AreRecipeOrdersEqual(PreviousRecipeOrder, NewFilteredRecipes);
	SelectedRecipe = NewSelectedRecipe;
	CraftQuantity = NewCraftQuantity;
	FilteredRecipes = MoveTemp(NewFilteredRecipes);
	if (bRecipeListChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilteredRecipes);
	}
	if (bRecipeOrderChanged)
	{
		OnRecipesChanged.Broadcast();
	}
}

void URpgCraftingStationViewModel::RebuildSelectedRecipeDetails()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	URpgCraftingRecipeDefinition* CurrentSelectedRecipe = SelectedRecipe.Get();
	const int32 NewMaxSelectedCraftQuantity =
		Station && CurrentSelectedRecipe
			? Station->GetMaxCraftableQuantity(Actor, CurrentSelectedRecipe)
			: 0;
	int32 NewCraftQuantity = CraftQuantity;
	if (NewCraftQuantity > FMath::Max(1, NewMaxSelectedCraftQuantity))
	{
		NewCraftQuantity = FMath::Max(1, NewMaxSelectedCraftQuantity);
	}

	const bool bNewCanCraftSelectedRecipe =
		Station &&
		CurrentSelectedRecipe &&
		Station->CanCraftRecipeQuantity(
			Actor,
			CurrentSelectedRecipe,
			NewCraftQuantity);
	const float NewSelectedTotalCraftTime =
		CurrentSelectedRecipe
			? FMath::Max(0.0f, CurrentSelectedRecipe->CraftTime) *
				NewCraftQuantity
			: 0.0f;
	const bool bNewStationPaused = Station && Station->IsCraftingPaused();
	const FText NewPauseResumeButtonText =
		MakePauseResumeButtonText(bNewStationPaused);

	TArray<TObjectPtr<URpgCraftingIngredientViewModel>> NewSelectedIngredients;
	TArray<TObjectPtr<URpgCraftingOutputViewModel>> NewSelectedOutputs;
	if (Station && CurrentSelectedRecipe)
	{
		NewSelectedIngredients.Reserve(
			CurrentSelectedRecipe->RequiredResources.Num());
		for (const FRpgCraftingResourceCost& RequiredResource :
			 CurrentSelectedRecipe->RequiredResources)
		{
			URpgCraftingIngredientViewModel* IngredientViewModel = NewObject<URpgCraftingIngredientViewModel>(this);
			IngredientViewModel->InitializeIngredient(
				RequiredResource.ItemDefinition,
				RequiredResource.Count * NewCraftQuantity,
				Station->GetAvailableResourceCount(Actor, RequiredResource.ItemDefinition));
			NewSelectedIngredients.Add(IngredientViewModel);
		}

		NewSelectedOutputs.Reserve(CurrentSelectedRecipe->OutputItems.Num());
		for (const FRpgCraftingOutputItem& OutputItem :
			 CurrentSelectedRecipe->OutputItems)
		{
			URpgCraftingOutputViewModel* OutputViewModel = NewObject<URpgCraftingOutputViewModel>(this);
			OutputViewModel->InitializeOutput(
				OutputItem.ItemDefinition,
				OutputItem.Count * NewCraftQuantity);
			NewSelectedOutputs.Add(OutputViewModel);
		}
	}

	const bool bMaxSelectedCraftQuantityChanged =
		MaxSelectedCraftQuantity != NewMaxSelectedCraftQuantity;
	const bool bSelectedTotalCraftTimeChanged =
		SelectedTotalCraftTime != NewSelectedTotalCraftTime;
	const bool bCanCraftSelectedRecipeChanged =
		bCanCraftSelectedRecipe != bNewCanCraftSelectedRecipe;
	const bool bStationPausedChanged = bStationPaused != bNewStationPaused;
	const bool bPauseResumeButtonTextChanged =
		!PauseResumeButtonText.IdenticalTo(
			NewPauseResumeButtonText,
			CraftingTextIdentityFlags);
	const bool bSelectedIngredientsChanged =
		!AreViewModelArraysEqual(
			SelectedIngredients,
			NewSelectedIngredients);
	const bool bSelectedOutputsChanged =
		!AreViewModelArraysEqual(SelectedOutputs, NewSelectedOutputs);

	CraftQuantity = NewCraftQuantity;
	MaxSelectedCraftQuantity = NewMaxSelectedCraftQuantity;
	SelectedTotalCraftTime = NewSelectedTotalCraftTime;
	bCanCraftSelectedRecipe = bNewCanCraftSelectedRecipe;
	bStationPaused = bNewStationPaused;
	PauseResumeButtonText = NewPauseResumeButtonText;
	SelectedIngredients = MoveTemp(NewSelectedIngredients);
	SelectedOutputs = MoveTemp(NewSelectedOutputs);

	RebuildActionAvailability();

	if (bMaxSelectedCraftQuantityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MaxSelectedCraftQuantity);
	}
	if (bSelectedTotalCraftTimeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedTotalCraftTime);
	}
	if (bCanCraftSelectedRecipeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanCraftSelectedRecipe);
	}
	if (bStationPausedChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	}
	if (bPauseResumeButtonTextChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
	}
	if (bSelectedIngredientsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedIngredients);
	}
	if (bSelectedOutputsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedOutputs);
	}
	OnSelectedRecipeDetailsChanged.Broadcast();
}

void URpgCraftingStationViewModel::RebuildJobs()
{
	const int32 PreviousJobCount = Jobs.Num();
	TArray<FGuid> PreviousJobOrder;
	PreviousJobOrder.Reserve(PreviousJobCount);

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
	bool bNewStationPaused = false;
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

			JobViewModel->InitializeJobForStation(Station, Job, ServerTime);
			NewJobs.Add(JobViewModel);
		}

		bNewStationPaused = Station->IsCraftingPaused();
	}

	const FText NewPauseResumeButtonText =
		MakePauseResumeButtonText(bNewStationPaused);
	const bool bJobListChanged = !AreViewModelArraysEqual(Jobs, NewJobs);
	const bool bJobOrderChanged =
		!AreJobOrdersEqual(PreviousJobOrder, NewJobs);
	const bool bStationPausedChanged = bStationPaused != bNewStationPaused;
	const bool bPauseResumeButtonTextChanged =
		!PauseResumeButtonText.IdenticalTo(
			NewPauseResumeButtonText,
			CraftingTextIdentityFlags);

	Jobs = MoveTemp(NewJobs);
	bStationPaused = bNewStationPaused;
	PauseResumeButtonText = NewPauseResumeButtonText;

	if (bStationPausedChanged)
	{
		RebuildActionAvailability();
	}
	if (bJobListChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Jobs);
	}

	if (bJobOrderChanged || PreviousJobCount > 0 || Jobs.Num() > 0)
	{
		OnJobsChanged.Broadcast();
	}

	if (bStationPausedChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bStationPaused);
	}
	if (bPauseResumeButtonTextChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PauseResumeButtonText);
	}
}

void URpgCraftingStationViewModel::RebuildActionAvailability()
{
	const bool bNewCanSubmitSelectedRecipe = bCanCraftSelectedRecipe;
	const bool bNewCanDecreaseCraftQuantity = CraftQuantity > 1;
	const bool bNewCanIncreaseCraftQuantity = CraftQuantity < MaxSelectedCraftQuantity;
	const bool bNewCanSetCraftQuantityToOne = MaxSelectedCraftQuantity >= 1 && CraftQuantity != 1;
	const bool bNewCanSetCraftQuantityToFive = MaxSelectedCraftQuantity >= 5 && CraftQuantity != 5;
	const bool bNewCanSetCraftQuantityToTen = MaxSelectedCraftQuantity >= 10 && CraftQuantity != 10;
	const bool bNewCanSetCraftQuantityToMax = MaxSelectedCraftQuantity > 1 && CraftQuantity < MaxSelectedCraftQuantity;
	const bool bNewHasCraftQuantityOptions = MaxSelectedCraftQuantity > 1;

	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	const bool bNewCanToggleCraftingPause = Station && Actor && Station->CanActorAccess(Actor);
	const bool bNewCanPauseCraftingStation = bNewCanToggleCraftingPause && !bStationPaused;
	const bool bNewCanResumeCraftingStation = bNewCanToggleCraftingPause && bStationPaused;

	const bool bCanSubmitSelectedRecipeChanged = bCanSubmitSelectedRecipe != bNewCanSubmitSelectedRecipe;
	const bool bCanDecreaseCraftQuantityChanged = bCanDecreaseCraftQuantity != bNewCanDecreaseCraftQuantity;
	const bool bCanIncreaseCraftQuantityChanged = bCanIncreaseCraftQuantity != bNewCanIncreaseCraftQuantity;
	const bool bCanSetCraftQuantityToOneChanged = bCanSetCraftQuantityToOne != bNewCanSetCraftQuantityToOne;
	const bool bCanSetCraftQuantityToFiveChanged = bCanSetCraftQuantityToFive != bNewCanSetCraftQuantityToFive;
	const bool bCanSetCraftQuantityToTenChanged = bCanSetCraftQuantityToTen != bNewCanSetCraftQuantityToTen;
	const bool bCanSetCraftQuantityToMaxChanged = bCanSetCraftQuantityToMax != bNewCanSetCraftQuantityToMax;
	const bool bHasCraftQuantityOptionsChanged = bHasCraftQuantityOptions != bNewHasCraftQuantityOptions;
	const bool bCanToggleCraftingPauseChanged = bCanToggleCraftingPause != bNewCanToggleCraftingPause;
	const bool bCanPauseCraftingStationChanged = bCanPauseCraftingStation != bNewCanPauseCraftingStation;
	const bool bCanResumeCraftingStationChanged = bCanResumeCraftingStation != bNewCanResumeCraftingStation;

	bCanSubmitSelectedRecipe = bNewCanSubmitSelectedRecipe;
	bCanDecreaseCraftQuantity = bNewCanDecreaseCraftQuantity;
	bCanIncreaseCraftQuantity = bNewCanIncreaseCraftQuantity;
	bCanSetCraftQuantityToOne = bNewCanSetCraftQuantityToOne;
	bCanSetCraftQuantityToFive = bNewCanSetCraftQuantityToFive;
	bCanSetCraftQuantityToTen = bNewCanSetCraftQuantityToTen;
	bCanSetCraftQuantityToMax = bNewCanSetCraftQuantityToMax;
	bHasCraftQuantityOptions = bNewHasCraftQuantityOptions;
	bCanToggleCraftingPause = bNewCanToggleCraftingPause;
	bCanPauseCraftingStation = bNewCanPauseCraftingStation;
	bCanResumeCraftingStation = bNewCanResumeCraftingStation;

	if (bCanSubmitSelectedRecipeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSubmitSelectedRecipe);
	}
	if (bCanDecreaseCraftQuantityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanDecreaseCraftQuantity);
	}
	if (bCanIncreaseCraftQuantityChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanIncreaseCraftQuantity);
	}
	if (bCanSetCraftQuantityToOneChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToOne);
	}
	if (bCanSetCraftQuantityToFiveChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToFive);
	}
	if (bCanSetCraftQuantityToTenChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToTen);
	}
	if (bCanSetCraftQuantityToMaxChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanSetCraftQuantityToMax);
	}
	if (bHasCraftQuantityOptionsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasCraftQuantityOptions);
	}
	if (bCanToggleCraftingPauseChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanToggleCraftingPause);
	}
	if (bCanPauseCraftingStationChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanPauseCraftingStation);
	}
	if (bCanResumeCraftingStationChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanResumeCraftingStation);
	}
}

void URpgCraftingStationViewModel::HandleCraftingStationChanged(FGameplayTag Channel, const FRpgCraftingStationChangeMessage& Message)
{
	if (ObservedStation.Get() == Message.Station)
	{
		RequestRefresh(CraftingRefreshDomains::All);
	}
}

void URpgCraftingStationViewModel::HandleRecipeUnlockChanged(FGameplayTag Channel, const FRpgRecipeUnlockChangeMessage& Message)
{
	RequestRefresh(CraftingRefreshDomains::RecipesAndDetails);
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
		RequestRefresh(
			CraftingRefreshDomains::Station |
			CraftingRefreshDomains::Jobs);
		return;
	}

	const TArray<URpgInventoryManagerComponent*> ResourceInventories = Station->GetResourceInventories(RequestingActor.Get());
	if (ResourceInventories.Contains(ChangedInventory))
	{
		RequestRefresh(CraftingRefreshDomains::RecipesAndDetails);
	}
}

void URpgCraftingStationViewModel::HandleBaseStorageChanged(FGameplayTag Channel, const FRpgBaseResourceChangeMessage& Message)
{
	RequestRefresh(CraftingRefreshDomains::RecipesAndDetails);
}
