#include "RpgSkillTreeMapWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonLocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Rendering/SlateLayoutTransform.h"
#include "SRpgSkillTreeMapPanel.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeAutomationTestTypes.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"
#include "Widgets/Layout/SBox.h"

#include <limits>

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_Tree, "SkillTree.Tree.AutomationMap");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_GridTree, "SkillTree.Tree.AutomationMapGrid");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_Root, "SkillTree.Node.AutomationMap.Root");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_Second, "SkillTree.Node.AutomationMap.Second");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_FormA, "SkillTree.Node.AutomationMap.FormA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_FormB, "SkillTree.Node.AutomationMap.FormB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_GridA, "SkillTree.Node.AutomationMapGrid.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_GridB, "SkillTree.Node.AutomationMapGrid.B");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeMapTest_GridC, "SkillTree.Node.AutomationMapGrid.C");

namespace RpgSkillTreeMapTests
{
	class FScopedWidgetWorld
	{
	public:
		FScopedWidgetWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedWidgetWorld()
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

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	FPointerEvent MakeMouseEvent(
		const FVector2D& ScreenPosition,
		const FKey& EffectingButton,
		const TSet<FKey>& PressedButtons,
		const float WheelDelta = 0.0f)
	{
		return FPointerEvent(0, 0, ScreenPosition, ScreenPosition, PressedButtons, EffectingButton, WheelDelta, FModifierKeysState());
	}

	FReply Press(SRpgSkillTreeMapPanel& Panel, const FGeometry& Geometry, const FVector2D& Screen, const FKey& Button = EKeys::LeftMouseButton)
	{
		return Panel.OnPreviewMouseButtonDown(Geometry, MakeMouseEvent(Screen, Button, {Button}));
	}

	FReply Move(SRpgSkillTreeMapPanel& Panel, const FGeometry& Geometry, const FVector2D& Screen, const FKey& HeldButton = EKeys::LeftMouseButton)
	{
		TSet<FKey> Held;
		if (HeldButton.IsValid())
		{
			Held.Add(HeldButton);
		}
		return Panel.OnMouseMove(Geometry, MakeMouseEvent(Screen, EKeys::Invalid, Held));
	}

	FReply Release(SRpgSkillTreeMapPanel& Panel, const FGeometry& Geometry, const FVector2D& Screen, const FKey& Button = EKeys::LeftMouseButton)
	{
		return Panel.OnMouseButtonUp(Geometry, MakeMouseEvent(Screen, Button, {}));
	}

	FNavigationReply Navigate(SRpgSkillTreeMapPanel& Panel, const FGeometry& Geometry, const EUINavigation Direction)
	{
		return Panel.OnNavigation(Geometry, FNavigationEvent(FModifierKeysState(), 0, Direction, ENavigationGenesis::Keyboard));
	}

	/**
	 * FreePosition tree: Root and Second in region Core, the exclusive FormA and FormB below Root in region Forms,
	 * which the map frames first.
	 */
	URpgSkillTreeDefinition* MakeMapTree()
	{
		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_SkillTreeMapTest_Tree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
		Tree->MaxPoints = 10;
		Tree->LayoutMode = ERpgSkillTreeLayoutMode::FreePosition;
		Tree->DefaultRegionId = TEXT("Forms");
		for (const TCHAR* RegionId : {TEXT("Core"), TEXT("Forms")})
		{
			FRpgSkillTreeMapRegion& Region = Tree->Regions.AddDefaulted_GetRef();
			Region.RegionId = RegionId;
			Region.Padding = FVector2D(60.0, 60.0);
		}

		auto AddNode = [Tree](const FGameplayTag NodeTag, const FVector2D& Position, const TCHAR* RegionId) -> FRpgSkillTreeNode&
		{
			FRpgSkillTreeNode& Node = Tree->Nodes.AddDefaulted_GetRef();
			Node.NodeTag = NodeTag;
			Node.MapPosition = Position;
			Node.RegionId = RegionId;
			return Node;
		};
		AddNode(TAG_SkillTreeMapTest_Root, FVector2D(0.0, 0.0), TEXT("Core"));
		AddNode(TAG_SkillTreeMapTest_Second, FVector2D(300.0, 0.0), TEXT("Core"));
		FRpgSkillTreeNode& FormA = AddNode(TAG_SkillTreeMapTest_FormA, FVector2D(-150.0, 300.0), TEXT("Forms"));
		FormA.Prerequisites.Add(TAG_SkillTreeMapTest_Root);
		FormA.ExclusiveGroup = TEXT("Form");
		FRpgSkillTreeNode& FormB = AddNode(TAG_SkillTreeMapTest_FormB, FVector2D(150.0, 300.0), TEXT("Forms"));
		FormB.Prerequisites.Add(TAG_SkillTreeMapTest_Root);
		FormB.ExclusiveGroup = TEXT("Form");
		return Tree;
	}

