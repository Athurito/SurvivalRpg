#include "RpgPlayerInventoryLayoutViews.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "MVVMSubsystem.h"
#include "Slate/UMGDragDropOp.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgActionBarSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryLayoutViews)

URpgInventoryAddressTileView::URpgInventoryAddressTileView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDragDrop = true;
	DragDropOperationClass = URpgInventoryDragDropOperation::StaticClass();
}

void URpgInventoryAddressTileView::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyCoordinatorToEntry(EntryWidget);
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgInventoryAddressTileView::SetAddressSlotItems(const TArray<URpgInventoryAddressSlotViewModel*>& InSlots)
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
	for (URpgInventoryAddressSlotViewModel* SlotViewModel : InSlots)
	{
		NewItems.Add(SlotViewModel);
	}

	SetListItems(NewItems);
	RequestRefresh();
}

void URpgInventoryAddressTileView::BindSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	BoundGroupViewModel = InGroupViewModel;
	RefreshAddressSlotItems();
}

URpgInventoryAddressSlotViewModel* URpgInventoryAddressTileView::GetSelectedAddressSlot() const
{
	return Cast<URpgInventoryAddressSlotViewModel>(GetSelectedItem());
}

bool URpgInventoryAddressTileView::SelectAddressListItem(UObject* Item, APlayerController* OwningPlayer)
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

bool URpgInventoryAddressTileView::SelectBestAddressSlot(APlayerController* OwningPlayer, bool bPreferOccupiedSlot)
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
			const URpgInventoryAddressSlotViewModel* AddressSlot = Cast<URpgInventoryAddressSlotViewModel>(Item);
			if (AddressSlot && !AddressSlot->IsEmptySlot())
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

	return SelectAddressListItem(DesiredItem, OwningPlayer);
}

bool URpgInventoryAddressTileView::SelectAddressSlotByIdentity(FGuid EntryId, int32 GlobalSlotIndex, APlayerController* OwningPlayer)
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
			const URpgInventoryAddressSlotViewModel* AddressSlot = Cast<URpgInventoryAddressSlotViewModel>(Item);
			if (AddressSlot && !AddressSlot->IsEmptySlot() && AddressSlot->GetEntryId() == EntryId)
			{
				return SelectAddressListItem(Item, OwningPlayer);
			}
		}
	}

	if (GlobalSlotIndex != INDEX_NONE)
	{
		for (UObject* Item : Items)
		{
			const URpgInventoryAddressSlotViewModel* AddressSlot = Cast<URpgInventoryAddressSlotViewModel>(Item);
			if (AddressSlot && AddressSlot->GetGlobalSlotIndex() == GlobalSlotIndex)
			{
				return SelectAddressListItem(Item, OwningPlayer);
			}
		}
	}

	return false;
}

void URpgInventoryAddressTileView::ClearAddressSelectionVisual()
{
	const bool bWasSuppressingPanelSelectionNotify = bSuppressPanelSelectionNotify;
	bSuppressPanelSelectionNotify = true;
	ITypedUMGListView<UObject*>::ClearSelection();
	bSuppressPanelSelectionNotify = bWasSuppressingPanelSelectionNotify;
}

void URpgInventoryAddressTileView::SetInventoryPanelActive(bool bInInventoryPanelActive)
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

void URpgInventoryAddressTileView::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

bool URpgInventoryAddressTileView::QuickSplitSelectedAddressSlot(int32 SplitCount, int32 TargetSlotIndex)
{
	return DragDropCoordinator && DragDropCoordinator->QuickSplitAddressSlot(GetSelectedAddressSlot(), FRpgInventoryGridPlacement(), SplitCount);
}

bool URpgInventoryAddressTileView::UseOrEquipSelectedAddressSlot(int32 StackCount)
{
	return DragDropCoordinator && DragDropCoordinator->UseOrEquipAddressSlot(GetSelectedAddressSlot(), StackCount);
}

bool URpgInventoryAddressTileView::DropSelectedAddressSlot(int32 StackCount, bool bConfirmed)
{
	return DragDropCoordinator && DragDropCoordinator->DropAddressSlot(GetSelectedAddressSlot(), StackCount, bConfirmed);
}

void URpgInventoryAddressTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
	ApplyPanelActiveStateToEntry(EntryWidget);
}

