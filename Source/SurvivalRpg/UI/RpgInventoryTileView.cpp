#include "RpgInventoryTileView.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "Slate/UMGDragDropOp.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySlotEntryWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryTileView)

URpgInventoryTileView::URpgInventoryTileView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDragDrop = true;
	DragDropOperationClass = URpgInventoryDragDropOperation::StaticClass();
}

void URpgInventoryTileView::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{ 
	DragDropCoordinator = InCoordinator;

	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyCoordinatorToEntry(EntryWidget);
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgInventoryTileView::SetInventoryEntryItems(const TArray<URpgInventoryEntryViewModel*>& InEntries)
{
	const TArray<UObject*>& CurrentListItems = GetListItems();
	if (CurrentListItems.Num() == InEntries.Num())
	{
		bool bSameItems = true;
		for (int32 Index = 0; Index < InEntries.Num(); ++Index)
		{
			if (CurrentListItems[Index] != InEntries[Index])
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

	TArray<UObject*> NewListItems;
	NewListItems.Reserve(InEntries.Num());
	for (URpgInventoryEntryViewModel* Entry : InEntries)
	{
		NewListItems.Add(Entry);
	}

	SetListItems(NewListItems);
	RequestRefresh();
}

void URpgInventoryTileView::BindInventoryPanelViewModel(URpgInventoryPanelViewModel* InPanelViewModel)
{
	if (BoundPanelViewModel == InPanelViewModel)
	{
		RefreshInventoryEntryItems();
		return;
	}

	if (BoundPanelViewModel)
	{
		BoundPanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::RefreshInventoryEntryItems);
	}

	BoundPanelViewModel = InPanelViewModel;
	if (BoundPanelViewModel)
	{
		BoundPanelViewModel->OnEntriesChanged.AddUniqueDynamic(this, &ThisClass::RefreshInventoryEntryItems);
	}

	RefreshInventoryEntryItems();
}

void URpgInventoryTileView::RefreshInventoryEntryItems()
{
	if (!BoundPanelViewModel)
	{
		SetInventoryEntryItems(TArray<URpgInventoryEntryViewModel*>());
		return;
	}

	SetInventoryEntryItems(BoundPanelViewModel->GetEntries());
}

URpgInventoryEntryViewModel* URpgInventoryTileView::GetSelectedInventoryEntry() const
{
	return Cast<URpgInventoryEntryViewModel>(GetSelectedItem());
}

bool URpgInventoryTileView::SelectInventoryListItem(UObject* Item, APlayerController* OwningPlayer)
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

bool URpgInventoryTileView::SelectBestInventoryEntry(APlayerController* OwningPlayer, bool bPreferOccupiedSlot)
{
	const TArray<UObject*>& Items = GetListItems();
	if (Items.IsEmpty())
	{
		return false;
	}

	UObject* DesiredItem = GetSelectedItem();
	if (!DesiredItem || !Items.Contains(DesiredItem))
	{
		DesiredItem = nullptr;
	}

	if (!DesiredItem && bPreferOccupiedSlot)
	{
		for (UObject* Item : Items)
		{
			const URpgInventoryEntryViewModel* Entry = Cast<URpgInventoryEntryViewModel>(Item);
			if (Entry && !Entry->IsEmptySlot())
			{
				DesiredItem = Item;
				break;
			}
		}
	}

	if (!DesiredItem)
	{
		DesiredItem = Items[0];
	}

	return SelectInventoryListItem(DesiredItem, OwningPlayer);
}

bool URpgInventoryTileView::SelectInventoryEntryByIdentity(FGuid EntryId, int32 SlotIndex, APlayerController* OwningPlayer)
{
	const TArray<UObject*>& Items = GetListItems();
	if (Items.IsEmpty())
	{
		return false;
	}

	if (EntryId.IsValid())
	{
		for (UObject* Item : Items)
		{
			const URpgInventoryEntryViewModel* Entry = Cast<URpgInventoryEntryViewModel>(Item);
			if (Entry && !Entry->IsEmptySlot() && Entry->GetEntryId() == EntryId)
			{
				return SelectInventoryListItem(Item, OwningPlayer);
			}
		}
	}

	if (SlotIndex != INDEX_NONE)
	{
		for (UObject* Item : Items)
		{
			const URpgInventoryEntryViewModel* Entry = Cast<URpgInventoryEntryViewModel>(Item);
			if (Entry && Entry->GetSlotIndex() == SlotIndex)
			{
				return SelectInventoryListItem(Item, OwningPlayer);
			}
		}
	}

	return false;
}

void URpgInventoryTileView::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgInventoryTileView::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

void URpgInventoryTileView::ClearInventorySelectionVisual()
{
	const bool bWasSuppressingPanelSelectionNotify = bSuppressPanelSelectionNotify;
	bSuppressPanelSelectionNotify = true;
	ITypedUMGListView<UObject*>::ClearSelection();
	bSuppressPanelSelectionNotify = bWasSuppressingPanelSelectionNotify;
}

bool URpgInventoryTileView::QuickTransferSelectedEntry(URpgInventoryManagerComponent* ExplicitTargetInventory)
{
	return DragDropCoordinator && DragDropCoordinator->QuickTransferEntry(GetSelectedInventoryEntry(), ExplicitTargetInventory);
}

bool URpgInventoryTileView::QuickSplitSelectedEntry(int32 SplitCount, int32 TargetSlotIndex)
{
	return DragDropCoordinator && DragDropCoordinator->QuickSplitEntry(GetSelectedInventoryEntry(), FRpgInventoryGridPlacement(), SplitCount);
}

bool URpgInventoryTileView::UseOrEquipSelectedEntry(int32 StackCount)
{
	return DragDropCoordinator && DragDropCoordinator->UseOrEquipEntry(GetSelectedInventoryEntry(), StackCount);
}

bool URpgInventoryTileView::DropSelectedEntry(int32 StackCount, bool bConfirmed)
{
	return DragDropCoordinator && DragDropCoordinator->DropEntry(GetSelectedInventoryEntry(), StackCount, bConfirmed);
}

void URpgInventoryTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);

	ApplyCoordinatorToEntry(EntryWidget);
	ApplyPanelActiveStateToEntry(EntryWidget);
}

