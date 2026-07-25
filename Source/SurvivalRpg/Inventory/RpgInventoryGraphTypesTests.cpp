#include "RpgInventoryGraphTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RpgPlayerInventoryLayoutTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
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
	FRpgInventorySlotAddressCanonicalIdentityAutomationTest,
	"SurvivalRpg.Inventory.Graph.SlotAddressCanonicalIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventorySlotAddressCanonicalIdentityAutomationTest::RunTest(const FString& Parameters)
{
	const FRpgInventoryItemId FirstOwner(FGuid(1, 2, 3, 4));
	const FRpgInventoryItemId SecondOwner(FGuid(5, 6, 7, 8));
	const FRpgInventoryContainerHandle FirstHandle =
		FRpgInventoryContainerHandle::MakeItemOwned(FirstOwner, TEXT("Main"), 1);
	const FRpgInventoryContainerHandle SecondHandle =
		FRpgInventoryContainerHandle::MakeItemOwned(SecondOwner, TEXT("Main"), 1);

	FRpgInventorySlotAddress FirstAddress;
	FirstAddress.SetContainerHandle(FirstHandle);
	FirstAddress.X = 2;
	FirstAddress.Y = 3;
	FRpgInventorySlotAddress SecondAddress;
	SecondAddress.SetContainerHandle(SecondHandle);
	SecondAddress.X = FirstAddress.X;
	SecondAddress.Y = FirstAddress.Y;

	TestTrue(TEXT("The first item-owned slot address is valid"), FirstAddress.IsValid());
	TestTrue(TEXT("The second item-owned slot address is valid"), SecondAddress.IsValid());
	TestTrue(
		TEXT("Equal local ids and coordinates remain distinct when their owning items differ"),
		FirstAddress != SecondAddress);
	TMap<FRpgInventorySlotAddress, int32> AddressValues;
	AddressValues.Add(FirstAddress, 11);
	AddressValues.Add(SecondAddress, 22);
	TestEqual(TEXT("A map retains both canonical item-owned addresses"), AddressValues.Num(), 2);
	TestEqual(TEXT("The first canonical address resolves its own map value"), AddressValues.FindRef(FirstAddress), 11);
	TestEqual(TEXT("The second canonical address resolves its own map value"), AddressValues.FindRef(SecondAddress), 22);

	const UScriptStruct* SlotAddressStruct = FRpgInventorySlotAddress::StaticStruct();
	const FProperty* ContainerHandleProperty =
		FindFProperty<FProperty>(SlotAddressStruct, TEXT("ContainerHandle"));
	const FProperty* XProperty = FindFProperty<FProperty>(SlotAddressStruct, TEXT("X"));
	const FProperty* YProperty = FindFProperty<FProperty>(SlotAddressStruct, TEXT("Y"));
	if (TestNotNull(TEXT("ContainerHandle remains reflected"), ContainerHandleProperty))
	{
		TestTrue(
			TEXT("ContainerHandle participates in SaveGame serialization"),
			ContainerHandleProperty->HasAnyPropertyFlags(CPF_SaveGame));
	}
	if (TestNotNull(TEXT("X remains reflected"), XProperty))
	{
		TestTrue(TEXT("X participates in SaveGame serialization"), XProperty->HasAnyPropertyFlags(CPF_SaveGame));
	}
	if (TestNotNull(TEXT("Y remains reflected"), YProperty))
	{
		TestTrue(TEXT("Y participates in SaveGame serialization"), YProperty->HasAnyPropertyFlags(CPF_SaveGame));
	}

	FNameProperty* LegacyContainerIdProperty =
		FindFProperty<FNameProperty>(SlotAddressStruct, TEXT("ContainerId"));
	if (TestNotNull(
		TEXT("The historical ContainerId remains reflected under its serialized name"),
		LegacyContainerIdProperty))
	{
		TestTrue(
			TEXT("The historical ContainerId remains available to SaveGame migration"),
			LegacyContainerIdProperty->HasAnyPropertyFlags(CPF_SaveGame));
		TestTrue(
			TEXT("The historical ContainerId is marked deprecated"),
			LegacyContainerIdProperty->HasAnyPropertyFlags(CPF_Deprecated));

		FRpgInventorySlotAddress LegacyOnlyAddress;
		LegacyOnlyAddress.X = FirstAddress.X;
		LegacyOnlyAddress.Y = FirstAddress.Y;
		*LegacyContainerIdProperty->ContainerPtrToValuePtr<FName>(&LegacyOnlyAddress) = TEXT("Main");
		TestFalse(
			TEXT("A deprecated ContainerId-only slot address is not runtime-valid"),
			LegacyOnlyAddress.IsValid());
		TestFalse(
			TEXT("A deprecated ContainerId never synthesizes a canonical runtime handle"),
			LegacyOnlyAddress.GetContainerHandle().IsValid());
		TestTrue(
			TEXT("A deprecated-only address cannot alias a canonical item-owned address"),
			LegacyOnlyAddress != FirstAddress);

		FRpgInventorySlotAddress AddressWithStaleLegacyShadow = FirstAddress;
		*LegacyContainerIdProperty->ContainerPtrToValuePtr<FName>(&AddressWithStaleLegacyShadow) = TEXT("StaleRoot");
		TestTrue(
			TEXT("A stale deprecated shadow cannot change canonical slot-address equality"),
			AddressWithStaleLegacyShadow == FirstAddress);
		TestEqual(
			TEXT("A stale deprecated shadow cannot change the canonical slot-address hash"),
			GetTypeHash(AddressWithStaleLegacyShadow),
			GetTypeHash(FirstAddress));

		AddressWithStaleLegacyShadow.SetContainerHandle(SecondHandle);
		TestTrue(
			TEXT("SetContainerHandle clears the deprecated ContainerId shadow"),
			LegacyContainerIdProperty->GetPropertyValue_InContainer(&AddressWithStaleLegacyShadow).IsNone());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryGraphPlacementHandleContractAutomationTest,
	"SurvivalRpg.Inventory.Graph.PlacementHandleContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryGraphPlacementHandleContractAutomationTest::RunTest(
	const FString& Parameters)
{
	const FRpgInventoryContainerHandle PocketsHandle =
		FRpgInventoryContainerHandle::MakeRoot(TEXT("Pockets"));
	FRpgInventoryGridPlacement ExplicitRootPlacement;
	ExplicitRootPlacement.SetContainerHandle(PocketsHandle);
	ExplicitRootPlacement.X = 0;
	ExplicitRootPlacement.Y = 0;
	ExplicitRootPlacement.Width = 2;
	ExplicitRootPlacement.Height = 1;

	TestTrue(
		TEXT("An explicit root handle produces a valid placement"),
		ExplicitRootPlacement.IsValid());
	TestTrue(
		TEXT("SetContainerHandle stores the exact canonical graph address"),
		ExplicitRootPlacement.GetContainerHandle() == PocketsHandle);

	FRpgInventoryGridPlacement MissingHandlePlacement = ExplicitRootPlacement;
	MissingHandlePlacement.SetContainerHandle(FRpgInventoryContainerHandle());
	TestFalse(
		TEXT("A placement without a canonical handle is invalid"),
		MissingHandlePlacement.IsValid());
	TestFalse(
		TEXT("A placement without a canonical handle cannot overlap runtime state"),
		MissingHandlePlacement.Overlaps(ExplicitRootPlacement));

	const FProperty* ContainerHandleProperty = FindFProperty<FProperty>(
		FRpgInventoryGridPlacement::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(
			FRpgInventoryGridPlacement,
			ContainerHandle));
	if (TestNotNull(
		TEXT("ContainerHandle remains the reflected placement address"),
		ContainerHandleProperty))
	{
		TestTrue(
			TEXT("The canonical placement address remains persisted"),
			ContainerHandleProperty->HasAnyPropertyFlags(CPF_SaveGame));
		TestTrue(
			TEXT("The canonical placement address remains Blueprint-visible"),
			ContainerHandleProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	}

	TestNull(
		TEXT("The historical root-only ContainerId placement property is retired"),
		FindFProperty<FProperty>(
			FRpgInventoryGridPlacement::StaticStruct(),
			TEXT("ContainerId")));
	TestNull(
		TEXT("No C++-named deprecated placement shadow remains reflected"),
		FindFProperty<FProperty>(
			FRpgInventoryGridPlacement::StaticStruct(),
			TEXT("ContainerId_DEPRECATED")));

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
