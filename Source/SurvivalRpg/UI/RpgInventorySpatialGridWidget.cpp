#include "RpgInventorySpatialGridWidget.h"

#include "RpgInventorySpatialCellWidget.h"
#include "RpgInventorySpatialItemWidget.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropOperation.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryEntryViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryPanelViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryAddressSlotViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventorySlotGroupViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryUiGeometry.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySpatialGridWidget)

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

	int32 ApplyOriginSnapHysteresis(
		float UnsnappedPosition,
		float Stride,
		int32 CandidateOrigin,
		int32 PreviousOrigin,
		float HysteresisFraction)
	{
		if (Stride <= KINDA_SMALL_NUMBER || CandidateOrigin == PreviousOrigin ||
			FMath::Abs(CandidateOrigin - PreviousOrigin) != 1)
		{
			return CandidateOrigin;
		}

		const float Hysteresis = FMath::Clamp(HysteresisFraction, 0.0f, 0.25f) * Stride;
		if (CandidateOrigin > PreviousOrigin)
		{
			const float SwitchThreshold = (static_cast<float>(PreviousOrigin) + 0.5f) * Stride + Hysteresis;
			return UnsnappedPosition < SwitchThreshold ? PreviousOrigin : CandidateOrigin;
		}

		const float SwitchThreshold = (static_cast<float>(PreviousOrigin) - 0.5f) * Stride - Hysteresis;
		return UnsnappedPosition > SwitchThreshold ? PreviousOrigin : CandidateOrigin;
	}

	bool PlacementFootprintContainsCellUnchecked(const FRpgInventoryGridPlacement& Placement, int32 CellX, int32 CellY)
	{
		if (!Placement.ContainerHandle.IsValid() || Placement.Width <= 0 || Placement.Height <= 0)
		{
			return false;
		}

		const FRpgInventoryGridSize OccupiedSize = Placement.GetOccupiedSize();
		return CellX >= Placement.X &&
			CellY >= Placement.Y &&
			CellX < Placement.X + OccupiedSize.Width &&
			CellY < Placement.Y + OccupiedSize.Height;
	}

	FRpgInventoryGridPlacement MakeCellPlacement(
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y,
		bool bRotated,
		int32 Width = 1,
		int32 Height = 1)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(ContainerHandle);
		Placement.X = X;
		Placement.Y = Y;
		Placement.Width = FMath::Max(1, Width);
		Placement.Height = FMath::Max(1, Height);
		Placement.bRotated = bRotated;
		return Placement;
	}

	FRpgInventoryGridSize GetPayloadUnrotatedFootprint(
		const FRpgInventoryDragPayload& Payload);

	FRpgInventoryGridSize GetPayloadOccupiedSize(const FRpgInventoryDragPayload& Payload, bool bTargetRotated)
	{
		const FRpgInventoryGridSize Footprint =
			GetPayloadUnrotatedFootprint(Payload);
		return Footprint.IsValid()
			? Footprint.GetRotated(bTargetRotated)
			: Footprint;
	}

	FRpgInventoryGridSize GetPayloadUnrotatedFootprint(const FRpgInventoryDragPayload& Payload)
	{
		if (Payload.ItemFootprint.IsValid())
		{
			return Payload.ItemFootprint;
		}

		FRpgInventoryGridSize InvalidFootprint;
		InvalidFootprint.Width = 0;
		InvalidFootprint.Height = 0;
		return InvalidFootprint;
	}

	FIntPoint ClampSpatialGrabOffset(const FRpgInventoryDragPayload& Payload, bool bTargetRotated)
	{
		const FRpgInventoryGridSize OccupiedSize = GetPayloadOccupiedSize(Payload, bTargetRotated);
		if (!OccupiedSize.IsValid())
		{
			return FIntPoint::ZeroValue;
		}
		const int32 MaxOffsetX = FMath::Max(0, OccupiedSize.Width - 1);
		const int32 MaxOffsetY = FMath::Max(0, OccupiedSize.Height - 1);
		return Payload.bHasSpatialGrabOffset
			? FIntPoint(FMath::Clamp(Payload.GrabCellOffsetX, 0, MaxOffsetX), FMath::Clamp(Payload.GrabCellOffsetY, 0, MaxOffsetY))
			: FIntPoint::ZeroValue;
	}

	void SetSpatialGrabOffset(FRpgInventoryDragPayload& Payload, int32 OffsetX, int32 OffsetY)
	{
		const FRpgInventoryGridSize OccupiedSize =
			GetPayloadOccupiedSize(
				Payload,
				Payload.SourcePlacement.bRotated);
		if (!OccupiedSize.IsValid())
		{
			Payload.bHasSpatialGrabOffset = false;
			Payload.GrabCellOffsetX = 0;
			Payload.GrabCellOffsetY = 0;
			return;
		}

		Payload.bHasSpatialGrabOffset = true;
		Payload.GrabCellOffsetX = FMath::Clamp(
			OffsetX,
			0,
			OccupiedSize.Width - 1);
		Payload.GrabCellOffsetY = FMath::Clamp(
			OffsetY,
			0,
			OccupiedSize.Height - 1);
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

	if (InventoryPresentationHost)
	{
		InventoryPresentationHost->DismissInventoryPresentationForSource(this);
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
	ContainerHandle = GroupViewModel ? GroupViewModel->GetContainerHandle() : FRpgInventoryContainerHandle();
	UpdateGridSizeFromBinding();
	ObserveSlotDelegates();
	RebuildItemOverlay();
	SelectBestCell(GetOwningPlayer(), false);
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

	if (InventoryPresentationHost)
	{
		InventoryPresentationHost->DismissInventoryPresentationForSource(this);
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

	if (PanelViewModel)
	{
		if (Inventory && ContainerHandle.IsValid())
		{
			PanelViewModel->BindInventoryContainer(Inventory, ContainerHandle);
		}
		else
		{
			PanelViewModel->UnbindInventory();
		}
		PanelViewModel->OnEntriesChanged.AddUniqueDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}

	UpdateGridSizeFromBinding();
	ObserveEntryDelegates();
	RebuildItemOverlay();
	if (Inventory && ContainerHandle.IsValid())
	{
		SelectBestCell(GetOwningPlayer(), false);
	}
	else
	{
		ClearSelectionVisual();
	}
}

void URpgInventorySpatialGridWidget::ReleaseInventoryPresentation()
{
	CancelPendingSplit();
	SetInventoryPresentationHost(nullptr);
	ClearEntryDimming();
	SetPanelNavigationCoordinator(nullptr, NAME_None);
	SetDragDropCoordinator(nullptr);
	BindInventoryContainerPanelViewModel(
		nullptr,
		nullptr,
		FRpgInventoryContainerHandle());

	bPendingLeftClickAccept = false;
	bHasLastPointerPreviewScreenPosition = false;
	LastPointerPreviewScreenPosition = FVector2D::ZeroVector;
	bInventoryPanelActive = true;
	ClearSelectionVisual();
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
		if (URpgInventoryInteractionSession* PreviousSession = DragDropCoordinator->GetInteractionSession())
		{
			PreviousSession->OnSpatialPreviewChanged.RemoveDynamic(this, &ThisClass::HandleSpatialPreviewChanged);
		}
	}

	DragDropCoordinator = InCoordinator;
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldPayloadChanged);
		if (URpgInventoryInteractionSession* Session = DragDropCoordinator->GetInteractionSession())
		{
			Session->OnSpatialPreviewChanged.AddUniqueDynamic(this, &ThisClass::HandleSpatialPreviewChanged);
			HandleSpatialPreviewChanged(Session->GetSpatialPreviewDescriptor());
		}
	}
	else
	{
		ClearSpatialPreviewLocal();
		bHasLastPointerPreviewScreenPosition = false;
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

void URpgInventorySpatialGridWidget::SetInventoryPresentationHost(
	URpgInventoryInteractionScreenWidget* InPresentationHost)
{
	if (InventoryPresentationHost &&
		InventoryPresentationHost != InPresentationHost)
	{
		InventoryPresentationHost->DismissInventoryPresentationForSource(this);
	}
	InventoryPresentationHost = InPresentationHost;
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
	URpgInventoryAddressSlotViewModel* AddressSlot =
		GetSelectedAddressSlot();
	URpgInventoryEntryViewModel* EntryViewModel =
		GetSelectedEntryViewModel();
	const bool bCanSplit = DragDropCoordinator &&
		(AddressSlot
			? DragDropCoordinator->CanExecuteContextAction(
				AddressSlot,
				ERpgInventoryContextAction::Split,
				true)
			: DragDropCoordinator->CanExecuteContextAction(
				EntryViewModel,
				ERpgInventoryContextAction::Split,
				true));
	if (!Item || StackCount <= 1 || !bCanSplit)
	{
		return false;
	}

	PendingSplitEntryId = GetSelectedEntryId();
	PendingSplitItemId = Item->GetItemId();
	if (!PendingSplitEntryId.IsValid() || !PendingSplitItemId.IsValid())
	{
		return false;
	}

	PendingSplitMaximum = StackCount - 1;
	const int32 DefaultSplitCount = FMath::Clamp(StackCount / 2, 1, PendingSplitMaximum);
	if (InventoryPresentationHost &&
		InventoryPresentationHost->OpenInventorySplitDialog(
			this,
			PendingSplitEntryId,
			1,
			PendingSplitMaximum,
			DefaultSplitCount))
	{
		return true;
	}

	CancelPendingSplit();
	return false;
}

bool URpgInventorySpatialGridWidget::ConfirmPendingSplit(int32 SplitCount)
{
	if (!DragDropCoordinator || SplitCount < 1 || SplitCount > PendingSplitMaximum)
	{
		return false;
	}

	URpgInventoryAddressSlotViewModel* PendingAddressSlot = nullptr;
	if (GroupViewModel)
	{
		for (URpgInventoryAddressSlotViewModel* AddressSlot : GroupViewModel->GetSlots())
		{
			if (AddressSlot && AddressSlot->GetEntryId() == PendingSplitEntryId && AddressSlot->GetItemInstance() &&
				AddressSlot->GetItemInstance()->GetItemId() == PendingSplitItemId)
			{
				PendingAddressSlot = AddressSlot;
				break;
			}
		}
	}
	URpgInventoryEntryViewModel* PendingEntry = nullptr;
	if (PanelViewModel)
	{
		for (URpgInventoryEntryViewModel* Entry : PanelViewModel->GetEntries())
		{
			if (Entry && Entry->GetEntryId() == PendingSplitEntryId && Entry->GetItemInstance() &&
				Entry->GetItemInstance()->GetItemId() == PendingSplitItemId)
			{
				PendingEntry = Entry;
				break;
			}
		}
	}

	bool bDispatched = false;
	if (PendingAddressSlot && SplitCount < PendingAddressSlot->GetStackCount())
	{
		bDispatched = DragDropCoordinator->QuickSplitAddressSlot(
			PendingAddressSlot,
			FRpgInventoryGridPlacement(),
			SplitCount);
	}
	else if (PendingEntry && SplitCount < PendingEntry->GetStackCount())
	{
		bDispatched = DragDropCoordinator->QuickSplitEntry(
			PendingEntry,
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
	PendingSplitEntryId.Invalidate();
	PendingSplitItemId = FRpgInventoryItemId();
	PendingSplitMaximum = 0;
}

bool URpgInventorySpatialGridWidget::RequestContextMenuForSelectedCell(FVector2D ScreenPosition)
{
	return InventoryPresentationHost &&
		InventoryPresentationHost->OpenInventoryContextMenu(
			this,
			ScreenPosition);
}

bool URpgInventorySpatialGridWidget::TryGetSelectedContextMenuScreenAnchor(
	FVector2D& OutAbsoluteScreenPosition) const
{
	const FGeometry GridGeometry = GetGridInteractionGeometry();
	FRpgInventoryGridPlacement SelectedPlacement;
	if (const URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		SelectedPlacement = AddressSlot->GetItemPlacement();
	}
	else if (const URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel())
	{
		SelectedPlacement = EntryViewModel->GetPlacement();
	}

	if (SelectedPlacement.IsValid())
	{
		const FVector2D ItemCenter =
			GetCellPosition(SelectedPlacement.X, SelectedPlacement.Y) +
			GetPlacementSize(SelectedPlacement) * 0.5f;
		if (RpgInventoryUiGeometry::TryResolveAbsolutePoint(
			GridGeometry,
			ItemCenter,
			OutAbsoluteScreenPosition))
		{
			return true;
		}
	}

	if (!IsValidCell(CursorX, CursorY))
	{
		OutAbsoluteScreenPosition = FVector2D::ZeroVector;
		return false;
	}

	return RpgInventoryUiGeometry::TryResolveAbsolutePoint(
		GridGeometry,
		GetCellPosition(CursorX, CursorY) + FVector2D(CellSize * 0.5f),
		OutAbsoluteScreenPosition);
}

TArray<ERpgInventoryContextAction> URpgInventorySpatialGridWidget::GetSelectedContextActions() const
{
	if (!DragDropCoordinator)
	{
		return TArray<ERpgInventoryContextAction>();
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->GetAvailableContextActions(
			AddressSlot,
			true);
	}

	return DragDropCoordinator->GetAvailableContextActions(
		GetSelectedEntryViewModel(),
		true);
}

bool URpgInventorySpatialGridWidget::QueryInventoryContextActions(
	FRpgInventoryContextActionSnapshot& OutSnapshot) const
{
	OutSnapshot = FRpgInventoryContextActionSnapshot();
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot =
		GetSelectedAddressSlot())
	{
		const URpgInventoryItemInstance* Item =
			AddressSlot->GetItemInstance();
		if (!Item)
		{
			return false;
		}

		OutSnapshot.SourceKind =
			ERpgInventoryContextActionSourceKind::Address;
		OutSnapshot.EntryId = AddressSlot->GetEntryId();
		OutSnapshot.ItemId = Item->GetItemId();
		OutSnapshot.SourcePlacement =
			AddressSlot->GetItemPlacement();
		OutSnapshot.SlotAddress =
			AddressSlot->GetSlotAddress();
		OutSnapshot.StackCount =
			AddressSlot->GetStackCount();
		OutSnapshot.Actions =
			DragDropCoordinator->GetAvailableContextActions(
				AddressSlot,
				true);
	}
	else if (URpgInventoryEntryViewModel* EntryViewModel =
		GetSelectedEntryViewModel())
	{
		const URpgInventoryItemInstance* Item =
			EntryViewModel->GetItemInstance();
		if (!Item)
		{
			return false;
		}

		OutSnapshot.SourceKind =
			ERpgInventoryContextActionSourceKind::SpatialEntry;
		OutSnapshot.EntryId = EntryViewModel->GetEntryId();
		OutSnapshot.ItemId = Item->GetItemId();
		OutSnapshot.SourcePlacement =
			EntryViewModel->GetPlacement();
		OutSnapshot.StackCount =
			EntryViewModel->GetStackCount();
		OutSnapshot.Actions =
			DragDropCoordinator->GetAvailableContextActions(
				EntryViewModel,
				true);
	}

	OutSnapshot.QuickAccessSlotIndex =
		GetSelectedQuickAccessSlotIndex();
	return OutSnapshot.IsValid();
}

bool URpgInventorySpatialGridWidget::ExecuteInventoryContextAction(
	const FRpgInventoryContextActionSnapshot& ExpectedSnapshot,
	ERpgInventoryContextAction Action,
	int32 QuickAccessSlotIndex)
{
	FRpgInventoryContextActionSnapshot CurrentSnapshot;
	return QueryInventoryContextActions(CurrentSnapshot) &&
		ExpectedSnapshot.MatchesStableSource(CurrentSnapshot) &&
		CurrentSnapshot.Actions.Contains(Action) &&
		ExecuteSelectedContextAction(
			Action,
			0,
			QuickAccessSlotIndex);
}

bool URpgInventorySpatialGridWidget::ExecuteSelectedContextAction(
	ERpgInventoryContextAction Action,
	int32 SplitCount,
	int32 QuickAccessSlotIndex)
{
	URpgInventoryItemInstance* Item = GetSelectedItemInstance();
	if (!Item || !DragDropCoordinator)
	{
		return false;
	}

	URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot();
	URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel();
	const bool bCanExecute = AddressSlot
		? DragDropCoordinator->CanExecuteContextAction(
			AddressSlot,
			Action,
			true)
		: DragDropCoordinator->CanExecuteContextAction(
			EntryViewModel,
			Action,
			true);
	if (!bCanExecute)
	{
		return false;
	}

	switch (Action)
	{
	case ERpgInventoryContextAction::Use:
		return AddressSlot
			? DragDropCoordinator->ExecuteAddressItemAction(AddressSlot, ERpgInventoryContextAction::Use, 1)
			: DragDropCoordinator->ExecuteEntryItemAction(EntryViewModel, ERpgInventoryContextAction::Use, 1);

	case ERpgInventoryContextAction::EquipAndActivate:
		return AddressSlot
			? DragDropCoordinator->ExecuteAddressItemAction(AddressSlot, ERpgInventoryContextAction::EquipAndActivate, 1)
			: DragDropCoordinator->ExecuteEntryItemAction(EntryViewModel, ERpgInventoryContextAction::EquipAndActivate, 1);

	case ERpgInventoryContextAction::MoveToCarry:
		return AddressSlot
			? DragDropCoordinator->ExecuteAddressItemAction(AddressSlot, ERpgInventoryContextAction::MoveToCarry, 1)
			: DragDropCoordinator->ExecuteEntryItemAction(EntryViewModel, ERpgInventoryContextAction::MoveToCarry, 1);

	case ERpgInventoryContextAction::Split:
		return SplitCount > 0 ? (RequestSplitDialogForSelectedCell() && ConfirmPendingSplit(SplitCount)) : RequestSplitDialogForSelectedCell();

	case ERpgInventoryContextAction::Rotate:
		return AddressSlot
			? DragDropCoordinator->RotateAddressSlotInPlace(AddressSlot)
			: DragDropCoordinator->RotateEntryInPlace(EntryViewModel);

	case ERpgInventoryContextAction::Transfer:
		return QuickTransferSelectedCell();

	case ERpgInventoryContextAction::Drop:
		return DropSelectedCell();

	case ERpgInventoryContextAction::QuickAccessBind:
		return DragDropCoordinator->BindPayloadToQuickAccessSlot(MakePayloadFromSelectedItem(), QuickAccessSlotIndex);

	case ERpgInventoryContextAction::QuickAccessUnbind:
		return DragDropCoordinator->ClearQuickAccessBindingForPayload(MakePayloadFromSelectedItem());

	case ERpgInventoryContextAction::OpenContainer:
	case ERpgInventoryContextAction::Inspect:
		BP_OnDeferredInventoryContextAction(Action, Item, QuickAccessSlotIndex);
		return true;

	default:
		return false;
	}
}

int32 URpgInventorySpatialGridWidget::GetSelectedQuickAccessSlotIndex() const
{
	return DragDropCoordinator
		? DragDropCoordinator->FindQuickAccessSlotForPayload(MakePayloadFromSelectedItem())
		: INDEX_NONE;
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

	URpgInventoryAddressSlotViewModel* AddressSlot =
		GetSelectedAddressSlot();
	URpgInventoryEntryViewModel* EntryViewModel =
		GetSelectedEntryViewModel();
	const bool bCanDrop = AddressSlot
		? DragDropCoordinator->CanExecuteContextAction(
			AddressSlot,
			ERpgInventoryContextAction::Drop,
			true)
		: DragDropCoordinator->CanExecuteContextAction(
			EntryViewModel,
			ERpgInventoryContextAction::Drop,
			true);
	if (!bCanDrop)
	{
		return false;
	}

	if (!bConfirmed && InventoryPresentationHost)
	{
		return InventoryPresentationHost->RequestInventoryDrop(
			this,
			StackCount);
	}

	if (AddressSlot)
	{
		return DragDropCoordinator->DropAddressSlot(AddressSlot, StackCount, bConfirmed);
	}

	return DragDropCoordinator->DropEntry(EntryViewModel, StackCount, bConfirmed);
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
	if (bHasLastPointerPreviewScreenPosition)
	{
		PreviewPayloadAtScreenPosition(DragDropCoordinator->GetHeldPayload(), LastPointerPreviewScreenPosition);
	}
	else
	{
		UpdateCellVisualStates();
	}
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

FRpgInventoryItemId URpgInventorySpatialGridWidget::GetSelectedItemId() const
{
	const URpgInventoryItemInstance* ItemInstance = GetSelectedItemInstance();
	return ItemInstance
		? ItemInstance->GetItemId()
		: FRpgInventoryItemId();
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

bool URpgInventorySpatialGridWidget::SelectCellFromPointerHover(
	FVector2D ScreenPosition,
	const FPointerEvent& PointerEvent,
	APlayerController* OwningPlayer)
{
	const UCommonInputSubsystem* CommonInputSubsystem =
		UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	if (!CommonInputSubsystem || !CommonInputSubsystem->IsUsingPointerInput() ||
		PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return false;
	}

	if (DragDropCoordinator)
	{
		if (DragDropCoordinator->HasHeldPayload())
		{
			return false;
		}

		const URpgInventoryInteractionSession* Session =
			DragDropCoordinator->GetInteractionSession();
		if (Session && Session->IsRequestPending())
		{
			return false;
		}
	}

	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	if (!TryGetCellFromScreenPosition(ScreenPosition, CellX, CellY))
	{
		return false;
	}

	const bool bPanelAlreadyActive = PanelNavigationCoordinator
		? PanelNavigationCoordinator->GetActiveSpatialGridWidget() == this
		: bInventoryPanelActive;
	const bool bGridAlreadyFocused = !OwningPlayer || HasUserFocus(OwningPlayer);
	if (CursorX == CellX && CursorY == CellY && bPanelAlreadyActive && bGridAlreadyFocused)
	{
		return true;
	}

	return SelectCell(CellX, CellY, OwningPlayer);
}

bool URpgInventorySpatialGridWidget::CommitPayloadToCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y)
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	if (!DragDropCoordinator || !IsValidCell(X, Y))
	{
		return false;
	}

	FRpgInventorySpatialPreviewDescriptor Descriptor;
	Descriptor.bValid = true;
	Descriptor.EntryId = ResolvedPayload.EntryId;
	Descriptor.TargetPlacement = MakeTargetPlacementForCell(ResolvedPayload, X, Y);
	Descriptor.Target = MakeDropTargetForPlacement(ResolvedPayload, Descriptor.TargetPlacement);
	const FRpgInventoryInteractionPreviewPlan PreviewPlan =
		DragDropCoordinator->PlanInteractionPreview(
			ResolvedPayload,
			Descriptor.Target);
	Descriptor.PreviewState = PreviewPlan.State;
	if (PreviewPlan.ResolvedTargetPlacement.IsValid())
	{
		Descriptor.TargetPlacement =
			PreviewPlan.ResolvedTargetPlacement;
	}
	Descriptor.SnappedLocalPosition = GetCellPosition(Descriptor.TargetPlacement.X, Descriptor.TargetPlacement.Y);
	Descriptor.SnappedLocalSize = GetPlacementSize(Descriptor.TargetPlacement);
	if (URpgInventoryInteractionSession* Session = DragDropCoordinator->GetInteractionSession())
	{
		Session->SetSpatialPreviewDescriptor(Descriptor);
	}
	return DragDropCoordinator->CommitPlannedPayloadToTarget(
		ResolvedPayload,
		Descriptor.Target,
		PreviewPlan);
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

	if (!DragDropCoordinator)
	{
		return false;
	}

	FRpgInventorySpatialPreviewDescriptor Descriptor;
	Descriptor.bValid = true;
	Descriptor.EntryId = ResolvedPayload.EntryId;
	Descriptor.TargetPlacement = MakeTargetPlacementForCell(ResolvedPayload, X, Y);
	Descriptor.Target = MakeDropTargetForPlacement(ResolvedPayload, Descriptor.TargetPlacement);
	const FRpgInventoryInteractionPreviewPlan PreviewPlan =
		DragDropCoordinator->PlanInteractionPreview(
			ResolvedPayload,
			Descriptor.Target);
	Descriptor.PreviewState = PreviewPlan.State;
	if (PreviewPlan.ResolvedTargetPlacement.IsValid())
	{
		Descriptor.TargetPlacement =
			PreviewPlan.ResolvedTargetPlacement;
	}
	Descriptor.SnappedLocalPosition = GetCellPosition(Descriptor.TargetPlacement.X, Descriptor.TargetPlacement.Y);
	Descriptor.SnappedLocalSize = GetPlacementSize(Descriptor.TargetPlacement);
	DragDropCoordinator->PublishInteractionPreview(
		ResolvedPayload,
		Descriptor.Target,
		PreviewPlan);
	if (URpgInventoryInteractionSession* Session = DragDropCoordinator->GetInteractionSession())
	{
		Session->SetSpatialPreviewDescriptor(Descriptor);
	}
	else
	{
		HandleSpatialPreviewChanged(Descriptor);
	}
	return PreviewPlan.IsAccepted();
}

bool URpgInventorySpatialGridWidget::CommitPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition)
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	FRpgInventorySpatialPreviewDescriptor Descriptor;
	FRpgInventoryInteractionPreviewPlan PreviewPlan;
	if (!ResolveSpatialPreviewDescriptorAtScreenPosition(
			ResolvedPayload,
			ScreenPosition,
			Descriptor,
			&PreviewPlan))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	if (!DragDropCoordinator)
	{
		return false;
	}
	if (URpgInventoryInteractionSession* Session = DragDropCoordinator->GetInteractionSession())
	{
		Session->SetSpatialPreviewDescriptor(Descriptor);
	}
	return DragDropCoordinator->CommitPlannedPayloadToTarget(
		ResolvedPayload,
		Descriptor.Target,
		PreviewPlan);
}