UDragDropOperation* URpgInventoryAddressTileView::HandleListEntryDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UUserWidget& EntryWidget)
{
	UObject* ListItem = GetListObjectFromEntry(EntryWidget);
	URpgInventoryAddressSlotViewModel* SlotViewModel = Cast<URpgInventoryAddressSlotViewModel>(ListItem);
	if (!SlotViewModel || !DragDropCoordinator || (!SlotViewModel->CanDrag() && !SlotViewModel->IsActionbarBindable()))
	{
		return nullptr;
	}

	const FRpgInventoryDragPayload Payload = URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(SlotViewModel);
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
			UUserWidget* DefaultDragVisual = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass);
			if (URpgInventoryAddressSlotWidget* AddressSlotDragVisual = Cast<URpgInventoryAddressSlotWidget>(DefaultDragVisual))
			{
				AddressSlotDragVisual->SetAddressSlotViewModel(SlotViewModel);
				AddressSlotDragVisual->SetDragDropCoordinator(DragDropCoordinator);
			}

			InventoryOperation->DefaultDragVisual = DefaultDragVisual;
		}
	}

	InventoryOperation->InventoryPayload = Payload;
	return InventoryOperation;
}

TOptional<EItemDropZone> URpgInventoryAddressTileView::HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
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

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(
		Cast<URpgInventoryAddressSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	return DragDropCoordinator->PreviewPayloadDrop(InventoryOperation->InventoryPayload, Target) ? TOptional<EItemDropZone>(DropZone) : NullOpt;
}

FReply URpgInventoryAddressTileView::HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return FReply::Unhandled();
	}

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(
		Cast<URpgInventoryAddressSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	if (!DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, Target))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled().EndDragDrop();
}

void URpgInventoryAddressTileView::OnSelectionChangedInternal(UObject* FirstSelectedItem)
{
	Super::OnSelectionChangedInternal(FirstSelectedItem);

	if (bSuppressPanelSelectionNotify || !PanelNavigationCoordinator || !FirstSelectedItem)
	{
		return;
	}

	PanelNavigationCoordinator->NotifyAddressPanelSelectionChanged(this, FirstSelectedItem);
}

void URpgInventoryAddressTileView::RefreshAddressSlotItems()
{
	SetAddressSlotItems(BoundGroupViewModel ? BoundGroupViewModel->GetSlots() : TArray<URpgInventoryAddressSlotViewModel*>());
}

void URpgInventoryAddressTileView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventoryAddressSlotWidget* AddressSlotWidget = Cast<URpgInventoryAddressSlotWidget>(EntryWidget))
	{
		AddressSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventoryAddressTileView::ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventoryAddressSlotWidget* AddressSlotWidget = Cast<URpgInventoryAddressSlotWidget>(EntryWidget))
	{
		AddressSlotWidget->SetInventoryPanelActive(bInventoryPanelActive);
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
	return DragDropCoordinator->PreviewPayloadDrop(InventoryOperation->InventoryPayload, Target) ? TOptional<EItemDropZone>(DropZone) : NullOpt;
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

void URpgInventorySlotGroupWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	if (SlotTileView)
	{
		SlotTileView->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupWidget::SetSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	GroupViewModel = InGroupViewModel;

	if (GroupViewModel)
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(GroupViewModel);
		}
	}

	if (SlotTileView)
	{
		SlotTileView->SetDragDropCoordinator(DragDropCoordinator);
		SlotTileView->BindSlotGroupViewModel(GroupViewModel);
	}

	BP_OnSlotGroupViewModelSet(GroupViewModel);
}

void URpgInventorySlotGroupWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetSlotGroupViewModel(Cast<URpgInventorySlotGroupViewModel>(ListItemObject));
}

void URpgInventorySlotGroupWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	SetSlotGroupViewModel(nullptr);
	BP_OnSlotGroupReleased();
}

URpgInventorySlotGroupListView::URpgInventorySlotGroupListView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgInventorySlotGroupListView::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyCoordinatorToEntry(EntryWidget);
	}
}

void URpgInventorySlotGroupListView::SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups)
{
	const TArray<UObject*>& CurrentItems = GetListItems();
	if (CurrentItems.Num() == InGroups.Num())
	{
		bool bSameItems = true;
		for (int32 Index = 0; Index < InGroups.Num(); ++Index)
		{
			if (CurrentItems[Index] != InGroups[Index])
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
	NewItems.Reserve(InGroups.Num());
	for (URpgInventorySlotGroupViewModel* GroupViewModel : InGroups)
	{
		NewItems.Add(GroupViewModel);
	}

	SetListItems(NewItems);
	RequestRefresh();
}

void URpgInventorySlotGroupListView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
}

void URpgInventorySlotGroupListView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventorySlotGroupWidget* GroupWidget = Cast<URpgInventorySlotGroupWidget>(EntryWidget))
	{
		GroupWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}
