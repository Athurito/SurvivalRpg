#include "RpgWorldSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWorldSaveGameMemoryRoundTripTest,
	"SurvivalRpg.Save.WorldSave.MemoryRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWorldSaveGameMemoryRoundTripTest::RunTest(const FString& Parameters)
{
	URpgWorldSaveGame* Source = NewObject<URpgWorldSaveGame>();
	Source->SaveSequence = 42;

	FRpgPlayerSaveData Player;
	Player.bHasCheckpoint = true;
	Player.CheckpointTransform = FTransform(FRotator::ZeroRotator, FVector(100.0, 200.0, 300.0));
	Player.bHasInventoryGraph = true;
	Player.QuickAccessBindings.SetNum(8);
	Source->Players.Add(TEXT("Offline:Automation"), Player);

	FRpgWorldContainerSaveData Container;
	Container.PersistentContainerId = TEXT("AutomationChest");
	Source->WorldContainers.Add(Container.PersistentContainerId, Container);

	FString ValidationError;
	TestTrue(TEXT("Source DTO validates"), Source->ValidateForLoad(ValidationError));

	TArray<uint8> Bytes;
	TestTrue(TEXT("SaveGame serializes to memory"), UGameplayStatics::SaveGameToMemory(Source, Bytes));
	URpgWorldSaveGame* Restored = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("SaveGame deserializes with the expected class"), Restored);
	if (!Restored)
	{
		return false;
	}

	TestTrue(TEXT("Restored DTO validates"), Restored->ValidateForLoad(ValidationError));
	TestEqual(TEXT("Monotonic sequence survives serialization"), Restored->SaveSequence, int64(42));
	const FRpgPlayerSaveData* RestoredPlayer = Restored->Players.Find(TEXT("Offline:Automation"));
	TestNotNull(TEXT("Stable profile key survives serialization"), RestoredPlayer);
	if (RestoredPlayer)
	{
		TestTrue(TEXT("Checkpoint flag survives serialization"), RestoredPlayer->bHasCheckpoint);
		TestEqual(TEXT("Exactly eight quick-access bindings survive serialization"), RestoredPlayer->QuickAccessBindings.Num(), 8);
		TestTrue(TEXT("Inventory graph presence survives serialization"), RestoredPlayer->bHasInventoryGraph);
	}
	TestTrue(TEXT("Persistent world-container id survives serialization"), Restored->WorldContainers.Contains(TEXT("AutomationChest")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWorldSaveGameValidationTest,
	"SurvivalRpg.Save.WorldSave.ValidationRejectsCorruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWorldSaveGameValidationTest::RunTest(const FString& Parameters)
{
	FString ValidationError;
	URpgWorldSaveGame* Save = NewObject<URpgWorldSaveGame>();
	TestTrue(TEXT("Fresh save validates"), Save->ValidateForLoad(ValidationError));

	Save->SchemaVersion = URpgWorldSaveGame::CurrentSchemaVersion + 1;
	TestFalse(TEXT("Unknown top-level schema is rejected"), Save->ValidateForLoad(ValidationError));
	Save->SchemaVersion = URpgWorldSaveGame::CurrentSchemaVersion;

	FRpgPlayerSaveData Player;
	Player.bHasInventoryGraph = true;
	Player.QuickAccessBindings.SetNum(7);
	Save->Players.Add(TEXT("Offline:BrokenBindings"), Player);
	TestFalse(TEXT("Persisted quick access cannot drift from eight bindings"), Save->ValidateForLoad(ValidationError));
	Save->Players.Reset();

	FRpgInventorySavedItem Item;
	Item.ItemId = FRpgInventoryItemId::NewId();
	Item.ItemDefinition = URpgInventoryItemDefinition::StaticClass();
	Item.StackCount = 1;
	Item.Container = FRpgInventoryContainerHandle::MakeRoot(TEXT("Pockets"));
	Player = FRpgPlayerSaveData();
	Player.bHasInventoryGraph = true;
	Player.QuickAccessBindings.SetNum(8);
	Player.InventoryGraph.Items = { Item, Item };
	Save->Players.Add(TEXT("Offline:DuplicateItem"), Player);
	TestFalse(TEXT("Duplicate persistent item ids are rejected before runtime import"), Save->ValidateForLoad(ValidationError));
	Save->Players.Reset();

	FRpgWorldContainerSaveData Container;
	Container.PersistentContainerId = TEXT("DifferentId");
	Save->WorldContainers.Add(TEXT("ChestA"), Container);
	TestFalse(TEXT("Mismatched persistent world-container ids are rejected"), Save->ValidateForLoad(ValidationError));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
