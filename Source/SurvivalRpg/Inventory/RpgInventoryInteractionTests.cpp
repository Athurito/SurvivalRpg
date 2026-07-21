#include "RpgInventoryDragDrop.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryInteractionSession.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"

#include "Components/Button.h"
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

	void PopulateExactEquipmentSourceSnapshot(
		FRpgInventoryDragPayload& Payload,
		UObject* Outer,
		FName ContainerId)
	{
		Payload.SourceInventory = NewObject<URpgInventoryManagerComponent>(Outer);
		Payload.EntryId = FGuid::NewGuid();
		Payload.StackCount = 1;
		Payload.SourcePlacement.SetContainerHandle(
			FRpgInventoryContainerHandle::MakeRoot(ContainerId));
		Payload.SourcePlacement.X = 0;
		Payload.SourcePlacement.Y = 0;
		Payload.SourcePlacement.Width = 1;
		Payload.SourcePlacement.Height = 1;
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
	PopulateExactEquipmentSourceSnapshot(
		Payload,
		GetTransientPackage(),
		TEXT("RotationAnchorEquipmentSource"));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacedVisualRotationTest,
	"SurvivalRpg.Inventory.Interaction.PlacedVisual.AuthoritativeRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacedVisualRotationTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryInteractionTests;

	URpgInventoryDragVisualWidget* Visual = NewObject<URpgInventoryDragVisualWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("World-free placed-item visual fixture exists"), Visual))
	{
		return false;
	}

	FRpgInventoryDragPayload Payload;
	Payload.ItemFootprint = MakeFootprint(3, 2);
	Payload.StackCount = 1;
	Payload.SourcePlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeRoot(FName(TEXT("Backpack"))));
	Payload.SourcePlacement.X = 0;
	Payload.SourcePlacement.Y = 0;
	Payload.SourcePlacement.Width = 3;
	Payload.SourcePlacement.Height = 2;
	Payload.SourcePlacement.bRotated = true;
	Payload.DragAnchor.bValid = true;
	Payload.DragAnchor.bRotated = false;

	Visual->ConfigureFromPayload(Payload, TestCellSize, TestCellPadding);
	TestEqual(TEXT("Placed rotation swaps occupied width"), Visual->GetOccupiedFootprint().Width, 2);
	TestEqual(TEXT("Placed rotation swaps occupied height"), Visual->GetOccupiedFootprint().Height, 3);
	TestTrue(
		TEXT("Placed rotation produces the same exact size as the snapped ghost"),
		Visual->GetExactVisualSize().Equals(FVector2D(142.0f, 214.0f), KINDA_SMALL_NUMBER));

	Payload.SourcePlacement.bRotated = false;
	Payload.DragAnchor.bRotated = true;
	Visual->ConfigureFromPayload(Payload, TestCellSize, TestCellPadding);
	TestEqual(TEXT("Placement, not stale drag-anchor state, restores occupied width"), Visual->GetOccupiedFootprint().Width, 3);
	TestEqual(TEXT("Placement, not stale drag-anchor state, restores occupied height"), Visual->GetOccupiedFootprint().Height, 2);
	TestTrue(
		TEXT("Unrotated placement restores its exact snapped size"),
		Visual->GetExactVisualSize().Equals(FVector2D(214.0f, 142.0f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryRotatedIconGeometryTest,
	"SurvivalRpg.Inventory.Interaction.PlacedVisual.RotatedIconGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryRotatedIconGeometryTest::RunTest(const FString& Parameters)
{
	FVector2D PaintPosition;
	FVector2D PaintSize;
	URpgInventoryDragVisualWidget::CalculateIconPaintGeometry(
		FVector2D(142.0f, 214.0f),
		true,
		4.0f,
		PaintPosition,
		PaintSize);

	TestTrue(
		TEXT("The pre-rotation icon quad swaps dimensions instead of stretching into the occupied bounds"),
		PaintSize.Equals(FVector2D(206.0f, 134.0f), KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("The swapped icon quad remains centered before rotation"),
		PaintPosition.Equals(FVector2D(-32.0f, 40.0f), KINDA_SMALL_NUMBER));

	const FVector2D RotatedBounds(PaintSize.Y, PaintSize.X);
	TestTrue(
		TEXT("After rotation the icon fits the inset occupied bounds exactly"),
		RotatedBounds.Equals(FVector2D(134.0f, 206.0f), KINDA_SMALL_NUMBER));

	const FVector2D OccupiedVisualSize(142.0f, 214.0f);
	const FVector2D RotatedRenderScale = URpgInventoryDragVisualWidget::CalculateIconRenderScale(
		OccupiedVisualSize,
		true);
	const FVector2D ScaledBeforeRotation(
		OccupiedVisualSize.X * RotatedRenderScale.X,
		OccupiedVisualSize.Y * RotatedRenderScale.Y);
	const FVector2D BoundsAfterRotation(ScaledBeforeRotation.Y, ScaledBeforeRotation.X);
	TestTrue(
		TEXT("A fill-aligned Blueprint image receives the inverse aspect correction before rotation"),
		BoundsAfterRotation.Equals(OccupiedVisualSize, KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Unrotated Blueprint images reset their render scale"),
		URpgInventoryDragVisualWidget::CalculateIconRenderScale(OccupiedVisualSize, false)
			.Equals(FVector2D(1.0f, 1.0f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryQuickAccessFeedbackCorrelationTest,
	"SurvivalRpg.Inventory.Interaction.QuickAccessFeedback.RequiresExactRequestId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryQuickAccessFeedbackCorrelationTest::RunTest(const FString& Parameters)
{
	const FGuid PendingRequestId = FGuid::NewGuid();
	const FRpgInventoryItemId PendingItemId = FRpgInventoryItemId::NewId();

	FRpgInventoryActionFeedbackMessage Feedback;
	Feedback.RequestId = PendingRequestId;
	Feedback.ItemId = PendingItemId;
	Feedback.ActionTag = RpgGameplayTags::Rpg_Inventory_Action_Transfer;
	Feedback.Result = ERpgInventoryActionFeedbackResult::Success;

	TestTrue(
		TEXT("Exact request-correlated Quick Access feedback acknowledges the pending command"),
		URpgInventoryInteractionSession::DoesFeedbackMatchPendingRequest(
			PendingRequestId,
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			PendingItemId,
			Feedback,
			true));

	FRpgInventoryActionFeedbackMessage UnrelatedRequest = Feedback;
	UnrelatedRequest.RequestId = FGuid::NewGuid();
	TestFalse(
		TEXT("An unrelated request cannot acknowledge a pending Quick Access command"),
		URpgInventoryInteractionSession::DoesFeedbackMatchPendingRequest(
			PendingRequestId,
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			PendingItemId,
			UnrelatedRequest,
			true));

	FRpgInventoryActionFeedbackMessage LegacyUncorrelated = Feedback;
	LegacyUncorrelated.RequestId.Invalidate();
	TestFalse(
		TEXT("Uncorrelated legacy feedback cannot acknowledge a pending Quick Access command"),
		URpgInventoryInteractionSession::DoesFeedbackMatchPendingRequest(
			PendingRequestId,
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			PendingItemId,
			LegacyUncorrelated,
			true));

	FRpgInventoryActionFeedbackMessage WrongSemanticAction = Feedback;
	WrongSemanticAction.ActionTag = RpgGameplayTags::Rpg_Inventory_Action_Equip;
	TestFalse(
		TEXT("A different semantic action cannot acknowledge the request even with the same id"),
		URpgInventoryInteractionSession::DoesFeedbackMatchPendingRequest(
			PendingRequestId,
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			PendingItemId,
			WrongSemanticAction,
			true));

	FRpgInventoryActionFeedbackMessage WrongItem = Feedback;
	WrongItem.ItemId = FRpgInventoryItemId::NewId();
	TestFalse(
		TEXT("Feedback for another concrete item cannot acknowledge the Quick Access request"),
		URpgInventoryInteractionSession::DoesFeedbackMatchPendingRequest(
			PendingRequestId,
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			PendingItemId,
			WrongItem,
			true));

	FRpgQuickAccessMutationRequest Request;
	Request.RequestId = PendingRequestId;
	Request.EnsureRequestId();
	TestEqual(TEXT("A valid caller-generated request id is preserved unchanged"), Request.RequestId, PendingRequestId);

	Request.RequestId.Invalidate();
	Request.EnsureRequestId();
	TestTrue(TEXT("Compatibility commands still receive a valid feedback correlation id"), Request.RequestId.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryQuickAccessReplicatedConfirmationTest,
	"SurvivalRpg.Inventory.Interaction.QuickAccessFeedback.ReplicatedBindingConfirmation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryQuickAccessReplicatedConfirmationTest::RunTest(const FString& Parameters)
{
	const FName CarryRole(TEXT("WeaponSlot1"));
	const FRpgInventoryItemId PendingItemId = FRpgInventoryItemId::NewId();

	FRpgInventoryDragPayload PendingCarryPayload;
	PendingCarryPayload.SourceSlotAddress.ContainerId = CarryRole;
	PendingCarryPayload.SourceSlotAddress.X = 0;
	PendingCarryPayload.SourceSlotAddress.Y = 0;

	FRpgActionBarSlot AppliedCarrySlot;
	AppliedCarrySlot.SlotType = ERpgActionBarSlotType::CarrySlot;
	AppliedCarrySlot.CarryRole = CarryRole;
	TestTrue(
		TEXT("Replicated Carry binding confirms and releases the matching pending drag"),
		URpgInventoryInteractionSession::DoesActionBarSlotConfirmPendingPayload(
			AppliedCarrySlot,
			PendingCarryPayload,
			PendingItemId));

	AppliedCarrySlot.CarryRole = FName(TEXT("WeaponSlot2"));
	TestFalse(
		TEXT("An unrelated replicated Carry role cannot acknowledge the pending drag"),
		URpgInventoryInteractionSession::DoesActionBarSlotConfirmPendingPayload(
			AppliedCarrySlot,
			PendingCarryPayload,
			PendingItemId));

	URpgInventoryItemInstance* PendingConsumable = NewObject<URpgInventoryItemInstance>(GetTransientPackage());
	FRpgInventoryDragPayload PendingConsumablePayload;
	PendingConsumablePayload.ItemInstance = PendingConsumable;
	FRpgActionBarSlot AppliedConsumableSlot;
	AppliedConsumableSlot.SlotType = ERpgActionBarSlotType::Consumable;
	AppliedConsumableSlot.ConsumableDefinition = PendingConsumable->GetItemDef();
	AppliedConsumableSlot.PreferredItemId = PendingItemId;
	TestTrue(
		TEXT("Replicated Consumable definition and preferred item id confirm the pending drag"),
		URpgInventoryInteractionSession::DoesActionBarSlotConfirmPendingPayload(
			AppliedConsumableSlot,
			PendingConsumablePayload,
			PendingItemId));

	AppliedConsumableSlot.PreferredItemId = FRpgInventoryItemId::NewId();
	TestFalse(
		TEXT("A replicated Consumable binding for another item cannot acknowledge the pending drag"),
		URpgInventoryInteractionSession::DoesActionBarSlotConfirmPendingPayload(
			AppliedConsumableSlot,
			PendingConsumablePayload,
			PendingItemId));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryDropConfirmationIntentTest,
	"SurvivalRpg.Inventory.Interaction.DropConfirmation.IntentCorrelationAndConsumeOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryDropConfirmationIntentTest::RunTest(const FString& Parameters)
{
	UButton* SourceWidget = NewObject<UButton>(GetTransientPackage());
	UButton* UnrelatedSourceWidget = NewObject<UButton>(GetTransientPackage());
	URpgInventoryManagerComponent* SourceInventory =
		NewObject<URpgInventoryManagerComponent>(GetTransientPackage());
	URpgInventoryManagerComponent* UnrelatedInventory =
		NewObject<URpgInventoryManagerComponent>(GetTransientPackage());
	if (!TestNotNull(TEXT("A transient drop source widget exists"), SourceWidget) ||
		!TestNotNull(TEXT("An unrelated transient source widget exists"), UnrelatedSourceWidget) ||
		!TestNotNull(TEXT("A transient source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("An unrelated transient inventory exists"), UnrelatedInventory))
	{
		return false;
	}

	FRpgInventoryManualDropRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.EntryId = FGuid::NewGuid();
	Request.ItemId = FRpgInventoryItemId::NewId();
	Request.ExpectedSourcePlacement.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeRoot(TEXT("Pockets")));
	Request.ExpectedSourcePlacement.X = 2;
	Request.ExpectedSourcePlacement.Y = 1;
	Request.ExpectedSourcePlacement.Width = 1;
	Request.ExpectedSourcePlacement.Height = 1;
	Request.ExpectedSourceQuantity = 5;
	Request.StackCount = 3;

	FRpgInventoryDropConfirmationIntent Intent;
	if (!TestTrue(
			TEXT("A complete unconfirmed drop snapshot arms the confirmation intent"),
			Intent.Arm(SourceWidget, SourceInventory, Request)))
	{
		return false;
	}
	TestTrue(TEXT("The confirmation intent reports itself armed"), Intent.IsArmed());

	FRpgInventoryActionFeedbackMessage Feedback;
	Feedback.RequestId = Request.RequestId;
	Feedback.ItemId = Request.ItemId;
	Feedback.ActionTag = RpgGameplayTags::Rpg_Inventory_Action_Drop;
	Feedback.Result = ERpgInventoryActionFeedbackResult::RequiresConfirmation;
	Feedback.InventoryOwner = SourceInventory;
	Feedback.StackCount = Request.StackCount;
	TestTrue(
		TEXT("Only the exact authoritative drop feedback matches the armed intent"),
		Intent.DoesFeedbackMatch(nullptr, Feedback));

	FRpgInventoryActionFeedbackMessage WrongRequest = Feedback;
	WrongRequest.RequestId = FGuid::NewGuid();
	TestFalse(
		TEXT("Feedback for another request id cannot open this confirmation"),
		Intent.DoesFeedbackMatch(nullptr, WrongRequest));

	FRpgInventoryActionFeedbackMessage WrongItem = Feedback;
	WrongItem.ItemId = FRpgInventoryItemId::NewId();
	TestFalse(
		TEXT("Feedback for another persistent item cannot open this confirmation"),
		Intent.DoesFeedbackMatch(nullptr, WrongItem));

	FRpgInventoryActionFeedbackMessage WrongAction = Feedback;
	WrongAction.ActionTag = RpgGameplayTags::Rpg_Inventory_Action_Transfer;
	TestFalse(
		TEXT("Feedback for another semantic action cannot open this confirmation"),
		Intent.DoesFeedbackMatch(nullptr, WrongAction));

	FRpgInventoryActionFeedbackMessage WrongInventory = Feedback;
	WrongInventory.InventoryOwner = UnrelatedInventory;
	TestFalse(
		TEXT("Feedback for another source inventory cannot open this confirmation"),
		Intent.DoesFeedbackMatch(nullptr, WrongInventory));

	FRpgInventoryActionFeedbackMessage WrongCount = Feedback;
	WrongCount.StackCount = Request.StackCount - 1;
	TestFalse(
		TEXT("Feedback for another exact quantity cannot open this confirmation"),
		Intent.DoesFeedbackMatch(nullptr, WrongCount));

	TestFalse(
		TEXT("Releasing an unrelated source does not reset the pending confirmation"),
		Intent.ResetForSource(UnrelatedSourceWidget));
	TestTrue(
		TEXT("The pending confirmation survives an unrelated source reset"),
		Intent.IsArmed());

	const FGuid InitialRequestId = Request.RequestId;
	URpgInventoryManagerComponent* ConfirmedSourceInventory = nullptr;
	FRpgInventoryManualDropRequest ConfirmedRequest;
	TestTrue(
		TEXT("The exact initial request can be consumed once"),
		Intent.ConsumeConfirmedRetry(
			InitialRequestId,
			ConfirmedSourceInventory,
			ConfirmedRequest));
	TestEqual(
		TEXT("The confirmed retry retains the exact source inventory"),
		ConfirmedSourceInventory,
		SourceInventory);
	TestEqual(
		TEXT("The confirmed retry retains the stable entry id"),
		ConfirmedRequest.EntryId,
		Request.EntryId);
	TestTrue(
		TEXT("The confirmed retry retains the persistent item id"),
		ConfirmedRequest.ItemId == Request.ItemId);
	TestTrue(
		TEXT("The confirmed retry retains the complete source placement"),
		ConfirmedRequest.ExpectedSourcePlacement ==
			Request.ExpectedSourcePlacement);
	TestEqual(
		TEXT("The confirmed retry retains the complete source quantity"),
		ConfirmedRequest.ExpectedSourceQuantity,
		Request.ExpectedSourceQuantity);
	TestEqual(
		TEXT("The confirmed retry retains the exact quantity"),
		ConfirmedRequest.StackCount,
		Request.StackCount);
	TestTrue(
		TEXT("The retry is explicitly confirmed"),
		ConfirmedRequest.bConfirmed);
	TestTrue(
		TEXT("The retry receives a fresh valid request id"),
		ConfirmedRequest.RequestId.IsValid() &&
			ConfirmedRequest.RequestId != InitialRequestId);
	TestFalse(
		TEXT("Consuming the request clears the pending intent before dispatch"),
		Intent.IsArmed());

	ConfirmedSourceInventory = SourceInventory;
	ConfirmedRequest = Request;
	TestFalse(
		TEXT("A second consume of the same initial request is rejected"),
		Intent.ConsumeConfirmedRetry(
			InitialRequestId,
			ConfirmedSourceInventory,
			ConfirmedRequest));
	TestNull(
		TEXT("A rejected second consume clears its output inventory"),
		ConfirmedSourceInventory);
	TestFalse(
		TEXT("A rejected second consume does not leak a confirmed request"),
		ConfirmedRequest.bConfirmed);
	TestFalse(
		TEXT("A rejected second consume returns no request id"),
		ConfirmedRequest.RequestId.IsValid());

	TestTrue(
		TEXT("The same complete snapshot can arm a later independent confirmation"),
		Intent.Arm(SourceWidget, SourceInventory, Request));
	TestTrue(
		TEXT("Releasing the exact source resets its pending confirmation"),
		Intent.ResetForSource(SourceWidget));
	TestFalse(
		TEXT("The exact-source reset leaves no pending confirmation"),
		Intent.IsArmed());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
