#include "RpgPlayerInventoryLayoutViews.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Slate/UMGDragDropOp.h"
#include "Styling/CoreStyle.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgActionBarSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryLayoutViews)

namespace
{
	const FSlateBrush* GetInventoryWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	bool IsSameGridCell(
		const FRpgInventoryGridPlacement& Placement,
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y)
	{
		return Placement.IsValid() &&
			Placement.GetContainerHandle() == ContainerHandle &&
			Placement.X == X &&
			Placement.Y == Y;
	}

	bool IsScreenPositionInsideGeometry(const FGeometry& Geometry, FVector2D ScreenPosition)
	{
		const FVector2D LocalPosition = Geometry.AbsoluteToLocal(ScreenPosition);
		const FVector2D LocalSize = Geometry.GetLocalSize();
		return LocalSize.X > KINDA_SMALL_NUMBER &&
			LocalSize.Y > KINDA_SMALL_NUMBER &&
			LocalPosition.X >= 0.0f &&
			LocalPosition.Y >= 0.0f &&
			LocalPosition.X <= LocalSize.X &&
			LocalPosition.Y <= LocalSize.Y;
	}

	FVector2D GetGridDesiredLocalSize(const FRpgInventoryGridSize& GridSize, float CellSize, float CellPadding)
	{
		return FVector2D(
			GridSize.Width * CellSize + FMath::Max(0, GridSize.Width - 1) * CellPadding,
			GridSize.Height * CellSize + FMath::Max(0, GridSize.Height - 1) * CellPadding);
	}

	int32 SnapLocalAxisToCell(float LocalPosition, int32 CellCount, float CellSize, float CellPadding)
	{
		if (CellCount <= 0 || CellSize <= 0.0f)
		{
			return INDEX_NONE;
		}

		const float DesiredSize = CellCount * CellSize + FMath::Max(0, CellCount - 1) * CellPadding;
		if (LocalPosition < 0.0f || LocalPosition > DesiredSize)
		{
			return INDEX_NONE;
		}

		const float Stride = CellSize + CellPadding;
		if (Stride <= 0.0f)
		{
			return INDEX_NONE;
		}

		int32 Cell = FMath::Clamp(FMath::FloorToInt(LocalPosition / Stride), 0, CellCount - 1);
		const float CellStart = Cell * Stride;
		const float PaddingStart = CellStart + CellSize;
		if (CellPadding > KINDA_SMALL_NUMBER && LocalPosition > PaddingStart && Cell < CellCount - 1)
		{
			const float PaddingOffset = LocalPosition - PaddingStart;
			if (PaddingOffset >= CellPadding * 0.5f)
			{
				++Cell;
			}
		}

		return FMath::Clamp(Cell, 0, CellCount - 1);
	}

	bool PlacementFootprintContainsCellUnchecked(const FRpgInventoryGridPlacement& Placement, int32 CellX, int32 CellY)
	{
		if (Placement.ContainerId.IsNone() || Placement.Width <= 0 || Placement.Height <= 0)
		{
			return false;
		}

		const FRpgInventoryGridSize OccupiedSize = Placement.GetOccupiedSize();
		return CellX >= Placement.X &&
			CellY >= Placement.Y &&
			CellX < Placement.X + OccupiedSize.Width &&
			CellY < Placement.Y + OccupiedSize.Height;
	}

	FRpgInventoryGridPlacement MakeCellPlacement(FName ContainerId, int32 X, int32 Y, bool bRotated, int32 Width = 1, int32 Height = 1)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.ContainerId = ContainerId;
		Placement.X = X;
		Placement.Y = Y;
		Placement.Width = FMath::Max(1, Width);
		Placement.Height = FMath::Max(1, Height);
		Placement.bRotated = bRotated;
		return Placement;
	}

	FRpgInventoryGridPlacement MakeCellPlacement(
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y,
		bool bRotated,
		int32 Width = 1,
		int32 Height = 1)
	{
		FRpgInventoryGridPlacement Placement = MakeCellPlacement(ContainerHandle.ContainerId, X, Y, bRotated, Width, Height);
		Placement.SetContainerHandle(ContainerHandle);
		return Placement;
	}

	FRpgInventoryGridSize GetPayloadOccupiedSize(const FRpgInventoryDragPayload& Payload, bool bTargetRotated)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.Width = Payload.ItemFootprint.IsValid()
			? Payload.ItemFootprint.Width
			: (Payload.SourcePlacement.IsValid() ? Payload.SourcePlacement.Width : 1);
		Placement.Height = Payload.ItemFootprint.IsValid()
			? Payload.ItemFootprint.Height
			: (Payload.SourcePlacement.IsValid() ? Payload.SourcePlacement.Height : 1);
		Placement.Width = FMath::Max(1, Placement.Width);
		Placement.Height = FMath::Max(1, Placement.Height);
		Placement.bRotated = bTargetRotated;
		return Placement.GetOccupiedSize();
	}

	FRpgInventoryGridSize GetPayloadUnrotatedFootprint(const FRpgInventoryDragPayload& Payload)
	{
		FRpgInventoryGridSize Footprint;
		Footprint.Width = Payload.ItemFootprint.IsValid()
			? Payload.ItemFootprint.Width
			: (Payload.SourcePlacement.IsValid() ? Payload.SourcePlacement.Width : 1);
		Footprint.Height = Payload.ItemFootprint.IsValid()
			? Payload.ItemFootprint.Height
			: (Payload.SourcePlacement.IsValid() ? Payload.SourcePlacement.Height : 1);
		Footprint.Width = FMath::Max(1, Footprint.Width);
		Footprint.Height = FMath::Max(1, Footprint.Height);
		return Footprint;
	}

	FIntPoint ClampSpatialGrabOffset(const FRpgInventoryDragPayload& Payload, bool bTargetRotated)
	{
		const FRpgInventoryGridSize OccupiedSize = GetPayloadOccupiedSize(Payload, bTargetRotated);
		const int32 MaxOffsetX = FMath::Max(0, OccupiedSize.Width - 1);
		const int32 MaxOffsetY = FMath::Max(0, OccupiedSize.Height - 1);
		return Payload.bHasSpatialGrabOffset
			? FIntPoint(FMath::Clamp(Payload.GrabCellOffsetX, 0, MaxOffsetX), FMath::Clamp(Payload.GrabCellOffsetY, 0, MaxOffsetY))
			: FIntPoint::ZeroValue;
	}

	void SetSpatialGrabOffset(FRpgInventoryDragPayload& Payload, int32 OffsetX, int32 OffsetY)
	{
		Payload.bHasSpatialGrabOffset = true;
		Payload.GrabCellOffsetX = FMath::Clamp(OffsetX, 0, FMath::Max(0, GetPayloadOccupiedSize(Payload, Payload.SourcePlacement.bRotated).Width - 1));
		Payload.GrabCellOffsetY = FMath::Clamp(OffsetY, 0, FMath::Max(0, GetPayloadOccupiedSize(Payload, Payload.SourcePlacement.bRotated).Height - 1));
	}

	void ApplySpatialGrabOffsetFromCell(FRpgInventoryDragPayload& Payload, int32 CellX, int32 CellY)
	{
		if (!Payload.SourcePlacement.IsValid() || !Payload.SourcePlacement.ContainsCell(CellX, CellY))
		{
			SetSpatialGrabOffset(Payload, 0, 0);
			return;
		}

		SetSpatialGrabOffset(Payload, CellX - Payload.SourcePlacement.X, CellY - Payload.SourcePlacement.Y);
	}

	void ApplySpatialGrabOffsetFromLocalPosition(FRpgInventoryDragPayload& Payload, FVector2D LocalPosition, FVector2D LocalSize)
	{
		const FRpgInventoryGridSize OccupiedSize = GetPayloadOccupiedSize(Payload, Payload.SourcePlacement.bRotated);
		if (OccupiedSize.Width <= 0 || OccupiedSize.Height <= 0 || LocalSize.X <= KINDA_SMALL_NUMBER || LocalSize.Y <= KINDA_SMALL_NUMBER)
		{
			SetSpatialGrabOffset(Payload, 0, 0);
			return;
		}

		const float CellWidth = LocalSize.X / static_cast<float>(OccupiedSize.Width);
		const float CellHeight = LocalSize.Y / static_cast<float>(OccupiedSize.Height);
		const int32 OffsetX = CellWidth > KINDA_SMALL_NUMBER ? FMath::FloorToInt(LocalPosition.X / CellWidth) : 0;
		const int32 OffsetY = CellHeight > KINDA_SMALL_NUMBER ? FMath::FloorToInt(LocalPosition.Y / CellHeight) : 0;
		SetSpatialGrabOffset(Payload, OffsetX, OffsetY);
	}

	void ApplyPointerGrabOffsetFromLocalPosition(FRpgInventoryDragPayload& Payload, FVector2D LocalPosition, FVector2D LocalSize)
	{
		Payload.bHasPointerGrabOffset = true;
		Payload.PointerGrabOffset.X = FMath::Clamp(LocalPosition.X, 0.0f, FMath::Max(0.0f, LocalSize.X));
		Payload.PointerGrabOffset.Y = FMath::Clamp(LocalPosition.Y, 0.0f, FMath::Max(0.0f, LocalSize.Y));
		Payload.DragVisualSize = LocalSize;
	}

}

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
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!OwningGrid || !InventoryOperation)
	{
		return false;
	}

	OwningGrid->SelectCell(CellX, CellY, GetOwningPlayer());
	OwningGrid->PreviewPayloadOnCell(InventoryOperation->InventoryPayload, CellX, CellY);
	return true;
}

bool URpgInventorySpatialCellWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!OwningGrid || !InventoryOperation)
	{
		return false;
	}

	OwningGrid->SelectCell(CellX, CellY, GetOwningPlayer());
	return OwningGrid->CommitPayloadToCell(InventoryOperation->InventoryPayload, CellX, CellY);
}

void URpgInventorySpatialCellWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (OwningGrid)
	{
		OwningGrid->ClearExternalPreviewPayload();
	}

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
	BP_OnSpatialCellStateChanged(CurrentVisualState);
}

ERpgInventorySpatialCellVisualState URpgInventorySpatialCellWidget::ResolveHoveredVisualState() const
{
	return bHovered && BaseVisualState == ERpgInventorySpatialCellVisualState::Normal
		? ERpgInventorySpatialCellVisualState::Hovered
		: BaseVisualState;
}

URpgActionBarTileView::URpgActionBarTileView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDragDrop = true;
	DragDropOperationClass = URpgInventoryDragDropOperation::StaticClass();
}

void URpgActionBarTileView::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyCoordinatorToEntry(EntryWidget);
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgActionBarTileView::SetActionBarSlotItems(const TArray<URpgActionBarSlotViewModel*>& InSlots)
{
	const TArray<UObject*>& CurrentItems = GetListItems();
	if (CurrentItems.Num() == InSlots.Num())
	{
		bool bSameItems = true;
		for (int32 Index = 0; Index < InSlots.Num(); ++Index)
		{
			if (CurrentItems[Index] != InSlots[Index])
			{
				bSameItems = false;
				break;
			}
		}

		if (bSameItems)
		{
			RequestRefresh();
			return;
		}
	}

	TArray<UObject*> NewItems;
	NewItems.Reserve(InSlots.Num());
	for (URpgActionBarSlotViewModel* SlotViewModel : InSlots)
	{
		NewItems.Add(SlotViewModel);
	}

	SetListItems(NewItems);
	RequestRefresh();
}

