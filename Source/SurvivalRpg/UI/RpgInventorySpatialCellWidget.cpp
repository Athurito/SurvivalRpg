#include "RpgInventorySpatialCellWidget.h"

#include "RpgInventorySpatialGridWidget.h"

#include "Components/Image.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySpatialCellWidget)

URpgInventorySpatialCellWidget::URpgInventorySpatialCellWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void URpgInventorySpatialCellWidget::SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid, int32 InCellX, int32 InCellY)
{
	OwningGrid = InOwningGrid;
	CellX = InCellX;
	CellY = InCellY;
	BP_OnSpatialCellSet(CellX, CellY, AddressSlotViewModel, EntryViewModel);
	ApplyResolvedVisualState();
}

void URpgInventorySpatialCellWidget::SetCellViewModels(URpgInventoryAddressSlotViewModel* InAddressSlotViewModel, URpgInventoryEntryViewModel* InEntryViewModel)
{
	if (AddressSlotViewModel == InAddressSlotViewModel && EntryViewModel == InEntryViewModel)
	{
		return;
	}

	AddressSlotViewModel = InAddressSlotViewModel;
	EntryViewModel = InEntryViewModel;
	BP_OnSpatialCellSet(CellX, CellY, AddressSlotViewModel, EntryViewModel);
}

void URpgInventorySpatialCellWidget::SetCellVisualState(ERpgInventorySpatialCellVisualState InVisualState)
{
	BaseVisualState = InVisualState;
	ApplyResolvedVisualState();
}

FReply URpgInventorySpatialCellWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!OwningGrid)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (!OwningGrid->SelectCell(CellX, CellY, GetOwningPlayer()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
		OwningGrid->RequestContextMenuForSelectedCell(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (InMouseEvent.IsControlDown())
		{
			return OwningGrid->QuickTransferSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsAltDown())
		{
			return OwningGrid->UseOrEquipSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsShiftDown())
		{
			return OwningGrid->RequestSplitDialogForSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		bPendingLeftClickAccept = true;
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventorySpatialCellWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		return OwningGrid && OwningGrid->HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URpgInventorySpatialCellWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	ApplyResolvedVisualState();
}

void URpgInventorySpatialCellWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyResolvedVisualState();
}

bool URpgInventorySpatialCellWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// One screen-level resolver owns target selection, preview, and commit for every mouse surface.
	return false;
}

bool URpgInventorySpatialCellWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

void URpgInventorySpatialCellWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Child boundaries must not clear a grid-level candidate. The screen router or drag cancellation owns cleanup.
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void URpgInventorySpatialCellWidget::ApplyResolvedVisualState()
{
	const ERpgInventorySpatialCellVisualState NewVisualState = ResolveHoveredVisualState();
	if (bHasAppliedVisualState && CurrentVisualState == NewVisualState)
	{
		return;
	}

	bHasAppliedVisualState = true;
	CurrentVisualState = NewVisualState;
	ApplyNativeVisualStyle(CurrentVisualState);
	BP_OnSpatialCellStateChanged(CurrentVisualState);
}

void URpgInventorySpatialCellWidget::ApplyNativeVisualStyle(ERpgInventorySpatialCellVisualState NewState)
{
	if (!Image_Background)
	{
		return;
	}

	FLinearColor Tint = NeutralTint;
	switch (NewState)
	{
	case ERpgInventorySpatialCellVisualState::Hovered:
		Tint = HoveredTint;
		break;
	case ERpgInventorySpatialCellVisualState::Selected:
		Tint = SelectedTint;
		break;
	case ERpgInventorySpatialCellVisualState::ValidPreview:
		Tint = ValidPreviewTint;
		break;
	case ERpgInventorySpatialCellVisualState::InvalidPreview:
		Tint = InvalidPreviewTint;
		break;
	case ERpgInventorySpatialCellVisualState::PendingPreview:
		Tint = PendingPreviewTint;
		break;
	case ERpgInventorySpatialCellVisualState::RejectedPreview:
		Tint = RejectedPreviewTint;
		break;
	case ERpgInventorySpatialCellVisualState::Normal:
	case ERpgInventorySpatialCellVisualState::Occupied:
	case ERpgInventorySpatialCellVisualState::Covered:
	default:
		break;
	}

	Image_Background->SetBrushTintColor(FSlateColor(Tint));
}

ERpgInventorySpatialCellVisualState URpgInventorySpatialCellWidget::ResolveHoveredVisualState() const
{
	return bHovered && BaseVisualState == ERpgInventorySpatialCellVisualState::Normal
		? ERpgInventorySpatialCellVisualState::Hovered
		: BaseVisualState;
}
