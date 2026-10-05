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

	AddInfo(FString::Printf(
		TEXT("Harvest instance budget: %d instances, %d stored. Bridge BeginPlay: %.2f ms without stored stock, %.2f ms presenting %d stored instances. Commit %.1f us, evaluation %.1f us."),
		InstanceCount,
		ChangedCount,
		FirstBridgeMs,
		StreamBridgeMs,
		ChangedCount,
		CommitUs,
		EvaluateUs));

	// Regression ceilings roughly ten times the measured Development editor cost (0.6 ms and 1.2 ms); Unreal
	// Insights has the per-function detail.
	TestTrue(TEXT("The bridge streams in 10,000 instances within 10 ms"), FirstBridgeMs < 10.0);
	TestTrue(TEXT("The bridge presents 1,000 stored instances on stream-in within 15 ms"), StreamBridgeMs < 15.0);
	TestTrue(TEXT("A commit stays within 200 us"), CommitUs < 200.0);
	TestTrue(TEXT("An evaluation stays within 50 us"), EvaluateUs < 50.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