URpgActionBarSlotViewModel* URpgActionBarTileView::GetSelectedActionBarSlot() const
{
	return Cast<URpgActionBarSlotViewModel>(GetSelectedItem());
}

bool URpgActionBarTileView::SelectActionBarListItem(UObject* Item, APlayerController* OwningPlayer)
{
	if (!Item || !GetListItems().Contains(Item))
	{
		return false;
	}

	SetSelectedItem(Item);
	RequestNavigateToItem(Item);
	if (OwningPlayer)
	{
		SetUserFocus(OwningPlayer);
	}
	return true;
}

bool URpgActionBarTileView::SelectBestActionBarSlot(APlayerController* OwningPlayer)
{
	const TArray<UObject*>& Items = GetListItems();
	if (Items.IsEmpty())
	{
		return false;
	}

	UObject* DesiredItem = GetSelectedItem();
	if (!DesiredItem || !Items.Contains(DesiredItem))
	{
		DesiredItem = Items[0];
	}

	return SelectActionBarListItem(DesiredItem, OwningPlayer);
}

bool URpgActionBarTileView::SelectActionBarSlotByIndex(int32 SlotIndex, APlayerController* OwningPlayer)
{
	for (UObject* Item : GetListItems())
	{
		const URpgActionBarSlotViewModel* ActionBarSlot = Cast<URpgActionBarSlotViewModel>(Item);
		if (ActionBarSlot && ActionBarSlot->GetSlotIndex() == SlotIndex)
		{
			return SelectActionBarListItem(Item, OwningPlayer);
		}
	}

	return false;
}

void URpgActionBarTileView::ClearActionBarSelectionVisual()
{
	const bool bWasSuppressingPanelSelectionNotify = bSuppressPanelSelectionNotify;
	bSuppressPanelSelectionNotify = true;
	ITypedUMGListView<UObject*>::ClearSelection();
	bSuppressPanelSelectionNotify = bWasSuppressingPanelSelectionNotify;
}

bool URpgActionBarTileView::PreviewPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition)
{
	ClearExternalPreviewPayloads();
	URpgActionBarSlotWidget* SlotWidget = FindActionBarSlotWidgetAtScreenPosition(ScreenPosition);
	if (!SlotWidget)
	{
		return false;
	}

	SlotWidget->PreviewPayloadDrop(Payload);
	return true;
}

bool URpgActionBarTileView::CommitPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition)
{
	ClearExternalPreviewPayloads();
	URpgActionBarSlotWidget* SlotWidget = FindActionBarSlotWidgetAtScreenPosition(ScreenPosition);
	if (!SlotWidget)
	{
		return false;
	}

	return SlotWidget->CommitPayloadDrop(Payload);
}

void URpgActionBarTileView::ClearExternalPreviewPayloads()
{
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		if (URpgActionBarSlotWidget* SlotWidget = Cast<URpgActionBarSlotWidget>(EntryWidget))
		{
			SlotWidget->ClearExternalPreviewPayload();
		}
	}
}

void URpgActionBarTileView::SetActionBarPanelActive(bool bInActionBarPanelActive)
{
	if (bActionBarPanelActive == bInActionBarPanelActive)
	{
		return;
	}

	bActionBarPanelActive = bInActionBarPanelActive;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgActionBarTileView::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

void URpgActionBarTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
	ApplyPanelActiveStateToEntry(EntryWidget);
}

TOptional<EItemDropZone> URpgActionBarTileView::HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	if (!DragDropCoordinator)
	{
		return NullOpt;
	}

	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (!InventoryOperation)
	{
		return NullOpt;
	}

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakeActionBarSlotTargetFromViewModel(
		Cast<URpgActionBarSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	DragDropCoordinator->PreviewPayloadDrop(InventoryOperation->InventoryPayload, Target);
	if (URpgActionBarSlotWidget* SlotWidget = Cast<URpgActionBarSlotWidget>(&EntryWidget))
	{
		SlotWidget->PreviewPayloadDrop(InventoryOperation->InventoryPayload);
	}
	return TOptional<EItemDropZone>(DropZone);
}

FReply URpgActionBarTileView::HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return FReply::Unhandled();
	}

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakeActionBarSlotTargetFromViewModel(
		Cast<URpgActionBarSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	if (!DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, Target))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled().EndDragDrop();
}

void URpgActionBarTileView::OnSelectionChangedInternal(UObject* FirstSelectedItem)
{
	Super::OnSelectionChangedInternal(FirstSelectedItem);

	if (bSuppressPanelSelectionNotify || !PanelNavigationCoordinator || !FirstSelectedItem)
	{
		return;
	}

	PanelNavigationCoordinator->NotifyActionBarPanelSelectionChanged(this, FirstSelectedItem);
}

void URpgActionBarTileView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgActionBarSlotWidget* ActionBarSlotWidget = Cast<URpgActionBarSlotWidget>(EntryWidget))
	{
		ActionBarSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgActionBarTileView::ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const
{
	if (URpgActionBarSlotWidget* ActionBarSlotWidget = Cast<URpgActionBarSlotWidget>(EntryWidget))
	{
		ActionBarSlotWidget->SetActionBarPanelActive(bActionBarPanelActive);
	}
}

URpgActionBarSlotWidget* URpgActionBarTileView::FindActionBarSlotWidgetAtScreenPosition(FVector2D ScreenPosition) const
{
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		URpgActionBarSlotWidget* SlotWidget = Cast<URpgActionBarSlotWidget>(EntryWidget);
		if (SlotWidget && IsScreenPositionInsideGeometry(SlotWidget->GetCachedGeometry(), ScreenPosition))
		{
			return SlotWidget;
		}
	}

	return nullptr;
}

URpgInventorySpatialItemWidget::URpgInventorySpatialItemWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URpgInventorySpatialItemWidget::SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid)
{
	OwningGrid = InOwningGrid;
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	DragDropCoordinator = InCoordinator;
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetAddressSlotViewModel(URpgInventoryAddressSlotViewModel* InSlotViewModel)
{
	if (AddressSlotViewModel)
	{
		AddressSlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleAddressSlotChanged);
	}
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryChanged);
	}

	AddressSlotViewModel = InSlotViewModel;
	EntryViewModel = nullptr;
	if (AddressSlotViewModel)
	{
		AddressSlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleAddressSlotChanged);
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(AddressSlotViewModel);
		}
	}

	BP_OnSpatialAddressItemSet(AddressSlotViewModel);
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetEntryViewModel(URpgInventoryEntryViewModel* InEntryViewModel)
{
	if (AddressSlotViewModel)
	{
		AddressSlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleAddressSlotChanged);
	}
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryChanged);
	}

	AddressSlotViewModel = nullptr;
	EntryViewModel = InEntryViewModel;
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.AddUniqueDynamic(this, &ThisClass::HandleEntryChanged);
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(EntryViewModel);
		}
	}

	BP_OnSpatialEntryItemSet(EntryViewModel);
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::RefreshDragDropVisualState()
{
	const bool bIsFocused = bInventoryPanelActive && IsFocusedItem();
	if (DragDropCoordinator && AddressSlotViewModel)
	{
		CurrentDragDropVisualState = DragDropCoordinator->GetInventoryAddressSlotVisualState(AddressSlotViewModel, bIsFocused);
	}
	else if (DragDropCoordinator && EntryViewModel)
	{
		CurrentDragDropVisualState = DragDropCoordinator->GetInventoryEntryVisualState(EntryViewModel, bIsFocused);
	}
	else
	{
		CurrentDragDropVisualState = bIsFocused ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal;
	}

	BP_OnSpatialItemDragDropStateChanged(CurrentDragDropVisualState);
}

int32 URpgInventorySpatialItemWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 PaintedLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bUseNativeFallbackPaint)
	{
		return PaintedLayer;
	}

	int32 NextLayer = PaintedLayer + 1;
	if (UTexture2D* IconTexture = GetIcon().LoadSynchronous())
	{
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(IconTexture);
		IconBrush.ImageSize = AllottedGeometry.GetLocalSize();
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer++,
			AllottedGeometry.ToPaintGeometry(),
			&IconBrush,
			ESlateDrawEffect::None,
			InWidgetStyle.GetColorAndOpacityTint());
	}

	const int32 StackCount = GetStackCount();
	if (StackCount > 0)
	{
		const FString StackText = FString::Printf(TEXT("%dx"), StackCount);
		const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12);
		const FVector2D TextPosition(
			FMath::Max(0.0f, AllottedGeometry.GetLocalSize().X - 30.0f),
			FMath::Max(0.0f, AllottedGeometry.GetLocalSize().Y - 18.0f));
		FSlateDrawElement::MakeText(
			OutDrawElements,
			NextLayer++,
			AllottedGeometry.ToPaintGeometry(FVector2f(30.0f, 18.0f), FSlateLayoutTransform(FVector2f(TextPosition))),
			StackText,
			FontInfo,
			ESlateDrawEffect::None,
			FLinearColor::White);
	}

	return NextLayer;
}

FReply URpgInventorySpatialItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (OwningGrid)
	{
		OwningGrid->SelectCellFromScreenPosition(InMouseEvent.GetScreenSpacePosition(), GetOwningPlayer());
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OwningGrid &&
		OwningGrid->RequestContextMenuForSelectedCell(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && URpgInventoryDragDropCoordinator::IsPayloadValid(MakeDragPayload()))
	{
		if (InMouseEvent.IsControlDown())
		{
			return OwningGrid && OwningGrid->QuickTransferSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsAltDown())
		{
			return OwningGrid && OwningGrid->UseOrEquipSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsShiftDown())
		{
			return OwningGrid && OwningGrid->RequestSplitDialogForSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		bPendingLeftClickAccept = true;
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventorySpatialItemWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		return OwningGrid && OwningGrid->HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URpgInventorySpatialItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPendingLeftClickAccept = false;

	FRpgInventoryDragPayload Payload = MakeDragPayload();
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return;
	}
	ApplySpatialGrabOffsetFromLocalPosition(
		Payload,
		InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()),
		InGeometry.GetLocalSize());
	ApplyPointerGrabOffsetFromLocalPosition(
		Payload,
		InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()),
		InGeometry.GetLocalSize());
	if (!DragDropCoordinator || !DragDropCoordinator->BeginPointerDrag(Payload))
	{
		return;
	}
	Payload = DragDropCoordinator->ResolveInteractionPayload(Payload);

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		return;
	}

	InventoryOperation->Pivot = EDragPivot::MouseDown;
	InventoryOperation->InventoryPayload = Payload;
	InventoryOperation->SetInteractionSession(DragDropCoordinator->GetInteractionSession());
	InventoryOperation->Payload = AddressSlotViewModel ? Cast<UObject>(AddressSlotViewModel.Get()) : Cast<UObject>(EntryViewModel.Get());

	TSubclassOf<UUserWidget> VisualClass = DragVisualClass;
	if (!VisualClass)
	{
		VisualClass = GetClass();
	}
	if (VisualClass)
	{
		UUserWidget* DragVisual = CreateWidget<UUserWidget>(GetWorld(), VisualClass);
		if (URpgInventorySpatialItemWidget* SpatialDragVisual = Cast<URpgInventorySpatialItemWidget>(DragVisual))
		{
			SpatialDragVisual->SetDragDropCoordinator(DragDropCoordinator);
			if (AddressSlotViewModel)
			{
				SpatialDragVisual->SetAddressSlotViewModel(AddressSlotViewModel);
			}
			else
			{
				SpatialDragVisual->SetEntryViewModel(EntryViewModel);
			}
		}
		InventoryOperation->DefaultDragVisual = DragVisual;
	}

	OutOperation = InventoryOperation;
}

bool URpgInventorySpatialItemWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!OwningGrid || !InventoryOperation)
	{
		return false;
	}

	const FVector2D ScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	if (!OwningGrid->ContainsScreenPosition(ScreenPosition))
	{
		OwningGrid->ClearExternalPreviewPayload();
		return false;
	}

	OwningGrid->PreviewPayloadAtScreenPosition(InventoryOperation->InventoryPayload, ScreenPosition);
	return true;
}

bool URpgInventorySpatialItemWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!OwningGrid || !InventoryOperation)
	{
		return false;
	}

	const FVector2D ScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	if (!OwningGrid->ContainsScreenPosition(ScreenPosition))
	{
		OwningGrid->ClearExternalPreviewPayload();
		return false;
	}

	return OwningGrid->CommitPayloadAtScreenPosition(InventoryOperation->InventoryPayload, ScreenPosition);
}

void URpgInventorySpatialItemWidget::HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == AddressSlotViewModel)
	{
		BP_OnSpatialAddressItemSet(AddressSlotViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgInventorySpatialItemWidget::HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	if (ChangedEntryViewModel == EntryViewModel)
	{
		BP_OnSpatialEntryItemSet(EntryViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgInventorySpatialItemWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

FRpgInventoryDragPayload URpgInventorySpatialItemWidget::MakeDragPayload() const
{
	if (AddressSlotViewModel)
	{
		return URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(AddressSlotViewModel);
	}

	if (EntryViewModel)
	{
		return URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromEntry(EntryViewModel);
	}

	return FRpgInventoryDragPayload();
}

TSoftObjectPtr<UTexture2D> URpgInventorySpatialItemWidget::GetIcon() const
{
	if (AddressSlotViewModel)
	{
		return AddressSlotViewModel->GetIcon();
	}

	return EntryViewModel ? EntryViewModel->GetIcon() : TSoftObjectPtr<UTexture2D>();
}

int32 URpgInventorySpatialItemWidget::GetStackCount() const
{
	if (AddressSlotViewModel)
	{
		return AddressSlotViewModel->GetStackCount();
	}

	return EntryViewModel ? EntryViewModel->GetStackCount() : 0;
}

FGuid URpgInventorySpatialItemWidget::GetRepresentedEntryId() const
{
	if (AddressSlotViewModel)
	{
		return AddressSlotViewModel->GetEntryId();
	}

	return EntryViewModel ? EntryViewModel->GetEntryId() : FGuid();
}

bool URpgInventorySpatialItemWidget::IsFocusedItem() const
{
	return OwningGrid && OwningGrid->IsItemWidgetFocused(this);
}

URpgInventorySpatialGridWidget::URpgInventorySpatialGridWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SpatialItemWidgetClass = URpgInventorySpatialItemWidget::StaticClass();
	SpatialCellWidgetClass = URpgInventorySpatialCellWidget::StaticClass();
}

void URpgInventorySpatialGridWidget::BindSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	if (GroupViewModel == InGroupViewModel)
	{
		UpdateGridSizeFromBinding();
		RebuildItemOverlay();
		return;
	}

	ClearObservedSlotDelegates();
	ClearObservedEntryDelegates();
	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}

	GroupViewModel = InGroupViewModel;
	PanelViewModel = nullptr;
	Inventory = nullptr;
	ContainerId = GroupViewModel ? GroupViewModel->GetGroupId() : NAME_None;
	ContainerHandle = GroupViewModel ? GroupViewModel->GetContainerHandle() : FRpgInventoryContainerHandle();
	UpdateGridSizeFromBinding();
	ObserveSlotDelegates();
	RebuildItemOverlay();
	SelectBestCell(GetOwningPlayer(), false);
}

void URpgInventorySpatialGridWidget::BindInventoryPanelViewModel(URpgInventoryPanelViewModel* InPanelViewModel, URpgInventoryManagerComponent* InInventory, FName InContainerId)
{
	URpgInventoryManagerComponent* ResolvedInventory = InInventory
		? InInventory
		: (InPanelViewModel ? InPanelViewModel->GetObservedInventory() : nullptr);
	const FName ResolvedContainerId = !InContainerId.IsNone()
		? InContainerId
		: (ResolvedInventory ? ResolvedInventory->GetDefaultContainerId() : NAME_None);
	BindInventoryContainerPanelViewModel(
		InPanelViewModel,
		ResolvedInventory,
		FRpgInventoryContainerHandle::MakeRoot(ResolvedContainerId));
}

void URpgInventorySpatialGridWidget::BindInventoryContainerPanelViewModel(
	URpgInventoryPanelViewModel* InPanelViewModel,
	URpgInventoryManagerComponent* InInventory,
	FRpgInventoryContainerHandle InContainerHandle)
{
	if (PanelViewModel == InPanelViewModel && Inventory == InInventory && ContainerHandle == InContainerHandle)
	{
		UpdateGridSizeFromBinding();
		RebuildItemOverlay();
		return;
	}

	ClearObservedSlotDelegates();
	ClearObservedEntryDelegates();
	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}

	GroupViewModel = nullptr;
	PanelViewModel = InPanelViewModel;
	Inventory = InInventory ? InInventory : (PanelViewModel ? PanelViewModel->GetObservedInventory() : nullptr);
	ContainerHandle = InContainerHandle;
	if (!ContainerHandle.IsValid() && Inventory)
	{
		ContainerHandle = FRpgInventoryContainerHandle::MakeRoot(Inventory->GetDefaultContainerId());
	}
	ContainerId = ContainerHandle.ContainerId;

	if (PanelViewModel)
	{
		PanelViewModel->BindInventoryContainer(Inventory, ContainerHandle);
		PanelViewModel->OnEntriesChanged.AddUniqueDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}

	UpdateGridSizeFromBinding();
	ObserveEntryDelegates();
	RebuildItemOverlay();
	SelectBestCell(GetOwningPlayer(), false);
}

void URpgInventorySpatialGridWidget::SetDimmedEntryIds(
	const TArray<FGuid>& InDimmedEntryIds,
	float InDimmedOpacity)
{
	DimmedEntryIds.Reset();
	for (const FGuid& EntryId : InDimmedEntryIds)
	{
		if (EntryId.IsValid())
		{
			DimmedEntryIds.Add(EntryId);
		}
	}
	DimmedEntryOpacity = FMath::Clamp(InDimmedOpacity, 0.05f, 1.0f);
	ApplyEntryDimming();
}

void URpgInventorySpatialGridWidget::ClearEntryDimming()
{
	DimmedEntryIds.Reset();
	ApplyEntryDimming();
}

bool URpgInventorySpatialGridWidget::IsEntryDimmed(FGuid EntryId) const
{
	return EntryId.IsValid() && DimmedEntryIds.Contains(EntryId);
}

void URpgInventorySpatialGridWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	DragDropCoordinator = InCoordinator;
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->SetDragDropCoordinator(DragDropCoordinator);
		}
	}
	UpdateCellVisualStates();
}

void URpgInventorySpatialGridWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

void URpgInventorySpatialGridWidget::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->SetInventoryPanelActive(bInventoryPanelActive);
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

void URpgInventorySpatialGridWidget::SetCellMetrics(float InCellSize, float InCellPadding)
{
	CellSize = FMath::Max(1.0f, InCellSize);
	CellPadding = FMath::Max(0.0f, InCellPadding);
	UpdateDesiredGridSize();
	RebuildCellLayer();
	RebuildItemOverlay();
}