bool URpgInventorySpatialGridWidget::PreviewPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition)
{
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	FRpgInventorySpatialPreviewDescriptor Descriptor;
	FRpgInventoryInteractionPreviewPlan PreviewPlan;
	if (!ResolveSpatialPreviewDescriptorAtScreenPosition(
			ResolvedPayload,
			ScreenPosition,
			Descriptor,
			&PreviewPlan))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	LastPointerPreviewScreenPosition = ScreenPosition;
	bHasLastPointerPreviewScreenPosition = true;
	if (!DragDropCoordinator)
	{
		HandleSpatialPreviewChanged(Descriptor);
		return true;
	}

	DragDropCoordinator->PublishInteractionPreview(
		ResolvedPayload,
		Descriptor.Target,
		PreviewPlan);
	if (URpgInventoryInteractionSession* Session = DragDropCoordinator->GetInteractionSession())
	{
		Session->SetSpatialPreviewDescriptor(Descriptor);
	}
	else
	{
		HandleSpatialPreviewChanged(Descriptor);
	}
	return true;
}

bool URpgInventorySpatialGridWidget::CanAddressPayloadAtScreenPosition(
	const FRpgInventoryDragPayload& Payload,
	FVector2D ScreenPosition) const
{
	FRpgInventorySpatialPreviewDescriptor Descriptor;
	return ResolveSpatialTargetGeometryAtScreenPosition(
		Payload,
		ScreenPosition,
		Descriptor);
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

	FRpgInventorySpatialPreviewDescriptor Descriptor;
	if (!ResolveSpatialTargetGeometryAtScreenPosition(
			Payload,
			ScreenPosition,
			Descriptor))
	{
		return false;
	}

	OutTarget = Descriptor.Target;
	OutTargetPlacement = Descriptor.TargetPlacement;
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	const bool bTargetRotated = DragDropCoordinator
		? DragDropCoordinator->GetTargetRotationForPayload(ResolvedPayload)
		: Descriptor.TargetPlacement.bRotated;
	const FIntPoint GrabCell = ResolvedPayload.DragAnchor.bValid
		? ResolvedPayload.DragAnchor.GrabbedCell
		: ClampSpatialGrabOffset(ResolvedPayload, bTargetRotated);
	OutAnchorX = Descriptor.TargetPlacement.X + GrabCell.X;
	OutAnchorY = Descriptor.TargetPlacement.Y + GrabCell.Y;
	return true;
}

