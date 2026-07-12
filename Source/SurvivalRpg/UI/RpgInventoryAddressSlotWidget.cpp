#include "RpgInventoryAddressSlotWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonUIExtensions.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryActionWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
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

void URpgInventoryAddressSlotWidget::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
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
		? DragDropCoordinator->GetInventoryAddressSlotVisualState(SlotViewModel, bSlotSelected && bInventoryPanelActive)
		: (bSlotSelected && bInventoryPanelActive ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal);

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
	if (ActiveContextMenu.IsValid())
	{
		ActiveContextMenu->CloseContextMenu();
		ActiveContextMenu = nullptr;
	}
	if (ActiveSplitDialog.IsValid())
	{
		ActiveSplitDialog->CancelSplitDialog();
		ActiveSplitDialog = nullptr;
	}

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

void URpgInventoryAddressSlotWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	bSlotSelected = true;
	BP_OnAddressSlotSelectionChanged(true);
	RefreshDragDropVisualState();
}

void URpgInventoryAddressSlotWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	bSlotSelected = false;
	BP_OnAddressSlotSelectionChanged(false);
	RefreshDragDropVisualState();
}

void URpgInventoryAddressSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	HandleSlotAccept();
}

FReply URpgInventoryAddressSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply HandledReply = HandlePointerButtonDown(InGeometry, InMouseEvent);
	if (HandledReply.IsEventHandled())
	{
		return HandledReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventoryAddressSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply HandledReply = HandlePointerButtonDown(InGeometry, InMouseEvent);
	if (HandledReply.IsEventHandled())
	{
		return HandledReply;
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventoryAddressSlotWidget::HandlePointerButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
		RequestAddressContextMenu(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && SlotViewModel && (SlotViewModel->CanDrag() || SlotViewModel->IsActionbarBindable()))
	{
		bPendingLeftClickAccept = false;
		bHasPendingPointerDragAnchor = false;
		if (InMouseEvent.IsControlDown())
		{
			return DragDropCoordinator && DragDropCoordinator->QuickTransferAddressSlot(SlotViewModel)
				? FReply::Handled()
				: FReply::Unhandled();
		}
		if (InMouseEvent.IsAltDown())
		{
			return DragDropCoordinator && DragDropCoordinator->UseOrEquipAddressSlot(SlotViewModel)
				? FReply::Handled()
				: FReply::Unhandled();
		}
		if (InMouseEvent.IsShiftDown())
		{
			return RequestAddressSplitDialog() ? FReply::Handled() : FReply::Unhandled();
		}

		FRpgInventoryDragPayload PointerPayload = MakeDragPayload(true);
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

FReply URpgInventoryAddressSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		bHasPendingPointerDragAnchor = false;
		return HandleSlotAccept() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URpgInventoryAddressSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPendingLeftClickAccept = false;

	FRpgInventoryDragPayload Payload = MakeDragPayload(true);
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
	InventoryOperation->Payload = SlotViewModel;
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
		if (URpgInventoryAddressSlotWidget* AddressSlotDragVisual = Cast<URpgInventoryAddressSlotWidget>(DragVisual))
		{
			AddressSlotDragVisual->SetAddressSlotViewModel(SlotViewModel);
			AddressSlotDragVisual->SetDragDropCoordinator(DragDropCoordinator);
		}
		if (URpgInventoryDragVisualWidget* CanonicalDragVisual = Cast<URpgInventoryDragVisualWidget>(DragVisual))
		{
			CanonicalDragVisual->ConfigureFromPayload(Payload, 70.0f, 2.0f);
		}

		InventoryOperation->DefaultDragVisual = DragVisual;
	}

	OutOperation = InventoryOperation;
}

bool URpgInventoryAddressSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

bool URpgInventoryAddressSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
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

TArray<ERpgInventoryContextAction> URpgInventoryAddressSlotWidget::GetAddressContextActions() const
{
	TArray<ERpgInventoryContextAction> Actions;
	URpgInventoryItemInstance* Item = SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
	if (!Item)
	{
		return Actions;
	}

	if (SlotViewModel->IsGearSlot())
	{
		Actions.Add(ERpgInventoryContextAction::Inspect);
		Actions.Add(ERpgInventoryContextAction::Unequip);
	}
	else
	{
		if (Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
		{
			Actions.Add(ERpgInventoryContextAction::OpenContainer);
		}
		Actions.Add(ERpgInventoryContextAction::Inspect);
		if (Item->FindFragmentByClass<URpgInventoryFragment_UsableItem>())
		{
			Actions.Add(ERpgInventoryContextAction::Use);
		}
		if (Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>())
		{
			Actions.Add(ERpgInventoryContextAction::EquipAndActivate);
			if (!SlotViewModel->IsCarrySlot())
			{
				Actions.Add(ERpgInventoryContextAction::MoveToCarry);
			}
		}
		else if (Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
		{
			Actions.Add(ERpgInventoryContextAction::EquipAndActivate);
		}
		if (!SlotViewModel->IsCarrySlot() && SlotViewModel->GetStackCount() > 1)
		{
			Actions.Add(ERpgInventoryContextAction::Split);
		}
		if (SlotViewModel->IsActionbarBindable())
		{
			Actions.Add(ERpgInventoryContextAction::QuickAccessBind);
			Actions.Add(ERpgInventoryContextAction::QuickAccessUnbind);
		}
		if (DragDropCoordinator && DragDropCoordinator->CanQuickTransferAddressSlot(SlotViewModel))
		{
			Actions.Add(ERpgInventoryContextAction::Transfer);
		}
	}

	const URpgInventoryFragment_ItemTraits* Traits = Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>();
	if (!Traits || Traits->GetResolvedManualDropPolicy() != ERpgInventoryManualDropPolicy::Disabled)
	{
		Actions.Add(ERpgInventoryContextAction::Drop);
	}
	return Actions;
}

bool URpgInventoryAddressSlotWidget::ExecuteAddressContextAction(
	ERpgInventoryContextAction Action,
	FRpgInventoryItemId ExpectedItemId)
{
	URpgInventoryItemInstance* Item = SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
	if (!DragDropCoordinator || !Item || !ExpectedItemId.IsValid() || Item->GetItemId() != ExpectedItemId ||
		!GetAddressContextActions().Contains(Action))
	{
		return false;
	}

	switch (Action)
	{
	case ERpgInventoryContextAction::Unequip:
		return SlotViewModel->IsGearSlot() && DragDropCoordinator->UseOrEquipAddressSlot(SlotViewModel);
	case ERpgInventoryContextAction::Use:
		return DragDropCoordinator->ExecuteAddressItemAction(SlotViewModel, ERpgInventoryItemActionIntent::Use, 1);
	case ERpgInventoryContextAction::EquipAndActivate:
		return DragDropCoordinator->ExecuteAddressItemAction(SlotViewModel, ERpgInventoryItemActionIntent::EquipAndActivate, 1);
	case ERpgInventoryContextAction::MoveToCarry:
		return DragDropCoordinator->ExecuteAddressItemAction(SlotViewModel, ERpgInventoryItemActionIntent::MoveToCarry, 1);
	case ERpgInventoryContextAction::Split:
		return RequestAddressSplitDialog();
	case ERpgInventoryContextAction::Transfer:
		return DragDropCoordinator->QuickTransferAddressSlot(SlotViewModel);
	case ERpgInventoryContextAction::Drop:
		return DragDropCoordinator->DropAddressSlot(SlotViewModel);
	case ERpgInventoryContextAction::OpenContainer:
	case ERpgInventoryContextAction::Inspect:
	case ERpgInventoryContextAction::QuickAccessBind:
	case ERpgInventoryContextAction::QuickAccessUnbind:
		BP_OnDeferredAddressContextAction(Action, Item);
		return true;
	default:
		return false;
	}
}

bool URpgInventoryAddressSlotWidget::ConfirmAddressSplit(FRpgInventoryItemId ExpectedItemId, int32 SplitCount)
{
	URpgInventoryItemInstance* Item = SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
	return DragDropCoordinator && Item && ExpectedItemId.IsValid() && Item->GetItemId() == ExpectedItemId &&
		SplitCount >= 1 && SplitCount < SlotViewModel->GetStackCount() &&
		DragDropCoordinator->QuickSplitAddressSlot(SlotViewModel, FRpgInventoryGridPlacement(), SplitCount);
}

bool URpgInventoryAddressSlotWidget::RequestAddressContextMenu(FVector2D ScreenPosition)
{
	URpgInventoryItemInstance* Item = SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
	const TArray<ERpgInventoryContextAction> Actions = GetAddressContextActions();
	if (!Item || Actions.IsEmpty())
	{
		return false;
	}
	if (ActiveContextMenu.IsValid())
	{
		ActiveContextMenu->CloseContextMenu();
		ActiveContextMenu = nullptr;
	}

	const TSubclassOf<URpgInventoryContextMenuWidget> MenuClass = ContextMenuWidgetClass
		? ContextMenuWidgetClass
		: URpgInventoryContextMenuWidget::StaticClass();
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		ActiveContextMenu = Cast<URpgInventoryContextMenuWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, RpgGameplayTags::UI_Layer_Modal, MenuClass));
	}
	if (ActiveContextMenu.IsValid() && ActiveContextMenu->InitializeAddressContextMenu(this, Actions, ScreenPosition))
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

bool URpgInventoryAddressSlotWidget::RequestAddressSplitDialog()
{
	URpgInventoryItemInstance* Item = SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
	const int32 StackCount = SlotViewModel ? SlotViewModel->GetStackCount() : 0;
	if (!Item || SlotViewModel->IsGearSlot() || SlotViewModel->IsCarrySlot() || StackCount <= 1)
	{
		return false;
	}
	if (ActiveSplitDialog.IsValid())
	{
		ActiveSplitDialog->CancelSplitDialog();
		ActiveSplitDialog = nullptr;
	}

	const TSubclassOf<URpgInventorySplitDialogWidget> DialogClass = SplitDialogWidgetClass
		? SplitDialogWidgetClass
		: URpgInventorySplitDialogWidget::StaticClass();
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		ActiveSplitDialog = Cast<URpgInventorySplitDialogWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, RpgGameplayTags::UI_Layer_Modal, DialogClass));
	}
	const int32 MaximumCount = StackCount - 1;
	const int32 DefaultCount = FMath::Clamp(StackCount / 2, 1, MaximumCount);
	if (ActiveSplitDialog.IsValid() && ActiveSplitDialog->InitializeAddressSplitDialog(
		this,
		Item->GetItemId(),
		1,
		MaximumCount,
		DefaultCount))
	{
		return true;
	}
	if (ActiveSplitDialog.IsValid())
	{
		ActiveSplitDialog->CancelSplitDialog();
		ActiveSplitDialog = nullptr;
	}
	return false;
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
