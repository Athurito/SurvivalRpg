#include "RpgInventoryGraphTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/Formatters/BinaryArchiveFormatter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryGraphIdentityAutomationTest,
	"SurvivalRpg.Inventory.Graph.IdentityAndDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryGraphIdentityAutomationTest::RunTest(const FString& Parameters)
{
	const FRpgInventoryItemId FirstId = FRpgInventoryItemId::NewId();
	const FRpgInventoryItemId SecondId = FRpgInventoryItemId::NewId();
	TestTrue(TEXT("Generated item id is valid"), FirstId.IsValid());
	TestTrue(TEXT("Generated item ids are distinct"), FirstId != SecondId);

	const FRpgInventoryContainerHandle Root = FRpgInventoryContainerHandle::MakeRoot(TEXT("Pockets"));
	TestTrue(TEXT("Root handle is valid"), Root.IsValid());
	TestTrue(TEXT("Root handle reports root semantics"), Root.IsRoot());
	TestEqual(TEXT("Direct child of a root has depth one"), Root.GetDirectChildDepth(), static_cast<uint8>(1));

	const FRpgInventoryContainerHandle DepthFour =
		FRpgInventoryContainerHandle::MakeItemOwned(FirstId, TEXT("Main"), RpgInventoryMaxItemOwnedDepth);
	TestTrue(TEXT("Depth four is accepted"), DepthFour.IsValid());
	TestFalse(TEXT("Depth four cannot create another item-owned level"), DepthFour.CanContainChildContainer());

	const FRpgInventoryContainerHandle DepthFive =
		FRpgInventoryContainerHandle::MakeItemOwned(FirstId, TEXT("Main"), RpgInventoryMaxItemOwnedDepth + 1);
	TestFalse(TEXT("Depth five is rejected"), DepthFive.IsValid());

	TArray<uint8> SerializedHandle;
	{
		FMemoryWriter Writer(SerializedHandle, true);
		FRpgInventoryContainerHandle MutableSource = DepthFour;
		bool bSerializeSucceeded = false;
		MutableSource.NetSerialize(Writer, nullptr, bSerializeSucceeded);
		TestTrue(TEXT("Container handle writes successfully"), bSerializeSucceeded);
	}

	FRpgInventoryContainerHandle RoundTrippedHandle;
	{
		FMemoryReader Reader(SerializedHandle, true);
		bool bSerializeSucceeded = false;
		RoundTrippedHandle.NetSerialize(Reader, nullptr, bSerializeSucceeded);
		TestTrue(TEXT("Container handle reads successfully"), bSerializeSucceeded);
	}
	TestTrue(TEXT("Container handle preserves identity across serialization"), RoundTrippedHandle == DepthFour);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryGraphPlacementAutomationTest,
	"SurvivalRpg.Inventory.Graph.PlacementCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryGraphPlacementAutomationTest::RunTest(const FString& Parameters)
{
	FRpgInventoryGridPlacement LegacyPlacement;
	LegacyPlacement.ContainerId_DEPRECATED = TEXT("Pockets");
	LegacyPlacement.X = 0;
	LegacyPlacement.Y = 0;
	LegacyPlacement.Width = 2;
	LegacyPlacement.Height = 1;

	FRpgInventoryGridPlacement ExplicitRootPlacement = LegacyPlacement;
	ExplicitRootPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeRoot(TEXT("Pockets")));
	TestFalse(TEXT("A deprecated ContainerId-only placement is not runtime-valid"), LegacyPlacement.IsValid());
	TestFalse(TEXT("Deprecated root-only data does not synthesize a runtime handle"), LegacyPlacement.GetContainerHandle().IsValid());
	TestFalse(TEXT("A deprecated-only placement cannot overlap canonical runtime state"), LegacyPlacement.Overlaps(ExplicitRootPlacement));
	TestTrue(TEXT("An explicit root handle produces a valid placement"), ExplicitRootPlacement.IsValid());
	TestTrue(TEXT("Assigning a canonical handle clears the deprecated shadow"), ExplicitRootPlacement.ContainerId_DEPRECATED.IsNone());

	FRpgInventoryGridPlacement StaleLegacyShadow = ExplicitRootPlacement;
	StaleLegacyShadow.ContainerId_DEPRECATED = TEXT("StaleRoot");
	TestTrue(TEXT("A stale deprecated shadow cannot change canonical placement equality"), StaleLegacyShadow == ExplicitRootPlacement);
	TestTrue(TEXT("A stale deprecated shadow cannot change canonical overlap"), StaleLegacyShadow.Overlaps(ExplicitRootPlacement));

	const FProperty* LegacyProperty = FindFProperty<FProperty>(
		FRpgInventoryGridPlacement::StaticStruct(),
		TEXT("ContainerId"));
	if (TestNotNull(TEXT("The historical serialized property remains loadable by its original name"), LegacyProperty))
	{
		TestTrue(TEXT("The historical property is flagged deprecated"), LegacyProperty->HasAnyPropertyFlags(CPF_Deprecated));
		TestTrue(TEXT("The historical property remains available to SaveGame migration"), LegacyProperty->HasAnyPropertyFlags(CPF_SaveGame));
		TestFalse(TEXT("The historical property is no longer editable"), LegacyProperty->HasAnyPropertyFlags(CPF_Edit));
		TestFalse(TEXT("The historical property is no longer Blueprint-visible"), LegacyProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	}

	FRpgInventoryContainerHandle HistoricalTaggedLayout;
	HistoricalTaggedLayout.ContainerId = TEXT("HistoricalRoot");
	TArray<uint8> HistoricalTaggedBytes;
	{
		FMemoryWriter Writer(HistoricalTaggedBytes, true);
		FBinaryArchiveFormatter Formatter(Writer);
		FStructuredArchive Archive(Formatter);
		FRpgInventoryContainerHandle::StaticStruct()->SerializeTaggedProperties(
			Archive.Open(),
			reinterpret_cast<uint8*>(&HistoricalTaggedLayout),
			nullptr,
			nullptr);
	}
	FRpgInventoryGridPlacement LoadedHistoricalPlacement;
	{
		FMemoryReader Reader(HistoricalTaggedBytes, true);
		FBinaryArchiveFormatter Formatter(Reader);
		FStructuredArchive Archive(Formatter);
		FRpgInventoryGridPlacement::StaticStruct()->SerializeTaggedProperties(
			Archive.Open(),
			reinterpret_cast<uint8*>(&LoadedHistoricalPlacement),
			nullptr,
			nullptr);
	}
	TestEqual(
		TEXT("A historical tagged ContainerId still loads into the deprecated migration field"),
		LoadedHistoricalPlacement.ContainerId_DEPRECATED,
		FName(TEXT("HistoricalRoot")));
	TestFalse(
		TEXT("Loading historical tagged data never synthesizes a canonical runtime handle"),
		LoadedHistoricalPlacement.ContainerHandle.IsValid());

	FRpgInventoryGridPlacement RotatedPlacement = ExplicitRootPlacement;
	RotatedPlacement.bRotated = true;
	TestTrue(TEXT("Placement equality distinguishes rotation"), RotatedPlacement != ExplicitRootPlacement);

	const FRpgInventoryItemId FirstBag = FRpgInventoryItemId::NewId();
	const FRpgInventoryItemId SecondBag = FRpgInventoryItemId::NewId();
	FRpgInventoryGridPlacement FirstBagPlacement = ExplicitRootPlacement;
	FirstBagPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(FirstBag, TEXT("Main"), 1));
	FRpgInventoryGridPlacement SecondBagPlacement = ExplicitRootPlacement;
	SecondBagPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(SecondBag, TEXT("Main"), 1));
	TestFalse(
		TEXT("Identical local coordinates in different item-owned containers never overlap"),
		FirstBagPlacement.Overlaps(SecondBagPlacement));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