	/** Legacy Grid tree: A and B in row 0, C below between them. */
	URpgSkillTreeDefinition* MakeGridTree()
	{
		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_SkillTreeMapTest_GridTree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Tree->MaxPoints = 10;

		auto AddNode = [Tree](const FGameplayTag NodeTag, const int32 Row, const int32 Column) -> FRpgSkillTreeNode&
		{
			FRpgSkillTreeNode& Node = Tree->Nodes.AddDefaulted_GetRef();
			Node.NodeTag = NodeTag;
			Node.Row = Row;
			Node.Column = Column;
			return Node;
		};
		AddNode(TAG_SkillTreeMapTest_GridA, 0, 0);
		AddNode(TAG_SkillTreeMapTest_GridB, 0, 2);
		AddNode(TAG_SkillTreeMapTest_GridC, 1, 1).Prerequisites.Add(TAG_SkillTreeMapTest_GridA);
		return Tree;
	}

	/** True when MapPoint appears inside the viewport shrunk by Inset on every side. */
	bool IsInsideViewport(const FRpgSkillTreeMapCamera& Camera, const FVector2D& MapPoint, const FVector2D& ViewportSize, const double Inset)
	{
		const FVector2D Local = Camera.MapToLocal(MapPoint, ViewportSize);
		constexpr double Tolerance = 0.01;
		return Local.X >= Inset - Tolerance && Local.Y >= Inset - Tolerance &&
			Local.X <= ViewportSize.X - Inset + Tolerance && Local.Y <= ViewportSize.Y - Inset + Tolerance;
	}

