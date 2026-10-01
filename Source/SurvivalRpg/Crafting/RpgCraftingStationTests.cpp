#include "RpgCraftingRecipeDefinition.h"
#include "RpgCraftingStationActor.h"
#include "RpgCraftingStationComponent.h"
#include "RpgCraftingAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageTypes.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPhysicalStorageViewModel.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"
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

		ARpgInventoryContainerActor* CreateChest()
		{
			ARpgInventoryContainerActor* Chest = World->SpawnActor<ARpgInventoryContainerActor>();
			if (Chest) { Chest->GetContainerComponent()->EnsurePersistentContainerId(); }
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

	int32 CountReservedMaterial(const URpgCraftingStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> Material)
	{
		int32 Total = 0;
		for (const FRpgCraftingJobEntry& Job : Station->GetCraftingJobs())
		{
			for (const FRpgCraftingRefundEntry& Refund : Job.RefundEntries)
			{
				if (Refund.ItemDefinition == Material) { Total += Refund.Count; }
			}
		}
		return Total;
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
	if (!TestNotNull(TEXT("The offered recipe fixture exists"), OfferedRecipe) ||
		!TestNotNull(TEXT("The outsider recipe fixture exists"), OutsiderRecipe) ||
		!TestWorld.OfferRecipes(*this, { OfferedRecipe }))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	AActor* RequestingActor = TestWorld.GetRequestingController();
	TestTrue(TEXT("The configured recipe is offered by the station"), Station->GetAvailableRecipes().Contains(OfferedRecipe));
	TestFalse(TEXT("An unconfigured recipe is absent from the station offer"), Station->GetAvailableRecipes().Contains(OutsiderRecipe));
	TestEqual(TEXT("An unconfigured recipe has no craftable quantity"), Station->GetMaxCraftableQuantity(RequestingActor, OutsiderRecipe), 0);
	TestFalse(TEXT("The server validation rejects an unconfigured recipe"), Station->CanCraftRecipeQuantity(RequestingActor, OutsiderRecipe, 1));
	TestFalse(TEXT("The authoritative queue rejects an unconfigured recipe"), Station->QueueCraftRecipe(RequestingActor, OutsiderRecipe, 1));
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
	FRpgCraftingSpatialOutputCapacityContractTest,
	"SurvivalRpg.Crafting.Output.SpatialCapacityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingSpatialOutputCapacityContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	URpgInventoryManagerComponent* OutputInventory =
		Station ? Station->GetOutputInventory() : nullptr;
	if (!TestNotNull(TEXT("The station output inventory exists"), OutputInventory))
	{
		return false;
	}

	TestTrue(
		TEXT("The default crafting output uses spatial capacity instead of a hidden four-entry cap"),
		OutputInventory->IsCapacityUnlimited());
	TestTrue(
		TEXT("The authored output root grid remains the actual finite capacity"),
		OutputInventory->GetDefaultGridSize().IsValid());

	FBoolProperty* SpatialCapacityProperty = FindFProperty<FBoolProperty>(
		URpgCraftingStationComponent::StaticClass(),
		TEXT("bUseSpatialOutputCapacity"));
	FIntProperty* LegacyEntryLimitProperty = FindFProperty<FIntProperty>(
		URpgCraftingStationComponent::StaticClass(),
		TEXT("OutputSlotCount"));
	if (!TestNotNull(
			TEXT("The explicit spatial-output policy property exists"),
			SpatialCapacityProperty) ||
		!TestNotNull(
			TEXT("The opt-in legacy output-entry limit exists"),
			LegacyEntryLimitProperty))
	{
		return false;
	}

	SpatialCapacityProperty->SetPropertyValue_InContainer(Station, false);
	LegacyEntryLimitProperty->SetPropertyValue_InContainer(Station, 4);
	Station->SetOutputInventoryManager(OutputInventory);
	TestFalse(
		TEXT("Designers can deliberately opt into the legacy entry-count policy"),
		OutputInventory->IsCapacityUnlimited());
	TestEqual(
		TEXT("The deliberate legacy cap is applied exactly"),
		OutputInventory->GetMaxEntries(),
		4);

	SpatialCapacityProperty->SetPropertyValue_InContainer(Station, true);
	Station->SetOutputInventoryManager(OutputInventory);
	TestTrue(
		TEXT("Returning to spatial capacity removes the legacy entry-count cap"),
		OutputInventory->IsCapacityUnlimited());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingFreeRecipeQuantityLimitTest,
	"SurvivalRpg.Crafting.Authority.FreeRecipeQuantityLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingFreeRecipeQuantityLimitTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* FreeRecipe = TestWorld.CreateRecipe();
	if (!TestNotNull(TEXT("The free recipe fixture exists"), FreeRecipe) ||
		!TestWorld.OfferRecipes(*this, { FreeRecipe }))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	AActor* RequestingActor = TestWorld.GetRequestingController();
	const int32 MaxFreeQuantity = Station->GetMaxCraftableQuantity(RequestingActor, FreeRecipe);
	TestEqual(TEXT("A free recipe uses the station's configured safety maximum"), MaxFreeQuantity, 99);
	TestTrue(TEXT("The configured free-recipe maximum is accepted"), Station->CanCraftRecipeQuantity(RequestingActor, FreeRecipe, MaxFreeQuantity));
	TestFalse(TEXT("A quantity above the free-recipe maximum is rejected"), Station->CanCraftRecipeQuantity(RequestingActor, FreeRecipe, MaxFreeQuantity + 1));
	TestFalse(TEXT("A huge free-recipe request is rejected by server validation"), Station->CanCraftRecipeQuantity(RequestingActor, FreeRecipe, MAX_int32));
	TestFalse(TEXT("The authoritative queue rejects a huge free-recipe request"), Station->QueueCraftRecipe(RequestingActor, FreeRecipe, MAX_int32));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingPhysicalSourcesTest,
	"SurvivalRpg.Crafting.Physical.PlayerFirstAndOtherPlayersExcluded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingPhysicalSourcesTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	URpgInventoryManagerComponent* Player = Fixture.CreatePlayerInventory();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	if (!TestNotNull(TEXT("Shared physical chest exists"), Chest)) { return false; }
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Player->AddItemDefinition(Material, 3);
	Chest->GetInventoryManager()->AddItemDefinition(Material, 9);
	FRpgCraftingResourceCost Cost;
	Cost.ItemDefinition = Material;
	Cost.Count = 5;
	TestEqual(TEXT("UI count includes own inventory and physical chest"), Station->GetAvailableResourceCount(Requester, Material), 12);
	TestTrue(TEXT("Exact cost is consumed atomically"), Station->ConsumeResources(Requester, { Cost }));
	TestEqual(TEXT("Own inventory was consumed first"), Player->GetTotalItemCountByDefinition(Material), 0);
	TestEqual(TEXT("Only remainder comes from shared chest"), Chest->GetInventoryManager()->GetTotalItemCountByDefinition(Material), 7);
	AActor* OtherPlayer = Station->GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	URpgInventoryManagerComponent* OtherInventory = Fixture.CreatePlayerInventory(OtherPlayer);
	OtherInventory->AddItemDefinition(Material, 10);
	TestEqual(TEXT("Nearby other player's materials are absent from UI count"), Station->GetAvailableResourceCount(Requester, Material), 7);
	TArray<FRpgInventoryBatchOperation> Operations;
	TArray<FRpgCraftingRefundEntry> Credits;
	Cost.Count = 8;
	TestFalse(TEXT("Even a caller-supplied other-player source is rejected by the shared debit seam"),
		URpgCraftingStationComponent::BuildResourceConsumptionPlan(Requester, { OtherInventory, Chest->GetInventoryManager() }, { Cost }, 1, Operations, Credits));
	TestEqual(TEXT("Failed planning leaves foreign inventory intact"), OtherInventory->GetTotalItemCountByDefinition(Material), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingAtomicOutputsTest,
	"SurvivalRpg.Crafting.Physical.AtomicOutputsAndNoWorldDrops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingAtomicOutputsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	URpgInventoryManagerComponent* Tray = Station->GetOutputInventory();
	FRpgInventoryGridSize OneCell;
	OneCell.Width = 1;
	OneCell.Height = 1;
	if (!TestTrue(TEXT("Output tray fixture has one cell"), Tray->SetDefaultGridSize(OneCell))) { return false; }
	FRpgCraftingOutputItem Output;
	Output.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	Output.Count = 1;
	TestFalse(TEXT("Two products cannot partially commit to a one-cell tray"), Station->AddCraftingOutputs({ Output, Output }));
	TestEqual(TEXT("First product was not leaked by later product failure"), Tray->GetUsedEntryCount(), 0);
	TestEqual(TEXT("Failure creates no world pickups"), CountDroppedOutputActors(Station->GetWorld()), 0);
	URpgInventoryManagerComponent* Player = Fixture.CreatePlayerInventory();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Player->AddItemDefinition(Material, 4);
	FRpgCraftingResourceCost Cost;
	Cost.ItemDefinition = Material;
	Cost.Count = 2;
	TestFalse(TEXT("Immediate crafting failure keeps costs too"), Station->CraftItems(Fixture.GetRequestingController(), { Cost }, { Output, Output }));
	TestEqual(TEXT("Player retains all input material"), Player->GetTotalItemCountByDefinition(Material), 4);
	TestTrue(TEXT("A fitting single output commits"), Station->AddCraftingOutputs({ Output }));
	TestFalse(TEXT("Full tray rejects another output without dropping"), Station->AddCraftingOutputs({ Output }));
	TestEqual(TEXT("Full-tray rejection retains exactly the existing item"), Tray->GetUsedEntryCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingDurableQueueTest,
	"SurvivalRpg.Crafting.Physical.DurableQueueAndRetainedRefund",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingDurableQueueTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	if (!TestNotNull(TEXT("Refund source chest exists"), Chest)) { return false; }
	URpgInventoryManagerComponent* Source = Chest->GetInventoryManager();
	FRpgInventoryGridSize OneCell;
	OneCell.Width = 1; OneCell.Height = 1;
	Source->SetDefaultGridSize(OneCell);
	Station->GetOutputInventory()->SetDefaultGridSize(OneCell);
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Source->AddItemDefinition(Material, 10);
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 30.0f;
	FRpgCraftingResourceCost& First = Recipe->RequiredResources.AddDefaulted_GetRef();
	First.ItemDefinition = Material; First.Count = 2;
	FRpgCraftingResourceCost& Second = Recipe->RequiredResources.AddDefaulted_GetRef();
	Second.ItemDefinition = Material; Second.Count = 3;
	if (!Fixture.OfferRecipes(*this, { Recipe })) { return false; }
	TestEqual(TEXT("Duplicate material costs aggregate for quantity"), Station->GetMaxCraftableQuantity(Requester, Recipe), 2);
	TestTrue(TEXT("Timed physical-storage job queues"), Station->QueueCraftRecipe(Requester, Recipe, 2));
	TestTrue(TEXT("Explicit pause is accepted"), Station->PauseCraftingStation(Requester));
	const FRpgCraftingStationSaveData Save = Station->ExportCraftingState();
	if (!TestEqual(TEXT("One paid batch is exported"), Save.Jobs.Num(), 1)) { return false; }
	TestEqual(TEXT("Refund records exact paid quantity"), Save.Jobs[0].Refunds[0].Count, 10);
	TestTrue(TEXT("The source identity is durable"), !Save.Jobs[0].Refunds[0].InventoryId.IsNone());
	TestTrue(TEXT("Saved queue restores with tray and paused state"), Station->RestoreCraftingState(Save));
	Station->ResumeRestoredCrafting();
	TestTrue(TEXT("Restored explicit pause remains paused"), Station->IsCraftingPaused());
	TestEqual(TEXT("Restore retains relative remaining time"), Station->ExportCraftingState().Jobs[0].RemainingTime, Save.Jobs[0].RemainingTime);
	Source->AddItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1);
	Station->GetOutputInventory()->AddItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1);
	TestFalse(TEXT("Cancel cannot discard a refund when source and tray are full"), Station->CancelCraftJob(Requester, Save.Jobs[0].JobId));
	TestEqual(TEXT("Unpaid refund claim remains with its job"), Station->GetCraftingJobs().Num(), 1);
	TestEqual(TEXT("No refund falls onto the ground"), CountDroppedOutputActors(Station->GetWorld()), 0);
	const TArray<FRpgInventoryEntryView> SourceEntries = Source->GetAllEntries();
	Source->ConsumeItemById(SourceEntries[0].ItemId, 1);
	TestTrue(TEXT("Cancel succeeds once original source has space"), Station->CancelCraftJob(Requester, Save.Jobs[0].JobId));
	TestEqual(TEXT("All paid materials are restored exactly once"), Source->GetTotalItemCountByDefinition(Material), 10);
	TestEqual(TEXT("Successful refund removes the job"), Station->GetCraftingJobs().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingOutputAssignmentTest,
	"SurvivalRpg.Crafting.Physical.TrayDefaultAssignmentAndRuntimeState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingOutputAssignmentTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	if (!TestNotNull(TEXT("Assigned destination exists"), Chest)) { return false; }
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	FRpgStorageAssignment Rule;
	Rule.ItemDefinition = Material;
	TestTrue(TEXT("An exact item assignment can be authored"), Chest->GetContainerComponent()->SetAssignments({ Rule }));
	FRpgCraftingOutputItem Output;
	Output.ItemDefinition = Material; Output.Count = 3;
	TestFalse(TEXT("Outputs remain in tray by default"), Station->IsCraftingOutputAutoDepositEnabled());
	TestTrue(TEXT("Default output is created"), Station->AddCraftingOutputs({ Output }));
	TestEqual(TEXT("The tray owns the default output"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(Material), 3);
	TestTrue(TEXT("Automatic routing needs no upgrade"), Station->SetCraftingOutputAutoDepositEnabled(Fixture.GetRequestingController(), true));
	TestEqual(TEXT("Turning automation on routes existing ordinary output"), Chest->GetInventoryManager()->GetTotalItemCountByDefinition(Material), 3);
	URpgInventoryItemInstance* Variant = Station->GetOutputInventory()->AddItemDefinition(Material, 2);
	if (!TestNotNull(TEXT("Runtime-state material fixture exists"), Variant)) { return false; }
	Variant->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 71);
	const FRpgInventoryItemId VariantId = Variant->GetItemId();
	const int32 Revision = Station->GetOutputInventory()->GetInventoryRevision();
	TestFalse(TEXT("Automation does not collapse a stateful material"), Station->FlushOutputToBaseStorage());
	TestEqual(TEXT("Skipped stateful output does not change the tray revision"), Station->GetOutputInventory()->GetInventoryRevision(), Revision);
	TestTrue(TEXT("Stateful output retains exact identity and state"), Station->GetOutputInventory()->FindItemById(VariantId) == Variant && Variant->GetStatTagStackCount(RpgGameplayTags::Ability_Attack_Basic) == 71);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingSpatialDomainTest,
	"SurvivalRpg.Crafting.Physical.BaseOverridesStationRadius",
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
		ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
		if (!Chest) { return false; }
		Chest->SetActorLocation(Position);
		return Chest->GetInventoryManager()->AddItemDefinition(Material, Count) != nullptr;
	};
	if (!MakeStock(FVector(4000, 0, 0), 7) || !MakeStock(FVector(4990, 0, 0), 11) ||
		!MakeStock(FVector(0, 0, 10000), 13) || !MakeStock(FVector(5100, 0, 0), 3)) { AddError(TEXT("Could not seed spatial source fixtures")); return false; }
	AActor* Requester = Fixture.GetRequestingController();
	TestEqual(TEXT("Inside uses the full horizontal base including other floors"), Station->GetAvailableResourceCount(Requester, Material), 31);
	Station->GetOwner()->SetActorLocation(FVector(5100, 0, 0));
	TestEqual(TEXT("Outside radius reaches individual nearby base chests without inheriting their base"), Station->GetAvailableResourceCount(Requester, Material), 14);
	Radius->SetPropertyValue_InContainer(Station, 0.0f);
	TestEqual(TEXT("Zero outside radius never means unlimited access"), Station->GetAvailableResourceCount(Requester, Material), 0);
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
	"SurvivalRpg.Crafting.Physical.SynchronousMutationGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStationReentrancyTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	AActor* Requester = Fixture.GetRequestingController();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 0.1f;
	Recipe->OutputItems[0].ItemDefinition = URpgCraftingAutomationReentrantProduct::StaticClass();
	URpgCraftingRecipeDefinition* FreeRecipe = Fixture.CreateRecipe();
	if (!Fixture.OfferRecipes(*this, { Recipe, FreeRecipe }) ||
		!TestTrue(TEXT("Fixture product is queued"), Station->QueueCraftRecipe(Requester, Recipe, 1))) { return false; }
	const FRpgCraftingStationSaveData Snapshot = Station->ExportCraftingState();
	const FGuid JobId = Snapshot.Jobs[0].JobId;
	int32 FragmentCalls = 0;
	int32 ObserverCalls = 0;
	bool bRejectedAllCommands = true;
	auto AttemptNestedCommands = [&]()
	{
		bRejectedAllCommands &= !Station->QueueCraftRecipe(Requester, FreeRecipe, 1);
		bRejectedAllCommands &= !Station->CancelCraftJob(Requester, JobId);
		bRejectedAllCommands &= !Station->PauseCraftingStation(Requester);
		bRejectedAllCommands &= !Station->ResumeCraftingStation(Requester);
		bRejectedAllCommands &= !Station->RestoreCraftingState(Snapshot);
		bRejectedAllCommands &= !Station->SetCraftingOutputAutoDepositEnabled(Requester, true);
		Station->ResumeRestoredCrafting();
	};
	URpgCraftingAutomationReentrantFragment::OnCreate = [&]()
	{
		++FragmentCalls;
		AttemptNestedCommands();
	};
	const FDelegateHandle Handle = Station->GetOutputInventory()->OnInventoryPostCommit.AddLambda([&](URpgInventoryManagerComponent*)
	{
		++ObserverCalls;
		AttemptNestedCommands();
	});
	// Isolated fixture clocks need a fresh frame both when registering and when expiring a pending timer.
	TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
	++GFrameCounter;
	Station->GetWorld()->GetTimerManager().Tick(0.0f);
	++GFrameCounter;
	Station->GetWorld()->GetTimerManager().Tick(0.2f);
	URpgCraftingAutomationReentrantFragment::OnCreate = {};
	Station->GetOutputInventory()->OnInventoryPostCommit.Remove(Handle);
	TestTrue(TEXT("Output preparation exercised the fragment callback"), FragmentCalls > 0);
	TestTrue(TEXT("Output publication exercised the synchronous inventory observer"), ObserverCalls > 0);
	TestTrue(TEXT("Queue, refund, pause, restore and settings reject nested mutation"), bRejectedAllCommands);
	TestEqual(TEXT("The original job completes exactly once"), Station->GetCraftingJobs().Num(), 0);
	TestEqual(TEXT("Only the original output is committed"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(URpgCraftingAutomationReentrantProduct::StaticClass()), 1);
	FIntProperty* StateRevision = FindFProperty<FIntProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("CraftingStateRevision"));
	if (!TestNotNull(TEXT("Queue revision contract exists"), StateRevision)) { return false; }
	TestTrue(TEXT("A second product queues after the guard releases"), Station->QueueCraftRecipe(Requester, Recipe, 1));
	URpgCraftingAutomationReentrantFragment::OnCreate = [&]()
	{
		// Simulate a future internal mutation bypassing command entrypoints: final context validation must still reject it.
		StateRevision->SetPropertyValue_InContainer(Station, StateRevision->GetPropertyValue_InContainer(Station) + 1);
	};
	++GFrameCounter;
	Station->GetWorld()->GetTimerManager().Tick(0.0f);
	++GFrameCounter;
	Station->GetWorld()->GetTimerManager().Tick(0.2f);
	URpgCraftingAutomationReentrantFragment::OnCreate = {};
	TestEqual(TEXT("Changed station revision rejects the staged output"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(URpgCraftingAutomationReentrantProduct::StaticClass()), 1);
	const TArray<FRpgCraftingJobEntry> PendingJobs = Station->GetCraftingJobs();
	if (!TestEqual(TEXT("Rejected output retains its unpaid job"), PendingJobs.Num(), 1)) { return false; }
	TestEqual(TEXT("The rejected unit waits for a safe retry"), PendingJobs[0].State, ERpgCraftingJobState::BlockedOutput);
	TestTrue(TEXT("The unchanged refund claim remains cancelable"), Station->CancelCraftJob(Requester, PendingJobs[0].JobId));
	TestTrue(TEXT("The guard releases after the outer operation"), Station->QueueCraftRecipe(Requester, FreeRecipe, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingRestoreGridValidationTest,
	"SurvivalRpg.Crafting.Physical.RestoreRejectsOutsideSavedGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingRestoreGridValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	URpgInventoryManagerComponent* Tray = Station->GetOutputInventory();
	FRpgInventoryGridSize Grid;
	Grid.Width = 4; Grid.Height = 4;
	if (!Tray->SetDefaultGridSize(Grid) || !Tray->AddItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1)) { return false; }
	const FRpgCraftingStationSaveData Original = Station->ExportCraftingState();
	const int32 Revision = Tray->GetInventoryRevision();
	int32 Notifications = 0;
	const FDelegateHandle Handle = Tray->OnInventoryPostCommit.AddLambda([&](URpgInventoryManagerComponent*) { ++Notifications; });
	FRpgCraftingStationSaveData Invalid = Original;
	Invalid.OutputGridSize.Width = 1;
	Invalid.OutputInventoryGraph.Items[0].Placement.X = 2;
	TestFalse(TEXT("A root item fitting the old tray but outside the saved tray rejects before publication"), Station->RestoreCraftingState(Invalid));
	Invalid.OutputGridSize.Width = 8;
	Invalid.OutputInventoryGraph.Items[0].Placement.X = 8;
	TestFalse(TEXT("Invalid expanded save does not temporarily enlarge the runtime tray"), Station->RestoreCraftingState(Invalid));
	Tray->OnInventoryPostCommit.Remove(Handle);
	const FRpgCraftingStationSaveData After = Station->ExportCraftingState();
	TestTrue(TEXT("Rejected restore retains the original dimensions"), After.OutputGridSize == Original.OutputGridSize);
	TestEqual(TEXT("Rejected restore retains the original position"), After.OutputInventoryGraph.Items[0].Placement.X, Original.OutputInventoryGraph.Items[0].Placement.X);
	TestEqual(TEXT("Rejected restore does not publish an inventory revision"), Tray->GetInventoryRevision(), Revision);
	TestEqual(TEXT("Rejected restore notifies no committed graph"), Notifications, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingCompetingQueuesConservationTest,
	"SurvivalRpg.Crafting.Physical.CompetingQueuesConserveMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingCompetingQueuesConservationTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* First = Fixture.GetStation();
	UWorld* World = First->GetWorld();
	ARpgCraftingStationActor* OtherActor = World->SpawnActor<ARpgCraftingStationActor>();
	ARpgInventoryAutomationTestPlayerController* OtherPlayer = World->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	if (!OtherActor || !OtherPlayer || !Chest) { return false; }
	URpgCraftingStationComponent* Second = OtherActor->GetCraftingStationComponent();
	URpgInventoryManagerComponent* Source = Chest->GetInventoryManager();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	if (!Source->AddItemDefinition(Material, 9)) { return false; }
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 0.1f;
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = Material; Cost.Count = 3;
	if (!Fixture.OfferRecipes(*this, { Recipe }) || !Fixture.OfferRecipes(*this, { Recipe }, Second)) { return false; }
	AActor* Requester = Fixture.GetRequestingController();
	TestTrue(TEXT("Both players initially see the same affordable batch"), First->CanCraftRecipeQuantity(Requester, Recipe, 2) && Second->CanCraftRecipeQuantity(OtherPlayer, Recipe, 2));
	TestTrue(TEXT("First request reserves six shared materials"), First->QueueCraftRecipe(Requester, Recipe, 2));
	TestFalse(TEXT("Another player cannot queue a stale affordable batch at the same station"), First->QueueCraftRecipe(OtherPlayer, Recipe, 2));
	TestFalse(TEXT("Another station cannot spend those same reserved materials"), Second->QueueCraftRecipe(OtherPlayer, Recipe, 2));
	TestTrue(TEXT("A smaller independent request can spend the remaining three"), Second->QueueCraftRecipe(OtherPlayer, Recipe, 1));
	TestEqual(TEXT("Live plus both stations' paid claims equals the initial stock"), Source->GetTotalItemCountByDefinition(Material) + CountReservedMaterial(First, Material) + CountReservedMaterial(Second, Material), 9);
	const TArray<FRpgCraftingJobEntry> FirstJobs = First->GetCraftingJobs();
	if (!TestEqual(TEXT("First queue has one paid batch"), FirstJobs.Num(), 1)) { return false; }
	TestTrue(TEXT("Second player can cancel the shared first queue"), First->CancelCraftJob(OtherPlayer, FirstJobs[0].JobId));
	TestFalse(TEXT("A repeated cancellation cannot refund twice"), First->CancelCraftJob(Requester, FirstJobs[0].JobId));
	AdvanceCraftingTimers(World, 0.2f);
	TestEqual(TEXT("Canceled timer produces no output"), First->GetOutputInventory()->GetTotalItemCountByDefinition(Product), 0);
	TestEqual(TEXT("Other station completes once"), Second->GetOutputInventory()->GetTotalItemCountByDefinition(Product), 1);
	TestEqual(TEXT("Refunded inputs plus inputs represented by produced output conserve all nine"), Source->GetTotalItemCountByDefinition(Material) + 3 * Second->GetOutputInventory()->GetTotalItemCountByDefinition(Product), 9);
	TestEqual(TEXT("Completed and canceled queues retain no credits"), CountReservedMaterial(First, Material) + CountReservedMaterial(Second, Material), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingCompletionDomainChangeTest,
	"SurvivalRpg.Crafting.Physical.CompletionRetriesAfterConcurrentSourceChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingCompletionDomainChangeTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* First = Fixture.GetStation();
	UWorld* World = First->GetWorld();
	ARpgCraftingStationActor* OtherActor = World->SpawnActor<ARpgCraftingStationActor>();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	if (!OtherActor || !Chest) { return false; }
	URpgCraftingStationComponent* Second = OtherActor->GetCraftingStationComponent();
	URpgInventoryManagerComponent* Source = Chest->GetInventoryManager();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	if (!Source->AddItemDefinition(Material, 6)) { return false; }
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 0.1f;
	Recipe->OutputItems[0].ItemDefinition = URpgCraftingAutomationReentrantProduct::StaticClass();
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = Material; Cost.Count = 2;
	URpgCraftingRecipeDefinition* OtherRecipe = Fixture.CreateRecipe();
	OtherRecipe->CraftTime = 0.1f;
	OtherRecipe->RequiredResources = Recipe->RequiredResources;
	if (!Fixture.OfferRecipes(*this, { Recipe }) || !Fixture.OfferRecipes(*this, { OtherRecipe }, Second)) { return false; }
	AActor* Requester = Fixture.GetRequestingController();
	if (!First->QueueCraftRecipe(Requester, Recipe, 1)) { return false; }
	bool bChanged = false, bOtherQueueAccepted = false, bExpanded = false;
	URpgCraftingAutomationReentrantFragment::OnCreate = [&]()
	{
		if (bChanged) { return; }
		bChanged = true;
		// An independent station may commit while the first station stages its finished output.
		bOtherQueueAccepted = Second->QueueCraftRecipe(Requester, OtherRecipe, 1);
		FRpgInventoryGridSize Larger = Source->GetDefaultGridSize();
		++Larger.Width;
		bExpanded = Source->SetDefaultGridSize(Larger);
		Chest->SetActorLocation(FVector(10000.0f, 0.0f, 0.0f));
	};
	AdvanceCraftingTimers(World, 0.2f);
	URpgCraftingAutomationReentrantFragment::OnCreate = {};
	TestTrue(TEXT("Independent queue, capacity update and source move occurred during staging"), bChanged && bOtherQueueAccepted && bExpanded);
	const TArray<FRpgCraftingJobEntry> Blocked = First->GetCraftingJobs();
	if (!TestEqual(TEXT("Changed domain preserves the paid first job"), Blocked.Num(), 1)) { return false; }
	TestEqual(TEXT("Stale output plan waits for retry"), Blocked[0].State, ERpgCraftingJobState::BlockedOutput);
	TestEqual(TEXT("Rejected preparation creates no first product"), First->GetOutputInventory()->GetUsedEntryCount(), 0);
	TestEqual(TEXT("Live stock and both paid claims still conserve six inputs"), Source->GetTotalItemCountByDefinition(Material) + CountReservedMaterial(First, Material) + CountReservedMaterial(Second, Material), 6);
	AdvanceCraftingTimers(World, 0.6f);
	TestEqual(TEXT("Retry completes first job once after its source moved away"), First->GetOutputInventory()->GetTotalItemCountByDefinition(URpgCraftingAutomationReentrantProduct::StaticClass()), 1);
	TestEqual(TEXT("Independent station also completes once"), Second->GetOutputInventory()->GetTotalItemCountByDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass()), 1);
	TestEqual(TEXT("Both completed jobs release all paid claims"), CountReservedMaterial(First, Material) + CountReservedMaterial(Second, Material), 0);
	TestEqual(TEXT("Untouched source remainder survives the upgrade and move"), Source->GetTotalItemCountByDefinition(Material), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingBlockedRemainderConservationTest,
	"SurvivalRpg.Crafting.Physical.PartialCompletionFullRefundAndReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingBlockedRemainderConservationTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	URpgInventoryManagerComponent* Tray = Station->GetOutputInventory();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	if (!Chest) { return false; }
	URpgInventoryManagerComponent* Source = Chest->GetInventoryManager();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	FRpgInventoryGridSize OneCell; OneCell.Width = 1; OneCell.Height = 1;
	if (!Tray->SetDefaultGridSize(OneCell) || !Source->SetDefaultGridSize(OneCell) || !Source->AddItemDefinition(Material, 6)) { return false; }
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 0.1f;
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = Material; Cost.Count = 2;
	if (!Fixture.OfferRecipes(*this, { Recipe }) || !Station->QueueCraftRecipe(Fixture.GetRequestingController(), Recipe, 3)) { return false; }
	if (!Source->AddItemDefinition(Product, 1)) { return false; }
	AdvanceCraftingTimers(Station->GetWorld(), 0.2f);
	AdvanceCraftingTimers(Station->GetWorld(), 0.2f);
	const FRpgCraftingStationSaveData Save = Station->ExportCraftingState();
	if (!TestEqual(TEXT("Full tray retains one unfinished batch"), Save.Jobs.Num(), 1)) { return false; }
	TestEqual(TEXT("Exactly one unit completed before the tray filled"), Save.Jobs[0].QuantityCompleted, 1);
	TestEqual(TEXT("Only the two unfinished units retain input credits"), CountReservedMaterial(Station, Material), 4);
	TestTrue(TEXT("Blocked output and its exact credits restore together"), Station->RestoreCraftingState(Save));
	TestFalse(TEXT("Cancel cannot delete credits when original source and tray are full"), Station->CancelCraftJob(Fixture.GetRequestingController(), Save.Jobs[0].JobId));
	TestEqual(TEXT("Failed cancellation preserves all four remaining inputs"), CountReservedMaterial(Station, Material), 4);
	Station->ResumeRestoredCrafting();
	const TArray<FRpgInventoryEntryView> FirstProducts = Tray->GetAllEntries();
	if (!TestEqual(TEXT("First output exists exactly once after restore"), FirstProducts.Num(), 1)) { return false; }
	TestTrue(TEXT("Player may collect the completed output"), Tray->ConsumeItemById(FirstProducts[0].ItemId, 1).IsSuccess());
	AdvanceCraftingTimers(Station->GetWorld(), 0.6f);
	TestEqual(TEXT("Freed tray permits one additional unit"), Tray->GetTotalItemCountByDefinition(Product), 1);
	TestEqual(TEXT("Only the last unit remains reserved"), CountReservedMaterial(Station, Material), 2);
	const TArray<FRpgInventoryEntryView> SecondProducts = Tray->GetAllEntries();
	if (!TestEqual(TEXT("The retry creates a single second output"), SecondProducts.Num(), 1)) { return false; }
	TestTrue(TEXT("Player collects second product before cancellation"), Tray->ConsumeItemById(SecondProducts[0].ItemId, 1).IsSuccess());
	TestTrue(TEXT("Remaining two inputs refund into the now-empty tray"), Station->CancelCraftJob(Fixture.GetRequestingController(), Save.Jobs[0].JobId));
	TestFalse(TEXT("Cancel replay after success cannot mint another refund"), Station->CancelCraftJob(Fixture.GetRequestingController(), Save.Jobs[0].JobId));
	AdvanceCraftingTimers(Station->GetWorld(), 1.0f);
	TestEqual(TEXT("Two collected products plus refund conserve the six original inputs"), 2 * 2 + Tray->GetTotalItemCountByDefinition(Material), 6);
	TestEqual(TEXT("Canceled completion cannot append another product"), Tray->GetTotalItemCountByDefinition(Product), 0);
	TestEqual(TEXT("No paid claims are left after successful refund"), CountReservedMaterial(Station, Material), 0);
	TestEqual(TEXT("Capacity pressure never produced world drops"), CountDroppedOutputActors(Station->GetWorld()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingMissingRefundSourcesTest,
	"SurvivalRpg.Crafting.Physical.DisconnectAndDestroyedSourceRetainRefunds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingMissingRefundSourcesTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	URpgInventoryManagerComponent* Tray = Station->GetOutputInventory();
	ARpgInventoryContainerActor* Chest = Fixture.CreateChest();
	URpgInventoryManagerComponent* PlayerInventory = Fixture.CreatePlayerInventory();
	if (!Chest || !PlayerInventory) { return false; }
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Product = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	FRpgInventoryGridSize OneCell; OneCell.Width = 1; OneCell.Height = 1;
	if (!Tray->SetDefaultGridSize(OneCell) || !PlayerInventory->AddItemDefinition(Material, 2) || !Chest->GetInventoryManager()->AddItemDefinition(Material, 4)) { return false; }
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 30.0f;
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = Material; Cost.Count = 3;
	if (!Fixture.OfferRecipes(*this, { Recipe }) || !Station->QueueCraftRecipe(Fixture.GetRequestingController(), Recipe, 2)) { return false; }
	TestEqual(TEXT("Own materials were reserved before the shared source"), PlayerInventory->GetTotalItemCountByDefinition(Material), 0);
	const FRpgCraftingStationSaveData Save = Station->ExportCraftingState();
	if (!TestEqual(TEXT("Paid batch records both sources"), Save.Jobs[0].Refunds.Num(), 2)) { return false; }
	Fixture.GetRequestingController()->Destroy();
	Chest->Destroy();
	ARpgInventoryAutomationTestPlayerController* OtherPlayer = Station->GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	if (!OtherPlayer || !Station->RestoreCraftingState(Save) || !Tray->AddItemDefinition(Product, 1)) { return false; }
	TestFalse(TEXT("Missing original sources and a full tray retain the cancel claim"), Station->CancelCraftJob(OtherPlayer, Save.Jobs[0].JobId));
	TestEqual(TEXT("Disconnected player's and destroyed chest's credits remain intact"), CountReservedMaterial(Station, Material), 6);
	const TArray<FRpgInventoryEntryView> Entries = Tray->GetAllEntries();
	if (Entries.Num() != 1 || !Tray->ConsumeItemById(Entries[0].ItemId, 1).IsSuccess()) { return false; }
	TestTrue(TEXT("Another player can recover the paid resources into the station tray"), Station->CancelCraftJob(OtherPlayer, Save.Jobs[0].JobId));
	TestEqual(TEXT("All six reserved inputs survive lost source actors and reload"), Tray->GetTotalItemCountByDefinition(Material), 6);
	TestFalse(TEXT("Repeated recovery has no surviving job to refund"), Station->CancelCraftJob(OtherPlayer, Save.Jobs[0].JobId));
	TestEqual(TEXT("No world drop substitutes for a missing source"), CountDroppedOutputActors(Station->GetWorld()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingActiveRestoreTimeTest,
	"SurvivalRpg.Crafting.Physical.ActiveRestoreNoOfflineProgressOrDuplicateOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingActiveRestoreTimeTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	URpgInventoryManagerComponent* Player = Fixture.CreatePlayerInventory();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	if (!Player || !Player->AddItemDefinition(Material, 4)) { return false; }
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	Recipe->CraftTime = 0.1f;
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = Material; Cost.Count = 2;
	if (!Fixture.OfferRecipes(*this, { Recipe }) || !Station->QueueCraftRecipe(Fixture.GetRequestingController(), Recipe, 2)) { return false; }
	FRpgCraftingStationSaveData Save = Station->ExportCraftingState();
	Save.Jobs[0].RemainingTime = 0.05f;
	TestTrue(TEXT("Active paid job restores with its saved remaining duration"), Station->RestoreCraftingState(Save));
	AdvanceCraftingTimers(Station->GetWorld(), 10.0f);
	TestEqual(TEXT("A restored queue cannot run before the world restore completes"), Station->GetOutputInventory()->GetUsedEntryCount(), 0);
	TestEqual(TEXT("Waiting for resume preserves all paid inputs"), CountReservedMaterial(Station, Material), 4);
	Station->ResumeRestoredCrafting();
	AdvanceCraftingTimers(Station->GetWorld(), 0.025f);
	TestEqual(TEXT("Resume does not complete before the stored remaining duration"), Station->GetOutputInventory()->GetUsedEntryCount(), 0);
	AdvanceCraftingTimers(Station->GetWorld(), 0.03f);
	TestEqual(TEXT("Stored active unit completes once"), Station->GetOutputInventory()->GetUsedEntryCount(), 1);
	TestEqual(TEXT("Completed unit spends exactly its two input credits"), CountReservedMaterial(Station, Material), 2);
	AdvanceCraftingTimers(Station->GetWorld(), 0.2f);
	TestEqual(TEXT("The full batch produces exactly two products"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass()), 2);
	TestEqual(TEXT("No paid job survives completion"), Station->GetCraftingJobs().Num(), 0);
	TestEqual(TEXT("Restore and completion never charge ingredients again"), Player->GetTotalItemCountByDefinition(Material), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingPendingSourceRestoreRefundTest,
	"SurvivalRpg.Crafting.Physical.PendingSourceRestoreRefundUsesTray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingPendingSourceRestoreRefundTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld Fixture;
	if (!Fixture.Initialize(*this, true)) { return false; }
	URpgCraftingStationComponent* Station = Fixture.GetStation();
	ARpgGameModeBase* Mode = Station->GetWorld()->GetAuthGameMode<ARpgGameModeBase>();
	APlayerController* Requester = Fixture.GetRequestingController();
	URpgInventoryManagerComponent* ProvisionalPlayer = Fixture.CreatePlayerInventory();
	ARpgInventoryContainerActor* ReturningChest = Fixture.CreateChest();
	if (!Mode || !ProvisionalPlayer || !ReturningChest) { return false; }
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	URpgCraftingRecipeDefinition* Recipe = Fixture.CreateRecipe();
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = Material; Cost.Count = 2;
	TestFalse(TEXT("The connected player still awaits its profile restore"), Mode->IsPlayerProfileRestoreComplete(Requester));
	ReturningChest->GetContainerComponent()->SetConstructionPending(true);

	for (URpgInventoryManagerComponent* Original : { ProvisionalPlayer, ReturningChest->GetInventoryManager() })
	{
		const FName SourceId = RpgStorageAccessRules::GetPersistentInventoryId(Original);
		if (!TestFalse(TEXT("The pending source has a durable identity"), SourceId.IsNone())) { return false; }
		TestNull(TEXT("Pending graph replacement excludes the provisional refund target"), RpgStorageAccessRules::FindPersistentInventory(Station->GetWorld(), SourceId));
		const FRpgInventoryGraphSaveData PendingGraph = Original->ExportInventoryGraph();
		FRpgCraftingStationSaveData Save = Station->ExportCraftingState();
		Save.bPaused = true;
		FRpgCraftingJobSaveData& Job = Save.Jobs.AddDefaulted_GetRef();
		Job.JobId = FGuid::NewGuid(); Job.Recipe = Recipe; Job.QuantityTotal = 1;
		Job.State = static_cast<uint8>(ERpgCraftingJobState::Paused);
		Job.RemainingTime = 1.0f;
		FRpgCraftingRefundSaveData& Credit = Job.Refunds.AddDefaulted_GetRef();
		Credit.ItemDefinition = Material; Credit.Count = 2; Credit.InventoryId = SourceId;
		const FGuid JobId = Job.JobId;
		if (!TestTrue(TEXT("A saved paid claim restores while its source is pending"), Station->RestoreCraftingState(Save))) { return false; }
		TestTrue(TEXT("Cancellation refunds to the stable tray while the source is provisional"), Station->CancelCraftJob(Requester, JobId));
		TestEqual(TEXT("Provisional source receives no items that its later restore could erase"), Original->GetTotalItemCountByDefinition(Material), 0);
		TestEqual(TEXT("The entire refund is retained in the output tray"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(Material), 2);
		FRpgInventoryMutationResult Result;
		TestTrue(TEXT("The delayed source graph replacement completes"), Original->RestoreInventoryGraph(PendingGraph, Result));
		TestEqual(TEXT("Whole graph replacement cannot erase the two refunded materials"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(Material) + Original->GetTotalItemCountByDefinition(Material), 2);
		const TArray<FRpgInventoryEntryView> RefundItems = Station->GetOutputInventory()->GetAllEntries();
		if (RefundItems.Num() != 1 || !Station->GetOutputInventory()->ConsumeItemById(RefundItems[0].ItemId, 2).IsSuccess()) { return false; }
	}
	return true;
}

#endif