bool URpgInventorySpatialGridWidget::SelectBestCell(APlayerController* OwningPlayer, bool bPreferOccupiedSlot)
{
	if (!GridSize.IsValid())
	{
		return false;
	}

	if (IsValidCell(CursorX, CursorY))
	{
		return SelectCell(CursorX, CursorY, OwningPlayer);
	}

	if (bPreferOccupiedSlot)
	{
		if (GroupViewModel)
		{
			for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
			{
				if (SlotViewModel && SlotViewModel->ShouldRenderItemVisual())
				{
					const FRpgInventoryGridPlacement Placement = SlotViewModel->GetItemPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
		else if (PanelViewModel)
		{
			for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
			{
				if (EntryViewModel && !EntryViewModel->IsEmptySlot() &&
					EntryViewModel->GetPlacement().GetContainerHandle() == ResolveContainerHandle())
				{
					const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
	}

	return SelectCell(0, 0, OwningPlayer);
}

bool URpgInventorySpatialGridWidget::SelectCellByIdentity(FGuid EntryId, int32 SlotIndex, APlayerController* OwningPlayer)
{
	if (EntryId.IsValid())
	{
		if (GroupViewModel)
		{
			for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
			{
				if (SlotViewModel && !SlotViewModel->IsEmptySlot() && SlotViewModel->GetEntryId() == EntryId)
				{
					const FRpgInventoryGridPlacement Placement = SlotViewModel->GetItemPlacement().IsValid()
						? SlotViewModel->GetItemPlacement()
						: SlotViewModel->GetPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
		else if (PanelViewModel)
		{
			for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
			{
				if (EntryViewModel && !EntryViewModel->IsEmptySlot() && EntryViewModel->GetEntryId() == EntryId)
				{
					const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
	}

	if (SlotIndex != INDEX_NONE)
	{
		const int32 X = SlotIndex % 1000;
		const int32 Y = SlotIndex / 1000;
		return SelectCell(X, Y, OwningPlayer);
	}

	return false;
}

bool URpgInventorySpatialGridWidget::SelectCell(int32 X, int32 Y, APlayerController* OwningPlayer)
{
	if (!IsValidCell(X, Y))
	{
		return false;
	}

	CursorX = X;
	CursorY = Y;
	if (OwningPlayer)
	{
		SetUserFocus(OwningPlayer);
	}

	NotifySelectionChanged();
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->RefreshDragDropVisualState();
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
	return true;
}

void URpgInventorySpatialGridWidget::ClearSelectionVisual()
{
	bSelectionVisualSuppressed = true;
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->RefreshDragDropVisualState();
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

bool URpgInventorySpatialGridWidget::HandleAcceptSelectedCell()
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (DragDropCoordinator->HasHeldPayload())
	{
		return DragDropCoordinator->CommitDrop(MakeDropTargetAtCursor());
	}

	FRpgInventoryDragPayload Payload = MakePayloadFromSelectedItem();
	ApplySpatialGrabOffsetFromCell(Payload, CursorX, CursorY);
	return URpgInventoryDragDropCoordinator::IsPayloadValid(Payload) && DragDropCoordinator->BeginHold(Payload);
}

bool URpgInventorySpatialGridWidget::QuickTransferSelectedCell(URpgInventoryManagerComponent* ExplicitTargetInventory)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->QuickTransferAddressSlot(AddressSlot, ExplicitTargetInventory);
	}

	return DragDropCoordinator->QuickTransferEntry(GetSelectedEntryViewModel(), ExplicitTargetInventory);
}

bool URpgInventorySpatialGridWidget::QuickSplitSelectedCell(int32 SplitCount)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (DragDropCoordinator->HasHeldPayload())
	{
		return ToggleHeldItemRotation();
	}
	if (SplitCount <= 0)
	{
		return RequestSplitDialogForSelectedCell();
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->QuickSplitAddressSlot(AddressSlot, FRpgInventoryGridPlacement(), SplitCount);
	}

	return DragDropCoordinator->QuickSplitEntry(GetSelectedEntryViewModel(), FRpgInventoryGridPlacement(), SplitCount);
}

bool URpgInventorySpatialGridWidget::RequestSplitDialogForSelectedCell()
{
	CancelPendingSplit();
	URpgInventoryItemInstance* Item = GetSelectedItemInstance();
	const int32 StackCount = GetSelectedItemStackCount();
	if (!Item || StackCount <= 1)
	{
		return false;
	}

	PendingSplitAddressSlot = GetSelectedAddressSlot();
	PendingSplitEntry = PendingSplitAddressSlot ? nullptr : GetSelectedEntryViewModel();
	if (!PendingSplitAddressSlot && !PendingSplitEntry)
	{
		return false;
	}

	PendingSplitMaximum = StackCount - 1;
	BP_OnInventorySplitDialogRequested(Item, 1, PendingSplitMaximum, FMath::Clamp(StackCount / 2, 1, PendingSplitMaximum));
	return true;
}

bool URpgInventorySpatialGridWidget::ConfirmPendingSplit(int32 SplitCount)
{
	if (!DragDropCoordinator || SplitCount < 1 || SplitCount > PendingSplitMaximum)
	{
		return false;
	}

	bool bDispatched = false;
	if (PendingSplitAddressSlot && PendingSplitAddressSlot->GetItemInstance() &&
		SplitCount < PendingSplitAddressSlot->GetStackCount())
	{
		bDispatched = DragDropCoordinator->QuickSplitAddressSlot(
			PendingSplitAddressSlot,
			FRpgInventoryGridPlacement(),
			SplitCount);
	}
	else if (PendingSplitEntry && PendingSplitEntry->GetItemInstance() &&
		SplitCount < PendingSplitEntry->GetStackCount())
	{
		bDispatched = DragDropCoordinator->QuickSplitEntry(
			PendingSplitEntry,
			FRpgInventoryGridPlacement(),
			SplitCount);
	}

	if (bDispatched)
	{
		CancelPendingSplit();
	}
	return bDispatched;
}

void URpgInventorySpatialGridWidget::CancelPendingSplit()
{
	PendingSplitAddressSlot = nullptr;
	PendingSplitEntry = nullptr;
	PendingSplitMaximum = 0;
}

bool URpgInventorySpatialGridWidget::RequestContextMenuForSelectedCell(FVector2D ScreenPosition)
{
	URpgInventoryItemInstance* Item = GetSelectedItemInstance();
	const TArray<ERpgInventoryContextAction> Actions = GetSelectedContextActions();
	if (!Item || Actions.IsEmpty())
	{
		return false;
	}

	BP_OnInventoryContextMenuRequested(Item, Actions, ScreenPosition);
	return true;
}

TArray<ERpgInventoryContextAction> URpgInventorySpatialGridWidget::GetSelectedContextActions() const
{
	TArray<ERpgInventoryContextAction> Actions;
	URpgInventoryItemInstance* Item = GetSelectedItemInstance();
	if (!Item)
	{
		return Actions;
	}

	if (Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
	{
		Actions.Add(ERpgInventoryContextAction::OpenContainer);
	}
	Actions.Add(ERpgInventoryContextAction::Inspect);
	if (Item->FindFragmentByClass<URpgInventoryFragment_UsableItem>())
	{
		Actions.Add(ERpgInventoryContextAction::Use);
	}
	if (Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>())
	{
		Actions.Add(ERpgInventoryContextAction::EquipAndActivate);
		Actions.Add(ERpgInventoryContextAction::MoveToCarry);
	}
	if (GetSelectedItemStackCount() > 1)
	{
		Actions.Add(ERpgInventoryContextAction::Split);
	}
	if (const URpgInventoryFragment_SpatialItem* Spatial = Item->FindFragmentByClass<URpgInventoryFragment_SpatialItem>();
		Spatial && Spatial->bAllowRotation)
	{
		Actions.Add(ERpgInventoryContextAction::Rotate);
	}
	if (const URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot();
		AddressSlot && AddressSlot->IsActionbarBindable())
	{
		Actions.Add(ERpgInventoryContextAction::QuickAccessBind);
		Actions.Add(ERpgInventoryContextAction::QuickAccessUnbind);
	}
	Actions.Add(ERpgInventoryContextAction::Transfer);
	Actions.Add(ERpgInventoryContextAction::Drop);
	return Actions;
}

bool URpgInventorySpatialGridWidget::ExecuteSelectedContextAction(
	ERpgInventoryContextAction Action,
	int32 SplitCount,
	int32 QuickAccessSlotIndex)
{
	URpgInventoryItemInstance* Item = GetSelectedItemInstance();
	if (!Item)
	{
		return false;
	}

	switch (Action)
	{
	case ERpgInventoryContextAction::Use:
	case ERpgInventoryContextAction::EquipAndActivate:
	case ERpgInventoryContextAction::MoveToCarry:
		return UseOrEquipSelectedCell();

	case ERpgInventoryContextAction::Split:
		return SplitCount > 0 ? (RequestSplitDialogForSelectedCell() && ConfirmPendingSplit(SplitCount)) : RequestSplitDialogForSelectedCell();

	case ERpgInventoryContextAction::Rotate:
		if (!DragDropCoordinator)
		{
			return false;
		}
		if (!DragDropCoordinator->HasHeldPayload() && !HandleAcceptSelectedCell())
		{
			return false;
		}
		return ToggleHeldItemRotation() && DragDropCoordinator->CommitDrop(MakeDropTargetAtCursor());

	case ERpgInventoryContextAction::Transfer:
		return QuickTransferSelectedCell();

	case ERpgInventoryContextAction::Drop:
		return DropSelectedCell();

	case ERpgInventoryContextAction::OpenContainer:
	case ERpgInventoryContextAction::Inspect:
	case ERpgInventoryContextAction::QuickAccessBind:
	case ERpgInventoryContextAction::QuickAccessUnbind:
		BP_OnDeferredInventoryContextAction(Action, Item, QuickAccessSlotIndex);
		return true;

	default:
		return false;
	}
}

bool URpgInventorySpatialGridWidget::UseOrEquipSelectedCell(int32 StackCount)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->UseOrEquipAddressSlot(AddressSlot, StackCount);
	}

	return DragDropCoordinator->UseOrEquipEntry(GetSelectedEntryViewModel(), StackCount);
}

bool URpgInventorySpatialGridWidget::DropSelectedCell(int32 StackCount, bool bConfirmed)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->DropAddressSlot(AddressSlot, StackCount, bConfirmed);
	}

	return DragDropCoordinator->DropEntry(GetSelectedEntryViewModel(), StackCount, bConfirmed);
}

bool URpgInventorySpatialGridWidget::ToggleHeldItemRotation()
{
	if (!DragDropCoordinator || !DragDropCoordinator->HasHeldPayload())
	{
		return false;
	}

	if (!DragDropCoordinator->ToggleInteractionRotation())
	{
		return false;
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
	return true;
}

URpgInventoryAddressSlotViewModel* URpgInventorySpatialGridWidget::GetSelectedAddressSlot() const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	if (URpgInventoryAddressSlotViewModel* OccupyingItem = FindAddressItemAtCell(CursorX, CursorY))
	{
		return OccupyingItem;
	}

	return FindAddressCell(CursorX, CursorY);
}

URpgInventoryEntryViewModel* URpgInventorySpatialGridWidget::GetSelectedEntryViewModel() const
{
	return PanelViewModel ? FindEntryAtCell(CursorX, CursorY) : nullptr;
}

URpgInventoryItemInstance* URpgInventorySpatialGridWidget::GetSelectedItemInstance() const
{
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return AddressSlot->GetItemInstance();
	}
	if (URpgInventoryEntryViewModel* Entry = GetSelectedEntryViewModel())
	{
		return Entry->GetItemInstance();
	}
	return nullptr;
}

int32 URpgInventorySpatialGridWidget::GetSelectedItemStackCount() const
{
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return AddressSlot->GetStackCount();
	}
	if (URpgInventoryEntryViewModel* Entry = GetSelectedEntryViewModel())
	{
		return Entry->GetStackCount();
	}
	return 0;
}

FGuid URpgInventorySpatialGridWidget::GetSelectedEntryId() const
{
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return !AddressSlot->IsEmptySlot() ? AddressSlot->GetEntryId() : FGuid();
	}

	if (URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel())
	{
		return !EntryViewModel->IsEmptySlot() ? EntryViewModel->GetEntryId() : FGuid();
	}

	return FGuid();
}

int32 URpgInventorySpatialGridWidget::GetSelectedSlotIndex() const
{
	return IsValidCell(CursorX, CursorY) ? CursorY * 1000 + CursorX : INDEX_NONE;
}

bool URpgInventorySpatialGridWidget::SelectCellFromScreenPosition(FVector2D ScreenPosition, APlayerController* OwningPlayer)
{
	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	if (!TryGetCellFromScreenPosition(ScreenPosition, CellX, CellY))
	{
		return false;
	}

	return SelectCell(CellX, CellY, OwningPlayer);
}

bool URpgInventorySpatialGridWidget::CommitPayloadToCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y)
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	bHasExternalPreviewPayload = false;
	ExternalPreviewPayload = FRpgInventoryDragPayload();
	bHasExternalPreviewTargetPlacement = false;
	ExternalPreviewTargetPlacement = FRpgInventoryGridPlacement();
	UpdateCellVisualStates();

	return DragDropCoordinator &&
		IsValidCell(X, Y) &&
		DragDropCoordinator->CommitPayloadToTarget(ResolvedPayload, MakeDropTargetForCell(ResolvedPayload, X, Y));
}

bool URpgInventorySpatialGridWidget::PreviewPayloadOnCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y)
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	if (!IsValidCell(X, Y))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	bHasExternalPreviewPayload = true;
	ExternalPreviewPayload = ResolvedPayload;
	bHasExternalPreviewTargetPlacement = true;
	ExternalPreviewTargetPlacement = MakeTargetPlacementForCell(ResolvedPayload, X, Y);
	UpdateCellVisualStates();

	return DragDropCoordinator &&
		DragDropCoordinator->UpdateInteractionPreview(ResolvedPayload, MakeDropTargetForCell(ResolvedPayload, X, Y));
}

