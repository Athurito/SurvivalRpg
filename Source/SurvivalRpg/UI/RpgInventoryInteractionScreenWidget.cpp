#include "RpgInventoryInteractionScreenWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonUIExtensions.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/UI/RpgInventoryActionWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryFeedbackToastWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryInteractionScreenWidget)

bool FRpgInventoryDropConfirmationIntent::Arm(
	UWidget* InSourceWidget,
	URpgInventoryManagerComponent* InSourceInventory,
	const FRpgInventoryManualDropRequest& InRequest)
{
	Reset();
	if (!InSourceWidget || !InSourceInventory ||
		!InRequest.RequestId.IsValid() ||
		!InRequest.EntryId.IsValid() ||
		!InRequest.ItemId.IsValid() ||
		!InRequest.ExpectedSourcePlacement.IsValid() ||
		InRequest.StackCount <= 0 ||
		InRequest.bConfirmed)
	{
		return false;
	}

	SourceWidget = InSourceWidget;
	SourceInventory = InSourceInventory;
	Request = InRequest;
	return true;
}

bool FRpgInventoryDropConfirmationIntent::IsArmed() const
{
	return SourceWidget.IsValid() &&
		SourceInventory.IsValid() &&
		Request.RequestId.IsValid() &&
		Request.EntryId.IsValid() &&
		Request.ItemId.IsValid() &&
		Request.ExpectedSourcePlacement.IsValid() &&
		Request.StackCount > 0 &&
		!Request.bConfirmed;
}

bool FRpgInventoryDropConfirmationIntent::DoesFeedbackMatch(
	const APlayerController* OwningPlayer,
	const FRpgInventoryActionFeedbackMessage& Message) const
{
	return IsArmed() &&
		Message.IsAddressedTo(OwningPlayer) &&
		Message.RequestId == Request.RequestId &&
		Message.ItemId == Request.ItemId &&
		Message.ActionTag == RpgGameplayTags::Rpg_Inventory_Action_Drop &&
		Message.InventoryOwner.Get() == SourceInventory.Get() &&
		Message.StackCount == Request.StackCount;
}

bool FRpgInventoryDropConfirmationIntent::ConsumeConfirmedRetry(
	const FGuid& InitialRequestId,
	URpgInventoryManagerComponent*& OutSourceInventory,
	FRpgInventoryManualDropRequest& OutConfirmedRequest)
{
	OutSourceInventory = nullptr;
	OutConfirmedRequest = FRpgInventoryManualDropRequest();
	if (!IsArmed() || InitialRequestId != Request.RequestId)
	{
		return false;
	}

	OutSourceInventory = SourceInventory.Get();
	OutConfirmedRequest = Request;
	Reset();

	// The first RequestId may be cached as RequiresConfirmation. A retry is a new, exactly-once command.
	OutConfirmedRequest.RequestId = FGuid::NewGuid();
	OutConfirmedRequest.bConfirmed = true;
	return OutSourceInventory != nullptr;
}

bool FRpgInventoryDropConfirmationIntent::ResetForSource(
	const UWidget* InSourceWidget)
{
	if (!InSourceWidget || SourceWidget.Get() != InSourceWidget)
	{
		return false;
	}

	Reset();
	return true;
}

void FRpgInventoryDropConfirmationIntent::Reset()
{
	SourceWidget.Reset();
	SourceInventory.Reset();
	Request = FRpgInventoryManualDropRequest();
}

URpgInventoryInteractionScreenWidget::URpgInventoryInteractionScreenWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URpgInventoryInteractionScreenWidget::RequestInventoryDrop(
	URpgInventorySpatialGridWidget* SourceGrid,
	int32 StackCount)
{
	if (!SourceGrid || !InventoryDragDropCoordinator)
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = nullptr;
	FRpgInventoryManualDropRequest Request;
	const bool bPrepared = SourceGrid->GetSelectedAddressSlot()
		? InventoryDragDropCoordinator->PrepareDropAddressSlotRequest(
			SourceGrid->GetSelectedAddressSlot(),
			StackCount,
			SourceInventory,
			Request)
		: InventoryDragDropCoordinator->PrepareDropEntryRequest(
			SourceGrid->GetSelectedEntryViewModel(),
			StackCount,
			SourceInventory,
			Request);
	return bPrepared &&
		BeginPreparedInventoryDrop(SourceGrid, SourceInventory, Request);
}

bool URpgInventoryInteractionScreenWidget::RequestInventoryDrop(
	URpgInventoryAddressSlotWidget* SourceAddressSlot,
	int32 StackCount)
{
	if (!SourceAddressSlot || !InventoryDragDropCoordinator)
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = nullptr;
	FRpgInventoryManualDropRequest Request;
	return InventoryDragDropCoordinator->PrepareDropAddressSlotRequest(
			SourceAddressSlot->GetAddressSlotViewModel(),
			StackCount,
			SourceInventory,
			Request) &&
		BeginPreparedInventoryDrop(
			SourceAddressSlot,
			SourceInventory,
			Request);
}

bool URpgInventoryInteractionScreenWidget::RequestInventoryDrop(
	URpgEquipmentSlotWidget* SourceEquipmentSlot,
	FRpgInventoryItemId ExpectedItemId)
{
	if (!SourceEquipmentSlot || !InventoryDragDropCoordinator)
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = nullptr;
	FRpgInventoryManualDropRequest Request;
	return InventoryDragDropCoordinator->PrepareDropEquipmentItemRequest(
			SourceEquipmentSlot->GetResolvedEquipmentSlot(),
			ExpectedItemId,
			SourceInventory,
			Request) &&
		BeginPreparedInventoryDrop(
			SourceEquipmentSlot,
			SourceInventory,
			Request);
}

bool URpgInventoryInteractionScreenWidget::ConfirmPendingInventoryDrop(
	FGuid InitialRequestId)
{
	URpgInventoryManagerComponent* SourceInventory = nullptr;
	FRpgInventoryManualDropRequest ConfirmedRequest;
	if (!InventoryDragDropCoordinator ||
		!PendingDropConfirmation.ConsumeConfirmedRetry(
			InitialRequestId,
			SourceInventory,
			ConfirmedRequest))
	{
		return false;
	}

	// Consume-before-dispatch makes button reentrancy, repeated callbacks, and pooled modal teardown harmless.
	if (!InventoryDragDropCoordinator->DispatchManualDropRequest(
			SourceInventory,
			ConfirmedRequest))
	{
		ShowLocalDropRetryRejection(
			SourceInventory,
			ConfirmedRequest);
		return false;
	}

	return true;
}

