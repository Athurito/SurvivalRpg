#include "RpgLoadoutSlotWidgets.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLoadoutSlotWidgets)

DEFINE_LOG_CATEGORY_STATIC(LogRpgLoadoutSlotWidgets, Log, All);

const FName URpgEquipmentSlotWidget::EquipmentSlotViewModelSourceName(
	TEXT("RpgEquipmentSlotViewModel"));

URpgEquipmentSlotWidget::URpgEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgEquipmentSlotWidget::BindInventoryPresentation(
	URpgEquipmentSlotViewModel* InSlotViewModel,
	const FRpgInventoryScreenPresentationContext& InContext)
{
	if (!InContext.IsComplete())
	{
		ReleaseInventoryPresentation();
		return;
	}

	// Preserve the previous host until the represented VM has dismissed any source-owned modal.
	SetEquipmentSlotViewModel(InSlotViewModel);
	SetDragDropCoordinator(InContext.DragDropCoordinator);
	SetPanelNavigationCoordinator(
		InContext.PanelNavigationCoordinator);
	SetInventoryPresentationHost(InContext.PresentationHost);
}

void URpgEquipmentSlotWidget::ReleaseInventoryPresentation()
{
	SetPanelNavigationCoordinator(nullptr);
	ReleaseEquipmentSlotState();
}

void URpgEquipmentSlotWidget::SetEquipmentSlotViewModel(URpgEquipmentSlotViewModel* InSlotViewModel)
{
	if (InSlotViewModel)
	{
		bEquipmentSlotStateReleased = false;
	}

	if (SlotViewModel != InSlotViewModel && InventoryPresentationHost)
	{
		InventoryPresentationHost->DismissInventoryPresentationForSource(this);
	}

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

	InjectEquipmentSlotViewModelIntoMvvm();
	RefreshDragDropVisualState();
}

void URpgEquipmentSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (InCoordinator)
	{
		bEquipmentSlotStateReleased = false;
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

void URpgEquipmentSlotWidget::SetPanelNavigationCoordinator(
	URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
}

void URpgEquipmentSlotWidget::SetInventoryPresentationHost(
	URpgInventoryInteractionScreenWidget* InPresentationHost)
{
	if (InPresentationHost)
	{
		bEquipmentSlotStateReleased = false;
	}
	if (InventoryPresentationHost &&
		InventoryPresentationHost != InPresentationHost)
	{
		InventoryPresentationHost->DismissInventoryPresentationForSource(this);
	}
	InventoryPresentationHost = InPresentationHost;
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
	URpgInventoryItemInstance* ItemInstance = GetRepresentedItem();
	if (!DragDropCoordinator || !ItemInstance)
	{
		return false;
	}

	return DragDropCoordinator->UnequipEquipmentItem(
		GetResolvedEquipmentSlot(),
		ItemInstance->GetItemId());
}

void URpgEquipmentSlotWidget::RefreshDragDropVisualState()
{
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;
	if (bHasExternalPreviewState)
	{
		CurrentDragDropVisualState = ExternalPreviewState;
	}
	else if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		CurrentDragDropVisualState = IsHeldSource()
			? ERpgInventorySlotDragVisualState::HeldSource
			: (DragDropCoordinator->PreviewDrop(MakeDropTarget())
				? ERpgInventorySlotDragVisualState::ValidTarget
				: ERpgInventorySlotDragVisualState::InvalidTarget);
	}

	BP_OnEquipmentSlotDragDropStateChanged(CurrentDragDropVisualState);
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
	return InventoryPresentationHost &&
		InventoryPresentationHost->OpenInventoryContextMenu(
			this,
			ScreenPosition);
}

bool URpgEquipmentSlotWidget::QueryInventoryContextActions(
	FRpgInventoryContextActionSnapshot& OutSnapshot) const
{
	OutSnapshot = FRpgInventoryContextActionSnapshot();
	const URpgInventoryItemInstance* ItemInstance =
		GetRepresentedItem();
	if (!DragDropCoordinator || !ItemInstance ||
		!ItemInstance->GetItemId().IsValid())
	{
		return false;
	}

	OutSnapshot.SourceKind =
		ERpgInventoryContextActionSourceKind::Equipment;
	OutSnapshot.ItemId = ItemInstance->GetItemId();
	OutSnapshot.EquipmentSlot =
		GetResolvedEquipmentSlot();
	OutSnapshot.Actions =
		DragDropCoordinator->GetAvailableContextActions(
			OutSnapshot.EquipmentSlot,
			OutSnapshot.ItemId);
	return OutSnapshot.IsValid();
}

bool URpgEquipmentSlotWidget::ExecuteInventoryContextAction(
	const FRpgInventoryContextActionSnapshot& ExpectedSnapshot,
	ERpgInventoryContextAction Action,
	int32 QuickAccessSlotIndex)
{
	(void)QuickAccessSlotIndex;
	FRpgInventoryContextActionSnapshot CurrentSnapshot;
	return QueryInventoryContextActions(CurrentSnapshot) &&
		ExpectedSnapshot.MatchesStableSource(CurrentSnapshot) &&
		CurrentSnapshot.Actions.Contains(Action) &&
		ExecuteEquipmentContextAction(
			Action,
			ExpectedSnapshot.ItemId);
}

