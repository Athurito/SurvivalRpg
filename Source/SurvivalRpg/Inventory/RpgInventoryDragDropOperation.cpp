#include "RpgInventoryDragDropOperation.h"

#include "RpgInventoryDragDropCoordinator.h"
#include "RpgInventoryInteractionSession.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDragDropOperation)

void URpgInventoryDragDropOperation::SetInteractionSession(URpgInventoryInteractionSession* InInteractionSession)
{
	InteractionSession = InInteractionSession;
}

FVector2D URpgInventoryDragDropOperation::ResolveDecoratorCenterScreen(FVector2D PointerScreenPosition) const
{
	if (!DefaultDragVisual || Pivot != EDragPivot::TopLeft)
	{
		return URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(InventoryPayload, PointerScreenPosition);
	}

	// FUMGDragDropOp wraps the visual in SDPIScaler before applying Offset, so mirror its desired screen size.
	const float ResolvedViewportScale = UWidgetLayoutLibrary::GetViewportScale(DefaultDragVisual);
	const float ViewportScale = ResolvedViewportScale > KINDA_SMALL_NUMBER ? ResolvedViewportScale : 1.0f;
	const URpgInventoryDragVisualWidget* CanonicalVisual = Cast<URpgInventoryDragVisualWidget>(DefaultDragVisual);
	const FVector2D DecoratorLocalSize = CanonicalVisual
		? CanonicalVisual->GetExactVisualSize()
		: DefaultDragVisual->GetDesiredSize();
	const FVector2D DecoratorSize = DecoratorLocalSize * ViewportScale;
	if (DecoratorSize.X <= KINDA_SMALL_NUMBER || DecoratorSize.Y <= KINDA_SMALL_NUMBER)
	{
		return URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(InventoryPayload, PointerScreenPosition);
	}

	// Matches FUMGDragDropOp's final TopLeft placement; player inventory paints its own non-interpolated ghost.
	return PointerScreenPosition + DecoratorSize * Offset + DecoratorSize * 0.5f;
}

void URpgInventoryDragDropOperation::RefreshDecoratorPointerOffset()
{
	const FRpgInventoryDragAnchor& Anchor = InventoryPayload.DragAnchor;
	if (!Anchor.bValid ||
		Anchor.SourceVisualSize.X <= KINDA_SMALL_NUMBER ||
		Anchor.SourceVisualSize.Y <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	Offset = FVector2D(
		-FMath::Clamp(Anchor.SourcePointerOffset.X / Anchor.SourceVisualSize.X, 0.0f, 1.0f),
		-FMath::Clamp(Anchor.SourcePointerOffset.Y / Anchor.SourceVisualSize.Y, 0.0f, 1.0f));
}

void URpgInventoryDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Dragged_Implementation(PointerEvent);
	SynchronizeFromInteractionSession();
}

void URpgInventoryDragDropOperation::SetScreenOwnedDragVisualActive(bool bInActive)
{
	bScreenOwnedDragVisualActive = bInActive;
	if (DefaultDragVisual)
	{
		const float DesiredOpacity = bScreenOwnedDragVisualActive ? 0.0f : 1.0f;
		if (!FMath::IsNearlyEqual(DefaultDragVisual->GetRenderOpacity(), DesiredOpacity))
		{
			DefaultDragVisual->SetRenderOpacity(DesiredOpacity);
		}
	}
}

void URpgInventoryDragDropOperation::SynchronizeFromInteractionSession()
{
	if (!InteractionSession)
	{
		return;
	}

	InventoryPayload = InteractionSession->GetPayload();
	if (DefaultDragVisual)
	{
		const float DesiredOpacity = bScreenOwnedDragVisualActive || InteractionSession->GetSpatialPreviewDescriptor().bValid
			? 0.0f
			: 1.0f;
		if (!FMath::IsNearlyEqual(DefaultDragVisual->GetRenderOpacity(), DesiredOpacity))
		{
			DefaultDragVisual->SetRenderOpacity(DesiredOpacity);
		}
	}
	if (URpgInventoryDragVisualWidget* DragVisual = Cast<URpgInventoryDragVisualWidget>(DefaultDragVisual))
	{
		DragVisual->SetFootprintRotated(InteractionSession->IsTargetRotated());
		DragVisual->SetPreviewState(InteractionSession->GetPreviewState());
	}
	RefreshDecoratorPointerOffset();
}

void URpgInventoryDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	if (InteractionSession && !InteractionSession->IsRequestPending())
	{
		InteractionSession->CancelInteraction();
	}

	Super::DragCancelled_Implementation(PointerEvent);
}
