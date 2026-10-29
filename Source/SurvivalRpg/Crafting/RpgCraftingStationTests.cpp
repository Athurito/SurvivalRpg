#include "RpgCraftingRecipeDefinition.h"
#include "RpgCraftingStationActor.h"
#include "RpgCraftingStationComponent.h"
#include "RpgCraftingAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Crafting/RpgRecipeUnlockComponent.h"
#include "SurvivalRpg/Runes/RpgRuneAutomationTestTypes.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageTypes.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPhysicalStorageViewModel.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RpgCraftingStationTests
{
	class FScopedCraftingWorld
	{
	public:
		FScopedCraftingWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedCraftingWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}

			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}

			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		bool Initialize(FAutomationTestBase& Test, bool bUsePersistentGameMode = false)
		{
			if (!GameInstance || !World)
			{
				Test.AddError(TEXT("Could not create an isolated standalone crafting test world."));
				return false;
			}

			if (bUsePersistentGameMode)
			{
				FURL GameUrl;
				GameUrl.AddOption(*FString::Printf(TEXT("game=%s"), *ARpgGameModeBase::StaticClass()->GetPathName()));
				if (!World->SetGameMode(GameUrl)) { return false; }
				World->GetAuthGameMode<ARpgGameModeBase>()->bEnableDiskPersistence = false;
				World->InitializeActorsForPlay(GameUrl);
			}

			FActorSpawnParameters ControllerSpawnParameters;
			ControllerSpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerController::StaticClass(),
				TEXT("CraftingTestController"));
			ControllerSpawnParameters.ObjectFlags = RF_Transient;
			RequestingController = World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

			FActorSpawnParameters StationSpawnParameters;
			StationSpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgCraftingStationActor::StaticClass(),
				TEXT("CraftingTestStation"));
			StationSpawnParameters.ObjectFlags = RF_Transient;
			StationActor = World->SpawnActor<ARpgCraftingStationActor>(StationSpawnParameters);
			Station = StationActor ? StationActor->GetCraftingStationComponent() : nullptr;

			return Test.TestNotNull(TEXT("The crafting requester fixture exists"), RequestingController.Get()) &&
				Test.TestNotNull(TEXT("The crafting station actor fixture exists"), StationActor.Get()) &&
				Test.TestNotNull(TEXT("The crafting station component fixture exists"), Station.Get());
		}

		URpgCraftingRecipeDefinition* CreateRecipe() const
		{
			URpgCraftingRecipeDefinition* Recipe = NewObject<URpgCraftingRecipeDefinition>(
				GetTransientPackage(),
				NAME_None,
				RF_Transient);
			if (Recipe)
			{
				Recipe->bUnlockedByDefault = true;
				FRpgCraftingOutputItem& Output = Recipe->OutputItems.AddDefaulted_GetRef();
				Output.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
				Output.Count = 1;
			}
			return Recipe;
		}

		/** One unit costs CostCount automation materials and takes CraftTime seconds. */
		URpgCraftingRecipeDefinition* CreateMaterialRecipe(int32 CostCount, float CraftTime) const
		{
			URpgCraftingRecipeDefinition* Recipe = CreateRecipe();
			if (Recipe)
			{
				Recipe->CraftTime = CraftTime;
				FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
				Cost.ItemDefinition = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
				Cost.Count = CostCount;
			}
			return Recipe;
		}

		bool OfferRecipes(
			FAutomationTestBase& Test,
			const TArray<URpgCraftingRecipeDefinition*>& Recipes,
			URpgCraftingStationComponent* TargetStation = nullptr)
		{
			RecipeSet = NewObject<URpgCraftingRecipeSet>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!Test.TestNotNull(TEXT("The transient recipe set exists"), RecipeSet.Get()))
			{
				return false;
			}

			for (URpgCraftingRecipeDefinition* Recipe : Recipes)
			{
				RecipeSet->Recipes.Add(Recipe);
			}

			FObjectPropertyBase* AvailableRecipeSetProperty = FindFProperty<FObjectPropertyBase>(
				URpgCraftingStationComponent::StaticClass(),
				TEXT("AvailableRecipeSet"));
			if (!Test.TestNotNull(TEXT("The station recipe-set property exists"), AvailableRecipeSetProperty))
			{
				return false;
			}

			AvailableRecipeSetProperty->SetObjectPropertyValue_InContainer(TargetStation ? TargetStation : Station.Get(), RecipeSet.Get());
			return true;
		}

		ARpgInventoryContainerActor* CreateChest(int32 GridWidth = 0, int32 GridHeight = 0, FVector Location = FVector::ZeroVector)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARpgInventoryContainerActor* Chest = World->SpawnActor<ARpgInventoryContainerActor>(Location, FRotator::ZeroRotator, SpawnParameters);
			if (Chest)
			{
				Chest->GetContainerComponent()->EnsurePersistentContainerId();
				if (GridWidth > 0 && GridHeight > 0)
				{
					FRpgInventoryGridSize Grid;
					Grid.Width = GridWidth;
					Grid.Height = GridHeight;
					Chest->GetInventoryManager()->SetDefaultGridSize(Grid);
				}
			}
			return Chest;
		}

		URpgInventoryManagerComponent* CreatePlayerInventory(AActor* Player = nullptr)
		{
			if (!Player) { Player = RequestingController; }
			URpgInventoryManagerComponent* Inventory = NewObject<URpgInventoryManagerComponent>(Player);
			Player->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

		ARpgInventoryAutomationTestPlayerController* GetRequestingController() const
		{
			return RequestingController;
		}

		URpgCraftingStationComponent* GetStation() const
		{
			return Station;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
		TObjectPtr<ARpgInventoryAutomationTestPlayerController> RequestingController;
		TObjectPtr<ARpgCraftingStationActor> StationActor;
		TObjectPtr<URpgCraftingStationComponent> Station;
		TObjectPtr<URpgCraftingRecipeSet> RecipeSet;
	};

	FName ChestId(const ARpgInventoryContainerActor* Chest)
	{
		return Chest ? Chest->GetContainerComponent()->GetPersistentContainerId() : NAME_None;
	}

	int32 CountDroppedOutputActors(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	void AdvanceCraftingTimers(UWorld* World, float Seconds)
	{
		// TimerManager processes each frame once; isolated fixtures have no engine frame loop.
		TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
		++GFrameCounter;
		World->GetTimerManager().Tick(0.0f);
		++GFrameCounter;
		World->GetTimerManager().Tick(Seconds);
	}

	int32 CountUnitCredits(const URpgCraftingStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> Material)
	{
		int32 Total = 0;
		for (const FRpgCraftingRefundEntry& Credit : Station->GetCurrentOrder().UnitCredits)
		{
			if (Credit.ItemDefinition == Material) { Total += Credit.Count; }
		}
		return Total;
	}

	int32 CountOf(const ARpgInventoryContainerActor* Chest, TSubclassOf<URpgInventoryItemDefinition> Definition)
	{
		return Chest ? Chest->GetInventoryManager()->GetTotalItemCountByDefinition(Definition) : 0;
	}

	bool RemoveFirst(ARpgInventoryContainerActor* Chest, TSubclassOf<URpgInventoryItemDefinition> Definition)
	{
		for (const FRpgInventoryEntryView& Entry : Chest->GetInventoryManager()->GetAllEntries())
		{
			if (Entry.Instance && Entry.Instance->GetItemDef() == Definition)
			{
				return Chest->GetInventoryManager()->ConsumeItemById(Entry.ItemId, Entry.StackCount).IsSuccess();
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingRejectsRecipeOutsideStationSetTest,
	"SurvivalRpg.Crafting.Authority.RejectsRecipeOutsideStationSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingRejectsRecipeOutsideStationSetTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* OfferedRecipe = TestWorld.CreateRecipe();
	URpgCraftingRecipeDefinition* OutsiderRecipe = TestWorld.CreateRecipe();
	ARpgInventoryContainerActor* Target = TestWorld.CreateChest();
	if (!TestNotNull(TEXT("The offered recipe fixture exists"), OfferedRecipe) ||
		!TestNotNull(TEXT("The outsider recipe fixture exists"), OutsiderRecipe) ||
		!TestNotNull(TEXT("The target chest exists"), Target) ||
		!TestWorld.OfferRecipes(*this, { OfferedRecipe }))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	AActor* RequestingActor = TestWorld.GetRequestingController();
	TestTrue(TEXT("The configured recipe is offered by the station"), Station->GetAvailableRecipes().Contains(OfferedRecipe));
	TestFalse(TEXT("An unconfigured recipe is absent from the station offer"), Station->GetAvailableRecipes().Contains(OutsiderRecipe));
	TestFalse(TEXT("The pre-check rejects an unconfigured recipe"), Station->CanStartCraftingOrder(RequestingActor, OutsiderRecipe, 1, ChestId(Target)));
	TestFalse(TEXT("The authoritative start rejects an unconfigured recipe"), Station->StartCraftingOrder(RequestingActor, OutsiderRecipe, 1, ChestId(Target)));
	TestTrue(TEXT("The offered recipe passes the pre-check"), Station->CanStartCraftingOrder(RequestingActor, OfferedRecipe, 1, ChestId(Target)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingWorldKnowledgeOfferGateTest,
	"SurvivalRpg.Crafting.Knowledge.WorldSharedOfferGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingWorldKnowledgeOfferGateTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* Recipe = TestWorld.CreateRecipe();
	if (!TestNotNull(TEXT("Knowledge-gated recipe fixture exists"), Recipe) ||
		!TestWorld.OfferRecipes(*this, { Recipe }))
	{
		return false;
	}
	Recipe->RequiredWorldKnowledgeTags.AddTag(
		RpgGameplayTags::Storage_Knowledge_MaterialStandardization_Basic);
	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	TestFalse(
		TEXT("Recipe stays hidden while the shared discovery is absent"),
		Station->GetAvailableRecipes().Contains(Recipe));

	UWorld* World = Station ? Station->GetWorld() : nullptr;
	ARpgGameStateBase* GameState = World
		? World->SpawnActor<ARpgGameStateBase>()
		: nullptr;
	if (!TestNotNull(TEXT("Knowledge fixture GameState exists"), GameState))
	{
		return false;
	}
	World->SetGameState(GameState);
	URpgWorldStorageKnowledgeComponent* Knowledge =
		GameState->GetWorldStorageKnowledgeComponent();
	if (!TestNotNull(TEXT("GameState owns shared storage knowledge"), Knowledge))
	{
		return false;
	}
	TestFalse(
		TEXT("Creating the world knowledge component does not unlock the recipe"),
		Station->GetAvailableRecipes().Contains(Recipe));
	TestTrue(
		TEXT("Authority grants the non-exclusive material competence node once"),
		Knowledge->GrantKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_MaterialStandardization_Basic));
	TestTrue(
		TEXT("Every member now sees the knowledge-gated recipe offer"),
		Station->GetAvailableRecipes().Contains(Recipe));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingOrderQuantityLimitTest,
	"SurvivalRpg.Crafting.Order.QuantityLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingOrderQuantityLimitTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* FreeRecipe = TestWorld.CreateRecipe();
	ARpgInventoryContainerActor* Target = TestWorld.CreateChest();
	if (!TestNotNull(TEXT("The free recipe fixture exists"), FreeRecipe) || !Target ||
		!TestWorld.OfferRecipes(*this, { FreeRecipe }))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	AActor* RequestingActor = TestWorld.GetRequestingController();
	TestEqual(TEXT("An order is capped by the station's configured maximum"), Station->GetMaxOrderQuantity(), 99);
	TestEqual(TEXT("A free recipe is affordable up to that maximum"), Station->GetAffordableUnitCount(FreeRecipe), 99);
	TestTrue(TEXT("The maximum is accepted"), Station->CanStartCraftingOrder(RequestingActor, FreeRecipe, 99, ChestId(Target)));
	TestFalse(TEXT("A quantity above the maximum is rejected"), Station->CanStartCraftingOrder(RequestingActor, FreeRecipe, 100, ChestId(Target)));
	TestFalse(TEXT("A zero quantity is rejected"), Station->CanStartCraftingOrder(RequestingActor, FreeRecipe, 0, ChestId(Target)));
	TestFalse(TEXT("The authoritative start rejects a huge request"), Station->StartCraftingOrder(RequestingActor, FreeRecipe, MAX_int32, ChestId(Target)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingChestOnlySourcesTest,
	"SurvivalRpg.Crafting.Order.ChestOnlySources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingChestOnlySourcesTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	URpgInventoryManagerComponent* Player = Fixture.CreatePlayerInventory();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	if (!TestNotNull(TEXT("Shared physical chest exists"), Chest) || !Target) { return false; }
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Player->AddItemDefinition(Material, 3);
	Chest->GetInventoryManager()->AddItemDefinition(Material, 9);
	TestEqual(TEXT("Available materials count connected chests only"), Station->GetAvailableResourceCount(Material), 9);
	AActor* OtherPlayer = Station->GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	URpgInventoryManagerComponent* OtherInventory = Fixture.CreatePlayerInventory(OtherPlayer);
	OtherInventory->AddItemDefinition(Material, 10);
	TestEqual(TEXT("Another player's materials never count"), Station->GetAvailableResourceCount(Material), 9);

	FRpgCraftingResourceCost Cost;
	Cost.ItemDefinition = Material;
	Cost.Count = 5;
	TArray<FRpgInventoryBatchOperation> Operations;
	TArray<FRpgCraftingRefundEntry> Credits;
	TestTrue(TEXT("The chest plan covers five"),
		URpgCraftingStationComponent::BuildStorageConsumptionPlan({ Player, OtherInventory, Chest->GetInventoryManager() }, { Cost }, 1, Operations, Credits));
	TestTrue(TEXT("Every planned debit comes from the chest even when player inventories are passed in"),
		!Operations.IsEmpty() && !Operations.ContainsByPredicate([Chest](const FRpgInventoryBatchOperation& Operation)
		{
			return Operation.SourceInventory != Chest->GetInventoryManager();
		}));
	Cost.Count = 10;
	TestFalse(TEXT("Player inventories cannot cover a shortfall"),
		URpgCraftingStationComponent::BuildStorageConsumptionPlan({ Player, OtherInventory, Chest->GetInventoryManager() }, { Cost }, 1, Operations, Credits));

	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(5, 30.0f);
	if (!Fixture.OfferRecipes(*this, { Recipe })) { return false; }
	TestTrue(TEXT("The order starts from chest materials"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Target)));
	TestEqual(TEXT("The player's own materials stay untouched"), Player->GetTotalItemCountByDefinition(Material), 3);
	TestEqual(TEXT("The first unit took its cost from the chest"), CountOf(Chest, Material), 4);
	TestEqual(TEXT("The paid unit holds exactly its credits"), CountUnitCredits(Station, Material), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingStartRequiresConnectedTargetTest,
	"SurvivalRpg.Crafting.Order.StartRequiresConnectedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStartRequiresConnectedTargetTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Near = Fixture.CreateChest();
	ARpgInventoryContainerActor* Far = Fixture.CreateChest(0, 0, FVector(5000.0f, 0.0f, 0.0f));
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 30.0f;
	if (!Near || !Far || !Fixture.OfferRecipes(*this, { Recipe })) { return false; }

	TestTrue(TEXT("No target stores automatically into a connected chest"), Station->CanStartCraftingOrder(Requester, Recipe, 1, NAME_None));
	TestFalse(TEXT("An unknown target is rejected"), Station->CanStartCraftingOrder(Requester, Recipe, 1, TEXT("Chest_Unknown")));
	TestNull(TEXT("A chest outside the station's reach is not connected"), Station->FindConnectedStorageInventory(ChestId(Far)));
	TestFalse(TEXT("A chest outside the station's reach is rejected as target"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Far)));
	TestTrue(TEXT("A connected chest is accepted"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Near)));
	TestEqual(TEXT("The order remembers its target"), Station->GetCurrentOrder().TargetContainerId, ChestId(Near));
	TestFalse(TEXT("A station runs one order at a time"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Near)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingAutomaticTargetTest,
	"SurvivalRpg.Crafting.Order.AutomaticTargetFollowsAssignments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingAutomaticTargetTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	ARpgInventoryContainerActor* MaterialChest = Fixture.CreateChest();
	ARpgInventoryContainerActor* ProductChest = Fixture.CreateChest(1, 1);
	ARpgInventoryContainerActor* FreeChest = Fixture.CreateChest(1, 1);
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!MaterialChest || !ProductChest || !FreeChest || !MaterialChest->GetInventoryManager()->AddItemDefinition(Material, 20) ||
		!Fixture.OfferRecipes(*this, { Recipe })) { return false; }
	FRpgStorageAssignment MaterialRule;
	MaterialRule.ItemDefinition = Material;
	FRpgStorageAssignment ProductRule;
	ProductRule.ItemDefinition = Product;
	MaterialChest->GetContainerComponent()->SetAssignments({ MaterialRule });
	ProductChest->GetContainerComponent()->SetAssignments({ MaterialRule });
	FreeChest->GetContainerComponent()->SetAssignments({ MaterialRule });
	// A chest meant for other materials stays excluded even when it already holds some product.
	if (!MaterialChest->GetInventoryManager()->AddItemDefinition(Product, 1)) { return false; }

	TestTrue(TEXT("Chests meant for other materials never receive automatic output"), Station->GetOutputTargets(Recipe, NAME_None).IsEmpty());
	TestFalse(TEXT("Automatic storing without a suitable chest cannot start"), Station->CanStartCraftingOrder(Requester, Recipe, 1, NAME_None));
	TestTrue(TEXT("A named chest stays a valid explicit target"), Station->CanStartCraftingOrder(Requester, Recipe, 1, ChestId(FreeChest)));

	ProductChest->GetContainerComponent()->SetAssignments({ ProductRule });
	FreeChest->GetContainerComponent()->SetAssignments({});
	const TArray<URpgInventoryManagerComponent*> Targets = Station->GetOutputTargets(Recipe, NAME_None);
	if (!TestEqual(TEXT("The assigned and the unassigned chest are automatic targets"), Targets.Num(), 2)) { return false; }
	TestTrue(TEXT("The chest assigned to the product comes first"), Targets[0] == ProductChest->GetInventoryManager());
	TestTrue(TEXT("The unassigned chest follows"), Targets[1] == FreeChest->GetInventoryManager());

	if (!TestTrue(TEXT("An automatic order starts"), Station->StartCraftingOrder(Requester, Recipe, 3, NAME_None))) { return false; }
	TestTrue(TEXT("The order stores automatically"), Station->GetCurrentOrder().TargetContainerId.IsNone());
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The first unit fills the assigned chest"), CountOf(ProductChest, Product), 1);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The second unit moves on to the unassigned chest"), CountOf(FreeChest, Product), 1);
	TestEqual(TEXT("Without room anywhere the order waits for space"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForSpace);
	TestFalse(TEXT("The waiting unit is not paid"), Station->GetCurrentOrder().bUnitPaid);
	TestEqual(TEXT("Only the two delivered units consumed material"), CountOf(MaterialChest, Material), 16);
	TestEqual(TEXT("The material chest never receives more product"), CountOf(MaterialChest, Product), 1);

	const FRpgCraftingStationSaveData Save = Station->ExportCraftingState();
	TestTrue(TEXT("Automatic storing is saved as no target"), Save.bHasOrder && Save.Order.TargetContainerId.IsNone());
	TestTrue(TEXT("An automatic order restores"), Station->RestoreCraftingState(Save));
	Station->ResumeRestoredCrafting();

	if (!RemoveFirst(ProductChest, Product)) { return false; }
	AdvanceCraftingTimers(World, 1.1f);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("Freed room in the assigned chest takes the last unit"), CountOf(ProductChest, Product), 1);
	TestFalse(TEXT("The order completes"), Station->HasCraftingOrder());
	TestEqual(TEXT("Three units consumed three costs"), CountOf(MaterialChest, Material), 14);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingConsumesPerUnitTest,
	"SurvivalRpg.Crafting.Order.ConsumesPerUnitAndDeliversToTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingConsumesPerUnitTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest(4, 4);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 10) || !Fixture.OfferRecipes(*this, { Recipe })) { return false; }

	TestTrue(TEXT("A three-unit order starts"), Station->StartCraftingOrder(Fixture.GetRequestingController(), Recipe, 3, ChestId(Target)));
	TestEqual(TEXT("Only the first unit's materials are taken at start"), CountOf(Source, Material), 8);
	TestTrue(TEXT("The first unit is paid"), Station->GetCurrentOrder().bUnitPaid);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The first unit lands in the target chest"), CountOf(Target, Product), 1);
	TestEqual(TEXT("The second unit pays when it starts"), CountOf(Source, Material), 6);
	AdvanceCraftingTimers(World, 0.2f);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("Every unit is delivered once"), CountOf(Target, Product), 3);
	TestEqual(TEXT("No unit beyond the order is paid"), CountOf(Source, Material), 4);
	TestFalse(TEXT("The finished order ends"), Station->HasCraftingOrder());
	TestEqual(TEXT("Outputs never become world drops"), CountDroppedOutputActors(World), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingWaitsForMaterialsTest,
	"SurvivalRpg.Crafting.Order.WaitsForMaterialsAndResumes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingWaitsForMaterialsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 2) || !Fixture.OfferRecipes(*this, { Recipe })) { return false; }

	TestTrue(TEXT("An order for more than the chests hold may start"), Station->StartCraftingOrder(Fixture.GetRequestingController(), Recipe, 2, ChestId(Target)));
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The paid unit completes"), CountOf(Target, Product), 1);
	TestTrue(TEXT("The order keeps waiting"), Station->HasCraftingOrder());
	TestEqual(TEXT("It waits for materials"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForMaterials);
	TestFalse(TEXT("Nothing is paid while waiting"), Station->GetCurrentOrder().bUnitPaid);
	Source->GetInventoryManager()->AddItemDefinition(Material, 2);
	AdvanceCraftingTimers(World, 1.1f);
	TestEqual(TEXT("A retry pays the next unit once materials arrive"), CountOf(Source, Material), 0);
	TestEqual(TEXT("The order runs again"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::Running);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The remaining unit completes"), CountOf(Target, Product), 2);
	TestFalse(TEXT("The order ends"), Station->HasCraftingOrder());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingRuneCopyTest,
	"SurvivalRpg.Crafting.Order.CraftedRuneIsANewRankOneCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingRuneCopyTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgGameStateBase* GameState = World->SpawnActor<ARpgGameStateBase>();
	if (!TestNotNull(TEXT("The world has a GameState"), GameState)) { return false; }
	World->SetGameState(GameState);
	URpgRecipeUnlockComponent* Unlocks = GameState->GetRecipeUnlockComponent();

	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> RuneDefinition = URpgRuneAutomationTestRuneDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	Recipe->bUnlockedByDefault = false;
	Recipe->RecipeUnlockTag = FGameplayTag::RequestGameplayTag(TEXT("Recipe.AutomationRune.A"));
	Recipe->OutputItems[0].ItemDefinition = RuneDefinition;
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* FullTarget = Fixture.CreateChest(1, 1);
	ARpgInventoryContainerActor* Target = Fixture.CreateChest(4, 4);
	if (!Source || !FullTarget || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 6) ||
		!FullTarget->GetInventoryManager()->AddItemDefinition(Material, 1) || !Fixture.OfferRecipes(*this, { Recipe }))
	{
		return false;
	}
	URpgInventoryItemInstance* FoundRune = Target->GetInventoryManager()->AddItemDefinition(RuneDefinition, 1);
	if (!TestNotNull(TEXT("A found rune lies in the target"), FoundRune) ||
		!TestTrue(TEXT("The found rune was upgraded"), FoundRune->SetRuneRank(2)))
	{
		return false;
	}

	TestTrue(TEXT("The station lists the rune recipe"), Station->GetAvailableRecipes().Contains(Recipe));
	TestFalse(TEXT("The rune recipe starts locked"), Station->IsRecipeUnlocked(Recipe));
	TestFalse(TEXT("A locked rune recipe cannot start"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Target)));
	TestTrue(TEXT("The world learns the recipe"), Unlocks->UnlockRecipe(Recipe));
	TestTrue(TEXT("The learned recipe is unlocked"), Station->IsRecipeUnlocked(Recipe));

	TestFalse(TEXT("A full target refuses the order"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(FullTarget)));
	TestEqual(TEXT("The refused order consumes nothing"), CountOf(Source, Material), 6);

	TestTrue(TEXT("The order starts"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Target)));
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("One unit's costs are paid"), CountOf(Source, Material), 4);
	TestEqual(TEXT("The target holds the found rune and the crafted one"), CountOf(Target, RuneDefinition), 2);
	URpgInventoryItemInstance* Crafted = nullptr;
	for (URpgInventoryItemInstance* Item : Target->GetInventoryManager()->GetAllItems())
	{
		if (Item && Item != FoundRune && Item->GetItemDef() == RuneDefinition)
		{
			Crafted = Item;
		}
	}
	if (!TestNotNull(TEXT("The crafted rune is its own instance"), Crafted))
	{
		return false;
	}
	TestTrue(TEXT("The crafted rune has its own identity"), Crafted->GetItemId() != FoundRune->GetItemId());
	TestEqual(TEXT("The crafted rune starts at rank I"), Crafted->GetRuneRank(), 1);
	TestEqual(TEXT("The found rune keeps its rank"), FoundRune->GetRuneRank(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingRuneWorkTest,
	"SurvivalRpg.Crafting.RuneWork.UpgradeAndSalvageTheChosenRune",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingRuneWorkTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgGameStateBase* GameState = World->SpawnActor<ARpgGameStateBase>();
	World->SetGameState(GameState);

	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Essence = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> RuneDefinition = URpgRuneAutomationTestRuneDefinition::StaticClass();
	auto Costs = [Material](const int32 Count)
	{
		FRpgCraftingResourceCost Cost;
		Cost.ItemDefinition = Material;
		Cost.Count = Count;
		return TArray<FRpgCraftingResourceCost>{Cost};
	};
	auto Outputs = [Essence](const int32 Count)
	{
		FRpgCraftingOutputItem Output;
		Output.ItemDefinition = Essence;
		Output.Count = Count;
		return TArray<FRpgCraftingOutputItem>{Output};
	};
	// Rank I salvages into two different items, more cells than removing the rune frees.
	TArray<FRpgCraftingOutputItem> RankOneOutputs = Outputs(2);
	RankOneOutputs.Append(Outputs(1));
	RankOneOutputs.Last().ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	URpgRuneAutomationTestRuneDefinition::SetRankWork(1, {}, RankOneOutputs);
	URpgRuneAutomationTestRuneDefinition::SetRankWork(2, Costs(2), Outputs(5));
	URpgRuneAutomationTestRuneDefinition::SetRankWork(3, Costs(3), Outputs(9));
	ON_SCOPE_EXIT
	{
		for (int32 Rank = 1; Rank <= 3; ++Rank)
		{
			URpgRuneAutomationTestRuneDefinition::SetRankWork(Rank, {}, {});
		}
	};

	URpgInventoryManagerComponent* Player = Fixture.CreatePlayerInventory();
	FRpgInventoryGridSize PlayerGrid;
	PlayerGrid.Width = 3;
	PlayerGrid.Height = 1;
	Player->SetDefaultGridSize(PlayerGrid);
	URpgInventoryItemInstance* Kept = Player->AddItemDefinition(RuneDefinition, 1);
	URpgInventoryItemInstance* Chosen = Player->AddItemDefinition(RuneDefinition, 1);
	URpgInventoryItemInstance* PlayerFiller = Player->AddItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1);
	if (!TestNotNull(TEXT("The kept rune exists"), Kept) || !TestNotNull(TEXT("The chosen rune exists"), Chosen) ||
		!TestNotNull(TEXT("A filler fills the player's inventory"), PlayerFiller)) { return false; }

	TestEqual(TEXT("A station without rune work refuses"), Station->UpgradeRune(Requester, Chosen->GetItemId()), ERpgRuneWorkResult::NotOffered);
	FBoolProperty* OffersRuneWork = FindFProperty<FBoolProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("bOffersRuneWork"));
	if (!TestNotNull(TEXT("The station has a rune work switch"), OffersRuneWork)) { return false; }
	OffersRuneWork->SetPropertyValue_InContainer(Station, true);

	// Only a full one-cell chest is connected and the player's inventory is full: salvage has no room and keeps the rune.
	ARpgInventoryContainerActor* Tiny = Fixture.CreateChest(1, 1);
	if (!Tiny || !Tiny->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1)) { return false; }
	TestEqual(TEXT("Salvage without room is refused"), Station->SalvageRune(Requester, Kept->GetItemId()), ERpgRuneWorkResult::NoRoom);
	TestTrue(TEXT("The rune stays without room"), Player->ContainsItemInstance(Kept));
	if (!TestTrue(TEXT("The filler leaves"), Player->ConsumeItemById(PlayerFiller->GetItemId(), 1).IsSuccess())) { return false; }
	TestEqual(TEXT("Without materials the upgrade is refused"), Station->UpgradeRune(Requester, Chosen->GetItemId()), ERpgRuneWorkResult::MissingMaterials);
	TestEqual(TEXT("A refused upgrade keeps the rank"), Chosen->GetRuneRank(), 1);

	ARpgInventoryContainerActor* Chest = Fixture.CreateChest(4, 4);
	if (!Chest || !Chest->GetInventoryManager()->AddItemDefinition(Material, 3)) { return false; }
	TestEqual(TEXT("An unknown rune is refused"), Station->UpgradeRune(Requester, FRpgInventoryItemId::NewId()), ERpgRuneWorkResult::NotFound);
	TestEqual(TEXT("The chosen rune is upgraded"), Station->UpgradeRune(Requester, Chosen->GetItemId()), ERpgRuneWorkResult::Done);
	TestEqual(TEXT("The chosen rune reaches rank II"), Chosen->GetRuneRank(), 2);
	TestEqual(TEXT("The other rune keeps rank I"), Kept->GetRuneRank(), 1);
	TestEqual(TEXT("Rank II's cost is paid from the chest"), CountOf(Chest, Material), 1);
	TestEqual(TEXT("Rank III costs more than the chest holds"), Station->UpgradeRune(Requester, Chosen->GetItemId()), ERpgRuneWorkResult::MissingMaterials);
	TestEqual(TEXT("Insufficient materials change nothing"), Chosen->GetRuneRank(), 2);
	TestEqual(TEXT("Insufficient materials take nothing"), CountOf(Chest, Material), 1);
	Chest->GetInventoryManager()->AddItemDefinition(Material, 2);
	TestEqual(TEXT("The chosen rune reaches rank III"), Station->UpgradeRune(Requester, Chosen->GetItemId()), ERpgRuneWorkResult::Done);
	TestEqual(TEXT("A rune at rank III cannot rise further"), Station->EvaluateRuneUpgrade(Requester, Chosen->GetItemId()), ERpgRuneWorkResult::MaxRank);

	// Salvage returns the rank's outputs and keeps what the world knows.
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->bUnlockedByDefault = false;
	Recipe->RecipeUnlockTag = FGameplayTag::RequestGameplayTag(TEXT("Recipe.AutomationRune.A"));
	GameState->GetRecipeUnlockComponent()->UnlockRecipe(Recipe);
	TestEqual(TEXT("The chosen rune is salvaged"), Station->SalvageRune(Requester, Chosen->GetItemId()), ERpgRuneWorkResult::Done);
	TestFalse(TEXT("The salvaged rune is gone"), Player->ContainsItemInstance(Chosen));
	TestEqual(TEXT("Rank III's outputs arrive in the chest"), CountOf(Chest, Essence), 9);
	TestTrue(TEXT("The kept rune stays"), Player->ContainsItemInstance(Kept));
	TestTrue(TEXT("Salvage keeps the recipe known"), GameState->GetRecipeUnlockComponent()->IsRecipeUnlocked(Recipe));

	// A socketed rune is refused.
	URpgInventoryItemInstance* Tool = Player->AddItemDefinition(URpgRuneAutomationTestToolDefinition::StaticClass(), 1);
	FRpgInventoryGridPlacement ToolPlacement;
	if (!Tool || !Player->GetItemPlacement(Tool, ToolPlacement)) { return false; }
	FRpgInventoryGridPlacement SocketPlacement;
	SocketPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(
		Tool->GetItemId(), RpgRuneAutomationTestSocketGridId, ToolPlacement.GetContainerHandle().GetDirectChildDepth()));
	SocketPlacement.X = 0;
	SocketPlacement.Y = 0;
	for (const FRpgInventoryEntryView& Entry : Player->GetAllEntries())
	{
		if (Entry.ItemId == Kept->GetItemId())
		{
			FRpgInventoryMoveIntent Intent;
			Intent.EnsureRequestId();
			Intent.ItemId = Entry.ItemId;
			Intent.ExpectedEntryId = Entry.EntryId;
			Intent.ExpectedSourcePlacement = Entry.Placement;
			Intent.ExpectedQuantity = Entry.StackCount;
			Intent.TargetPlacement = SocketPlacement;
			TestTrue(TEXT("The kept rune goes into a socket"), Player->MoveItem(Intent).IsSuccess());
		}
	}
	TestEqual(TEXT("A socketed rune cannot be salvaged"), Station->SalvageRune(Requester, Kept->GetItemId()), ERpgRuneWorkResult::Socketed);
	TestTrue(TEXT("The socketed rune stays"), Player->ContainsItemInstance(Kept));
	Chest->GetInventoryManager()->AddItemDefinition(Material, 2);
	TestEqual(TEXT("A socketed rune can be upgraded"), Station->UpgradeRune(Requester, Kept->GetItemId()), ERpgRuneWorkResult::Done);
	TestEqual(TEXT("The socketed rune reaches rank II in place"), Kept->GetRuneRank(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingRuneSalvageFallbackTest,
	"SurvivalRpg.Crafting.RuneWork.SalvageFallsBackToThePlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingRuneSalvageFallbackTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	FBoolProperty* OffersRuneWork = FindFProperty<FBoolProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("bOffersRuneWork"));
	if (!TestNotNull(TEXT("The station has a rune work switch"), OffersRuneWork)) { return false; }
	OffersRuneWork->SetPropertyValue_InContainer(Station, true);
	const TSubclassOf<URpgInventoryItemDefinition> Essence = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	FRpgCraftingOutputItem Output;
	Output.ItemDefinition = Essence;
	Output.Count = 2;
	URpgRuneAutomationTestRuneDefinition::SetRankWork(1, {}, {Output});
	ON_SCOPE_EXIT
	{
		URpgRuneAutomationTestRuneDefinition::SetRankWork(1, {}, {});
	};

	URpgInventoryManagerComponent* Player = Fixture.CreatePlayerInventory();
	FRpgInventoryGridSize PlayerGrid;
	PlayerGrid.Width = 2;
	PlayerGrid.Height = 1;
	Player->SetDefaultGridSize(PlayerGrid);
	URpgInventoryItemInstance* Rune = Player->AddItemDefinition(URpgRuneAutomationTestRuneDefinition::StaticClass(), 1);
	ARpgInventoryContainerActor* Tiny = Fixture.CreateChest(1, 1);
	if (!TestNotNull(TEXT("The rune exists"), Rune) || !Tiny ||
		!Tiny->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1)) { return false; }

	TestEqual(TEXT("Without a chest with room the salvage still succeeds"), Station->SalvageRune(Requester, Rune->GetItemId()), ERpgRuneWorkResult::Done);
	TestFalse(TEXT("The rune is gone"), Player->ContainsItemInstance(Rune));
	TestEqual(TEXT("The outputs land in the player's inventory"), Player->GetTotalItemCountByDefinition(Essence), 2);
	TestEqual(TEXT("The full chest is untouched"), CountOf(Tiny, Essence), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingStationScreenTagTest,
	"SurvivalRpg.Crafting.Station.ScreenTagDefaultsToCrafting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStationScreenTagTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	TestTrue(TEXT("A station without a screen tag opens the crafting screen"), Station->GetScreenTag() == RpgGameplayTags::UI_Screen_Crafting);
	FStructProperty* ScreenTag = FindFProperty<FStructProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("ScreenTag"));
	if (!TestNotNull(TEXT("The station names its screen"), ScreenTag)) { return false; }
	*ScreenTag->ContainerPtrToValuePtr<FGameplayTag>(Station) = RpgGameplayTags::UI_Screen_Loot;
	TestTrue(TEXT("A station with a screen tag opens that screen"), Station->GetScreenTag() == RpgGameplayTags::UI_Screen_Loot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingRuneShrineReadModelTest,
	"SurvivalRpg.Crafting.RuneWork.ShrineReadModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingRuneShrineReadModelTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	FBoolProperty* OffersRuneWork = FindFProperty<FBoolProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("bOffersRuneWork"));
	if (!TestNotNull(TEXT("The station has a rune work switch"), OffersRuneWork)) { return false; }
	OffersRuneWork->SetPropertyValue_InContainer(Station, true);

	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Essence = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> RuneDefinition = URpgRuneAutomationTestRuneDefinition::StaticClass();
	FRpgCraftingResourceCost Cost;
	Cost.ItemDefinition = Material;
	Cost.Count = 2;
	FRpgCraftingOutputItem Output;
	Output.ItemDefinition = Essence;
	Output.Count = 4;
	URpgRuneAutomationTestRuneDefinition::SetRankWork(1, {}, {Output});
	URpgRuneAutomationTestRuneDefinition::SetRankWork(2, {Cost}, {});
	URpgRuneAutomationTestRuneDefinition::SetRankDescription(1, INVTEXT("Rank one burns"));
	URpgRuneAutomationTestRuneDefinition::SetRankDescription(2, INVTEXT("Rank two burns longer"));
	ON_SCOPE_EXIT
	{
		for (int32 Rank = 1; Rank <= 3; ++Rank)
		{
			URpgRuneAutomationTestRuneDefinition::SetRankWork(Rank, {}, {});
			URpgRuneAutomationTestRuneDefinition::SetRankDescription(Rank, FText::GetEmpty());
		}
	};
	auto ReadInt = [](const UObject* Object, const TCHAR* Name)
	{
		const FIntProperty* Property = Object ? FindFProperty<FIntProperty>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Object) : INDEX_NONE;
	};

	URpgInventoryManagerComponent* Player = Fixture.CreatePlayerInventory();
	URpgInventoryItemInstance* First = Player->AddItemDefinition(RuneDefinition, 1);
	URpgInventoryItemInstance* Second = Player->AddItemDefinition(RuneDefinition, 1);
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest(4, 4);
	if (!TestNotNull(TEXT("The first rune exists"), First) || !TestNotNull(TEXT("The second rune exists"), Second) ||
		!Chest || !Chest->GetInventoryManager()->AddItemDefinition(Material, 1)) { return false; }

	TStrongObjectPtr<URpgCraftingStationViewModel> ViewModel(NewObject<URpgCraftingStationViewModel>());
	ViewModel->BindCraftingStation(Station, Requester);
	ViewModel->Refresh();
	ON_SCOPE_EXIT
	{
		ViewModel->UnbindCraftingStation();
	};

	const FString RuneName = GetDefault<URpgInventoryItemDefinition>(RuneDefinition)->DisplayName.ToString();
	TestTrue(TEXT("The read model knows the station offers rune work"), ViewModel->OffersRuneWork());
	TestEqual(TEXT("Both carried runes are listed"), ViewModel->GetRuneRows().Num(), 2);
	TestTrue(TEXT("The first rune is selected"), ViewModel->GetSelectedRuneItemId() == First->GetItemId());
	TestEqual(TEXT("The title names the rune and its rank"), ViewModel->GetSelectedRuneTitle().ToString(), RuneName + TEXT(" I"));
	TestEqual(TEXT("A rune in the pockets is carried"), ViewModel->GetSelectedRuneLocation().ToString(), FString(TEXT("Carried")));
	TestEqual(TEXT("The current rank is described"), ViewModel->GetSelectedRuneRankDescription().ToString(), FString(TEXT("Rank one burns")));
	TestEqual(TEXT("The preview names the next rank"), ViewModel->GetSelectedRuneNextRankTitle().ToString(), FString(TEXT("Next: Rank II")));
	TestEqual(TEXT("The preview describes the next rank"), ViewModel->GetSelectedRuneNextRankDescription().ToString(), FString(TEXT("Rank two burns longer")));
	const TArray<URpgCraftingIngredientViewModel*> Costs = ViewModel->GetRuneUpgradeCosts();
	if (TestEqual(TEXT("The next rank's cost is one material row"), Costs.Num(), 1))
	{
		TestEqual(TEXT("The row needs the next rank's count"), ReadInt(Costs[0], TEXT("RequiredCount")), 2);
		TestEqual(TEXT("The row counts the connected chests"), ReadInt(Costs[0], TEXT("AvailableCount")), 1);
	}
	TestFalse(TEXT("Without materials the upgrade is not offered"), ViewModel->CanUpgradeSelectedRune());
	TestEqual(TEXT("The status explains the missing materials"), ViewModel->GetRuneUpgradeStatusText().ToString(), FString(TEXT("The connected chests lack materials.")));
	TestTrue(TEXT("A carried rune can be salvaged"), ViewModel->CanSalvageSelectedRune());
	TestEqual(TEXT("The salvage line names the outputs"), ViewModel->GetRuneSalvageText().ToString(),
		FString::Printf(TEXT("Returns 4 %s"), *GetDefault<URpgInventoryItemDefinition>(Essence)->DisplayName.ToString()));

	// The selection follows the chosen rune through an upgrade.
	ViewModel->SelectRune(Second->GetItemId());
	TestTrue(TEXT("The second rune is selected"), ViewModel->GetSelectedRuneItemId() == Second->GetItemId());
	Chest->GetInventoryManager()->AddItemDefinition(Material, 1);
	ViewModel->Refresh();
	TestTrue(TEXT("With materials the upgrade is offered"), ViewModel->CanUpgradeSelectedRune());
	TestTrue(TEXT("Nothing blocks the upgrade"), ViewModel->GetRuneUpgradeStatusText().IsEmpty());
	TestEqual(TEXT("The second rune is upgraded"), Station->UpgradeRune(Requester, Second->GetItemId()), ERpgRuneWorkResult::Done);
	ViewModel->Refresh();
	TestTrue(TEXT("The upgraded rune stays selected"), ViewModel->GetSelectedRuneItemId() == Second->GetItemId());
	TestEqual(TEXT("The title shows the new rank"), ViewModel->GetSelectedRuneTitle().ToString(), RuneName + TEXT(" II"));
	TestEqual(TEXT("The preview moves to rank III"), ViewModel->GetSelectedRuneNextRankTitle().ToString(), FString(TEXT("Next: Rank III")));
	TestEqual(TEXT("The first rune keeps rank I in its row"), ViewModel->GetRuneRows()[0]->GetRank(), 1);

	// A socketed rune names its tool and explains why it cannot be salvaged.
	URpgInventoryItemInstance* Tool = Player->AddItemDefinition(URpgRuneAutomationTestToolDefinition::StaticClass(), 1);
	FRpgInventoryGridPlacement ToolPlacement;
	if (!Tool || !Player->GetItemPlacement(Tool, ToolPlacement)) { return false; }
	FRpgInventoryGridPlacement SocketPlacement;
	SocketPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(
		Tool->GetItemId(), RpgRuneAutomationTestSocketGridId, ToolPlacement.GetContainerHandle().GetDirectChildDepth()));
	SocketPlacement.X = 0;
	SocketPlacement.Y = 0;
	for (const FRpgInventoryEntryView& Entry : Player->GetAllEntries())
	{
		if (Entry.ItemId == First->GetItemId())
		{
			FRpgInventoryMoveIntent Intent;
			Intent.EnsureRequestId();
			Intent.ItemId = Entry.ItemId;
			Intent.ExpectedEntryId = Entry.EntryId;
			Intent.ExpectedSourcePlacement = Entry.Placement;
			Intent.ExpectedQuantity = Entry.StackCount;
			Intent.TargetPlacement = SocketPlacement;
			TestTrue(TEXT("The first rune goes into a socket"), Player->MoveItem(Intent).IsSuccess());
		}
	}
	ViewModel->Refresh();
	ViewModel->SelectRune(First->GetItemId());
	const FString ToolName = Tool->GetItemDef() ? GetDefault<URpgInventoryItemDefinition>(Tool->GetItemDef())->DisplayName.ToString() : FString();
	TestEqual(TEXT("A socketed rune names its tool"), ViewModel->GetSelectedRuneLocation().ToString(), TEXT("Socketed in ") + ToolName);
	TestFalse(TEXT("A socketed rune cannot be salvaged"), ViewModel->CanSalvageSelectedRune());
	TestEqual(TEXT("The salvage line explains the socket"), ViewModel->GetRuneSalvageText().ToString(),
		FString::Printf(TEXT("Take it out of %s to salvage it."), *ToolName));

	// Salvaging the selected rune moves the selection to the remaining one.
	ViewModel->SelectRune(Second->GetItemId());
	TestEqual(TEXT("The second rune is salvaged"), Station->SalvageRune(Requester, Second->GetItemId()), ERpgRuneWorkResult::Done);
	ViewModel->Refresh();
	TestEqual(TEXT("One rune is left"), ViewModel->GetRuneRows().Num(), 1);
	TestTrue(TEXT("The selection moves to the remaining rune"), ViewModel->GetSelectedRuneItemId() == First->GetItemId());

	// A station without rune work lists no runes.
	OffersRuneWork->SetPropertyValue_InContainer(Station, false);
	ViewModel->Refresh();
	TestFalse(TEXT("The read model sees the switch"), ViewModel->OffersRuneWork());
	TestEqual(TEXT("No runes are listed"), ViewModel->GetRuneRows().Num(), 0);
	TestFalse(TEXT("Nothing is selected"), ViewModel->GetSelectedRuneItemId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingWaitsForSpaceTest,
	"SurvivalRpg.Crafting.Order.WaitsForSpaceBeforeConsuming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingWaitsForSpaceTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest(1, 1);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Filler = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 6) ||
		!Target->GetInventoryManager()->AddItemDefinition(Filler, 1) || !Fixture.OfferRecipes(*this, { Recipe })) { return false; }

	TestFalse(TEXT("An order cannot start while its first unit cannot be delivered"), Station->StartCraftingOrder(Requester, Recipe, 3, ChestId(Target)));
	TestEqual(TEXT("A rejected start consumes nothing"), CountOf(Source, Material), 6);
	if (!RemoveFirst(Target, Filler)) { return false; }
	TestTrue(TEXT("The order starts once the target has room"), Station->StartCraftingOrder(Requester, Recipe, 3, ChestId(Target)));
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The first unit fills the one-cell target"), CountOf(Target, Product), 1);
	TestEqual(TEXT("The order waits for space"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForSpace);
	TestEqual(TEXT("No material is consumed while the target is full"), CountOf(Source, Material), 4);
	TestFalse(TEXT("The waiting unit is not paid"), Station->GetCurrentOrder().bUnitPaid);
	if (!RemoveFirst(Target, Product)) { return false; }
	AdvanceCraftingTimers(World, 1.1f);
	TestEqual(TEXT("A retry pays the next unit once space frees up"), CountOf(Source, Material), 2);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The next unit is delivered"), CountOf(Target, Product), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingCompletedUnitWaitsTest,
	"SurvivalRpg.Crafting.Order.CompletedUnitWaitsAndKeepsCredits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingCompletedUnitWaitsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest(1, 1);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Filler = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 4) || !Fixture.OfferRecipes(*this, { Recipe }) ||
		!Station->StartCraftingOrder(Fixture.GetRequestingController(), Recipe, 2, ChestId(Target))) { return false; }

	// Someone fills the target while the paid unit is produced.
	Target->GetInventoryManager()->AddItemDefinition(Filler, 1);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The produced unit waits for space"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForSpace);
	TestTrue(TEXT("The produced unit stays paid"), Station->GetCurrentOrder().bUnitPaid);
	TestEqual(TEXT("Its credits are kept"), CountUnitCredits(Station, Material), 2);
	TestEqual(TEXT("A waiting order saves its paid unit"), Station->ExportCraftingState().Order.UnitCredits.Num(), 1);
	if (!RemoveFirst(Target, Filler)) { return false; }
	AdvanceCraftingTimers(World, 1.1f);
	TestEqual(TEXT("The retry delivers the produced unit"), CountOf(Target, Product), 1);
	TestEqual(TEXT("Delivery spends the produced unit's credits"), CountUnitCredits(Station, Material), 0);
	TestEqual(TEXT("The refilled one-cell target holds the next unit back"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForSpace);
	TestEqual(TEXT("No material is consumed for the held-back unit"), CountOf(Source, Material), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingTargetChangeTest,
	"SurvivalRpg.Crafting.Order.TargetChangeAndMissingTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingTargetChangeTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Full = Fixture.CreateChest(1, 1);
	ARpgInventoryContainerActor* Spare = Fixture.CreateChest(2, 2);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!Source || !Full || !Spare || !Source->GetInventoryManager()->AddItemDefinition(Material, 4) ||
		!Full->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestStackItemDefinition::StaticClass(), 1) ||
		!Fixture.OfferRecipes(*this, { Recipe }) ||
		!Station->StartCraftingOrder(Requester, Recipe, 2, ChestId(Spare))) { return false; }
	const FGuid OrderId = Station->GetCurrentOrder().OrderId;

	TestFalse(TEXT("An unknown target is rejected"), Station->SetCraftingOrderTarget(Requester, OrderId, TEXT("Chest_Unknown")));
	TestTrue(TEXT("The target may change while an order runs"), Station->SetCraftingOrderTarget(Requester, OrderId, ChestId(Full)));
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The full new target makes the order wait"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForSpace);
	TestTrue(TEXT("Switching back retries at once"), Station->SetCraftingOrderTarget(Requester, OrderId, ChestId(Spare)));
	TestEqual(TEXT("The waiting unit lands in the new target without another tick"), CountOf(Spare, Product), 1);
	TestTrue(TEXT("The next unit starts"), Station->GetCurrentOrder().bUnitPaid);
	Spare->Destroy();
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("A destroyed target makes the order wait for one"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForTarget);
	TestTrue(TEXT("The produced unit keeps its credits"), Station->GetCurrentOrder().bUnitPaid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingPauseFreezesUnitTest,
	"SurvivalRpg.Crafting.Order.PauseFreezesUnit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingPauseFreezesUnitTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 0.1f;
	if (!Target || !Fixture.OfferRecipes(*this, { Recipe })) { return false; }

	TestFalse(TEXT("An idle station cannot pause"), Station->PauseCraftingStation(Requester));
	if (!Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Target))) { return false; }
	TestTrue(TEXT("A running order pauses"), Station->PauseCraftingStation(Requester));
	AdvanceCraftingTimers(World, 1.0f);
	TestEqual(TEXT("A paused unit does not complete"), CountOf(Target, URpgInventoryAutomationTestUnitItemDefinition::StaticClass()), 0);
	TestTrue(TEXT("The paused remaining time is saved"), Station->ExportCraftingState().Order.RemainingTime > 0.05f);
	TestTrue(TEXT("A paused order resumes"), Station->ResumeCraftingStation(Requester));
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The resumed unit completes"), CountOf(Target, URpgInventoryAutomationTestUnitItemDefinition::StaticClass()), 1);
	TestFalse(TEXT("The order ends"), Station->HasCraftingOrder());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingStopRefundTest,
	"SurvivalRpg.Crafting.Order.StopRefundOrFinishPiece",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStopRefundTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest(1, 1);
	ARpgInventoryContainerActor* Target = Fixture.CreateChest(1, 1);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Stack = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 30.0f);
	Recipe->OutputItems[0].ItemDefinition = Stack;
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 4) || !Fixture.OfferRecipes(*this, { Recipe }) ||
		!Station->StartCraftingOrder(Requester, Recipe, 2, ChestId(Target))) { return false; }

	FGuid OrderId = Station->GetCurrentOrder().OrderId;
	TestEqual(TEXT("The first unit was paid"), CountOf(Source, Material), 2);
	TestTrue(TEXT("Stop is accepted"), Station->StopCraftingOrder(Requester, OrderId));
	TestEqual(TEXT("The paid unit's materials return to their chest"), CountOf(Source, Material), 4);
	TestFalse(TEXT("The order ends"), Station->HasCraftingOrder());
	TestFalse(TEXT("A repeated stop finds no order"), Station->StopCraftingOrder(Requester, OrderId));

	// Fill every chest so the paid unit's materials fit nowhere; the output still merges into the target's stack.
	if (!Station->StartCraftingOrder(Requester, Recipe, 2, ChestId(Target))) { return false; }
	OrderId = Station->GetCurrentOrder().OrderId;
	if (!RemoveFirst(Source, Material) || !Source->GetInventoryManager()->AddItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1) ||
		!Target->GetInventoryManager()->AddItemDefinition(Stack, 1)) { return false; }
	TestTrue(TEXT("Stop is still accepted"), Station->StopCraftingOrder(Requester, OrderId));
	TestTrue(TEXT("Without room for a refund the order keeps its paid unit"), Station->HasCraftingOrder());
	TestEqual(TEXT("It finishes that unit, then ends"), Station->GetCurrentOrder().QuantityTotal, 1);
	AdvanceCraftingTimers(World, 31.0f);
	TestEqual(TEXT("The last unit is delivered"), CountOf(Target, Stack), 2);
	TestFalse(TEXT("The order ends after it"), Station->HasCraftingOrder());
	TestEqual(TEXT("No refund falls onto the ground"), CountDroppedOutputActors(World), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingSaveRestoreUnitCreditsTest,
	"SurvivalRpg.Crafting.Order.SaveRestoreUnitCredits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingSaveRestoreUnitCreditsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 6) || !Fixture.OfferRecipes(*this, { Recipe }) ||
		!Station->StartCraftingOrder(Fixture.GetRequestingController(), Recipe, 3, ChestId(Target))) { return false; }

	FRpgCraftingStationSaveData Save = Station->ExportCraftingState();
	if (!TestTrue(TEXT("The order is saved"), Save.bHasOrder) || !TestEqual(TEXT("One credit row is saved"), Save.Order.UnitCredits.Num(), 1)) { return false; }
	TestTrue(TEXT("The paid unit is saved"), Save.Order.bUnitPaid);
	TestEqual(TEXT("Credits equal one unit's cost"), Save.Order.UnitCredits[0].Count, 2);
	TestFalse(TEXT("The credit's chest identity is durable"), Save.Order.UnitCredits[0].InventoryId.IsNone());
	TestEqual(TEXT("The target is saved"), Save.Order.TargetContainerId, ChestId(Target));

	FRpgCraftingStationSaveData Corrupt = Save;
	Corrupt.Order.UnitCredits[0].Count = 3;
	TestFalse(TEXT("Credits above one unit's cost are rejected"), Station->RestoreCraftingState(Corrupt));
	Corrupt = Save;
	Corrupt.Order.bUnitPaid = false;
	TestFalse(TEXT("Credits on an unpaid unit are rejected"), Station->RestoreCraftingState(Corrupt));

	Save.Order.RemainingTime = 0.05f;
	TestTrue(TEXT("The paid unit restores"), Station->RestoreCraftingState(Save));
	AdvanceCraftingTimers(World, 10.0f);
	TestEqual(TEXT("A restored order cannot run before the world restore completes"), CountOf(Target, Product), 0);
	TestEqual(TEXT("Its credits survive"), CountUnitCredits(Station, Material), 2);
	Station->ResumeRestoredCrafting();
	AdvanceCraftingTimers(World, 0.025f);
	TestEqual(TEXT("Resume keeps the saved remaining time"), CountOf(Target, Product), 0);
	AdvanceCraftingTimers(World, 0.03f);
	TestEqual(TEXT("The restored unit completes once"), CountOf(Target, Product), 1);
	AdvanceCraftingTimers(World, 0.2f);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("The whole order produces exactly three products"), CountOf(Target, Product), 3);
	TestEqual(TEXT("Restore never charges a unit twice"), CountOf(Source, Material), 0);

	FRpgCraftingStationSaveData Idle;
	Idle.StationId = Station->GetPersistentStationId();
	TestTrue(TEXT("An idle station restores"), Station->RestoreCraftingState(Idle));
	TestFalse(TEXT("It has no order"), Station->HasCraftingOrder());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingItemizedOutputsTest,
	"SurvivalRpg.Crafting.Order.ItemizedOutputsRollPerPiece",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingItemizedOutputsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest(4, 4);
	const TSubclassOf<URpgInventoryItemDefinition> Itemized = URpgItemizationAutomationTestItemDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->OutputItems[0].ItemDefinition = Itemized;
	Recipe->OutputItems[0].Count = 3;
	Recipe->OutputItemLevel = 10;
	if (!Target || !Fixture.OfferRecipes(*this, { Recipe }) ||
		!Station->StartCraftingOrder(Fixture.GetRequestingController(), Recipe, 1, ChestId(Target))) { return false; }
	AdvanceCraftingTimers(Station->GetWorld(), 0.1f);

	const URpgItemizationProfile* Profile = GetDefault<URpgItemizationAutomationTestProfile>();
	TArray<FRpgItemStatRange> Ranges;
	if (!TestTrue(TEXT("The profile reports its base-stat ranges"), Profile->GetBaseStatRanges(10, Ranges))) { return false; }
	int32 Pieces = 0;
	for (const FRpgInventoryEntryView& Entry : Target->GetInventoryManager()->GetAllEntries())
	{
		if (!Entry.Instance || Entry.Instance->GetItemDef() != Itemized) { continue; }
		++Pieces;
		TestEqual(TEXT("Every piece is its own entry"), Entry.StackCount, 1);
		const FRpgItemizationState& State = Entry.Instance->GetItemizationStateRef();
		TestTrue(TEXT("Every piece is rolled"), State.bGenerated);
		TestEqual(TEXT("Every base stat is rolled"), State.BaseStats.Num(), Ranges.Num());
		for (const FRpgItemStatRange& Range : Ranges)
		{
			const float Value = State.GetBaseValueForStat(Range.StatTag);
			TestTrue(TEXT("Rolled base stats stay inside the previewed range"), Value >= Range.MinValue - KINDA_SMALL_NUMBER && Value <= Range.MaxValue + KINDA_SMALL_NUMBER);
		}
	}
	TestEqual(TEXT("All three pieces are delivered"), Pieces, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingSpatialDomainTest,
	"SurvivalRpg.Crafting.Order.BaseOverridesStationRadius",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingSpatialDomainTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	ARpgBaseCampActor* Base = Station->GetWorld()->SpawnActor<ARpgBaseCampActor>();
	if (!TestNotNull(TEXT("Area fixture exists"), Base) || !TestTrue(TEXT("Valid base area is accepted"), Base->SetBaseArea(FVector::ZeroVector, 5000.0f))) { return false; }
	FFloatProperty* Radius = FindFProperty<FFloatProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("StorageSearchRadius"));
	if (!TestNotNull(TEXT("Outside radius tuning exists"), Radius)) { return false; }
	Radius->SetPropertyValue_InContainer(Station, 150.0f);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	auto MakeStock = [&](FVector Position, int32 Count)
	{
		ARpgInventoryContainerActor* Chest = Fixture.CreateChest(0, 0, Position);
		return Chest && Chest->GetInventoryManager()->AddItemDefinition(Material, Count) != nullptr;
	};
	if (!MakeStock(FVector(4000, 0, 0), 7) || !MakeStock(FVector(4990, 0, 0), 11) ||
		!MakeStock(FVector(0, 0, 10000), 13) || !MakeStock(FVector(5100, 0, 0), 3)) { AddError(TEXT("Could not seed spatial source fixtures")); return false; }
	TestEqual(TEXT("Inside uses the full horizontal base including other floors"), Station->GetAvailableResourceCount(Material), 31);
	Station->GetOwner()->SetActorLocation(FVector(5100, 0, 0));
	TestEqual(TEXT("Outside radius reaches individual nearby base chests without inheriting their base"), Station->GetAvailableResourceCount(Material), 14);
	Radius->SetPropertyValue_InContainer(Station, 0.0f);
	TestEqual(TEXT("Zero outside radius never means unlimited access"), Station->GetAvailableResourceCount(Material), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageReadModelLifecycleTest,
	"SurvivalRpg.Inventory.PhysicalStorage.ReadModelLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageReadModelLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	ARpgInventoryContainerActor* First = Fixture.CreateChest();
	ARpgInventoryContainerActor* Second = Fixture.CreateChest();
	if (!TestNotNull(TEXT("First view-model source exists"), First) || !TestNotNull(TEXT("Second view-model source exists"), Second)) { return false; }
	URpgPhysicalStorageViewModel* Model = NewObject<URpgPhysicalStorageViewModel>();
	FRpgStorageAssignment Rule;
	Rule.ItemDefinition = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Model->BindContainer(First->GetContainerComponent());
	TestTrue(TEXT("A physical chest enables its metadata projection"), Model->IsPhysicalStorage());
	First->GetContainerComponent()->SetAssignments({ Rule });
	TestEqual(TEXT("Server-confirmed settings immediately refresh the read model"), Model->GetAssignments().Num(), 1);
	TestEqual(TEXT("The read model carries the observed settings revision"), Model->GetSettingsRevision(), First->GetContainerComponent()->GetSettingsRevision());
	Model->BindContainer(Second->GetContainerComponent());
	First->GetContainerComponent()->SetAssignments({});
	First->GetContainerComponent()->SetAssignments({ Rule });
	TestEqual(TEXT("Changes on the released source cannot leak into the new context"), Model->GetAssignments().Num(), 0);
	Second->GetContainerComponent()->SetAssignments({ Rule });
	TestEqual(TEXT("New source metadata remains observed"), Model->GetAssignments().Num(), 1);
	Model->SetCommandPending(true);
	Model->UnbindContainer();
	TestFalse(TEXT("Releasing presentation clears pending state"), Model->IsCommandPending());
	TestFalse(TEXT("Unbound projection exposes no gameplay target"), Model->IsPhysicalStorage());
	Second->GetContainerComponent()->SetAssignments({});
	Second->GetContainerComponent()->SetAssignments({ Rule });
	TestEqual(TEXT("Released projection remains empty despite later source changes"), Model->GetAssignments().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingStationReentrancyTest,
	"SurvivalRpg.Crafting.Order.SynchronousMutationGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStationReentrancyTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 0.1f;
	Recipe->OutputItems[0].ItemDefinition = URpgCraftingAutomationReentrantProduct::StaticClass();
	URpgCraftingRecipeDefinition* FreeRecipe = Fixture.CreateRecipe();
	if (!Target || !Fixture.OfferRecipes(*this, { Recipe, FreeRecipe }) ||
		!TestTrue(TEXT("Fixture product order starts"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Target)))) { return false; }
	const FRpgCraftingStationSaveData Snapshot = Station->ExportCraftingState();
	const FGuid OrderId = Snapshot.Order.OrderId;
	int32 FragmentCalls = 0;
	int32 ObserverCalls = 0;
	bool bRejectedAllCommands = true;
	auto AttemptNestedCommands = [&]()
	{
		bRejectedAllCommands &= !Station->StartCraftingOrder(Requester, FreeRecipe, 1, ChestId(Target));
		bRejectedAllCommands &= !Station->StopCraftingOrder(Requester, OrderId);
		bRejectedAllCommands &= !Station->PauseCraftingStation(Requester);
		bRejectedAllCommands &= !Station->ResumeCraftingStation(Requester);
		bRejectedAllCommands &= !Station->RestoreCraftingState(Snapshot);
		bRejectedAllCommands &= !Station->SetCraftingOrderTarget(Requester, OrderId, ChestId(Target));
		Station->ResumeRestoredCrafting();
	};
	URpgCraftingAutomationReentrantFragment::OnCreate = [&]()
	{
		++FragmentCalls;
		AttemptNestedCommands();
	};
	const FDelegateHandle Handle = Target->GetInventoryManager()->OnInventoryPostCommit.AddLambda([&](URpgInventoryManagerComponent*)
	{
		++ObserverCalls;
		AttemptNestedCommands();
	});
	AdvanceCraftingTimers(Station->GetWorld(), 0.2f);
	URpgCraftingAutomationReentrantFragment::OnCreate = {};
	Target->GetInventoryManager()->OnInventoryPostCommit.Remove(Handle);
	TestTrue(TEXT("Output preparation exercised the fragment callback"), FragmentCalls > 0);
	TestTrue(TEXT("Output publication exercised the synchronous inventory observer"), ObserverCalls > 0);
	TestTrue(TEXT("Start, stop, pause, restore and target changes reject nested mutation"), bRejectedAllCommands);
	TestFalse(TEXT("The original order completes exactly once"), Station->HasCraftingOrder());
	TestEqual(TEXT("Only the original output is committed"), CountOf(Target, URpgCraftingAutomationReentrantProduct::StaticClass()), 1);

	FIntProperty* StateRevision = FindFProperty<FIntProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("CraftingStateRevision"));
	if (!TestNotNull(TEXT("Order revision contract exists"), StateRevision)) { return false; }
	TestTrue(TEXT("A second order starts after the guard releases"), Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Target)));
	URpgCraftingAutomationReentrantFragment::OnCreate = [&]()
	{
		// Simulate a future internal mutation bypassing command entrypoints: final context validation must still reject it.
		StateRevision->SetPropertyValue_InContainer(Station, StateRevision->GetPropertyValue_InContainer(Station) + 1);
	};
	AdvanceCraftingTimers(Station->GetWorld(), 0.2f);
	URpgCraftingAutomationReentrantFragment::OnCreate = {};
	TestEqual(TEXT("Changed station revision rejects the staged output"), CountOf(Target, URpgCraftingAutomationReentrantProduct::StaticClass()), 1);
	TestTrue(TEXT("The rejected unit stays paid"), Station->HasCraftingOrder() && Station->GetCurrentOrder().bUnitPaid);
	TestEqual(TEXT("The rejected unit waits for a safe retry"), Station->GetCurrentOrder().State, ERpgCraftingOrderState::WaitingForSpace);
	TestTrue(TEXT("The waiting order can be stopped"), Station->StopCraftingOrder(Requester, Station->GetCurrentOrder().OrderId));
	TestTrue(TEXT("The guard releases after the outer operation"), Station->StartCraftingOrder(Requester, FreeRecipe, 1, ChestId(Target)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingCompetingOrdersConservationTest,
	"SurvivalRpg.Crafting.Order.CompetingOrdersConserveMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingCompetingOrdersConservationTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* First = Fixture.GetStation();
	UWorld* World = First->GetWorld();
	ARpgCraftingStationActor* OtherActor = World->SpawnActor<ARpgCraftingStationActor>();
	ARpgInventoryAutomationTestPlayerController* OtherPlayer = World->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	if (!OtherActor || !OtherPlayer || !Source || !Target) { return false; }
	URpgCraftingStationComponent* Second = OtherActor->GetCraftingStationComponent();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	if (!Source->GetInventoryManager()->AddItemDefinition(Material, 6)) { return false; }
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(3, 0.1f);
	if (!Fixture.OfferRecipes(*this, { Recipe }) || !Fixture.OfferRecipes(*this, { Recipe }, Second)) { return false; }
	AActor* Requester = Fixture.GetRequestingController();
	TestTrue(TEXT("The first station starts and pays one unit"), First->StartCraftingOrder(Requester, Recipe, 2, ChestId(Target)));
	TestFalse(TEXT("Another player cannot start a second order at the same station"), First->StartCraftingOrder(OtherPlayer, Recipe, 1, ChestId(Target)));
	TestTrue(TEXT("The second station starts and pays the remaining unit"), Second->StartCraftingOrder(OtherPlayer, Recipe, 2, ChestId(Target)));
	TestEqual(TEXT("Live stock plus both paid units equals the initial stock"),
		CountOf(Source, Material) + CountUnitCredits(First, Material) + CountUnitCredits(Second, Material), 6);
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("Each station delivers its paid unit"), CountOf(Target, Product), 2);
	TestEqual(TEXT("Both stations wait for materials"),
		First->GetCurrentOrder().State == ERpgCraftingOrderState::WaitingForMaterials && Second->GetCurrentOrder().State == ERpgCraftingOrderState::WaitingForMaterials, true);
	TestEqual(TEXT("Products account for every consumed input"),
		CountOf(Source, Material) + CountUnitCredits(First, Material) + CountUnitCredits(Second, Material) + 3 * CountOf(Target, Product), 6);
	TestTrue(TEXT("Either player may stop the shared first order"), First->StopCraftingOrder(OtherPlayer, First->GetCurrentOrder().OrderId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingCommitRetryTest,
	"SurvivalRpg.Crafting.Order.CommitRetriesAfterConcurrentChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingCommitRetryTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgCraftingAutomationReentrantProduct::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	Recipe->OutputItems[0].ItemDefinition = Product;
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 6) || !Fixture.OfferRecipes(*this, { Recipe }) ||
		!Station->StartCraftingOrder(Fixture.GetRequestingController(), Recipe, 1, ChestId(Target))) { return false; }

	bool bChanged = false, bExpanded = false;
	URpgCraftingAutomationReentrantFragment::OnCreate = [&]()
	{
		if (bChanged) { return; }
		bChanged = true;
		// The source chest grows and moves away while the station stages its finished output.
		FRpgInventoryGridSize Larger = Source->GetInventoryManager()->GetDefaultGridSize();
		++Larger.Width;
		bExpanded = Source->GetInventoryManager()->SetDefaultGridSize(Larger);
		Source->SetActorLocation(FVector(10000.0f, 0.0f, 0.0f));
	};
	AdvanceCraftingTimers(World, 0.2f);
	URpgCraftingAutomationReentrantFragment::OnCreate = {};
	TestTrue(TEXT("Capacity update and source move occurred during staging"), bChanged && bExpanded);
	TestEqual(TEXT("The stale commit is rejected"), CountOf(Target, Product), 0);
	TestTrue(TEXT("The paid unit survives"), Station->HasCraftingOrder() && Station->GetCurrentOrder().bUnitPaid);
	TestEqual(TEXT("Live stock and the paid unit still conserve six inputs"), CountOf(Source, Material) + CountUnitCredits(Station, Material), 6);
	AdvanceCraftingTimers(World, 1.1f);
	TestEqual(TEXT("The retry delivers once after the source moved away"), CountOf(Target, Product), 1);
	TestFalse(TEXT("The order ends"), Station->HasCraftingOrder());
	TestEqual(TEXT("The untouched source remainder survives the upgrade and move"), CountOf(Source, Material), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingDestroyedSourceRefundTest,
	"SurvivalRpg.Crafting.Order.DestroyedSourceRefundFallsBackToAreaChest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingDestroyedSourceRefundTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Source = Fixture.CreateChest(1, 1);
	ARpgInventoryContainerActor* Target = Fixture.CreateChest(2, 2);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(3, 30.0f);
	if (!Source || !Target || !Source->GetInventoryManager()->AddItemDefinition(Material, 3) || !Fixture.OfferRecipes(*this, { Recipe }) ||
		!Station->StartCraftingOrder(Requester, Recipe, 1, ChestId(Target))) { return false; }

	Source->Destroy();
	TestTrue(TEXT("Stop succeeds without the original chest"), Station->StopCraftingOrder(Requester, Station->GetCurrentOrder().OrderId));
	TestEqual(TEXT("The paid materials fall back to another connected chest"), CountOf(Target, Material), 3);
	TestFalse(TEXT("The order ends"), Station->HasCraftingOrder());
	TestEqual(TEXT("No world drop substitutes for a missing source"), CountDroppedOutputActors(Station->GetWorld()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingPendingSourceRefundTest,
	"SurvivalRpg.Crafting.Order.PendingSourceRefundUsesAreaChest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingPendingSourceRefundTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this, true)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	APlayerController* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* ReturningChest = Fixture.CreateChest();
	ARpgInventoryContainerActor* Target = Fixture.CreateChest();
	if (!ReturningChest || !Target) { return false; }
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 1.0f);
	ReturningChest->GetContainerComponent()->SetConstructionPending(true);

	URpgInventoryManagerComponent* Original = ReturningChest->GetInventoryManager();
	const FName SourceId = RpgStorageAccessRules::GetPersistentInventoryId(Original);
	if (!TestFalse(TEXT("The pending source has a durable identity"), SourceId.IsNone())) { return false; }
	TestNull(TEXT("Pending graph replacement excludes the provisional refund target"), RpgStorageAccessRules::FindPersistentInventory(Station->GetWorld(), SourceId));
	const FRpgInventoryGraphSaveData PendingGraph = Original->ExportInventoryGraph();
	FRpgCraftingStationSaveData Save;
	Save.StationId = Station->GetPersistentStationId();
	Save.bHasOrder = true;
	Save.Order.OrderId = FGuid::NewGuid();
	Save.Order.Recipe = Recipe;
	Save.Order.TargetContainerId = ChestId(Target);
	Save.Order.QuantityTotal = 1;
	Save.Order.State = static_cast<uint8>(ERpgCraftingOrderState::Running);
	Save.Order.bPaused = true;
	Save.Order.bUnitPaid = true;
	Save.Order.RemainingTime = 1.0f;
	FRpgCraftingRefundSaveData& Credit = Save.Order.UnitCredits.AddDefaulted_GetRef();
	Credit.ItemDefinition = Material; Credit.Count = 2; Credit.InventoryId = SourceId;
	if (!TestTrue(TEXT("A saved paid unit restores while its source is pending"), Station->RestoreCraftingState(Save))) { return false; }
	TestTrue(TEXT("Stop refunds into a connected chest while the source is provisional"), Station->StopCraftingOrder(Requester, Save.Order.OrderId));
	TestEqual(TEXT("The provisional source receives nothing its later restore could erase"), Original->GetTotalItemCountByDefinition(Material), 0);
	TestEqual(TEXT("The whole refund lands in the connected chest"), CountOf(Target, Material), 2);
	FRpgInventoryMutationResult Result;
	TestTrue(TEXT("The delayed source graph replacement completes"), Original->RestoreInventoryGraph(PendingGraph, Result));
	TestEqual(TEXT("Graph replacement cannot erase the refunded materials"), CountOf(Target, Material) + Original->GetTotalItemCountByDefinition(Material), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingStationChestTargetTest,
	"SurvivalRpg.Crafting.StationChest.DefaultTargetAndRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStationChestTargetTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	UWorld* World = Station->GetWorld();
	AActor* Requester = Fixture.GetRequestingController();
	FNameProperty* StationIdProperty = FindFProperty<FNameProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("PersistentStationId"));
	if (!TestNotNull(TEXT("The station id property exists"), StationIdProperty)) { return false; }
	StationIdProperty->SetPropertyValue_InContainer(Station, FName(TEXT("Station_Kiln")));
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	ARpgInventoryContainerActor* MaterialChest = Fixture.CreateChest();
	ARpgInventoryContainerActor* StationChest = Fixture.CreateChest(4, 4);
	ARpgInventoryContainerActor* FreeChest = Fixture.CreateChest(4, 4);
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateMaterialRecipe(2, 0.1f);
	if (!MaterialChest || !StationChest || !FreeChest || !MaterialChest->GetInventoryManager()->AddItemDefinition(Material, 20) ||
		!Fixture.OfferRecipes(*this, { Recipe })) { return false; }
	FRpgStorageAssignment MaterialRule;
	MaterialRule.ItemDefinition = Material;
	FRpgStorageAssignment ProductRule;
	ProductRule.ItemDefinition = Product;
	MaterialChest->GetContainerComponent()->SetAssignments({ MaterialRule });

	TestTrue(TEXT("A station without a station chest stores automatically by default"), Station->GetDefaultOutputTargetId().IsNone());
	TStrongObjectPtr<URpgPhysicalStorageViewModel> ChestModel(NewObject<URpgPhysicalStorageViewModel>());
	ChestModel->BindContainer(StationChest->GetContainerComponent());
	TestFalse(TEXT("The chest screen starts with an ordinary chest"), ChestModel->IsStationChest());
	StationChest->GetContainerComponent()->SetLinkedStationId(Station->GetPersistentStationId());
	TestEqual(TEXT("The station chest is the default target"), Station->GetDefaultOutputTargetId(), ChestId(StationChest));
	TestTrue(TEXT("The chest screen follows the replicated link"), ChestModel->IsStationChest());

	// Unassigned, the station chest stays out of automatic storing and deposits, even while it holds the product.
	if (!StationChest->GetInventoryManager()->AddItemDefinition(Product, 1) || !FreeChest->GetInventoryManager()->AddItemDefinition(Product, 1)) { return false; }
	TArray<URpgInventoryManagerComponent*> Targets = Station->GetOutputTargets(Recipe, NAME_None);
	TestTrue(TEXT("Automatic storing skips the unassigned station chest"),
		Targets.Num() == 1 && Targets[0] == FreeChest->GetInventoryManager());
	int64 Order = 0;
	TestEqual(TEXT("Deposits skip the unassigned station chest despite its stock"),
		StationChest->GetContainerComponent()->GetAssignmentRank(Product, Order), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("An ordinary chest with stock still takes deposits"), FreeChest->GetContainerComponent()->GetAssignmentRank(Product, Order), 2);
	StationChest->GetContainerComponent()->SetAssignments({ ProductRule });
	Targets = Station->GetOutputTargets(Recipe, NAME_None);
	TestTrue(TEXT("An assigned station chest takes automatic output first"),
		!Targets.IsEmpty() && Targets[0] == StationChest->GetInventoryManager());
	TestEqual(TEXT("An assigned station chest takes deposits"), StationChest->GetContainerComponent()->GetAssignmentRank(Product, Order), 0);
	StationChest->GetContainerComponent()->SetAssignments({});

	// An order started with the default target delivers into the station chest.
	if (!TestTrue(TEXT("An order starts with the default target"), Station->StartCraftingOrder(Requester, Recipe, 2, Station->GetDefaultOutputTargetId()))) { return false; }
	AdvanceCraftingTimers(World, 0.2f);
	AdvanceCraftingTimers(World, 0.2f);
	TestFalse(TEXT("The order completes"), Station->HasCraftingOrder());
	TestEqual(TEXT("Both units land in the station chest"), CountOf(StationChest, Product), 3);
	TestEqual(TEXT("The free chest receives nothing"), CountOf(FreeChest, Product), 1);

	// Another station sees the chest as an ordinary connected chest, never as its own default.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient;
	ARpgCraftingStationActor* OtherActor = World->SpawnActor<ARpgCraftingStationActor>(SpawnParameters);
	URpgCraftingStationComponent* Other = OtherActor ? OtherActor->GetCraftingStationComponent() : nullptr;
	if (!TestNotNull(TEXT("A second station exists"), Other)) { return false; }
	StationIdProperty->SetPropertyValue_InContainer(Other, FName(TEXT("Station_Forge")));
	if (!Fixture.OfferRecipes(*this, { Recipe }, Other)) { return false; }
	TestTrue(TEXT("Another station keeps automatic storing as its default"), Other->GetDefaultOutputTargetId().IsNone());
	TestFalse(TEXT("Another station's automatic storing skips the station chest"),
		Other->GetOutputTargets(Recipe, NAME_None).Contains(StationChest->GetInventoryManager()));
	TestTrue(TEXT("Another station may still pick the station chest explicitly"),
		Other->CanStartCraftingOrder(Requester, Recipe, 1, ChestId(StationChest)));

	StationChest->GetContainerComponent()->SetLinkedStationId(NAME_None);
	TestFalse(TEXT("Clearing the link makes an ordinary chest again"), ChestModel->IsStationChest());
	TestTrue(TEXT("Without its chest the station stores automatically"), Station->GetDefaultOutputTargetId().IsNone());
	ChestModel->UnbindContainer();
	return true;
}

#endif
