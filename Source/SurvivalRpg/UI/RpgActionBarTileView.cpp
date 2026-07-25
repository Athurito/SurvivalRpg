#include "RpgActionBarTileView.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropOperation.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarSlotViewModel.h"
#include "SurvivalRpg/UI/RpgActionBarSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"

#include "Input/Reply.h"
#include "Slate/UMGDragDropOp.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarTileView)

namespace
{
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

bool URpgActionBarTileView::HasActionBarSlotAtScreenPosition(FVector2D ScreenPosition) const
{
	return FindActionBarSlotWidgetAtScreenPosition(ScreenPosition) != nullptr;
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
	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (InventoryOperation)
	{
		// Inventory pointer routing is screen-owned so the visible ghost center, preview, and commit
		// cannot disagree merely because Slate happened to hit a recycled ListView entry first.
		return NullOpt;
	}

	return NullOpt;
}

FReply URpgActionBarTileView::HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (InventoryOperation)
	{
		// Bubble to URpgPlayerInventoryWidget; it resolves the actionbar slot from the canonical ghost center.
		return FReply::Unhandled();
	}

	return FReply::Unhandled();
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