	bool AreCamerasNearlyEqual(const FRpgSkillTreeMapCamera& A, const FRpgSkillTreeMapCamera& B)
	{
		return A.Center.Equals(B.Center, 0.01) && FMath::IsNearlyEqual(A.Zoom, B.Zoom, 0.0001);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeMapCameraTest,
	"SurvivalRpg.UI.SkillTreeMap.Camera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeMapCameraTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeMapTests;

	const FVector2D Viewport(800.0, 600.0);
	FRpgSkillTreeMapCamera Camera;
	Camera.Center = FVector2D(100.0, -50.0);
	Camera.Zoom = 0.8;

	for (const FVector2D& MapPoint : {FVector2D(0.0, 0.0), FVector2D(-420.0, 130.0), FVector2D(1234.5, -987.25)})
	{
		TestTrue(TEXT("LocalToMap inverts MapToLocal"), Camera.LocalToMap(Camera.MapToLocal(MapPoint, Viewport), Viewport).Equals(MapPoint, 0.001));
	}
	TestTrue(TEXT("The camera center appears at the viewport center"), Camera.MapToLocal(Camera.Center, Viewport).Equals(Viewport * 0.5, 0.001));

	// Zooming keeps the map point under the cursor in place, also at the zoom limits.
	const FVector2D Anchor(620.0, 140.0);
	const FVector2D AnchoredMapPoint = Camera.LocalToMap(Anchor, Viewport);
	Camera.ZoomAt(Anchor, 1.6, 0.25, 2.0, Viewport);
	TestEqual(TEXT("The zoom changes to the requested value"), Camera.Zoom, 1.6);
	TestTrue(TEXT("The map point under the cursor stays under it"), Camera.LocalToMap(Anchor, Viewport).Equals(AnchoredMapPoint, 0.001));
	Camera.ZoomAt(Anchor, 10.0, 0.25, 2.0, Viewport);
	TestEqual(TEXT("Zoom stops at the maximum"), Camera.Zoom, 2.0);
	TestTrue(TEXT("The anchor holds at the maximum"), Camera.LocalToMap(Anchor, Viewport).Equals(AnchoredMapPoint, 0.001));
	Camera.ZoomAt(Anchor, 0.001, 0.25, 2.0, Viewport);
	TestEqual(TEXT("Zoom stops at the minimum"), Camera.Zoom, 0.25);
	Camera.ZoomAt(Anchor, std::numeric_limits<double>::quiet_NaN(), 0.25, 2.0, Viewport);
	TestEqual(TEXT("A non-finite zoom changes nothing"), Camera.Zoom, 0.25);
	TestTrue(TEXT("The anchor holds after every zoom"), Camera.LocalToMap(Anchor, Viewport).Equals(AnchoredMapPoint, 0.001));

	// Panning moves the map with the pointer.
	const FVector2D Probe(40.0, 25.0);
	const FVector2D ProbeLocal = Camera.MapToLocal(Probe, Viewport);
	Camera.PanByLocal(FVector2D(30.0, -20.0));
	TestTrue(TEXT("A pan moves map points by the pointer delta"), Camera.MapToLocal(Probe, Viewport).Equals(ProbeLocal + FVector2D(30.0, -20.0), 0.001));

	// Fit frames the bounds inside the padding and centers them.
	const FBox2D Bounds(FVector2D(-200.0, -100.0), FVector2D(600.0, 300.0));
	const FRpgSkillTreeMapCamera Fit = FRpgSkillTreeMapCamera::MakeFit(Bounds, Viewport, 50.0, 0.1, 2.0);
	TestTrue(TEXT("Fit centers the bounds"), Fit.Center.Equals(FVector2D(200.0, 100.0), 0.001));
	TestEqual(TEXT("Fit uses the tighter axis"), Fit.Zoom, 700.0 / 800.0, 0.0001);
	TestTrue(TEXT("Fit shows the bounds' minimum inside the padding"), IsInsideViewport(Fit, Bounds.Min, Viewport, 50.0));
	TestTrue(TEXT("Fit shows the bounds' maximum inside the padding"), IsInsideViewport(Fit, Bounds.Max, Viewport, 50.0));
	TestEqual(TEXT("Fitting one point uses the maximum zoom"),
		FRpgSkillTreeMapCamera::MakeFit(FBox2D(FVector2D(5.0, 5.0), FVector2D(5.0, 5.0)), Viewport, 50.0, 0.1, 2.0).Zoom, 2.0);
	TestEqual(TEXT("Fitting a huge map stops at the minimum zoom"),
		FRpgSkillTreeMapCamera::MakeFit(Bounds.ExpandBy(100000.0), Viewport, 50.0, 0.1, 2.0).Zoom, 0.1);
	const FRpgSkillTreeMapCamera EmptyFit = FRpgSkillTreeMapCamera::MakeFit(FBox2D(ForceInit), Viewport, 50.0, 0.1, 2.0);
	TestTrue(TEXT("Fitting nothing keeps a neutral camera"), EmptyFit.Center.IsZero() && EmptyFit.Zoom == 1.0);

	// Bring into view pans as little as possible and centers boxes that cannot fit.
	FRpgSkillTreeMapCamera Focus;
	TestTrue(TEXT("An off-screen point is brought into view"), Focus.BringIntoView(FVector2D(1000.0, 0.0), FVector2D(100.0, 100.0), 48.0, Viewport));
	TestTrue(TEXT("The point stops at the margin"), Focus.MapToLocal(FVector2D(1000.0, 0.0), Viewport).Equals(FVector2D(800.0 - 48.0 - 50.0, 300.0), 0.001));
	TestFalse(TEXT("A visible point needs no pan"), Focus.BringIntoView(FVector2D(1000.0, 0.0), FVector2D(100.0, 100.0), 48.0, Viewport));
	Focus.BringIntoView(FVector2D(0.0, 0.0), FVector2D(2000.0, 50.0), 48.0, Viewport);
	TestEqual(TEXT("A box wider than the viewport is centered"), Focus.MapToLocal(FVector2D(0.0, 0.0), Viewport).X, 400.0, 0.001);

	FRpgSkillTreeMapCamera Clamped;
	Clamped.Center = FVector2D(5000.0, -5000.0);
	Clamped.ClampCenter(Bounds);
	TestTrue(TEXT("The center stays on the map"), Clamped.Center.Equals(FVector2D(600.0, -100.0), 0.001));
	Clamped.ClampCenter(FBox2D(ForceInit));
	TestTrue(TEXT("An empty map does not move the center"), Clamped.Center.Equals(FVector2D(600.0, -100.0), 0.001));

	// Resizing keeps the map point at the view center; DPI scaling is part of the geometry, not the camera.
	const FVector2D Wide(1720.0, 720.0);
	TestTrue(TEXT("A resize keeps the centered map point"),
		Camera.LocalToMap(Viewport * 0.5, Viewport).Equals(Camera.LocalToMap(Wide * 0.5, Wide), 0.001));
	const FGeometry ScaledGeometry = FGeometry::MakeRoot(Viewport, FSlateLayoutTransform(1.5f, FVector2D(100.0, 50.0)));
	const FVector2D ScreenPoint(733.0, 412.0);
	const FVector2D ScaledLocalSize = ScaledGeometry.GetLocalSize();
	const FVector2D ScaledLocalPoint = ScaledGeometry.AbsoluteToLocal(ScreenPoint);
	const FVector2D MapPoint = Camera.LocalToMap(ScaledLocalPoint, ScaledLocalSize);
	const FVector2D BackOnScreen = ScaledGeometry.LocalToAbsolute(Camera.MapToLocal(MapPoint, ScaledLocalSize));
	TestTrue(TEXT("Screen, local and map space round-trip under DPI scale"), BackOnScreen.Equals(ScreenPoint, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeMapNavigationTest,
	"SurvivalRpg.UI.SkillTreeMap.Navigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeMapNavigationTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeMapNavigation;

	// 0 root, 1 and 2 below it to the sides, 3 further down, 4 straight right of the root.
	const TArray<FVector2D> Positions = {
		FVector2D(0.0, 0.0),
		FVector2D(-200.0, 200.0),
		FVector2D(200.0, 200.0),
		FVector2D(0.0, 400.0),
		FVector2D(400.0, 0.0)};

	TestEqual(TEXT("Right prefers the node straight ahead over a closer diagonal one"), FindNode(Positions, 0, EUINavigation::Right), 4);
	TestEqual(TEXT("Down prefers the node on the axis"), FindNode(Positions, 0, EUINavigation::Down), 3);
	TestEqual(TEXT("Left finds the node on the same row"), FindNode(Positions, 2, EUINavigation::Left), 1);
	TestEqual(TEXT("Up returns to the root"), FindNode(Positions, 3, EUINavigation::Up), 0);
	TestEqual(TEXT("Down from a side node reaches the bottom node"), FindNode(Positions, 1, EUINavigation::Down), 3);
	TestEqual(TEXT("No node to the left leaves the map"), FindNode(Positions, 1, EUINavigation::Left), INDEX_NONE);
	TestEqual(TEXT("Next follows the authored order"), FindNode(Positions, 0, EUINavigation::Next), 1);
	TestEqual(TEXT("Previous follows the authored order"), FindNode(Positions, 3, EUINavigation::Previous), 2);
	TestEqual(TEXT("Next after the last node leaves the map"), FindNode(Positions, 4, EUINavigation::Next), INDEX_NONE);
	TestEqual(TEXT("Previous before the first node leaves the map"), FindNode(Positions, 0, EUINavigation::Previous), INDEX_NONE);
	TestEqual(TEXT("An invalid origin finds nothing"), FindNode(Positions, 7, EUINavigation::Right), INDEX_NONE);

	TSet<int32> Reached = {0};
	for (int32 Index = 0; Index != INDEX_NONE; Index = FindNode(Positions, Index, EUINavigation::Next))
	{
		Reached.Add(Index);
	}
	TestEqual(TEXT("Next reaches every node"), Reached.Num(), Positions.Num());

	TestEqual(TEXT("The nearest node wins"), FindNearest(Positions, FVector2D(390.0, 10.0)), 4);
	TestEqual(TEXT("No nodes, no nearest"), FindNearest(TArray<FVector2D>(), FVector2D::ZeroVector), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeMapPanelInputTest,
	"SurvivalRpg.UI.SkillTreeMap.Panel.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeMapPanelInputTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeMapTests;

	TArray<int32> Clicked;
	TArray<int32> Navigated;
	int32 ViewChanges = 0;
	const TSharedRef<SRpgSkillTreeMapPanel> Panel = SNew(SRpgSkillTreeMapPanel)
		.OnNodeClicked_Lambda([&Clicked](const int32 Index) { Clicked.Add(Index); })
		.OnNodeNavigated_Lambda([&Navigated](const int32 Index) { Navigated.Add(Index); })
		.OnViewChanged_Lambda([&ViewChanges]() { ++ViewChanges; });

	const TArray<FVector2D> Positions = {FVector2D(0.0, 0.0), FVector2D(300.0, 0.0), FVector2D(0.0, 300.0)};
	TArray<TSharedRef<SWidget>> Entries;
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		Entries.Add(SNew(SBox).WidthOverride(100.0f).HeightOverride(60.0f));
	}
	Panel->SetNodes(Entries, Positions);
	Panel->SlatePrepass(1.0f);
	TestEqual(TEXT("The map clips its content"), Panel->GetClipping(), EWidgetClipping::ClipToBounds);

	const FVector2D Viewport(800.0, 600.0);
	const FGeometry Geometry = FGeometry::MakeRoot(Viewport, FSlateLayoutTransform(1.0f));
	Panel->Tick(Geometry, 0.0, 0.0f);
	FRpgSkillTreeMapCamera Start;
	Panel->SetCamera(Start);
	const FVector2D NodeScreen(400.0, 300.0);

	// Hit targets follow the drawn entry but never shrink below the minimum size.
	TestEqual(TEXT("The node's center hits it"), Panel->HitTestNode(NodeScreen), 0);
	TestEqual(TEXT("The entry's edge still hits it"), Panel->HitTestNode(NodeScreen + FVector2D(49.0, 0.0)), 0);
	TestEqual(TEXT("Outside the entry nothing is hit"), Panel->HitTestNode(NodeScreen + FVector2D(60.0, 0.0)), INDEX_NONE);
	FRpgSkillTreeMapCamera ZoomedOut;
	ZoomedOut.Zoom = 0.25;
	Panel->SetCamera(ZoomedOut);
	TestEqual(TEXT("Zoomed out, entries draw at the minimum scale"), Panel->GetEntryScale(), 0.5);
	TestEqual(TEXT("The minimum hit size covers more than the 30-unit entry"), Panel->HitTestNode(NodeScreen + FVector2D(0.0, 22.0)), 0);
	TestEqual(TEXT("Beyond the minimum hit size nothing is hit"), Panel->HitTestNode(NodeScreen + FVector2D(0.0, 26.0)), INDEX_NONE);
	Panel->SetCamera(Start);

	// A press and release on a node clicks it, also with jitter below the threshold.
	Press(*Panel, Geometry, NodeScreen);
	Release(*Panel, Geometry, NodeScreen + FVector2D(2.0, 1.0));
	Press(*Panel, Geometry, NodeScreen);
	Move(*Panel, Geometry, NodeScreen + FVector2D(3.0, 2.0));
	TestFalse(TEXT("Jitter below the threshold is no drag"), Panel->IsDragging());
	Release(*Panel, Geometry, NodeScreen + FVector2D(3.0, 2.0));
	TestTrue(TEXT("Both presses clicked node 0"), Clicked == TArray<int32>({0, 0}));
	TestTrue(TEXT("Clicks do not move the camera"), AreCamerasNearlyEqual(Panel->GetCamera(), Start));

	// A drag pans with the cursor and never clicks, even when it ends on the pressed node.
	Clicked.Reset();
	const int32 ViewChangesBeforeDrag = ViewChanges;
	Press(*Panel, Geometry, NodeScreen);
	Move(*Panel, Geometry, NodeScreen + FVector2D(20.0, 0.0));
	TestTrue(TEXT("Passing the threshold starts a drag"), Panel->IsDragging());
	TestTrue(TEXT("The node stays glued to the cursor"),
		Panel->GetCamera().MapToLocal(Positions[0], Viewport).Equals(NodeScreen + FVector2D(20.0, 0.0), 0.001));
	Move(*Panel, Geometry, NodeScreen);
	Release(*Panel, Geometry, NodeScreen);
	TestTrue(TEXT("A drag never clicks"), Clicked.IsEmpty());
	TestFalse(TEXT("Releasing ends the drag"), Panel->IsDragging() || Panel->IsPointerDown());
	TestTrue(TEXT("Panning reports view changes"), ViewChanges > ViewChangesBeforeDrag);

	// Other buttons pan but never click; a second button does not restart the gesture.
	Press(*Panel, Geometry, NodeScreen, EKeys::RightMouseButton);
	Release(*Panel, Geometry, NodeScreen, EKeys::RightMouseButton);
	TestTrue(TEXT("A right click selects nothing"), Clicked.IsEmpty());
	Press(*Panel, Geometry, NodeScreen);
	Press(*Panel, Geometry, NodeScreen, EKeys::RightMouseButton);
	Release(*Panel, Geometry, NodeScreen, EKeys::RightMouseButton);
	TestTrue(TEXT("The left press survives another button"), Panel->IsPointerDown());
	Release(*Panel, Geometry, NodeScreen);
	TestTrue(TEXT("The left release still clicks"), Clicked == TArray<int32>({0}));

	// Lost capture, a release elsewhere, an explicit cancel and a refresh all end the gesture without a click.
	Clicked.Reset();
	Press(*Panel, Geometry, NodeScreen);
	Move(*Panel, Geometry, NodeScreen + FVector2D(30.0, 0.0));
	Panel->OnMouseCaptureLost(FCaptureLostEvent(0, 0));
	TestFalse(TEXT("Lost capture cancels the drag"), Panel->IsDragging() || Panel->IsPointerDown());
	TestFalse(TEXT("A later release is not handled"), Release(*Panel, Geometry, NodeScreen).IsEventHandled());
	Panel->SetCamera(Start);
	Press(*Panel, Geometry, NodeScreen);
	Move(*Panel, Geometry, NodeScreen + FVector2D(30.0, 0.0), EKeys::Invalid);
	TestFalse(TEXT("A move without the held button ends the gesture"), Panel->IsPointerDown());
	Press(*Panel, Geometry, NodeScreen);
	Panel->CancelGesture();
	TestFalse(TEXT("Cancel ends a press"), Panel->IsPointerDown());
	Release(*Panel, Geometry, NodeScreen);
	Press(*Panel, Geometry, NodeScreen);
	Panel->SetNodes(Entries, Positions);
	Release(*Panel, Geometry, NodeScreen);
	TestTrue(TEXT("No gesture that was interrupted clicks"), Clicked.IsEmpty());

	// The wheel zooms around the cursor.
	Panel->SetCamera(Start);
	const FVector2D Cursor(600.0, 200.0);
	const FVector2D MapUnderCursor = Panel->GetCamera().LocalToMap(Cursor, Viewport);
	Panel->OnMouseWheel(Geometry, MakeMouseEvent(Cursor, EKeys::Invalid, {}, 1.0f));
	TestEqual(TEXT("One notch zooms by one step"), Panel->GetCamera().Zoom, Panel->GetSettings().ZoomStep, 0.0001);
	TestTrue(TEXT("The map stays anchored at the cursor"), Panel->GetCamera().LocalToMap(Cursor, Viewport).Equals(MapUnderCursor, 0.001));
	for (int32 Notch = 0; Notch < 40; ++Notch)
	{
		Panel->OnMouseWheel(Geometry, MakeMouseEvent(Cursor, EKeys::Invalid, {}, 1.0f));
	}
	TestEqual(TEXT("The wheel stops at the maximum zoom"), Panel->GetCamera().Zoom, Panel->GetSettings().MaxZoom, 0.0001);

	// Navigation moves the selection spatially and brings the node into view; at the edge focus leaves.
	Panel->SetSelectedIndex(0);
	FNavigationReply Reply = Navigate(*Panel, Geometry, EUINavigation::Right);
	TestTrue(TEXT("Right navigates to node 1"), Navigated == TArray<int32>({1}));
	TestEqual(TEXT("Navigation keeps focus on the map"), Reply.GetBoundaryRule(), EUINavigationRule::Stop);
	TestEqual(TEXT("The navigated node is selected"), Panel->GetSelectedIndex(), 1);
	const FSlateRect NodeRect = Panel->GetNodeLocalRect(1);
	const double Margin = Panel->GetSettings().FocusMargin;
	TestTrue(TEXT("The navigated node is brought into view"),
		NodeRect.Left >= Margin - 0.01 && NodeRect.Right <= Viewport.X - Margin + 0.01 &&
		NodeRect.Top >= Margin - 0.01 && NodeRect.Bottom <= Viewport.Y - Margin + 0.01);
	Reply = Navigate(*Panel, Geometry, EUINavigation::Right);
	TestEqual(TEXT("At the map's edge focus escapes"), Reply.GetBoundaryRule(), EUINavigationRule::Escape);
	TestEqual(TEXT("Escaping keeps the selection"), Panel->GetSelectedIndex(), 1);
	Panel->SetSelectedIndex(INDEX_NONE);
	Panel->SetCamera(Start);
	Navigate(*Panel, Geometry, EUINavigation::Down);
	TestEqual(TEXT("Without a selection navigation starts at the node nearest the view center"), Panel->GetSelectedIndex(), 0);

	// DPI scale and window offset only change the geometry; clicks and drags work in local units.
	Clicked.Reset();
	Panel->SetCamera(Start);
	const FGeometry ScaledGeometry = FGeometry::MakeRoot(Viewport, FSlateLayoutTransform(1.5f, FVector2D(100.0, 50.0)));
	const FVector2D ScaledNodeScreen = ScaledGeometry.LocalToAbsolute(NodeScreen);
	Press(*Panel, ScaledGeometry, ScaledNodeScreen);
	Release(*Panel, ScaledGeometry, ScaledNodeScreen);
	TestTrue(TEXT("A click hits the node under DPI scale"), Clicked == TArray<int32>({0}));
	Press(*Panel, ScaledGeometry, ScaledNodeScreen);
	Move(*Panel, ScaledGeometry, ScaledNodeScreen + FVector2D(30.0, 0.0));
	TestTrue(TEXT("Thirty pixels at 1.5 DPI pan twenty local units"),
		Panel->GetCamera().MapToLocal(Positions[0], Viewport).Equals(NodeScreen + FVector2D(20.0, 0.0), 0.001));
	Release(*Panel, ScaledGeometry, ScaledNodeScreen + FVector2D(30.0, 0.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeMapWidgetLifecycleTest,
	"SurvivalRpg.UI.SkillTreeMap.Widget.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeMapWidgetLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeMapTests;

	FScopedWidgetWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	if (!TestNotNull(TEXT("Player state with skill trees spawns"), SkillTrees) || !TestNotNull(TEXT("Engine exists"), GEngine))
	{
		return false;
	}
	SkillTrees->RegisterSkillTree(MakeMapTree());
	SkillTrees->RegisterSkillTree(MakeGridTree());

	UCommonLocalPlayer* LocalPlayer = NewObject<UCommonLocalPlayer>(GEngine);
	URpgUIScreenSubsystem* ViewCache = NewObject<URpgUIScreenSubsystem>(LocalPlayer);
	URpgSkillTreeAutomationTestMapEntry* Host = CreateWidget<URpgSkillTreeAutomationTestMapEntry>(World);
	if (!TestNotNull(TEXT("View cache exists"), ViewCache) || !TestNotNull(TEXT("Host widget exists"), Host))
	{
		return false;
	}
	if (!Host->WidgetTree)
	{
		Host->WidgetTree = NewObject<UWidgetTree>(Host, TEXT("WidgetTree"), RF_Transient);
	}

	auto MakeMap = [Host, ViewCache]()
	{
		URpgSkillTreeMapWidget* NewMap = Host->WidgetTree->ConstructWidget<URpgSkillTreeMapWidget>();
		NewMap->NodeEntryClass = URpgSkillTreeAutomationTestMapEntry::StaticClass();
		NewMap->ViewStateCacheForTests = ViewCache;
		return NewMap;
	};
	auto MakeTreeView = [SkillTrees](const FGameplayTag TreeTag)
	{
		URpgSkillTreeViewModel* TreeView = NewObject<URpgSkillTreeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		TreeView->BindSkillTree(SkillTrees, TreeTag);
		return TreeView;
	};
	auto ClickAt = [](SRpgSkillTreeMapPanel& Panel, const FGeometry& Geometry, const FVector2D& Local)
	{
		Press(Panel, Geometry, Local);
		Release(Panel, Geometry, Local);
	};

	const FGameplayTag TreeTag = TAG_SkillTreeMapTest_Tree;
	URpgSkillTreeMapWidget* Map = MakeMap();
	URpgSkillTreeViewModel* TreeView = MakeTreeView(TreeTag);
	Map->SetSkillTree(TreeView);

	const TArray<UUserWidget*> Entries = Map->GetNodeEntries();
	TestEqual(TEXT("One entry per node"), Entries.Num(), 4);
	bool bEntriesShowTheirNodes = Entries.Num() == 4;
	for (const URpgSkillTreeNodeViewModel* Node : TreeView->GetNodes())
	{
		const URpgSkillTreeAutomationTestMapEntry* Entry = Cast<URpgSkillTreeAutomationTestMapEntry>(Map->GetNodeEntry(Node->GetNodeTag()));
		bEntriesShowTheirNodes &= Entry && Entry->GetEntryItem() == Node;
	}
	TestTrue(TEXT("Each entry receives its node view model"), bEntriesShowTheirNodes);

	Map->TakeWidget();
	TSharedPtr<SRpgSkillTreeMapPanel> Panel = Map->MyPanel;
	if (!TestTrue(TEXT("The map builds its panel"), Panel.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("Framing waits for the viewport size"), Panel->HasPendingFit());
	const FVector2D Viewport(1000.0, 700.0);
	const FGeometry Geometry = FGeometry::MakeRoot(Viewport, FSlateLayoutTransform(1.0f));
	Panel->Tick(Geometry, 0.0, 0.0f);
	TestFalse(TEXT("The first tick frames the map"), Panel->HasPendingFit());
	const FRpgSkillTreeRegionView* Forms = TreeView->FindRegion(TEXT("Forms"));
	TestTrue(TEXT("The default region is framed inside the padding"),
		Forms && IsInsideViewport(Panel->GetCamera(), Forms->Bounds.Min, Viewport, 63.9) &&
			IsInsideViewport(Panel->GetCamera(), Forms->Bounds.Max, Viewport, 63.9));
	const FRpgUIMapViewState* FittedState = ViewCache->FindMapViewState(TreeTag);
	TestTrue(TEXT("The framed view is remembered"),
		FittedState && FMath::IsNearlyEqual(FittedState->Zoom, Panel->GetCamera().Zoom) && FittedState->Center.Equals(Panel->GetCamera().Center));

	// Pan, zoom and refreshes keep every entry and view model.
	const TArray<URpgSkillTreeNodeViewModel*> NodesBefore = TreeView->GetNodes();
	Map->ZoomIn();
	Map->PanBy(FVector2D(40.0, -25.0));
	Map->ZoomOut();
	TreeView->Refresh();
	TestTrue(TEXT("Pan and zoom keep the entries"), Map->GetNodeEntries() == Entries);
	TestTrue(TEXT("Pan and zoom keep the node view models"), TreeView->GetNodes() == NodesBefore);

	Map->SetZoom(1.0f);
	TestEqual(TEXT("Zoomed in, entries show details"), Map->GetDetailLevel(), ERpgSkillTreeMapDetail::Detail);
	Map->SetZoom(0.3f);
	TestEqual(TEXT("Zoomed out, entries show the overview"), Map->GetDetailLevel(), ERpgSkillTreeMapDetail::Overview);

	// A click selects without learning anything.
	Map->SetZoom(1.0f);
	TestTrue(TEXT("FocusNode finds FormA"), Map->FocusNode(TAG_SkillTreeMapTest_FormA));
	ClickAt(*Panel, Geometry, Viewport * 0.5);
	URpgSkillTreeNodeViewModel* FormA = TreeView->FindNode(TAG_SkillTreeMapTest_FormA);
	TestTrue(TEXT("The clicked node is selected"), FormA && FormA->IsSelected());
	TestFalse(TEXT("A click never learns"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeMapTest_FormA));
	const FRpgUIMapViewState* SelectedState = ViewCache->FindMapViewState(TreeTag);
	TestTrue(TEXT("The selection is remembered"), SelectedState && SelectedState->SelectedTag == TAG_SkillTreeMapTest_FormA);

	// A drag and a canceled drag change the view but never the selection.
	const FVector2D SecondLocal = Panel->GetCamera().MapToLocal(FVector2D(300.0, 0.0), Viewport);
	Press(*Panel, Geometry, Viewport * 0.5);
	Move(*Panel, Geometry, Viewport * 0.5 + FVector2D(-40.0, 30.0));
	TestTrue(TEXT("Dragging pans the map"), Map->IsPanning());
	Release(*Panel, Geometry, Viewport * 0.5 + FVector2D(-40.0, 30.0));
	Press(*Panel, Geometry, SecondLocal + FVector2D(-40.0, 30.0));
	Move(*Panel, Geometry, SecondLocal + FVector2D(10.0, 10.0));
	Map->CancelGesture();
	TestFalse(TEXT("CancelGesture ends the pan"), Map->IsPanning());
	Release(*Panel, Geometry, SecondLocal + FVector2D(10.0, 10.0));
	TestTrue(TEXT("FormA stays selected"), FormA && FormA->IsSelected());

	// Closing and reopening with a new view model restores camera and selection.
	const FRpgSkillTreeMapCamera BeforeClose = Panel->GetCamera();
	Map->ReleaseSlateResources(true);
	Panel.Reset();
	URpgSkillTreeViewModel* ReopenedView = MakeTreeView(TreeTag);
	Map->SetSkillTree(ReopenedView);
	Map->TakeWidget();
	Panel = Map->MyPanel;
	TestTrue(TEXT("A reopened map restores its camera"), Panel.IsValid() && !Panel->HasPendingFit() && AreCamerasNearlyEqual(Panel->GetCamera(), BeforeClose));
	const URpgSkillTreeNodeViewModel* ReopenedFormA = ReopenedView->FindNode(TAG_SkillTreeMapTest_FormA);
	TestTrue(TEXT("A reopened map restores its selection"), ReopenedFormA && ReopenedFormA->IsSelected());

	URpgSkillTreeMapWidget* RecreatedMap = MakeMap();
	URpgSkillTreeViewModel* RecreatedView = MakeTreeView(TreeTag);
	RecreatedMap->SetSkillTree(RecreatedView);
	RecreatedMap->TakeWidget();
	TestTrue(TEXT("A newly created map restores the camera"),
		RecreatedMap->MyPanel.IsValid() && AreCamerasNearlyEqual(RecreatedMap->MyPanel->GetCamera(), BeforeClose));
	const URpgSkillTreeNodeViewModel* RecreatedFormA = RecreatedView->FindNode(TAG_SkillTreeMapTest_FormA);
	TestTrue(TEXT("A newly created map restores the selection"), RecreatedFormA && RecreatedFormA->IsSelected());
	RecreatedMap->ReleaseSlateResources(true);

	// Another tree has its own view; the legacy grid layout still places nodes from Row and Column.
	Map->SetSkillTree(MakeTreeView(TAG_SkillTreeMapTest_GridTree));
	TestTrue(TEXT("Another tree frames itself"), Panel->HasPendingFit());
	Panel->Tick(Geometry, 0.0, 0.0f);
	TestEqual(TEXT("The grid tree has its three entries"), Panel->GetNodeCount(), 3);
	TestTrue(TEXT("Grid cells become map positions"),
		Panel->GetNodePosition(0).Equals(FVector2D(0.0, 0.0)) && Panel->GetNodePosition(1).Equals(FVector2D(320.0, 0.0)) &&
			Panel->GetNodePosition(2).Equals(FVector2D(160.0, 160.0)));
	bool bGridFits = true;
	for (int32 Index = 0; Index < Panel->GetNodeCount(); ++Index)
	{
		bGridFits &= IsInsideViewport(Panel->GetCamera(), Panel->GetNodePosition(Index), Viewport, 63.9);
	}
	TestTrue(TEXT("The framed grid tree shows every node"), bGridFits);
	Map->SetSkillTree(ReopenedView);
	TestTrue(TEXT("Switching back restores the first tree's view"), AreCamerasNearlyEqual(Panel->GetCamera(), BeforeClose));

	Map->SetSkillTree(nullptr);
	TestEqual(TEXT("Clearing the tree removes every entry"), Panel->GetNodeCount(), 0);
	Map->ReleaseSlateResources(true);

	ViewCache->Deinitialize();
	TestNull(TEXT("Removing the local player forgets the views"), ViewCache->FindMapViewState(TreeTag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeMapContentTest,
	"SurvivalRpg.UI.SkillTreeMap.Content.ExistingTreesFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeMapContentTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeMapTests;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();
	TArray<FAssetData> TreeAssets;
	AssetRegistry.GetAssetsByClass(URpgSkillTreeDefinition::StaticClass()->GetClassPathName(), TreeAssets, true);
	TestTrue(TEXT("Skill tree assets exist"), !TreeAssets.IsEmpty());

	// The shipped trees use the legacy grid; framed at 720p every node must appear inside the padding.
	const FVector2D Viewport(1280.0, 720.0);
	constexpr double Padding = 64.0;
	for (const FAssetData& TreeAsset : TreeAssets)
	{
		const URpgSkillTreeDefinition* Tree = Cast<URpgSkillTreeDefinition>(TreeAsset.GetAsset());
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *TreeAsset.GetObjectPathString()), Tree))
		{
			continue;
		}

		const FBox2D Bounds = Tree->GetMapBounds();
		TestTrue(*FString::Printf(TEXT("%s has map bounds"), *GetNameSafe(Tree)), Bounds.bIsValid);
		const FRpgSkillTreeMapCamera Fit = FRpgSkillTreeMapCamera::MakeFit(Bounds, Viewport, Padding, 0.25, 2.0);
		for (const FRpgSkillTreeNode& Node : Tree->Nodes)
		{
			TestTrue(
				*FString::Printf(TEXT("%s: %s is visible when the tree is framed"), *GetNameSafe(Tree), *Node.NodeTag.ToString()),
				IsInsideViewport(Fit, Tree->GetNodeMapPosition(Node), Viewport, Padding - 0.1));
		}
	}
	return true;
}

#endif
