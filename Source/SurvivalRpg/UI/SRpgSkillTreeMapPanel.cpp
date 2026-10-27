#include "SRpgSkillTreeMapPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

FVector2D FRpgSkillTreeMapCamera::MapToLocal(const FVector2D& MapPoint, const FVector2D& ViewportSize) const
{
	return (MapPoint - Center) * Zoom + ViewportSize * 0.5;
}

FVector2D FRpgSkillTreeMapCamera::LocalToMap(const FVector2D& LocalPoint, const FVector2D& ViewportSize) const
{
	return Zoom > 0.0 ? (LocalPoint - ViewportSize * 0.5) / Zoom + Center : Center;
}

void FRpgSkillTreeMapCamera::ZoomAt(
	const FVector2D& LocalAnchor,
	const double NewZoom,
	const double MinZoom,
	const double MaxZoom,
	const FVector2D& ViewportSize)
{
	const double ClampedZoom = FMath::Clamp(FMath::IsFinite(NewZoom) && NewZoom > 0.0 ? NewZoom : Zoom, MinZoom, MaxZoom);
	const FVector2D AnchoredMapPoint = LocalToMap(LocalAnchor, ViewportSize);
	Zoom = ClampedZoom;
	Center = AnchoredMapPoint - (LocalAnchor - ViewportSize * 0.5) / Zoom;
}

void FRpgSkillTreeMapCamera::PanByLocal(const FVector2D& LocalDelta)
{
	if (Zoom > 0.0)
	{
		Center -= LocalDelta / Zoom;
	}
}

bool FRpgSkillTreeMapCamera::BringIntoView(
	const FVector2D& MapPoint,
	const FVector2D& LocalSize,
	const double Margin,
	const FVector2D& ViewportSize)
{
	const FVector2D Local = MapToLocal(MapPoint, ViewportSize);
	FVector2D Shift = FVector2D::ZeroVector;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const double Low = Margin + LocalSize[Axis] * 0.5;
		const double High = ViewportSize[Axis] - Margin - LocalSize[Axis] * 0.5;
		if (Low > High)
		{
			Shift[Axis] = ViewportSize[Axis] * 0.5 - Local[Axis];
		}
		else if (Local[Axis] < Low)
		{
			Shift[Axis] = Low - Local[Axis];
		}
		else if (Local[Axis] > High)
		{
			Shift[Axis] = High - Local[Axis];
		}
	}

	if (Shift.IsNearlyZero())
	{
		return false;
	}
	PanByLocal(Shift);
	return true;
}

void FRpgSkillTreeMapCamera::ClampCenter(const FBox2D& Bounds)
{
	if (Bounds.bIsValid)
	{
		Center.X = FMath::Clamp(Center.X, Bounds.Min.X, Bounds.Max.X);
		Center.Y = FMath::Clamp(Center.Y, Bounds.Min.Y, Bounds.Max.Y);
	}
}

FRpgSkillTreeMapCamera FRpgSkillTreeMapCamera::MakeFit(
	const FBox2D& Bounds,
	const FVector2D& ViewportSize,
	const double Padding,
	const double MinZoom,
	const double MaxZoom)
{
	FRpgSkillTreeMapCamera Result;
	Result.Zoom = FMath::Clamp(1.0, MinZoom, MaxZoom);
	if (!Bounds.bIsValid)
	{
		return Result;
	}

	Result.Center = Bounds.GetCenter();
	const FVector2D Size = Bounds.GetSize();
	const FVector2D Available(
		FMath::Max(ViewportSize.X - 2.0 * Padding, 1.0),
		FMath::Max(ViewportSize.Y - 2.0 * Padding, 1.0));
	double Zoom = MaxZoom;
	if (Size.X > UE_KINDA_SMALL_NUMBER)
	{
		Zoom = FMath::Min(Zoom, Available.X / Size.X);
	}
	if (Size.Y > UE_KINDA_SMALL_NUMBER)
	{
		Zoom = FMath::Min(Zoom, Available.Y / Size.Y);
	}
	Result.Zoom = FMath::Clamp(Zoom, MinZoom, MaxZoom);
	return Result;
}

