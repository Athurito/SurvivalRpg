// Copyright Epic Games, Inc. All Rights Reserved.

#include "SActorCanvas.h"

#include "Engine/GameViewportClient.h"
#include "IActorIndicatorWidget.h"
#include "IndicatorDescriptor.h"
#include "Layout/ArrangedChildren.h"
#include "RpgIndicatorManagerComponent.h"
#include "SceneView.h"
#include "Widgets/Layout/SBox.h"

void SActorCanvas::Construct(const FArguments& InArgs, const FLocalPlayerContext& InLocalPlayerContext)
{
	LocalPlayerContext = InLocalPlayerContext;
	IndicatorPool.SetWorld(LocalPlayerContext.GetWorld());
	IndicatorPool.SetDefaultPlayerController(LocalPlayerContext.GetPlayerController());

	SetCanTick(false);
	SetVisibility(EVisibility::SelfHitTestInvisible);
	UpdateActiveTimer();
}

EActiveTimerReturnType SActorCanvas::UpdateCanvas(double CurrentTime, float DeltaTime)
{
	if (!PaintGeometry.IsSet())
	{
		return EActiveTimerReturnType::Continue;
	}

	URpgIndicatorManagerComponent* Manager = IndicatorManager.Get();
	if (!Manager)
	{
		Manager = URpgIndicatorManagerComponent::GetComponent(LocalPlayerContext.GetPlayerController());
		if (!Manager)
		{
			return EActiveTimerReturnType::Continue;
		}

		IndicatorManager = Manager;
		IndicatorPool.SetWorld(LocalPlayerContext.GetWorld());
		IndicatorPool.SetDefaultPlayerController(LocalPlayerContext.GetPlayerController());
		Manager->OnIndicatorAdded.AddSP(this, &SActorCanvas::OnIndicatorAdded);
		Manager->OnIndicatorRemoved.AddSP(this, &SActorCanvas::OnIndicatorRemoved);

		for (UIndicatorDescriptor* Indicator : Manager->GetIndicators())
		{
			OnIndicatorAdded(Indicator);
		}
	}

	ULocalPlayer* LocalPlayer = LocalPlayerContext.GetLocalPlayer();
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport)
	{
		return EActiveTimerReturnType::Continue;
	}

	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
	{
		return EActiveTimerReturnType::Continue;
	}

	bool bNeedsPaint = false;
	for (int32 ChildIndex = CanvasChildren.Num() - 1; ChildIndex >= 0; --ChildIndex)
	{
		FSlot& Slot = CanvasChildren[ChildIndex];
		UIndicatorDescriptor* Indicator = Slot.Indicator;
		if (!Indicator || Indicator->CanAutomaticallyRemove())
		{
			if (Indicator)
			{
				Indicator->UnregisterIndicator();
			}
			continue;
		}

		FVector ScreenPositionWithDepth;
		const bool bHasPosition = Indicator->GetIsVisible()
			&& FIndicatorProjection().Project(*Indicator, ProjectionData, PaintGeometry->GetLocalSize(), ScreenPositionWithDepth);

		Slot.bHasValidPosition = bHasPosition;
		Slot.GetWidget()->SetVisibility(bHasPosition ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
		if (bHasPosition)
		{
			Slot.ScreenPosition = FVector2D(ScreenPositionWithDepth);
			Slot.Depth = ScreenPositionWithDepth.Z;
		}
		bNeedsPaint = true;
	}

	if (bNeedsPaint)
	{
		Invalidate(EInvalidateWidget::Paint);
	}

	if (ActiveIndicators.IsEmpty() && IndicatorManager.IsValid())
	{
		TickHandle.Reset();
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

void SActorCanvas::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	TArray<const FSlot*> SortedSlots;
	for (int32 ChildIndex = 0; ChildIndex < CanvasChildren.Num(); ++ChildIndex)
	{
		SortedSlots.Add(&CanvasChildren[ChildIndex]);
	}

	SortedSlots.StableSort([](const FSlot& A, const FSlot& B)
	{
		const int32 PriorityA = A.Indicator ? A.Indicator->GetPriority() : 0;
		const int32 PriorityB = B.Indicator ? B.Indicator->GetPriority() : 0;
		return PriorityA == PriorityB ? A.Depth > B.Depth : PriorityA < PriorityB;
	});

	for (const FSlot* Slot : SortedSlots)
	{
		if (!Slot || !Slot->Indicator || !Slot->bHasValidPosition || !ArrangedChildren.Accepts(Slot->GetWidget()->GetVisibility()))
		{
			continue;
		}

		FVector2D Size = FVector2D::ZeroVector;
		FVector2D Offset = FVector2D::ZeroVector;
		GetOffsetAndSize(Slot->Indicator, Size, Offset);

		FVector2D ScreenPosition = Slot->ScreenPosition;
		if (Slot->Indicator->GetClampToScreen())
		{
			ScreenPosition.X = FMath::Clamp(ScreenPosition.X, -Offset.X, AllottedGeometry.GetLocalSize().X - (Size.X + Offset.X));
			ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y, -Offset.Y, AllottedGeometry.GetLocalSize().Y - (Size.Y + Offset.Y));
		}

		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Slot->GetWidget(), ScreenPosition + Offset, Size, 1.0f));
	}
}

