#include "RpgInventoryGraphTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

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
	LegacyPlacement.ContainerId = TEXT("Pockets");
	LegacyPlacement.X = 0;
	LegacyPlacement.Y = 0;
	LegacyPlacement.Width = 2;
	LegacyPlacement.Height = 1;

	FRpgInventoryGridPlacement ExplicitRootPlacement = LegacyPlacement;
	ExplicitRootPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeRoot(TEXT("Pockets")));
	TestTrue(TEXT("Legacy root placement remains valid"), LegacyPlacement.IsValid());
	TestTrue(TEXT("Legacy and explicit root handles resolve equally"), LegacyPlacement == ExplicitRootPlacement);
	TestTrue(TEXT("Legacy and explicit root placements overlap"), LegacyPlacement.Overlaps(ExplicitRootPlacement));

	FRpgInventoryGridPlacement RotatedPlacement = ExplicitRootPlacement;
	RotatedPlacement.bRotated = true;
	TestTrue(TEXT("Placement equality distinguishes rotation"), RotatedPlacement != ExplicitRootPlacement);

	const FRpgInventoryItemId FirstBag = FRpgInventoryItemId::NewId();
	const FRpgInventoryItemId SecondBag = FRpgInventoryItemId::NewId();
	FRpgInventoryGridPlacement FirstBagPlacement = LegacyPlacement;
	FirstBagPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(FirstBag, TEXT("Main"), 1));
	FRpgInventoryGridPlacement SecondBagPlacement = LegacyPlacement;
	SecondBagPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(SecondBag, TEXT("Main"), 1));
	TestFalse(
		TEXT("Identical local coordinates in different item-owned containers never overlap"),
		FirstBagPlacement.Overlaps(SecondBagPlacement));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