namespace RpgSkillTreeMapNavigation
{
	int32 FindNode(const TConstArrayView<FVector2D> Positions, const int32 FromIndex, const EUINavigation Direction)
	{
		if (!Positions.IsValidIndex(FromIndex))
		{
			return INDEX_NONE;
		}
		if (Direction == EUINavigation::Next)
		{
			return Positions.IsValidIndex(FromIndex + 1) ? FromIndex + 1 : INDEX_NONE;
		}
		if (Direction == EUINavigation::Previous)
		{
			return FromIndex > 0 ? FromIndex - 1 : INDEX_NONE;
		}

		FVector2D Axis;
		switch (Direction)
		{
		case EUINavigation::Left:
			Axis = FVector2D(-1.0, 0.0);
			break;
		case EUINavigation::Right:
			Axis = FVector2D(1.0, 0.0);
			break;
		case EUINavigation::Up:
			Axis = FVector2D(0.0, -1.0);
			break;
		case EUINavigation::Down:
			Axis = FVector2D(0.0, 1.0);
			break;
		default:
			return INDEX_NONE;
		}

		// Distance along the direction plus a penalty for drifting off its axis, so a node straight ahead wins over a
		// slightly closer diagonal one.
		constexpr double OffAxisWeight = 2.0;
		int32 BestIndex = INDEX_NONE;
		double BestScore = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			if (Index == FromIndex)
			{
				continue;
			}
			const FVector2D Delta = Positions[Index] - Positions[FromIndex];
			const double Along = FVector2D::DotProduct(Delta, Axis);
			if (Along <= UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const double Score = Along + FMath::Abs(FVector2D::CrossProduct(Delta, Axis)) * OffAxisWeight;
			if (Score < BestScore)
			{
				BestScore = Score;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}

	int32 FindNearest(const TConstArrayView<FVector2D> Positions, const FVector2D& MapPoint)
	{
		int32 BestIndex = INDEX_NONE;
		double BestDistance = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const double Distance = FVector2D::DistSquared(Positions[Index], MapPoint);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}
}

SRpgSkillTreeMapPanel::SRpgSkillTreeMapPanel()
	: Children(this)
{
}

void SRpgSkillTreeMapPanel::Construct(const FArguments& InArgs)
{
	OnNodeClicked = InArgs._OnNodeClicked;
	OnNodeNavigated = InArgs._OnNodeNavigated;
	OnViewChanged = InArgs._OnViewChanged;
	OnSelectedNodeMoved = InArgs._OnSelectedNodeMoved;
	SetClipping(EWidgetClipping::ClipToBounds);
	SetCanTick(true);
}

void SRpgSkillTreeMapPanel::SetSettings(const FRpgSkillTreeMapSettings& InSettings)
{
	Settings = InSettings;
	Settings.MinZoom = FMath::Max(Settings.MinZoom, UE_KINDA_SMALL_NUMBER);
	Settings.MaxZoom = FMath::Max(Settings.MaxZoom, Settings.MinZoom);
	Settings.ZoomStep = FMath::Max(Settings.ZoomStep, 1.0 + UE_KINDA_SMALL_NUMBER);
	Settings.MaxEntryScale = FMath::Max(Settings.MaxEntryScale, Settings.MinEntryScale);
	Settings.CameraBlendTime = FMath::Max(Settings.CameraBlendTime, 0.0);
	SetCameraInternal(Camera, true);
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SRpgSkillTreeMapPanel::SetNodes(const TConstArrayView<TSharedRef<SWidget>> Entries, const TConstArrayView<FVector2D> Positions)
{
	Children.Empty(Entries.Num());
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FSlot::FSlotArguments SlotArguments(MakeUnique<FSlot>());
		SlotArguments.AttachWidget(Entries[Index]);
		const int32 SlotIndex = Children.AddSlot(MoveTemp(SlotArguments));
		Children[SlotIndex].Position = Positions.IsValidIndex(Index) ? Positions[Index] : FVector2D::ZeroVector;
	}

	// A press that started on the old node set must not click whatever node now has its index.
	PressedNodeIndex = INDEX_NONE;
	if (!Children.IsValidIndex(SelectedIndex))
	{
		SelectedIndex = INDEX_NONE;
	}
	Invalidate(EInvalidateWidgetReason::ChildOrder | EInvalidateWidgetReason::Layout);
}

void SRpgSkillTreeMapPanel::SetNodePositions(const TConstArrayView<FVector2D> Positions)
{
	if (Positions.Num() != Children.Num())
	{
		return;
	}
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		Children[Index].Position = Positions[Index];
	}
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SRpgSkillTreeMapPanel::SetLinks(TArray<FLink> InLinks)
{
	Links = MoveTemp(InLinks);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SRpgSkillTreeMapPanel::SetRegions(TArray<FBox2D> InRegions)
{
	Regions = MoveTemp(InRegions);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SRpgSkillTreeMapPanel::SetContentBounds(const FBox2D& InBounds)
{
	ContentBounds = InBounds;
	SetCameraInternal(Camera, true);
}

void SRpgSkillTreeMapPanel::SetSelectedIndex(const int32 NodeIndex)
{
	const int32 NewIndex = Children.IsValidIndex(NodeIndex) ? NodeIndex : INDEX_NONE;
	if (SelectedIndex != NewIndex)
	{
		SelectedIndex = NewIndex;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FVector2D SRpgSkillTreeMapPanel::GetNodePosition(const int32 NodeIndex) const
{
	return Children.IsValidIndex(NodeIndex) ? Children[NodeIndex].Position : FVector2D::ZeroVector;
}

void SRpgSkillTreeMapPanel::SetCamera(const FRpgSkillTreeMapCamera& InCamera)
{
	FitBounds.Reset();
	SetCameraInternal(InCamera);
}

void SRpgSkillTreeMapPanel::RequestFit(const FBox2D& Bounds, const bool bGlide)
{
	FitBounds = Bounds;
	FittedViewportSize = FVector2D::ZeroVector;
	bGlideNextFit = bGlide;
	ApplyFitForViewport();
}

void SRpgSkillTreeMapPanel::ApplyFitForViewport()
{
	if (!FitBounds.IsSet() || ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0 || ViewportSize.Equals(FittedViewportSize))
	{
		return;
	}
	// Only a requested fit glides; framing an opening screen or a settling layout jumps.
	const bool bGlide = bGlideNextFit;
	bGlideNextFit = false;
	FittedViewportSize = ViewportSize;
	SetCameraInternal(
		FRpgSkillTreeMapCamera::MakeFit(FitBounds.GetValue(), ViewportSize, Settings.FitPadding, Settings.MinZoom, Settings.MaxZoom),
		bGlide);
}

void SRpgSkillTreeMapPanel::ZoomBySteps(const double Steps)
{
	ZoomAtLocal(ViewportSize * 0.5, Camera.Zoom * FMath::Pow(Settings.ZoomStep, Steps));
}

void SRpgSkillTreeMapPanel::ZoomAtLocal(const FVector2D& LocalAnchor, const double NewZoom)
{
	// Anchor on what the player sees, so the point under the cursor stays put even in the middle of a glide.
	FRpgSkillTreeMapCamera NewCamera = ShownCamera;
	NewCamera.ZoomAt(LocalAnchor, NewZoom, Settings.MinZoom, Settings.MaxZoom, ViewportSize);
	FitBounds.Reset();
	SetCameraInternal(NewCamera, true, &LocalAnchor);
}

void SRpgSkillTreeMapPanel::PanByLocal(const FVector2D& LocalDelta)
{
	// A pan grabs the map where it is shown and stops any glide there.
	FRpgSkillTreeMapCamera NewCamera = ShownCamera;
	NewCamera.PanByLocal(LocalDelta);
	SetCamera(NewCamera);
}

void SRpgSkillTreeMapPanel::BringNodeIntoView(const int32 NodeIndex)
{
	// Without a known viewport there is nothing to compare against; a pending fit stays in charge.
	if (!Children.IsValidIndex(NodeIndex) || ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0)
	{
		return;
	}

	FRpgSkillTreeMapCamera NewCamera = Camera;
	const FVector2D EntrySize = GetEntrySize(NodeIndex) * FMath::Clamp(Camera.Zoom, Settings.MinEntryScale, Settings.MaxEntryScale);
	if (NewCamera.BringIntoView(Children[NodeIndex].Position, EntrySize, Settings.FocusMargin, ViewportSize))
	{
		FitBounds.Reset();
		SetCameraInternal(NewCamera, true);
	}
}

void SRpgSkillTreeMapPanel::CenterOnNode(const int32 NodeIndex)
{
	if (Children.IsValidIndex(NodeIndex))
	{
		FRpgSkillTreeMapCamera NewCamera = Camera;
		NewCamera.Center = Children[NodeIndex].Position;
		FitBounds.Reset();
		SetCameraInternal(NewCamera, true);
	}
}

void SRpgSkillTreeMapPanel::CancelGesture()
{
	const bool bHadPress = IsPointerDown();
	PressedButton = FKey();
	bDragging = false;
	PressedNodeIndex = INDEX_NONE;

	// A capture kept by a hidden or closing map would swallow the next clicks elsewhere.
	if (bHadPress && FSlateApplication::IsInitialized())
	{
		const TSharedRef<const SWidget> Self = SharedThis(this);
		FSlateApplication::Get().ForEachUser([&Self](FSlateUser& User)
		{
			if (User.DoesWidgetHaveAnyCapture(Self))
			{
				User.ReleaseAllCapture();
			}
		});
	}
}

double SRpgSkillTreeMapPanel::GetEntryScale() const
{
	return FMath::Clamp(ShownCamera.Zoom, Settings.MinEntryScale, Settings.MaxEntryScale);
}

FSlateRect SRpgSkillTreeMapPanel::GetNodeLocalRect(const int32 NodeIndex) const
{
	if (!Children.IsValidIndex(NodeIndex))
	{
		return FSlateRect();
	}
	const FVector2D Center = ShownCamera.MapToLocal(Children[NodeIndex].Position, ViewportSize);
	const FVector2D HalfSize = GetEntrySize(NodeIndex) * GetEntryScale() * 0.5;
	return FSlateRect(Center - HalfSize, Center + HalfSize);
}

int32 SRpgSkillTreeMapPanel::HitTestNode(const FVector2D& LocalPoint) const
{
	const double MinHalf = Settings.MinHitTargetSize * 0.5;
	int32 BestIndex = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		const FVector2D Center = ShownCamera.MapToLocal(Children[Index].Position, ViewportSize);
		const FVector2D EntryHalf = GetEntrySize(Index) * GetEntryScale() * 0.5;
		const FVector2D Half(FMath::Max(EntryHalf.X, MinHalf), FMath::Max(EntryHalf.Y, MinHalf));
		const FVector2D Delta = LocalPoint - Center;
		if (FMath::Abs(Delta.X) > Half.X || FMath::Abs(Delta.Y) > Half.Y)
		{
			continue;
		}
		const double Distance = Delta.SizeSquared();
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestIndex = Index;
		}
	}
	return BestIndex;
}

void SRpgSkillTreeMapPanel::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Scale = static_cast<float>(GetEntryScale());
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		const FSlot& Slot = Children[Index];
		const TSharedRef<SWidget>& Widget = Slot.GetWidget();
		if (!ArrangedChildren.Accepts(Widget->GetVisibility()))
		{
			continue;
		}

		const FVector2D EntryDesiredSize = Widget->GetDesiredSize();
		const FVector2D TopLeft = ShownCamera.MapToLocal(Slot.Position, Size) - EntryDesiredSize * Scale * 0.5;
		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Widget, EntryDesiredSize, FSlateLayoutTransform(Scale, TopLeft)));
	}
}

