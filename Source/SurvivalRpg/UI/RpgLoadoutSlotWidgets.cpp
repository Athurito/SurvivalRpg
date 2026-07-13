#include "RpgLoadoutSlotWidgets.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonUIExtensions.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryActionWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
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

	if (SlotViewModel)
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(SlotViewModel);
		}
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
	if (bHasExternalPreviewState)
	{
		NewState = ExternalPreviewState;
	}
	else if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		NewState = IsHeldSource()
			? ERpgInventorySlotDragVisualState::HeldSource
			: (DragDropCoordinator->PreviewDrop(MakeDropTarget())
				? ERpgInventorySlotDragVisualState::ValidTarget
				: ERpgInventorySlotDragVisualState::InvalidTarget);
	}

	BP_OnEquipmentSlotDragDropStateChanged(NewState);
}

bool URpgEquipmentSlotWidget::PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	if (!DragDropCoordinator || !URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	const bool bCanDrop = DragDropCoordinator->UpdateInteractionPreview(Payload, MakeDropTarget());
	bHasExternalPreviewState = true;
	ExternalPreviewState = bCanDrop
		? ERpgInventorySlotDragVisualState::ValidTarget
		: ERpgInventorySlotDragVisualState::InvalidTarget;
	RefreshDragDropVisualState();
	return bCanDrop;
}

bool URpgEquipmentSlotWidget::CommitPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	ClearExternalPreviewPayload();
	return DragDropCoordinator && DragDropCoordinator->CommitPayloadToTarget(Payload, MakeDropTarget());
}

void URpgEquipmentSlotWidget::ClearExternalPreviewPayload()
{
	if (!bHasExternalPreviewState)
	{
		return;
	}

	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	RefreshDragDropVisualState();
}

bool URpgEquipmentSlotWidget::RequestEquipmentContextMenu(FVector2D ScreenPosition)
{
	URpgInventoryItemInstance* ItemInstance = GetRepresentedItem();
	if (!ItemInstance || !ItemInstance->GetItemId().IsValid())
	{
		return false;
	}

	if (ActiveContextMenu.IsValid())
	{
		ActiveContextMenu->CloseContextMenu();
		ActiveContextMenu = nullptr;
	}

	TArray<ERpgInventoryContextAction> Actions;
	Actions.Reserve(3);
	Actions.Add(ERpgInventoryContextAction::Inspect);
	Actions.Add(ERpgInventoryContextAction::Unequip);
	const URpgInventoryFragment_ItemTraits* Traits = ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>();
	if (!Traits || Traits->GetResolvedManualDropPolicy() != ERpgInventoryManualDropPolicy::Disabled)
	{
		Actions.Add(ERpgInventoryContextAction::Drop);
	}

	TSubclassOf<URpgInventoryContextMenuWidget> MenuClass = ContextMenuWidgetClass;
	if (!MenuClass)
	{
		MenuClass = URpgInventoryContextMenuWidget::StaticClass();
	}
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		ActiveContextMenu = Cast<URpgInventoryContextMenuWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(
				LocalPlayer,
				RpgGameplayTags::UI_Layer_Modal,
				MenuClass));
	}

	if (ActiveContextMenu.IsValid() && ActiveContextMenu->InitializeEquipmentContextMenu(this, Actions, ScreenPosition))
	{
		return true;
	}

	if (ActiveContextMenu.IsValid())
	{
		ActiveContextMenu->CloseContextMenu();
		ActiveContextMenu = nullptr;
	}
	return false;
}

bool URpgEquipmentSlotWidget::ExecuteEquipmentContextAction(
	ERpgInventoryContextAction Action,
	const FRpgInventoryItemId& ExpectedItemId)
{
	URpgInventoryItemInstance* ItemInstance = GetRepresentedItem();
	if (!ItemInstance || !ExpectedItemId.IsValid() || ItemInstance->GetItemId() != ExpectedItemId)
	{
		return false;
	}

	switch (Action)
	{
	case ERpgInventoryContextAction::Inspect:
		BP_OnInspectEquipmentItemRequested(ItemInstance);
		return true;

	case ERpgInventoryContextAction::Unequip:
		return DragDropCoordinator && DragDropCoordinator->UnequipEquipmentItem(GetResolvedEquipmentSlot(), ExpectedItemId);

	case ERpgInventoryContextAction::Drop:
		return DragDropCoordinator && DragDropCoordinator->DropEquipmentItem(GetResolvedEquipmentSlot(), ExpectedItemId);

	default:
		return false;
	}
}