bool URpgInventorySpatialGridWidget::CommitPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition)
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	FRpgInventoryDropTarget Target;
	FRpgInventoryGridPlacement TargetPlacement;
	int32 AnchorX = INDEX_NONE;
	int32 AnchorY = INDEX_NONE;
	if (!ResolveDropTargetAtScreenPosition(ResolvedPayload, ScreenPosition, Target, TargetPlacement, AnchorX, AnchorY))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	bHasExternalPreviewPayload = false;
	ExternalPreviewPayload = FRpgInventoryDragPayload();
	bHasExternalPreviewTargetPlacement = false;
	ExternalPreviewTargetPlacement = FRpgInventoryGridPlacement();
	UpdateCellVisualStates();

	SelectCell(AnchorX, AnchorY, GetOwningPlayer());
	return DragDropCoordinator && DragDropCoordinator->CommitPayloadToTarget(ResolvedPayload, Target);
}

bool URpgInventorySpatialGridWidget::PreviewPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition)
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	FRpgInventoryDropTarget Target;
	FRpgInventoryGridPlacement TargetPlacement;
	int32 AnchorX = INDEX_NONE;
	int32 AnchorY = INDEX_NONE;
	if (!ResolveDropTargetAtScreenPosition(ResolvedPayload, ScreenPosition, Target, TargetPlacement, AnchorX, AnchorY))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	bHasExternalPreviewPayload = true;
	ExternalPreviewPayload = ResolvedPayload;
	bHasExternalPreviewTargetPlacement = true;
	ExternalPreviewTargetPlacement = TargetPlacement;

	SelectCell(AnchorX, AnchorY, GetOwningPlayer());
	UpdateCellVisualStates();
	return DragDropCoordinator && DragDropCoordinator->UpdateInteractionPreview(ResolvedPayload, Target);
}

bool URpgInventorySpatialGridWidget::ContainsScreenPosition(FVector2D ScreenPosition) const
{
	const FGeometry GridGeometry = GetGridInteractionGeometry();
	const FVector2D LocalPosition = GridGeometry.AbsoluteToLocal(ScreenPosition);
	const FVector2D LocalGridSize = GridSize.IsValid()
		? GetGridDesiredLocalSize(GridSize, CellSize, CellPadding)
		: GridGeometry.GetLocalSize();

	return LocalGridSize.X > KINDA_SMALL_NUMBER &&
		LocalGridSize.Y > KINDA_SMALL_NUMBER &&
		LocalPosition.X >= 0.0f &&
		LocalPosition.Y >= 0.0f &&
		LocalPosition.X <= LocalGridSize.X &&
		LocalPosition.Y <= LocalGridSize.Y;
}

bool URpgInventorySpatialGridWidget::ResolveDropTargetAtScreenPosition(
	const FRpgInventoryDragPayload& Payload,
	FVector2D ScreenPosition,
	FRpgInventoryDropTarget& OutTarget,
	FRpgInventoryGridPlacement& OutTargetPlacement,
	int32& OutAnchorX,
	int32& OutAnchorY) const
{
	OutTarget = FRpgInventoryDropTarget();
	OutTargetPlacement = FRpgInventoryGridPlacement();
	OutAnchorX = INDEX_NONE;
	OutAnchorY = INDEX_NONE;

	OutTargetPlacement = ResolveTargetPlacementAtScreenPosition(Payload, ScreenPosition, OutAnchorX, OutAnchorY);
	if (!IsValidCell(OutAnchorX, OutAnchorY))
	{
		return false;
	}

	OutTarget = MakeDropTargetForPlacement(Payload, OutTargetPlacement);
	return true;
}

void URpgInventorySpatialGridWidget::ClearExternalPreviewPayload()
{
	if (!bHasExternalPreviewPayload && !bHasExternalPreviewTargetPlacement)
	{
		return;
	}

	bHasExternalPreviewPayload = false;
	ExternalPreviewPayload = FRpgInventoryDragPayload();
	bHasExternalPreviewTargetPlacement = false;
	ExternalPreviewTargetPlacement = FRpgInventoryGridPlacement();
	UpdateCellVisualStates();
}

bool URpgInventorySpatialGridWidget::IsItemWidgetFocused(const URpgInventorySpatialItemWidget* ItemWidget) const
{
	if (!ItemWidget)
	{
		return false;
	}

	if (const URpgInventoryAddressSlotViewModel* AddressSlot = ItemWidget->GetAddressSlotViewModel())
	{
		const FRpgInventoryGridPlacement Placement = AddressSlot->GetItemPlacement().IsValid()
			? AddressSlot->GetItemPlacement()
			: AddressSlot->GetPlacement();
		return Placement.ContainsCell(CursorX, CursorY);
	}

	if (const URpgInventoryEntryViewModel* EntryViewModel = ItemWidget->GetEntryViewModel())
	{
		return EntryViewModel->GetPlacement().ContainsCell(CursorX, CursorY);
	}

	return false;
}

void URpgInventorySpatialGridWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureRuntimeWidgets();
	UpdateDesiredGridSize();
}

void URpgInventorySpatialGridWidget::NativeDestruct()
{
	ClearObservedSlotDelegates();
	ClearObservedEntryDelegates();
	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	Super::NativeDestruct();
}

int32 URpgInventorySpatialGridWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 NextLayer = LayerId;
	const FSlateBrush* WhiteBrush = GetInventoryWhiteBrush();
	if (bUseNativeDebugPaint && WhiteBrush && GridSize.IsValid())
	{
		for (int32 Y = 0; Y < GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < GridSize.Width; ++X)
			{
				const FVector2D Position = GetCellPosition(X, Y);
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					NextLayer,
					AllottedGeometry.ToPaintGeometry(FVector2f(CellSize, CellSize), FSlateLayoutTransform(FVector2f(Position))),
					WhiteBrush,
					ESlateDrawEffect::None,
					CellFillColor);

				TArray<FVector2f> BorderPoints;
				BorderPoints.Add(FVector2f(Position));
				BorderPoints.Add(FVector2f(Position.X + CellSize, Position.Y));
				BorderPoints.Add(FVector2f(Position.X + CellSize, Position.Y + CellSize));
				BorderPoints.Add(FVector2f(Position.X, Position.Y + CellSize));
				BorderPoints.Add(FVector2f(Position));
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					NextLayer + 1,
					AllottedGeometry.ToPaintGeometry(),
					BorderPoints,
					ESlateDrawEffect::None,
					CellBorderColor,
					true,
					1.0f);
			}
		}

		NextLayer += 2;
		if (bInventoryPanelActive && !bSelectionVisualSuppressed && IsValidCell(CursorX, CursorY))
		{
			const FVector2D Position = GetCellPosition(CursorX, CursorY);
			TArray<FVector2f> CursorPoints;
			CursorPoints.Add(FVector2f(Position));
			CursorPoints.Add(FVector2f(Position.X + CellSize, Position.Y));
			CursorPoints.Add(FVector2f(Position.X + CellSize, Position.Y + CellSize));
			CursorPoints.Add(FVector2f(Position.X, Position.Y + CellSize));
			CursorPoints.Add(FVector2f(Position));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer++,
				AllottedGeometry.ToPaintGeometry(),
				CursorPoints,
				ESlateDrawEffect::None,
				CursorColor,
				true,
				2.0f);
		}
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NextLayer, InWidgetStyle, bParentEnabled);
}

FReply URpgInventorySpatialGridWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		return MoveCursorBy(-1, 0, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		return MoveCursorBy(1, 0, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		return MoveCursorBy(0, -1, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		return MoveCursorBy(0, 1, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		return HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::R)
	{
		return ToggleHeldItemRotation() ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Left)
	{
		return QuickTransferSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Top)
	{
		return QuickSplitSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Gamepad_RightTrigger)
	{
		return UseOrEquipSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Gamepad_LeftTrigger)
	{
		return RequestContextMenuForSelectedCell(FVector2D::ZeroVector) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Right && DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		DragDropCoordinator->CancelHold();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URpgInventorySpatialGridWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	if (!TryGetCellFromScreenPosition(InMouseEvent.GetScreenSpacePosition(), CellX, CellY))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	SelectCell(CellX, CellY, GetOwningPlayer());

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
		RequestContextMenuForSelectedCell(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (InMouseEvent.IsControlDown())
		{
			return QuickTransferSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsAltDown())
		{
			return UseOrEquipSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsShiftDown())
		{
			return RequestSplitDialogForSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		bPendingLeftClickAccept = true;
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventorySpatialGridWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		return HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

bool URpgInventorySpatialGridWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation)
	{
		return false;
	}

	const FVector2D ScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	if (!ContainsScreenPosition(ScreenPosition))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	PreviewPayloadAtScreenPosition(InventoryOperation->InventoryPayload, ScreenPosition);
	return true;
}

bool URpgInventorySpatialGridWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation)
	{
		return false;
	}

	const FVector2D ScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	if (!ContainsScreenPosition(ScreenPosition))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	return CommitPayloadAtScreenPosition(InventoryOperation->InventoryPayload, ScreenPosition);
}

void URpgInventorySpatialGridWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearExternalPreviewPayload();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

FReply URpgInventorySpatialGridWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	bSelectionVisualSuppressed = false;
	NotifySelectionChanged();
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
	return FReply::Handled();
}

void URpgInventorySpatialGridWidget::RefreshFromPanelViewModel()
{
	UpdateGridSizeFromBinding();
	ObserveEntryDelegates();
	RebuildItemOverlay();
}

void URpgInventorySpatialGridWidget::HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	RebuildItemOverlay();
}

void URpgInventorySpatialGridWidget::HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	RebuildItemOverlay();
}

void URpgInventorySpatialGridWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->RefreshDragDropVisualState();
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

