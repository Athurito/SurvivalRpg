#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCraftingViewModels.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RpgCraftingViewModelPresentationTests
{
	/** Standalone world with one crafting station and a requesting controller. */
	class FScopedStationWorld
	{
	public:
		FScopedStationWorld()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			if (!World)
			{
				return;
			}

			Requester = World->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
			ARpgCraftingStationActor* StationActor = World->SpawnActor<ARpgCraftingStationActor>();
			Station = StationActor ? StationActor->GetCraftingStationComponent() : nullptr;
		}

		~FScopedStationWorld()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		bool IsValid() const
		{
			return World && Requester && Station;
		}

		/** Offers the recipes through a transient recipe set, like an authored station. */
		void OfferRecipes(const TArray<URpgCraftingRecipeDefinition*>& Recipes)
		{
			URpgCraftingRecipeSet* RecipeSet = NewObject<URpgCraftingRecipeSet>(GetTransientPackage(), NAME_None, RF_Transient);
			for (URpgCraftingRecipeDefinition* Recipe : Recipes)
			{
				RecipeSet->Recipes.Add(Recipe);
			}
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
				URpgCraftingStationComponent::StaticClass(),
				TEXT("AvailableRecipeSet"));
			Property->SetObjectPropertyValue_InContainer(Station, RecipeSet);
		}

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
		ARpgInventoryAutomationTestPlayerController* Requester = nullptr;
		URpgCraftingStationComponent* Station = nullptr;
	};

	/** Free, unlocked one-output recipe; further costs or locks are set by the caller. */
	URpgCraftingRecipeDefinition* MakeRecipe(const TCHAR* Name, float CraftTime = 0.0f)
	{
		URpgCraftingRecipeDefinition* Recipe = NewObject<URpgCraftingRecipeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Recipe->DisplayName = FText::FromString(Name);
		Recipe->bUnlockedByDefault = true;
		Recipe->CraftTime = CraftTime;
		FRpgCraftingOutputItem& Output = Recipe->OutputItems.AddDefaulted_GetRef();
		Output.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
		Output.Count = 1;
		return Recipe;
	}

	/** Counts notifications of one FieldNotify field by its binding name. */
	class FFieldCounter
	{
	public:
		FFieldCounter(UMVVMViewModelBase* InViewModel, const TCHAR* FieldName)
			: ViewModel(InViewModel)
		{
			FieldId = ViewModel->GetFieldNotificationDescriptor().GetField(ViewModel->GetClass(), FieldName);
			if (FieldId.IsValid())
			{
				Handle = ViewModel->AddFieldValueChangedDelegate(
					FieldId,
					INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
						[this](UObject*, UE::FieldNotification::FFieldId) { ++Count; }));
			}
		}

		~FFieldCounter()
		{
			if (FieldId.IsValid())
			{
				ViewModel->RemoveFieldValueChangedDelegate(FieldId, Handle);
			}
		}

		bool IsBound() const { return FieldId.IsValid(); }

		int32 Count = 0;

	private:
		UMVVMViewModelBase* ViewModel = nullptr;
		UE::FieldNotification::FFieldId FieldId;
		FDelegateHandle Handle;
	};

	URpgCraftingRecipeViewModel* FindRow(
		const URpgCraftingStationViewModel* ViewModel,
		const URpgCraftingRecipeDefinition* Recipe)
	{
		for (URpgCraftingRecipeViewModel* Row : ViewModel->GetFilteredRecipes())
		{
			if (Row && Row->GetRecipeDefinition() == Recipe)
			{
				return Row;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelStationIdentityTest,
	"SurvivalRpg.Crafting.ViewModel.StationIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelStationIdentityTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	FFieldCounter NameCounter(ViewModel, TEXT("StationDisplayName"));
	TestTrue(TEXT("StationDisplayName is a field-notify field"), NameCounter.IsBound());
	TestTrue(TEXT("An unbound view model has no station title"), ViewModel->GetStationDisplayName().IsEmpty());

	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);
	TestEqual(
		TEXT("A station without an authored name gets the generic title"),
		ViewModel->GetStationDisplayName().ToString(),
		FString(TEXT("Crafting Station")));
	TestTrue(TEXT("A station without an icon exposes none"), ViewModel->GetStationIcon().IsNull());
	TestEqual(TEXT("Binding publishes the title once"), NameCounter.Count, 1);

	FindFProperty<FTextProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("StationDisplayName"))
		->SetPropertyValue_InContainer(Fixture.Station, FText::FromString(TEXT("Kiln")));
	const FSoftObjectPath IconPath(TEXT("/Game/SurvivalRpg/UI/Art/Icons/crafting/T_UI_Station_Kiln.T_UI_Station_Kiln"));
	FindFProperty<FSoftObjectProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("StationIcon"))
		->SetPropertyValue_InContainer(Fixture.Station, FSoftObjectPtr(IconPath));

	ViewModel->RefreshStationState();
	TestEqual(TEXT("The authored station name replaces the generic title"), ViewModel->GetStationDisplayName().ToString(), FString(TEXT("Kiln")));
	TestEqual(TEXT("The authored station icon is exposed"), ViewModel->GetStationIcon().ToSoftObjectPath().ToString(), IconPath.ToString());
	TestEqual(TEXT("A changed name notifies once"), NameCounter.Count, 2);

	ViewModel->RefreshStationState();
	TestEqual(TEXT("An unchanged name stays quiet"), NameCounter.Count, 2);

	ViewModel->UnbindCraftingStation();
	TestTrue(TEXT("Unbinding clears the title"), ViewModel->GetStationDisplayName().IsEmpty());
	TestTrue(TEXT("Unbinding clears the icon"), ViewModel->GetStationIcon().IsNull());
	TestEqual(TEXT("Unbinding notifies the cleared title"), NameCounter.Count, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelRecipeStateTest,
	"SurvivalRpg.Crafting.ViewModel.RecipeState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelRecipeStateTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* FreeRecipe = MakeRecipe(TEXT("Free"));
	URpgCraftingRecipeDefinition* CostlyRecipe = MakeRecipe(TEXT("Costly"));
	FRpgCraftingResourceCost& Cost = CostlyRecipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Cost.Count = 2;
	URpgCraftingRecipeDefinition* LockedRecipe = MakeRecipe(TEXT("Locked"));
	LockedRecipe->bUnlockedByDefault = false;
	Fixture.OfferRecipes({ FreeRecipe, CostlyRecipe, LockedRecipe });

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);

	const URpgCraftingRecipeViewModel* FreeRow = FindRow(ViewModel, FreeRecipe);
	URpgCraftingRecipeViewModel* CostlyRow = FindRow(ViewModel, CostlyRecipe);
	const URpgCraftingRecipeViewModel* LockedRow = FindRow(ViewModel, LockedRecipe);
	if (!TestNotNull(TEXT("The free recipe has a row"), FreeRow) ||
		!TestNotNull(TEXT("The costly recipe has a row"), CostlyRow) ||
		!TestNotNull(TEXT("The locked recipe keeps a row"), LockedRow))
	{
		ViewModel->UnbindCraftingStation();
		return false;
	}

	TestEqual(TEXT("A free unlocked recipe is craftable"), FreeRow->GetRecipeState(), ERpgCraftingRecipeState::Craftable);
	TestEqual(TEXT("A recipe without its materials is missing resources"), CostlyRow->GetRecipeState(), ERpgCraftingRecipeState::MissingResources);
	TestEqual(TEXT("A recipe that is not unlocked is locked"), LockedRow->GetRecipeState(), ERpgCraftingRecipeState::Locked);

	FFieldCounter StateCounter(CostlyRow, TEXT("RecipeState"));
	TestTrue(TEXT("RecipeState is a field-notify field"), StateCounter.IsBound());
	CostlyRow->InitializeRecipe(Fixture.Station, Fixture.Requester, CostlyRecipe);
	TestEqual(TEXT("An unchanged state stays quiet"), StateCounter.Count, 0);

	URpgInventoryManagerComponent* Inventory = NewObject<URpgInventoryManagerComponent>(Fixture.Requester);
	Fixture.Requester->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	Inventory->AddItemDefinition(URpgInventoryAutomationTestMaterialDefinition::StaticClass(), 2);
	CostlyRow->InitializeRecipe(Fixture.Station, Fixture.Requester, CostlyRecipe);
	TestEqual(TEXT("Owning the materials makes the recipe craftable"), CostlyRow->GetRecipeState(), ERpgCraftingRecipeState::Craftable);
	TestEqual(TEXT("The state change notifies once"), StateCounter.Count, 1);

	ViewModel->UnbindCraftingStation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelAvailableCategoriesTest,
	"SurvivalRpg.Crafting.ViewModel.AvailableCategories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelAvailableCategoriesTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	const FGameplayTag Building = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Building"));
	const FGameplayTag Refining = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Refining"));
	const FGameplayTag Weapons = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Weapons"));
	URpgCraftingRecipeDefinition* Chest = MakeRecipe(TEXT("Chest"));
	Chest->RecipeCategory = Building;
	URpgCraftingRecipeDefinition* Charcoal = MakeRecipe(TEXT("Charcoal"));
	Charcoal->RecipeCategory = Refining;
	URpgCraftingRecipeDefinition* Uncategorised = MakeRecipe(TEXT("Loose"));
	Fixture.OfferRecipes({ Chest, Charcoal, Uncategorised });

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	FFieldCounter CategoryCounter(ViewModel, TEXT("AvailableCategories"));
	FFieldCounter HasRecipesCounter(ViewModel, TEXT("bHasFilteredRecipes"));
	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);

	TestTrue(TEXT("Building is offered"), ViewModel->GetAvailableCategories().HasTagExact(Building));
	TestTrue(TEXT("Refining is offered"), ViewModel->GetAvailableCategories().HasTagExact(Refining));
	TestFalse(TEXT("Weapons is not offered"), ViewModel->GetAvailableCategories().HasTagExact(Weapons));
	TestEqual(TEXT("Recipes without a category add no tag"), ViewModel->GetAvailableCategories().Num(), 2);
	TestTrue(TEXT("The unfiltered list has recipes"), ViewModel->HasFilteredRecipes());
	TestEqual(TEXT("Binding publishes the categories once"), CategoryCounter.Count, 1);

	ViewModel->SetCategoryFilter(Building);
	TestEqual(TEXT("The category filter keeps one row"), ViewModel->GetFilteredRecipes().Num(), 1);
	TestEqual(TEXT("The filter does not narrow the available categories"), ViewModel->GetAvailableCategories().Num(), 2);
	TestEqual(TEXT("A filter change leaves the categories quiet"), CategoryCounter.Count, 1);

	ViewModel->SetSearchText(FText::FromString(TEXT("no such recipe")));
	TestFalse(TEXT("A search without matches empties the list"), ViewModel->HasFilteredRecipes());
	TestEqual(TEXT("The empty list notifies once"), HasRecipesCounter.Count, 2);

	ViewModel->UnbindCraftingStation();
	TestEqual(TEXT("Unbinding clears the categories"), ViewModel->GetAvailableCategories().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelActionAndCountsTest,
	"SurvivalRpg.Crafting.ViewModel.ActionAndCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelActionAndCountsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()) ||
		!TestNotNull(TEXT("The station has an output tray"), Fixture.Station->GetOutputInventory()))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* Arrows = MakeRecipe(TEXT("Arrows"));
	Arrows->OutputItems[0].Count = 20;
	URpgCraftingRecipeDefinition* Pair = MakeRecipe(TEXT("Pair"));
	const FRpgCraftingOutputItem SecondOutput = Pair->OutputItems[0];
	Pair->OutputItems.Add(SecondOutput);
	URpgCraftingRecipeDefinition* Slow = MakeRecipe(TEXT("Slow"), 10.0f);
	Fixture.OfferRecipes({ Arrows, Pair, Slow });
	const FString OutputName =
		GetDefault<URpgInventoryItemDefinition>(URpgInventoryAutomationTestUnitItemDefinition::StaticClass())->DisplayName.ToString();

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	FFieldCounter ActionCounter(ViewModel, TEXT("CraftActionText"));
	FFieldCounter JobCounter(ViewModel, TEXT("JobCount"));
	FFieldCounter StackCounter(ViewModel, TEXT("OutputStackCount"));
	TestTrue(TEXT("The new counts are field-notify fields"), ActionCounter.IsBound() && JobCounter.IsBound() && StackCounter.IsBound());
	TestTrue(TEXT("An unbound view model has no craft label"), ViewModel->GetCraftActionText().IsEmpty());

	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);
	const URpgCraftingRecipeViewModel* ArrowRow = FindRow(ViewModel, Arrows);
	const URpgCraftingRecipeViewModel* PairRow = FindRow(ViewModel, Pair);
	if (!TestNotNull(TEXT("The arrow recipe has a row"), ArrowRow) ||
		!TestNotNull(TEXT("The two-output recipe has a row"), PairRow))
	{
		ViewModel->UnbindCraftingStation();
		return false;
	}
	TestEqual(TEXT("A single output shows its yield"), ArrowRow->GetYieldText().ToString(), FString(TEXT("×20")));
	TestTrue(TEXT("Several outputs show no single yield"), PairRow->GetYieldText().IsEmpty());

	ViewModel->SelectRecipe(Arrows);
	TestEqual(
		TEXT("The craft label names the output at quantity one"),
		ViewModel->GetCraftActionText().ToString(),
		FString::Printf(TEXT("Craft 20x %s"), *OutputName));
	ViewModel->SetCraftQuantity(2);
	TestEqual(
		TEXT("The craft label follows the quantity"),
		ViewModel->GetCraftActionText().ToString(),
		FString::Printf(TEXT("Craft 40x %s"), *OutputName));
	const int32 ActionNotifications = ActionCounter.Count;
	ViewModel->RefreshSelectedRecipeDetails();
	TestEqual(TEXT("An unchanged craft label stays quiet"), ActionCounter.Count, ActionNotifications);
	ViewModel->SelectRecipe(Pair);
	TestEqual(TEXT("Several outputs fall back to Craft"), ViewModel->GetCraftActionText().ToString(), FString(TEXT("Craft")));

	TestEqual(TEXT("An idle station has no jobs"), ViewModel->GetJobCount(), 0);
	TestTrue(TEXT("The station queues a timed job"), Fixture.Station->QueueCraftRecipe(Fixture.Requester, Slow, 1));
	ViewModel->RefreshJobs();
	TestEqual(TEXT("The queued job is counted"), ViewModel->GetJobCount(), 1);
	TestEqual(TEXT("The job count notifies once"), JobCounter.Count, 1);
	ViewModel->RefreshJobs();
	TestEqual(TEXT("An unchanged job count stays quiet"), JobCounter.Count, 1);

	TestEqual(TEXT("An empty tray has no stacks"), ViewModel->GetOutputStackCount(), 0);
	FRpgCraftingOutputItem Output;
	Output.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	Output.Count = 1;
	TestTrue(TEXT("The tray accepts an output"), Fixture.Station->AddCraftingOutputs({ Output }));
	ViewModel->RefreshStationState();
	TestEqual(TEXT("The tray stack is counted"), ViewModel->GetOutputStackCount(), 1);
	TestEqual(TEXT("The stack count notifies once"), StackCounter.Count, 1);

	ViewModel->UnbindCraftingStation();
	TestTrue(TEXT("Unbinding clears the craft label"), ViewModel->GetCraftActionText().IsEmpty());
	TestEqual(TEXT("Unbinding clears the job count"), ViewModel->GetJobCount(), 0);
	TestEqual(TEXT("Unbinding clears the stack count"), ViewModel->GetOutputStackCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelJobTextTest,
	"SurvivalRpg.Crafting.ViewModel.JobText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelJobTextTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	URpgCraftingRecipeDefinition* Recipe = MakeRecipe(TEXT("Plank"), 10.0f);
	URpgCraftingJobViewModel* ViewModel = NewObject<URpgCraftingJobViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	FFieldCounter TimeCounter(ViewModel, TEXT("RemainingTimeText"));
	TestTrue(TEXT("RemainingTimeText is a field-notify field"), TimeCounter.IsBound());

	FRpgCraftingJobEntry Job;
	Job.JobId = FGuid::NewGuid();
	Job.Recipe = Recipe;
	Job.QuantityTotal = 3;
	Job.QuantityCompleted = 1;
	Job.State = ERpgCraftingJobState::Active;
	Job.StartServerTime = 100.0f;
	Job.FinishServerTime = 110.0f;

	ViewModel->InitializeJob(Job, 104.0f);
	TestEqual(TEXT("An active job reads Crafting"), ViewModel->GetStateText().ToString(), FString(TEXT("Crafting")));
	TestEqual(TEXT("Active time covers the running unit and the next one"), ViewModel->GetRemainingTimeText().ToString(), FString(TEXT("16 s")));
	TestEqual(TEXT("The first time text notifies once"), TimeCounter.Count, 1);

	ViewModel->InitializeJob(Job, 104.2f);
	TestEqual(TEXT("A sub-second tick keeps the same text"), TimeCounter.Count, 1);

	Job.State = ERpgCraftingJobState::Paused;
	Job.QuantityCompleted = 2;
	Job.PausedRemainingTime = 65.0f;
	ViewModel->InitializeJob(Job, 200.0f);
	TestEqual(TEXT("A paused job reads Paused"), ViewModel->GetStateText().ToString(), FString(TEXT("Paused")));
	TestEqual(TEXT("A minute and more uses minutes and seconds"), ViewModel->GetRemainingTimeText().ToString(), FString(TEXT("1:05")));

	Recipe->CraftTime = 1800.0f;
	Job.State = ERpgCraftingJobState::Queued;
	Job.QuantityCompleted = 0;
	ViewModel->InitializeJob(Job, 200.0f);
	TestEqual(TEXT("A queued job reads Queued"), ViewModel->GetStateText().ToString(), FString(TEXT("Queued")));
	TestEqual(TEXT("A queued job counts every unit"), ViewModel->GetRemainingTimeText().ToString(), FString(TEXT("1:30:00")));

	Job.State = ERpgCraftingJobState::BlockedOutput;
	ViewModel->InitializeJob(Job, 200.0f);
	TestEqual(TEXT("A blocked job names the full output"), ViewModel->GetStateText().ToString(), FString(TEXT("Output full")));
	TestTrue(TEXT("A blocked job shows no time"), ViewModel->GetRemainingTimeText().IsEmpty());
	return true;
}

#endif