void URpgInventoryInteractionScreenWidget::CancelPendingInventoryDrop(
	FGuid InitialRequestId)
{
	if (PendingDropConfirmation.GetInitialRequestId() == InitialRequestId)
	{
		PendingDropConfirmation.Reset();
	}
}

bool URpgInventoryInteractionScreenWidget::OpenInventoryContextMenu(
	URpgInventorySpatialGridWidget* SourceGrid,
	const TArray<ERpgInventoryContextAction>& Actions,
	FVector2D ScreenPosition)
{
	if (!SourceGrid || Actions.IsEmpty() || !ContextMenuWidgetClass)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	DismissInventoryModalPresentation();
	URpgInventoryContextMenuWidget* ContextMenu =
		Cast<URpgInventoryContextMenuWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(
				LocalPlayer,
				RpgGameplayTags::UI_Layer_Modal,
				ContextMenuWidgetClass));
	if (!ContextMenu ||
		!ContextMenu->InitializeContextMenu(SourceGrid, Actions, ScreenPosition))
	{
		if (ContextMenu)
		{
			ContextMenu->CloseContextMenu();
		}
		return false;
	}

	ActiveContextMenu = ContextMenu;
	ActiveContextMenuSource = SourceGrid;
	ContextMenu->OnDeactivated().RemoveAll(this);
	ContextMenu->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleContextMenuDeactivated,
		ContextMenu);
	return true;
}

bool URpgInventoryInteractionScreenWidget::OpenInventoryContextMenu(
	URpgInventoryAddressSlotWidget* SourceAddressSlot,
	const TArray<ERpgInventoryContextAction>& Actions,
	FVector2D ScreenPosition)
{
	if (!SourceAddressSlot || Actions.IsEmpty() || !ContextMenuWidgetClass)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	DismissInventoryModalPresentation();
	URpgInventoryContextMenuWidget* ContextMenu =
		Cast<URpgInventoryContextMenuWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(
				LocalPlayer,
				RpgGameplayTags::UI_Layer_Modal,
				ContextMenuWidgetClass));
	if (!ContextMenu ||
		!ContextMenu->InitializeAddressContextMenu(
			SourceAddressSlot,
			Actions,
			ScreenPosition))
	{
		if (ContextMenu)
		{
			ContextMenu->CloseContextMenu();
		}
		return false;
	}

	ActiveContextMenu = ContextMenu;
	ActiveContextMenuSource = SourceAddressSlot;
	ContextMenu->OnDeactivated().RemoveAll(this);
	ContextMenu->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleContextMenuDeactivated,
		ContextMenu);
	return true;
}

bool URpgInventoryInteractionScreenWidget::OpenInventoryContextMenu(
	URpgEquipmentSlotWidget* SourceEquipmentSlot,
	const TArray<ERpgInventoryContextAction>& Actions,
	FVector2D ScreenPosition)
{
	if (!SourceEquipmentSlot || Actions.IsEmpty() || !ContextMenuWidgetClass)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	DismissInventoryModalPresentation();
	URpgInventoryContextMenuWidget* ContextMenu =
		Cast<URpgInventoryContextMenuWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(
				LocalPlayer,
				RpgGameplayTags::UI_Layer_Modal,
				ContextMenuWidgetClass));
	if (!ContextMenu ||
		!ContextMenu->InitializeEquipmentContextMenu(
			SourceEquipmentSlot,
			Actions,
			ScreenPosition))
	{
		if (ContextMenu)
		{
			ContextMenu->CloseContextMenu();
		}
		return false;
	}

	ActiveContextMenu = ContextMenu;
	ActiveContextMenuSource = SourceEquipmentSlot;
	ContextMenu->OnDeactivated().RemoveAll(this);
	ContextMenu->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleContextMenuDeactivated,
		ContextMenu);
	return true;
}

bool URpgInventoryInteractionScreenWidget::OpenInventorySplitDialog(
	URpgInventorySpatialGridWidget* SourceGrid,
	FGuid EntryId,
	int32 MinimumCount,
	int32 MaximumCount,
	int32 DefaultCount)
{
	if (!SourceGrid || !EntryId.IsValid() || !SplitDialogWidgetClass)
	{
		if (SourceGrid)
		{
			SourceGrid->CancelPendingSplit();
		}
		return false;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		SourceGrid->CancelPendingSplit();
		return false;
	}

	DismissInventoryModalPresentation();
	URpgInventorySplitDialogWidget* SplitDialog =
		Cast<URpgInventorySplitDialogWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(
				LocalPlayer,
				RpgGameplayTags::UI_Layer_Modal,
				SplitDialogWidgetClass));
	if (!SplitDialog ||
		!SplitDialog->InitializeSplitDialog(
			SourceGrid,
			EntryId,
			MinimumCount,
			MaximumCount,
			DefaultCount))
	{
		if (SplitDialog)
		{
			SplitDialog->CancelSplitDialog();
		}
		else
		{
			SourceGrid->CancelPendingSplit();
		}
		return false;
	}

	ActiveSplitDialog = SplitDialog;
	ActiveSplitDialogSource = SourceGrid;
	SplitDialog->OnDeactivated().RemoveAll(this);
	SplitDialog->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleSplitDialogDeactivated,
		SplitDialog);
	return true;
}

