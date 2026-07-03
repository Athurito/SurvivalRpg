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

void URpgInventoryAddressTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
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

void URpgActionBarTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
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

void URpgActionBarTileView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgActionBarSlotWidget* ActionBarSlotWidget = Cast<URpgActionBarSlotWidget>(EntryWidget))
	{
		ActionBarSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
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

	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->SetViewModelByClass(GroupViewModel);
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