UDragDropOperation* URpgInventoryTileView::HandleListEntryDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UUserWidget& EntryWidget)
{
	UObject* ListItem = GetListObjectFromEntry(EntryWidget);
	URpgInventoryEntryViewModel* EntryViewModel = Cast<URpgInventoryEntryViewModel>(ListItem);
	if (!DragDropCoordinator || !EntryViewModel)
	{
		return nullptr;
	}

	const FRpgInventoryDragPayload Payload = URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromEntry(EntryViewModel);
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return nullptr;
	}

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		return nullptr;
	}

	InventoryOperation->Pivot = DragDropVisualPivot;
	InventoryOperation->Offset = DragDropVisualOffset;
	InventoryOperation->Payload = ListItem;

	if (DragVisualWidget)
	{
		InventoryOperation->DefaultDragVisual = DragVisualWidget;
	}
	else
	{
		const TSubclassOf<UUserWidget> DragVisualClass = DragDropVisualEntryClass ? DragDropVisualEntryClass : EntryWidgetClass;
		if (DragVisualClass)
		{
			InventoryOperation->DefaultDragVisual = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass);
		}
	}

	InventoryOperation->InventoryPayload = Payload;
	return InventoryOperation;
}

TOptional<EItemDropZone> URpgInventoryTileView::HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
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

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakeInventoryTargetFromEntry(
		Cast<URpgInventoryEntryViewModel>(GetListObjectFromEntry(EntryWidget)));
	if (DragDropCoordinator->PreviewPayloadDrop(InventoryOperation->InventoryPayload, Target))
	{
		return DropZone;
	}

	return NullOpt;
}

FReply URpgInventoryTileView::HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return FReply::Unhandled();
	}

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakeInventoryTargetFromEntry(
		Cast<URpgInventoryEntryViewModel>(GetListObjectFromEntry(EntryWidget)));
	if (!DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, Target))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled().EndDragDrop();
}

void URpgInventoryTileView::OnItemClickedInternal(UObject* Item)
{
	Super::OnItemClickedInternal(Item);

	if (!bUseItemClickAsAccept || !DragDropCoordinator)
	{
		return;
	}

	DragDropCoordinator->HandleInventoryEntryAccept(Cast<URpgInventoryEntryViewModel>(Item));
}

void URpgInventoryTileView::OnSelectionChangedInternal(UObject* FirstSelectedItem)
{
	Super::OnSelectionChangedInternal(FirstSelectedItem);

	if (bSuppressPanelSelectionNotify || !PanelNavigationCoordinator || !FirstSelectedItem)
	{
		return;
	}

	PanelNavigationCoordinator->NotifyPanelSelectionChanged(this, FirstSelectedItem);
}

void URpgInventoryTileView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventorySlotEntryWidget* InventoryEntryWidget = Cast<URpgInventorySlotEntryWidget>(EntryWidget))
	{
		InventoryEntryWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventoryTileView::ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventorySlotEntryWidget* InventoryEntryWidget = Cast<URpgInventorySlotEntryWidget>(EntryWidget))
	{
		InventoryEntryWidget->SetInventoryPanelActive(bInventoryPanelActive);
	}
}
