#pragma once

#include "CoreMinimal.h"
#include "Layout/Children.h"
#include "SlotBase.h"
#include "Types/SlateEnums.h"
#include "Widgets/SPanel.h"

struct FSlateBrush;

/**
 * Pan and zoom of a skill tree map: the map point shown at the viewport center and the slate units one map unit
 * spans. Map units are slate units at zoom 1, X to the right and Y down. Local presentation state.
 */
struct SURVIVALRPG_API FRpgSkillTreeMapCamera
{
	/** Map point at the viewport center. */
	FVector2D Center = FVector2D::ZeroVector;

	/** Slate units per map unit. */
	double Zoom = 1.0;

	/** Viewport-local position of MapPoint. */
	FVector2D MapToLocal(const FVector2D& MapPoint, const FVector2D& ViewportSize) const;

	/** Map point under the viewport-local LocalPoint; the exact inverse of MapToLocal. */
	FVector2D LocalToMap(const FVector2D& LocalPoint, const FVector2D& ViewportSize) const;

	/** Sets the zoom, clamped to [MinZoom, MaxZoom], keeping the map point under LocalAnchor in place. */
	void ZoomAt(const FVector2D& LocalAnchor, double NewZoom, double MinZoom, double MaxZoom, const FVector2D& ViewportSize);

	/** Moves the map with the pointer by LocalDelta slate units. */
	void PanByLocal(const FVector2D& LocalDelta);

	/**
	 * Pans as little as possible so a LocalSize box centered on MapPoint lies inside the viewport shrunk by Margin.
	 * A box larger than that area is centered instead. Returns true when the camera moved.
	 */
	bool BringIntoView(const FVector2D& MapPoint, const FVector2D& LocalSize, double Margin, const FVector2D& ViewportSize);

	/** Keeps Center inside Bounds when Bounds is valid, so the view never drifts away from the map. */
	void ClampCenter(const FBox2D& Bounds);

	/** Camera that shows all of Bounds inside the viewport shrunk by Padding, with a zoom clamped to [MinZoom, MaxZoom]. */
	static FRpgSkillTreeMapCamera MakeFit(
		const FBox2D& Bounds,
		const FVector2D& ViewportSize,
		double Padding,
		double MinZoom,
		double MaxZoom);

	bool operator==(const FRpgSkillTreeMapCamera& Other) const = default;
};

namespace RpgSkillTreeMapNavigation
{
	/**
	 * Returns the node to move to from FromIndex. Directions pick the nearest node in that half-plane, preferring nodes
	 * close to the direction's axis; Next and Previous step through the authored order, so every node stays reachable.
	 * INDEX_NONE when there is no such node, which lets focus leave the map at its edges and ends.
	 */
	SURVIVALRPG_API int32 FindNode(TConstArrayView<FVector2D> Positions, int32 FromIndex, EUINavigation Direction);

	/** Index of the node whose position is closest to MapPoint, or INDEX_NONE without nodes. */
	SURVIVALRPG_API int32 FindNearest(TConstArrayView<FVector2D> Positions, const FVector2D& MapPoint);
}

/** Tuning the map widget pushes into its panel. Units are slate units unless stated otherwise. */
struct FRpgSkillTreeMapSettings
{
	double MinZoom = 0.25;
	double MaxZoom = 2.0;

	/** Zoom factor per mouse wheel notch. */
	double ZoomStep = 1.15;

	/** Pointer travel that turns a press into a pan. */
	double DragThreshold = 6.0;

	/** Space kept free around fitted bounds. */
	double FitPadding = 64.0;

	/** Range the entry scale follows the zoom within, so entries stay readable when zoomed out. */
	double MinEntryScale = 0.5;
	double MaxEntryScale = 1.0;

	/** Smallest square a node's hit target covers, whatever its entry's size. */
	double MinHitTargetSize = 48.0;

	/** Space kept between a navigated node and the viewport edge. */
	double FocusMargin = 48.0;