bool URpgInventorySpatialGridWidget::ResolveSpatialPreviewDescriptorAtScreenPosition(
	const FRpgInventoryDragPayload& Payload,
	FVector2D ScreenPosition,
	FRpgInventorySpatialPreviewDescriptor& OutDescriptor,
	FRpgInventoryInteractionPreviewPlan* OutPreviewPlan) const
{
	if (OutPreviewPlan)
	{
		*OutPreviewPlan = FRpgInventoryInteractionPreviewPlan();
	}
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	if (!ResolveSpatialTargetGeometryAtScreenPosition(
			ResolvedPayload,
			ScreenPosition,
			OutDescriptor))
	{
		return false;
	}

	FRpgInventoryInteractionPreviewPlan PreviewPlan;
	PreviewPlan.State = ERpgInventoryInteractionPreviewState::Blocked;
	if (DragDropCoordinator)
	{
		PreviewPlan = DragDropCoordinator->PlanInteractionPreview(
			ResolvedPayload,
			OutDescriptor.Target);
	}
	OutDescriptor.PreviewState = PreviewPlan.State;
	if (PreviewPlan.ResolvedTargetPlacement.IsValid())
	{
		OutDescriptor.TargetPlacement =
			PreviewPlan.ResolvedTargetPlacement;
	}
	OutDescriptor.SnappedLocalPosition = GetCellPosition(
		OutDescriptor.TargetPlacement.X,
		OutDescriptor.TargetPlacement.Y);
	OutDescriptor.SnappedLocalSize = GetPlacementSize(
		OutDescriptor.TargetPlacement);
	if (OutPreviewPlan)
	{
		*OutPreviewPlan = MoveTemp(PreviewPlan);
	}
	return true;
}