bool URpgInventoryInteractionScreenWidget::OpenInventorySplitDialog(
	URpgInventoryAddressSlotWidget* SourceAddressSlot,
	FRpgInventoryItemId ItemId,
	int32 MinimumCount,
	int32 MaximumCount,
	int32 DefaultCount)
{
	if (!SourceAddressSlot || !ItemId.IsValid() || !SplitDialogWidgetClass)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	DismissInventoryModalPresentation();
	URpgInventorySplitDialogWidget* SplitDialog =
		Cast<URpgInventorySplitDialogWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(
				LocalPlayer,
				RpgGameplayTags::UI_Layer_Modal,
				SplitDialogWidgetClass));
	if (!SplitDialog ||
		!SplitDialog->InitializeAddressSplitDialog(
			SourceAddressSlot,
			ItemId,
			MinimumCount,
			MaximumCount,
			DefaultCount))
	{
		if (SplitDialog)
		{
			SplitDialog->CancelSplitDialog();
		}
		return false;
	}

	ActiveSplitDialog = SplitDialog;
	ActiveSplitDialogSource = SourceAddressSlot;
	SplitDialog->OnDeactivated().RemoveAll(this);
	SplitDialog->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleSplitDialogDeactivated,
		SplitDialog);
	return true;
}

bool URpgInventoryInteractionScreenWidget::BeginPreparedInventoryDrop(
	UWidget* SourceWidget,
	URpgInventoryManagerComponent* SourceInventory,
	const FRpgInventoryManualDropRequest& Request)
{
	if (!InventoryDragDropCoordinator)
	{
		return false;
	}

	// Only one confirmation candidate may belong to this pooled screen. Arm before dispatch because a standalone or
	// listen server can deliver RequiresConfirmation inside the same callstack as the server RPC.
	DismissActiveDropConfirmationPresentation();
	if (!PendingDropConfirmation.Arm(
			SourceWidget,
			SourceInventory,
			Request))
	{
		return false;
	}

	if (!InventoryDragDropCoordinator->DispatchManualDropRequest(
			SourceInventory,
			Request))
	{
		PendingDropConfirmation.Reset();
		return false;
	}

	return true;
}

bool URpgInventoryInteractionScreenWidget::OpenPendingDropConfirmation()
{
	if (!PendingDropConfirmation.IsArmed() ||
		!DropConfirmationDialogWidgetClass)
	{
		return false;
	}

	if (URpgInventoryDropConfirmationDialogWidget* ExistingDialog =
		ActiveDropConfirmation.Get())
	{
		return ExistingDialog->GetInitialRequestId() ==
			PendingDropConfirmation.GetInitialRequestId();
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		PendingDropConfirmation.GetSourceInventory();
	const FRpgInventoryManualDropRequest Request =
		PendingDropConfirmation.GetRequest();
	URpgInventoryItemInstance* Item = SourceInventory
		? SourceInventory->FindItemById(Request.ItemId)
		: nullptr;
	const URpgInventoryItemDefinition* ItemDefinition = Item &&
		Item->GetItemDef()
		? GetDefault<URpgInventoryItemDefinition>(Item->GetItemDef())
		: nullptr;
	const FText ItemName = ItemDefinition &&
		!ItemDefinition->DisplayName.IsEmpty()
		? ItemDefinition->DisplayName
		: NSLOCTEXT(
			"RpgInventoryInteractionScreen",
			"UnknownDropItem",
			"this item");

	// The confirmation replaces the initiating action menu, but its armed request must survive that replacement.
	DismissActiveContextMenuPresentation();
	DismissActiveSplitDialogPresentation();
	URpgInventoryDropConfirmationDialogWidget* DropConfirmation =
		Cast<URpgInventoryDropConfirmationDialogWidget>(
			UCommonUIExtensions::PushContentToLayer_ForPlayer(
				LocalPlayer,
				RpgGameplayTags::UI_Layer_Modal,
				DropConfirmationDialogWidgetClass));
	if (!DropConfirmation ||
		!DropConfirmation->InitializeDropConfirmation(
			this,
			Request.RequestId,
			ItemName,
			Request.StackCount))
	{
		if (DropConfirmation)
		{
			DropConfirmation->CancelDropConfirmation();
		}
		return false;
	}

	ActiveDropConfirmation = DropConfirmation;
	DropConfirmation->OnDeactivated().RemoveAll(this);
	DropConfirmation->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleDropConfirmationDeactivated,
		DropConfirmation);
	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->HideInventoryActionFeedback();
	}
	return true;
}

void URpgInventoryInteractionScreenWidget::ShowLocalDropRetryRejection(
	URpgInventoryManagerComponent* SourceInventory,
	const FRpgInventoryManualDropRequest& ConfirmedRequest)
{
	if (!InventoryFeedbackToast)
	{
		return;
	}

	FRpgInventoryActionFeedbackMessage Message;
	Message.Recipient = GetOwningPlayer();
	Message.RequestId = ConfirmedRequest.RequestId;
	Message.ItemId = ConfirmedRequest.ItemId;
	Message.ActionTag = RpgGameplayTags::Rpg_Inventory_Action_Drop;
	Message.Result =
		ERpgInventoryActionFeedbackResult::InvalidRequest;
	Message.InventoryOwner = SourceInventory;
	Message.StackCount = ConfirmedRequest.StackCount;
	InventoryFeedbackToast->ShowInventoryActionFeedback(Message);
}

void URpgInventoryInteractionScreenWidget::DismissInventoryPresentationForSource(
	const UWidget* SourceWidget)
{
	if (!SourceWidget)
	{
		return;
	}

	if (ActiveContextMenuSource.Get() == SourceWidget)
	{
		DismissActiveContextMenuPresentation();
	}
	if (ActiveSplitDialogSource.Get() == SourceWidget)
	{
		DismissActiveSplitDialogPresentation();
	}
	if (PendingDropConfirmation.GetSourceWidget() == SourceWidget)
	{
		if (ActiveDropConfirmation.IsValid())
		{
			DismissActiveDropConfirmationPresentation();
		}
		else
		{
			PendingDropConfirmation.ResetForSource(SourceWidget);
		}
	}
}

void URpgInventoryInteractionScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (DragVisualCanvas)
	{
		// A fullscreen drag host must never steal pointer hits from the inventory beneath it.
		DragVisualCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->HideInventoryActionFeedback();
	}
}