void URpgEquipmentSlotWidget::NativeDestruct()
{
	if (ActiveContextMenu.IsValid())
	{
		ActiveContextMenu->CloseContextMenu();
		ActiveContextMenu = nullptr;
	}

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
	const FReply HandledReply = HandlePointerButtonDown(InGeometry, InMouseEvent);
	if (HandledReply.IsEventHandled())
	{
		return HandledReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgEquipmentSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply HandledReply = HandlePointerButtonDown(InGeometry, InMouseEvent);
	if (HandledReply.IsEventHandled())
	{
		return HandledReply;
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgEquipmentSlotWidget::HandlePointerButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
		RequestEquipmentContextMenu(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && GetRepresentedItem())
	{
		if (InMouseEvent.IsControlDown())
		{
			return DragDropCoordinator && DragDropCoordinator->QuickTransferPlayerItem(GetRepresentedItem())
				? FReply::Handled()
				: FReply::Unhandled();
		}
		FRpgInventoryDragPayload PointerPayload = MakeDragPayload();
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
			PointerPayload,
			InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()),
			InGeometry.GetLocalSize());
		const FVector2D ScreenTopLeft = InGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ScreenBottomRight = InGeometry.LocalToAbsolute(InGeometry.GetLocalSize());
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchorScreenGeometry(
			PointerPayload,
			ScreenTopLeft,
			InMouseEvent.GetScreenSpacePosition(),
			FVector2D(
				FMath::Abs(ScreenBottomRight.X - ScreenTopLeft.X),
				FMath::Abs(ScreenBottomRight.Y - ScreenTopLeft.Y)));
		PendingPointerDragAnchor = PointerPayload.DragAnchor;
		bHasPendingPointerDragAnchor = PendingPointerDragAnchor.bValid;
		bPendingLeftClickAccept = true;
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return FReply::Unhandled();
}

FReply URpgEquipmentSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		bHasPendingPointerDragAnchor = false;
		return HandleSlotAccept() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URpgEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPendingLeftClickAccept = false;

	FRpgInventoryDragPayload Payload = MakeDragPayload();
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return;
	}
	if (bHasPendingPointerDragAnchor)
	{
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
			Payload,
			PendingPointerDragAnchor.SourcePointerOffset,
			PendingPointerDragAnchor.SourceVisualSize);
		Payload.DragAnchor.SourceScreenPointerOffset = PendingPointerDragAnchor.SourceScreenPointerOffset;
		Payload.DragAnchor.SourceScreenVisualSize = PendingPointerDragAnchor.SourceScreenVisualSize;
	}
	else
	{
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
			Payload,
			InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()),
			InGeometry.GetLocalSize());
		const FVector2D ScreenTopLeft = InGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ScreenBottomRight = InGeometry.LocalToAbsolute(InGeometry.GetLocalSize());
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchorScreenGeometry(
			Payload,
			ScreenTopLeft,
			InMouseEvent.GetScreenSpacePosition(),
			FVector2D(
				FMath::Abs(ScreenBottomRight.X - ScreenTopLeft.X),
				FMath::Abs(ScreenBottomRight.Y - ScreenTopLeft.Y)));
	}
	bHasPendingPointerDragAnchor = false;
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

	InventoryOperation->Pivot = EDragPivot::TopLeft;
	if (Payload.DragAnchor.SourceVisualSize.X > KINDA_SMALL_NUMBER && Payload.DragAnchor.SourceVisualSize.Y > KINDA_SMALL_NUMBER)
	{
		InventoryOperation->Offset = FVector2D(
			-Payload.DragAnchor.SourcePointerOffset.X / Payload.DragAnchor.SourceVisualSize.X,
			-Payload.DragAnchor.SourcePointerOffset.Y / Payload.DragAnchor.SourceVisualSize.Y);
	}
	InventoryOperation->Payload = GetRepresentedItem();
	InventoryOperation->InventoryPayload = Payload;
	InventoryOperation->SetInteractionSession(DragDropCoordinator->GetInteractionSession());

	TSubclassOf<UUserWidget> VisualClass = DragVisualClass;
	if (!VisualClass)
	{
		VisualClass = URpgInventoryDragVisualWidget::StaticClass();
	}

	if (VisualClass)
	{
		UUserWidget* DragVisual = CreateWidget<UUserWidget>(GetWorld(), VisualClass);
		if (URpgEquipmentSlotWidget* EquipmentSlotDragVisual = Cast<URpgEquipmentSlotWidget>(DragVisual))
		{
			EquipmentSlotDragVisual->SetEquipmentSlotViewModel(SlotViewModel);
			EquipmentSlotDragVisual->SetDragDropCoordinator(DragDropCoordinator);
		}
		if (URpgInventoryDragVisualWidget* CanonicalDragVisual = Cast<URpgInventoryDragVisualWidget>(DragVisual))
		{
			CanonicalDragVisual->ConfigureFromPayload(Payload, 70.0f, 2.0f);
		}

		InventoryOperation->DefaultDragVisual = DragVisual;
	}

	OutOperation = InventoryOperation;
}

bool URpgEquipmentSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

bool URpgEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

void URpgEquipmentSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Child boundaries do not own preview lifetime; the screen resolver clears the previous target atomically.
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
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