void URpgInventorySpatialGridWidget::EnsureRuntimeWidgets()
{
	if (!RootSizeBox)
	{
		RootSizeBox = Cast<USizeBox>(GetRootWidget());
	}

	if (!CellCanvas && CellLayer)
	{
		CellCanvas = CellLayer;
	}

	if (ItemCanvas)
	{
		ItemCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (!RootSizeBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint SizeBox named RootSizeBox to enforce fixed spatial grid dimensions."), *GetNameSafe(this));
	}

	if (!CellCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint CanvasPanel named CellCanvas or CellLayer for designable spatial slots."), *GetNameSafe(this));
	}

	if (!ItemCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint CanvasPanel named ItemCanvas for spatial item overlays."), *GetNameSafe(this));
	}
}

void URpgInventorySpatialGridWidget::UpdateGridSizeFromBinding()
{
	if (GroupViewModel)
	{
		GridSize = GroupViewModel->GetGridSize();
		ContainerId = GroupViewModel->GetGroupId();
		ContainerHandle = GroupViewModel->GetContainerHandle();
	}
	else if (Inventory)
	{
		FRpgInventoryGridSize ResolvedGridSize;
		const FRpgInventoryContainerHandle ResolvedContainerHandle = ResolveContainerHandle();
		GridSize = Inventory->GetGridSizeForContainerHandle(ResolvedContainerHandle, ResolvedGridSize)
			? ResolvedGridSize
			: Inventory->GetDefaultGridSize();
		ContainerHandle = ResolvedContainerHandle;
		ContainerId = ResolvedContainerHandle.ContainerId;
	}
	else
	{
		GridSize = FRpgInventoryGridSize();
	}

	if (!GridSize.IsValid())
	{
		GridSize.Width = 1;
		GridSize.Height = 1;
	}

	CursorX = FMath::Clamp(CursorX, 0, GridSize.Width - 1);
	CursorY = FMath::Clamp(CursorY, 0, GridSize.Height - 1);
	UpdateDesiredGridSize();
	RebuildCellLayer();
}

void URpgInventorySpatialGridWidget::UpdateDesiredGridSize()
{
	EnsureRuntimeWidgets();

	const float DesiredWidth = GridSize.Width * CellSize + FMath::Max(0, GridSize.Width - 1) * CellPadding;
	const float DesiredHeight = GridSize.Height * CellSize + FMath::Max(0, GridSize.Height - 1) * CellPadding;
	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(DesiredWidth);
		RootSizeBox->SetHeightOverride(DesiredHeight);
	}
}

void URpgInventorySpatialGridWidget::RebuildCellLayer()
{
	EnsureRuntimeWidgets();
	UCanvasPanel* ResolvedCellCanvas = CellCanvas ? CellCanvas.Get() : CellLayer.Get();
	if (!ResolvedCellCanvas || !SpatialCellWidgetClass || !GridSize.IsValid())
	{
		CellWidgets.Reset();
		return;
	}

	TMap<FIntPoint, TObjectPtr<URpgInventorySpatialCellWidget>> ExistingCells;
	for (URpgInventorySpatialCellWidget* CellWidget : CellWidgets)
	{
		if (CellWidget)
		{
			ExistingCells.Add(FIntPoint(CellWidget->GetCellX(), CellWidget->GetCellY()), CellWidget);
		}
	}

	TArray<TObjectPtr<URpgInventorySpatialCellWidget>> ReconciledCells;
	ReconciledCells.Reserve(GridSize.Width * GridSize.Height);
	TSet<TObjectPtr<URpgInventorySpatialCellWidget>> ReusedCells;
	for (int32 Y = 0; Y < GridSize.Height; ++Y)
	{
		for (int32 X = 0; X < GridSize.Width; ++X)
		{
			URpgInventorySpatialCellWidget* CellWidget = ExistingCells.FindRef(FIntPoint(X, Y));
			if (CellWidget && !CellWidget->IsA(SpatialCellWidgetClass))
			{
				CellWidget = nullptr;
			}
			if (!CellWidget)
			{
				CellWidget = CreateWidget<URpgInventorySpatialCellWidget>(GetWorld(), SpatialCellWidgetClass);
			}
			if (!CellWidget)
			{
				continue;
			}

			CellWidget->SetOwningSpatialGrid(this, X, Y);
			CellWidget->SetCellViewModels(FindAddressCell(X, Y), FindEntryAtCell(X, Y));
			CellWidget->SetCellVisualState(GetCellVisualState(X, Y));

			if (CellWidget->GetParent() != ResolvedCellCanvas)
			{
				ResolvedCellCanvas->AddChildToCanvas(CellWidget);
			}
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CellWidget->Slot))
			{
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetPosition(GetCellPosition(X, Y));
				CanvasSlot->SetSize(FVector2D(CellSize, CellSize));
				CanvasSlot->SetZOrder(0);
			}

			ReconciledCells.Add(CellWidget);
			ReusedCells.Add(CellWidget);
		}
	}

	for (URpgInventorySpatialCellWidget* PreviousCell : CellWidgets)
	{
		if (PreviousCell && !ReusedCells.Contains(PreviousCell))
		{
			ResolvedCellCanvas->RemoveChild(PreviousCell);
		}
	}
	CellWidgets = MoveTemp(ReconciledCells);
}

void URpgInventorySpatialGridWidget::UpdateCellVisualStates()
{
	for (URpgInventorySpatialCellWidget* CellWidget : CellWidgets)
	{
		if (!CellWidget)
		{
			continue;
		}

		const int32 X = CellWidget->GetCellX();
		const int32 Y = CellWidget->GetCellY();
		CellWidget->SetCellViewModels(FindAddressCell(X, Y), FindEntryAtCell(X, Y));
		CellWidget->SetCellVisualState(GetCellVisualState(X, Y));
	}
}

ERpgInventorySpatialCellVisualState URpgInventorySpatialGridWidget::GetCellVisualState(int32 X, int32 Y) const
{
	if (!IsValidCell(X, Y))
	{
		return ERpgInventorySpatialCellVisualState::Normal;
	}

	const bool bIsSelectedCell = X == CursorX && Y == CursorY;
	const bool bCanShowCursor = bInventoryPanelActive && !bSelectionVisualSuppressed && bIsSelectedCell;
	if (bInventoryPanelActive && !bSelectionVisualSuppressed)
	{
		if (bHasExternalPreviewPayload)
		{
			ERpgInventorySpatialCellVisualState PreviewState = ERpgInventorySpatialCellVisualState::Normal;
			if (ResolvePayloadPreviewCellState(ExternalPreviewPayload, X, Y, PreviewState))
			{
				return PreviewState;
			}
		}

		if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
		{
			ERpgInventorySpatialCellVisualState PreviewState = ERpgInventorySpatialCellVisualState::Normal;
			if (ResolvePayloadPreviewCellState(DragDropCoordinator->GetHeldPayload(), X, Y, PreviewState))
			{
				return PreviewState;
			}
		}
	}

	if (bCanShowCursor)
	{
		return ERpgInventorySpatialCellVisualState::Selected;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = FindAddressItemAtCell(X, Y))
	{
		const FRpgInventoryGridPlacement Placement = AddressSlot->GetItemPlacement().IsValid()
			? AddressSlot->GetItemPlacement()
			: AddressSlot->GetPlacement();
		return Placement.X == X && Placement.Y == Y
			? ERpgInventorySpatialCellVisualState::Occupied
			: ERpgInventorySpatialCellVisualState::Covered;
	}

	if (URpgInventoryEntryViewModel* EntryViewModel = FindEntryAtCell(X, Y))
	{
		const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
		return Placement.X == X && Placement.Y == Y
			? ERpgInventorySpatialCellVisualState::Occupied
			: ERpgInventorySpatialCellVisualState::Covered;
	}

	return ERpgInventorySpatialCellVisualState::Normal;
}

bool URpgInventorySpatialGridWidget::ResolvePayloadPreviewCellState(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y, ERpgInventorySpatialCellVisualState& OutState) const
{
	OutState = ERpgInventorySpatialCellVisualState::Normal;
	if (!DragDropCoordinator || !URpgInventoryDragDropCoordinator::IsPayloadValid(Payload) || !IsValidCell(CursorX, CursorY))
	{
		return false;
	}

	const FRpgInventoryGridPlacement TargetPlacement = bHasExternalPreviewTargetPlacement
		? ExternalPreviewTargetPlacement
		: MakeTargetPlacementForCell(Payload, CursorX, CursorY);
	const bool bIsAnchorCell = X == CursorX && Y == CursorY;
	const bool bIsPreviewCell = PlacementFootprintContainsCellUnchecked(TargetPlacement, X, Y);
	if (!bIsAnchorCell && !bIsPreviewCell)
	{
		return false;
	}

	const FRpgInventoryDropTarget Target = bHasExternalPreviewTargetPlacement
		? MakeDropTargetForPlacement(Payload, ExternalPreviewTargetPlacement)
		: MakeDropTargetForCell(Payload, CursorX, CursorY);
	const bool bCanDrop = DragDropCoordinator->PreviewPayloadDrop(Payload, Target);
	OutState = bCanDrop ? ERpgInventorySpatialCellVisualState::ValidPreview : ERpgInventorySpatialCellVisualState::InvalidPreview;
	return true;
}

void URpgInventorySpatialGridWidget::RebuildItemOverlay()
{
	EnsureRuntimeWidgets();
	if (!ItemCanvas || !SpatialItemWidgetClass)
	{
		ItemWidgets.Reset();
		UpdateCellVisualStates();
		return;
	}

	ItemCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TMap<FGuid, TObjectPtr<URpgInventorySpatialItemWidget>> ExistingItems;
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget && ItemWidget->GetRepresentedEntryId().IsValid())
		{
			ExistingItems.Add(ItemWidget->GetRepresentedEntryId(), ItemWidget);
		}
	}

	TArray<TObjectPtr<URpgInventorySpatialItemWidget>> ReconciledItems;
	TSet<TObjectPtr<URpgInventorySpatialItemWidget>> ReusedItems;

	auto AddSpatialItem = [this, &ExistingItems, &ReconciledItems, &ReusedItems](
		const FRpgInventoryGridPlacement& Placement,
		URpgInventoryAddressSlotViewModel* AddressSlot,
		URpgInventoryEntryViewModel* EntryViewModel)
	{
		if (!Placement.IsValid())
		{
			return;
		}

		const FGuid EntryId = AddressSlot
			? AddressSlot->GetEntryId()
			: (EntryViewModel ? EntryViewModel->GetEntryId() : FGuid());
		URpgInventorySpatialItemWidget* ItemWidget = EntryId.IsValid() ? ExistingItems.FindRef(EntryId) : nullptr;
		if (ItemWidget && !ItemWidget->IsA(SpatialItemWidgetClass))
		{
			ItemWidget = nullptr;
		}
		if (!ItemWidget)
		{
			ItemWidget = CreateWidget<URpgInventorySpatialItemWidget>(GetWorld(), SpatialItemWidgetClass);
		}
		if (!ItemWidget)
		{
			return;
		}

		ItemWidget->SetVisibility(ESlateVisibility::Visible);
		ItemWidget->SetIsEnabled(true);
		ItemWidget->SetOwningSpatialGrid(this);
		ItemWidget->SetDragDropCoordinator(DragDropCoordinator);
		ItemWidget->SetInventoryPanelActive(bInventoryPanelActive);
		if (AddressSlot)
		{
			ItemWidget->SetAddressSlotViewModel(AddressSlot);
		}
		else
		{
			ItemWidget->SetEntryViewModel(EntryViewModel);
		}

		if (ItemWidget->GetParent() != ItemCanvas)
		{
			ItemCanvas->AddChildToCanvas(ItemWidget);
		}
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ItemWidget->Slot))
		{
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetPosition(GetCellPosition(Placement.X, Placement.Y));
			CanvasSlot->SetSize(GetPlacementSize(Placement));
			CanvasSlot->SetZOrder(10);
		}

		ReconciledItems.Add(ItemWidget);
		ReusedItems.Add(ItemWidget);
	};

	if (GroupViewModel)
	{
		for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
		{
			if (!SlotViewModel || !SlotViewModel->ShouldRenderItemVisual())
			{
				continue;
			}

			AddSpatialItem(SlotViewModel->GetItemPlacement(), SlotViewModel, nullptr);
		}
	}
	else if (PanelViewModel)
	{
		for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
		{
			if (!EntryViewModel || EntryViewModel->IsEmptySlot())
			{
				continue;
			}

			const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
			if (Placement.GetContainerHandle() == ResolveContainerHandle())
			{
				AddSpatialItem(Placement, nullptr, EntryViewModel);
			}
		}
	}

	for (URpgInventorySpatialItemWidget* PreviousItem : ItemWidgets)
	{
		if (PreviousItem && !ReusedItems.Contains(PreviousItem))
		{
			ItemCanvas->RemoveChild(PreviousItem);
		}
	}
	ItemWidgets = MoveTemp(ReconciledItems);
	ApplyEntryDimming();
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