FChildren* SRpgSkillTreeMapPanel::GetChildren()
{
	return &Children;
}

int32 SRpgSkillTreeMapPanel::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

	// The background belongs to the map: it pans and zooms with the nodes instead of hanging behind the screen.
	if (const FSlateBrush* BackgroundBrush = Settings.BackgroundBrush;
		BackgroundBrush && BackgroundBrush->DrawAs != ESlateBrushDrawType::NoDrawType && ContentBounds.bIsValid)
	{
		const FVector2D BoundsSize = ContentBounds.GetSize();
		const double Side = FMath::Max(BoundsSize.X, BoundsSize.Y) * Settings.BackgroundScale;
		if (Side > 0.0)
		{
			const FVector2D Min = ShownCamera.MapToLocal(ContentBounds.GetCenter() - FVector2D(Side * 0.5), Size);
			const FVector2D Max = ShownCamera.MapToLocal(ContentBounds.GetCenter() + FVector2D(Side * 0.5), Size);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(Max - Min), FSlateLayoutTransform(1.0f, FVector2f(Min))),
				BackgroundBrush,
				ESlateDrawEffect::None,
				BackgroundBrush->GetTint(InWidgetStyle) * Tint);
		}
	}

	if (const FSlateBrush* RegionBrush = Settings.RegionBrush; RegionBrush && RegionBrush->DrawAs != ESlateBrushDrawType::NoDrawType)
	{
		for (const FBox2D& Region : Regions)
		{
			if (!Region.bIsValid)
			{
				continue;
			}
			const FVector2D Min = ShownCamera.MapToLocal(Region.Min, Size);
			const FVector2D Max = ShownCamera.MapToLocal(Region.Max, Size);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(Max - Min), FSlateLayoutTransform(1.0f, FVector2f(Min))),
				RegionBrush,
				ESlateDrawEffect::None,
				RegionBrush->GetTint(InWidgetStyle) * Tint);
		}
	}

	TArray<FVector2f> Points;
	Points.SetNumUninitialized(2);
	for (const FLink& Link : Links)
	{
		Points[0] = FVector2f(ShownCamera.MapToLocal(Link.From, Size));
		Points[1] = FVector2f(ShownCamera.MapToLocal(Link.To, Size));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Link.Color * Tint,
			true,
			Settings.LinkThickness);
	}

	int32 MaxLayerId = SPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);

	const FSlateBrush* FocusBrush = Settings.FocusBrush;
	if (bShowFocusBrush && Children.IsValidIndex(SelectedIndex) && FocusBrush &&
		FocusBrush->DrawAs != ESlateBrushDrawType::NoDrawType && HasAnyUserFocus().IsSet())
	{
		const FVector2D Center = ShownCamera.MapToLocal(Children[SelectedIndex].Position, Size);
		const FVector2D HalfSize = GetEntrySize(SelectedIndex) * GetEntryScale() * 0.5 + FVector2D(Settings.FocusPadding);
		++MaxLayerId;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			MaxLayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(HalfSize * 2.0), FSlateLayoutTransform(1.0f, FVector2f(Center - HalfSize))),
			FocusBrush,
			ESlateDrawEffect::None,
			FocusBrush->GetTint(InWidgetStyle) * Tint);
	}
	return MaxLayerId;
}

void SRpgSkillTreeMapPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SPanel::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// A moved camera keeps its center on resize and DPI changes; an untouched fit frames its bounds again.
	ViewportSize = AllottedGeometry.GetLocalSize();
	ApplyFitForViewport();
	AdvanceGlide(InDeltaTime);
	ReportSelectedNodeBox();
}

void SRpgSkillTreeMapPanel::ReportSelectedNodeBox()
{
	// The shown camera moves on every glide frame and the box with every resize, so compare the result, not the cause.
	const bool bHasSelection = Children.IsValidIndex(SelectedIndex) && !ViewportSize.IsNearlyZero();
	const FSlateRect Box = bHasSelection ? GetNodeLocalRect(SelectedIndex) : FSlateRect(0.0f, 0.0f, 0.0f, 0.0f);
	if (bHasSelection == bReportedSelection && Box == ReportedSelectedBox && ViewportSize == ReportedViewportSize)
	{
		return;
	}
	bReportedSelection = bHasSelection;
	ReportedSelectedBox = Box;
	ReportedViewportSize = ViewportSize;
	OnSelectedNodeMoved.ExecuteIfBound(bHasSelection, Box, ViewportSize);
}

FReply SRpgSkillTreeMapPanel::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Tunneling ahead of the entries keeps every press for the map, so a drag can never click an entry's control.
	return BeginPress(MyGeometry, MouseEvent);
}

