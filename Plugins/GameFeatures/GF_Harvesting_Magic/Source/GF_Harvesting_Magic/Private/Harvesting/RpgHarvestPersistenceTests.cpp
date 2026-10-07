#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestPersistenceComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "UObject/UnrealType.h"

namespace RpgHarvestPersistenceTests
{
	using namespace RpgHarvestAutomation;

	constexpr int32 SectionCount = 3;
	constexpr float RespawnSeconds = 100.0f;
	const FName OtherMapId(TEXT("/Game/Automation/OtherMap"));

	URpgHarvestProfile* MakeProfile(UObject* Outer)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgHarvestAutomationTestStackItemDefinition::StaticClass();
		Entry.MinimumQuantity = 1;
		Entry.MaximumQuantity = 1;
		Entry.ChancePercent = 100.0f;

		Profile->LootTable = Table;
		Profile->SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Profile->MinimumSkillLevel = 1;
		Profile->SectionCount = SectionCount;
		Profile->MinimumRespawnSeconds = RespawnSeconds;
		Profile->MaximumRespawnSeconds = RespawnSeconds;
		return Profile;
	}

	/** Adds the persistence component to World's GameState, the way the harvesting GameFeature does on the server. */
	URpgHarvestPersistenceComponent* AddPersistence(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		AGameStateBase* GameState = World->GetGameState();
		if (!GameState)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags = RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GameState = World->SpawnActor<AGameStateBase>(SpawnParameters);
			if (!GameState)
			{
				return nullptr;
			}
			World->SetGameState(GameState);
		}
		if (!GameState->HasActorBegunPlay())
		{
			GameState->DispatchBeginPlay();
		}

		URpgHarvestPersistenceComponent* Persistence =
			NewObject<URpgHarvestPersistenceComponent>(GameState, TEXT("HarvestPersistence"));
		Persistence->RegisterComponent();
		return Persistence;
	}

	/** Spawns a node named Name; bLoadedWithMap marks it like an actor loaded with its map. */
	ARpgHarvestAutomationNodeActor* SpawnNode(UWorld* World, URpgHarvestProfile* Profile, const FName Name, const bool bLoadedWithMap)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = Name;
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.bDeferConstruction = true;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARpgHarvestAutomationNodeActor* Node = World ? World->SpawnActor<ARpgHarvestAutomationNodeActor>(SpawnParameters) : nullptr;
		FObjectProperty* ProfileProperty =
			FindFProperty<FObjectProperty>(URpgHarvestableComponent::StaticClass(), TEXT("HarvestProfile"));
		if (!Node || !Node->HarvestableNode || !ProfileProperty)
		{
			return nullptr;
		}

		ProfileProperty->SetObjectPropertyValue_InContainer(Node->HarvestableNode, Profile);
		Node->bNetStartup = bLoadedWithMap;
		Node->FinishSpawning(FTransform::Identity);
		if (!Node->HasActorBegunPlay())
		{
			Node->DispatchBeginPlay();
		}
		return Node;
	}

	ARpgHarvestAutomationTestPlayerState* SpawnHarvester(UWorld* World)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World ? World->SpawnActor<ARpgHarvestAutomationTestPlayerState>(SpawnParameters) : nullptr;
	}

	bool HarvestNode(ARpgHarvestAutomationNodeActor* Node, AActor* Harvester, const int32 Sections)
	{
		if (!Node || !Node->HarvestableNode)
		{
			return false;
		}
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.RequestedSections = Sections;
		Request.Hit = FHitResult(Node, nullptr, FVector(0.0, 0.0, 50.0), FVector::UpVector);
		Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Node->HarvestableNode, Request.Hit);
		return Node->HarvestableNode->CommitHarvest_Implementation(Request).IsSuccess();
	}

	/** Moves the entry of FromMap to ToMap, as if the same map were loaded again, and adds an entry for another map. */
	bool MoveToMap(const FRpgWorldFeatureSaveData& Saved, const FName FromMap, const FName ToMap, FRpgWorldFeatureSaveData& OutMoved)
	{
		FRpgHarvestSaveData Data;
		FRpgHarvestSavedMap Map;
		if (!URpgHarvestPersistenceComponent::ReadPayload(Saved, Data) || !Data.Maps.RemoveAndCopyValue(FromMap, Map))
		{
			return false;
		}
		Data.Maps.Add(ToMap, Map);

		FRpgHarvestSavedNode& Elsewhere = Data.Maps.Add(OtherMapId).Nodes.AddDefaulted_GetRef();
		Elsewhere.NodeId = TEXT("ElsewhereNode");
		Elsewhere.Stock.HarvestedSections = 2;
		return URpgHarvestPersistenceComponent::WritePayload(Data, OutMoved);
	}

	const FRpgHarvestSavedNode* FindNode(const FRpgHarvestSavedMap& Map, const FName NodeId)
	{
		return Map.Nodes.FindByPredicate([NodeId](const FRpgHarvestSavedNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPersistenceNodeTest,
	"SurvivalRpg.Harvesting.Persistence.NodeStockSurvivesReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPersistenceNodeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestPersistenceTests;

	const FName PartialId(TEXT("Persist_PartialVein"));
	const FName DepletedId(TEXT("Persist_DepletedVein"));
	FRpgWorldFeatureSaveData Saved;
	FName FirstMapId;
	{
		FScopedTestWorld TestWorld;
		UWorld* World = TestWorld.GetWorld();
		URpgHarvestPersistenceComponent* Persistence = AddPersistence(World);
		URpgHarvestProfile* Profile = MakeProfile(World);
		ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
		ARpgHarvestAutomationNodeActor* Partial = SpawnNode(World, Profile, PartialId, true);
		ARpgHarvestAutomationNodeActor* Depleted = SpawnNode(World, Profile, DepletedId, true);
		ARpgHarvestAutomationNodeActor* Spawned = SpawnNode(World, Profile, TEXT("Persist_RuntimeVein"), false);
		if (!TestNotNull(TEXT("Persistence"), Persistence) || !TestNotNull(TEXT("Harvester"), Harvester) ||
			!TestNotNull(TEXT("Partial node"), Partial) || !TestNotNull(TEXT("Depleted node"), Depleted) ||
			!TestNotNull(TEXT("Runtime node"), Spawned))
		{
			return false;
		}
		TestWorld.PrimeTimerManager();
		FirstMapId = URpgHarvestPersistenceComponent::MakeMapId(*World);

		TestEqual(TEXT("Untouched resources need no record"), Persistence->GetNumRecordedResources(), 0);
		TestTrue(TEXT("One section from the partial node"), HarvestNode(Partial, Harvester, 1));
		TestTrue(TEXT("The depleted node is emptied"), HarvestNode(Depleted, Harvester, SectionCount));
		TestTrue(TEXT("The runtime node is harvested"), HarvestNode(Spawned, Harvester, 1));
		TestEqual(TEXT("Only map-placed nodes are recorded"), Persistence->GetNumRecordedResources(), 2);

		TestWorld.AdvanceTimers(30.0f);
		if (!TestTrue(TEXT("The stock is captured"), Persistence->CaptureWorldSaveData(Saved)))
		{
			return false;
		}

		FRpgHarvestSaveData Data;
		const FRpgHarvestSavedMap* Map = URpgHarvestPersistenceComponent::ReadPayload(Saved, Data)
			? Data.Maps.Find(FirstMapId)
			: nullptr;
		if (!TestNotNull(TEXT("The payload holds this map"), Map))
		{
			return false;
		}
		TestEqual(TEXT("Two nodes are saved"), Map->Nodes.Num(), 2);
		const FRpgHarvestSavedNode* SavedDepleted = FindNode(*Map, DepletedId);
		if (TestNotNull(TEXT("The depleted node is saved by its actor name"), SavedDepleted))
		{
			TestFalse(TEXT("It is saved depleted"), SavedDepleted->Stock.bActive);
			TestTrue(
				TEXT("It keeps its remaining respawn time"),
				FMath::IsNearlyEqual(SavedDepleted->Stock.RespawnSeconds, RespawnSeconds - 30.0f, 0.1f));
		}
	}

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestProfile* Profile = MakeProfile(World);
	URpgHarvestAutomationNodeStateListener* Listener = NewObject<URpgHarvestAutomationNodeStateListener>();
	ARpgHarvestAutomationNodeActor* Partial = SpawnNode(World, Profile, PartialId, true);
	ARpgHarvestAutomationNodeActor* Depleted = SpawnNode(World, Profile, DepletedId, true);
	if (!TestNotNull(TEXT("Reloaded partial node"), Partial) || !TestNotNull(TEXT("Reloaded depleted node"), Depleted))
	{
		return false;
	}
	Depleted->HarvestableNode->OnHarvestStateChanged.AddDynamic(
		Listener,
		&URpgHarvestAutomationNodeStateListener::HandleStateChanged);

	// The GameFeature adds the component after the map's resources began play.
	URpgHarvestPersistenceComponent* Persistence = AddPersistence(World);
	FRpgWorldFeatureSaveData Reloaded;
	if (!TestNotNull(TEXT("Reloaded persistence"), Persistence) ||
		!TestTrue(TEXT("The save is moved to the reloaded map"), MoveToMap(Saved, FirstMapId, URpgHarvestPersistenceComponent::MakeMapId(*World), Reloaded)))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	if (!TestTrue(TEXT("The saved stock is restored"), Persistence->RestoreWorldSaveData(&Reloaded)))
	{
		return false;
	}

	TestEqual(TEXT("The partial node keeps its remaining sections"), Partial->HarvestableNode->GetRemainingSections(), SectionCount - 1);
	TestTrue(TEXT("The partial node stays active"), Partial->HarvestableNode->GetHarvestState().bActive);
	TestFalse(TEXT("The depleted node stays depleted"), Depleted->HarvestableNode->GetHarvestState().bActive);
	TestTrue(TEXT("The restored stock is presented as initial state"), Listener->bLastInitialState);
	TestEqual(TEXT("Both nodes are recorded again"), Persistence->GetNumRecordedResources(), 2);

	TestWorld.AdvanceTimers(RespawnSeconds - 30.0f - 1.0f);
	TestFalse(TEXT("The respawn countdown continues where it stopped"), Depleted->HarvestableNode->GetHarvestState().bActive);
	TestWorld.AdvanceTimers(2.0f);
	TestEqual(TEXT("The node respawns after its remaining time"), Depleted->HarvestableNode->GetRemainingSections(), SectionCount);
	TestEqual(TEXT("A respawned node drops its record"), Persistence->GetNumRecordedResources(), 1);

	FRpgWorldFeatureSaveData Recaptured;
	FRpgHarvestSaveData Data;
	if (TestTrue(TEXT("The stock is captured again"), Persistence->CaptureWorldSaveData(Recaptured)) &&
		TestTrue(TEXT("The payload reads"), URpgHarvestPersistenceComponent::ReadPayload(Recaptured, Data)))
	{
		const FRpgHarvestSavedMap* OtherMap = Data.Maps.Find(OtherMapId);
		TestTrue(
			TEXT("The entry of a map that is not loaded is written back unchanged"),
			OtherMap && OtherMap->Nodes.Num() == 1 && OtherMap->Nodes[0].Stock.HarvestedSections == 2);
	}

	TestTrue(TEXT("Restoring without an entry is accepted"), Persistence->RestoreWorldSaveData(nullptr));
	TestEqual(TEXT("It returns the partial node to its authored stock"), Partial->HarvestableNode->GetRemainingSections(), SectionCount);
	TestEqual(TEXT("It drops every record"), Persistence->GetNumRecordedResources(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPersistenceInstancesTest,
	"SurvivalRpg.Harvesting.Persistence.InstanceStockSurvivesReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPersistenceInstancesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestPersistenceTests;

	const TArray<FVector> Locations = {FVector(0.0, 0.0, 0.0), FVector(200.0, 0.0, 0.0), FVector(400.0, 0.0, 0.0)};
	const FVector FieldLocation(1000.0, 500.0, 0.0);
	const FVector RuntimeFieldLocation(-3000.0, 0.0, 0.0);
	FRpgWorldFeatureSaveData Saved;
	FName FirstMapId;
	{
		FScopedTestWorld TestWorld;
		UWorld* World = TestWorld.GetWorld();
		URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
		URpgHarvestPersistenceComponent* Persistence = AddPersistence(World);
		ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
		ARpgHarvestAutomationInstancesActor* MapField = SpawnInstances(World, MakeProfile(World), Locations, FieldLocation);
		ARpgHarvestAutomationInstancesActor* RuntimeField = SpawnInstances(World, MakeProfile(World), Locations, RuntimeFieldLocation);
		if (!TestNotNull(TEXT("Stock"), Stock) || !TestNotNull(TEXT("Persistence"), Persistence) ||
			!TestNotNull(TEXT("Harvester"), Harvester) || !TestNotNull(TEXT("Map field"), MapField) ||
			!TestNotNull(TEXT("Runtime field"), RuntimeField))
		{
			return false;
		}
		MapField->bNetStartup = true;
		TestWorld.PrimeTimerManager();
		FirstMapId = URpgHarvestPersistenceComponent::MakeMapId(*World);

		TestTrue(
			TEXT("The first instance is emptied"),
			MapField->Instances->CommitHarvest_Implementation(MakeInstanceRequest(MapField->Instances, 0, Harvester, SectionCount)).bDepleted);
		TestTrue(
			TEXT("The third instance is harvested once"),
			MapField->Instances->CommitHarvest_Implementation(MakeInstanceRequest(MapField->Instances, 2, Harvester)).IsSuccess());
		TestTrue(
			TEXT("A runtime field is harvested"),
			RuntimeField->Instances->CommitHarvest_Implementation(MakeInstanceRequest(RuntimeField->Instances, 0, Harvester)).IsSuccess());
		TestEqual(TEXT("The stock holds three changed instances"), Stock->GetNumChangedInstances(), 3);
		TestEqual(TEXT("Only instances loaded with the map are recorded"), Persistence->GetNumRecordedResources(), 2);

		TestWorld.AdvanceTimers(40.0f);
		if (!TestTrue(TEXT("The stock is captured"), Persistence->CaptureWorldSaveData(Saved)))
		{
			return false;
		}
	}

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestPersistenceComponent* Persistence = AddPersistence(World);
	FRpgWorldFeatureSaveData Reloaded;
	if (!TestNotNull(TEXT("Reloaded persistence"), Persistence) ||
		!TestTrue(TEXT("The save is moved to the reloaded map"), MoveToMap(Saved, FirstMapId, URpgHarvestPersistenceComponent::MakeMapId(*World), Reloaded)) ||
		!TestTrue(TEXT("The saved stock is restored before the stock exists"), Persistence->RestoreWorldSaveData(&Reloaded)))
	{
		return false;
	}

	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	if (!TestNotNull(TEXT("A later stock"), Stock))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	TestEqual(TEXT("The stock takes the saved instances when it begins play"), Stock->GetNumChangedInstances(), 2);

	ARpgHarvestAutomationInstancesActor* MapField = SpawnInstances(World, MakeProfile(World), Locations, FieldLocation);
	if (!TestNotNull(TEXT("The field streams in"), MapField))
	{
		return false;
	}
	URpgHarvestAutomationInstancesComponent* Instances = MapField->Instances;
	TestEqual(TEXT("The emptied instance streams in empty"), Instances->GetRemainingSections(0), 0);
	TestEqual(TEXT("The untouched instance streams in full"), Instances->GetRemainingSections(1), SectionCount);
	TestEqual(TEXT("The harvested instance keeps its sections"), Instances->GetRemainingSections(2), SectionCount - 1);
	TestTrue(TEXT("The saved stock is presented as initial state"), Instances->bLastInitialState);

	TestWorld.AdvanceTimers(RespawnSeconds - 40.0f - 1.0f);
	TestEqual(TEXT("The respawn countdown continues where it stopped"), Instances->GetRemainingSections(0), 0);
	TestWorld.AdvanceTimers(2.0f);
	TestEqual(TEXT("The instance respawns after its remaining time"), Instances->GetRemainingSections(0), SectionCount);
	TestEqual(TEXT("A respawned instance drops its record"), Persistence->GetNumRecordedResources(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPersistenceInvalidTest,
	"SurvivalRpg.Harvesting.Persistence.InvalidSaveIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPersistenceInvalidTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestPersistenceTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestPersistenceComponent* Persistence = AddPersistence(World);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeProfile(World), TEXT("Persist_GuardedVein"), true);
	if (!TestNotNull(TEXT("Persistence"), Persistence) || !TestNotNull(TEXT("Node"), Node) ||
		!TestTrue(TEXT("The node is harvested"), HarvestNode(Node, Harvester, 1)))
	{
		return false;
	}

	FRpgWorldFeatureSaveData UnknownSchema;
	TestTrue(TEXT("A valid payload is written"), URpgHarvestPersistenceComponent::WritePayload(FRpgHarvestSaveData(), UnknownSchema));
	UnknownSchema.SchemaVersion = URpgHarvestPersistenceComponent::CurrentSchemaVersion + 1;
	AddExpectedError(TEXT("Saved harvest state has schema"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("An unknown schema is rejected"), Persistence->RestoreWorldSaveData(&UnknownSchema));

	FRpgHarvestSaveData Unnamed;
	Unnamed.Maps.Add(URpgHarvestPersistenceComponent::MakeMapId(*World)).Nodes.AddDefaulted_GetRef().Stock.HarvestedSections = 1;
	FRpgWorldFeatureSaveData UnnamedPayload;
	TestTrue(TEXT("The unnamed node payload is written"), URpgHarvestPersistenceComponent::WritePayload(Unnamed, UnnamedPayload));
	AddExpectedError(TEXT("is invalid"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A node without an id is rejected"), Persistence->RestoreWorldSaveData(&UnnamedPayload));

	TestEqual(TEXT("A rejected save leaves the stock untouched"), Node->HarvestableNode->GetRemainingSections(), SectionCount - 1);
	TestEqual(TEXT("A rejected save keeps the records"), Persistence->GetNumRecordedResources(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