void URpgInventoryInteractionScreenWidget::NativeOnActivated()
{
	EnsureInventoryInteractionObjects();
	BindInventoryScreenPresentation();
	ForwardInventoryInteractionContextToChildren();
	RefreshInventoryScreenNavigationPanels();
	RegisterInventoryFeedbackListener();

	// Controller actions register only after the derived presentation has supplied a complete panel registry.
	Super::NativeOnActivated();
	RefreshInventoryControllerFocus();
	QueueDeferredInventoryScreenRefresh();
}

void URpgInventoryInteractionScreenWidget::NativeOnDeactivated()
{
	DismissInventoryModalPresentation();

	if (InventoryDragDropCoordinator && InventoryDragDropCoordinator->HasHeldPayload())
	{
		InventoryDragDropCoordinator->ForceCancelInteraction();
	}

	ActivePointerDropTarget.Reset();
	ClearExternalDragPreviews();
	UnregisterInventoryFeedbackListener();
	ClearFreePointerDragVisual();

	if (InventoryDragDropCoordinator && InventoryDragDropCoordinator->GetInteractionSession())
	{
		InventoryDragDropCoordinator->GetInteractionSession()->OnInteractionStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryInteractionStateChanged);
	}

	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->HideInventoryActionFeedback();
	}

	UnbindInventoryScreenPresentation();

	Super::NativeOnDeactivated();
}

void URpgInventoryInteractionScreenWidget::NativeDestruct()
{
	DismissInventoryModalPresentation();
	UnregisterInventoryFeedbackListener();
	if (InventoryDragDropCoordinator && InventoryDragDropCoordinator->GetInteractionSession())
	{
		InventoryDragDropCoordinator->GetInteractionSession()->OnInteractionStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryInteractionStateChanged);
	}

	ClearFreePointerDragVisual();
	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->HideInventoryActionFeedback();
	}

	UnbindInventoryScreenPresentation();

	Super::NativeDestruct();
}

bool URpgInventoryInteractionScreenWidget::NativeOnDragOver(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation =
		Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation ||
		!URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}

	ActivePointerDragOperation = InventoryOperation;
	LastPointerDragScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	bHasLastPointerDragScreenPosition = true;
	InventoryOperation->SetScreenOwnedDragVisualActive(true);
	InventoryOperation->SynchronizeFromInteractionSession();

	bool bHandled = false;
	{
		TGuardValue<bool> RoutingGuard(bRoutingPointerPreview, true);
		bHandled = RouteInventoryPayloadAtScreenPosition(
			InventoryOperation->InventoryPayload,
			LastPointerDragScreenPosition,
			false,
			InventoryOperation);
	}

	UpdateFreePointerDragVisual(
		InventoryDragDropCoordinator
			? InventoryDragDropCoordinator->ResolveInteractionPayload(
				InventoryOperation->InventoryPayload)
			: InventoryOperation->InventoryPayload,
		LastPointerDragScreenPosition,
		InventoryOperation);
	return bHandled;
}

bool URpgInventoryInteractionScreenWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation =
		Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation ||
		!URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	ActivePointerDragOperation = InventoryOperation;
	LastPointerDragScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	bHasLastPointerDragScreenPosition = true;
	InventoryOperation->SetScreenOwnedDragVisualActive(true);
	InventoryOperation->SynchronizeFromInteractionSession();

	bool bCommitted = false;
	{
		TGuardValue<bool> RoutingGuard(bRoutingPointerPreview, true);
		bCommitted = RouteInventoryPayloadAtScreenPosition(
			InventoryOperation->InventoryPayload,
			LastPointerDragScreenPosition,
			true,
			InventoryOperation);
	}

	UpdateFreePointerDragVisual(
		InventoryDragDropCoordinator
			? InventoryDragDropCoordinator->ResolveInteractionPayload(
				InventoryOperation->InventoryPayload)
			: InventoryOperation->InventoryPayload,
		LastPointerDragScreenPosition,
		InventoryOperation);
	return bCommitted;
}

void URpgInventoryInteractionScreenWidget::NativeOnDragLeave(
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation =
		Cast<URpgInventoryDragDropOperation>(InOperation);
	bool bGhostStillAddressesTarget = false;
	if (InventoryOperation &&
		URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
	{
		TGuardValue<bool> RoutingGuard(bRoutingPointerPreview, true);
		bGhostStillAddressesTarget = RouteInventoryPayloadAtScreenPosition(
			InventoryOperation->InventoryPayload,
			InDragDropEvent.GetScreenSpacePosition(),
			false,
			InventoryOperation);
	}

	if (bGhostStillAddressesTarget)
	{
		// The pointer may leave the screen root while the visible footprint still addresses an edge target.
		ActivePointerDragOperation = InventoryOperation;
		LastPointerDragScreenPosition = InDragDropEvent.GetScreenSpacePosition();
		bHasLastPointerDragScreenPosition = true;
		UpdateFreePointerDragVisual(
			InventoryDragDropCoordinator
				? InventoryDragDropCoordinator->ResolveInteractionPayload(
					InventoryOperation->InventoryPayload)
				: InventoryOperation->InventoryPayload,
			LastPointerDragScreenPosition,
			ActivePointerDragOperation);
		Super::NativeOnDragLeave(InDragDropEvent, InOperation);
		return;
	}

	ClearExternalDragPreviews();
	if (InventoryDragDropCoordinator)
	{
		const URpgInventoryInteractionSession* Session =
			InventoryDragDropCoordinator->GetInteractionSession();
		if (Session &&
			Session->GetInputMode() == ERpgInventoryInteractionInputMode::Mouse &&
			!Session->IsRequestPending())
		{
			InventoryDragDropCoordinator->CancelHold();
		}
	}

	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void URpgInventoryInteractionScreenWidget::BindInventoryScreenPresentation()
{
}

void URpgInventoryInteractionScreenWidget::UnbindInventoryScreenPresentation()
{
}

void URpgInventoryInteractionScreenWidget::ForwardInventoryInteractionContextToChildren()
{
}

void URpgInventoryInteractionScreenWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	(void)Navigator;
}

void URpgInventoryInteractionScreenWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	(void)OutGrids;
}

bool URpgInventoryInteractionScreenWidget::RouteInventoryPayloadToScreenSpecificTarget(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	(void)Payload;
	(void)GhostCenterScreenPosition;
	(void)bCommit;
	bOutTargetAddressed = false;
	return false;
}

