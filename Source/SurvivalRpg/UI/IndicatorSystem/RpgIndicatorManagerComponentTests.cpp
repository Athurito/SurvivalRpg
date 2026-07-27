#if WITH_DEV_AUTOMATION_TESTS

#include "RpgIndicatorManagerComponent.h"

#include "IndicatorDescriptor.h"
#include "Components/SceneComponent.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgIndicatorDescriptorProjectionRevisionTest,
	"SurvivalRpg.UI.Indicator.ProjectionRevision",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgIndicatorDescriptorProjectionRevisionTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	UIndicatorDescriptor* Descriptor = NewObject<UIndicatorDescriptor>();
	USceneComponent* FirstComponent = NewObject<USceneComponent>();
	USceneComponent* SecondComponent = NewObject<USceneComponent>();
	if (!TestNotNull(TEXT("Indicator descriptor exists"), Descriptor) ||
		!TestNotNull(TEXT("First projection component exists"), FirstComponent) ||
		!TestNotNull(TEXT("Second projection component exists"), SecondComponent))
	{
		return false;
	}

	const uint32 InitialRevision = Descriptor->GetProjectionRevision();
	Descriptor->SetSceneComponent(FirstComponent);
	const uint32 FirstPlacementRevision = Descriptor->GetProjectionRevision();
	TestTrue(
		TEXT("Changing the projection component advances the revision"),
		FirstPlacementRevision > InitialRevision);
	Descriptor->SetSceneComponent(FirstComponent);
	TestEqual(
		TEXT("Reapplying identical placement does not dirty the descriptor"),
		Descriptor->GetProjectionRevision(),
		FirstPlacementRevision);

	Descriptor->SetSceneComponent(SecondComponent);
	TestTrue(
		TEXT("Retargeting to another actor component advances the revision"),
		Descriptor->GetProjectionRevision() > FirstPlacementRevision);
	const uint32 SecondPlacementRevision = Descriptor->GetProjectionRevision();
	Descriptor->SetWorldPositionOverride(FVector(10.0f, 20.0f, 30.0f));
	TestTrue(
		TEXT("Changing an absolute instance point advances the revision"),
		Descriptor->GetProjectionRevision() > SecondPlacementRevision);
	const uint32 OverrideRevision = Descriptor->GetProjectionRevision();
	Descriptor->SetWorldPositionOverride(FVector(10.0f, 20.0f, 30.0f));
	TestEqual(
		TEXT("Reapplying an identical instance point is stable"),
		Descriptor->GetProjectionRevision(),
		OverrideRevision);
	Descriptor->ClearWorldPositionOverride();
	TestTrue(
		TEXT("Returning to component projection advances the revision"),
		Descriptor->GetProjectionRevision() > OverrideRevision);
	return true;
}

#endif