bool URpgInventorySpatialGridWidget::
	ResolveSpatialTargetGeometryAtScreenPosition(
		const FRpgInventoryDragPayload& Payload,
		FVector2D ScreenPosition,
		FRpgInventorySpatialPreviewDescriptor& OutDescriptor) const
{
	OutDescriptor = FRpgInventorySpatialPreviewDescriptor();
	const FRpgInventoryDragPayload ResolvedPayload = DragDropCoordinator
		? DragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(
			ResolvedPayload) ||
		!GridSize.IsValid())
	{
		return false;
	}

	const bool bTargetRotated = DragDropCoordinator
		? DragDropCoordinator->GetTargetRotationForPayload(ResolvedPayload)
		: (ResolvedPayload.SourcePlacement.IsValid() && ResolvedPayload.SourcePlacement.bRotated);
	const FGeometry GridGeometry = GetGridInteractionGeometry();
	const FVector2D LocalPointer = GridGeometry.AbsoluteToLocal(ScreenPosition);
	const FVector2D GrabPixels = URpgInventoryDragDropCoordinator::ResolveTargetGrabPixels(
		ResolvedPayload,
		bTargetRotated,
		CellSize,
		CellPadding);
	const FVector2D UnsnappedTopLeft = LocalPointer - GrabPixels;
	const FRpgInventoryGridSize Footprint = GetPayloadUnrotatedFootprint(ResolvedPayload);
	if (!Footprint.IsValid())
	{
		return false;
	}
	FRpgInventoryGridPlacement SizePlacement = MakeCellPlacement(
		ResolveContainerHandle(), 0, 0, bTargetRotated, Footprint.Width, Footprint.Height);
	const FVector2D GhostSize = GetPlacementSize(SizePlacement);
	const FVector2D GhostCenter = UnsnappedTopLeft + GhostSize * 0.5f;
	const FVector2D LocalGridSize = GetGridDesiredLocalSize(GridSize, CellSize, CellPadding);
	if (GhostCenter.X < 0.0f || GhostCenter.Y < 0.0f ||
		GhostCenter.X > LocalGridSize.X || GhostCenter.Y > LocalGridSize.Y)
	{
		return false;
	}

	const float Stride = FMath::Max(1.0f, CellSize + CellPadding);
	int32 OriginX = FMath::RoundToInt(UnsnappedTopLeft.X / Stride);
	int32 OriginY = FMath::RoundToInt(UnsnappedTopLeft.Y / Stride);
	if (ActiveSpatialPreview.bValid &&
		ActiveSpatialPreview.TargetPlacement.GetContainerHandle() == ResolveContainerHandle())
	{
		OriginX = ApplyOriginSnapHysteresis(
			UnsnappedTopLeft.X,
			Stride,
			OriginX,
			ActiveSpatialPreview.TargetPlacement.X,
			SnapHysteresisFraction);
		OriginY = ApplyOriginSnapHysteresis(
			UnsnappedTopLeft.Y,
			Stride,
			OriginY,
			ActiveSpatialPreview.TargetPlacement.Y,
			SnapHysteresisFraction);
	}
	OutDescriptor.bValid = true;
	OutDescriptor.EntryId = ResolvedPayload.EntryId;
	OutDescriptor.TargetPlacement = MakeCellPlacement(
		ResolveContainerHandle(), OriginX, OriginY, bTargetRotated, Footprint.Width, Footprint.Height);
	OutDescriptor.Target = MakeDropTargetForPlacement(ResolvedPayload, OutDescriptor.TargetPlacement);
	OutDescriptor.PreviewState =
		ERpgInventoryInteractionPreviewState::None;
	OutDescriptor.SnappedLocalPosition = GetCellPosition(
		OutDescriptor.TargetPlacement.X,
		OutDescriptor.TargetPlacement.Y);
	OutDescriptor.SnappedLocalSize = GetPlacementSize(
		OutDescriptor.TargetPlacement);
	OutDescriptor.PointerScreenPosition = ScreenPosition;
	return true;
}