void URpgInventoryInteractionScreenWidget::ClearInventoryScreenSpecificDragPreviews()
{
}

bool URpgInventoryInteractionScreenWidget::UpdateInventoryScreenSpecificControllerDragVisual(
	const FRpgInventoryDragPayload& Payload)
{
	(void)Payload;
	return false;
}

void URpgInventoryInteractionScreenWidget::RefreshInventoryScreenSpecificInteractionPresentation(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	(void)PreviewState;
	(void)bHasPayload;
	(void)bPendingRequest;
}

void URpgInventoryInteractionScreenWidget::EnsureInventoryInteractionObjects()
{
	if (!InventoryDragDropCoordinator)
	{
		InventoryDragDropCoordinator =
			NewObject<URpgInventoryDragDropCoordinator>(this);
		if (InventoryDragDropCoordinator)
		{
			InventoryDragDropCoordinator->Initialize(GetOwningPlayer());
		}
	}

	if (InventoryDragDropCoordinator &&
		InventoryDragDropCoordinator->GetInteractionSession())
	{
		InventoryDragDropCoordinator->GetInteractionSession()->OnInteractionStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleInventoryInteractionStateChanged);
	}

	if (!InventoryPanelNavigationCoordinator)
	{
		InventoryPanelNavigationCoordinator =
			NewObject<URpgInventoryPanelNavigationCoordinator>(this);
		if (InventoryPanelNavigationCoordinator)
		{
			InventoryPanelNavigationCoordinator->Initialize(
				GetOwningPlayer(),
				InventoryDragDropCoordinator);
		}
	}

	SetInventoryControllerCoordinators(
		InventoryPanelNavigationCoordinator,
		InventoryDragDropCoordinator);
}

void URpgInventoryInteractionScreenWidget::RefreshInventoryScreenNavigationPanels()
{
	if (!InventoryPanelNavigationCoordinator)
	{
		return;
	}

	InventoryPanelNavigationCoordinator->BeginPanelRefresh();
	RegisterInventoryScreenNavigationPanels(InventoryPanelNavigationCoordinator);
	InventoryPanelNavigationCoordinator->EndPanelRefresh();
}

void URpgInventoryInteractionScreenWidget::QueueDeferredInventoryScreenRefresh()
{
	if (bDeferredInventoryScreenRefreshQueued)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ExecuteDeferredInventoryScreenRefresh();
		return;
	}

	bDeferredInventoryScreenRefreshQueued = true;
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::ExecuteDeferredInventoryScreenRefresh));
}

void URpgInventoryInteractionScreenWidget::ExecuteDeferredInventoryScreenRefresh()
{
	bDeferredInventoryScreenRefreshQueued = false;
	if (!IsActivated())
	{
		return;
	}

	ForwardInventoryInteractionContextToChildren();
	RefreshInventoryScreenNavigationPanels();
	RefreshInventoryControllerFocus();
}

bool URpgInventoryInteractionScreenWidget::RouteInventoryPayloadAtScreenPosition(
	const FRpgInventoryDragPayload& Payload,
	FVector2D ScreenPosition,
	bool bCommit,
	const URpgInventoryDragDropOperation* DragOperation)
{
	const FRpgInventoryDragPayload ResolvedPayload = InventoryDragDropCoordinator
		? InventoryDragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	const FVector2D GhostCenterScreenPosition = DragOperation
		? DragOperation->ResolveDecoratorCenterScreen(ScreenPosition)
		: URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(
			ResolvedPayload,
			ScreenPosition);

	bool bTargetAddressed = false;
	const bool bScreenSpecificHandled =
		RouteInventoryPayloadToScreenSpecificTarget(
			ResolvedPayload,
			GhostCenterScreenPosition,
			bCommit,
			bTargetAddressed);
	if (bTargetAddressed)
	{
		return bScreenSpecificHandled;
	}

	if (RoutePayloadToSpatialGrid(ResolvedPayload, ScreenPosition, bCommit))
	{
		return true;
	}

	SwitchActivePointerDropTarget(nullptr);
	return false;
}

bool URpgInventoryInteractionScreenWidget::RoutePayloadToSpatialGrid(
	const FRpgInventoryDragPayload& Payload,
	FVector2D ScreenPosition,
	bool bCommit)
{
	TArray<URpgInventorySpatialGridWidget*> SpatialGrids;
	CollectSpatialGrids(SpatialGrids);
	for (URpgInventorySpatialGridWidget* SpatialGrid : SpatialGrids)
	{
		if (!SpatialGrid ||
			!SpatialGrid->CanAddressPayloadAtScreenPosition(Payload, ScreenPosition))
		{
			continue;
		}

		SwitchActivePointerDropTarget(SpatialGrid);
		if (bCommit)
		{
			return SpatialGrid->CommitPayloadAtScreenPosition(Payload, ScreenPosition);
		}

		SpatialGrid->PreviewPayloadAtScreenPosition(Payload, ScreenPosition);
		return true;
	}

	return false;
}

void URpgInventoryInteractionScreenWidget::SwitchActivePointerDropTarget(
	UWidget* NewTarget)
{
	if (ActivePointerDropTarget.Get() == NewTarget)
	{
		return;
	}

	ClearExternalDragPreviews();
	ActivePointerDropTarget = NewTarget;
}

void URpgInventoryInteractionScreenWidget::CollectSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	AppendInventoryScreenSpatialGrids(OutGrids);
}

void URpgInventoryInteractionScreenWidget::ClearExternalDragPreviews()
{
	ClearInventoryScreenSpecificDragPreviews();

	TArray<URpgInventorySpatialGridWidget*> SpatialGrids;
	CollectSpatialGrids(SpatialGrids);
	for (URpgInventorySpatialGridWidget* SpatialGrid : SpatialGrids)
	{
		if (SpatialGrid)
		{
			SpatialGrid->ClearExternalPreviewPayload();
		}
	}

	if (InventoryDragDropCoordinator)
	{
		InventoryDragDropCoordinator->ClearInteractionPreview();
	}
}

