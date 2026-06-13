#include "RpgInventoryTileView.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "Slate/UMGDragDropOp.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
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
	}
}

void URpgInventoryTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);

	ApplyCoordinatorToEntry(EntryWidget);
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

void URpgInventoryTileView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventorySlotEntryWidget* InventoryEntryWidget = Cast<URpgInventorySlotEntryWidget>(EntryWidget))
	{
		InventoryEntryWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}
