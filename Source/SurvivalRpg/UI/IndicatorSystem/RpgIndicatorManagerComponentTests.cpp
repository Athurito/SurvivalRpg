#if WITH_DEV_AUTOMATION_TESTS

#include "RpgIndicatorManagerComponent.h"

#include "IndicatorDescriptor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgIndicatorManagerRegistryTest,
	"SurvivalRpg.UI.Indicator.ManagerRegistry",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgIndicatorManagerRegistryTest::RunTest(
	const FString& Parameters)
{
	URpgIndicatorManagerComponent* Manager =
		NewObject<URpgIndicatorManagerComponent>();
	UIndicatorDescriptor* Descriptor =
		NewObject<UIndicatorDescriptor>();
	if (!TestNotNull(
			TEXT("Indicator manager is created"),
			Manager) ||
		!TestNotNull(
			TEXT("Indicator descriptor is created"),
			Descriptor))
	{
		return false;
	}

	int32 AddedCount = 0;
	int32 RemovedCount = 0;
	UIndicatorDescriptor* LastAdded = nullptr;
	UIndicatorDescriptor* LastRemoved = nullptr;
	Manager->OnIndicatorAdded.AddLambda(
		[&AddedCount, &LastAdded](
			UIndicatorDescriptor* AddedDescriptor)
		{
			++AddedCount;
			LastAdded = AddedDescriptor;
		});
	Manager->OnIndicatorRemoved.AddLambda(
		[&RemovedCount, &LastRemoved](
			UIndicatorDescriptor* RemovedDescriptor)
		{
			++RemovedCount;
			LastRemoved = RemovedDescriptor;
		});

	Manager->AddIndicator(Descriptor);

	TestEqual(
		TEXT("Add broadcasts exactly once"),
		AddedCount,
		1);
	TestEqual(
		TEXT("Add broadcasts the registered descriptor"),
		LastAdded,
		Descriptor);
	TestEqual(
		TEXT("Descriptor points back to its registry"),
		Descriptor->GetIndicatorManagerComponent(),
		Manager);
	TestEqual(
		TEXT(
			"Registry retains the descriptor for a layer that "
			"binds later"),
		Manager->GetIndicators().Num(),
		1);
	TestTrue(
		TEXT("Registry retains the exact descriptor"),
		Manager->GetIndicators().Contains(Descriptor));

	Manager->RemoveIndicator(Descriptor);

	TestEqual(
		TEXT("Remove broadcasts exactly once"),
		RemovedCount,
		1);
	TestEqual(
		TEXT("Remove broadcasts the unregistered descriptor"),
		LastRemoved,
		Descriptor);
	TestTrue(
		TEXT("Registry is empty after removal"),
		Manager->GetIndicators().IsEmpty());

	return true;
}

#endif