void URpgInventorySpatialGridWidget::ApplyEntryDimming()
{
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetRenderOpacity(
			IsEntryDimmed(ItemWidget->GetRepresentedEntryId())
				? DimmedEntryOpacity
				: 1.0f);
	}
	InvalidateLayoutAndVolatility();
}

void URpgInventorySpatialGridWidget::ClearObservedSlotDelegates()
{
	for (URpgInventoryAddressSlotViewModel* SlotViewModel : ObservedAddressSlots)
	{
		if (SlotViewModel)
		{
			SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleAddressSlotChanged);
		}
	}
	ObservedAddressSlots.Reset();
}

void URpgInventorySpatialGridWidget::ObserveSlotDelegates()
{
	ClearObservedSlotDelegates();
	if (!GroupViewModel)
	{
		return;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel)
		{
			SlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleAddressSlotChanged);
			ObservedAddressSlots.Add(SlotViewModel);
		}
	}
}

void URpgInventorySpatialGridWidget::ClearObservedEntryDelegates()
{
	for (URpgInventoryEntryViewModel* EntryViewModel : ObservedEntries)
	{
		if (EntryViewModel)
		{
			EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryChanged);
		}
	}
	ObservedEntries.Reset();
}

void URpgInventorySpatialGridWidget::ObserveEntryDelegates()
{
	ClearObservedEntryDelegates();
	if (!PanelViewModel)
	{
		return;
	}

	for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
	{
		if (EntryViewModel)
		{
			EntryViewModel->OnEntryChanged.AddUniqueDynamic(this, &ThisClass::HandleEntryChanged);
			ObservedEntries.Add(EntryViewModel);
		}
	}
}

void URpgInventorySpatialGridWidget::NotifySelectionChanged()
{
	bSelectionVisualSuppressed = false;
	if (!PanelNavigationCoordinator)
	{
		return;
	}

	UObject* SelectedObject = nullptr;
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		SelectedObject = AddressSlot;
	}
	else if (URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel())
	{
		SelectedObject = EntryViewModel;
	}

	PanelNavigationCoordinator->NotifySpatialPanelSelectionChanged(this, SelectedObject);
}

bool URpgInventorySpatialGridWidget::MoveCursorBy(int32 DeltaX, int32 DeltaY, APlayerController* OwningPlayer)
{
	const int32 NextX = FMath::Clamp(CursorX + DeltaX, 0, FMath::Max(0, GridSize.Width - 1));
	const int32 NextY = FMath::Clamp(CursorY + DeltaY, 0, FMath::Max(0, GridSize.Height - 1));
	if (NextX == CursorX && NextY == CursorY)
	{
		return false;
	}

	return SelectCell(NextX, NextY, OwningPlayer);
}

bool URpgInventorySpatialGridWidget::TryGetCellFromLocalPosition(FVector2D LocalPosition, int32& OutX, int32& OutY) const
{
	OutX = INDEX_NONE;
	OutY = INDEX_NONE;
	if (!GridSize.IsValid())
	{
		return false;
	}

	const int32 X = SnapLocalAxisToCell(LocalPosition.X, GridSize.Width, CellSize, CellPadding);
	const int32 Y = SnapLocalAxisToCell(LocalPosition.Y, GridSize.Height, CellSize, CellPadding);
	if (!IsValidCell(X, Y))
	{
		return false;
	}

	OutX = X;
	OutY = Y;
	return true;
}

bool URpgInventorySpatialGridWidget::TryGetCellFromScreenPosition(FVector2D ScreenPosition, int32& OutX, int32& OutY) const
{
	return TryGetCellFromLocalPosition(GetGridInteractionGeometry().AbsoluteToLocal(ScreenPosition), OutX, OutY);
}

FRpgInventoryDropTarget URpgInventorySpatialGridWidget::MakeDropTargetAtCursor() const
{
	return DragDropCoordinator && DragDropCoordinator->HasHeldPayload()
		? MakeDropTargetForCell(DragDropCoordinator->GetHeldPayload(), CursorX, CursorY)
		: MakeDropTargetForCell(CursorX, CursorY);
}

FRpgInventoryDropTarget URpgInventorySpatialGridWidget::MakeDropTargetForCell(int32 X, int32 Y) const
{
	if (GroupViewModel)
	{
		return URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(FindAddressCell(X, Y));
	}

	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	Target.TargetInventory = Inventory.Get();
	Target.TargetPlacement = MakeCellPlacement(
		ResolveContainerHandle(),
		X,
		Y,
		DragDropCoordinator && DragDropCoordinator->HasHeldPayload()
			? DragDropCoordinator->GetTargetRotationForPayload(DragDropCoordinator->GetHeldPayload())
			: false);
	return Target;
}

FRpgInventoryDropTarget URpgInventorySpatialGridWidget::MakeDropTargetForCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y) const
{
	const FRpgInventoryGridPlacement TargetPlacement = MakeTargetPlacementForCell(Payload, X, Y);
	return MakeDropTargetForPlacement(Payload, TargetPlacement);
}

FRpgInventoryDropTarget URpgInventorySpatialGridWidget::MakeDropTargetForPlacement(const FRpgInventoryDragPayload& Payload, const FRpgInventoryGridPlacement& TargetPlacement) const
{
	FRpgInventoryDropTarget Target;

	if (GroupViewModel)
	{
		Target.TargetInventory = ResolveGridInventory();
		Target.TargetPlacement = TargetPlacement;
		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot ||
			Payload.SourceType == ERpgInventoryDragSourceType::PlayerInventorySlotAddress)
		{
			Target.TargetType = ERpgInventoryDropTargetType::PlayerInventorySlotAddress;
			if (TargetPlacement.IsValid())
			{
				Target.SlotAddress.SetContainerHandle(TargetPlacement.GetContainerHandle());
				Target.SlotAddress.X = TargetPlacement.X;
				Target.SlotAddress.Y = TargetPlacement.Y;
			}
			return Target;
		}

		Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
		return Target;
	}

	Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	Target.TargetInventory = Inventory.Get();
	Target.TargetPlacement = TargetPlacement;
	return Target;
}

FRpgInventoryGridPlacement URpgInventorySpatialGridWidget::MakeTargetPlacementForCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y) const
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	const bool bTargetRotated = DragDropCoordinator
		? DragDropCoordinator->GetTargetRotationForPayload(ResolvedPayload)
		: (ResolvedPayload.SourcePlacement.IsValid() && ResolvedPayload.SourcePlacement.bRotated);
	const FIntPoint GrabOffset = ClampSpatialGrabOffset(ResolvedPayload, bTargetRotated);
	const FRpgInventoryGridSize Footprint = GetPayloadUnrotatedFootprint(ResolvedPayload);

	return MakeCellPlacement(
		ResolveContainerHandle(),
		X - GrabOffset.X,
		Y - GrabOffset.Y,
		bTargetRotated,
		Footprint.Width,
		Footprint.Height);
}

FRpgInventoryGridPlacement URpgInventorySpatialGridWidget::ResolveTargetPlacementAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, int32& OutAnchorX, int32& OutAnchorY) const
{
	OutAnchorX = INDEX_NONE;
	OutAnchorY = INDEX_NONE;

	const FGeometry GridGeometry = GetGridInteractionGeometry();
	const FVector2D LocalPointer = GridGeometry.AbsoluteToLocal(ScreenPosition);
	if (!TryGetCellFromLocalPosition(LocalPointer, OutAnchorX, OutAnchorY))
	{
		return FRpgInventoryGridPlacement();
	}

	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	const bool bTargetRotated = DragDropCoordinator
		? DragDropCoordinator->GetTargetRotationForPayload(ResolvedPayload)
		: (ResolvedPayload.SourcePlacement.IsValid() && ResolvedPayload.SourcePlacement.bRotated);

	if (!ResolvedPayload.bHasPointerGrabOffset)
	{
		return MakeTargetPlacementForCell(ResolvedPayload, OutAnchorX, OutAnchorY);
	}

	const float Stride = CellSize + CellPadding;
	const FVector2D GhostTopLeft = LocalPointer - ResolvedPayload.PointerGrabOffset;
	const int32 OriginX = Stride > 0.0f ? FMath::RoundToInt(GhostTopLeft.X / Stride) : OutAnchorX;
	const int32 OriginY = Stride > 0.0f ? FMath::RoundToInt(GhostTopLeft.Y / Stride) : OutAnchorY;
	const FRpgInventoryGridSize Footprint = GetPayloadUnrotatedFootprint(ResolvedPayload);

	return MakeCellPlacement(
		ResolveContainerHandle(),
		OriginX,
		OriginY,
		bTargetRotated,
		Footprint.Width,
		Footprint.Height);
}

FRpgInventoryDragPayload URpgInventorySpatialGridWidget::MakePayloadFromSelectedItem() const
{
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return AddressSlot->GetItemInstance()
			? URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(AddressSlot)
			: FRpgInventoryDragPayload();
	}

	if (URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel())
	{
		return URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromEntry(EntryViewModel);
	}

	return FRpgInventoryDragPayload();
}

URpgInventoryAddressSlotViewModel* URpgInventorySpatialGridWidget::FindAddressCell(int32 X, int32 Y) const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel && IsSameGridCell(SlotViewModel->GetPlacement(), ResolveContainerHandle(), X, Y))
		{
			return SlotViewModel;
		}
	}

	return nullptr;
}

URpgInventoryAddressSlotViewModel* URpgInventorySpatialGridWidget::FindAddressItemAtCell(int32 X, int32 Y) const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel &&
			SlotViewModel->ShouldRenderItemVisual() &&
			SlotViewModel->GetItemPlacement().GetContainerHandle() == ResolveContainerHandle() &&
			SlotViewModel->GetItemPlacement().ContainsCell(X, Y))
		{
			return SlotViewModel;
		}
	}

	return nullptr;
}

