#include "RpgInventoryAddressSlotWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryAddressSlotWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryAddressSlotWidget, Log, All);

const FName URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName(
	TEXT("RpgInventoryAddressSlotViewModel"));

URpgInventoryAddressSlotWidget::URpgInventoryAddressSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgInventoryAddressSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (InCoordinator)
	{
		bAddressSlotStateReleased = false;
	}

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
	if (InSlotViewModel)
	{
		bAddressSlotStateReleased = false;
	}

	if (SlotViewModel != InSlotViewModel && InventoryPresentationHost)
	{
		InventoryPresentationHost->DismissInventoryPresentationForSource(this);
	}

	if (SlotViewModel == InSlotViewModel)
	{
		InjectAddressSlotViewModelIntoMvvm();
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

	InjectAddressSlotViewModelIntoMvvm();
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

void URpgInventoryAddressSlotWidget::SetInventoryPresentationHost(
	URpgInventoryInteractionScreenWidget* InPresentationHost)
{
	if (InPresentationHost)
	{
		bAddressSlotStateReleased = false;
	}
	if (InventoryPresentationHost &&
		InventoryPresentationHost != InPresentationHost)
	{
		InventoryPresentationHost->DismissInventoryPresentationForSource(this);
	}
	InventoryPresentationHost = InPresentationHost;
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
	CurrentDragDropVisualState = bHasExternalPreviewState
		? ExternalPreviewState
		: (DragDropCoordinator
			? DragDropCoordinator->GetInventoryAddressSlotVisualState(SlotViewModel, bSlotSelected && bInventoryPanelActive)
			: (bSlotSelected && bInventoryPanelActive ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal));

	BP_OnAddressSlotDragDropStateChanged(CurrentDragDropVisualState);
}

bool URpgInventoryAddressSlotWidget::PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	if (!DragDropCoordinator || !SlotViewModel || !URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
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

bool URpgInventoryAddressSlotWidget::CommitPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	ClearExternalPreviewPayload();
	return DragDropCoordinator && SlotViewModel &&
		DragDropCoordinator->CommitPayloadToTarget(Payload, MakeDropTarget());
}

void URpgInventoryAddressSlotWidget::ClearExternalPreviewPayload()
{
	if (!bHasExternalPreviewState)
	{
		return;
	}

	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	RefreshDragDropVisualState();
}

void URpgInventoryAddressSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetAddressSlotViewModel(Cast<URpgInventoryAddressSlotViewModel>(ListItemObject));
}

void URpgInventoryAddressSlotWidget::NativeDestruct()
{
	ReleaseAddressSlotState();
	Super::NativeDestruct();
}

void URpgInventoryAddressSlotWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	ReleaseAddressSlotState();
}

void URpgInventoryAddressSlotWidget::ReleaseAddressSlotState()
{
	if (bAddressSlotStateReleased)
	{
		return;
	}
	bAddressSlotStateReleased = true;
	StopAllAnimations();

	URpgInventoryDragDropCoordinator* ReleasedCoordinator = DragDropCoordinator;
	URpgInventoryAddressSlotViewModel* ReleasedViewModel = SlotViewModel;
	bool bOwnsCurrentPreviewTarget = false;
	if (bHasExternalPreviewState && ReleasedCoordinator && ReleasedViewModel)
	{
		if (const URpgInventoryInteractionSession* Session = ReleasedCoordinator->GetInteractionSession())
		{
			const FRpgInventoryDropTarget& Target = Session->GetTarget();
			bOwnsCurrentPreviewTarget = Target.TargetType == ERpgInventoryDropTargetType::PlayerInventorySlotAddress &&
				Target.SlotAddress == ReleasedViewModel->GetSlotAddress();
		}
	}

	if (ReleasedViewModel)
	{
		ReleasedViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}
	if (ReleasedCoordinator)
	{
		ReleasedCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	SlotViewModel = nullptr;
	DragDropCoordinator = nullptr;
	SetInventoryPresentationHost(nullptr);
	InjectAddressSlotViewModelIntoMvvm();
	bSlotSelected = false;
	bInventoryPanelActive = true;
	bPendingLeftClickAccept = false;
	PendingPointerDragAnchor = FRpgInventoryDragAnchor();
	bHasPendingPointerDragAnchor = false;
	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	// A target preview belongs to this presentation surface and may be cleared when it disappears. A server-pending
	// request remains owned by the screen session; ClearInteractionPreview intentionally preserves that request.
	if (bOwnsCurrentPreviewTarget && ReleasedCoordinator)
	{
		ReleasedCoordinator->ClearInteractionPreview();
	}

	BP_OnAddressSlotSelectionChanged(false);
	BP_OnAddressSlotViewModelSet(nullptr);
	BP_OnAddressSlotDragDropStateChanged(ERpgInventorySlotDragVisualState::Normal);
	BP_OnAddressSlotReleased();
}

bool URpgInventoryAddressSlotWidget::InjectAddressSlotViewModelIntoMvvm()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		// Specialized address presenters such as authored carry slots intentionally use their richer imperative
		// presentation hook. When an MVVM view is authored, the canonical exact-source contract below is mandatory.
		return false;
	}

	const FMVVMViewClass_Source* CompiledSource = ViewClass->GetSources().FindByPredicate(
		[](const FMVVMViewClass_Source& Candidate)
		{
			return Candidate.IsViewModel() &&
				Candidate.GetName() == AddressSlotViewModelSourceName;
		});
	if (!CompiledSource ||
		!CompiledSource->CanBeSet() ||
		!CompiledSource->IsOptional() ||
		CompiledSource->GetSourceClass() != URpgInventoryAddressSlotViewModel::StaticClass())
	{
		UE_LOG(
			LogRpgInventoryAddressSlotWidget,
			Error,
			TEXT("%s requires one settable optional manual MVVM source named %s with type RpgInventoryAddressSlotViewModel."),
			*GetNameSafe(this),
			*AddressSlotViewModelSourceName.ToString());
		return false;
	}

	if (View->GetViewModel(AddressSlotViewModelSourceName).GetObject() == SlotViewModel)
	{
		return true;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	if (SlotViewModel)
	{
		ViewModelInterface.SetObject(SlotViewModel);
		ViewModelInterface.SetInterface(SlotViewModel.Get());
	}

	if (!View->SetViewModel(AddressSlotViewModelSourceName, ViewModelInterface))
	{
		UE_LOG(
			LogRpgInventoryAddressSlotWidget,
			Error,
			TEXT("%s failed to inject its address-slot VM into MVVM source %s."),
			*GetNameSafe(this),
			*AddressSlotViewModelSourceName.ToString());
		return false;
	}

	return View->GetViewModel(AddressSlotViewModelSourceName).GetObject() == SlotViewModel;
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
	if (!DragDropCoordinator || !DragVisualClass)
	{
		return;
	}
	if (!DragDropCoordinator->BeginPointerDrag(Payload))
	{
		return;
	}
	Payload = DragDropCoordinator->ResolveInteractionPayload(Payload);

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		DragDropCoordinator->CancelHold();
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

	URpgInventoryDragVisualWidget* DragVisual =
		CreateWidget<URpgInventoryDragVisualWidget>(this, DragVisualClass);
	if (!DragVisual)
	{
		DragDropCoordinator->CancelHold();
		return;
	}

	DragVisual->ConfigureFromPayload(Payload, 70.0f, 2.0f);
	InventoryOperation->DefaultDragVisual = DragVisual;
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
	return DragDropCoordinator
		? DragDropCoordinator->GetAvailableContextActions(SlotViewModel)
		: TArray<ERpgInventoryContextAction>();
}