void URpgInventorySpatialGridWidget::ClearExternalPreviewPayload()
{
	if (DragDropCoordinator)
	{
		if (URpgInventoryInteractionSession* Session = DragDropCoordinator->GetInteractionSession())
		{
			const FRpgInventorySpatialPreviewDescriptor Descriptor = Session->GetSpatialPreviewDescriptor();
			if (!Session->IsRequestPending() && Descriptor.bValid &&
				Descriptor.TargetPlacement.GetContainerHandle() == ResolveContainerHandle())
			{
				Session->ClearSpatialPreviewDescriptor();
				Session->ClearPreviewTarget();
			}
		}
	}
	ClearSpatialPreviewLocal();
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
	ReleaseInventoryPresentation();

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
	return false;
}

bool URpgInventorySpatialGridWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

void URpgInventorySpatialGridWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Pointer leave is not equivalent to ghost leave when an item was grabbed near an edge.
	// The screen-level router or drag cancellation clears the active target once the ghost center leaves.
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

FReply URpgInventorySpatialGridWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	bSelectionVisualSuppressed = false;
	NotifySelectionChanged();
	UpdateCellVisualStates();
	return FReply::Handled();
}

void URpgInventorySpatialGridWidget::RefreshFromPanelViewModel()
{
	UpdateGridSizeFromBinding();
	ObserveEntryDelegates();
	RebuildItemOverlay();
	ReplanActivePointerPreview();
}

