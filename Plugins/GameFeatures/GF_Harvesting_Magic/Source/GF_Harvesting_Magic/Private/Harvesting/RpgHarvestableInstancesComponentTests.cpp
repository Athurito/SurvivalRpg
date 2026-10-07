#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include "Components/InstancedSkinnedMeshComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

namespace RpgHarvestableInstancesTests
{
	using namespace RpgHarvestAutomation;

	constexpr int32 YieldPerSection = 2;
	constexpr int32 ExperiencePerSection = 10;
	constexpr float RespawnSeconds = 30.0f;

	URpgHarvestProfile* MakeProfile(UObject* Outer, const int32 SectionCount, const float InRespawnSeconds = RespawnSeconds)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgHarvestAutomationTestStackItemDefinition::StaticClass();
		Entry.MinimumQuantity = YieldPerSection;
		Entry.MaximumQuantity = YieldPerSection;
		Entry.ChancePercent = 100.0f;

		Profile->LootTable = Table;
		Profile->SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = ExperiencePerSection;
		Profile->SectionCount = SectionCount;
		Profile->MinimumRespawnSeconds = InRespawnSeconds;
		Profile->MaximumRespawnSeconds = InRespawnSeconds;
		return Profile;
	}

	ARpgHarvestAutomationTestPlayerState* SpawnHarvester(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			ARpgHarvestAutomationTestPlayerState::StaticClass(),
			TEXT("InstanceHarvesterState"));
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ARpgHarvestAutomationTestPlayerState>(SpawnParameters);
	}

	int32 CountMaterial(const ARpgHarvestAutomationTestPlayerState* Harvester)
	{
		const URpgInventoryManagerComponent* Inventory =
			Harvester ? Harvester->GetInventoryManagerComponent() : nullptr;
		return Inventory
			? Inventory->GetTotalItemCountByDefinition(URpgHarvestAutomationTestStackItemDefinition::StaticClass())
			: 0;
	}

	float GetMiningXP(const ARpgHarvestAutomationTestPlayerState* Harvester)
	{
		const URpgTradeSkillProgressionComponent* TradeSkills =
			Harvester ? Harvester->GetTradeSkillProgressionComponent() : nullptr;
		return TradeSkills ? TradeSkills->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Mining) : 0.0f;
	}

	/** Returns the current presented scale of InstanceIndex along X. */
	double GetPresentedScale(const URpgHarvestableInstancesComponent* Instances, const int32 InstanceIndex)
	{
		FTransform InstanceTransform;
		return Instances && Instances->GetInstanceTransform(InstanceIndex, InstanceTransform, false)
			? InstanceTransform.GetScale3D().X
			: -1.0;
	}

	const TArray<FVector> ThreeInstanceLocations = {
		FVector(0.0, 0.0, 0.0),
		FVector(200.0, 0.0, 0.0),
		FVector(400.0, 0.0, 0.0)};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestInstancesSharedStockTest,
	"SurvivalRpg.Harvesting.Instances.SectionsUseSparseSharedStock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestInstancesSharedStockTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancesTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Actor = SpawnInstances(World, MakeProfile(World, 4), ThreeInstanceLocations);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Instances exist"), Actor) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	URpgHarvestAutomationInstancesComponent* Instances = Actor->Instances;
	TestEqual(TEXT("Every instance is addressable by key"), Instances->GetNumKeyedInstances(), 3);
	TestEqual(TEXT("Untouched instances store nothing"), Stock->GetNumChangedInstances(), 0);
	TestEqual(TEXT("Untouched instances report the authored stock"), Instances->GetRemainingSections(1), 4);

	TestWorld.PrimeTimerManager();
	const FRpgHarvestResult Preview = IRpgHarvestableTarget::Execute_EvaluateHarvest(
		Instances,
		MakeInstanceRequest(Instances, 1, Harvester));
	TestTrue(
		TEXT("An evaluation previews one section without extracting it"),
		Preview.IsSuccess() && Preview.SectionsTaken == 1 && Preview.RemainingSections == 3);
	TestEqual(TEXT("An evaluation stores nothing"), Stock->GetNumChangedInstances(), 0);

	TestTrue(
		TEXT("A first harvest extracts one section"),
		Instances->CommitHarvest_Implementation(MakeInstanceRequest(Instances, 1, Harvester)).IsSuccess());
	TestTrue(
		TEXT("A second harvester shares the same stock"),
		Instances->CommitHarvest_Implementation(MakeInstanceRequest(Instances, 1, Harvester)).IsSuccess());
	TestEqual(TEXT("Two sections remain on the harvested instance"), Instances->GetRemainingSections(1), 2);
	TestEqual(TEXT("Neighbouring instances keep their stock"), Instances->GetRemainingSections(0), 4);
	TestEqual(TEXT("Only the harvested instance is stored"), Stock->GetNumChangedInstances(), 1);
	TestEqual(TEXT("Each section is rewarded"), CountMaterial(Harvester), 2 * YieldPerSection);
	TestTrue(
		TEXT("Each section awards experience"),
		FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(2 * ExperiencePerSection)));
	TestEqual(TEXT("Partial harvests are presented"), Instances->EventCount, 2);
	TestTrue(
		TEXT("Live changes are not initial state"),
		Instances->LastInstanceIndex == 1 && Instances->LastRemainingSections == 2 && !Instances->bLastInitialState);
	TestTrue(TEXT("A partially harvested instance stays visible"), FMath::IsNearlyEqual(GetPresentedScale(Instances, 1), 1.0));

	const FRpgHarvestRequest PreDepletionRequest = MakeInstanceRequest(Instances, 1, Harvester);
	const FRpgHarvestResult Depleting =
		Instances->CommitHarvest_Implementation(MakeInstanceRequest(Instances, 1, Harvester, 3));
	TestTrue(TEXT("An oversized request is clamped to the remaining stock"), Depleting.IsSuccess() && Depleting.SectionsTaken == 2);
	TestTrue(TEXT("The last section depletes the instance"), Depleting.bDepleted);
	TestEqual(TEXT("A depleted instance has no stock"), Instances->GetRemainingSections(1), 0);
	TestEqual(TEXT("The whole stock was rewarded once"), CountMaterial(Harvester), 4 * YieldPerSection);
	TestTrue(TEXT("The depleted instance is hidden"), FMath::IsNearlyZero(GetPresentedScale(Instances, 1)));
	FTransform AuthoredTransform;
	TestTrue(
		TEXT("Hiding keeps the authored transform"),
		Instances->GetAuthoredInstanceTransform(1, AuthoredTransform, false) &&
			AuthoredTransform.GetScale3D().Equals(FVector::OneVector));
	TestTrue(
		TEXT("A depleted instance rejects further harvests"),
		Instances->CommitHarvest_Implementation(MakeInstanceRequest(Instances, 1, Harvester)).Outcome ==
			ERpgHarvestOutcome::Depleted);
	TestTrue(
		TEXT("A request selected before depletion is stale"),
		IRpgHarvestableTarget::Execute_GetHarvestRevision(Instances, PreDepletionRequest.Hit) != PreDepletionRequest.ExpectedRevision);

	TestWorld.AdvanceTimers(RespawnSeconds + 1.0f);
	TestEqual(TEXT("Respawn restores the complete stock"), Instances->GetRemainingSections(1), 4);
	TestEqual(TEXT("A restored instance stores nothing again"), Stock->GetNumChangedInstances(), 0);
	TestTrue(TEXT("A restored instance is shown again"), FMath::IsNearlyEqual(GetPresentedScale(Instances, 1), 1.0));
	TestTrue(TEXT("The respawn is presented"), Instances->bLastActive && Instances->LastRemainingSections == 4);
	TestTrue(
		TEXT("A restored instance is harvestable again"),
		Instances->CommitHarvest_Implementation(MakeInstanceRequest(Instances, 1, Harvester)).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestInstancesStreamingTest,
	"SurvivalRpg.Harvesting.Instances.StreamingAndLateStockRestoreState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestInstancesStreamingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancesTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	const FVector FieldLocation(1000.0, 500.0, 0.0);
	ARpgHarvestAutomationInstancesActor* EarlyActor =
		SpawnInstances(World, MakeProfile(World, 4), ThreeInstanceLocations, FieldLocation);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Instances exist before the stock"), EarlyActor) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	TestTrue(
		TEXT("Without the stock component instances reject harvests"),
		EarlyActor->Instances->CommitHarvest_Implementation(MakeInstanceRequest(EarlyActor->Instances, 0, Harvester)).Outcome ==
			ERpgHarvestOutcome::Invalid);

	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	if (!TestNotNull(TEXT("A later GameFeature adds the stock"), Stock))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	TestTrue(
		TEXT("The stock registers instances that began play earlier"),
		EarlyActor->Instances->CommitHarvest_Implementation(MakeInstanceRequest(EarlyActor->Instances, 0, Harvester, 4)).bDepleted);
	TestTrue(
		TEXT("A partial harvest on another instance"),
		EarlyActor->Instances->CommitHarvest_Implementation(MakeInstanceRequest(EarlyActor->Instances, 2, Harvester)).IsSuccess());
	TestTrue(TEXT("The registered instances present the depletion"), FMath::IsNearlyZero(GetPresentedScale(EarlyActor->Instances, 0)));
	TestEqual(TEXT("Two instances are stored"), Stock->GetNumChangedInstances(), 2);

	// Streaming out ends play; the stored stock outlives the representation.
	EarlyActor->Destroy();
	TestEqual(TEXT("Streaming out keeps the stored stock"), Stock->GetNumChangedInstances(), 2);

	ARpgHarvestAutomationInstancesActor* StreamedActor =
		SpawnInstances(World, MakeProfile(World, 4), ThreeInstanceLocations, FieldLocation);
	if (!TestNotNull(TEXT("Instances stream in again"), StreamedActor))
	{
		return false;
	}
	URpgHarvestAutomationInstancesComponent* Streamed = StreamedActor->Instances;
	TestEqual(TEXT("Only changed instances are presented on stream-in"), Streamed->EventCount, 2);
	TestTrue(TEXT("Stream-in presents stored stock as initial state"), Streamed->bLastInitialState);
	TestTrue(TEXT("The depleted instance streams in hidden"), FMath::IsNearlyZero(GetPresentedScale(Streamed, 0)));
	TestEqual(TEXT("The depleted instance streams in empty"), Streamed->GetRemainingSections(0), 0);
	TestEqual(TEXT("The partial instance streams in partial"), Streamed->GetRemainingSections(2), 3);
	TestEqual(TEXT("The untouched instance streams in full"), Streamed->GetRemainingSections(1), 4);
	TestTrue(TEXT("Visible instances stay visible"), FMath::IsNearlyEqual(GetPresentedScale(Streamed, 2), 1.0));

	TestWorld.AdvanceTimers(RespawnSeconds + 1.0f);
	TestTrue(
		TEXT("A respawn reaches the streamed-in representation"),
		Streamed->GetRemainingSections(0) == 4 && FMath::IsNearlyEqual(GetPresentedScale(Streamed, 0), 1.0));
	TestEqual(TEXT("Only the partial instance stays stored"), Stock->GetNumChangedInstances(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestInstancesKeyTest,
	"SurvivalRpg.Harvesting.Instances.KeysFollowAuthoredLocations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestInstancesKeyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancesTests;

	TestTrue(
		TEXT("Keys round world locations to whole centimeters"),
		URpgHarvestInstanceStockComponent::MakeInstanceKey(FVector(10.4, -10.6, 0.5)) == FIntVector(10, -11, 1));

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	AddExpectedMessage(
		TEXT("instances sharing a location with another instance"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	ARpgHarvestAutomationInstancesActor* Field = SpawnInstances(
		World,
		MakeProfile(World, 2),
		{FVector(100.0, 0.0, 0.0), FVector(100.0, 0.0, 0.4)},
		FVector(0.0, 0.0, 300.0));
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Instances exist"), Field) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	URpgHarvestAutomationInstancesComponent* Instances = Field->Instances;
	TestEqual(TEXT("Instances within one centimeter share a key"), Instances->GetNumKeyedInstances(), 1);

	FIntVector Key;
	TestTrue(TEXT("Instances report their key"), Instances->GetInstanceKey(0, Key));
	TestTrue(TEXT("Keys use world locations"), Key == FIntVector(100, 0, 300));
	int32 FoundIndex = INDEX_NONE;
	TestTrue(TEXT("A key resolves to its first instance"), Instances->FindInstanceByKey(Key, FoundIndex) && FoundIndex == 0);

	// Presentation scaling never changes the key that identifies an instance.
	TestWorld.PrimeTimerManager();
	TestTrue(TEXT("Presentation scaling succeeds"), Instances->SetInstancePresentationScale(0, 0.5f));
	FIntVector ScaledKey;
	TestTrue(TEXT("A scaled instance keeps its key"), Instances->GetInstanceKey(0, ScaledKey) && ScaledKey == Key);
	TestTrue(
		TEXT("Requests against a scaled instance still harvest"),
		Instances->CommitHarvest_Implementation(MakeInstanceRequest(Instances, 0, Harvester)).IsSuccess());
	TestTrue(TEXT("Restoring the scale succeeds"), Instances->SetInstancePresentationScale(0, 1.0f));
	TestTrue(TEXT("Restoring the scale shows the authored transform"), FMath::IsNearlyEqual(GetPresentedScale(Instances, 0), 1.0));
	TestFalse(TEXT("Invalid instances cannot be scaled"), Instances->SetInstancePresentationScale(7, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestInstancesBudgetTest,
	"SurvivalRpg.Harvesting.Instances.Budget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestInstancesBudgetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancesTests;

	// One streamed cell of a dense resource field and a busy session's worth of changed instances.
	constexpr int32 GridSize = 100;
	constexpr int32 InstanceCount = GridSize * GridSize;
	constexpr int32 ChangedCount = 1000;
	constexpr double Spacing = 150.0;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	// Loot never drops, which isolates the stock bridge from inventory and drop costs that do not depend on it.
	URpgHarvestProfile* Profile = MakeProfile(World, 4, 0.0f);
	Profile->LootTable->Groups[0].GroupChancePercent = 0.0f;
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TArray<FTransform> Transforms;
	Transforms.Reserve(InstanceCount);
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			Transforms.Emplace(FVector(X * Spacing, Y * Spacing, 0.0));
		}
	}

	// Times only the bridge's part of a stream-in: BeginPlay builds the instance keys, registers with the stock, and
	// presents stored stock. Render and physics state are paid by every instanced mesh and are not measured here.
	// A linked field also links visible instances at every point and hides those of stored depletions, like trees.
	auto StreamIn = [World, Profile, Cube, &Transforms](double& OutBridgeMs, const bool bLinked = false)
	{
		ARpgHarvestAutomationInstancesActor* Actor = World->SpawnActorDeferred<ARpgHarvestAutomationInstancesActor>(
			ARpgHarvestAutomationInstancesActor::StaticClass(),
			FTransform::Identity,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor)
		{
			return Actor;
		}
		Actor->Instances->ConfigureProfile(Profile);
		Actor->Instances->SetStaticMesh(Cube);
		Actor->Instances->AddInstances(Transforms, false);

		Actor->FinishSpawning(FTransform::Identity);
		if (bLinked)
		{
			const FName LinkTag(TEXT("BudgetVisual"));
			Actor->Instances->ConfigureLinkedPresentation(LinkTag);
			UInstancedStaticMeshComponent* Visual = NewObject<UInstancedStaticMeshComponent>(Actor, TEXT("BudgetVisual"));
			Visual->ComponentTags.Add(LinkTag);
			Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Visual->SetupAttachment(Actor->GetRootComponent());
			Visual->RegisterComponent();
			Visual->SetStaticMesh(Cube);
			Visual->AddInstances(Transforms, false);
		}
		const double StartSeconds = FPlatformTime::Seconds();
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		OutBridgeMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		return Actor;
	};

	double FirstBridgeMs = 0.0;
	ARpgHarvestAutomationInstancesActor* Field = StreamIn(FirstBridgeMs);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Cube mesh exists"), Cube) ||
		!TestNotNull(TEXT("Resource field exists"), Field) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	TestEqual(TEXT("Every instance is keyed"), Field->Instances->GetNumKeyedInstances(), InstanceCount);

	double StartSeconds = FPlatformTime::Seconds();
	int32 Harvested = 0;
	for (int32 Index = 0; Index < ChangedCount; ++Index)
	{
		Harvested += Field->Instances->CommitHarvest_Implementation(
			MakeInstanceRequest(Field->Instances, Index * 7 % InstanceCount, Harvester)).IsSuccess() ? 1 : 0;
	}
	const double CommitUs = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0 / ChangedCount;
	TestEqual(TEXT("Every budget harvest succeeded"), Harvested, ChangedCount);
	TestEqual(TEXT("Every harvested instance is stored"), Stock->GetNumChangedInstances(), ChangedCount);

	StartSeconds = FPlatformTime::Seconds();
	int32 Evaluated = 0;
	for (int32 Index = 0; Index < ChangedCount; ++Index)
	{
		Evaluated += IRpgHarvestableTarget::Execute_EvaluateHarvest(
			Field->Instances,
			MakeInstanceRequest(Field->Instances, Index * 13 % InstanceCount, Harvester)).IsSuccess() ? 1 : 0;
	}
	const double EvaluateUs = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0 / ChangedCount;
	TestEqual(TEXT("Every budget evaluation succeeded"), Evaluated, ChangedCount);

	Field->Destroy();
	double StreamBridgeMs = 0.0;
	ARpgHarvestAutomationInstancesActor* Streamed = StreamIn(StreamBridgeMs);
	TestTrue(TEXT("The streamed field presents every stored instance"), Streamed && Streamed->Instances->EventCount == ChangedCount);

	if (Streamed)
	{
		Streamed->Destroy();
	}
	double LinkedBridgeMs = 0.0;
	ARpgHarvestAutomationInstancesActor* Linked = StreamIn(LinkedBridgeMs, true);
	TestTrue(
		TEXT("The linked field links every point and presents every stored instance"),
		Linked && Linked->Instances->GetNumLinkedInstances() == InstanceCount && Linked->Instances->EventCount == ChangedCount);

	AddInfo(FString::Printf(
		TEXT("Harvest instance budget: %d instances, %d stored. Bridge BeginPlay: %.2f ms without stored stock, %.2f ms presenting %d stored instances, %.2f ms with linked visible instances. Commit %.1f us, evaluation %.1f us."),
		InstanceCount,
		ChangedCount,
		FirstBridgeMs,
		StreamBridgeMs,
		ChangedCount,
		LinkedBridgeMs,
		CommitUs,
		EvaluateUs));

	// Regression ceilings roughly ten times the measured Development editor cost (0.6 ms and 1.2 ms); Unreal
	// Insights has the per-function detail.
	TestTrue(TEXT("The bridge streams in 10,000 instances within 10 ms"), FirstBridgeMs < 10.0);
	TestTrue(TEXT("The bridge presents 1,000 stored instances on stream-in within 15 ms"), StreamBridgeMs < 15.0);
	TestTrue(TEXT("A linked field of 10,000 instances streams in within 30 ms"), LinkedBridgeMs < 30.0);
	TestTrue(TEXT("A commit stays within 200 us"), CommitUs < 200.0);
	TestTrue(TEXT("An evaluation stays within 50 us"), EvaluateUs < 50.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestInstancesLargeStockBudgetTest,
	"SurvivalRpg.Harvesting.Instances.LargeStockBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestInstancesLargeStockBudgetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancesTests;

	// A long co-op session in a saved world: most of a dense field differs from its authored stock, and every depleted
	// instance waits for its respawn.
	constexpr int32 GridSize = 100;
	constexpr int32 InstanceCount = GridSize * GridSize;
	constexpr int32 ChangedCount = 6000;
	constexpr double Spacing = 150.0;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	URpgHarvestProfile* Profile = MakeProfile(World, 4, 600.0f);
	Profile->LootTable->Groups[0].GroupChancePercent = 0.0f;
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Cube mesh exists"), Cube) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();

	TArray<FTransform> Transforms;
	Transforms.Reserve(InstanceCount);
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			Transforms.Emplace(FVector(X * Spacing, Y * Spacing, 0.0));
		}
	}
	auto StreamIn = [World, Profile, Cube, &Transforms](double& OutBridgeMs)
	{
		ARpgHarvestAutomationInstancesActor* Actor = World->SpawnActorDeferred<ARpgHarvestAutomationInstancesActor>(
			ARpgHarvestAutomationInstancesActor::StaticClass(),
			FTransform::Identity,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor)
		{
			return Actor;
		}
		Actor->Instances->ConfigureProfile(Profile);
		Actor->Instances->SetStaticMesh(Cube);
		Actor->Instances->AddInstances(Transforms, false);
		Actor->FinishSpawning(FTransform::Identity);
		const double StartSeconds = FPlatformTime::Seconds();
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		OutBridgeMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		return Actor;
	};

	double FirstBridgeMs = 0.0;
	ARpgHarvestAutomationInstancesActor* Field = StreamIn(FirstBridgeMs);
	if (!TestNotNull(TEXT("Resource field exists"), Field))
	{
		return false;
	}

	// Every commit depletes one instance and schedules its respawn, so the stock and the respawn queue keep growing.
	double StartSeconds = FPlatformTime::Seconds();
	int32 Depleted = 0;
	for (int32 Index = 0; Index < ChangedCount; ++Index)
	{
		Depleted += Field->Instances->CommitHarvest_Implementation(
			MakeInstanceRequest(Field->Instances, Index * 7 % InstanceCount, Harvester, 4)).bDepleted ? 1 : 0;
	}
	const double CommitUs = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0 / ChangedCount;
	TestEqual(TEXT("Every budget harvest depleted its instance"), Depleted, ChangedCount);
	TestEqual(TEXT("Every depleted instance is stored"), Stock->GetNumChangedInstances(), ChangedCount);

	// Target previews read the stock of changed and untouched instances alike.
	StartSeconds = FPlatformTime::Seconds();
	int32 Evaluated = 0;
	for (int32 Index = 0; Index < ChangedCount; ++Index)
	{
		Evaluated += IRpgHarvestableTarget::Execute_EvaluateHarvest(
			Field->Instances,
			MakeInstanceRequest(Field->Instances, Index * 13 % InstanceCount, Harvester)).Outcome != ERpgHarvestOutcome::Invalid ? 1 : 0;
	}
	const double EvaluateUs = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0 / ChangedCount;
	TestEqual(TEXT("Every budget evaluation resolved its instance"), Evaluated, ChangedCount);

	Field->Destroy();
	double StreamBridgeMs = 0.0;
	ARpgHarvestAutomationInstancesActor* Streamed = StreamIn(StreamBridgeMs);
	TestTrue(TEXT("The streamed field presents every stored instance"), Streamed && Streamed->Instances->EventCount == ChangedCount);

	// The stock replicates through shards, each of which must stay well below the engine's per-update and initial-bunch
	// limits (2,048 changes, 64 KB or about 1,700 entries).
	const int32 MaxShardEntries = Stock->GetMaxShardEntries();
	const int32 AverageShardEntries = ChangedCount / URpgHarvestInstanceStockComponent::NumShards;

	AddInfo(FString::Printf(
		TEXT("Large harvest stock budget: %d instances, %d depleted with pending respawns. Bridge BeginPlay: %.2f ms without stored stock, %.2f ms presenting %d stored instances. Commit %.1f us, evaluation %.1f us. Fullest shard: %d entries, average %d."),
		InstanceCount,
		ChangedCount,
		FirstBridgeMs,
		StreamBridgeMs,
		ChangedCount,
		CommitUs,
		EvaluateUs,
		MaxShardEntries,
		AverageShardEntries));

	// Before HARV-10c the stream-in took 1.9 s and a commit 334 us: every hidden instance recomputed the bounds of all
	// 10,000 instances, and stock lookups scanned every entry. Ceilings are roughly ten times the measured cost.
	TestTrue(TEXT("The bridge presents 6,000 stored depletions on stream-in within 200 ms"), StreamBridgeMs < 200.0);
	TestTrue(TEXT("A depleting commit with 6,000 stored instances stays within 100 us"), CommitUs < 100.0);
	TestTrue(TEXT("An evaluation with 6,000 stored instances stays within 50 us"), EvaluateUs < 50.0);
	TestTrue(TEXT("The key hash spreads the stock evenly over the shards"), MaxShardEntries < 2 * AverageShardEntries);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestInstancesLinkedPresentationTest,
	"SurvivalRpg.Harvesting.Instances.LinkedPresentationFollowsStock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestInstancesLinkedPresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancesTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester) ||
		!TestNotNull(TEXT("Cube mesh exists"), Cube))
	{
		return false;
	}
	const FName LinkTag(TEXT("HarvestTreeVisual"));
	const FVector FieldLocation(500.0, -300.0, 0.0);
	TestWorld.PrimeTimerManager();

	// Like a PCG graph that spawns invisible trunk proxies and visible trees at the same points: a tagged static and
	// a tagged skinned visual, plus an untagged mesh at the same points that must stay untouched.
	struct FLinkedField
	{
		ARpgHarvestAutomationInstancesActor* Actor = nullptr;
		UInstancedStaticMeshComponent* StaticVisual = nullptr;
		UInstancedSkinnedMeshComponent* SkinnedVisual = nullptr;
		UInstancedStaticMeshComponent* Unrelated = nullptr;
	};
	auto SpawnLinkedField = [World, Cube, LinkTag, FieldLocation]()
	{
		FLinkedField Field;
		Field.Actor = World->SpawnActorDeferred<ARpgHarvestAutomationInstancesActor>(
			ARpgHarvestAutomationInstancesActor::StaticClass(),
			FTransform(FieldLocation),
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Field.Actor)
		{
			return Field;
		}
		Field.Actor->Instances->ConfigureProfile(MakeProfile(World, 4));
		Field.Actor->Instances->ConfigureLinkedPresentation(LinkTag);
		Field.Actor->Instances->SetStaticMesh(Cube);
		for (const FVector& Location : ThreeInstanceLocations)
		{
			Field.Actor->Instances->AddInstance(FTransform(Location));
		}
		Field.Actor->FinishSpawning(FTransform(FieldLocation));

		Field.StaticVisual = NewObject<UInstancedStaticMeshComponent>(Field.Actor, TEXT("StaticVisual"));
		Field.SkinnedVisual = NewObject<UInstancedSkinnedMeshComponent>(Field.Actor, TEXT("SkinnedVisual"));
		Field.Unrelated = NewObject<UInstancedStaticMeshComponent>(Field.Actor, TEXT("Unrelated"));
		Field.StaticVisual->ComponentTags.Add(LinkTag);
		Field.SkinnedVisual->ComponentTags.Add(LinkTag);
		for (UMeshComponent* Mesh : TArray<UMeshComponent*>{Field.StaticVisual, Field.SkinnedVisual, Field.Unrelated})
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetupAttachment(Field.Actor->GetRootComponent());
			Mesh->RegisterComponent();
		}
		Field.StaticVisual->SetStaticMesh(Cube);
		Field.Unrelated->SetStaticMesh(Cube);
		// Visible meshes list their instances in their own order, and one visual has no resource at its point.
		for (int32 Index = ThreeInstanceLocations.Num() - 1; Index >= 0; --Index)
		{
			Field.StaticVisual->AddInstance(FTransform(ThreeInstanceLocations[Index]));
			Field.SkinnedVisual->AddInstance(FTransform(ThreeInstanceLocations[Index]), 0);
			Field.Unrelated->AddInstance(FTransform(ThreeInstanceLocations[Index]));
		}
		Field.StaticVisual->AddInstance(FTransform(FVector(0.0, 0.0, 900.0)));
		if (!Field.Actor->HasActorBegunPlay())
		{
			Field.Actor->DispatchBeginPlay();
		}
		return Field;
	};
	auto StaticScale = [](const UInstancedStaticMeshComponent* Mesh, const int32 Index)
	{
		FTransform Transform;
		return Mesh && Mesh->GetInstanceTransform(Index, Transform, false) ? Transform.GetScale3D().X : -1.0;
	};
	auto SkinnedScale = [](const UInstancedSkinnedMeshComponent* Mesh, const int32 Index)
	{
		FTransform Transform;
		return Mesh && Mesh->GetInstanceTransform(Mesh->GetInstanceId(Index), Transform, false) ? Transform.GetScale3D().X : -1.0;
	};

	FLinkedField Field = SpawnLinkedField();
	if (!TestNotNull(TEXT("Linked field exists"), Field.Actor) ||
		!TestEqual(TEXT("The skinned visual holds three instances"), Field.SkinnedVisual->GetInstanceCount(), 3))
	{
		return false;
	}
	URpgHarvestAutomationInstancesComponent* Proxies = Field.Actor->Instances;
	TestEqual(TEXT("Every proxy is linked to its visible instances"), Proxies->GetNumLinkedInstances(), 3);
	UMeshComponent* LinkedComponent = nullptr;
	FTransform LinkedTransform;
	TestTrue(
		TEXT("The proxy reports the visible instance at its location"),
		Proxies->GetLinkedPresentationInstance(0, LinkedComponent, LinkedTransform) &&
			(LinkedComponent == Field.StaticVisual || LinkedComponent == Field.SkinnedVisual) &&
			LinkedTransform.GetLocation().Equals(FieldLocation + ThreeInstanceLocations[0], 0.5));

	// Proxy 0 sits at the visuals' last index, because they list their instances in reverse order.
	TestTrue(TEXT("Depleting a proxy succeeds"), Proxies->CommitHarvest_Implementation(MakeInstanceRequest(Proxies, 0, Harvester, 4)).bDepleted);
	TestTrue(TEXT("The linked static visual hides"), FMath::IsNearlyZero(StaticScale(Field.StaticVisual, 2)));
	TestTrue(TEXT("The linked skinned visual hides"), FMath::IsNearlyZero(SkinnedScale(Field.SkinnedVisual, 2)));
	TestTrue(TEXT("Other visible instances stay"), FMath::IsNearlyEqual(StaticScale(Field.StaticVisual, 1), 1.0));
	TestTrue(TEXT("An untagged mesh at the same point stays"), FMath::IsNearlyEqual(StaticScale(Field.Unrelated, 2), 1.0));
	TestTrue(TEXT("A visual without a resource stays"), FMath::IsNearlyEqual(StaticScale(Field.StaticVisual, 3), 1.0));
	TestTrue(
		TEXT("A hidden visual still reports its authored transform"),
		Proxies->GetLinkedPresentationInstance(0, LinkedComponent, LinkedTransform) &&
			FMath::IsNearlyEqual(LinkedTransform.GetScale3D().X, 1.0));
	TestTrue(
		TEXT("A partial harvest keeps the visuals"),
		Proxies->CommitHarvest_Implementation(MakeInstanceRequest(Proxies, 1, Harvester)).IsSuccess() &&
			FMath::IsNearlyEqual(SkinnedScale(Field.SkinnedVisual, 1), 1.0));

	// Streaming in again hides the visuals of stored depletions as initial state.
	Field.Actor->Destroy();
	Field = SpawnLinkedField();
	if (!TestNotNull(TEXT("Linked field streams in again"), Field.Actor))
	{
		return false;
	}
	TestTrue(TEXT("A stored depletion hides its static visual on stream-in"), FMath::IsNearlyZero(StaticScale(Field.StaticVisual, 2)));
	TestTrue(TEXT("A stored depletion hides its skinned visual on stream-in"), FMath::IsNearlyZero(SkinnedScale(Field.SkinnedVisual, 2)));

	TestWorld.AdvanceTimers(RespawnSeconds + 1.0f);
	TestTrue(TEXT("A respawn restores the static visual"), FMath::IsNearlyEqual(StaticScale(Field.StaticVisual, 2), 1.0));
	TestTrue(TEXT("A respawn restores the skinned visual"), FMath::IsNearlyEqual(SkinnedScale(Field.SkinnedVisual, 2), 1.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