	float LinkThickness = 3.0f;

	/** Drawn behind each region; null or NoDrawType draws nothing. Owned by the map widget. */
	const FSlateBrush* RegionBrush = nullptr;

	/** Drawn around the selected node while the map has keyboard or gamepad focus. Owned by the map widget. */
	const FSlateBrush* FocusBrush = nullptr;

	/** Space between the selected entry and the focus brush. */
	double FocusPadding = 6.0;

	/** Space between a tree's lowest node and its tree label. */
	double TreeLabelGap = 8.0;

	/**
	 * Time constant in seconds with which the shown camera glides to a new zoom, fit or focus. Zero jumps. Drags
	 * always follow the pointer directly.
	 */
	double CameraBlendTime = 0.0;

	/** Drawn behind everything, centered on the map and moving and scaling with it; null or NoDrawType draws nothing. */
	const FSlateBrush* BackgroundBrush = nullptr;

	/** Side of the square background as a multiple of the map's larger side. */
	double BackgroundScale = 1.2;
};

/**
 * Internal Slate panel of URpgSkillTreeMapWidget: places node entries on a clipped, pannable, zoomable map, draws
 * regions and prerequisite links with the same camera, and owns every pointer gesture. Entries are presentation only;
 * presses never reach them, so a drag can never click one. Hit testing, spatial focus and the camera all use one
 * transform. Tree labels, one per tree of a map with several trees, sit centered below their tree and take part in
 * neither hit testing nor navigation. Owns no gameplay state.
 */
class SURVIVALRPG_API SRpgSkillTreeMapPanel : public SPanel
{
public:
	/** One node entry and the center it sits at, in map units. */
	class FSlot : public TSlotBase<FSlot>
	{
	public:
		FVector2D Position = FVector2D::ZeroVector;
	};

	/**
	 * One tree label. It sits centered on Anchor's X, just below the lowest drawn node of its range, so it never covers
	 * a node at any zoom; a range without nodes puts it at Anchor.
	 */
	class FLabelSlot : public TSlotBase<FLabelSlot>
	{
	public:
		/** Usually the bottom center of the tree's bounds, in map units. */
		FVector2D Anchor = FVector2D::ZeroVector;

		/** Nodes of the label's tree: FirstNode and the NodeCount nodes after it in the node order. */
		int32 FirstNode = 0;
		int32 NodeCount = 0;
	};

	/** Where one tree label sits; see FLabelSlot. */
	struct FTreeLabelPlacement
	{
		FVector2D Anchor = FVector2D::ZeroVector;
		int32 FirstNode = 0;
		int32 NodeCount = 0;

		bool operator==(const FTreeLabelPlacement& Other) const = default;
	};

	/** One prerequisite link between two node centers, in map units. */
	struct FLink
	{
		FVector2D From = FVector2D::ZeroVector;
		FVector2D To = FVector2D::ZeroVector;
		FLinearColor Color = FLinearColor::White;
	};

	DECLARE_DELEGATE_OneParam(FOnNodeEvent, int32 /*NodeIndex*/);
	DECLARE_DELEGATE_ThreeParams(FOnSelectedNodeMoved, bool /*bHasSelection*/, const FSlateRect& /*LocalBox*/, const FVector2D& /*ViewportSize*/);

	SLATE_BEGIN_ARGS(SRpgSkillTreeMapPanel) {}
		/** A node was pressed and released with the left button without a drag. */
		SLATE_EVENT(FOnNodeEvent, OnNodeClicked)
		/** Keyboard or gamepad navigation moved to a node. */
		SLATE_EVENT(FOnNodeEvent, OnNodeNavigated)
		/** The camera moved or zoomed. */
		SLATE_EVENT(FSimpleDelegate, OnViewChanged)
		/**
		 * The selected node's shown box changed in viewport-local space: on every glide frame, pan, zoom and resize,
		 * and when the selection changes or clears. Reported once per tick at most, and only on a change.
		 */
		SLATE_EVENT(FOnSelectedNodeMoved, OnSelectedNodeMoved)
	SLATE_END_ARGS()