void URpgInventorySpatialGridWidget::HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	RebuildItemOverlay();
	ReplanActivePointerPreview();
	if (PanelNavigationCoordinator && PanelNavigationCoordinator->GetActiveSpatialGridWidget() == this)
	{
		PanelNavigationCoordinator->OnActiveSelectionChanged.Broadcast();
	}
}

void URpgInventorySpatialGridWidget::HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	RebuildItemOverlay();
	ReplanActivePointerPreview();
	if (PanelNavigationCoordinator && PanelNavigationCoordinator->GetActiveSpatialGridWidget() == this)
	{
		PanelNavigationCoordinator->OnActiveSelectionChanged.Broadcast();
	}
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
	if (!bHasHeldPayload)
	{
		bHasLastPointerPreviewScreenPosition = false;
		ClearSpatialPreviewLocal();
	}
	else if (ActiveSpatialPreview.bValid && bHasLastPointerPreviewScreenPosition)
	{
		// Rotation is a payload change, not pointer motion. Rebuild the one canonical descriptor immediately so
		// the snapped ghost and its exact footprint rotate without waiting for another mouse event.
		PreviewPayloadAtScreenPosition(HeldPayload, LastPointerPreviewScreenPosition);
		return;
	}
	UpdateCellVisualStates();
}

void URpgInventorySpatialGridWidget::ReplanActivePointerPreview()
{
	if (!ActiveSpatialPreview.bValid ||
		!bHasLastPointerPreviewScreenPosition ||
		!DragDropCoordinator)
	{
		return;
	}

	URpgInventoryInteractionSession* Session =
		DragDropCoordinator->GetInteractionSession();
	if (!Session || !Session->HasPayload() || Session->IsRequestPending())
	{
		return;
	}

	// Inventory replication may change occupancy while the pointer is stationary. Re-evaluate the exact domain query
	// so the ghost, cell colors, and next commit never retain stale Merge/Swap/capacity semantics.
	PreviewPayloadAtScreenPosition(
		Session->GetPayload(),
		LastPointerPreviewScreenPosition);
}

void URpgInventorySpatialGridWidget::HandleSpatialPreviewChanged(const FRpgInventorySpatialPreviewDescriptor& Descriptor)
{
	const bool bTargetsThisGrid = Descriptor.bValid &&
		Descriptor.TargetPlacement.GetContainerHandle() == ResolveContainerHandle();
	if (!bTargetsThisGrid)
	{
		ClearSpatialPreviewLocal();
		return;
	}

	if (ActiveSpatialPreview.IsEquivalentTo(Descriptor))
	{
		return;
	}

	ActiveSpatialPreview = Descriptor;
	LastPointerPreviewScreenPosition = Descriptor.PointerScreenPosition;
	bHasLastPointerPreviewScreenPosition = true;
	UpdateCellVisualStates();
	UpdateSpatialPreviewGhost();
}

void URpgInventorySpatialGridWidget::ClearSpatialPreviewLocal()
{
	if (!ActiveSpatialPreview.bValid && (!SpatialPreviewGhost || SpatialPreviewGhost->GetVisibility() == ESlateVisibility::Collapsed))
	{
		return;
	}

	ActiveSpatialPreview = FRpgInventorySpatialPreviewDescriptor();
	if (SpatialPreviewGhost)
	{
		SpatialPreviewGhost->SetVisibility(ESlateVisibility::Collapsed);
	}
	UpdateCellVisualStates();
}