int32 SActorCanvas::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	PaintGeometry = AllottedGeometry;

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	int32 MaxLayerId = LayerId;
	const FPaintArgs NewArgs = Args.WithNewParent(this);
	const bool bShouldBeEnabled = ShouldBeEnabled(bParentEnabled);
	for (const FArrangedWidget& Widget : ArrangedChildren.GetInternalArray())
	{
		if (!IsChildWidgetCulled(MyCullingRect, Widget))
		{
			MaxLayerId = FMath::Max(
				MaxLayerId,
				Widget.Widget->Paint(NewArgs, Widget.Geometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bShouldBeEnabled));
		}
	}

	return MaxLayerId;
}

FString SActorCanvas::GetReferencerName() const
{
	return TEXT("SActorCanvas");
}

void SActorCanvas::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ActiveIndicators);
}

void SActorCanvas::OnIndicatorAdded(UIndicatorDescriptor* Indicator)
{
	if (Indicator && !ActiveIndicators.Contains(Indicator))
	{
		ActiveIndicators.Add(Indicator);
		AddIndicatorWidget(Indicator);
		UpdateActiveTimer();
	}
}

void SActorCanvas::OnIndicatorRemoved(UIndicatorDescriptor* Indicator)
{
	RemoveIndicatorWidget(Indicator);
	ActiveIndicators.Remove(Indicator);
}

void SActorCanvas::AddIndicatorWidget(UIndicatorDescriptor* Indicator)
{
	const TSoftClassPtr<UUserWidget> IndicatorClass = Indicator->GetIndicatorClass();
	UClass* WidgetClass = IndicatorClass.LoadSynchronous();
	if (!WidgetClass)
	{
		return;
	}

	UUserWidget* IndicatorWidget = IndicatorPool.GetOrCreateInstance(TSubclassOf<UUserWidget>(WidgetClass));
	if (!IndicatorWidget)
	{
		return;
	}

	if (IndicatorWidget->GetClass()->ImplementsInterface(UIndicatorWidgetInterface::StaticClass()))
	{
		IIndicatorWidgetInterface::Execute_BindIndicator(IndicatorWidget, Indicator);
	}

	Indicator->IndicatorWidget = IndicatorWidget;
	AddActorSlot(Indicator)
	[
		SAssignNew(Indicator->CanvasHost, SBox)
		[
			IndicatorWidget->TakeWidget()
		]
	];
}

void SActorCanvas::RemoveIndicatorWidget(UIndicatorDescriptor* Indicator)
{
	if (!Indicator)
	{
		return;
	}

	if (UUserWidget* IndicatorWidget = Indicator->IndicatorWidget.Get())
	{
		if (IndicatorWidget->GetClass()->ImplementsInterface(UIndicatorWidgetInterface::StaticClass()))
		{
			IIndicatorWidgetInterface::Execute_UnbindIndicator(IndicatorWidget, Indicator);
		}
		Indicator->IndicatorWidget = nullptr;
		IndicatorPool.Release(IndicatorWidget);
	}

	if (TSharedPtr<SWidget> CanvasHost = Indicator->CanvasHost.Pin())
	{
		for (int32 SlotIndex = 0; SlotIndex < CanvasChildren.Num(); ++SlotIndex)
		{
			if (CanvasHost == CanvasChildren[SlotIndex].GetWidget())
			{
				CanvasChildren.RemoveAt(SlotIndex);
				break;
			}
		}
		Indicator->CanvasHost.Reset();
	}
}

SActorCanvas::FScopedWidgetSlotArguments SActorCanvas::AddActorSlot(UIndicatorDescriptor* Indicator)
{
	return FScopedWidgetSlotArguments(MakeUnique<FSlot>(Indicator), CanvasChildren, INDEX_NONE);
}

void SActorCanvas::GetOffsetAndSize(const UIndicatorDescriptor* Indicator, FVector2D& OutSize, FVector2D& OutOffset) const
{
	OutSize = FVector2D::ZeroVector;
	OutOffset = FVector2D::ZeroVector;

	if (const TSharedPtr<SWidget> CanvasHost = Indicator->CanvasHost.Pin())
	{
		OutSize = CanvasHost->GetDesiredSize();
	}

	switch (Indicator->GetHAlign())
	{
	case HAlign_Left:
		break;
	case HAlign_Right:
		OutOffset.X = -OutSize.X;
		break;
	case HAlign_Center:
	default:
		OutOffset.X = -OutSize.X * 0.5f;
		break;
	}

	switch (Indicator->GetVAlign())
	{
	case VAlign_Top:
		break;
	case VAlign_Bottom:
		OutOffset.Y = -OutSize.Y;
		break;
	case VAlign_Center:
	default:
		OutOffset.Y = -OutSize.Y * 0.5f;
		break;
	}
}

void SActorCanvas::UpdateActiveTimer()
{
	if (!TickHandle.IsValid() && (!IndicatorManager.IsValid() || !ActiveIndicators.IsEmpty()))
	{
		TickHandle = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SActorCanvas::UpdateCanvas));
	}
}
