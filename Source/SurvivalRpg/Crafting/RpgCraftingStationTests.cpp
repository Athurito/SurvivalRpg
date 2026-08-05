#include "RpgCraftingRecipeDefinition.h"
#include "RpgCraftingStationActor.h"
#include "RpgCraftingStationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
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

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!GameInstance || !World)
			{
				Test.AddError(TEXT("Could not create an isolated standalone crafting test world."));
				return false;
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
			const TArray<URpgCraftingRecipeDefinition*>& Recipes)
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

			AvailableRecipeSetProperty->SetObjectPropertyValue_InContainer(Station, RecipeSet.Get());
			return true;
		}

		ARpgBaseCampActor* CreateLinkedBaseCamp(FAutomationTestBase& Test)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgBaseCampActor::StaticClass(),
				TEXT("CraftingTestBaseCamp"));
			SpawnParameters.ObjectFlags = RF_Transient;
			ARpgBaseCampActor* BaseCamp = World->SpawnActor<ARpgBaseCampActor>(SpawnParameters);
			if (Test.TestNotNull(TEXT("The linked base-camp fixture exists"), BaseCamp))
			{
				URpgBaseStorageComponent* BaseStorage =
					BaseCamp->GetBaseStorageComponent();
				if (!Test.TestNotNull(
						TEXT("The linked base-camp fixture owns storage"),
						BaseStorage))
				{
					return nullptr;
				}

				// InitializeStandalone does not dispatch actor BeginPlay. Restore an
				// empty validated network snapshot so the fixture receives the same
				// baseline Materials capacity and capabilities as a live base.
				FRpgBaseStorageSaveData BootstrapState;
				FString BootstrapError;
				if (!Test.TestTrue(
						TEXT("The standalone base-storage fixture initializes its baseline network"),
						BaseStorage->RestoreStorageState(
							BootstrapState,
							BootstrapError)))
				{
					Test.AddError(BootstrapError);
					return nullptr;
				}
				Station->SetLinkedBaseCamp(BaseCamp);
			}
			return BaseCamp;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingResourceBackedQuantityTest,
	"SurvivalRpg.Crafting.Authority.ResourceBackedQuantityUsesResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingResourceBackedQuantityTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* ResourceRecipe = TestWorld.CreateRecipe();
	if (!TestNotNull(TEXT("The resource-backed recipe fixture exists"), ResourceRecipe))
	{
		return false;
	}

	FRpgCraftingResourceCost& Cost = ResourceRecipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Cost.Count = 2;
	if (!TestWorld.OfferRecipes(*this, { ResourceRecipe }))
	{
		return false;
	}

	ARpgBaseCampActor* BaseCamp = TestWorld.CreateLinkedBaseCamp(*this);
	URpgBaseStorageComponent* BaseStorage = BaseCamp ? BaseCamp->GetBaseStorageComponent() : nullptr;
	if (!TestNotNull(TEXT("The linked resource storage exists"), BaseStorage))
	{
		return false;
	}

	BaseStorage->AddResourceCapacity(
		URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
		250);
	if (!TestTrue(
			TEXT("The linked base stores the resource fixture"),
			BaseStorage->StoreDefinitionResource(
				URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
				250)))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	AActor* RequestingActor = TestWorld.GetRequestingController();
	TestEqual(
		TEXT("Resource-backed quantity is derived from available resources and is not capped by the free-recipe fallback"),
		Station->GetMaxCraftableQuantity(RequestingActor, ResourceRecipe),
		125);
	TestTrue(TEXT("The full resource-backed maximum is accepted"), Station->CanCraftRecipeQuantity(RequestingActor, ResourceRecipe, 125));
	TestFalse(TEXT("A resource-backed quantity above the available amount is rejected"), Station->CanCraftRecipeQuantity(RequestingActor, ResourceRecipe, 126));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingOutputValidationAndWorldDropFallbackTest,
	"SurvivalRpg.Crafting.Authority.OutputValidationAndWorldDropFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingOutputValidationAndWorldDropFallbackTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* EmptyOutputRecipe = TestWorld.CreateRecipe();
	if (!TestNotNull(TEXT("The empty-output recipe fixture exists"), EmptyOutputRecipe))
	{
		return false;
	}

	EmptyOutputRecipe->OutputItems.Reset();
	if (!TestWorld.OfferRecipes(*this, { EmptyOutputRecipe }))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	AActor* RequestingActor = TestWorld.GetRequestingController();
	const TArray<FRpgCraftingOutputItem> EmptyOutputs;
	TestFalse(TEXT("An empty output list is rejected fail-closed"), Station->CanAcceptCraftingOutputs(EmptyOutputs));
	TestEqual(
		TEXT("An offered recipe without outputs has no craftable quantity"),
		Station->GetMaxCraftableQuantity(RequestingActor, EmptyOutputRecipe),
		0);

	TArray<FRpgCraftingOutputItem> InvalidOutputs;
	FRpgCraftingOutputItem& InvalidOutput = InvalidOutputs.AddDefaulted_GetRef();
	InvalidOutput.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	InvalidOutput.Count = 0;
	TestFalse(TEXT("A malformed output entry is rejected fail-closed"), Station->CanAcceptCraftingOutputs(InvalidOutputs));

	TArray<FRpgCraftingOutputItem> ValidOutputs;
	FRpgCraftingOutputItem& ValidOutput = ValidOutputs.AddDefaulted_GetRef();
	ValidOutput.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	ValidOutput.Count = 1;

	URpgInventoryManagerComponent* OutputInventory = Station->GetOutputInventory();
	if (!TestNotNull(TEXT("The station output inventory exists"), OutputInventory))
	{
		return false;
	}

	const FRpgInventoryGridSize OutputGridSize =
		OutputInventory->GetDefaultGridSize();
	bool bFilledOutputGrid = OutputGridSize.IsValid();
	for (int32 CellIndex = 0;
		bFilledOutputGrid &&
			CellIndex < OutputGridSize.Width * OutputGridSize.Height;
		++CellIndex)
	{
		bFilledOutputGrid =
			OutputInventory->AddItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				1) != nullptr;
	}
	if (!TestTrue(
			TEXT("The finite spatial output grid can be filled completely"),
			bFilledOutputGrid))
	{
		return false;
	}
	TestFalse(
		TEXT("The full spatial output grid cannot accept another valid item"),
		OutputInventory->CanAddItemDefinition(ValidOutput.ItemDefinition, ValidOutput.Count));
	TestTrue(
		TEXT("A valid output remains acceptable because storage overflow has a world-drop fallback"),
		Station->CanAcceptCraftingOutputs(ValidOutputs));

	UWorld* World = Station->GetWorld();
	const int32 DropsBefore = CountDroppedOutputActors(World);
	TestTrue(
		TEXT("Authority materializes valid overflow through the world-drop fallback"),
		Station->AddCraftingOutputs(ValidOutputs));
	TestEqual(
		TEXT("Exactly one world pickup is spawned for the overflow"),
		CountDroppedOutputActors(World),
		DropsBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingDuplicateResourceAggregationTest,
	"SurvivalRpg.Crafting.Authority.DuplicateResourceCostsAreAggregated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingDuplicateResourceAggregationTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingStationTests;
	FScopedCraftingWorld TestWorld;
	if (!TestWorld.Initialize(*this))
	{
		return false;
	}

	URpgCraftingRecipeDefinition* DuplicateCostRecipe = TestWorld.CreateRecipe();
	if (!TestNotNull(TEXT("The duplicate-cost recipe fixture exists"), DuplicateCostRecipe))
	{
		return false;
	}

	FRpgCraftingResourceCost& FirstCost = DuplicateCostRecipe->RequiredResources.AddDefaulted_GetRef();
	FirstCost.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	FirstCost.Count = 2;
	FRpgCraftingResourceCost& DuplicateCost = DuplicateCostRecipe->RequiredResources.AddDefaulted_GetRef();
	DuplicateCost.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	DuplicateCost.Count = 3;
	if (!TestWorld.OfferRecipes(*this, { DuplicateCostRecipe }))
	{
		return false;
	}

	ARpgBaseCampActor* BaseCamp = TestWorld.CreateLinkedBaseCamp(*this);
	URpgBaseStorageComponent* BaseStorage = BaseCamp ? BaseCamp->GetBaseStorageComponent() : nullptr;
	if (!TestNotNull(TEXT("The linked resource storage exists"), BaseStorage))
	{
		return false;
	}

	BaseStorage->AddResourceCapacity(
		URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
		20);
	if (!TestTrue(
			TEXT("The linked base stores twenty duplicate-cost resources"),
			BaseStorage->StoreDefinitionResource(
				URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
				20)))
	{
		return false;
	}

	URpgCraftingStationComponent* Station = TestWorld.GetStation();
	AActor* RequestingActor = TestWorld.GetRequestingController();
	TestEqual(
		TEXT("Costs of two plus three for the same definition are treated as five per unit"),
		Station->GetMaxCraftableQuantity(RequestingActor, DuplicateCostRecipe),
		4);
	TestTrue(
		TEXT("The aggregated resource maximum is accepted"),
		Station->CanCraftRecipeQuantity(RequestingActor, DuplicateCostRecipe, 4));
	TestFalse(
		TEXT("A quantity that only passes the individual duplicate entries is rejected"),
		Station->CanCraftRecipeQuantity(RequestingActor, DuplicateCostRecipe, 5));

	if (!TestTrue(
			TEXT("The authoritative queue consumes the exact aggregated batch cost"),
			Station->QueueCraftRecipe(RequestingActor, DuplicateCostRecipe, 4)))
	{
		return false;
	}
	TestEqual(
		TEXT("The aggregated batch consumes all twenty resources"),
		BaseStorage->GetResourceCount(
			URpgInventoryAutomationTestMaterialDefinition::StaticClass()),
		0);

	const TArray<FRpgCraftingJobEntry> Jobs = Station->GetCraftingJobs();
	if (!TestEqual(TEXT("Exactly one aggregated-cost job was queued"), Jobs.Num(), 1))
	{
		return false;
	}

	TestTrue(
		TEXT("Canceling the batch refunds the aggregated resource credit"),
		Station->CancelCraftJob(RequestingActor, Jobs[0].JobId));
	TestEqual(
		TEXT("The complete aggregated batch cost is restored"),
		BaseStorage->GetResourceCount(
			URpgInventoryAutomationTestMaterialDefinition::StaticClass()),
		20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingRuntimeStateBaseStorageBoundaryTest,
	"SurvivalRpg.Crafting.Output.RuntimeStateBaseStorageBoundary",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCraftingRuntimeStateBaseStorageBoundaryTest::RunTest(
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
	ARpgBaseCampActor* BaseCamp =
		TestWorld.CreateLinkedBaseCamp(*this);
	URpgBaseStorageComponent* BaseStorage =
		BaseCamp ? BaseCamp->GetBaseStorageComponent() : nullptr;
	if (!TestNotNull(
			TEXT("The state-boundary output inventory exists"),
			OutputInventory) ||
		!TestNotNull(
			TEXT("The state-boundary base storage exists"),
			BaseStorage))
	{
		return false;
	}

	const TSubclassOf<URpgInventoryItemDefinition> PlainMaterialDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	BaseStorage->AddResourceCapacity(PlainMaterialDefinition, 20);
	URpgInventoryItemInstance* DefaultMaterial =
		OutputInventory->AddItemDefinition(PlainMaterialDefinition, 3);
	if (!TestNotNull(
		TEXT("A default-state material output exists"),
		DefaultMaterial))
	{
		return false;
	}
	TestTrue(
		TEXT("Base storage accepts a concrete reproducible default state"),
		BaseStorage->CanStoreResourceInstance(DefaultMaterial, 3));
	TestTrue(
		TEXT("Default-state crafting output is projected into the base pool"),
		Station->FlushOutputToBaseStorage());
	TestEqual(
		TEXT("The default material count reaches base storage"),
		BaseStorage->GetResourceCount(PlainMaterialDefinition),
		3);
	TestEqual(
		TEXT("The projected default material leaves no concrete output entry"),
		OutputInventory->GetUsedEntryCount(),
		0);

	URpgInventoryItemInstance* VariantMaterial =
		OutputInventory->AddItemDefinition(PlainMaterialDefinition, 2);
	if (!TestNotNull(
			TEXT("A concrete runtime-state variant exists"),
			VariantMaterial))
	{
		return false;
	}
	VariantMaterial->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		71);

	const TArray<FRpgInventoryEntryView> EntriesBefore =
		OutputInventory->GetAllEntries();
	if (!TestEqual(
		TEXT("Exactly one variant output entry is present"),
		EntriesBefore.Num(),
		1))
	{
		return false;
	}
	const FRpgInventoryEntryView EntryBefore = EntriesBefore[0];
	const int32 RevisionBefore = OutputInventory->GetInventoryRevision();
	FRpgInventoryStackKey KeyBefore;
	if (!TestTrue(
		TEXT("The variant output has a valid canonical key"),
		VariantMaterial->TryBuildStackKey(KeyBefore)))
	{
		return false;
	}

	TestFalse(
		TEXT("Base storage rejects a concrete non-default runtime state"),
		BaseStorage->CanStoreResourceInstance(VariantMaterial, 2));
	TestFalse(
		TEXT("Crafting flush reports no movement for the rejected variant"),
		Station->FlushOutputToBaseStorage());
	TestEqual(
		TEXT("Rejected variant does not change the base resource count"),
		BaseStorage->GetResourceCount(PlainMaterialDefinition),
		3);
	TestEqual(
		TEXT("Rejected variant does not advance the output revision"),
		OutputInventory->GetInventoryRevision(),
		RevisionBefore);
	TestTrue(
		TEXT("Rejected variant preserves its persistent identity and UObject"),
		OutputInventory->FindItemById(EntryBefore.ItemId) ==
			VariantMaterial);
	TestTrue(
		TEXT("Rejected variant preserves its replicated entry identity"),
		OutputInventory->ContainsEntry(EntryBefore.EntryId));
	TestEqual(
		TEXT("Rejected variant preserves its exact quantity"),
		OutputInventory->GetItemStackCount(VariantMaterial),
		EntryBefore.StackCount);
	FRpgInventoryGridPlacement PlacementAfter;
	TestTrue(
		TEXT("Rejected variant preserves its exact placement"),
		OutputInventory->GetItemPlacement(
			VariantMaterial,
			PlacementAfter) &&
			PlacementAfter == EntryBefore.Placement);
	FRpgInventoryStackKey KeyAfter;
	TestTrue(
		TEXT("Rejected variant preserves all fragment runtime bytes"),
		VariantMaterial->TryBuildStackKey(KeyAfter) &&
			KeyAfter == KeyBefore &&
			VariantMaterial->GetStatTagStackCount(
				RpgGameplayTags::Ability_Attack_Basic) == 71);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingContainerProviderBaseStorageBoundaryTest,
	"SurvivalRpg.Crafting.Output.ContainerProviderBaseStorageBoundary",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCraftingContainerProviderBaseStorageBoundaryTest::RunTest(
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
	ARpgBaseCampActor* BaseCamp =
		TestWorld.CreateLinkedBaseCamp(*this);
	URpgBaseStorageComponent* BaseStorage =
		BaseCamp ? BaseCamp->GetBaseStorageComponent() : nullptr;
	URpgInventoryManagerComponent* ArmoryInventory =
		BaseCamp ? BaseCamp->GetArmoryInventoryComponent() : nullptr;
	if (!TestNotNull(
			TEXT("The provider-boundary output inventory exists"),
			OutputInventory) ||
		!TestNotNull(
			TEXT("The provider-boundary base storage exists"),
			BaseStorage) ||
		!TestNotNull(
			TEXT("The provider-boundary armory exists"),
			ArmoryInventory))
	{
		return false;
	}

	const TSubclassOf<URpgInventoryItemDefinition> ProviderDefinition =
		URpgInventoryAutomationTestMaterialContainerDefinition::
			StaticClass();
	BaseStorage->AddResourceCapacity(ProviderDefinition, 10);
	URpgInventoryItemInstance* Provider =
		OutputInventory->AddItemDefinition(ProviderDefinition, 1);
	if (!TestNotNull(
			TEXT("A material container provider exists"),
			Provider))
	{
		return false;
	}

	FRpgInventoryGridPlacement RequestedChildPlacement;
	RequestedChildPlacement.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeItemOwned(
			Provider->GetItemId(),
			TEXT("Main"),
			1));
	RequestedChildPlacement.X = 0;
	RequestedChildPlacement.Y = 0;
	RequestedChildPlacement.Width = 1;
	RequestedChildPlacement.Height = 1;
	URpgInventoryItemInstance* Child =
		OutputInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			RequestedChildPlacement);
	if (!TestNotNull(
			TEXT("The material provider owns one concrete child"),
			Child))
	{
		return false;
	}
	Child->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 3);
	const FRpgInventoryItemId ProviderItemId = Provider->GetItemId();
	const FRpgInventoryItemId ChildItemId = Child->GetItemId();
	const int32 RevisionBefore =
		OutputInventory->GetInventoryRevision();

	TestFalse(
		TEXT("A container provider cannot collapse into definition/count"),
		Provider->CanCollapseIntoDefinitionCount());
	TestFalse(
		TEXT("Base storage rejects a material container before consume"),
		BaseStorage->CanStoreResourceInstance(Provider, 1));
	TestTrue(
		TEXT("Crafting flush preserves the concrete provider by routing it to the Armory"),
		Station->FlushOutputToBaseStorage());
	TestEqual(
		TEXT("Rejected provider creates no base resource credit"),
		BaseStorage->GetResourceCount(ProviderDefinition),
		0);
	TestEqual(
		TEXT("The exact cross-inventory transfer advances the output revision once"),
		OutputInventory->GetInventoryRevision(),
		RevisionBefore + 1);
	TestEqual(
		TEXT("The transferred provider subtree leaves the output graph atomically"),
		OutputInventory->GetUsedEntryCount(),
		0);

	URpgInventoryItemInstance* TransferredProvider =
		ArmoryInventory->FindItemById(ProviderItemId);
	URpgInventoryItemInstance* TransferredChild =
		ArmoryInventory->FindItemById(ChildItemId);
	if (!TestNotNull(
			TEXT("Armory receives the provider's persistent identity"),
			TransferredProvider) ||
		!TestNotNull(
			TEXT("Armory receives the child's persistent identity"),
			TransferredChild))
	{
		return false;
	}
	TestEqual(
		TEXT("Armory receives the complete provider subtree"),
		ArmoryInventory->GetUsedEntryCount(),
		2);
	TestTrue(
		TEXT("Cross-actor transfer re-owns concrete instances without reusing source UObjects"),
		TransferredProvider != Provider && TransferredChild != Child);
	TestEqual(
		TEXT("Transferred child preserves its runtime stat payload"),
		TransferredChild->GetStatTagStackCount(
			RpgGameplayTags::Ability_Attack_Basic),
		3);
	FRpgInventoryGridPlacement ProviderPlacementAfter;
	FRpgInventoryGridPlacement ChildPlacementAfter;
	TestTrue(
		TEXT("Transferred provider occupies the Armory root"),
		ArmoryInventory->GetItemPlacement(
			TransferredProvider,
			ProviderPlacementAfter) &&
			!ProviderPlacementAfter.GetContainerHandle().IsItemOwned());
	TestTrue(
		TEXT("Transferred child remains inside the same persistent provider identity"),
		ArmoryInventory->GetItemPlacement(
			TransferredChild,
			ChildPlacementAfter) &&
			ChildPlacementAfter.GetContainerHandle().IsItemOwned() &&
			ChildPlacementAfter.GetContainerHandle().ItemOwnerId ==
				ProviderItemId &&
			ArmoryInventory->GetItemStackCount(TransferredChild) == 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