FReply SRpgSkillTreeMapPanel::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Slate sends the second press of a double click as this event instead of another button down.
	return BeginPress(MyGeometry, MouseEvent);
}

FReply SRpgSkillTreeMapPanel::BeginPress(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button != EKeys::LeftMouseButton && Button != EKeys::RightMouseButton && Button != EKeys::MiddleMouseButton)
	{
		return FReply::Unhandled();
	}
	if (IsPointerDown())
	{
		// A second button during a gesture neither restarts nor clicks it.
		return FReply::Handled();
	}

	ViewportSize = MyGeometry.GetLocalSize();
	PressedButton = Button;
	PressLocal = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	LastPointerLocal = PressLocal;
	PressedNodeIndex = HitTestNode(PressLocal);
	bDragging = false;
	bShowFocusBrush = false;
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SRpgSkillTreeMapPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsPointerDown())
	{
		return FReply::Unhandled();
	}
	if (MouseEvent.GetEffectingButton() != PressedButton)
	{
		return FReply::Handled();
	}

	ViewportSize = MyGeometry.GetLocalSize();
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const bool bWasDrag = bDragging;
	const bool bWasLeft = PressedButton == EKeys::LeftMouseButton;
	const int32 ClickedNode = PressedNodeIndex;
	PressedButton = FKey();
	bDragging = false;
	PressedNodeIndex = INDEX_NONE;

	// A click needs the same node under the press and the release; the end of a drag never clicks.
	if (!bWasDrag && bWasLeft && ClickedNode != INDEX_NONE && HitTestNode(Local) == ClickedNode)
	{
		OnNodeClicked.ExecuteIfBound(ClickedNode);
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SRpgSkillTreeMapPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsPointerDown())
	{
		return FReply::Unhandled();
	}
	if (!MouseEvent.IsMouseButtonDown(PressedButton))
	{
		// The release went elsewhere, for example to another window; end the gesture instead of panning forever.
		PressedButton = FKey();
		bDragging = false;
		PressedNodeIndex = INDEX_NONE;
		return FReply::Handled().ReleaseMouseCapture();
	}

	ViewportSize = MyGeometry.GetLocalSize();
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (!bDragging && FVector2D::Distance(Local, PressLocal) >= Settings.DragThreshold)
	{
		bDragging = true;
	}
	if (bDragging)
	{
		// Panning from the last point, which is still the press point at the threshold, keeps the map under the cursor.
		PanByLocal(Local - LastPointerLocal);
		LastPointerLocal = Local;
	}
	return FReply::Handled();
}