	SRpgSkillTreeMapPanel();

	void Construct(const FArguments& InArgs);

	void SetSettings(const FRpgSkillTreeMapSettings& InSettings);
	const FRpgSkillTreeMapSettings& GetSettings() const { return Settings; }

	/** Replaces every node entry. Entries and Positions are parallel arrays in authored node order. */
	void SetNodes(TConstArrayView<TSharedRef<SWidget>> Entries, TConstArrayView<FVector2D> Positions);

	/** Moves the existing entries without recreating them. Ignored unless the count matches. */
	void SetNodePositions(TConstArrayView<FVector2D> Positions);

	/** Replaces every tree label. Labels and Placements are parallel arrays. */
	void SetTreeLabels(TConstArrayView<TSharedRef<SWidget>> Labels, TConstArrayView<FTreeLabelPlacement> Placements);

	/** Moves the existing tree labels without recreating them. Ignored unless the count matches. */
	void SetTreeLabelPlacements(TConstArrayView<FTreeLabelPlacement> Placements);
	int32 GetTreeLabelCount() const { return TreeLabels.Num(); }

	/** Viewport-local box the tree label fills at the current camera. */
	FSlateRect GetTreeLabelLocalRect(int32 LabelIndex) const;

	void SetLinks(TArray<FLink> InLinks);
	void SetRegions(TArray<FBox2D> InRegions);

	/** Map area the camera center stays within; an invalid box lets it move freely. */
	void SetContentBounds(const FBox2D& InBounds);

	/** Node drawn as selected and used as the origin of navigation, or INDEX_NONE. */
	void SetSelectedIndex(int32 NodeIndex);
	int32 GetSelectedIndex() const { return SelectedIndex; }
	int32 GetNodeCount() const { return Children.Num(); }
	FVector2D GetNodePosition(int32 NodeIndex) const;

	/** Camera the view is at or gliding to. Gameplay-facing queries, the view cache and commands use it. */
	const FRpgSkillTreeMapCamera& GetCamera() const { return Camera; }

	/** Camera drawn this frame; it differs from GetCamera only while gliding. Hit tests use it. */
	const FRpgSkillTreeMapCamera& GetShownCamera() const { return ShownCamera; }

	/** Applies InCamera at once, with its zoom and center clamped, and ends a requested fit. */
	void SetCamera(const FRpgSkillTreeMapCamera& InCamera);

	/**
	 * Frames Bounds as soon as the panel knows its size, and again whenever that size changes, until anything else
	 * moves the camera. A screen's layout can settle over several frames; the fit follows it. bGlide glides to the
	 * first framing when the size is already known, for example for a Fit button.
	 */
	void RequestFit(const FBox2D& Bounds, bool bGlide = false);

	/** True while a requested fit has not met a viewport size yet, so the camera still shows its default. */
	bool HasPendingFit() const { return FitBounds.IsSet() && FittedViewportSize.IsZero(); }

	/** Zooms by Steps wheel notches around the viewport center; negative steps zoom out. */
	void ZoomBySteps(double Steps);

	/** Sets the zoom, keeping the map point under LocalAnchor in place. */
	void ZoomAtLocal(const FVector2D& LocalAnchor, double NewZoom);

	void PanByLocal(const FVector2D& LocalDelta);

	/** Glides so the node's entry lies inside the viewport with the focus margin. */
	void BringNodeIntoView(int32 NodeIndex);

	/** Glides so the node sits at the viewport center. */
	void CenterOnNode(int32 NodeIndex);

	/** Ends a press or drag without a click and releases nothing else. Safe to call at any time. */
	void CancelGesture();
	bool IsPointerDown() const { return PressedButton.IsValid(); }
	bool IsDragging() const { return bDragging; }