void URpgInventoryInteractionScreenWidget::HandleInventoryInteractionStateChanged(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	const URpgInventoryInteractionSession* InteractionSession =
		InventoryDragDropCoordinator
			? InventoryDragDropCoordinator->GetInteractionSession()
			: nullptr;

	if (!bHasPayload)
	{
		ActivePointerDropTarget.Reset();
		ClearExternalDragPreviews();
		ClearFreePointerDragVisual();
	}
	else if (
		InteractionSession &&
		InteractionSession->GetInputMode() ==
			ERpgInventoryInteractionInputMode::Controller)
	{
		if (!UpdateInventoryScreenSpecificControllerDragVisual(
			InteractionSession->GetPayload()))
		{
			// Spatial grids own their controller preview locally.
			ClearFreePointerDragVisual();
		}
	}
	else if (bHasLastPointerDragScreenPosition && InventoryDragDropCoordinator)
	{
		FRpgInventoryDragPayload Payload =
			InventoryDragDropCoordinator->GetHeldPayload();
		if (ActivePointerDragOperation)
		{
			ActivePointerDragOperation->SynchronizeFromInteractionSession();
			Payload = ActivePointerDragOperation->InventoryPayload;
		}

		if (!bPendingRequest &&
			PreviewState != ERpgInventoryInteractionPreviewState::Rejected &&
			!bRoutingPointerPreview)
		{
			TGuardValue<bool> RoutingGuard(bRoutingPointerPreview, true);
			RouteInventoryPayloadAtScreenPosition(
				Payload,
				LastPointerDragScreenPosition,
				false,
				ActivePointerDragOperation);
		}

		UpdateFreePointerDragVisual(
			Payload,
			LastPointerDragScreenPosition,
			ActivePointerDragOperation);
	}

	RefreshInventoryScreenSpecificInteractionPresentation(
		PreviewState,
		bHasPayload,
		bPendingRequest);
	BP_OnInventoryInteractionStateChanged(
		PreviewState,
		bHasPayload,
		bPendingRequest);
}

void URpgInventoryInteractionScreenWidget::HandleInventoryActionFeedback(
	FGameplayTag Channel,
	const FRpgInventoryActionFeedbackMessage& Message)
{
	if (Channel !=
			RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback ||
		!Message.IsAddressedTo(GetOwningPlayer()))
	{
		return;
	}

	if (PendingDropConfirmation.DoesFeedbackMatch(
			GetOwningPlayer(),
			Message))
	{
		if (Message.Result ==
			ERpgInventoryActionFeedbackResult::RequiresConfirmation)
		{
			if (OpenPendingDropConfirmation())
			{
				return;
			}

			// Missing authored presentation fails closed: never auto-confirm and never retain a hidden request.
			PendingDropConfirmation.Reset();
		}
		else
		{
			PendingDropConfirmation.Reset();
		}
	}

	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->ShowInventoryActionFeedback(Message);
	}
}

void URpgInventoryInteractionScreenWidget::DismissActiveContextMenuPresentation()
{
	TWeakObjectPtr<URpgInventoryContextMenuWidget> ContextMenuToClose =
		ActiveContextMenu;
	ActiveContextMenu.Reset();
	ActiveContextMenuSource.Reset();
	if (ContextMenuToClose.IsValid())
	{
		ContextMenuToClose->OnDeactivated().RemoveAll(this);
		ContextMenuToClose->CloseContextMenu();
	}
}

void URpgInventoryInteractionScreenWidget::DismissActiveSplitDialogPresentation()
{
	TWeakObjectPtr<URpgInventorySplitDialogWidget> SplitDialogToClose =
		ActiveSplitDialog;
	ActiveSplitDialog.Reset();
	ActiveSplitDialogSource.Reset();
	if (SplitDialogToClose.IsValid())
	{
		SplitDialogToClose->OnDeactivated().RemoveAll(this);
		SplitDialogToClose->CancelSplitDialog();
	}
}

void URpgInventoryInteractionScreenWidget::DismissActiveDropConfirmationPresentation()
{
	TWeakObjectPtr<URpgInventoryDropConfirmationDialogWidget>
		DropConfirmationToClose = ActiveDropConfirmation;
	ActiveDropConfirmation.Reset();
	PendingDropConfirmation.Reset();
	if (DropConfirmationToClose.IsValid())
	{
		DropConfirmationToClose->OnDeactivated().RemoveAll(this);
		DropConfirmationToClose->CancelDropConfirmation();
	}
}

void URpgInventoryInteractionScreenWidget::DismissInventoryModalPresentation()
{
	DismissActiveContextMenuPresentation();
	DismissActiveSplitDialogPresentation();
	DismissActiveDropConfirmationPresentation();
}

void URpgInventoryInteractionScreenWidget::HandleContextMenuDeactivated(
	URpgInventoryContextMenuWidget* DeactivatedMenu)
{
	if (DeactivatedMenu)
	{
		DeactivatedMenu->OnDeactivated().RemoveAll(this);
	}
	if (ActiveContextMenu.Get() == DeactivatedMenu)
	{
		ActiveContextMenu.Reset();
		ActiveContextMenuSource.Reset();
	}
}

void URpgInventoryInteractionScreenWidget::HandleSplitDialogDeactivated(
	URpgInventorySplitDialogWidget* DeactivatedDialog)
{
	if (DeactivatedDialog)
	{
		DeactivatedDialog->OnDeactivated().RemoveAll(this);
	}
	if (ActiveSplitDialog.Get() == DeactivatedDialog)
	{
		ActiveSplitDialog.Reset();
		ActiveSplitDialogSource.Reset();
	}
}

void URpgInventoryInteractionScreenWidget::HandleDropConfirmationDeactivated(
	URpgInventoryDropConfirmationDialogWidget* DeactivatedDialog)
{
	if (DeactivatedDialog)
	{
		DeactivatedDialog->OnDeactivated().RemoveAll(this);
	}
	if (ActiveDropConfirmation.Get() == DeactivatedDialog)
	{
		ActiveDropConfirmation.Reset();
		PendingDropConfirmation.Reset();
	}
}