FReply SRpgSkillTreeMapPanel::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ViewportSize = MyGeometry.GetLocalSize();
	const FVector2D Anchor = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	ZoomAtLocal(Anchor, Camera.Zoom * FMath::Pow(Settings.ZoomStep, static_cast<double>(MouseEvent.GetWheelDelta())));
	return FReply::Handled();
}

FReply SRpgSkillTreeMapPanel::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	if (GestureEvent.GetGestureType() != EGestureEvent::Magnify)
	{
		return FReply::Unhandled();
	}

	ViewportSize = MyGeometry.GetLocalSize();
	const FVector2D Anchor = MyGeometry.AbsoluteToLocal(GestureEvent.GetScreenSpacePosition());
	ZoomAtLocal(Anchor, Camera.Zoom * (1.0 + GestureEvent.GetGestureDelta().X));
	return FReply::Handled();
}

void SRpgSkillTreeMapPanel::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// Slate releases capture when the window deactivates or the map leaves the visible hierarchy, for example when
	// its screen closes; the gesture ends there without a click.
	PressedButton = FKey();
	bDragging = false;
	PressedNodeIndex = INDEX_NONE;
	SPanel::OnMouseCaptureLost(CaptureLostEvent);
}

FCursorReply SRpgSkillTreeMapPanel::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return bDragging ? FCursorReply::Cursor(EMouseCursor::GrabHandClosed) : FCursorReply::Unhandled();
}

