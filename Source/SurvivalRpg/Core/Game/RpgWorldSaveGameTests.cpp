#include "RpgWorldSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"

namespace RpgWorldSaveGameTests
{
	FRpgInventorySavedItem MakeValidSavedItem(FName RootContainerId)
	{
		FRpgInventorySavedItem Item;
		Item.ItemId = FRpgInventoryItemId::NewId();
		Item.ItemDefinition = URpgInventoryItemDefinition::StaticClass();
		Item.StackCount = 1;
		Item.Container = FRpgInventoryContainerHandle::MakeRoot(RootContainerId);
		Item.Placement.SetContainerHandle(Item.Container);
		Item.Placement.X = 0;
		Item.Placement.Y = 0;
		FRpgInventoryFragmentStatePayload& CoreState =
			Item.RuntimeState.AddDefaulted_GetRef();
		CoreState.FragmentId = TEXT("Inventory.Core.StatTags");
		CoreState.Version = 2;
		FMemoryWriter Writer(CoreState.Payload, true);
		int32 EmptySemanticStackCount = 0;
		Writer << EmptySemanticStackCount;
		return Item;
	}

	FRpgBaseStorageSaveData MakeValidBaseStorage(FName BaseId)
	{
		FRpgBaseStorageSaveData Base;
		Base.BaseId = BaseId;
		Base.OwnerProfileKey = TEXT("Offline:BaseOwner");
		Base.bHasArmoryGraph = true;
		Base.ArmoryGraph.Items.Add(MakeValidSavedItem(TEXT("Armory")));
		Base.bHasContainmentGraph = true;
		return Base;
	}
}

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
	Player.bHasPlayerProgression = true;
	Player.PlayerProgression.Level = 7;
	Player.PlayerProgression.XP = 321.0f;
	Player.PlayerProgression.UnspentSkillPoints = 2;
	Player.bHasTradeSkillProgression = true;
	FTradeSkillState& SavedForaging = Player.TradeSkillStates.AddDefaulted_GetRef();
	SavedForaging.SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Foraging;
	SavedForaging.Level = 14;
	SavedForaging.XP = 55.0f;

	const FRpgInventoryContainerHandle CarryContainer =
		FRpgInventoryContainerHandle::MakeRoot(TEXT("Carry.Weapon1"));
	FRpgInventorySlotAddress& CarryAddress = Player.QuickAccessBindings[0].SlotAddress;
	Player.QuickAccessBindings[0].SlotType = ERpgActionBarSlotType::CarrySlot;
	Player.QuickAccessBindings[0].CarrySemanticRole =
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary;
	CarryAddress.SetContainerHandle(CarryContainer);
	CarryAddress.X = 2;
	CarryAddress.Y = 3;

	const FRpgInventoryItemId ContainerOwnerId(FGuid(0x8BB80217, 0xD34D4FD0, 0xAD68868A, 0x719F23D1));
	const FRpgInventoryContainerHandle NestedItemContainer =
		FRpgInventoryContainerHandle::MakeItemOwned(ContainerOwnerId, TEXT("Main"), 2);
	FRpgInventorySlotAddress& NestedItemAddress = Player.QuickAccessBindings[1].SlotAddress;
	Player.QuickAccessBindings[1].SlotType = ERpgActionBarSlotType::Consumable;
	NestedItemAddress.SetContainerHandle(NestedItemContainer);
	NestedItemAddress.X = 5;
	NestedItemAddress.Y = 7;
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
		TestTrue(TEXT("General progression presence survives serialization"), RestoredPlayer->bHasPlayerProgression);
		TestEqual(TEXT("Character level survives serialization"), RestoredPlayer->PlayerProgression.Level, 7);
		TestEqual(TEXT("Character XP survives serialization"), RestoredPlayer->PlayerProgression.XP, 321.0f);
		TestTrue(TEXT("Trade-skill progression presence survives serialization"), RestoredPlayer->bHasTradeSkillProgression);
		TestEqual(TEXT("Exactly one authored trade-skill state survives serialization"), RestoredPlayer->TradeSkillStates.Num(), 1);
		if (RestoredPlayer->TradeSkillStates.Num() == 1)
		{
			TestEqual(
				TEXT("Trade-skill tag survives serialization"),
				RestoredPlayer->TradeSkillStates[0].SkillTag,
				FGameplayTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging));
			TestEqual(TEXT("Trade-skill level survives serialization"), RestoredPlayer->TradeSkillStates[0].Level, 14);
			TestEqual(TEXT("Trade-skill XP survives serialization"), RestoredPlayer->TradeSkillStates[0].XP, 55.0f);
		}

		const FRpgInventorySlotAddress& RestoredCarryAddress = RestoredPlayer->QuickAccessBindings[0].SlotAddress;
		TestEqual(
			TEXT("Root Carry slot preserves its complete container handle"),
			RestoredCarryAddress.GetContainerHandle(),
			CarryContainer);
		TestEqual(TEXT("Root Carry slot preserves X"), RestoredCarryAddress.X, 2);
		TestEqual(TEXT("Root Carry slot preserves Y"), RestoredCarryAddress.Y, 3);
		TestTrue(
			TEXT("Carry semantic role survives serialization independently from the container id"),
			RestoredPlayer->QuickAccessBindings[0].CarrySemanticRole ==
				RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);

		const FRpgInventorySlotAddress& RestoredNestedItemAddress = RestoredPlayer->QuickAccessBindings[1].SlotAddress;
		TestEqual(
			TEXT("Nested item slot preserves its complete item-owned container handle"),
			RestoredNestedItemAddress.GetContainerHandle(),
			NestedItemContainer);
		TestEqual(
			TEXT("Nested item slot preserves its owning item identity"),
			RestoredNestedItemAddress.GetContainerHandle().ItemOwnerId,
			ContainerOwnerId);
		TestEqual(
			TEXT("Nested item slot preserves its definition-local container id"),
			RestoredNestedItemAddress.GetContainerHandle().ContainerId,
			FName(TEXT("Main")));
		TestEqual(
			TEXT("Nested item slot preserves its graph depth"),
			RestoredNestedItemAddress.GetContainerHandle().Depth,
			static_cast<uint8>(2));
		TestEqual(TEXT("Nested item slot preserves X"), RestoredNestedItemAddress.X, 5);
		TestEqual(TEXT("Nested item slot preserves Y"), RestoredNestedItemAddress.Y, 7);
	}
	TestTrue(TEXT("Persistent world-container id survives serialization"), Restored->WorldContainers.Contains(TEXT("AutomationChest")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWorldSaveGameLegacyV1EmptyMigrationTest,
	"SurvivalRpg.Save.WorldSave.V2.LegacyV1MigratesWithEmptyStorageState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWorldSaveGameLegacyV1EmptyMigrationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	URpgWorldSaveGame* Legacy = NewObject<URpgWorldSaveGame>();
	Legacy->SchemaVersion = 1;

	FString ValidationError;
	TestTrue(
		TEXT("Schema V1 remains loadable when its V2 storage fields are empty"),
		Legacy->ValidateForLoad(ValidationError));

	TArray<uint8> Bytes;
	TestTrue(
		TEXT("Legacy V1 snapshot serializes through the current SaveGame class"),
		UGameplayStatics::SaveGameToMemory(Legacy, Bytes));
	URpgWorldSaveGame* Restored =
		Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Legacy V1 snapshot deserializes"), Restored))
	{
		return false;
	}

	TestTrue(
		TEXT("Deserialized V1 snapshot validates before migration"),
		Restored->ValidateForLoad(ValidationError));
	TestTrue(
		TEXT("V1 migration starts with no persisted base networks"),
		Restored->BaseStorages.IsEmpty());
	TestTrue(
		TEXT("V1 migration starts with no storage knowledge"),
		Restored->StorageKnowledgeTags.IsEmpty());

	Restored->SchemaVersion = URpgWorldSaveGame::CurrentSchemaVersion;
	TestTrue(
		TEXT("The empty legacy payload is a valid V2 reconstruction boundary"),
		Restored->ValidateForLoad(ValidationError));

	Legacy->StorageKnowledgeTags.AddTag(
		RpgGameplayTags::Storage_Knowledge_RiftContainment);
	TestFalse(
		TEXT("Schema V1 cannot falsely claim a V2 knowledge payload"),
		Legacy->ValidateForLoad(ValidationError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWorldSaveGameStorageKnowledgeRoundTripTest,
	"SurvivalRpg.Save.WorldSave.V2.StorageKnowledgeRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWorldSaveGameStorageKnowledgeRoundTripTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	URpgWorldSaveGame* Source = NewObject<URpgWorldSaveGame>();
	Source->StorageKnowledgeTags.AddTag(
		RpgGameplayTags::Storage_Knowledge_RiftContainment);
	Source->StorageKnowledgeTags.AddTag(
		RpgGameplayTags::Storage_Knowledge_RiftAnalysis);

	FString ValidationError;
	TestTrue(
		TEXT("V2 source accepts concrete Storage.Knowledge discoveries"),
		Source->ValidateForLoad(ValidationError));

	TArray<uint8> Bytes;
	TestTrue(
		TEXT("V2 storage knowledge serializes to memory"),
		UGameplayStatics::SaveGameToMemory(Source, Bytes));
	URpgWorldSaveGame* Restored =
		Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("V2 storage knowledge snapshot deserializes"), Restored))
	{
		return false;
	}

	TestTrue(
		TEXT("Restored V2 knowledge snapshot validates"),
		Restored->ValidateForLoad(ValidationError));
	TestTrue(
		TEXT("Rift-containment knowledge survives the SaveGame roundtrip"),
		Restored->StorageKnowledgeTags.HasTagExact(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestTrue(
		TEXT("Rift-analysis knowledge survives the SaveGame roundtrip"),
		Restored->StorageKnowledgeTags.HasTagExact(
			RpgGameplayTags::Storage_Knowledge_RiftAnalysis));
	TestEqual(
		TEXT("Roundtrip does not synthesize additional knowledge"),
		Restored->StorageKnowledgeTags.Num(),
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWorldSaveGameBaseStorageValidationTest,
	"SurvivalRpg.Save.WorldSave.V2.BaseStorageValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWorldSaveGameBaseStorageValidationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	URpgWorldSaveGame* Save = NewObject<URpgWorldSaveGame>();
	FString ValidationError;

	const FName ValidBaseId(TEXT("AutomationBase"));
	FRpgBaseStorageSaveData ValidBase =
		RpgWorldSaveGameTests::MakeValidBaseStorage(ValidBaseId);
	Save->BaseStorages.Add(ValidBaseId, ValidBase);
	TestTrue(
		TEXT("A matching BaseId and valid armory graph pass V2 validation"),
		Save->ValidateForLoad(ValidationError));

	FRpgBaseStorageSaveData MissingIdBase = ValidBase;
	MissingIdBase.BaseId = NAME_None;
	Save->BaseStorages.Reset();
	Save->BaseStorages.Add(ValidBaseId, MissingIdBase);
	TestFalse(
		TEXT("A base payload without its stable BaseId is rejected"),
		Save->ValidateForLoad(ValidationError));

	FRpgBaseStorageSaveData MismatchedIdBase = ValidBase;
	MismatchedIdBase.BaseId = TEXT("DifferentBase");
	Save->BaseStorages.Reset();
	Save->BaseStorages.Add(ValidBaseId, MismatchedIdBase);
	TestFalse(
		TEXT("A base payload whose BaseId disagrees with its map key is rejected"),
		Save->ValidateForLoad(ValidationError));

	FRpgBaseStorageSaveData InvalidGraphBase = ValidBase;
	const FRpgInventorySavedItem DuplicateArmoryItem =
		InvalidGraphBase.ArmoryGraph.Items[0];
	InvalidGraphBase.ArmoryGraph.Items.Add(DuplicateArmoryItem);
	Save->BaseStorages.Reset();
	Save->BaseStorages.Add(ValidBaseId, InvalidGraphBase);
	TestFalse(
		TEXT("A base armory graph with duplicate persistent ItemIds is rejected"),
		Save->ValidateForLoad(ValidationError));

	FRpgBaseStorageSaveData InvalidLockerGraphBase = ValidBase;
	FRpgInventoryGraphSaveData InvalidLockerGraph;
	InvalidLockerGraph.Items.Add(
		RpgWorldSaveGameTests::MakeValidSavedItem(TEXT("Personal")));
	InvalidLockerGraph.Items[0].ItemId = FRpgInventoryItemId();
	InvalidLockerGraphBase.PersonalLockerGraphs.Add(
		TEXT("Offline:LockerOwner"),
		InvalidLockerGraph);
	Save->BaseStorages.Reset();
	Save->BaseStorages.Add(ValidBaseId, InvalidLockerGraphBase);
	TestFalse(
		TEXT("A personal-locker graph with an invalid persistent ItemId is rejected"),
		Save->ValidateForLoad(ValidationError));
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

	FRpgPlayerSaveData VersionedPlayer;
	VersionedPlayer.SchemaVersion = FRpgPlayerSaveData::MinimumSupportedSchemaVersion;
	TestTrue(TEXT("Legacy player schema remains eligible for explicit restore migration"), VersionedPlayer.IsSchemaSupported());
	VersionedPlayer.SchemaVersion = FRpgPlayerSaveData::CurrentSchemaVersion;
	TestTrue(TEXT("Current player schema is accepted"), VersionedPlayer.IsSchemaSupported());
	VersionedPlayer.SchemaVersion = FRpgPlayerSaveData::MinimumSupportedSchemaVersion - 1;
	TestFalse(TEXT("Player schemas older than the migration floor are rejected"), VersionedPlayer.IsSchemaSupported());
	VersionedPlayer.SchemaVersion = FRpgPlayerSaveData::CurrentSchemaVersion + 1;
	TestFalse(TEXT("Unknown future player schemas are rejected"), VersionedPlayer.IsSchemaSupported());

	Save->SchemaVersion = URpgWorldSaveGame::CurrentSchemaVersion + 1;
	TestFalse(TEXT("Unknown top-level schema is rejected"), Save->ValidateForLoad(ValidationError));
	Save->SchemaVersion = URpgWorldSaveGame::CurrentSchemaVersion;

	VersionedPlayer = FRpgPlayerSaveData();
	VersionedPlayer.SchemaVersion = FRpgPlayerSaveData::SemanticCarryRoleSchemaVersion;
	TestTrue(TEXT("Schema v2 profiles without progression migrate to default state"), VersionedPlayer.IsSchemaSupported());
	VersionedPlayer.bHasPlayerProgression = true;
	TestFalse(TEXT("Legacy schemas cannot claim a v3 player-progression payload"), VersionedPlayer.IsSchemaSupported());

	VersionedPlayer = FRpgPlayerSaveData();
	VersionedPlayer.bHasTradeSkillProgression = true;
	FTradeSkillState FirstSavedSkill;
	FirstSavedSkill.SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	VersionedPlayer.TradeSkillStates = { FirstSavedSkill, FirstSavedSkill };
	TestFalse(TEXT("Duplicate saved trade-skill tags are rejected"), VersionedPlayer.IsSchemaSupported());

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
