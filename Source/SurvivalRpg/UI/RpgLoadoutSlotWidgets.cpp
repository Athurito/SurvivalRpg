#include "RpgLoadoutSlotWidgets.h"

#include "Blueprint/DragDropOperation.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLoadoutSlotWidgets)

URpgEquipmentSlotWidget::URpgEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgEquipmentSlotWidget::SetEquipmentSlotViewModel(URpgEquipmentSlotViewModel* InSlotViewModel)
{
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	SlotViewModel = InSlotViewModel;
	if (SlotViewModel)
	{
		EquipmentSlot = SlotViewModel->GetEquipmentSlot();
		SlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->SetViewModelByClass(SlotViewModel);
	}

	BP_OnEquipmentSlotUpdated(SlotViewModel, GetRepresentedItem(), GetRepresentedItem() != nullptr);
	RefreshDragDropVisualState();
}

void URpgEquipmentSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
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

ERpgEquipmentSlot URpgEquipmentSlotWidget::GetResolvedEquipmentSlot() const
{
	return SlotViewModel ? SlotViewModel->GetEquipmentSlot() : EquipmentSlot;
}

URpgInventoryItemInstance* URpgEquipmentSlotWidget::GetRepresentedItem() const
{
	return SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
}

bool URpgEquipmentSlotWidget::HandleSlotAccept()
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (DragDropCoordinator->HasHeldPayload())
	{
		return DragDropCoordinator->CommitDrop(MakeDropTarget());
	}

	const FRpgInventoryDragPayload Payload = MakeDragPayload();
	return URpgInventoryDragDropCoordinator::IsPayloadValid(Payload) && DragDropCoordinator->BeginHold(Payload);
}

bool URpgEquipmentSlotWidget::HandleClearAssignment()
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	const FRpgInventoryDragPayload Payload = MakeDragPayload();
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return false;
	}

	return DragDropCoordinator->CommitPayloadToTarget(Payload, URpgInventoryDragDropCoordinator::MakeClearTarget());
}

void URpgEquipmentSlotWidget::RefreshDragDropVisualState()
{
	ERpgInventorySlotDragVisualState NewState = ERpgInventorySlotDragVisualState::Normal;
	if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		NewState = IsHeldSource()
			? ERpgInventorySlotDragVisualState::HeldSource
			: (DragDropCoordinator->PreviewDrop(MakeDropTarget())
				? ERpgInventorySlotDragVisualState::ValidTarget
				: ERpgInventorySlotDragVisualState::InvalidTarget);
	}

	BP_OnEquipmentSlotDragDropStateChanged(NewState);
}

void URpgEquipmentSlotWidget::NativeDestruct()
{
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	Super::NativeDestruct();
}

void URpgEquipmentSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	HandleSlotAccept();
}

FReply URpgEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && HandleClearAssignment())
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && GetRepresentedItem())
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URpgEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	const FRpgInventoryDragPayload Payload = MakeDragPayload();
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return;
	}

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		return;
	}

	InventoryOperation->Payload = GetRepresentedItem();
	InventoryOperation->InventoryPayload = Payload;
	OutOperation = InventoryOperation;
}

bool URpgEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return false;
	}

	return DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, MakeDropTarget());
}

void URpgEquipmentSlotWidget::HandleSlotViewModelChanged(URpgEquipmentSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == SlotViewModel)
	{
		BP_OnEquipmentSlotUpdated(SlotViewModel, GetRepresentedItem(), GetRepresentedItem() != nullptr);
		RefreshDragDropVisualState();
	}
}

void URpgEquipmentSlotWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

FRpgInventoryDragPayload URpgEquipmentSlotWidget::MakeDragPayload() const
{
	return URpgInventoryDragDropCoordinator::MakeEquipmentPayload(GetRepresentedItem(), GetResolvedEquipmentSlot());
}

FRpgInventoryDropTarget URpgEquipmentSlotWidget::MakeDropTarget() const
{
	return URpgInventoryDragDropCoordinator::MakeEquipmentTarget(GetResolvedEquipmentSlot());
}

bool URpgEquipmentSlotWidget::IsHeldSource() const
{
	if (!DragDropCoordinator || !DragDropCoordinator->HasHeldPayload())
	{
		return false;
	}

	const FRpgInventoryDragPayload HeldPayload = DragDropCoordinator->GetHeldPayload();
	return HeldPayload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot &&
		HeldPayload.EquipmentSlot == GetResolvedEquipmentSlot();
}