FNavigationReply SRpgSkillTreeMapPanel::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	ViewportSize = MyGeometry.GetLocalSize();
	const TArray<FVector2D> Positions = GetNodePositions();
	const EUINavigation Direction = InNavigationEvent.GetNavigationType();

	int32 TargetIndex = INDEX_NONE;
	if (Children.IsValidIndex(SelectedIndex))
	{
		TargetIndex = RpgSkillTreeMapNavigation::FindNode(Positions, SelectedIndex, Direction);
	}
	else if (Direction == EUINavigation::Next)
	{
		TargetIndex = Positions.IsEmpty() ? INDEX_NONE : 0;
	}
	else if (Direction == EUINavigation::Previous)
	{
		TargetIndex = Positions.Num() - 1;
	}
	else if (Direction != EUINavigation::Invalid)
	{
		TargetIndex = RpgSkillTreeMapNavigation::FindNearest(Positions, Camera.Center);
	}

	if (TargetIndex == INDEX_NONE)
	{
		// At the map's edge, focus leaves for the neighboring widgets, for example the detail panel.
		return FNavigationReply::Escape();
	}
	NavigateTo(TargetIndex);
	return FNavigationReply::Stop();
}

FReply SRpgSkillTreeMapPanel::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	if (InFocusEvent.GetCause() != EFocusCause::Mouse)
	{
		bShowFocusBrush = true;
		ViewportSize = MyGeometry.GetLocalSize();

		// Navigating into the map picks a node, so the next press starts somewhere visible. Focus set directly, for
		// example by a screen that opens, leaves the selection to the player.
		if (InFocusEvent.GetCause() == EFocusCause::Navigation && !Children.IsValidIndex(SelectedIndex) && Children.Num() > 0)
		{
			NavigateTo(RpgSkillTreeMapNavigation::FindNearest(GetNodePositions(), Camera.Center));
		}
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return SPanel::OnFocusReceived(MyGeometry, InFocusEvent);
}

FVector2D SRpgSkillTreeMapPanel::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// The map fills whatever space its slot gives it; its content has no natural size.
	return FVector2D::ZeroVector;
}

