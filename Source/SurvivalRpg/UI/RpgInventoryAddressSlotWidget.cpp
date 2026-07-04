#include "RpgInventoryAddressSlotWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryAddressSlotWidget)

URpgInventoryAddressSlotWidget::URpgInventoryAddressSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgInventoryAddressSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
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

void URpgInventoryAddressSlotWidget::SetAddressSlotViewModel(URpgInventoryAddressSlotViewModel* InSlotViewModel)
{
	if (SlotViewModel == InSlotViewModel)
	{
		BP_OnAddressSlotViewModelSet(SlotViewModel);
		RefreshDragDropVisualState();
		return;
	}

	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	SlotViewModel = InSlotViewModel;
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	if (SlotViewModel)
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(SlotViewModel);
		}
	}

	BP_OnAddressSlotViewModelSet(SlotViewModel);
	RefreshDragDropVisualState();
}

bool URpgInventoryAddressSlotWidget::HandleSlotAccept()
{
	if (!DragDropCoordinator || !SlotViewModel)
	{
		return false;
	}

	if (DragDropCoordinator->HasHeldPayload())
	{
		return DragDropCoordinator->CommitDrop(MakeDropTarget());
	}

	const FRpgInventoryDragPayload Payload = MakeDragPayload(false);
	return URpgInventoryDragDropCoordinator::IsPayloadValid(Payload) && DragDropCoordinator->BeginHold(Payload);
}

void URpgInventoryAddressSlotWidget::RefreshDragDropVisualState()
{
	CurrentDragDropVisualState = DragDropCoordinator
		? DragDropCoordinator->GetInventoryAddressSlotVisualState(SlotViewModel, bSlotSelected)
		: (bSlotSelected ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal);

	BP_OnAddressSlotDragDropStateChanged(CurrentDragDropVisualState);
}

void URpgInventoryAddressSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetAddressSlotViewModel(Cast<URpgInventoryAddressSlotViewModel>(ListItemObject));
}

void URpgInventoryAddressSlotWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();

	bSlotSelected = false;
	BP_OnAddressSlotSelectionChanged(false);
	SetAddressSlotViewModel(nullptr);
	BP_OnAddressSlotReleased();
}

void URpgInventoryAddressSlotWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	bSlotSelected = bIsSelected;
	BP_OnAddressSlotSelectionChanged(bSlotSelected);
	RefreshDragDropVisualState();
}

void URpgInventoryAddressSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	HandleSlotAccept();
}

FReply URpgInventoryAddressSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && SlotViewModel && (SlotViewModel->CanDrag() || SlotViewModel->IsActionbarBindable()))
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URpgInventoryAddressSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	const FRpgInventoryDragPayload Payload = MakeDragPayload(true);
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return;
	}

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		return;
	}

	InventoryOperation->Payload = SlotViewModel;
	InventoryOperation->InventoryPayload = Payload;

	TSubclassOf<UUserWidget> VisualClass = DragVisualClass;
	if (!VisualClass)
	{
		VisualClass = GetClass();
	}

	if (VisualClass)
	{
		UUserWidget* DragVisual = CreateWidget<UUserWidget>(GetWorld(), VisualClass);
		if (URpgInventoryAddressSlotWidget* AddressSlotDragVisual = Cast<URpgInventoryAddressSlotWidget>(DragVisual))
		{
			AddressSlotDragVisual->SetAddressSlotViewModel(SlotViewModel);
			AddressSlotDragVisual->SetDragDropCoordinator(DragDropCoordinator);
		}

		InventoryOperation->DefaultDragVisual = DragVisual;
	}

	OutOperation = InventoryOperation;
}

bool URpgInventoryAddressSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return false;
	}

	return DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, MakeDropTarget());
}

void URpgInventoryAddressSlotWidget::HandleSlotViewModelChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == SlotViewModel)
	{
		BP_OnAddressSlotViewModelSet(ChangedSlotViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgInventoryAddressSlotWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

FRpgInventoryDragPayload URpgInventoryAddressSlotWidget::MakeDragPayload(bool bAllowEmptyAddressPayload) const
{
	if (!SlotViewModel)
	{
		return FRpgInventoryDragPayload();
	}

	if (!SlotViewModel->CanDrag() && !bAllowEmptyAddressPayload)
	{
		return FRpgInventoryDragPayload();
	}

	if (!SlotViewModel->CanDrag() && !SlotViewModel->IsActionbarBindable())
	{
		return FRpgInventoryDragPayload();
	}

	return URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(SlotViewModel);
}

FRpgInventoryDropTarget URpgInventoryAddressSlotWidget::MakeDropTarget() const
{
	return URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(SlotViewModel);
}
