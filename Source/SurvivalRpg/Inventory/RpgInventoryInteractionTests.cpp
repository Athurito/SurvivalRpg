#include "RpgInventoryDragDrop.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryInteractionSession.h"
#include "RpgInventoryItemInstance.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace RpgInventoryInteractionTests
{
	constexpr float TestCellSize = 70.0f;
	constexpr float TestCellPadding = 2.0f;

	FRpgInventoryGridSize MakeFootprint(int32 Width, int32 Height)
	{
		FRpgInventoryGridSize Footprint;
		Footprint.Width = Width;
		Footprint.Height = Height;
		return Footprint;
	}

	FString MakeCaseLabel(const TCHAR* AnchorName, const FRpgInventoryGridSize& Footprint, const TCHAR* Property)
	{
		return FString::Printf(
			TEXT("%s anchor on %dx%d: %s"),
			AnchorName,
			Footprint.Width,
			Footprint.Height,
			Property);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPointerDragAnchorCaptureTest,
	"SurvivalRpg.Inventory.Interaction.PointerDragAnchor.BottomRightCorner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPointerDragAnchorCaptureTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryInteractionTests;

	struct FAnchorCase
	{
		const TCHAR* Name;
		FVector2D NormalizedPointer;
	};

	const FRpgInventoryGridSize Footprints[] = {
		MakeFootprint(1, 1),
		MakeFootprint(2, 1),
		MakeFootprint(2, 2),
		MakeFootprint(3, 2)
	};
	const FAnchorCase AnchorCases[] = {
		{TEXT("Bottom"), FVector2D(0.35f, 0.999f)},
		{TEXT("Right"), FVector2D(0.999f, 0.35f)},
		{TEXT("BottomRightCorner"), FVector2D(0.999f, 0.999f)}
	};

	for (const FRpgInventoryGridSize& Footprint : Footprints)
	{
		const FVector2D SourceVisualSize = URpgInventoryDragVisualWidget::CalculateExactVisualSize(
			Footprint,
			false,
			TestCellSize,
			TestCellPadding);

		for (const FAnchorCase& AnchorCase : AnchorCases)
		{
			FRpgInventoryDragPayload Payload;
			Payload.ItemFootprint = Footprint;
			URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
				Payload,
				AnchorCase.NormalizedPointer * SourceVisualSize,
				SourceVisualSize);

			const FVector2D ExpectedCellSpace(
				AnchorCase.NormalizedPointer.X * Footprint.Width,
				AnchorCase.NormalizedPointer.Y * Footprint.Height);
			const FIntPoint ExpectedCell(
				FMath::Clamp(FMath::FloorToInt(ExpectedCellSpace.X), 0, Footprint.Width - 1),
				FMath::Clamp(FMath::FloorToInt(ExpectedCellSpace.Y), 0, Footprint.Height - 1));
			const FVector2D ExpectedWithinCell(
				ExpectedCellSpace.X - ExpectedCell.X,
				ExpectedCellSpace.Y - ExpectedCell.Y);

			TestTrue(
				*MakeCaseLabel(AnchorCase.Name, Footprint, TEXT("anchor is valid")),
				Payload.DragAnchor.bValid);
			TestEqual(
				*MakeCaseLabel(AnchorCase.Name, Footprint, TEXT("grabbed cell X")),
				Payload.DragAnchor.GrabbedCell.X,
				ExpectedCell.X);
			TestEqual(
				*MakeCaseLabel(AnchorCase.Name, Footprint, TEXT("grabbed cell Y")),
				Payload.DragAnchor.GrabbedCell.Y,
				ExpectedCell.Y);
			TestTrue(
				*MakeCaseLabel(AnchorCase.Name, Footprint, TEXT("within-cell X remains DPI independent")),
				FMath::IsNearlyEqual(Payload.DragAnchor.WithinCellNormalized.X, ExpectedWithinCell.X, KINDA_SMALL_NUMBER));
			TestTrue(
				*MakeCaseLabel(AnchorCase.Name, Footprint, TEXT("within-cell Y remains DPI independent")),
				FMath::IsNearlyEqual(Payload.DragAnchor.WithinCellNormalized.Y, ExpectedWithinCell.Y, KINDA_SMALL_NUMBER));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTargetGrabPixelsTest,
	"SurvivalRpg.Inventory.Interaction.TargetGrabPixels.CellMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTargetGrabPixelsTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryInteractionTests;

	FRpgInventoryDragPayload Payload;
	Payload.ItemFootprint = MakeFootprint(3, 2);
	Payload.DragAnchor.bValid = true;
	Payload.DragAnchor.GrabbedCell = FIntPoint(2, 1);
	Payload.DragAnchor.WithinCellNormalized = FVector2D(0.25f, 0.75f);

	const FVector2D GrabPixels = URpgInventoryDragDropCoordinator::ResolveTargetGrabPixels(
		Payload,
		false,
		TestCellSize,
		TestCellPadding);
	const FVector2D ExpectedGrabPixels(
		2.0f * (TestCellSize + TestCellPadding) + 0.25f * TestCellSize,
		1.0f * (TestCellSize + TestCellPadding) + 0.75f * TestCellSize);
	TestTrue(
		TEXT("Target grab pixels include the 2px gap before each grabbed cell"),
		GrabPixels.Equals(ExpectedGrabPixels, KINDA_SMALL_NUMBER));

	Payload.DragAnchor = FRpgInventoryDragAnchor();
	const FVector2D FallbackGrabPixels = URpgInventoryDragDropCoordinator::ResolveTargetGrabPixels(
		Payload,
		false,
		TestCellSize,
		TestCellPadding);
	TestTrue(
		TEXT("A payload without a pointer anchor falls back to the first cell center"),
		FallbackGrabPixels.Equals(FVector2D(35.0f, 35.0f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryInteractionRotationAnchorRoundTripTest,
	"SurvivalRpg.Inventory.Interaction.RotationAnchor.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryInteractionRotationAnchorRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryInteractionTests;

	URpgInventoryItemInstance* Item = NewObject<URpgInventoryItemInstance>(GetTransientPackage());
	URpgInventoryInteractionSession* Session = NewObject<URpgInventoryInteractionSession>(GetTransientPackage());
	if (!TestNotNull(TEXT("Transient item fixture exists"), Item) ||
		!TestNotNull(TEXT("World-free interaction session exists"), Session))
	{
		return false;
	}

	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::EquipmentSlot;
	Payload.ItemInstance = Item;
	Payload.EquipmentSlot = ERpgEquipmentSlot::MainHand;
	Payload.ItemFootprint = MakeFootprint(3, 2);
	Payload.DragAnchor.bValid = true;
	Payload.DragAnchor.GrabbedCell = FIntPoint(2, 1);
	Payload.DragAnchor.WithinCellNormalized = FVector2D(0.25f, 0.75f);
	Payload.DragAnchor.SourceVisualSize = FVector2D(214.0f, 142.0f);
	Payload.DragAnchor.SourcePointerOffset = FVector2D(161.5f, 124.5f);
	Payload.DragAnchor.SourceScreenVisualSize = FVector2D(321.0f, 213.0f);
	Payload.DragAnchor.SourceScreenPointerOffset = FVector2D(242.25f, 186.75f);

	const FRpgInventoryDragAnchor OriginalAnchor = Payload.DragAnchor;
	TestTrue(
		TEXT("The world-free session accepts a valid equipment payload"),
		Session->BeginInteraction(Payload, ERpgInventoryInteractionInputMode::Mouse));
	TestTrue(TEXT("First rotation succeeds"), Session->ToggleTargetRotation());

	const FRpgInventoryDragAnchor RotatedAnchor = Session->GetPayload().DragAnchor;
	TestEqual(TEXT("Clockwise rotation maps anchor cell X"), RotatedAnchor.GrabbedCell.X, 0);
	TestEqual(TEXT("Clockwise rotation maps anchor cell Y"), RotatedAnchor.GrabbedCell.Y, 2);
	TestTrue(
		TEXT("Clockwise rotation maps the within-cell point"),
		RotatedAnchor.WithinCellNormalized.Equals(FVector2D(0.25f, 0.25f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Rotated anchor records the current orientation"), RotatedAnchor.bRotated);

	TestTrue(TEXT("Second rotation succeeds"), Session->ToggleTargetRotation());
	const FRpgInventoryDragAnchor RoundTripAnchor = Session->GetPayload().DragAnchor;
	TestFalse(TEXT("Two rotations restore the original orientation"), Session->IsTargetRotated());
	TestEqual(TEXT("Two rotations restore anchor cell X"), RoundTripAnchor.GrabbedCell.X, OriginalAnchor.GrabbedCell.X);
	TestEqual(TEXT("Two rotations restore anchor cell Y"), RoundTripAnchor.GrabbedCell.Y, OriginalAnchor.GrabbedCell.Y);
	TestTrue(
		TEXT("Two rotations restore the within-cell point"),
		RoundTripAnchor.WithinCellNormalized.Equals(OriginalAnchor.WithinCellNormalized, KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Two rotations restore the source visual size"),
		RoundTripAnchor.SourceVisualSize.Equals(OriginalAnchor.SourceVisualSize, KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Two rotations restore the source pointer offset"),
		RoundTripAnchor.SourcePointerOffset.Equals(OriginalAnchor.SourcePointerOffset, KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Two rotations restore the DPI-scaled source visual size"),
		RoundTripAnchor.SourceScreenVisualSize.Equals(OriginalAnchor.SourceScreenVisualSize, KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Two rotations restore the DPI-scaled pointer offset"),
		RoundTripAnchor.SourceScreenPointerOffset.Equals(OriginalAnchor.SourceScreenPointerOffset, KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryFreeGhostDpiGeometryTest,
	"SurvivalRpg.Inventory.Interaction.FreeGhost.DpiScaledCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryFreeGhostDpiGeometryTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryInteractionTests;

	FRpgInventoryDragPayload Payload;
	Payload.ItemFootprint = MakeFootprint(2, 1);
	URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
		Payload,
		FVector2D(25.0f, 40.0f),
		FVector2D(100.0f, 50.0f));
	URpgInventoryDragDropCoordinator::CapturePointerDragAnchorScreenGeometry(
		Payload,
		FVector2D(1000.0f, 500.0f),
		FVector2D(1050.0f, 580.0f),
		FVector2D(200.0f, 100.0f));

	const FVector2D GhostCenter = URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(
		Payload,
		FVector2D(1200.0f, 700.0f));
	TestTrue(
		TEXT("Free ghost routing uses absolute Slate units instead of mixing local pixels with the screen pointer"),
		GhostCenter.Equals(FVector2D(1250.0f, 670.0f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventorySpatialPreviewDescriptorEquivalenceTest,
	"SurvivalRpg.Inventory.Interaction.SpatialPreviewDescriptor.Equivalence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventorySpatialPreviewDescriptorEquivalenceTest::RunTest(const FString& Parameters)
{
	FRpgInventorySpatialPreviewDescriptor BaseDescriptor;
	BaseDescriptor.bValid = true;
	BaseDescriptor.EntryId = FGuid::NewGuid();
	BaseDescriptor.Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	BaseDescriptor.Target.TargetPlacement.X = 2;
	BaseDescriptor.Target.TargetPlacement.Y = 3;
	BaseDescriptor.Target.TargetPlacement.Width = 2;
	BaseDescriptor.Target.TargetPlacement.Height = 1;
	BaseDescriptor.TargetPlacement = BaseDescriptor.Target.TargetPlacement;
	BaseDescriptor.PreviewState = ERpgInventoryInteractionPreviewState::Move;
	BaseDescriptor.SnappedLocalPosition = FVector2D(144.0f, 216.0f);
	BaseDescriptor.SnappedLocalSize = FVector2D(142.0f, 70.0f);
	BaseDescriptor.PointerScreenPosition = FVector2D(500.0f, 400.0f);

	FRpgInventorySpatialPreviewDescriptor PointerMoved = BaseDescriptor;
	PointerMoved.PointerScreenPosition = FVector2D(750.0f, 675.0f);
	TestTrue(
		TEXT("Pure pointer movement is equivalent after placement snapped to the same candidate"),
		BaseDescriptor.IsEquivalentTo(PointerMoved));

	FRpgInventorySpatialPreviewDescriptor VisiblePlacementChanged = BaseDescriptor;
	VisiblePlacementChanged.TargetPlacement.X += 1;
	TestFalse(
		TEXT("A changed canonical preview placement is not equivalent"),
		BaseDescriptor.IsEquivalentTo(VisiblePlacementChanged));

	FRpgInventorySpatialPreviewDescriptor RequestPlacementChanged = BaseDescriptor;
	RequestPlacementChanged.Target.TargetPlacement.Y += 1;
	TestFalse(
		TEXT("A changed commit target placement is not equivalent"),
		BaseDescriptor.IsEquivalentTo(RequestPlacementChanged));

	FRpgInventorySpatialPreviewDescriptor StateChanged = BaseDescriptor;
	StateChanged.PreviewState = ERpgInventoryInteractionPreviewState::Blocked;
	TestFalse(
		TEXT("A changed semantic preview state is not equivalent"),
		BaseDescriptor.IsEquivalentTo(StateChanged));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryDragVisualExactSizeTest,
	"SurvivalRpg.Inventory.Interaction.DragVisual.ExactSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryDragVisualExactSizeTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryInteractionTests;

	struct FVisualSizeCase
	{
		FRpgInventoryGridSize Footprint;
		bool bRotated;
		FVector2D ExpectedSize;
	};

	const FVisualSizeCase Cases[] = {
		{MakeFootprint(1, 1), false, FVector2D(70.0f, 70.0f)},
		{MakeFootprint(2, 1), false, FVector2D(142.0f, 70.0f)},
		{MakeFootprint(2, 2), false, FVector2D(142.0f, 142.0f)},
		{MakeFootprint(3, 2), false, FVector2D(214.0f, 142.0f)},
		{MakeFootprint(3, 2), true, FVector2D(142.0f, 214.0f)}
	};

	for (const FVisualSizeCase& TestCase : Cases)
	{
		const FVector2D ActualSize = URpgInventoryDragVisualWidget::CalculateExactVisualSize(
			TestCase.Footprint,
			TestCase.bRotated,
			TestCellSize,
			TestCellPadding);
		const FString Label = FString::Printf(
			TEXT("%dx%d%s footprint includes padding exactly once between cells"),
			TestCase.Footprint.Width,
			TestCase.Footprint.Height,
			TestCase.bRotated ? TEXT(" rotated") : TEXT(""));
		TestTrue(*Label, ActualSize.Equals(TestCase.ExpectedSize, KINDA_SMALL_NUMBER));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