void URpgInventoryInteractionScreenWidget::RegisterInventoryFeedbackListener()
{
	UnregisterInventoryFeedbackListener();
	if (!GetWorld())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(GetWorld());
	InventoryActionFeedbackHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryActionFeedbackMessage>(
			RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
			this,
			&ThisClass::HandleInventoryActionFeedback);
}

void URpgInventoryInteractionScreenWidget::UnregisterInventoryFeedbackListener()
{
	if (InventoryActionFeedbackHandle.IsValid())
	{
		InventoryActionFeedbackHandle.Unregister();
	}
}

void URpgInventoryInteractionScreenWidget::UpdateFreePointerDragVisual(
	const FRpgInventoryDragPayload& Payload,
	FVector2D PointerScreenPosition,
	URpgInventoryDragDropOperation* DragOperation,
	bool bCenterVisualOnScreenPosition)
{
	URpgInventoryDragVisualWidget* OperationVisual = nullptr;
	if (DragOperation)
	{
		DragOperation->SetScreenOwnedDragVisualActive(true);
		DragOperation->SynchronizeFromInteractionSession();
		if (DragOperation->DefaultDragVisual)
		{
			// UMG still carries pointer events, but its interpolated decorator is never painted.
			OperationVisual =
				Cast<URpgInventoryDragVisualWidget>(
					DragOperation->DefaultDragVisual);
		}
	}

	const URpgInventoryInteractionSession* Session =
		InventoryDragDropCoordinator
			? InventoryDragDropCoordinator->GetInteractionSession()
			: nullptr;
	FRpgInventoryDragPayload ResolvedPayload = Payload;
	if (Session)
	{
		// A listen server can acknowledge before NativeOnDrop returns. Never resurrect a cleared interaction from
		// the drag operation's stale payload copy after that synchronous acknowledgement.
		if (!Session->HasPayload())
		{
			ClearFreePointerDragVisual();
			return;
		}
		ResolvedPayload = Session->GetPayload();
	}
	else if (DragOperation)
	{
		ResolvedPayload = DragOperation->InventoryPayload;
	}

	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(ResolvedPayload))
	{
		ClearFreePointerDragVisual();
		return;
	}

	if (OperationVisual)
	{
		FreePointerDragCellSize =
			OperationVisual->GetConfiguredCellSize();
		FreePointerDragCellPadding =
			OperationVisual->GetConfiguredCellPadding();
	}

	TSubclassOf<URpgInventoryDragVisualWidget> VisualClass =
		FreeDragVisualWidgetClass;
	if (!VisualClass && OperationVisual)
	{
		VisualClass = OperationVisual->GetClass();
	}
	if (!VisualClass)
	{
		VisualClass = URpgInventoryDragVisualWidget::StaticClass();
	}

	if (FreePointerDragVisual &&
		!FreePointerDragVisual->IsA(VisualClass))
	{
		FreePointerDragVisual->RemoveFromParent();
		FreePointerDragVisual = nullptr;
		FreePointerDragVisualSize = FVector2D::ZeroVector;
		bFreePointerDragVisualConfigured = false;
	}

	if (!FreePointerDragVisual)
	{
		FreePointerDragVisual =
			CreateWidget<URpgInventoryDragVisualWidget>(
				GetOwningPlayer(),
				VisualClass);
		if (!FreePointerDragVisual)
		{
			return;
		}

		if (DragVisualCanvas)
		{
			if (UCanvasPanelSlot* DragCanvasSlot =
				DragVisualCanvas->AddChildToCanvas(
					FreePointerDragVisual))
			{
				DragCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
				DragCanvasSlot->SetAlignment(FVector2D::ZeroVector);
				DragCanvasSlot->SetPosition(FVector2D::ZeroVector);
				DragCanvasSlot->SetAutoSize(false);
				DragCanvasSlot->SetZOrder(1000);
			}
		}
		else
		{
			// Migration fallback until the authored screen supplies a top-most DragVisualCanvas.
			FreePointerDragVisual->AddToPlayerScreen(1100);
			FreePointerDragVisual->SetAlignmentInViewport(
				FVector2D::ZeroVector);
			FreePointerDragVisual->SetPositionInViewport(
				FVector2D::ZeroVector,
				true);
		}
	}

	const ERpgInventoryInteractionPreviewState PreviewState = Session
		? Session->GetPreviewState()
		: ERpgInventoryInteractionPreviewState::None;
	const bool bTargetRotated = InventoryDragDropCoordinator
		? InventoryDragDropCoordinator->GetTargetRotationForPayload(
			ResolvedPayload)
		: (
			ResolvedPayload.SourcePlacement.IsValid() &&
			ResolvedPayload.SourcePlacement.bRotated);

	const bool bNeedsVisualConfiguration =
		!bFreePointerDragVisualConfigured ||
		FreePointerDragConfiguredItem.Get() !=
			ResolvedPayload.ItemInstance ||
		FreePointerDragConfiguredEntryId !=
			ResolvedPayload.EntryId ||
		FreePointerDragConfiguredFootprint !=
			ResolvedPayload.ItemFootprint ||
		FreePointerDragConfiguredStackCount !=
			ResolvedPayload.StackCount ||
		FreePointerDragConfiguredSourceType !=
			ResolvedPayload.SourceType ||
		!FMath::IsNearlyEqual(
			FreePointerDragVisual->GetConfiguredCellSize(),
			FreePointerDragCellSize) ||
		!FMath::IsNearlyEqual(
			FreePointerDragVisual->GetConfiguredCellPadding(),
			FreePointerDragCellPadding);
	if (bNeedsVisualConfiguration)
	{
		FreePointerDragVisual->ConfigureFromPayload(
			ResolvedPayload,
			FreePointerDragCellSize,
			FreePointerDragCellPadding,
			PreviewState);
		FreePointerDragConfiguredItem =
			ResolvedPayload.ItemInstance.Get();
		FreePointerDragConfiguredEntryId =
			ResolvedPayload.EntryId;
		FreePointerDragConfiguredFootprint =
			ResolvedPayload.ItemFootprint;
		FreePointerDragConfiguredStackCount =
			ResolvedPayload.StackCount;
		FreePointerDragConfiguredSourceType =
			ResolvedPayload.SourceType;
		bFreePointerDragVisualConfigured = true;
	}
	else
	{
		FreePointerDragVisual->SetPreviewState(PreviewState);
	}

	FreePointerDragVisual->SetFootprintRotated(bTargetRotated);
	const FVector2D LocalVisualSize =
		FreePointerDragVisual->GetExactVisualSize();
	if (!FreePointerDragVisualSize.Equals(LocalVisualSize))
	{
		FreePointerDragVisualSize = LocalVisualSize;
		if (UCanvasPanelSlot* DragCanvasSlot =
			Cast<UCanvasPanelSlot>(FreePointerDragVisual->Slot))
		{
			DragCanvasSlot->SetSize(LocalVisualSize);
		}
		else
		{
			FreePointerDragVisual->SetDesiredSizeInViewport(
				LocalVisualSize);
		}
	}

	FVector2D PointerFraction(0.5f, 0.5f);
	if (
		!bCenterVisualOnScreenPosition &&
		ResolvedPayload.DragAnchor.bValid &&
		ResolvedPayload.DragAnchor.SourceScreenVisualSize.X >
			KINDA_SMALL_NUMBER &&
		ResolvedPayload.DragAnchor.SourceScreenVisualSize.Y >
			KINDA_SMALL_NUMBER)
	{
		PointerFraction.X = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourceScreenPointerOffset.X /
				ResolvedPayload.DragAnchor.SourceScreenVisualSize.X,
			0.0f,
			1.0f);
		PointerFraction.Y = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourceScreenPointerOffset.Y /
				ResolvedPayload.DragAnchor.SourceScreenVisualSize.Y,
			0.0f,
			1.0f);
	}
	else if (
		!bCenterVisualOnScreenPosition &&
		ResolvedPayload.DragAnchor.bValid &&
		ResolvedPayload.DragAnchor.SourceVisualSize.X >
			KINDA_SMALL_NUMBER &&
		ResolvedPayload.DragAnchor.SourceVisualSize.Y >
			KINDA_SMALL_NUMBER)
	{
		PointerFraction.X = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourcePointerOffset.X /
				ResolvedPayload.DragAnchor.SourceVisualSize.X,
			0.0f,
			1.0f);
		PointerFraction.Y = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourcePointerOffset.Y /
				ResolvedPayload.DragAnchor.SourceVisualSize.Y,
			0.0f,
			1.0f);
	}

	const bool bCanvasOwnedVisual =
		DragVisualCanvas &&
		FreePointerDragVisual->GetParent() == DragVisualCanvas;
	const FGeometry PlayerScreenGeometry =
		UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(
			GetOwningPlayer());
	const FGeometry VisualSpaceGeometry = bCanvasOwnedVisual
		? DragVisualCanvas->GetCachedGeometry()
		: PlayerScreenGeometry;
	const FVector2D VisualSpaceLocalSize =
		VisualSpaceGeometry.GetLocalSize();

	FVector2D ScreenVisualSize = FVector2D::ZeroVector;
	float FallbackViewportScale = 1.0f;
	if (
		VisualSpaceLocalSize.X > KINDA_SMALL_NUMBER &&
		VisualSpaceLocalSize.Y > KINDA_SMALL_NUMBER)
	{
		const FVector2D ScreenOrigin =
			VisualSpaceGeometry.LocalToAbsolute(
				FVector2D::ZeroVector);
		const FVector2D ScreenExtent =
			VisualSpaceGeometry.LocalToAbsolute(LocalVisualSize) -
			ScreenOrigin;
		ScreenVisualSize = FVector2D(
			FMath::Abs(ScreenExtent.X),
			FMath::Abs(ScreenExtent.Y));
	}
	else
	{
		const float ResolvedViewportScale =
			UWidgetLayoutLibrary::GetViewportScale(
				FreePointerDragVisual);
		FallbackViewportScale =
			ResolvedViewportScale > KINDA_SMALL_NUMBER
				? ResolvedViewportScale
				: 1.0f;
		ScreenVisualSize =
			LocalVisualSize * FallbackViewportScale;
	}

	const FVector2D ScreenTopLeft =
		PointerScreenPosition -
		PointerFraction * ScreenVisualSize;
	const FVector2D VisualSpaceLocalTopLeft =
		VisualSpaceLocalSize.X > KINDA_SMALL_NUMBER &&
			VisualSpaceLocalSize.Y > KINDA_SMALL_NUMBER
		? VisualSpaceGeometry.AbsoluteToLocal(ScreenTopLeft)
		: ScreenTopLeft / FallbackViewportScale;
	FreePointerDragVisual->SetRenderTranslation(
		VisualSpaceLocalTopLeft);

	const bool bSpatialTargetOwnsGhost =
		Session &&
		Session->GetSpatialPreviewDescriptor().bValid;
	const ESlateVisibility DesiredVisibility =
		bSpatialTargetOwnsGhost
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible;
	if (FreePointerDragVisual->GetVisibility() !=
		DesiredVisibility)
	{
		FreePointerDragVisual->SetVisibility(
			DesiredVisibility);
	}
}

void URpgInventoryInteractionScreenWidget::ClearFreePointerDragVisual()
{
	if (ActivePointerDragOperation)
	{
		// If the screen closes while Slate still owns the operation, return presentation to its decorator first.
		ActivePointerDragOperation->SetScreenOwnedDragVisualActive(false);
	}

	if (FreePointerDragVisual)
	{
		FreePointerDragVisual->RemoveFromParent();
		FreePointerDragVisual = nullptr;
	}

	ActivePointerDragOperation = nullptr;
	bHasLastPointerDragScreenPosition = false;
	LastPointerDragScreenPosition = FVector2D::ZeroVector;
	FreePointerDragVisualSize = FVector2D::ZeroVector;
	FreePointerDragConfiguredItem.Reset();
	FreePointerDragConfiguredEntryId.Invalidate();
	FreePointerDragConfiguredFootprint = FRpgInventoryGridSize();
	FreePointerDragConfiguredStackCount = INDEX_NONE;
	FreePointerDragConfiguredSourceType =
		ERpgInventoryDragSourceType::None;
	bFreePointerDragVisualConfigured = false;
}