bool URpgInventoryAddressSlotWidget::ExecuteAddressContextAction(
	ERpgInventoryContextAction Action,
	FRpgInventoryItemId ExpectedItemId,
	int32 QuickAccessSlotIndex)
{
	URpgInventoryItemInstance* Item = SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
	if (!DragDropCoordinator || !Item || !ExpectedItemId.IsValid() ||
		Item->GetItemId() != ExpectedItemId ||
		!DragDropCoordinator->CanExecuteContextAction(SlotViewModel, Action))
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
		return RequestAddressItemDrop();
	case ERpgInventoryContextAction::QuickAccessBind:
		return DragDropCoordinator->BindPayloadToQuickAccessSlot(MakeDragPayload(false), QuickAccessSlotIndex);
	case ERpgInventoryContextAction::QuickAccessUnbind:
		return DragDropCoordinator->ClearQuickAccessBindingForPayload(MakeDragPayload(false));
	case ERpgInventoryContextAction::OpenContainer:
	case ERpgInventoryContextAction::Inspect:
		BP_OnDeferredAddressContextAction(Action, Item);
		return true;
	default:
		return false;
	}
}

bool URpgInventoryAddressSlotWidget::RequestAddressItemDrop(
	int32 StackCount,
	bool bConfirmed)
{
	if (!DragDropCoordinator || !SlotViewModel ||
		!DragDropCoordinator->CanExecuteContextAction(
			SlotViewModel,
			ERpgInventoryContextAction::Drop))
	{
		return false;
	}

	if (!bConfirmed && InventoryPresentationHost)
	{
		return InventoryPresentationHost->RequestInventoryDrop(
			this,
			StackCount);
	}

	return DragDropCoordinator->DropAddressSlot(
		SlotViewModel,
		StackCount,
		bConfirmed);
}

int32 URpgInventoryAddressSlotWidget::GetQuickAccessSlotIndex() const
{
	return DragDropCoordinator && SlotViewModel
		? DragDropCoordinator->FindQuickAccessSlotForPayload(MakeDragPayload(false))
		: INDEX_NONE;
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
	return InventoryPresentationHost &&
		InventoryPresentationHost->OpenInventoryContextMenu(
			this,
			Actions,
			ScreenPosition);
}

bool URpgInventoryAddressSlotWidget::RequestAddressSplitDialog()
{
	URpgInventoryItemInstance* Item = SlotViewModel ? SlotViewModel->GetItemInstance() : nullptr;
	const int32 StackCount = SlotViewModel ? SlotViewModel->GetStackCount() : 0;
	if (!Item || !DragDropCoordinator ||
		!DragDropCoordinator->CanExecuteContextAction(
			SlotViewModel,
			ERpgInventoryContextAction::Split))
	{
		return false;
	}
	const int32 MaximumCount = StackCount - 1;
	const int32 DefaultCount = FMath::Clamp(StackCount / 2, 1, MaximumCount);
	return InventoryPresentationHost &&
		InventoryPresentationHost->OpenInventorySplitDialog(
			this,
			Item->GetItemId(),
			1,
			MaximumCount,
			DefaultCount);
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