void SRpgSkillTreeMapPanel::SetCameraInternal(
	const FRpgSkillTreeMapCamera& NewCamera,
	const bool bGlide,
	const FVector2D* ZoomAnchor)
{
	FRpgSkillTreeMapCamera Clamped = NewCamera;
	if (!FMath::IsFinite(Clamped.Zoom) || Clamped.Zoom <= 0.0)
	{
		Clamped.Zoom = Camera.Zoom;
	}
	Clamped.Zoom = FMath::Clamp(Clamped.Zoom, Settings.MinZoom, Settings.MaxZoom);
	if (!FMath::IsFinite(Clamped.Center.X) || !FMath::IsFinite(Clamped.Center.Y))
	{
		Clamped.Center = Camera.Center;
	}
	Clamped.ClampCenter(ContentBounds);

	if (Clamped == Camera)
	{
		// An unchanged camera keeps a running glide, unless the caller wants the view to jump.
		if (!bGlide && !(ShownCamera == Camera))
		{
			ShownCamera = Camera;
			GlideProgress = 1.0;
			GlideAnchorLocal.Reset();
			Invalidate(EInvalidateWidgetReason::Layout);
		}
		return;
	}

	Camera = Clamped;
	if (bGlide && Settings.CameraBlendTime > 0.0)
	{
		GlideStart = ShownCamera;
		GlideProgress = 0.0;
		GlideAnchorLocal.Reset();
		if (ZoomAnchor)
		{
			GlideAnchorLocal = *ZoomAnchor;
			GlideAnchorMap = ShownCamera.LocalToMap(*ZoomAnchor, ViewportSize);
		}
	}
	else
	{
		ShownCamera = Camera;
		GlideProgress = 1.0;
		GlideAnchorLocal.Reset();
	}
	Invalidate(EInvalidateWidgetReason::Layout);
	OnViewChanged.ExecuteIfBound();
}

void SRpgSkillTreeMapPanel::AdvanceGlide(const double DeltaTime)
{
	if (GlideProgress >= 1.0 || DeltaTime <= 0.0)
	{
		return;
	}

	// Exponential approach: quick at first, settling gently, independent of the frame rate.
	const double Alpha = Settings.CameraBlendTime > 0.0 ? 1.0 - FMath::Exp(-DeltaTime / Settings.CameraBlendTime) : 1.0;
	GlideProgress += (1.0 - GlideProgress) * Alpha;
	if (GlideProgress >= 0.995)
	{
		ShownCamera = Camera;
		GlideProgress = 1.0;
		GlideAnchorLocal.Reset();
	}
	else
	{
		// Zoom moves on a log scale, so every zoom step feels the same.
		ShownCamera.Zoom = FMath::Exp(FMath::Lerp(FMath::Loge(GlideStart.Zoom), FMath::Loge(Camera.Zoom), GlideProgress));
		if (GlideAnchorLocal.IsSet())
		{
			// Keep the anchored map point under the anchor; blend in any clamping of the target center on the way.
			const FVector2D AnchorOffset = GlideAnchorLocal.GetValue() - ViewportSize * 0.5;
			const FVector2D TargetAnchoredCenter = GlideAnchorMap - AnchorOffset / Camera.Zoom;
			ShownCamera.Center =
				GlideAnchorMap - AnchorOffset / ShownCamera.Zoom + (Camera.Center - TargetAnchoredCenter) * GlideProgress;
		}
		else
		{
			ShownCamera.Center = FMath::Lerp(GlideStart.Center, Camera.Center, GlideProgress);
		}
	}
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SRpgSkillTreeMapPanel::NavigateTo(const int32 NodeIndex)
{
	if (!Children.IsValidIndex(NodeIndex))
	{
		return;
	}
	SelectedIndex = NodeIndex;
	bShowFocusBrush = true;
	BringNodeIntoView(NodeIndex);
	Invalidate(EInvalidateWidgetReason::Paint);
	OnNodeNavigated.ExecuteIfBound(NodeIndex);
}

TArray<FVector2D> SRpgSkillTreeMapPanel::GetNodePositions() const
{
	TArray<FVector2D> Positions;
	Positions.Reserve(Children.Num());
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		Positions.Add(Children[Index].Position);
	}
	return Positions;
}

FVector2D SRpgSkillTreeMapPanel::GetEntrySize(const int32 NodeIndex) const
{
	const FVector2D EntryDesiredSize = Children.IsValidIndex(NodeIndex) ? Children[NodeIndex].GetWidget()->GetDesiredSize() : FVector2D::ZeroVector;
	return EntryDesiredSize.IsNearlyZero() ? FVector2D(Settings.MinHitTargetSize) : EntryDesiredSize;
}
