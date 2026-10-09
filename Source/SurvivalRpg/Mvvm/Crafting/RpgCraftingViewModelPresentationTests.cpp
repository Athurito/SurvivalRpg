#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCraftingViewModels.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Crafting/RpgCraftingCategoryCatalog.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
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

		/** Chest next to the station; a positive size replaces its grid. */
		ARpgInventoryContainerActor* CreateChest(int32 Width = 0, int32 Height = 0) const
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARpgInventoryContainerActor* Chest = World->SpawnActor<ARpgInventoryContainerActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
			if (Chest)
			{
				Chest->GetContainerComponent()->EnsurePersistentContainerId();
				if (Width > 0 && Height > 0)
				{
					FRpgInventoryGridSize Grid;
					Grid.Width = Width;
					Grid.Height = Height;
					Chest->GetInventoryManager()->SetDefaultGridSize(Grid);
				}
			}
			return Chest;
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

	FName ChestId(const ARpgInventoryContainerActor* Chest)
	{
		return Chest->GetContainerComponent()->GetPersistentContainerId();
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

	/** Reads a reflected FText field of a view model by name. */
	FString ReadText(const UObject* Object, const TCHAR* FieldName)
	{
		const FTextProperty* Property = FindFProperty<FTextProperty>(Object->GetClass(), FieldName);
		return Property ? Property->GetPropertyValue_InContainer(Object).ToString() : FString(TEXT("<missing>"));
	}

	template <typename ValueType, typename PropertyType>
	ValueType ReadValue(const UObject* Object, const TCHAR* FieldName)
	{
		const PropertyType* Property = FindFProperty<PropertyType>(Object->GetClass(), FieldName);
		return Property ? Property->GetPropertyValue_InContainer(Object) : ValueType();
	}

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

	URpgCraftingCategoryViewModel* FindCategoryRow(
		const URpgCraftingStationViewModel* ViewModel,
		ERpgCraftingCategoryRowKind Kind,
		const FGameplayTag& Tag)
	{
		for (URpgCraftingCategoryViewModel* Row : ViewModel->GetCategoryRows())
		{
			if (Row && Row->GetKind() == Kind && Row->GetCategoryTag() == Tag)
			{
				return Row;
			}
		}
		return nullptr;
	}

	/** Display names of the category rows in order. */
	FString DescribeCategoryRows(const URpgCraftingStationViewModel* ViewModel)
	{
		TArray<FString> Parts;
		for (const URpgCraftingCategoryViewModel* Row : ViewModel->GetCategoryRows())
		{
			Parts.Add(FString::Printf(TEXT("%s %d"), *ReadText(Row, TEXT("DisplayName")), ReadValue<int32, FIntProperty>(Row, TEXT("RecipeCount"))));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	/** Tier headers and recipe names of the recipe list in order. */
	FString DescribeRecipeList(const URpgCraftingStationViewModel* ViewModel)
	{
		TArray<FString> Parts;
		for (const UObject* Item : ViewModel->GetRecipeListItems())
		{
			if (const URpgCraftingTierSectionViewModel* Section = Cast<URpgCraftingTierSectionViewModel>(Item))
			{
				Parts.Add(FString::Printf(TEXT("[%s]"), *ReadText(Section, TEXT("TitleText"))));
			}
			else if (const URpgCraftingRecipeViewModel* Row = Cast<URpgCraftingRecipeViewModel>(Item))
			{
				Parts.Add(Row->GetRecipeDefinition()->DisplayName.ToString());
			}
		}
		return FString::Join(Parts, TEXT(" "));
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
	TestEqual(TEXT("Without chests the header says so"), ReadText(ViewModel, TEXT("ConnectedStorageText")), FString(TEXT("No connected chest")));

	FindFProperty<FTextProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("StationDisplayName"))
		->SetPropertyValue_InContainer(Fixture.Station, FText::FromString(TEXT("Kiln")));
	const FSoftObjectPath IconPath(TEXT("/Game/SurvivalRpg/UI/Art/Icons/crafting/T_UI_Station_Kiln.T_UI_Station_Kiln"));
	FindFProperty<FSoftObjectProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("StationIcon"))
		->SetPropertyValue_InContainer(Fixture.Station, FSoftObjectPtr(IconPath));
	Fixture.CreateChest();
	Fixture.CreateChest();

	ViewModel->RefreshStationState();
	TestEqual(TEXT("The authored station name replaces the generic title"), ViewModel->GetStationDisplayName().ToString(), FString(TEXT("Kiln")));
	TestEqual(TEXT("The authored station icon is exposed"), ViewModel->GetStationIcon().ToSoftObjectPath().ToString(), IconPath.ToString());
	TestEqual(TEXT("A changed name notifies once"), NameCounter.Count, 2);
	TestEqual(TEXT("The header counts the connected chests"), ReadText(ViewModel, TEXT("ConnectedStorageText")), FString(TEXT("Materials from 2 connected chests")));
	TestEqual(TEXT("Chests sharing a name are numbered"), ReadText(ViewModel, TEXT("ConnectedStorageNamesText")), FString(TEXT("Storage Chest 1 · Storage Chest 2")));

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
	TestEqual(TEXT("A locked row says so"), LockedRow->GetDetailText().ToString(), FString(TEXT("Locked")));

	FFieldCounter StateCounter(CostlyRow, TEXT("RecipeState"));
	TestTrue(TEXT("RecipeState is a field-notify field"), StateCounter.IsBound());
	ViewModel->Refresh();
	TestEqual(TEXT("An unchanged state stays quiet"), StateCounter.Count, 0);

	URpgInventoryManagerComponent* Inventory = NewObject<URpgInventoryManagerComponent>(Fixture.Requester);
	Fixture.Requester->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	Inventory->AddItemDefinition(URpgInventoryAutomationTestMaterialDefinition::StaticClass(), 2);
	ViewModel->Refresh();
	TestEqual(TEXT("Materials in the player's own inventory do not count"), CostlyRow->GetRecipeState(), ERpgCraftingRecipeState::MissingResources);

	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	Chest->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestMaterialDefinition::StaticClass(), 2);
	ViewModel->Refresh();
	TestEqual(TEXT("Materials in a connected chest make the recipe craftable"), CostlyRow->GetRecipeState(), ERpgCraftingRecipeState::Craftable);
	TestEqual(TEXT("The row is reused and its state notifies once"), StateCounter.Count, 1);

	ViewModel->UnbindCraftingStation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelCategoryTreeTest,
	"SurvivalRpg.Crafting.ViewModel.CategoryTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelCategoryTreeTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	const FGameplayTag Building = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Building"));
	const FGameplayTag Materials = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Materials"));
	const FGameplayTag Wood = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Materials.Wood"));
	const FGameplayTag Fuel = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Materials.Fuel"));
	const FGameplayTag Melee = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Weapons.Melee"));
	const FGameplayTag Weapons = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Weapons"));
	URpgCraftingRecipeDefinition* Plank = MakeRecipe(TEXT("Plank"));
	Plank->RecipeCategory = Wood;
	URpgCraftingRecipeDefinition* Charcoal = MakeRecipe(TEXT("Charcoal"));
	Charcoal->RecipeCategory = Fuel;
	URpgCraftingRecipeDefinition* Sword = MakeRecipe(TEXT("Sword"));
	Sword->RecipeCategory = Melee;
	URpgCraftingRecipeDefinition* Kit = MakeRecipe(TEXT("Kit"));
	Kit->RecipeCategory = Building;
	URpgCraftingRecipeDefinition* Loose = MakeRecipe(TEXT("Loose"));
	Fixture.OfferRecipes({ Plank, Charcoal, Sword, Kit, Loose });

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	FFieldCounter RowsCounter(ViewModel, TEXT("CategoryRows"));
	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);
	TestEqual(TEXT("Groups and subcategories sort by name and count every offered recipe"),
		DescribeCategoryRows(ViewModel), FString(TEXT("All recipes 5, Building 1, Materials 2, Fuel 1, Wood 1, Weapons 1, Melee 1")));
	TestEqual(TEXT("Binding publishes the rows once"), RowsCounter.Count, 1);
	TestEqual(TEXT("The list title names all recipes"), ReadText(ViewModel, TEXT("RecipeListTitleText")), FString(TEXT("All recipes")));

	ViewModel->SetCategoryFilter(Materials);
	TestEqual(TEXT("A group filter includes its subcategories"), ViewModel->GetFilteredRecipes().Num(), 2);
	TestEqual(TEXT("The list title names the group"), ReadText(ViewModel, TEXT("RecipeListTitleText")), FString(TEXT("Materials")));
	TestTrue(TEXT("The group row is selected"), ReadValue<bool, FBoolProperty>(FindCategoryRow(ViewModel, ERpgCraftingCategoryRowKind::Group, Materials), TEXT("bSelected")));
	TestEqual(TEXT("Selecting reuses the row objects"), RowsCounter.Count, 1);

	ViewModel->ActivateCategoryRow(FindCategoryRow(ViewModel, ERpgCraftingCategoryRowKind::Group, Materials));
	TestEqual(TEXT("Activating the selected group collapses it"),
		DescribeCategoryRows(ViewModel), FString(TEXT("All recipes 5, Building 1, Materials 2, Weapons 1, Melee 1")));
	ViewModel->ActivateCategoryRow(FindCategoryRow(ViewModel, ERpgCraftingCategoryRowKind::Subcategory, Melee));
	TestEqual(TEXT("A subcategory filters itself"), ViewModel->GetFilteredRecipes().Num(), 1);
	ViewModel->ActivateCategoryRow(FindCategoryRow(ViewModel, ERpgCraftingCategoryRowKind::Group, Materials));
	TestEqual(TEXT("Activating another group expands and filters it"), ViewModel->GetCategoryFilter(), Materials);
	TestNotNull(TEXT("Its subcategories show again"), FindCategoryRow(ViewModel, ERpgCraftingCategoryRowKind::Subcategory, Wood));
	ViewModel->ActivateCategoryRow(FindCategoryRow(ViewModel, ERpgCraftingCategoryRowKind::All, FGameplayTag()));
	TestEqual(TEXT("All clears the filter"), ViewModel->GetFilteredRecipes().Num(), 5);

	URpgCraftingCategoryCatalog* Catalog = NewObject<URpgCraftingCategoryCatalog>(GetTransientPackage(), NAME_None, RF_Transient);
	FRpgCraftingCategoryDisplay& WeaponsRow = Catalog->Categories.AddDefaulted_GetRef();
	WeaponsRow.Category = Weapons;
	WeaponsRow.DisplayName = FText::FromString(TEXT("Arms"));
	WeaponsRow.SortOrder = -1;
	FRpgCraftingCategoryDisplay& WoodRow = Catalog->Categories.AddDefaulted_GetRef();
	WoodRow.Category = Wood;
	WoodRow.DisplayName = FText::FromString(TEXT("Lumber"));
	ViewModel->SetPresentationCatalog(Catalog);
	TestEqual(TEXT("The catalog renames and reorders rows"),
		DescribeCategoryRows(ViewModel), FString(TEXT("All recipes 5, Arms 1, Melee 1, Building 1, Materials 2, Fuel 1, Lumber 1")));

	ViewModel->SetSearchText(FText::FromString(TEXT("no such recipe")));
	TestFalse(TEXT("A search without matches empties the list"), ViewModel->HasFilteredRecipes());
	TestEqual(TEXT("Search leaves the tree counts alone"), ReadValue<int32, FIntProperty>(FindCategoryRow(ViewModel, ERpgCraftingCategoryRowKind::All, FGameplayTag()), TEXT("RecipeCount")), 5);

	ViewModel->UnbindCraftingStation();
	TestEqual(TEXT("Unbinding clears the rows"), ViewModel->GetCategoryRows().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelTierSectionsTest,
	"SurvivalRpg.Crafting.ViewModel.TierSectionsAndSort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelTierSectionsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* Copper = MakeRecipe(TEXT("Copper"));
	URpgCraftingRecipeDefinition* Bronze = MakeRecipe(TEXT("Bronze"));
	URpgCraftingRecipeDefinition* Iron = MakeRecipe(TEXT("Iron"));
	Iron->RecipeTier = 2;
	Fixture.OfferRecipes({ Iron, Copper, Bronze });

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);
	TestEqual(TEXT("Recipes group under tier headers, low tiers first, by name inside a tier"),
		DescribeRecipeList(ViewModel), FString(TEXT("[Tier I] Bronze Copper [Tier II] Iron")));
	TestEqual(TEXT("Tier options list all tiers first"), ViewModel->GetTierOptions().Num(), 3);
	TestEqual(TEXT("The filter starts at all tiers"), ReadText(ViewModel, TEXT("TierFilterText")), FString(TEXT("All tiers")));
	TestEqual(TEXT("The sort toggle shows ascending"), ReadText(ViewModel, TEXT("TierSortText")), FString(TEXT("Tier ↑")));

	ViewModel->ToggleTierSortDirection();
	TestEqual(TEXT("The toggle puts high tiers first"), DescribeRecipeList(ViewModel), FString(TEXT("[Tier II] Iron [Tier I] Bronze Copper")));

	URpgCraftingCategoryCatalog* Catalog = NewObject<URpgCraftingCategoryCatalog>(GetTransientPackage(), NAME_None, RF_Transient);
	FRpgCraftingTierDisplay& TierRow = Catalog->Tiers.AddDefaulted_GetRef();
	TierRow.Tier = 2;
	TierRow.DisplayName = FText::FromString(TEXT("Iron"));
	ViewModel->SetPresentationCatalog(Catalog);
	TestEqual(TEXT("Catalog tier names join the numeral"), DescribeRecipeList(ViewModel), FString(TEXT("[II · Iron] Iron [Tier I] Bronze Copper")));

	ViewModel->SetTierFilter(2);
	TestEqual(TEXT("The tier filter keeps one tier"), DescribeRecipeList(ViewModel), FString(TEXT("[II · Iron] Iron")));
	TestEqual(TEXT("The filter label names the tier"), ReadText(ViewModel, TEXT("TierFilterText")), FString(TEXT("Tier II")));
	ViewModel->CycleTierFilter(1);
	TestEqual(TEXT("Cycling past the last tier wraps to all tiers"), ViewModel->GetTierFilter(), 0);
	ViewModel->SetTierFilter(5);
	TestEqual(TEXT("A tier the station does not offer falls back to all tiers"), ViewModel->GetTierFilter(), 0);
	TestTrue(TEXT("Section headers are not recipe rows"), ViewModel->GetFilteredRecipes().Num() == 3 && ViewModel->GetRecipeListItems().Num() == 5);

	ViewModel->UnbindCraftingStation();
	TestEqual(TEXT("Unbinding clears the list"), ViewModel->GetRecipeListItems().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelTargetStorageTest,
	"SurvivalRpg.Crafting.ViewModel.TargetStorageOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelTargetStorageTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* Recipe = MakeRecipe(TEXT("Unit"), 1.0f);
	Fixture.OfferRecipes({ Recipe });
	ARpgInventoryContainerActor* Full = Fixture.CreateChest(1, 1);
	ARpgInventoryContainerActor* Roomy = Fixture.CreateChest(2, 2);
	Full->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestStackItemDefinition::StaticClass(), 1);
	const bool bFullFirst = ChestId(Full).LexicalLess(ChestId(Roomy));

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);
	TestEqual(TEXT("Both connected chests are offered"), ViewModel->GetTargetStorageOptions().Num(), 2);
	TestEqual(TEXT("The suggestion skips the full chest"), ViewModel->GetSelectedTargetContainerId(), ChestId(Roomy));
	TestTrue(TEXT("The roomy target holds the selection"), ReadValue<bool, FBoolProperty>(ViewModel, TEXT("bTargetHasRoom")));
	TestEqual(TEXT("The room line counts the output"), ReadText(ViewModel, TEXT("TargetCapacityText")),
		FString::Printf(TEXT("Room for 1 of 1 %s"), *GetDefault<URpgInventoryItemDefinition>(URpgInventoryAutomationTestUnitItemDefinition::StaticClass())->DisplayName.ToString()));
	TestTrue(TEXT("The selection may start"), ViewModel->CanStartOrder());

	ViewModel->SelectTargetStorage(ChestId(Full));
	TestEqual(TEXT("A picked chest becomes the target"), ViewModel->GetSelectedTargetContainerId(), ChestId(Full));
	TestFalse(TEXT("A full target has no room"), ReadValue<bool, FBoolProperty>(ViewModel, TEXT("bTargetHasRoom")));
	TestTrue(TEXT("The room line says there is no room"), ReadText(ViewModel, TEXT("TargetCapacityText")).StartsWith(TEXT("No room for")));
	TestFalse(TEXT("A full target blocks the start"), ViewModel->CanStartOrder());
	ViewModel->RefreshSelectedRecipeDetails();
	TestEqual(TEXT("The pick survives a refresh"), ViewModel->GetSelectedTargetContainerId(), ChestId(Full));

	ViewModel->CycleTargetStorage(1);
	TestEqual(TEXT("Cycling moves to the other chest"), ViewModel->GetSelectedTargetContainerId(), ChestId(Roomy));
	TestTrue(TEXT("Cycling keeps sorted option order"), bFullFirst
		? ViewModel->GetTargetStorageOptions()[0]->GetContainerId() == ChestId(Full)
		: ViewModel->GetTargetStorageOptions()[0]->GetContainerId() == ChestId(Roomy));

	ViewModel->UnbindCraftingStation();
	TestEqual(TEXT("Unbinding clears the options"), ViewModel->GetTargetStorageOptions().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelPlanAndPreviewTest,
	"SurvivalRpg.Crafting.ViewModel.PlanAndPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelPlanAndPreviewTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	FStructProperty* PresentationProperty = FindFProperty<FStructProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("Presentation"));
	if (!TestNotNull(TEXT("Station texts are designer data"), PresentationProperty))
	{
		return false;
	}
	FRpgCraftingStationPresentation* Presentation = PresentationProperty->ContainerPtrToValuePtr<FRpgCraftingStationPresentation>(Fixture.Station);
	Presentation->UnitSingular = FText::FromString(TEXT("run"));
	Presentation->UnitPlural = FText::FromString(TEXT("runs"));
	Presentation->StartActionText = FText::FromString(TEXT("Start smelting"));

	URpgCraftingRecipeDefinition* Bars = MakeRecipe(TEXT("Bars"), 3.0f);
	Bars->OutputItems[0].ItemDefinition = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	Bars->OutputItems[0].Count = 2;
	FRpgCraftingResourceCost& Cost = Bars->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Cost.Count = 4;
	URpgCraftingRecipeDefinition* Blade = MakeRecipe(TEXT("Blade"), 12.0f);
	Blade->OutputItems[0].ItemDefinition = URpgItemizationAutomationTestItemDefinition::StaticClass();
	Blade->OutputItemLevel = 10;
	Fixture.OfferRecipes({ Bars, Blade });
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	Chest->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestMaterialDefinition::StaticClass(), 10);
	const FString MaterialName = GetDefault<URpgInventoryItemDefinition>(URpgInventoryAutomationTestMaterialDefinition::StaticClass())->DisplayName.ToString();
	const FString StackName = GetDefault<URpgInventoryItemDefinition>(URpgInventoryAutomationTestStackItemDefinition::StaticClass())->DisplayName.ToString();

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);
	ViewModel->SelectRecipe(Bars);
	ViewModel->SetCraftQuantity(20);
	TestEqual(TEXT("The plan names runs and results"), ReadText(ViewModel, TEXT("PlanSummaryText")), FString::Printf(TEXT("20 runs → 40 %s"), *StackName));
	TestEqual(TEXT("A stackable plan shows the pure time"), ReadText(ViewModel, TEXT("PlanDetailText")), FString(TEXT("1 min 00 s pure time")));
	TestEqual(TEXT("The formula lists one run's inputs"), ReadText(ViewModel, TEXT("FormulaInputsText")), FString::Printf(TEXT("4 %s"), *MaterialName));
	TestEqual(TEXT("The formula lists one run's outputs"), ReadText(ViewModel, TEXT("FormulaOutputText")), FString::Printf(TEXT("2 %s"), *StackName));
	TestEqual(TEXT("The station's start text is used"), ViewModel->GetStartActionText().ToString(), FString(TEXT("Start smelting")));
	TestEqual(TEXT("The materials line counts affordable runs"), ReadText(ViewModel, TEXT("MaterialsSummaryText")), FString(TEXT("From connected chests · enough for 2 runs now")));
	TestEqual(TEXT("A plan beyond the chests still lets the order start and wait"), ViewModel->CanStartOrder(), true);
	ViewModel->SetCraftQuantityToMax();
	TestEqual(TEXT("Max uses what the chests can pay for"), ViewModel->GetCraftQuantity(), 2);

	const TArray<URpgCraftingDetailRowViewModel*> StackRows = ViewModel->GetPreviewRows();
	if (!TestEqual(TEXT("A stackable output shows yield, time and stack size"), StackRows.Num(), 3)) { return false; }
	TestEqual(TEXT("Yield leads"), ReadText(StackRows[0], TEXT("Label")), FString(TEXT("Yield per run")));
	TestEqual(TEXT("Yield counts pieces"), ReadText(StackRows[0], TEXT("ValueText")), FString(TEXT("2 pieces")));
	TestEqual(TEXT("Time per run is shown"), ReadText(StackRows[1], TEXT("ValueText")), FString(TEXT("3 s")));
	TestEqual(TEXT("The stack size is shown"), ReadText(StackRows[2], TEXT("ValueText")), FString(TEXT("up to 10")));
	TestEqual(TEXT("A stackable output says so"), ReadText(ViewModel, TEXT("OutputKindText")), FString(TEXT("Stackable material")));

	ViewModel->SelectRecipe(Blade);
	TestEqual(TEXT("An itemized output says each piece rolls"), ReadText(ViewModel, TEXT("OutputKindText")), FString(TEXT("Single item · individual stats")));
	TArray<FRpgItemStatRange> Ranges;
	GetDefault<URpgItemizationAutomationTestProfile>()->GetBaseStatRanges(10, Ranges);
	const TArray<URpgCraftingDetailRowViewModel*> BladeRows = ViewModel->GetPreviewRows();
	TestTrue(TEXT("Stat ranges lead the itemized preview"), !Ranges.IsEmpty() && BladeRows.Num() >= Ranges.Num() &&
		ReadText(BladeRows[0], TEXT("Label")).StartsWith(GetRpgItemStatDisplayName(Ranges[0].StatTag).ToString()));
	TestEqual(TEXT("Single items show their cell size"), ReadText(ViewModel, TEXT("OutputSizeText")), FString(TEXT("Space per item 1 × 1 cells")));

	ViewModel->UnbindCraftingStation();
	TestTrue(TEXT("Unbinding clears the plan"), ReadText(ViewModel, TEXT("PlanSummaryText")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelOrderStatusTest,
	"SurvivalRpg.Crafting.ViewModel.OrderStatusText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelOrderStatusTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingViewModelPresentationTests;

	FScopedStationWorld Fixture;
	if (!TestTrue(TEXT("The station fixture exists"), Fixture.IsValid()))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* Recipe = MakeRecipe(TEXT("Plank"), 10.0f);
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Cost.Count = 2;
	Fixture.OfferRecipes({ Recipe });
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	Chest->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestMaterialDefinition::StaticClass(), 2);

	URpgCraftingStationViewModel* ViewModel = NewObject<URpgCraftingStationViewModel>(Fixture.Station, NAME_None, RF_Transient);
	ViewModel->BindCraftingStation(Fixture.Station, Fixture.Requester);
	TestEqual(TEXT("An idle station waits for the player"), ReadText(ViewModel, TEXT("OrderStatusText")), FString(TEXT("Waiting for your start")));
	TestEqual(TEXT("The idle title names the station"), ReadText(ViewModel, TEXT("OrderTitleText")), FString(TEXT("Crafting Station ready")));

	if (!TestTrue(TEXT("The station starts the order"), Fixture.Station->StartCraftingOrder(Fixture.Requester, Recipe, 2, ChestId(Chest)))) { return false; }
	ViewModel->Refresh();
	TestTrue(TEXT("The view model sees the order"), ViewModel->HasActiveOrder());
	TestTrue(TEXT("A running order shows its remaining time"), ReadText(ViewModel, TEXT("OrderStatusText")).StartsWith(TEXT("Running · ")));
	TestEqual(TEXT("Counts name the units"), ReadText(ViewModel, TEXT("OrderCountsText")), FString(TEXT("0 / 2 pieces done")));
	TestEqual(TEXT("The start action is blocked while an order runs"), ViewModel->GetStartActionText().ToString(), FString(TEXT("An order is active")));
	TestFalse(TEXT("A second order cannot start"), ViewModel->CanStartOrder());

	FTimerManager& Timers = Fixture.World->GetTimerManager();
	TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
	++GFrameCounter; Timers.Tick(0.0f);
	++GFrameCounter; Timers.Tick(11.0f);
	ViewModel->Refresh();
	TestEqual(TEXT("Without materials the order waits"), ReadText(ViewModel, TEXT("OrderStatusText")), FString(TEXT("Waiting for materials")));
	TestTrue(TEXT("The hint names what is missing"),
		ReadText(ViewModel, TEXT("OrderHintText")).Contains(GetDefault<URpgInventoryItemDefinition>(URpgInventoryAutomationTestMaterialDefinition::StaticClass())->DisplayName.ToString()));
	TestTrue(TEXT("A waiting order is flagged"), ReadValue<bool, FBoolProperty>(ViewModel, TEXT("bOrderWaiting")));
	TestEqual(TEXT("Progress covers the delivered unit"), ReadValue<float, FFloatProperty>(ViewModel, TEXT("OrderProgress")), 0.5f);

	TestTrue(TEXT("The order pauses"), Fixture.Station->PauseCraftingStation(Fixture.Requester));
	ViewModel->Refresh();
	TestEqual(TEXT("A paused order says so"), ReadText(ViewModel, TEXT("OrderStatusText")), FString(TEXT("Paused")));
	TestTrue(TEXT("The order stops"), Fixture.Station->StopCraftingOrder(Fixture.Requester, ViewModel->GetActiveOrderId()));
	ViewModel->Refresh();
	TestFalse(TEXT("The view model sees the idle station"), ViewModel->HasActiveOrder());

	ViewModel->UnbindCraftingStation();
	TestTrue(TEXT("Unbinding clears the strip"), ReadText(ViewModel, TEXT("OrderStatusText")).IsEmpty());
	return true;
}

#endif