	/** Viewport size seen by the last tick or pointer event; zero before the first one. */
	FVector2D GetViewportSize() const { return ViewportSize; }

	/** Scale entries are drawn with at the current zoom. */
	double GetEntryScale() const;

	/** Viewport-local box the node's entry fills at the current camera. */
	FSlateRect GetNodeLocalRect(int32 NodeIndex) const;

	/** Node whose hit target contains LocalPoint, preferring the nearest center, or INDEX_NONE. */
	int32 HitTestNode(const FVector2D& LocalPoint) const;

	//~ SWidget interface
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	//~ End SWidget interface

protected:
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	FReply BeginPress(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * Clamps NewCamera and makes it the camera. Without bGlide, or without a blend time, the shown camera jumps there;
	 * otherwise it glides, keeping the map point under ZoomAnchor in place when one is given.
	 */
	void SetCameraInternal(const FRpgSkillTreeMapCamera& NewCamera, bool bGlide = false, const FVector2D* ZoomAnchor = nullptr);
	void ApplyFitForViewport();
	void AdvanceGlide(double DeltaTime);
	void NavigateTo(int32 NodeIndex);

	/** Reports the selected node's shown box through OnSelectedNodeMoved when it differs from the last report. */
	void ReportSelectedNodeBox();
	TArray<FVector2D> GetNodePositions() const;
	FVector2D GetEntrySize(int32 NodeIndex) const;

	/** Top-left corner and size of a tree label in viewport-local space, for a viewport of Size. */
	FSlateRect ComputeTreeLabelRect(int32 LabelIndex, const FVector2D& Size) const;

	/** Node entries; their indices are the node indices of every event and query. */
	TPanelChildren<FSlot> Children;
	TPanelChildren<FLabelSlot> TreeLabels;

	/** Nodes, then tree labels, for Slate's child traversal. */
	FCombinedChildren AllChildren;
	FRpgSkillTreeMapSettings Settings;
	FRpgSkillTreeMapCamera Camera;
	FRpgSkillTreeMapCamera ShownCamera;

	/** Glide from GlideStart to Camera: progress in [0, 1] and an optional viewport point that stays on its map point. */
	FRpgSkillTreeMapCamera GlideStart;
	double GlideProgress = 1.0;
	TOptional<FVector2D> GlideAnchorLocal;
	FVector2D GlideAnchorMap = FVector2D::ZeroVector;
	bool bGlideNextFit = false;
	/** Bounds the camera keeps framing until it is moved otherwise, and the viewport size it last framed them for. */
	TOptional<FBox2D> FitBounds;
	FVector2D FittedViewportSize = FVector2D::ZeroVector;
	FBox2D ContentBounds = FBox2D(ForceInit);
	TArray<FLink> Links;
	TArray<FBox2D> Regions;
	FVector2D ViewportSize = FVector2D::ZeroVector;
	int32 SelectedIndex = INDEX_NONE;

	/** Button of the current press, or invalid while no gesture runs. */
	FKey PressedButton;
	FVector2D PressLocal = FVector2D::ZeroVector;
	FVector2D LastPointerLocal = FVector2D::ZeroVector;
	int32 PressedNodeIndex = INDEX_NONE;
	bool bDragging = false;

	/** Shows the focus brush only after keyboard or gamepad navigation, not after mouse clicks. */
	bool bShowFocusBrush = false;

	FOnNodeEvent OnNodeClicked;
	FOnNodeEvent OnNodeNavigated;
	FSimpleDelegate OnViewChanged;
	FOnSelectedNodeMoved OnSelectedNodeMoved;

	/** Last selected-node box sent through OnSelectedNodeMoved, so unchanged frames send nothing. */
	bool bReportedSelection = false;
	FSlateRect ReportedSelectedBox = FSlateRect(0.0f, 0.0f, 0.0f, 0.0f);
	FVector2D ReportedViewportSize = FVector2D::ZeroVector;
};
