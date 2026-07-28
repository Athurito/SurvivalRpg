#include "RpgLootSourceAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryAutomationTestTypes.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace RpgLootSourceTests
{
	class FScopedLootWorld
	{
	public:
		FScopedLootWorld()
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

		~FScopedLootWorld()
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

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	URpgLootTable* MakeGuaranteedTable(UObject* Outer)
	{
		URpgLootTable* Table = NewObject<URpgLootTable>(Outer);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition =
			URpgInventoryAutomationTestStackItemDefinition::StaticClass();
		Entry.MinimumQuantity = 1;
		Entry.MaximumQuantity = 1;
		Entry.ChancePercent = 100.0f;
		return Table;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootSourceMissingConfigurationTest,
	"SurvivalRpg.Loot.Source.MissingConfigurationFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootSourceMissingConfigurationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgLootSourceTests::FScopedLootWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone missing-loot test world is available"), World))
	{
		return false;
	}

	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Missing-loot fixture owner is spawned"), Owner))
	{
		return false;
	}
	URpgInventoryManagerComponent* Inventory =
		NewObject<URpgInventoryManagerComponent>(Owner, TEXT("LootInventory"), RF_Transient);
	Owner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	URpgInventoryContainerComponent* Container =
		NewObject<URpgInventoryContainerComponent>(Owner, TEXT("LootContainer"), RF_Transient);
	Owner->AddInstanceComponent(Container);
	Container->RegisterComponent();
	URpgLootSourceAutomationTestComponent* LootSource =
		NewObject<URpgLootSourceAutomationTestComponent>(Owner, TEXT("LootSource"), RF_Transient);
	LootSource->ConfigureLootTable(nullptr, true);
	Owner->AddInstanceComponent(LootSource);
	LootSource->RegisterComponent();
	if (!Owner->HasActorBegunPlay())
	{
		Owner->DispatchBeginPlay();
	}

#if WITH_EDITOR
	FDataValidationContext ValidationContext;
	TestEqual(
		TEXT("An unconfigured loot source fails editor data validation"),
		LootSource->IsDataValid(ValidationContext),
		EDataValidationResult::Invalid);
