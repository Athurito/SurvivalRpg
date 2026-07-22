// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryLegacySnapshot.h"
#include "RpgInventoryManagerComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace RpgInventoryLegacySnapshotTests
{
	class FScopedInventoryWorld
	{
	public:
		FScopedInventoryWorld()
		{
			GameInstance = NewObject<UGameInstance>(
				GEngine,
				NAME_None,
				RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedInventoryWorld()
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

		bool IsValid() const
		{
			return GameInstance != nullptr && World != nullptr;
		}

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName) const
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				AActor::StaticClass(),
				FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor = World->SpawnActor<AActor>(SpawnParameters);
			if (!OwnerActor)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory =
				NewObject<URpgInventoryManagerComponent>(
					OwnerActor,
					MakeUniqueObjectName(
						OwnerActor,
						URpgInventoryManagerComponent::StaticClass(),
						TEXT("Inventory")),
					RF_Transient);
			OwnerActor->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	FRpgInventoryGridPlacement MakeRootPlacement(
		FName RootContainerId,
		int32 X,
		int32 Y,
		bool bRotated = false)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(
			FRpgInventoryContainerHandle::MakeRoot(RootContainerId));
		Placement.X = X;
		Placement.Y = Y;
		Placement.bRotated = bRotated;
		return Placement;
	}

	FRpgInventorySnapshotEntry MakeSpatialEntry(
		const FGuid& EntryId,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 StackCount,
		FName RootContainerId,
		int32 X,
		int32 Y)
	{
		FRpgInventorySnapshotEntry Entry;
		Entry.EntryId = EntryId;
		Entry.ItemDefinition = ItemDefinition;
		Entry.StackCount = StackCount;
		Entry.Placement = MakeRootPlacement(RootContainerId, X, Y);
		return Entry;
	}

	FRpgInventorySnapshotEntry MakeV0Entry(
		const FGuid& EntryId,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 SortIndex)
	{
		FRpgInventorySnapshotEntry Entry;
		Entry.EntryId = EntryId;
		Entry.ItemDefinition = ItemDefinition;
		Entry.StackCount = 1;
		Entry.SortIndex = SortIndex;
		return Entry;
	}

	const FRpgInventorySavedItem* FindSavedItem(
		const FRpgInventoryGraphSaveData& SaveData,
		const FRpgInventoryItemId& ItemId)
	{
		return SaveData.Items.FindByPredicate(
			[&ItemId](const FRpgInventorySavedItem& Item)
			{
				return Item.ItemId == ItemId;
			});
	}

	bool AreRuntimePayloadsEqual(
		const TArray<FRpgInventoryFragmentStatePayload>& A,
		const TArray<FRpgInventoryFragmentStatePayload>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].FragmentId != B[Index].FragmentId ||
				A[Index].Version != B[Index].Version ||
				A[Index].Payload != B[Index].Payload)
			{
				return false;
			}
		}
		return true;
	}

	bool AreSavedItemsEquivalent(
		const FRpgInventorySavedItem& A,
		const FRpgInventorySavedItem& B)
	{
		return A.ItemId == B.ItemId &&
			A.ItemDefinition == B.ItemDefinition &&
			A.StackCount == B.StackCount &&
			A.Container == B.Container &&
			A.Placement == B.Placement &&
			AreRuntimePayloadsEqual(A.RuntimeState, B.RuntimeState);
	}

	bool GetEntryView(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		FRpgInventoryEntryView& OutEntry)
	{
		if (!Inventory)
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry :
			 Inventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	FString MakeStrictInventorySignature(
		const URpgInventoryManagerComponent* Inventory)
	{
		if (!Inventory)
		{
			return TEXT("InvalidInventory");
		}

		TArray<FString> Rows;
		for (const FRpgInventoryEntryView& Entry :
			 Inventory->GetAllEntries())
		{
			Rows.Add(FString::Printf(
				TEXT("%s|%s|%p|%d|%s|%s|%d|%d|%d|%d|%d"),
				*Entry.EntryId.ToString(),
				*Entry.ItemId.ToString(),
				static_cast<const void*>(Entry.Instance.Get()),
				Entry.StackCount,
				*Entry.Placement.ContainerHandle.ToString(),
				*Entry.Placement.ContainerId.ToString(),
				Entry.Placement.X,
				Entry.Placement.Y,
				Entry.Placement.Width,
				Entry.Placement.Height,
				Entry.Placement.bRotated ? 1 : 0));
		}
		Rows.Sort();
		return FString::Printf(
			TEXT("Revision=%d;%s"),
			Inventory->GetInventoryRevision(),
			*FString::Join(Rows, TEXT(";")));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLegacySpatialSnapshotConversionTest,
	"SurvivalRpg.Inventory.LegacySnapshot.SpatialV1.DeterministicCanonicalConversion",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgLegacySpatialSnapshotConversionTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryLegacySnapshotTests;
	FScopedInventoryWorld TestWorld;
	if (!TestTrue(TEXT("Legacy conversion test world exists"), TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("LegacySpatialConverter"));
	if (!TestNotNull(TEXT("Legacy spatial converter exists"), Inventory))
	{
		return false;
	}

	const FName RootId = Inventory->GetDefaultContainerId();
	const FRpgInventoryContainerHandle Root =
		FRpgInventoryContainerHandle::MakeRoot(RootId);
	const FGuid LegacyEntryId = FGuid::NewGuid();
	const FRpgInventoryItemId DerivedItemId(LegacyEntryId);

	FRpgInventorySnapshot Snapshot;
	Snapshot.ContainerId = RootId;
	FRpgInventorySnapshotEntry& LegacyEntry =
		Snapshot.Entries.AddDefaulted_GetRef();
	LegacyEntry.EntryId = LegacyEntryId;
	LegacyEntry.ItemDefinition =
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	LegacyEntry.StackCount = 1;
	// SpatialV1 could contain only the compatibility ContainerId rather than an explicit graph handle.
	LegacyEntry.Placement.ContainerId = RootId;
	LegacyEntry.Placement.X = 2;
	LegacyEntry.Placement.Y = 1;
	LegacyEntry.Placement.Width = 37;
	LegacyEntry.Placement.Height = 41;

	FRpgInventoryGraphSaveData FirstConversion;
	FString FirstError;
	TestTrue(
		TEXT("A valid SpatialV1 root snapshot converts"),
		Inventory->ConvertLegacyInventorySnapshot(
			ERpgLegacyInventorySnapshotVersion::SpatialV1,
			Snapshot,
			RootId,
			FirstConversion,
			FirstError));
	TestTrue(TEXT("Successful conversion leaves no error"), FirstError.IsEmpty());
	TestEqual(
		TEXT("Conversion always writes the current graph schema"),
		FirstConversion.SchemaVersion,
		FRpgInventoryGraphSaveData::CurrentSchemaVersion);
	TestEqual(TEXT("One legacy row becomes one graph row"), FirstConversion.Items.Num(), 1);
	if (FirstConversion.Items.Num() != 1)
	{
		return false;
	}

	const FRpgInventorySavedItem& ConvertedItem = FirstConversion.Items[0];
	TestEqual(
		TEXT("A missing persistent id is deterministically seeded from the legacy entry id"),
		ConvertedItem.ItemId,
		DerivedItemId);
	TestEqual(
		TEXT("The converted row targets the canonical root handle"),
		ConvertedItem.Container,
		Root);
	TestEqual(
		TEXT("Placement owns the same canonical root handle"),
		ConvertedItem.Placement.GetContainerHandle(),
		Root);
	TestEqual(TEXT("Spatial X is preserved"), ConvertedItem.Placement.X, 2);
	TestEqual(TEXT("Spatial Y is preserved"), ConvertedItem.Placement.Y, 1);
	TestEqual(
		TEXT("Serialized legacy width is rebuilt from the current definition"),
		ConvertedItem.Placement.Width,
		1);
	TestEqual(
		TEXT("Serialized legacy height is rebuilt from the current definition"),
		ConvertedItem.Placement.Height,
		1);
	TestTrue(
		TEXT("Conversion synthesizes current default runtime state"),
		!ConvertedItem.RuntimeState.IsEmpty());

	FRpgInventoryGraphSaveData SecondConversion;
	FString SecondError;
	TestTrue(
		TEXT("The same SpatialV1 input converts repeatedly"),
		Inventory->ConvertLegacyInventorySnapshot(
			ERpgLegacyInventorySnapshotVersion::SpatialV1,
			Snapshot,
			RootId,
			SecondConversion,
			SecondError));
	TestTrue(TEXT("Repeated conversion leaves no error"), SecondError.IsEmpty());
	TestEqual(
		TEXT("Repeated conversion emits the same row count"),
		SecondConversion.Items.Num(),
		FirstConversion.Items.Num());
	if (SecondConversion.Items.Num() == 1)
	{
		TestTrue(
			TEXT("Repeated conversion is deterministic, including runtime payload bytes"),
			AreSavedItemsEquivalent(
				FirstConversion.Items[0],
				SecondConversion.Items[0]));
	}

	// Canonical restore, not the converter, owns runtime identity retention.
	URpgInventoryManagerComponent* IdentityInventory =
		TestWorld.CreateInventory(TEXT("LegacySpatialIdentityRestore"));
	if (!TestNotNull(TEXT("Identity restore inventory exists"), IdentityInventory))
	{
		return false;
	}
	URpgInventoryItemInstance* LiveItem =
		IdentityInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2,
			MakeRootPlacement(RootId, 0, 0));
	if (!TestNotNull(TEXT("Identity fixture item exists"), LiveItem))
	{
		return false;
	}
	FRpgInventoryEntryView LiveEntryBefore;
	if (!TestTrue(
			TEXT("Identity fixture entry resolves"),
			GetEntryView(
				IdentityInventory,
				LiveItem->GetItemId(),
				LiveEntryBefore)))
	{
		return false;
	}

	FRpgInventorySnapshot IdentitySnapshot;
	IdentitySnapshot.ContainerId = RootId;
	FRpgInventorySnapshotEntry IdentityLegacyEntry = MakeSpatialEntry(
		FGuid::NewGuid(),
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		5,
		RootId,
		4,
		3);
	IdentityLegacyEntry.ItemId = LiveItem->GetItemId();
	IdentitySnapshot.Entries.Add(IdentityLegacyEntry);

	FRpgInventoryGraphSaveData IdentityGraph;
	FString IdentityError;
	TestTrue(
		TEXT("A valid explicit legacy item id is preserved during conversion"),
		IdentityInventory->ConvertLegacyInventorySnapshot(
			ERpgLegacyInventorySnapshotVersion::SpatialV1,
			IdentitySnapshot,
			RootId,
			IdentityGraph,
			IdentityError));
	FRpgInventoryMutationResult RestoreResult;
	TestTrue(
		TEXT("The converted graph passes canonical restore"),
		IdentityInventory->RestoreInventoryGraph(
			IdentityGraph,
			RestoreResult));
	TestEqual(
		TEXT("Canonical restore reports success"),
		RestoreResult.Code,
		ERpgInventoryMutationResultCode::Success);

	URpgInventoryItemInstance* RestoredLiveItem =
		IdentityInventory->FindItemById(LiveItem->GetItemId());
	TestEqual(
		TEXT("Matching persistent identity and definition retain the live UObject"),
		RestoredLiveItem,
		LiveItem);
	FRpgInventoryEntryView LiveEntryAfter;
	if (TestTrue(
			TEXT("Restored live entry resolves"),
			GetEntryView(
				IdentityInventory,
				LiveItem->GetItemId(),
				LiveEntryAfter)))
	{
		TestEqual(
			TEXT("Canonical restore retains the live FastArray entry identity"),
			LiveEntryAfter.EntryId,
			LiveEntryBefore.EntryId);
		TestEqual(TEXT("Converted stack count commits"), LiveEntryAfter.StackCount, 5);
		TestEqual(TEXT("Converted X commits"), LiveEntryAfter.Placement.X, 4);
		TestEqual(TEXT("Converted Y commits"), LiveEntryAfter.Placement.Y, 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLegacySingleSlotSnapshotConversionTest,
	"SurvivalRpg.Inventory.LegacySnapshot.SingleSlotV0.SortIndexFirstFit",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgLegacySingleSlotSnapshotConversionTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryLegacySnapshotTests;
	FScopedInventoryWorld TestWorld;
	if (!TestTrue(TEXT("Legacy V0 test world exists"), TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("LegacySingleSlotConverter"));
	if (!TestNotNull(TEXT("Legacy V0 converter exists"), Inventory))
	{
		return false;
	}

	const FName RootId = Inventory->GetDefaultContainerId();
	const FRpgInventoryContainerHandle Root =
		FRpgInventoryContainerHandle::MakeRoot(RootId);
	const FGuid WideEntryId = FGuid::NewGuid();
	const FGuid UnitEntryId = FGuid::NewGuid();
	const FGuid LargeEntryId = FGuid::NewGuid();

	FRpgInventorySnapshot Snapshot;
	// Deliberately serialize rows out of order; SortIndex is the V0 ordering contract.
	Snapshot.Entries.Add(MakeV0Entry(
		LargeEntryId,
		URpgInventoryAutomationTestLargeItemDefinition::StaticClass(),
		2));
	Snapshot.Entries.Add(MakeV0Entry(
		WideEntryId,
		URpgInventoryAutomationTestFixedWideItemDefinition::StaticClass(),
		0));
	Snapshot.Entries.Add(MakeV0Entry(
		UnitEntryId,
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1));

	FRpgInventoryGraphSaveData ConvertedGraph;
	FString ConversionError;
	TestTrue(
		TEXT("Out-of-order SingleSlotV0 rows convert through shared FirstFit"),
		Inventory->ConvertLegacyInventorySnapshot(
			ERpgLegacyInventorySnapshotVersion::SingleSlotV0,
			Snapshot,
			RootId,
			ConvertedGraph,
			ConversionError));
	TestTrue(TEXT("Successful V0 conversion leaves no error"), ConversionError.IsEmpty());
	TestEqual(TEXT("Every V0 row is converted"), ConvertedGraph.Items.Num(), 3);

	const FRpgInventorySavedItem* WideItem = FindSavedItem(
		ConvertedGraph,
		FRpgInventoryItemId(WideEntryId));
	const FRpgInventorySavedItem* UnitItem = FindSavedItem(
		ConvertedGraph,
		FRpgInventoryItemId(UnitEntryId));
	const FRpgInventorySavedItem* LargeItem = FindSavedItem(
		ConvertedGraph,
		FRpgInventoryItemId(LargeEntryId));
	TestNotNull(TEXT("SortIndex zero row is present"), WideItem);
	TestNotNull(TEXT("SortIndex one row is present"), UnitItem);
	TestNotNull(TEXT("SortIndex two row is present"), LargeItem);
	if (!WideItem || !UnitItem || !LargeItem)
	{
		return false;
	}

	TestEqual(TEXT("SortIndex zero starts at X zero"), WideItem->Placement.X, 0);
	TestEqual(TEXT("SortIndex zero starts at Y zero"), WideItem->Placement.Y, 0);
	TestEqual(TEXT("The fixed wide footprint stays two cells wide"), WideItem->Placement.Width, 2);
	TestEqual(TEXT("SortIndex one follows the wide row"), UnitItem->Placement.X, 2);
	TestEqual(TEXT("SortIndex one remains on the first row"), UnitItem->Placement.Y, 0);
	TestEqual(TEXT("SortIndex two follows both earlier footprints"), LargeItem->Placement.X, 3);
	TestEqual(TEXT("SortIndex two remains on the first row"), LargeItem->Placement.Y, 0);
	TestEqual(TEXT("Every V0 placement uses the fallback root"), LargeItem->Container, Root);

	FRpgInventoryMutationResult RestoreResult;
	TestTrue(
		TEXT("The deterministically packed V0 graph passes canonical restore"),
		Inventory->RestoreInventoryGraph(ConvertedGraph, RestoreResult));
	TestEqual(
		TEXT("V0 canonical restore reports success"),
		RestoreResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("V0 restore reconstructs every entry"), Inventory->GetUsedEntryCount(), 3);

	URpgInventoryItemInstance* RestoredWide =
		Inventory->FindItemById(FRpgInventoryItemId(WideEntryId));
	URpgInventoryItemInstance* RestoredUnit =
		Inventory->FindItemById(FRpgInventoryItemId(UnitEntryId));
	URpgInventoryItemInstance* RestoredLarge =
		Inventory->FindItemById(FRpgInventoryItemId(LargeEntryId));
	TestEqual(
		TEXT("FirstFit restores the wide item at its first cell"),
		Inventory->GetItemAtContainerCell(Root, 0, 0),
		RestoredWide);
	TestEqual(
		TEXT("FirstFit restores the unit after the wide footprint"),
		Inventory->GetItemAtContainerCell(Root, 2, 0),
		RestoredUnit);
	TestEqual(
		TEXT("FirstFit restores the large item after prior footprints"),
		Inventory->GetItemAtContainerCell(Root, 3, 0),
		RestoredLarge);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLegacySnapshotFailClosedTest,
	"SurvivalRpg.Inventory.LegacySnapshot.Validation.FailClosedAndAtomicRestore",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgLegacySnapshotFailClosedTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryLegacySnapshotTests;
	FScopedInventoryWorld TestWorld;
	if (!TestTrue(TEXT("Legacy validation test world exists"), TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("LegacyValidationConverter"));
	if (!TestNotNull(TEXT("Legacy validation converter exists"), Inventory))
	{
		return false;
	}

	const FName RootId = Inventory->GetDefaultContainerId();
	const FRpgInventoryContainerHandle Root =
		FRpgInventoryContainerHandle::MakeRoot(RootId);
	auto MakeValidSpatialSnapshot = [RootId]()
	{
		FRpgInventorySnapshot Snapshot;
		Snapshot.ContainerId = RootId;
		Snapshot.Entries.Add(MakeSpatialEntry(
			FGuid::NewGuid(),
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			RootId,
			0,
			0));
		return Snapshot;
	};
	auto ExpectFailure = [this, Inventory, RootId](
		const TCHAR* CaseName,
		ERpgLegacyInventorySnapshotVersion Version,
		const FRpgInventorySnapshot& Snapshot)
	{
		FRpgInventoryGraphSaveData Output;
		Output.SchemaVersion = 9876;
		Output.Items.AddDefaulted();
		FString Error;
		const bool bConverted = Inventory->ConvertLegacyInventorySnapshot(
			Version,
			Snapshot,
			RootId,
			Output,
			Error);
		TestFalse(
			*FString::Printf(TEXT("%s is rejected"), CaseName),
			bConverted);
		TestEqual(
			*FString::Printf(TEXT("%s resets the output schema"), CaseName),
			Output.SchemaVersion,
			FRpgInventoryGraphSaveData::CurrentSchemaVersion);
		TestTrue(
			*FString::Printf(TEXT("%s leaves no partial graph rows"), CaseName),
			Output.Items.IsEmpty());
		TestTrue(
			*FString::Printf(TEXT("%s reports a diagnostic"), CaseName),
			!Error.IsEmpty());
	};

	ExpectFailure(
		TEXT("Unknown version"),
		static_cast<ERpgLegacyInventorySnapshotVersion>(255),
		MakeValidSpatialSnapshot());

	FRpgInventorySnapshot MissingIdentity = MakeValidSpatialSnapshot();
	MissingIdentity.Entries[0].EntryId.Invalidate();
	MissingIdentity.Entries[0].ItemId.Reset();
	ExpectFailure(
		TEXT("Missing entry and item identity"),
		ERpgLegacyInventorySnapshotVersion::SpatialV1,
		MissingIdentity);

	FRpgInventorySnapshot DuplicateResultIdentity = MakeValidSpatialSnapshot();
	const FGuid DuplicateGuid = FGuid::NewGuid();
	DuplicateResultIdentity.Entries[0].ItemId =
		FRpgInventoryItemId(DuplicateGuid);
	FRpgInventorySnapshotEntry DuplicateRow = MakeSpatialEntry(
		DuplicateGuid,
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		RootId,
		1,
		0);
	DuplicateResultIdentity.Entries.Add(DuplicateRow);
	ExpectFailure(
		TEXT("Duplicate derived result identity"),
		ERpgLegacyInventorySnapshotVersion::SpatialV1,
		DuplicateResultIdentity);

	FRpgInventorySnapshot DuplicateSortIndex;
	DuplicateSortIndex.Entries.Add(MakeV0Entry(
		FGuid::NewGuid(),
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		0));
	DuplicateSortIndex.Entries.Add(MakeV0Entry(
		FGuid::NewGuid(),
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		0));
	ExpectFailure(
		TEXT("Duplicate V0 SortIndex"),
		ERpgLegacyInventorySnapshotVersion::SingleSlotV0,
		DuplicateSortIndex);

	FRpgInventorySnapshot NegativeSortIndex;
	NegativeSortIndex.Entries.Add(MakeV0Entry(
		FGuid::NewGuid(),
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		-2));
	ExpectFailure(
		TEXT("Negative V0 SortIndex"),
		ERpgLegacyInventorySnapshotVersion::SingleSlotV0,
		NegativeSortIndex);

	FRpgInventorySnapshot EnvelopeMismatch = MakeValidSpatialSnapshot();
	EnvelopeMismatch.Entries[0].Placement.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeRoot(TEXT("OtherRoot")));
	ExpectFailure(
		TEXT("Envelope and placement root mismatch"),
		ERpgLegacyInventorySnapshotVersion::SpatialV1,
		EnvelopeMismatch);

	FRpgInventorySnapshot ItemOwnedPlacement = MakeValidSpatialSnapshot();
	ItemOwnedPlacement.Entries[0].Placement.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeItemOwned(
			FRpgInventoryItemId::NewId(),
			TEXT("Main"),
			1));
	ExpectFailure(
		TEXT("Item-owned legacy placement"),
		ERpgLegacyInventorySnapshotVersion::SpatialV1,
		ItemOwnedPlacement);

	FRpgInventorySnapshot PartialHandle = MakeValidSpatialSnapshot();
	PartialHandle.Entries[0].Placement.ContainerHandle =
		FRpgInventoryContainerHandle();
	PartialHandle.Entries[0].Placement.ContainerHandle.Root = RootId;
	PartialHandle.Entries[0].Placement.ContainerId = RootId;
	ExpectFailure(
		TEXT("Partially populated container handle"),
		ERpgLegacyInventorySnapshotVersion::SpatialV1,
		PartialHandle);

	// A legacy definition can describe an old stackable container provider, but only canonical graph restore
	// decides whether that graph can become authoritative under current provider invariants.
	URpgInventoryItemInstance* ExistingItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakeRootPlacement(RootId, 9, 5));
	if (!TestNotNull(TEXT("Atomic restore fixture item exists"), ExistingItem))
	{
		return false;
	}
	const FString BeforeSignature = MakeStrictInventorySignature(Inventory);
	const uint64 BeforeEpoch = Inventory->GetMutationEpoch();
	const int32 BeforeRevision = Inventory->GetInventoryRevision();
	const FRpgInventoryItemId ExistingItemId = ExistingItem->GetItemId();

	FRpgInventorySnapshot ProviderStackSnapshot;
	ProviderStackSnapshot.ContainerId = RootId;
	ProviderStackSnapshot.Entries.Add(MakeSpatialEntry(
		FGuid::NewGuid(),
		URpgInventoryAutomationTestLegacyStackableBagItemDefinition::StaticClass(),
		2,
		RootId,
		0,
		0));
	FRpgInventoryGraphSaveData ProviderStackGraph;
	FString ProviderConversionError;
	TestTrue(
		TEXT("Legacy provider stack is syntactically convertible"),
		Inventory->ConvertLegacyInventorySnapshot(
			ERpgLegacyInventorySnapshotVersion::SpatialV1,
			ProviderStackSnapshot,
			RootId,
			ProviderStackGraph,
			ProviderConversionError));
	TestTrue(
		TEXT("Syntactic provider conversion leaves no error"),
		ProviderConversionError.IsEmpty());

	FRpgInventoryMutationResult RestoreResult;
	TestFalse(
		TEXT("Canonical restore rejects a container provider stack above one"),
		Inventory->RestoreInventoryGraph(
			ProviderStackGraph,
			RestoreResult));
	TestTrue(
		TEXT("Rejected provider restore does not report success"),
		RestoreResult.Code != ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("Rejected restore preserves the complete live graph and UObject identity"),
		MakeStrictInventorySignature(Inventory),
		BeforeSignature);
	TestEqual(
		TEXT("Rejected restore preserves the replicated revision"),
		Inventory->GetInventoryRevision(),
		BeforeRevision);
	TestEqual(
		TEXT("Rejected restore preserves the command epoch"),
		Inventory->GetMutationEpoch(),
		BeforeEpoch);
	TestEqual(
		TEXT("Rejected restore retains the original item UObject"),
		Inventory->FindItemById(ExistingItemId),
		ExistingItem);
	TestEqual(
		TEXT("Rejected restore retains exactly one existing row"),
		Inventory->GetUsedEntryCount(),
		1);
	TestEqual(
		TEXT("Rejected restore leaves the original occupied cell unchanged"),
		Inventory->GetItemAtContainerCell(Root, 9, 5),
		ExistingItem);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