bool URpgEquipmentSlotWidget::ExecuteEquipmentContextAction(
	ERpgInventoryContextAction Action,
	const FRpgInventoryItemId& ExpectedItemId)
{
	URpgInventoryItemInstance* ItemInstance = GetRepresentedItem();
	if (!DragDropCoordinator || !ItemInstance || !ExpectedItemId.IsValid() ||
		ItemInstance->GetItemId() != ExpectedItemId ||
		!DragDropCoordinator->CanExecuteContextAction(
			GetResolvedEquipmentSlot(),
			ExpectedItemId,
			Action))
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
		return InventoryPresentationHost
			? InventoryPresentationHost->RequestInventoryDrop(
				this,
				ExpectedItemId)
			: DragDropCoordinator &&
				DragDropCoordinator->DropEquipmentItem(
					GetResolvedEquipmentSlot(),
					ExpectedItemId);

	default:
		return false;
	}
}

void URpgEquipmentSlotWidget::NativeDestruct()
{
	ReleaseInventoryPresentation();
	Super::NativeDestruct();
}

void URpgEquipmentSlotWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	if (PanelNavigationCoordinator)
	{
		PanelNavigationCoordinator->NotifyEquipmentSlotFocused(this);
	}
}

void URpgEquipmentSlotWidget::ReleaseEquipmentSlotState()
{
	if (bEquipmentSlotStateReleased)
	{
		return;
	}
	bEquipmentSlotStateReleased = true;
	StopAllAnimations();

	URpgEquipmentSlotViewModel* ReleasedViewModel = SlotViewModel;
	URpgInventoryDragDropCoordinator* ReleasedCoordinator = DragDropCoordinator;
	const ERpgEquipmentSlot ReleasedEquipmentSlot = GetResolvedEquipmentSlot();
	bool bOwnsCurrentPreviewTarget = false;
	if (bHasExternalPreviewState && ReleasedCoordinator)
	{
		if (const URpgInventoryInteractionSession* Session = ReleasedCoordinator->GetInteractionSession())
		{
			const FRpgInventoryDropTarget& Target = Session->GetTarget();
			bOwnsCurrentPreviewTarget =
				Target.TargetType == ERpgInventoryDropTargetType::EquipmentSlot &&
				Target.EquipmentSlot == ReleasedEquipmentSlot;
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
	InjectEquipmentSlotViewModelIntoMvvm();
	bPendingLeftClickAccept = false;
	PendingPointerDragAnchor = FRpgInventoryDragAnchor();
	bHasPendingPointerDragAnchor = false;
	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	// This presentation surface may release its own hover preview, but never cancels a server-pending request.
	if (bOwnsCurrentPreviewTarget && ReleasedCoordinator)
	{
		ReleasedCoordinator->ClearInteractionPreview();
	}

	BP_OnEquipmentSlotDragDropStateChanged(ERpgInventorySlotDragVisualState::Normal);
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
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (PanelNavigationCoordinator)
		{
			PanelNavigationCoordinator->NotifyEquipmentSlotFocused(this);
		}
		if (RequestEquipmentContextMenu(InMouseEvent.GetScreenSpacePosition()))
		{
			return FReply::Handled();
		}
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
	InventoryOperation->Payload = GetRepresentedItem();
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
		RefreshDragDropVisualState();
	}
}

void URpgEquipmentSlotWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

bool URpgEquipmentSlotWidget::InjectEquipmentSlotViewModelIntoMvvm()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		if (GetClass() != StaticClass())
		{
			UE_LOG(
				LogRpgLoadoutSlotWidgets,
				Error,
				TEXT("%s has no compiled MVVM view. Author one optional manual %s source for equipment-slot data."),
				*GetNameSafe(this),
				*EquipmentSlotViewModelSourceName.ToString());
		}
		return false;
	}

	const FMVVMViewClass_Source* CompiledSource = ViewClass->GetSources().FindByPredicate(
		[](const FMVVMViewClass_Source& Candidate)
		{
			return Candidate.IsViewModel() &&
				Candidate.GetName() == EquipmentSlotViewModelSourceName;
		});
	if (!CompiledSource ||
		!CompiledSource->CanBeSet() ||
		!CompiledSource->IsOptional() ||
		CompiledSource->GetSourceClass() != URpgEquipmentSlotViewModel::StaticClass())
	{
		UE_LOG(
			LogRpgLoadoutSlotWidgets,
			Error,
			TEXT("%s requires one settable optional manual MVVM source named %s with type RpgEquipmentSlotViewModel."),
			*GetNameSafe(this),
			*EquipmentSlotViewModelSourceName.ToString());
		return false;
	}

	if (View->GetViewModel(EquipmentSlotViewModelSourceName).GetObject() == SlotViewModel)
	{
		return true;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	if (SlotViewModel)
	{
		ViewModelInterface.SetObject(SlotViewModel);
		ViewModelInterface.SetInterface(SlotViewModel.Get());
	}

	if (!View->SetViewModel(EquipmentSlotViewModelSourceName, ViewModelInterface))
	{
		UE_LOG(
			LogRpgLoadoutSlotWidgets,
			Error,
			TEXT("%s failed to inject its equipment-slot VM into MVVM source %s."),
			*GetNameSafe(this),
			*EquipmentSlotViewModelSourceName.ToString());
		return false;
	}

	return View->GetViewModel(EquipmentSlotViewModelSourceName).GetObject() == SlotViewModel;
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