#endif
	TestFalse(
		TEXT("An unconfigured death-gated container remains inaccessible"),
		Container->IsContainerAccessible());
	AddExpectedError(
		TEXT("has no LootTable or deprecated fixed loot configured"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	LootSource->PopulateLoot();
	TestEqual(
		TEXT("An unconfigured source grants no inventory entries"),
		Inventory->GetUsedEntryCount(),
		0);
	TestFalse(
		TEXT("Failed empty population never exposes the corpse container"),
		Container->IsContainerAccessible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootSourceRetryAndExactlyOnceTest,
	"SurvivalRpg.Loot.Source.AtomicFailureRetryAndExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootSourceRetryAndExactlyOnceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgLootSourceTests::FScopedLootWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone loot-source world is available"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		World,
		AActor::StaticClass(),
		TEXT("LootSourceRetryOwner"));
	SpawnParameters.ObjectFlags = RF_Transient;
	AActor* Owner = World->SpawnActor<AActor>(SpawnParameters);
	if (!TestNotNull(TEXT("Authoritative loot-source owner is spawned"), Owner) ||
		!TestTrue(TEXT("Loot-source fixture has server authority"), Owner->HasAuthority()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		NewObject<URpgInventoryManagerComponent>(Owner, TEXT("LootInventory"), RF_Transient);
	Owner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	Inventory->SetFixedMaxEntries(0);
	Inventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	URpgInventoryContainerComponent* Container =
		NewObject<URpgInventoryContainerComponent>(
			Owner,
			TEXT("LootContainer"),
			RF_Transient);
	Owner->AddInstanceComponent(Container);
	Container->RegisterComponent();

	URpgLootSourceAutomationTestComponent* LootSource =
		NewObject<URpgLootSourceAutomationTestComponent>(
			Owner,
			TEXT("LootSource"),
			RF_Transient);
	URpgLootTable* LootTable =
		RpgLootSourceTests::MakeGuaranteedTable(Owner);
	LootSource->ConfigureLootTable(LootTable, true);
	Owner->AddInstanceComponent(LootSource);
	LootSource->RegisterComponent();
	if (!Owner->HasActorBegunPlay())
	{
		Owner->DispatchBeginPlay();
	}
	int32 CompletionBroadcastCount = 0;
	bool bCompletionHadLoot = false;
	LootSource->OnLootPopulationCompleted.AddLambda(
		[&](URpgLootSourceComponent* CompletedSource, bool bHasLoot)
		{
			TestTrue(
				TEXT("The completion callback identifies its loot source"),
				CompletedSource == LootSource);
			++CompletionBroadcastCount;
			bCompletionHadLoot = bHasLoot;
		});
	TestFalse(
		TEXT("A death-gated corpse container starts inaccessible"),
		Container->IsContainerAccessible());

	AddExpectedError(
		TEXT("Atomic loot population failed"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	LootSource->PopulateLoot();
	TestEqual(
		TEXT("A failed atomic population leaves the inventory untouched"),
		Inventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		0);
	TestFalse(
		TEXT("A failed population does not expose an empty corpse"),
		Container->IsContainerAccessible());
	TestFalse(
		TEXT("A failed population is not marked complete"),
		LootSource->IsLootPopulated());
	TestEqual(
		TEXT("A failed population emits no completion callback"),
		CompletionBroadcastCount,
		0);

	// Mutating the source asset after the failed delivery proves that the retry
	// consumes the first cached roll instead of evaluating the table a second time.
	LootTable->Groups[0].Entries[0].ItemDefinition =
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass();

	Inventory->SetCapacityMode(ERpgInventoryCapacityMode::Unlimited);
	LootSource->PopulateLoot();
	TestEqual(
		TEXT("The retry delivers the exact first cached roll after capacity becomes available"),
		Inventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		1);
	TestEqual(
		TEXT("The retry never re-evaluates the mutated table"),
		Inventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass()),
		0);
	TestTrue(
		TEXT("A successful retry unlocks the populated corpse container"),
		Container->IsContainerAccessible());
	TestTrue(
		TEXT("A successful retry records population completion"),
		LootSource->IsLootPopulated());
	TestEqual(
		TEXT("Successful population emits exactly one completion callback"),
		CompletionBroadcastCount,
		1);
	TestTrue(
		TEXT("The populated callback distinguishes a non-empty reward"),
		bCompletionHadLoot);

	LootSource->PopulateLoot();
	TestEqual(
		TEXT("A completed source never rolls or inserts a second death reward"),
		Inventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		1);
	TestEqual(
		TEXT("A completed source never repeats its completion callback"),
		CompletionBroadcastCount,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootSourceNoDropAndExternalUnlockTest,
	"SurvivalRpg.Loot.Source.NoDropCompletionAndExternalUnlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootSourceNoDropAndExternalUnlockTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	RpgLootSourceTests::FScopedLootWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone no-drop test world is available"), World))
	{
		return false;
	}

	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("No-drop fixture owner is spawned"), Owner))
	{
		return false;
	}
	URpgInventoryManagerComponent* Inventory =
		NewObject<URpgInventoryManagerComponent>(
			Owner,
			TEXT("LootInventory"),
			RF_Transient);
	Owner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	URpgInventoryContainerComponent* Container =
		NewObject<URpgInventoryContainerComponent>(
			Owner,
			TEXT("LootContainer"),
			RF_Transient);
	Container->ConfigureAsDeathLootContainer();
	Owner->AddInstanceComponent(Container);
	Container->RegisterComponent();

	URpgLootSourceAutomationTestComponent* LootSource =
		NewObject<URpgLootSourceAutomationTestComponent>(
			Owner,
			TEXT("LootSource"),
			RF_Transient);
	URpgLootTable* NoDropTable =
		RpgLootSourceTests::MakeGuaranteedTable(Owner);
	NoDropTable->Groups[0].Entries[0].ChancePercent = 0.0f;
	LootSource->ConfigureLootTable(NoDropTable, true);
	LootSource->SetAutomaticContainerUnlockEnabled(false);
	LootSource->SetGenerateLootOnDeathEnabled(false);
	Owner->AddInstanceComponent(LootSource);
	LootSource->RegisterComponent();
	if (!Owner->HasActorBegunPlay())
	{
		Owner->DispatchBeginPlay();
	}

	int32 CompletionBroadcastCount = 0;
	bool bCompletionHadLoot = true;
	LootSource->OnLootPopulationCompleted.AddLambda(
		[&](URpgLootSourceComponent* CompletedSource, bool bHasLoot)
		{
			TestTrue(
				TEXT("The no-drop callback identifies its source"),
				CompletedSource == LootSource);
			++CompletionBroadcastCount;
			bCompletionHadLoot = bHasLoot;
		});

	LootSource->PopulateLoot();
	TestTrue(
		TEXT("A valid no-drop roll still completes population"),
		LootSource->IsLootPopulated());
	TestEqual(
		TEXT("A no-drop roll adds no inventory entries"),
		Inventory->GetUsedEntryCount(),
		0);
	TestEqual(
		TEXT("A no-drop roll emits one completion callback"),
		CompletionBroadcastCount,
		1);
	TestFalse(
		TEXT("The completion callback distinguishes an empty no-drop roll"),
		bCompletionHadLoot);
	TestFalse(
		TEXT("External lifecycle ownership keeps the container locked"),
		Container->IsContainerAccessible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootSourceDisabledDeathGenerationValidationTest,
	"SurvivalRpg.Loot.Source.DisabledDeathGenerationAllowsHarvestOnlySource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootSourceDisabledDeathGenerationValidationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
#if WITH_EDITOR
	URpgLootSourceAutomationTestComponent* LootSource =
		NewObject<URpgLootSourceAutomationTestComponent>();
	LootSource->ConfigureLootTable(nullptr, false);
	LootSource->SetGenerateLootOnDeathEnabled(false);
	FDataValidationContext ValidationContext;
	TestEqual(
		TEXT("A harvest-only source needs no inventory loot configuration"),
		LootSource->IsDataValid(ValidationContext),
		EDataValidationResult::Valid);
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