URpgInventoryEntryViewModel* URpgInventorySpatialGridWidget::FindEntryAtCell(int32 X, int32 Y) const
{
	if (!PanelViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
	{
		if (EntryViewModel &&
			!EntryViewModel->IsEmptySlot() &&
			EntryViewModel->GetPlacement().GetContainerHandle() == ResolveContainerHandle() &&
			EntryViewModel->GetPlacement().ContainsCell(X, Y))
		{
			return EntryViewModel;
		}
	}

	return nullptr;
}

URpgInventoryManagerComponent* URpgInventorySpatialGridWidget::ResolveGridInventory() const
{
	if (Inventory)
	{
		return Inventory.Get();
	}

	if (GroupViewModel)
	{
		for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
		{
			if (SlotViewModel && SlotViewModel->GetInventoryManager())
			{
				return SlotViewModel->GetInventoryManager();
			}
		}
	}

	return nullptr;
}

FGeometry URpgInventorySpatialGridWidget::GetGridInteractionGeometry() const
{
	return RootSizeBox ? RootSizeBox->GetCachedGeometry() : GetCachedGeometry();
}

FVector2D URpgInventorySpatialGridWidget::GetGridLocalSize() const
{
	const FVector2D DesiredSize = GridSize.IsValid()
		? GetGridDesiredLocalSize(GridSize, CellSize, CellPadding)
		: FVector2D::ZeroVector;
	const FVector2D CachedSize = GetGridInteractionGeometry().GetLocalSize();
	return DesiredSize.X > KINDA_SMALL_NUMBER && DesiredSize.Y > KINDA_SMALL_NUMBER
		? DesiredSize
		: CachedSize;
}

FVector2D URpgInventorySpatialGridWidget::GetCellPosition(int32 X, int32 Y) const
{
	const float Stride = CellSize + CellPadding;
	return FVector2D(X * Stride, Y * Stride);
}

FVector2D URpgInventorySpatialGridWidget::GetPlacementSize(const FRpgInventoryGridPlacement& Placement) const
{
	const FRpgInventoryGridSize OccupiedSize = Placement.GetOccupiedSize();
	return FVector2D(
		OccupiedSize.Width * CellSize + FMath::Max(0, OccupiedSize.Width - 1) * CellPadding,
		OccupiedSize.Height * CellSize + FMath::Max(0, OccupiedSize.Height - 1) * CellPadding);
}

FRpgInventoryContainerHandle URpgInventorySpatialGridWidget::ResolveContainerHandle() const
{
	if (ContainerHandle.IsValid())
	{
		return ContainerHandle;
	}

	const FName ResolvedId = !ContainerId.IsNone()
		? ContainerId
		: (Inventory ? Inventory->GetDefaultContainerId() : NAME_None);
	return FRpgInventoryContainerHandle::MakeRoot(ResolvedId);
}

FName URpgInventorySpatialGridWidget::ResolveContainerId() const
{
	return ResolveContainerHandle().ContainerId;
}

bool URpgInventorySpatialGridWidget::IsValidCell(int32 X, int32 Y) const
{
	return GridSize.IsValid() &&
		X >= 0 &&
		Y >= 0 &&
		X < GridSize.Width &&
		Y < GridSize.Height;
}

URpgInventorySlotGroupWidget::URpgInventorySlotGroupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SpatialGridWidgetClass = URpgInventorySpatialGridWidget::StaticClass();
}

void URpgInventorySlotGroupWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	EnsureSpatialGrid();
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationIdPrefix = InPanelIdPrefix;
	RegisterPanelNavigationEntry();
}

void URpgInventorySlotGroupWidget::SetSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	GroupViewModel = InGroupViewModel;
	EnsureSpatialGrid();
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
		SpatialGrid->BindSlotGroupViewModel(GroupViewModel);
	}

	RegisterPanelNavigationEntry();
	BP_OnSlotGroupViewModelSet(GroupViewModel);
}

void URpgInventorySlotGroupWidget::NativeDestruct()
{
	if (SpatialGrid)
	{
		SpatialGrid->SetPanelNavigationCoordinator(nullptr, NAME_None);
	}

	Super::NativeDestruct();
}

void URpgInventorySlotGroupWidget::EnsureSpatialGrid()
{
	if (SpatialGrid)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint child widget named SpatialGrid using URpgInventorySpatialGridWidget."), *GetNameSafe(this));
}

void URpgInventorySlotGroupWidget::RegisterPanelNavigationEntry()
{
	if (!PanelNavigationCoordinator || !SpatialGrid || !GroupViewModel)
	{
		return;
	}

	URpgInventoryManagerComponent* Inventory = ResolveGroupInventory();
	if (!Inventory)
	{
		return;
	}

	SpatialGrid->SetPanelNavigationCoordinator(PanelNavigationCoordinator, MakePanelNavigationId());
	PanelNavigationCoordinator->RegisterSpatialInventoryPanel(MakePanelNavigationId(), SpatialGrid, Inventory);
}

URpgInventoryManagerComponent* URpgInventorySlotGroupWidget::ResolveGroupInventory() const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel && SlotViewModel->GetInventoryManager())
		{
			return SlotViewModel->GetInventoryManager();
		}
	}

	return nullptr;
}

FName URpgInventorySlotGroupWidget::MakePanelNavigationId() const
{
	const FRpgInventoryContainerHandle GroupHandle = GroupViewModel
		? GroupViewModel->GetContainerHandle()
		: FRpgInventoryContainerHandle();
	const FName GroupId = GroupHandle.IsValid() ? FName(*GroupHandle.ToString()) : NAME_None;
	if (PanelNavigationIdPrefix.IsNone())
	{
		return GroupId;
	}

	return FName(*FString::Printf(TEXT("%s.%s"), *PanelNavigationIdPrefix.ToString(), *GroupId.ToString()));
}

FName URpgInventorySlotGroupWidget::GetSlotGroupId() const
{
	return GroupViewModel ? GroupViewModel->GetGroupId() : NAME_None;
}

FRpgInventoryContainerHandle URpgInventorySlotGroupWidget::GetSlotGroupHandle() const
{
	return GroupViewModel ? GroupViewModel->GetContainerHandle() : FRpgInventoryContainerHandle();
}

URpgInventorySlotGroupPanelWidget::URpgInventorySlotGroupPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GroupWidgetClass = URpgInventorySlotGroupWidget::StaticClass();
}

void URpgInventorySlotGroupPanelWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		ApplyCoordinatorToGroup(GroupWidget);
	}
}

void URpgInventorySlotGroupPanelWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationIdPrefix = InPanelIdPrefix;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		ApplyNavigationToGroup(GroupWidget);
	}
}

void URpgInventorySlotGroupPanelWidget::SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups)
{
	GroupItems.Reset();
	GroupItems.Reserve(InGroups.Num());
	for (URpgInventorySlotGroupViewModel* GroupViewModel : InGroups)
	{
		GroupItems.Add(GroupViewModel);
	}

	RebuildGroupWidgets();
}

void URpgInventorySlotGroupPanelWidget::GetSpatialGridWidgets(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		if (GroupWidget && GroupWidget->GetSpatialGridWidget())
		{
			OutGrids.Add(GroupWidget->GetSpatialGridWidget());
		}
	}
}

void URpgInventorySlotGroupPanelWidget::EnsureGroupsPanel()
{
	if (GroupsPanel)
	{
		return;
	}

	GroupsPanel = Cast<UPanelWidget>(GetRootWidget());
	if (GroupsPanel || !WidgetTree)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint panel named GroupsPanel or a panel root to receive spatial group widgets."), *GetNameSafe(this));
}

void URpgInventorySlotGroupPanelWidget::RebuildGroupWidgets()
{
	EnsureGroupsPanel();
	if (!GroupsPanel)
	{
		GroupWidgets.Reset();
		return;
	}

	TSubclassOf<URpgInventorySlotGroupWidget> EntryClass = GroupWidgetClass;
	if (!EntryClass)
	{
		EntryClass = URpgInventorySlotGroupWidget::StaticClass();
	}

	TMap<FRpgInventoryContainerHandle, TObjectPtr<URpgInventorySlotGroupWidget>> ExistingGroups;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		if (GroupWidget && GroupWidget->GetSlotGroupHandle().IsValid())
		{
			ExistingGroups.Add(GroupWidget->GetSlotGroupHandle(), GroupWidget);
		}
	}

	TArray<TObjectPtr<URpgInventorySlotGroupWidget>> ReconciledGroups;
	TSet<FRpgInventoryContainerHandle> AddedGroupHandles;
	for (URpgInventorySlotGroupViewModel* GroupViewModel : GroupItems)
	{
		if (!GroupViewModel)
		{
			continue;
		}

		const FRpgInventoryContainerHandle GroupHandle = GroupViewModel->GetContainerHandle();
		if (!GroupHandle.IsValid() || AddedGroupHandles.Contains(GroupHandle))
		{
			continue;
		}

		URpgInventorySlotGroupWidget* GroupWidget = ExistingGroups.FindRef(GroupHandle);
		if (GroupWidget && !GroupWidget->IsA(EntryClass))
		{
			GroupWidget = nullptr;
		}
		if (!GroupWidget)
		{
			GroupWidget = CreateWidget<URpgInventorySlotGroupWidget>(GetWorld(), EntryClass);
		}
		if (!GroupWidget)
		{
			continue;
		}

		ReconciledGroups.Add(GroupWidget);
		AddedGroupHandles.Add(GroupHandle);
		ApplyCoordinatorToGroup(GroupWidget);
		ApplyNavigationToGroup(GroupWidget);
		GroupWidget->SetSlotGroupViewModel(GroupViewModel);
	}

	bool bOrderMatches = ReconciledGroups.Num() == GroupWidgets.Num();
	if (bOrderMatches)
	{
		for (int32 Index = 0; Index < ReconciledGroups.Num(); ++Index)
		{
			if (ReconciledGroups[Index] != GroupWidgets[Index] || ReconciledGroups[Index]->GetParent() != GroupsPanel)
			{
				bOrderMatches = false;
				break;
			}
		}
	}

	if (!bOrderMatches)
	{
		// Strong local references keep keyed widgets alive while the panel order is reconciled.
		GroupsPanel->ClearChildren();
		for (URpgInventorySlotGroupWidget* GroupWidget : ReconciledGroups)
		{
			if (GroupWidget)
			{
				GroupsPanel->AddChild(GroupWidget);
			}
		}
	}

	GroupWidgets = MoveTemp(ReconciledGroups);
}

void URpgInventorySlotGroupPanelWidget::ApplyCoordinatorToGroup(URpgInventorySlotGroupWidget* GroupWidget) const
{
	if (GroupWidget)
	{
		GroupWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupPanelWidget::ApplyNavigationToGroup(URpgInventorySlotGroupWidget* GroupWidget) const
{
	if (GroupWidget)
	{
		GroupWidget->SetPanelNavigationCoordinator(PanelNavigationCoordinator, PanelNavigationIdPrefix);
	}
}