URpgInventoryDragVisualWidget* URpgInventorySpatialGridWidget::EnsureSpatialPreviewGhost()
{
	if (!PreviewCanvas || !SpatialPreviewWidgetClass)
	{
		return nullptr;
	}

	TSubclassOf<URpgInventoryDragVisualWidget> GhostClass = SpatialPreviewWidgetClass;
	if (!SpatialPreviewGhost || SpatialPreviewGhost->GetClass() != GhostClass.Get())
	{
		if (SpatialPreviewGhost && SpatialPreviewGhost->GetParent())
		{
			SpatialPreviewGhost->RemoveFromParent();
		}
		SpatialPreviewGhost = CreateWidget<URpgInventoryDragVisualWidget>(this, GhostClass);
		bSpatialPreviewGhostConfigured = false;
	}
	if (!SpatialPreviewGhost)
	{
		return nullptr;
	}

	if (SpatialPreviewGhost->GetParent() != PreviewCanvas)
	{
		PreviewCanvas->AddChildToCanvas(SpatialPreviewGhost);
	}
	if (SpatialPreviewGhost->GetVisibility() != ESlateVisibility::HitTestInvisible)
	{
		SpatialPreviewGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return SpatialPreviewGhost;
}

void URpgInventorySpatialGridWidget::UpdateSpatialPreviewGhost()
{
	if (!ActiveSpatialPreview.bValid || !DragDropCoordinator || !DragDropCoordinator->HasHeldPayload())
	{
		if (SpatialPreviewGhost)
		{
			SpatialPreviewGhost->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	URpgInventoryDragVisualWidget* Ghost = EnsureSpatialPreviewGhost();
	if (!Ghost)
	{
		return;
	}

	const FRpgInventoryDragPayload Payload = DragDropCoordinator->GetHeldPayload();
	const bool bNeedsVisualConfiguration = !bSpatialPreviewGhostConfigured ||
		SpatialPreviewConfiguredItem.Get() != Payload.ItemInstance ||
		SpatialPreviewConfiguredEntryId != Payload.EntryId ||
		SpatialPreviewConfiguredFootprint != Payload.ItemFootprint ||
		SpatialPreviewConfiguredStackCount != Payload.StackCount ||
		SpatialPreviewConfiguredSourceType != Payload.SourceType ||
		!FMath::IsNearlyEqual(Ghost->GetConfiguredCellSize(), CellSize) ||
		!FMath::IsNearlyEqual(Ghost->GetConfiguredCellPadding(), CellPadding);
	if (bNeedsVisualConfiguration)
	{
		Ghost->ConfigureFromPayload(Payload, CellSize, CellPadding, ActiveSpatialPreview.PreviewState);
		SpatialPreviewConfiguredItem = Payload.ItemInstance.Get();
		SpatialPreviewConfiguredEntryId = Payload.EntryId;
		SpatialPreviewConfiguredFootprint = Payload.ItemFootprint;
		SpatialPreviewConfiguredStackCount = Payload.StackCount;
		SpatialPreviewConfiguredSourceType = Payload.SourceType;
		bSpatialPreviewGhostConfigured = true;
	}
	else
	{
		Ghost->SetPreviewState(ActiveSpatialPreview.PreviewState);
	}
	Ghost->SetFootprintRotated(ActiveSpatialPreview.TargetPlacement.bRotated);
	if (Ghost->GetVisibility() != ESlateVisibility::HitTestInvisible)
	{
		Ghost->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Ghost->SetRenderTranslation(ActiveSpatialPreview.SnappedLocalPosition);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Ghost->Slot))
	{
		if (CanvasSlot->GetAutoSize())
		{
			CanvasSlot->SetAutoSize(false);
		}
		if (!CanvasSlot->GetPosition().IsNearlyZero())
		{
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
		}
		if (!CanvasSlot->GetSize().Equals(ActiveSpatialPreview.SnappedLocalSize))
		{
			CanvasSlot->SetSize(ActiveSpatialPreview.SnappedLocalSize);
		}
		if (CanvasSlot->GetZOrder() != 100)
		{
			CanvasSlot->SetZOrder(100);
		}
	}
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
	if (PreviewCanvas)
	{
		PreviewCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
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
		ContainerHandle = GroupViewModel->GetContainerHandle();
	}
	else if (Inventory)
	{
		FRpgInventoryGridSize ResolvedGridSize;
		const FRpgInventoryContainerHandle ResolvedContainerHandle = ResolveContainerHandle();
		if (ResolvedContainerHandle.IsValid() &&
			Inventory->GetGridSizeForContainerHandle(ResolvedContainerHandle, ResolvedGridSize))
		{
			GridSize = ResolvedGridSize;
		}
		else
		{
			GridSize.Width = 0;
			GridSize.Height = 0;
		}
		ContainerHandle = ResolvedContainerHandle;
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
				CellWidget = CreateWidget<URpgInventorySpatialCellWidget>(this, SpatialCellWidgetClass);
			}
			if (!CellWidget)
			{
				continue;
			}

			CellWidget->SetOwningSpatialGrid(this, X, Y);
			CellWidget->SetCellViewModels(FindAddressCell(X, Y), FindEntryAtCell(X, Y));
			CellWidget->SetCellVisualState(
				ERpgInventorySpatialCellVisualState::Normal);

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
	UpdateCellVisualStates();
}

void URpgInventorySpatialGridWidget::UpdateCellVisualStates()
{
	FRpgInventorySpatialPreviewDescriptor PreviewDescriptor;
	BuildCellPreviewDescriptor(PreviewDescriptor);
	for (URpgInventorySpatialCellWidget* CellWidget : CellWidgets)
	{
		if (!CellWidget)
		{
			continue;
		}

		const int32 X = CellWidget->GetCellX();
		const int32 Y = CellWidget->GetCellY();
		CellWidget->SetCellViewModels(FindAddressCell(X, Y), FindEntryAtCell(X, Y));
		CellWidget->SetCellVisualState(
			GetCellVisualState(X, Y, PreviewDescriptor));
	}
}

ERpgInventorySpatialCellVisualState
URpgInventorySpatialGridWidget::GetCellVisualState(
	int32 X,
	int32 Y,
	const FRpgInventorySpatialPreviewDescriptor& PreviewDescriptor) const
{
	if (!IsValidCell(X, Y))
	{
		return ERpgInventorySpatialCellVisualState::Normal;
	}

	const bool bIsSelectedCell = X == CursorX && Y == CursorY;
	const bool bCanShowCursor = bInventoryPanelActive && !bSelectionVisualSuppressed && bIsSelectedCell;
	if (PreviewDescriptor.bValid)
	{
		ERpgInventorySpatialCellVisualState PreviewCellState = ERpgInventorySpatialCellVisualState::Normal;
		if (ResolvePayloadPreviewCellState(
				PreviewDescriptor,
				X,
				Y,
				PreviewCellState))
		{
			return PreviewCellState;
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

bool URpgInventorySpatialGridWidget::BuildCellPreviewDescriptor(
	FRpgInventorySpatialPreviewDescriptor& OutDescriptor) const
{
	OutDescriptor = ActiveSpatialPreview;
	if (OutDescriptor.bValid || !bInventoryPanelActive ||
		!DragDropCoordinator || !DragDropCoordinator->HasHeldPayload() ||
		!IsValidCell(CursorX, CursorY))
	{
		return OutDescriptor.bValid;
	}

	const URpgInventoryInteractionSession* Session =
		DragDropCoordinator->GetInteractionSession();
	if (!Session || Session->GetInputMode() !=
		ERpgInventoryInteractionInputMode::Controller)
	{
		return false;
	}

	const FRpgInventoryDragPayload Payload =
		DragDropCoordinator->GetHeldPayload();
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return false;
	}

	OutDescriptor.bValid = true;
	OutDescriptor.EntryId = Payload.EntryId;
	OutDescriptor.TargetPlacement =
		MakeTargetPlacementForCell(Payload, CursorX, CursorY);
	OutDescriptor.Target = MakeDropTargetForPlacement(
		Payload,
		OutDescriptor.TargetPlacement);
	const FRpgInventoryInteractionPreviewPlan PreviewPlan =
		DragDropCoordinator->PlanInteractionPreview(
			Payload,
			OutDescriptor.Target);
	OutDescriptor.PreviewState = PreviewPlan.State;
	if (PreviewPlan.ResolvedTargetPlacement.IsValid())
	{
		OutDescriptor.TargetPlacement =
			PreviewPlan.ResolvedTargetPlacement;
	}
	return true;
}

bool URpgInventorySpatialGridWidget::ResolvePayloadPreviewCellState(
	const FRpgInventorySpatialPreviewDescriptor& PreviewDescriptor,
	int32 X,
	int32 Y,
	ERpgInventorySpatialCellVisualState& OutState) const
{
	OutState = ERpgInventorySpatialCellVisualState::Normal;
	if (!IsValidCell(X, Y) || !PreviewDescriptor.bValid)
	{
		return false;
	}

	const FRpgInventoryGridPlacement& TargetPlacement =
		PreviewDescriptor.TargetPlacement;
	const ERpgInventoryInteractionPreviewState SemanticState =
		PreviewDescriptor.PreviewState;
	const bool bIsPreviewCell =
		PlacementFootprintContainsCellUnchecked(TargetPlacement, X, Y);
	if (!bIsPreviewCell)
	{
		return false;
	}

	switch (SemanticState)
	{
	case ERpgInventoryInteractionPreviewState::Pending:
		OutState = ERpgInventorySpatialCellVisualState::PendingPreview;
		break;
	case ERpgInventoryInteractionPreviewState::Rejected:
		OutState = ERpgInventorySpatialCellVisualState::RejectedPreview;
		break;
	case ERpgInventoryInteractionPreviewState::Blocked:
	case ERpgInventoryInteractionPreviewState::OutOfBounds:
	case ERpgInventoryInteractionPreviewState::None:
		OutState = ERpgInventorySpatialCellVisualState::InvalidPreview;
		break;
	default:
		OutState = ERpgInventorySpatialCellVisualState::ValidPreview;
		break;
	}
	return true;
}

void URpgInventorySpatialGridWidget::RebuildItemOverlay()
{
	EnsureRuntimeWidgets();
	if (!ItemCanvas || !SpatialItemWidgetClass)
	{
		for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
		{
			if (ItemWidget)
			{
				ItemWidget->ReleaseSpatialItemState();
			}
		}
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
			ItemWidget = CreateWidget<URpgInventorySpatialItemWidget>(this, SpatialItemWidgetClass);
		}
		if (!ItemWidget)
		{
			return;
		}

		ItemWidget->SetVisibility(ESlateVisibility::Visible);
		ItemWidget->SetIsEnabled(true);
		// Reconciliation changes several presentation inputs for a reused widget. Defer its semantic target query until
		// the complete candidate is bound so one rebuild performs exactly one plan without retaining gameplay truth.
		ItemWidget->BeginDragDropVisualRefreshBatch();
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
		ItemWidget->EndDragDropVisualRefreshBatch();

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
			PreviousItem->ReleaseSpatialItemState();
			ItemCanvas->RemoveChild(PreviousItem);
		}
	}
	ItemWidgets = MoveTemp(ReconciledItems);
	ApplyEntryDimming();
	UpdateCellVisualStates();
}

void URpgInventorySpatialGridWidget::ApplyEntryDimming()
{
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetEntryFilterOpacity(
			IsEntryDimmed(ItemWidget->GetRepresentedEntryId())
				? DimmedEntryOpacity
				: 1.0f);
	}
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
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(ResolvedPayload))
	{
		return FRpgInventoryGridPlacement();
	}

	const bool bTargetRotated = DragDropCoordinator
		? DragDropCoordinator->GetTargetRotationForPayload(ResolvedPayload)
		: (ResolvedPayload.SourcePlacement.IsValid() && ResolvedPayload.SourcePlacement.bRotated);
	const FIntPoint GrabOffset = ClampSpatialGrabOffset(ResolvedPayload, bTargetRotated);
	const FRpgInventoryGridSize Footprint = GetPayloadUnrotatedFootprint(ResolvedPayload);
	if (!Footprint.IsValid())
	{
		return FRpgInventoryGridPlacement();
	}

	return MakeCellPlacement(
		ResolveContainerHandle(),
		X - GrabOffset.X,
		Y - GrabOffset.Y,
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
	return ContainerHandle;
}

bool URpgInventorySpatialGridWidget::IsValidCell(int32 X, int32 Y) const
{
	return GridSize.IsValid() &&
		X >= 0 &&
		Y >= 0 &&
		X < GridSize.Width &&
		Y < GridSize.Height;
}
