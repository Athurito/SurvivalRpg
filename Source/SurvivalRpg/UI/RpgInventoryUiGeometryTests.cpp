#include "SurvivalRpg/UI/RpgInventoryUiGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"
#include "Rendering/SlateLayoutTransform.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryUiGeometryAndClampingTest,
	"SurvivalRpg.Inventory.UI.ContextAnchor.GeometryAndClamping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryUiGeometryAndClampingTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryUiGeometry;

	const FGeometry TransformedGeometry = FGeometry::MakeRoot(
		FVector2D(120.0, 80.0),
		FSlateLayoutTransform(
			2.0f,
			FVector2D(100.0, 50.0)));

	FVector2D ResolvedPosition = FVector2D::ZeroVector;
	TestTrue(
		TEXT("A valid transformed geometry resolves its absolute center"),
		TryResolveAbsoluteCenter(
			TransformedGeometry,
			ResolvedPosition));
	TestTrue(
		TEXT("Center resolution applies both layout scale and translation"),
		ResolvedPosition.Equals(
			FVector2D(220.0, 130.0),
			KINDA_SMALL_NUMBER));

	TestTrue(
		TEXT("A valid in-bounds local point resolves to absolute Slate space"),
		TryResolveAbsolutePoint(
			TransformedGeometry,
			FVector2D(25.0, 10.0),
			ResolvedPosition));
	TestTrue(
		TEXT("Explicit point resolution applies both layout scale and translation"),
		ResolvedPosition.Equals(
			FVector2D(150.0, 70.0),
			KINDA_SMALL_NUMBER));

	const FGeometry DefaultGeometry;
	TestFalse(
		TEXT("A default geometry fails center resolution closed"),
		TryResolveAbsoluteCenter(
			DefaultGeometry,
			ResolvedPosition));
	TestFalse(
		TEXT("A default geometry fails explicit point resolution closed"),
		TryResolveAbsolutePoint(
			DefaultGeometry,
			FVector2D::ZeroVector,
			ResolvedPosition));

	const FGeometry ZeroWidthGeometry = FGeometry::MakeRoot(
		FVector2D(0.0, 80.0),
		FSlateLayoutTransform());
	const FGeometry ZeroHeightGeometry = FGeometry::MakeRoot(
		FVector2D(120.0, 0.0),
		FSlateLayoutTransform());
	TestFalse(
		TEXT("A zero-width geometry fails center resolution closed"),
		TryResolveAbsoluteCenter(
			ZeroWidthGeometry,
			ResolvedPosition));
	TestFalse(
		TEXT("A zero-height geometry fails center resolution closed"),
		TryResolveAbsoluteCenter(
			ZeroHeightGeometry,
			ResolvedPosition));

	const double QuietNaN =
		std::numeric_limits<double>::quiet_NaN();
	const float QuietFloatNaN =
		std::numeric_limits<float>::quiet_NaN();
	TestFalse(
		TEXT("A NaN local point fails absolute resolution closed"),
		TryResolveAbsolutePoint(
			TransformedGeometry,
			FVector2D(QuietNaN, 10.0),
			ResolvedPosition));

	const FGeometry NaNTransformGeometry = FGeometry::MakeRoot(
		FVector2D(120.0, 80.0),
		FSlateLayoutTransform(
			1.0f,
			FVector2D(QuietFloatNaN, 0.0f)));
	TestFalse(
		TEXT("A geometry producing a non-finite absolute point fails closed"),
		TryResolveAbsoluteCenter(
			NaNTransformGeometry,
			ResolvedPosition));

	TestFalse(
		TEXT("A local point left of the geometry fails closed"),
		TryResolveAbsolutePoint(
			TransformedGeometry,
			FVector2D(-1.0, 10.0),
			ResolvedPosition));
	TestFalse(
		TEXT("A local point right of the geometry fails closed"),
		TryResolveAbsolutePoint(
			TransformedGeometry,
			FVector2D(121.0, 10.0),
			ResolvedPosition));
	TestFalse(
		TEXT("A local point below the geometry fails closed"),
		TryResolveAbsolutePoint(
			TransformedGeometry,
			FVector2D(10.0, 81.0),
			ResolvedPosition));

	const FGeometry CanvasGeometry = FGeometry::MakeRoot(
		FVector2D(400.0, 300.0),
		FSlateLayoutTransform(
			1.5f,
			FVector2D(200.0, 100.0)));
	const FVector2D MenuSize(100.0, 80.0);

	const FVector2D InteriorAbsoluteAnchor =
		CanvasGeometry.LocalToAbsolute(
			FVector2D(150.0, 120.0));
	TestTrue(
		TEXT("A valid absolute anchor resolves into canvas-local space"),
		TryResolveClampedMenuCanvasPosition(
			CanvasGeometry,
			InteriorAbsoluteAnchor,
			MenuSize,
			ResolvedPosition));
	TestTrue(
		TEXT("Canvas conversion reverses layout scale and translation"),
		ResolvedPosition.Equals(
			FVector2D(150.0, 120.0),
			KINDA_SMALL_NUMBER));

	const FVector2D BeforeTopLeftAbsoluteAnchor =
		CanvasGeometry.LocalToAbsolute(
			FVector2D(-25.0, -30.0));
	TestTrue(
		TEXT("An anchor before the canvas is accepted for clamping"),
		TryResolveClampedMenuCanvasPosition(
			CanvasGeometry,
			BeforeTopLeftAbsoluteAnchor,
			MenuSize,
			ResolvedPosition));
	TestTrue(
		TEXT("Menu position clamps against the left and top canvas edges"),
		ResolvedPosition.Equals(
			FVector2D::ZeroVector,
			KINDA_SMALL_NUMBER));

	const FVector2D NearBottomRightAbsoluteAnchor =
		CanvasGeometry.LocalToAbsolute(
			FVector2D(390.0, 295.0));
	TestTrue(
		TEXT("An anchor near the bottom-right canvas edge resolves"),
		TryResolveClampedMenuCanvasPosition(
			CanvasGeometry,
			NearBottomRightAbsoluteAnchor,
			MenuSize,
			ResolvedPosition));
	TestTrue(
		TEXT("Menu position clamps against the right and bottom canvas edges"),
		ResolvedPosition.Equals(
			FVector2D(300.0, 220.0),
			KINDA_SMALL_NUMBER));

	TestFalse(
		TEXT("A default canvas geometry fails menu positioning closed"),
		TryResolveClampedMenuCanvasPosition(
			DefaultGeometry,
			InteriorAbsoluteAnchor,
			MenuSize,
			ResolvedPosition));
	TestFalse(
		TEXT("A zero-width canvas geometry fails menu positioning closed"),
		TryResolveClampedMenuCanvasPosition(
			ZeroWidthGeometry,
			InteriorAbsoluteAnchor,
			MenuSize,
			ResolvedPosition));
	TestFalse(
		TEXT("A NaN absolute anchor fails menu positioning closed"),
		TryResolveClampedMenuCanvasPosition(
			CanvasGeometry,
			FVector2D(QuietNaN, 200.0),
			MenuSize,
			ResolvedPosition));
	TestFalse(
		TEXT("A NaN menu size fails menu positioning closed"),
		TryResolveClampedMenuCanvasPosition(
			CanvasGeometry,
			InteriorAbsoluteAnchor,
			FVector2D(QuietNaN, 80.0),
			ResolvedPosition));
	TestFalse(
		TEXT("A negative-height menu fails menu positioning closed"),
		TryResolveClampedMenuCanvasPosition(
			CanvasGeometry,
			InteriorAbsoluteAnchor,
			FVector2D(100.0, -1.0),
			ResolvedPosition));

	return true;
}

#endif
