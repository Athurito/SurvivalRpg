#include "RpgLoadoutSlotWidgets.h"

#include "Blueprint/DragDropOperation.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLoadoutSlotWidgets)

URpgQuickBarHandSlotWidget::URpgQuickBarHandSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgQuickBarHandSlotWidget::SetQuickBarSlotViewModel(URpgQuickBarSlotViewModel* InSlotViewModel)
{
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	SlotViewModel = InSlotViewModel;
	if (SlotViewModel)
	{
		QuickBarSlotIndex = SlotViewModel->GetSlotIndex();
		SlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	BP_OnQuickBarHandSlotUpdated(SlotViewModel, GetRepresentedItem(), GetRepresentedItem() != nullptr, SlotViewModel && SlotViewModel->IsActiveSlot());
	RefreshDragDropVisualState();
}

void URpgQuickBarHandSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
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

URpgInventoryItemInstance* URpgQuickBarHandSlotWidget::GetRepresentedItem() const
{
	return SlotViewModel ? SlotViewModel->GetItemForEquipmentSlot(EquipmentSlot) : nullptr;
}

int32 URpgQuickBarHandSlotWidget::GetResolvedQuickBarSlotIndex() const
{
	return SlotViewModel ? SlotViewModel->GetSlotIndex() : QuickBarSlotIndex;
}

bool URpgQuickBarHandSlotWidget::HandleSlotAccept()
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

bool URpgQuickBarHandSlotWidget::HandleClearAssignment()
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

void URpgQuickBarHandSlotWidget::RefreshDragDropVisualState()
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

	BP_OnQuickBarHandSlotDragDropStateChanged(NewState);
}

void URpgQuickBarHandSlotWidget::NativeDestruct()
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

void URpgQuickBarHandSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	HandleSlotAccept();
}

FReply URpgQuickBarHandSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

void URpgQuickBarHandSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
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

bool URpgQuickBarHandSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return false;
	}

	return DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, MakeDropTarget());
}

void URpgQuickBarHandSlotWidget::HandleSlotViewModelChanged(URpgQuickBarSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == SlotViewModel)
	{
		BP_OnQuickBarHandSlotUpdated(SlotViewModel, GetRepresentedItem(), GetRepresentedItem() != nullptr, SlotViewModel && SlotViewModel->IsActiveSlot());
		RefreshDragDropVisualState();
	}
}

void URpgQuickBarHandSlotWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

FRpgInventoryDragPayload URpgQuickBarHandSlotWidget::MakeDragPayload() const
{
	return URpgInventoryDragDropCoordinator::MakeQuickBarPayload(GetRepresentedItem(), GetResolvedQuickBarSlotIndex(), EquipmentSlot);
}

FRpgInventoryDropTarget URpgQuickBarHandSlotWidget::MakeDropTarget() const
{
	return URpgInventoryDragDropCoordinator::MakeQuickBarTarget(GetResolvedQuickBarSlotIndex(), EquipmentSlot);
}

bool URpgQuickBarHandSlotWidget::IsHeldSource() const
{
	if (!DragDropCoordinator || !DragDropCoordinator->HasHeldPayload())
	{
		return false;
	}

	const FRpgInventoryDragPayload HeldPayload = DragDropCoordinator->GetHeldPayload();
	return HeldPayload.SourceType == ERpgInventoryDragSourceType::QuickBarSlot &&
		HeldPayload.SourceSlotIndex == GetResolvedQuickBarSlotIndex() &&
		HeldPayload.EquipmentSlot == EquipmentSlot;
}

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
