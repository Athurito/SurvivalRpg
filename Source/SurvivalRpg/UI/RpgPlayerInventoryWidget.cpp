#include "RpgPlayerInventoryWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryCarrySlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryFeedbackToastWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "TimerManager.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgPlayerInventoryWidget, Log, All);

namespace
{
	bool IsWidgetUnderScreenPosition(const UWidget* Widget, FVector2D ScreenPosition)
	{
		if (!Widget || Widget->GetVisibility() == ESlateVisibility::Collapsed || Widget->GetVisibility() == ESlateVisibility::Hidden)
		{
			return false;
		}

		const FGeometry Geometry = Widget->GetCachedGeometry();
		const FVector2D LocalPosition = Geometry.AbsoluteToLocal(ScreenPosition);
		const FVector2D LocalSize = Geometry.GetLocalSize();
		return LocalSize.X > KINDA_SMALL_NUMBER &&
			LocalSize.Y > KINDA_SMALL_NUMBER &&
			LocalPosition.X >= 0.0f &&
			LocalPosition.Y >= 0.0f &&
			LocalPosition.X <= LocalSize.X &&
			LocalPosition.Y <= LocalSize.Y;
	}
}

URpgPlayerInventoryWidget::URpgPlayerInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgPlayerInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (DragVisualCanvas)
	{
		// A fullscreen drag host must never steal pointer hits from the inventory beneath it.
		DragVisualCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();
	BP_OnPlayerInventoryViewModelReady(PlayerInventoryViewModel);
}

void URpgPlayerInventoryWidget::NativeOnActivated()
{
	EnsurePlayerInventoryCoordinator();
	EnsurePlayerInventoryPanelNavigator();
	BindPlayerInventoryViewModel();
	RegisterInventoryFeedbackListener();

	Super::NativeOnActivated();
	RefreshInventoryControllerFocus();
	QueueDeferredPlayerInventoryRefresh();
}

void URpgPlayerInventoryWidget::NativeOnDeactivated()
{
	if (PlayerDragDropCoordinator && PlayerDragDropCoordinator->HasHeldPayload())
	{
		PlayerDragDropCoordinator->ForceCancelInteraction();
	}
	ActivePointerDropTarget.Reset();
	ClearExternalDragPreviews();
	UnregisterInventoryFeedbackListener();
	ClearFreePointerDragVisual();
	if (PlayerDragDropCoordinator && PlayerDragDropCoordinator->GetInteractionSession())
	{
		PlayerDragDropCoordinator->GetInteractionSession()->OnInteractionStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryInteractionStateChanged);
	}
	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->RemoveFromParent();
		InventoryFeedbackToast = nullptr;
	}

	Super::NativeOnDeactivated();
}

void URpgPlayerInventoryWidget::NativeDestruct()
{
	UnregisterInventoryFeedbackListener();
	if (PlayerDragDropCoordinator && PlayerDragDropCoordinator->GetInteractionSession())
	{
		PlayerDragDropCoordinator->GetInteractionSession()->OnInteractionStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryInteractionStateChanged);
	}
	ClearFreePointerDragVisual();
	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->RemoveFromParent();
		InventoryFeedbackToast = nullptr;
	}
	Super::NativeDestruct();
}

bool URpgPlayerInventoryWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation || !URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
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
		PlayerDragDropCoordinator ? PlayerDragDropCoordinator->ResolveInteractionPayload(InventoryOperation->InventoryPayload) : InventoryOperation->InventoryPayload,
		LastPointerDragScreenPosition,
		InventoryOperation);
	return bHandled;
}

bool URpgPlayerInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation || !URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
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
		PlayerDragDropCoordinator ? PlayerDragDropCoordinator->ResolveInteractionPayload(InventoryOperation->InventoryPayload) : InventoryOperation->InventoryPayload,
		LastPointerDragScreenPosition,
		InventoryOperation);
	return bCommitted;
}

void URpgPlayerInventoryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	bool bGhostStillAddressesTarget = false;
	if (InventoryOperation && URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
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
			PlayerDragDropCoordinator ? PlayerDragDropCoordinator->ResolveInteractionPayload(InventoryOperation->InventoryPayload) : InventoryOperation->InventoryPayload,
			LastPointerDragScreenPosition,
			ActivePointerDragOperation);
		Super::NativeOnDragLeave(InDragDropEvent, InOperation);
		return;
	}

	ClearExternalDragPreviews();
	if (PlayerDragDropCoordinator)
	{
		const URpgInventoryInteractionSession* Session = PlayerDragDropCoordinator->GetInteractionSession();
		if (Session && Session->GetInputMode() == ERpgInventoryInteractionInputMode::Mouse && !Session->IsRequestPending())
		{
			PlayerDragDropCoordinator->CancelHold();
		}
	}
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryCoordinator()
{
	if (!PlayerDragDropCoordinator)
	{
		PlayerDragDropCoordinator = URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(this, GetOwningPlayer());
	}
	if (PlayerDragDropCoordinator && PlayerDragDropCoordinator->GetInteractionSession())
	{
		PlayerDragDropCoordinator->GetInteractionSession()->OnInteractionStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleInventoryInteractionStateChanged);
	}

	ForwardCoordinatorToChildren();
	SetInventoryControllerCoordinators(PlayerPanelNavigationCoordinator, PlayerDragDropCoordinator);
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryPanelNavigator()
{
	if (!PlayerPanelNavigationCoordinator)
	{
		PlayerPanelNavigationCoordinator = URpgInventoryPanelNavigationCoordinator::CreateInventoryPanelNavigationCoordinator(this, GetOwningPlayer(), PlayerDragDropCoordinator);
	}

	SetInventoryControllerCoordinators(PlayerPanelNavigationCoordinator, PlayerDragDropCoordinator);
	RegisterPlayerInventoryNavigationPanels();
}

void URpgPlayerInventoryWidget::BindPlayerInventoryViewModel()
{
	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();

	if (PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel->BindPlayerController(GetOwningPlayer());
	}

	RefreshPlayerInventoryViews();
}

void URpgPlayerInventoryWidget::RefreshPlayerInventoryViews()
{
	RefreshGearSlots();
	RefreshSlotGroups();
	RefreshActionBar();
	RegisterPlayerInventoryNavigationPanels();
}

FString URpgPlayerInventoryWidget::GetPlayerInventoryWidgetDebugSummary() const
{
	const int32 CarryCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetCarryGroups().Num() : INDEX_NONE;
	const int32 InventoryGroupCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetInventoryGroups().Num() : INDEX_NONE;
	const int32 ActionBarCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetActionBarSlots().Num() : INDEX_NONE;

	return FString::Printf(
		TEXT("PlayerInventoryWidget VM=%s Coordinator=%s Carry1=%s Carry2=%s Offhand=%s Pockets=%s Backpack=%s ActionBar=%s CarryGroups=%d InventoryGroups=%d ActionBarSlots=%d"),
		*GetNameSafe(PlayerInventoryViewModel),
		*GetNameSafe(PlayerDragDropCoordinator),
		*GetNameSafe(Carry_Weapon1),
		*GetNameSafe(Carry_Weapon2),
		*GetNameSafe(Carry_Offhand),
		*GetNameSafe(Content_Pockets),
		*GetNameSafe(Content_Backpack),
		*GetNameSafe(ActionBarTileView),
		CarryCount,
		InventoryGroupCount,
		ActionBarCount);
}

void URpgPlayerInventoryWidget::RefreshSlotGroups()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	auto ReportMissingContentHost = [this](URpgInventorySlotGroupWidget* GroupWidget, FName BindingName)
	{
		if (!GroupWidget && !ReportedInvalidPlayerBindings.Contains(BindingName))
		{
			ReportedInvalidPlayerBindings.Add(BindingName);
			UE_LOG(LogRpgPlayerInventoryWidget, Warning,
				TEXT("%s has no direct %s widget. Add an RpgInventorySlotGroupWidget with that exact BindWidget name; player InventoryGroupsList fallback was removed."),
				*GetNameSafe(this),
				*BindingName.ToString());
		}
	};

	auto BindContentGroup = [this, &ReportMissingContentHost](
		URpgInventorySlotGroupWidget* GroupWidget,
		URpgInventorySlotGroupViewModel* GroupViewModel,
		FName BindingName)
	{
		if (!GroupWidget)
		{
			ReportMissingContentHost(GroupWidget, BindingName);
			return;
		}

		GroupWidget->SetDragDropCoordinator(PlayerDragDropCoordinator);
		GroupWidget->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Content"));
		GroupWidget->SetSlotGroupViewModel(GroupViewModel);
		if (URpgInventorySpatialGridWidget* SpatialGrid = GroupWidget->GetSpatialGridWidget())
		{
			SpatialGrid->SetContextMenuWidgetClass(ContextMenuWidgetClass);
		}
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(GroupWidget->Slot))
		{
			// Spatial children must own their complete title + grid geometry; undersized hosts break Slate hit testing.
			CanvasSlot->SetAutoSize(true);
		}
		GroupWidget->SetVisibility(GroupViewModel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	auto BindCarrySlot = [this](
		UWidget* BoundWidget,
		FName BindingName,
		URpgInventorySlotGroupViewModel* GroupViewModel)
	{
		URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, true);
		if (!CarrySlot)
		{
			return;
		}

		CarrySlot->SetDragDropCoordinator(PlayerDragDropCoordinator);
		CarrySlot->SetContextMenuWidgetClass(ContextMenuWidgetClass);
		CarrySlot->SetCarrySlotGroupViewModel(GroupViewModel);
		CarrySlot->SetVisibility(GroupViewModel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	BindCarrySlot(Carry_Weapon1, TEXT("Carry_Weapon1"), PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId));
	BindCarrySlot(Carry_Weapon2, TEXT("Carry_Weapon2"), PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId));
	BindCarrySlot(Carry_Offhand, TEXT("Carry_Offhand"), PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId));

	BindContentGroup(Content_Pockets, PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::PocketsGroupId), TEXT("Content_Pockets"));
	BindContentGroup(Content_Backpack, FindEquipmentProvidedContentGroup(TEXT("Backpack")), TEXT("Content_Backpack"));
	BindContentGroup(Content_Belt, FindEquipmentProvidedContentGroup(TEXT("Belt")), TEXT("Content_Belt"));
	BindContentGroup(Content_Pouch, FindEquipmentProvidedContentGroup(TEXT("Pouch")), TEXT("Content_Pouch"));
	BindContentGroup(Content_ResourceBag, FindEquipmentProvidedContentGroup(TEXT("ResourceBag")), TEXT("Content_ResourceBag"));
}

void URpgPlayerInventoryWidget::RefreshActionBar()
{
	if (!PlayerInventoryViewModel || !ActionBarTileView)
	{
		return;
	}

	ActionBarTileView->SetActionBarSlotItems(PlayerInventoryViewModel->GetActionBarSlots());
	if (PlayerPanelNavigationCoordinator)
	{
		PlayerPanelNavigationCoordinator->RegisterActionBarPanel(TEXT("Actionbar"), ActionBarTileView);
	}
}

void URpgPlayerInventoryWidget::RefreshGearSlots()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	SetGearSlotViewModel(Gear_Head, ERpgEquipmentSlot::Head, false);
	SetGearSlotViewModel(Gear_Chest, ERpgEquipmentSlot::Chest, false);
	SetGearSlotViewModel(Gear_Hands, ERpgEquipmentSlot::Hands, false);
	SetGearSlotViewModel(Gear_Legs, ERpgEquipmentSlot::Legs, false);
	SetGearSlotViewModel(Gear_Feet, ERpgEquipmentSlot::Feet, false);

	SetGearSlotViewModel(Gear_Backpack, ERpgEquipmentSlot::Backpack, true);
	SetGearSlotViewModel(Gear_Belt, ERpgEquipmentSlot::Belt, true);
	SetGearSlotViewModel(Gear_Pouch, ERpgEquipmentSlot::Pouch, true);
	SetGearSlotViewModel(Gear_ResourceBag, ERpgEquipmentSlot::ResourceBag, true);
}

void URpgPlayerInventoryWidget::HandleGearSlotsChanged()
{
	RefreshGearSlots();
}

void URpgPlayerInventoryWidget::HandleSlotGroupsChanged()
{
	RefreshSlotGroups();
	QueueDeferredPlayerInventoryRefresh();
}

void URpgPlayerInventoryWidget::HandleActionBarSlotsChanged()
{
	RefreshActionBar();
}

void URpgPlayerInventoryWidget::HandleInventoryInteractionStateChanged(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	const URpgInventoryInteractionSession* InteractionSession = PlayerDragDropCoordinator
		? PlayerDragDropCoordinator->GetInteractionSession()
		: nullptr;
	if (!bHasPayload)
	{
		ActivePointerDropTarget.Reset();
		ClearExternalDragPreviews();
		ClearFreePointerDragVisual();
	}
	else if (InteractionSession &&
		InteractionSession->GetInputMode() == ERpgInventoryInteractionInputMode::Controller)
	{
		if (URpgInventoryCarrySlotWidget* CarrySlot = FindControllerPreviewCarrySlot())
		{
			UpdateControllerCarryDragVisual(InteractionSession->GetPayload(), CarrySlot);
		}
		else
		{
			// Spatial grids own their controller preview locally; no screen ghost may remain on the previous carry slot.
			ClearFreePointerDragVisual();
		}
	}
	else if (bHasLastPointerDragScreenPosition && PlayerDragDropCoordinator)
	{
		FRpgInventoryDragPayload Payload = PlayerDragDropCoordinator->GetHeldPayload();
		if (ActivePointerDragOperation)
		{
			ActivePointerDragOperation->SynchronizeFromInteractionSession();
			Payload = ActivePointerDragOperation->InventoryPayload;
		}

		if (!bPendingRequest && PreviewState != ERpgInventoryInteractionPreviewState::Rejected && !bRoutingPointerPreview)
		{
			TGuardValue<bool> RoutingGuard(bRoutingPointerPreview, true);
			RouteInventoryPayloadAtScreenPosition(
				Payload,
				LastPointerDragScreenPosition,
				false,
				ActivePointerDragOperation);
		}
		UpdateFreePointerDragVisual(Payload, LastPointerDragScreenPosition, ActivePointerDragOperation);
	}

	// Carry slots expose semantic Pending/Rejected state through their presentation hook. Refresh all three because
	// the interaction session target can change without replacing the held payload delegate they otherwise observe.
	auto RefreshCarryPresentation = [this](UWidget* BoundWidget, FName BindingName)
	{
		if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false))
		{
			CarrySlot->RefreshCarrySlotPresentation();
		}
	};
	RefreshCarryPresentation(Carry_Weapon1, TEXT("Carry_Weapon1"));
	RefreshCarryPresentation(Carry_Weapon2, TEXT("Carry_Weapon2"));
	RefreshCarryPresentation(Carry_Offhand, TEXT("Carry_Offhand"));
	BP_OnInventoryInteractionStateChanged(PreviewState, bHasPayload, bPendingRequest);
}

void URpgPlayerInventoryWidget::HandleInventoryActionFeedback(
	FGameplayTag Channel,
	const FRpgInventoryActionFeedbackMessage& Message)
{
	if (Channel != RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback)
	{
		return;
	}

	if (URpgInventoryFeedbackToastWidget* Toast = EnsureInventoryFeedbackToast())
	{
		Toast->ShowInventoryActionFeedback(Message);
	}
}

void URpgPlayerInventoryWidget::RegisterInventoryFeedbackListener()
{
	UnregisterInventoryFeedbackListener();
	if (!GetWorld())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(GetWorld());
	InventoryActionFeedbackHandle = MessageSubsystem.RegisterListener<FRpgInventoryActionFeedbackMessage>(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		this,
		&ThisClass::HandleInventoryActionFeedback);
}

void URpgPlayerInventoryWidget::UnregisterInventoryFeedbackListener()
{
	if (InventoryActionFeedbackHandle.IsValid())
	{
		InventoryActionFeedbackHandle.Unregister();
	}
}

URpgInventoryFeedbackToastWidget* URpgPlayerInventoryWidget::EnsureInventoryFeedbackToast()
{
	if (InventoryFeedbackToast)
	{
		return InventoryFeedbackToast;
	}

	TSubclassOf<URpgInventoryFeedbackToastWidget> ToastClass = FeedbackToastWidgetClass;
	if (!ToastClass)
	{
		ToastClass = URpgInventoryFeedbackToastWidget::StaticClass();
	}
	InventoryFeedbackToast = CreateWidget<URpgInventoryFeedbackToastWidget>(GetOwningPlayer(), ToastClass);
	if (InventoryFeedbackToast)
	{
		InventoryFeedbackToast->AddToPlayerScreen(250);
	}
	return InventoryFeedbackToast;
}

void URpgPlayerInventoryWidget::UpdateFreePointerDragVisual(
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
			// FUMG still carries pointer events, but its hard-coded 150 ms decorator interpolation is never painted.
			OperationVisual = Cast<URpgInventoryDragVisualWidget>(DragOperation->DefaultDragVisual);
		}
	}

	const URpgInventoryInteractionSession* Session = PlayerDragDropCoordinator
		? PlayerDragDropCoordinator->GetInteractionSession()
		: nullptr;
	FRpgInventoryDragPayload ResolvedPayload = Payload;
	if (Session)
	{
		// A listen server may acknowledge the drop before NativeOnDrop returns. Never resurrect a cleared
		// interaction from the drag operation's stale copy after that synchronous acknowledgement.
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
		FreePointerDragCellSize = OperationVisual->GetConfiguredCellSize();
		FreePointerDragCellPadding = OperationVisual->GetConfiguredCellPadding();
	}

	TSubclassOf<URpgInventoryDragVisualWidget> VisualClass = FreeDragVisualWidgetClass;
	if (!VisualClass && OperationVisual)
	{
		VisualClass = OperationVisual->GetClass();
	}
	if (!VisualClass)
	{
		VisualClass = URpgInventoryDragVisualWidget::StaticClass();
	}

	if (FreePointerDragVisual && !FreePointerDragVisual->IsA(VisualClass))
	{
		FreePointerDragVisual->RemoveFromParent();
		FreePointerDragVisual = nullptr;
		FreePointerDragVisualSize = FVector2D::ZeroVector;
		bFreePointerDragVisualConfigured = false;
	}
	if (!FreePointerDragVisual)
	{
		FreePointerDragVisual = CreateWidget<URpgInventoryDragVisualWidget>(GetOwningPlayer(), VisualClass);
		if (!FreePointerDragVisual)
		{
			return;
		}
		if (DragVisualCanvas)
		{
			if (UCanvasPanelSlot* DragCanvasSlot = DragVisualCanvas->AddChildToCanvas(FreePointerDragVisual))
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
			// Migration fallback: remain above the inventory and modal-adjacent decorators until DragVisualCanvas exists.
			FreePointerDragVisual->AddToPlayerScreen(1100);
			FreePointerDragVisual->SetAlignmentInViewport(FVector2D::ZeroVector);
			FreePointerDragVisual->SetPositionInViewport(FVector2D::ZeroVector, true);
		}
	}

	const ERpgInventoryInteractionPreviewState PreviewState = Session
		? Session->GetPreviewState()
		: ERpgInventoryInteractionPreviewState::None;
	const bool bTargetRotated = PlayerDragDropCoordinator
		? PlayerDragDropCoordinator->GetTargetRotationForPayload(ResolvedPayload)
		: (ResolvedPayload.SourcePlacement.IsValid() && ResolvedPayload.SourcePlacement.bRotated);

	const bool bNeedsVisualConfiguration = !bFreePointerDragVisualConfigured ||
		FreePointerDragConfiguredItem.Get() != ResolvedPayload.ItemInstance ||
		FreePointerDragConfiguredEntryId != ResolvedPayload.EntryId ||
		FreePointerDragConfiguredFootprint != ResolvedPayload.ItemFootprint ||
		FreePointerDragConfiguredStackCount != ResolvedPayload.StackCount ||
		FreePointerDragConfiguredSourceType != ResolvedPayload.SourceType ||
		!FMath::IsNearlyEqual(FreePointerDragVisual->GetConfiguredCellSize(), FreePointerDragCellSize) ||
		!FMath::IsNearlyEqual(FreePointerDragVisual->GetConfiguredCellPadding(), FreePointerDragCellPadding);
	if (bNeedsVisualConfiguration)
	{
		FreePointerDragVisual->ConfigureFromPayload(
			ResolvedPayload,
			FreePointerDragCellSize,
			FreePointerDragCellPadding,
			PreviewState);
		FreePointerDragConfiguredItem = ResolvedPayload.ItemInstance.Get();
		FreePointerDragConfiguredEntryId = ResolvedPayload.EntryId;
		FreePointerDragConfiguredFootprint = ResolvedPayload.ItemFootprint;
		FreePointerDragConfiguredStackCount = ResolvedPayload.StackCount;
		FreePointerDragConfiguredSourceType = ResolvedPayload.SourceType;
		bFreePointerDragVisualConfigured = true;
	}
	else
	{
		// Pointer movement changes only render translation. Semantic feedback is paint-only and does not
		// rebuild the icon brush or the widget layout.
		FreePointerDragVisual->SetPreviewState(PreviewState);
	}
	FreePointerDragVisual->SetFootprintRotated(bTargetRotated);
	const FVector2D LocalVisualSize = FreePointerDragVisual->GetExactVisualSize();
	if (!FreePointerDragVisualSize.Equals(LocalVisualSize))
	{
		FreePointerDragVisualSize = LocalVisualSize;
		if (UCanvasPanelSlot* DragCanvasSlot = Cast<UCanvasPanelSlot>(FreePointerDragVisual->Slot))
		{
			DragCanvasSlot->SetSize(LocalVisualSize);
		}
		else
		{
			FreePointerDragVisual->SetDesiredSizeInViewport(LocalVisualSize);
		}
	}

	FVector2D PointerFraction(0.5f, 0.5f);
	if (!bCenterVisualOnScreenPosition && ResolvedPayload.DragAnchor.bValid &&
		ResolvedPayload.DragAnchor.SourceScreenVisualSize.X > KINDA_SMALL_NUMBER &&
		ResolvedPayload.DragAnchor.SourceScreenVisualSize.Y > KINDA_SMALL_NUMBER)
	{
		PointerFraction.X = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourceScreenPointerOffset.X / ResolvedPayload.DragAnchor.SourceScreenVisualSize.X,
			0.0f,
			1.0f);
		PointerFraction.Y = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourceScreenPointerOffset.Y / ResolvedPayload.DragAnchor.SourceScreenVisualSize.Y,
			0.0f,
			1.0f);
	}
	else if (!bCenterVisualOnScreenPosition && ResolvedPayload.DragAnchor.bValid &&
		ResolvedPayload.DragAnchor.SourceVisualSize.X > KINDA_SMALL_NUMBER &&
		ResolvedPayload.DragAnchor.SourceVisualSize.Y > KINDA_SMALL_NUMBER)
	{
		PointerFraction.X = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourcePointerOffset.X / ResolvedPayload.DragAnchor.SourceVisualSize.X,
			0.0f,
			1.0f);
		PointerFraction.Y = FMath::Clamp(
			ResolvedPayload.DragAnchor.SourcePointerOffset.Y / ResolvedPayload.DragAnchor.SourceVisualSize.Y,
			0.0f,
			1.0f);
	}

	const bool bCanvasOwnedVisual = DragVisualCanvas && FreePointerDragVisual->GetParent() == DragVisualCanvas;
	const FGeometry PlayerScreenGeometry = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(GetOwningPlayer());
	const FGeometry VisualSpaceGeometry = bCanvasOwnedVisual
		? DragVisualCanvas->GetCachedGeometry()
		: PlayerScreenGeometry;
	const FVector2D VisualSpaceLocalSize = VisualSpaceGeometry.GetLocalSize();
	FVector2D ScreenVisualSize = FVector2D::ZeroVector;
	float FallbackViewportScale = 1.0f;
	if (VisualSpaceLocalSize.X > KINDA_SMALL_NUMBER && VisualSpaceLocalSize.Y > KINDA_SMALL_NUMBER)
	{
		const FVector2D ScreenOrigin = VisualSpaceGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ScreenExtent = VisualSpaceGeometry.LocalToAbsolute(LocalVisualSize) - ScreenOrigin;
		ScreenVisualSize = FVector2D(FMath::Abs(ScreenExtent.X), FMath::Abs(ScreenExtent.Y));
	}
	else
	{
		const float ResolvedViewportScale = UWidgetLayoutLibrary::GetViewportScale(FreePointerDragVisual);
		FallbackViewportScale = ResolvedViewportScale > KINDA_SMALL_NUMBER ? ResolvedViewportScale : 1.0f;
		ScreenVisualSize = LocalVisualSize * FallbackViewportScale;
	}
	const FVector2D ScreenTopLeft = PointerScreenPosition - PointerFraction * ScreenVisualSize;
	const FVector2D VisualSpaceLocalTopLeft = VisualSpaceLocalSize.X > KINDA_SMALL_NUMBER && VisualSpaceLocalSize.Y > KINDA_SMALL_NUMBER
		? VisualSpaceGeometry.AbsoluteToLocal(ScreenTopLeft)
		: ScreenTopLeft / FallbackViewportScale;
	FreePointerDragVisual->SetRenderTranslation(VisualSpaceLocalTopLeft);

	const bool bSpatialTargetOwnsGhost = Session && Session->GetSpatialPreviewDescriptor().bValid;
	const ESlateVisibility DesiredVisibility = bSpatialTargetOwnsGhost
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible;
	if (FreePointerDragVisual->GetVisibility() != DesiredVisibility)
	{
		FreePointerDragVisual->SetVisibility(DesiredVisibility);
	}
}

void URpgPlayerInventoryWidget::UpdateControllerCarryDragVisual(
	const FRpgInventoryDragPayload& Payload,
	URpgInventoryCarrySlotWidget* CarrySlotWidget)
{
	if (!CarrySlotWidget)
	{
		ClearFreePointerDragVisual();
		return;
	}

	const FGeometry CarryGeometry = CarrySlotWidget->GetCachedGeometry();
	const FVector2D CarryLocalSize = CarryGeometry.GetLocalSize();
	if (CarryLocalSize.X <= KINDA_SMALL_NUMBER || CarryLocalSize.Y <= KINDA_SMALL_NUMBER)
	{
		ClearFreePointerDragVisual();
		return;
	}

	const FVector2D CarryCenterScreenPosition = CarryGeometry.LocalToAbsolute(CarryLocalSize * 0.5f);
	UpdateFreePointerDragVisual(Payload, CarryCenterScreenPosition, nullptr, true);
}

URpgInventoryCarrySlotWidget* URpgPlayerInventoryWidget::FindControllerPreviewCarrySlot() const
{
	auto ResolveTargetedCarrySlot = [this](UWidget* BoundWidget, FName BindingName)
		-> URpgInventoryCarrySlotWidget*
	{
		URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false);
		return CarrySlot &&
			CarrySlot->GetCarryInteractionPreviewState() != ERpgInventoryInteractionPreviewState::None
			? CarrySlot
			: nullptr;
	};

	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveTargetedCarrySlot(Carry_Weapon1, TEXT("Carry_Weapon1")))
	{
		return CarrySlot;
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveTargetedCarrySlot(Carry_Weapon2, TEXT("Carry_Weapon2")))
	{
		return CarrySlot;
	}
	return ResolveTargetedCarrySlot(Carry_Offhand, TEXT("Carry_Offhand"));
}

void URpgPlayerInventoryWidget::ClearFreePointerDragVisual()
{
	if (ActivePointerDragOperation)
	{
		// If the screen is deactivated while Slate still owns the operation, hand presentation back to
		// its decorator before removing the screen-local ghost.
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
	FreePointerDragConfiguredSourceType = ERpgInventoryDragSourceType::None;
	bFreePointerDragVisualConfigured = false;
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryViewModel()
{
	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->InitializeSources();
		for (const FMVVMView_Source& Source : View->GetSources())
		{
			if (URpgPlayerInventoryViewModel* ExistingViewModel = Cast<URpgPlayerInventoryViewModel>(Source.Source))
			{
				if (PlayerInventoryViewModel != ExistingViewModel)
				{
					bViewModelDelegatesBound = false;
				}
				PlayerInventoryViewModel = ExistingViewModel;
				return;
			}
		}
	}

	if (!PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel = NewObject<URpgPlayerInventoryViewModel>(this);
	}
}

void URpgPlayerInventoryWidget::BindViewModelDelegates()
{
	if (!PlayerInventoryViewModel || bViewModelDelegatesBound)
	{
		return;
	}

	PlayerInventoryViewModel->OnGearSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleGearSlotsChanged);
	PlayerInventoryViewModel->OnSlotGroupsChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotGroupsChanged);
	PlayerInventoryViewModel->OnActionBarSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleActionBarSlotsChanged);
	bViewModelDelegatesBound = true;
}

void URpgPlayerInventoryWidget::SetGearSlotViewModel(URpgEquipmentSlotWidget* GearSlotWidget, ERpgEquipmentSlot EquipmentSlot, bool bBagSlot) const
{
	if (!GearSlotWidget || !PlayerInventoryViewModel)
	{
		return;
	}

	GearSlotWidget->SetDragDropCoordinator(PlayerDragDropCoordinator);
	GearSlotWidget->SetContextMenuWidgetClass(ContextMenuWidgetClass);
	GearSlotWidget->SetEquipmentSlotViewModel(
		bBagSlot
			? PlayerInventoryViewModel->GetBagSlot(EquipmentSlot)
			: PlayerInventoryViewModel->GetArmorSlot(EquipmentSlot));
}

void URpgPlayerInventoryWidget::ForwardCoordinatorToChildren()
{
	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneContentGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget)
		{
			GroupWidget->SetDragDropCoordinator(PlayerDragDropCoordinator);
			if (URpgInventorySpatialGridWidget* SpatialGrid = GroupWidget->GetSpatialGridWidget())
			{
				SpatialGrid->SetContextMenuWidgetClass(ContextMenuWidgetClass);
			}
		}
	}

	auto ForwardCarrySlot = [this](UWidget* BoundWidget, FName BindingName)
	{
		if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false))
		{
			CarrySlot->SetDragDropCoordinator(PlayerDragDropCoordinator);
			CarrySlot->SetContextMenuWidgetClass(ContextMenuWidgetClass);
		}
	};
	ForwardCarrySlot(Carry_Weapon1, TEXT("Carry_Weapon1"));
	ForwardCarrySlot(Carry_Weapon2, TEXT("Carry_Weapon2"));
	ForwardCarrySlot(Carry_Offhand, TEXT("Carry_Offhand"));

	if (ActionBarTileView)
	{
		ActionBarTileView->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}

	auto ForwardGearSlot = [this](URpgEquipmentSlotWidget* GearSlot)
	{
		if (GearSlot)
		{
			GearSlot->SetDragDropCoordinator(PlayerDragDropCoordinator);
			GearSlot->SetContextMenuWidgetClass(ContextMenuWidgetClass);
		}
	};
	ForwardGearSlot(Gear_Head);
	ForwardGearSlot(Gear_Chest);
	ForwardGearSlot(Gear_Hands);
	ForwardGearSlot(Gear_Legs);
	ForwardGearSlot(Gear_Feet);
	ForwardGearSlot(Gear_Backpack);
	ForwardGearSlot(Gear_Belt);
	ForwardGearSlot(Gear_Pouch);
	ForwardGearSlot(Gear_ResourceBag);
}

void URpgPlayerInventoryWidget::RegisterPlayerInventoryNavigationPanels()
{
	if (!PlayerPanelNavigationCoordinator)
	{
		return;
	}

	PlayerPanelNavigationCoordinator->BeginPanelRefresh();

	if (Gear_Head)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Head"), Gear_Head);
	}
	if (Gear_Chest)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Chest"), Gear_Chest);
	}
	if (Gear_Hands)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Hands"), Gear_Hands);
	}
	if (Gear_Legs)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Legs"), Gear_Legs);
	}
	if (Gear_Feet)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Feet"), Gear_Feet);
	}
	if (Gear_Backpack)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Backpack"), Gear_Backpack);
	}
	if (Gear_Belt)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Belt"), Gear_Belt);
	}
	if (Gear_Pouch)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Pouch"), Gear_Pouch);
	}
	if (Gear_ResourceBag)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.ResourceBag"), Gear_ResourceBag);
	}


	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon1, TEXT("Carry_Weapon1"), false))
	{
		PlayerPanelNavigationCoordinator->RegisterCarrySlotPanel(TEXT("Carry.Weapon1"), CarrySlot);
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon2, TEXT("Carry_Weapon2"), false))
	{
		PlayerPanelNavigationCoordinator->RegisterCarrySlotPanel(TEXT("Carry.Weapon2"), CarrySlot);
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Offhand, TEXT("Carry_Offhand"), false))
	{
		PlayerPanelNavigationCoordinator->RegisterCarrySlotPanel(TEXT("Carry.Offhand"), CarrySlot);
	}

	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneContentGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget)
		{
			GroupWidget->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Content"));
		}
	}

	if (ActionBarTileView)
	{
		PlayerPanelNavigationCoordinator->RegisterActionBarPanel(TEXT("Actionbar"), ActionBarTileView);
	}

	RegisterAdditionalInventoryNavigationPanels(PlayerPanelNavigationCoordinator);

	PlayerPanelNavigationCoordinator->EndPanelRefresh();
}

void URpgPlayerInventoryWidget::RegisterAdditionalInventoryNavigationPanels(URpgInventoryPanelNavigationCoordinator* Navigator)
{
	(void)Navigator;
}

void URpgPlayerInventoryWidget::AppendAdditionalSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	(void)OutGrids;
}

bool URpgPlayerInventoryWidget::RouteInventoryPayloadAtScreenPosition(
	const FRpgInventoryDragPayload& Payload,
	FVector2D ScreenPosition,
	bool bCommit,
	const URpgInventoryDragDropOperation* DragOperation)
{
	const FRpgInventoryDragPayload ResolvedPayload = PlayerDragDropCoordinator
		? PlayerDragDropCoordinator->ResolveInteractionPayload(Payload)
		: Payload;
	const FVector2D GhostCenterScreenPosition = DragOperation
		? DragOperation->ResolveDecoratorCenterScreen(ScreenPosition)
		: URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(ResolvedPayload, ScreenPosition);
	bool bTargetAddressed = false;
	const bool bGearHandled = RoutePayloadToGearSlot(
		ResolvedPayload,
		GhostCenterScreenPosition,
		bCommit,
		bTargetAddressed);
	if (bTargetAddressed)
	{
		return bGearHandled;
	}

	const bool bCarryHandled = RoutePayloadToCarrySlot(
		ResolvedPayload,
		GhostCenterScreenPosition,
		bCommit,
		bTargetAddressed);
	if (bTargetAddressed)
	{
		return bCarryHandled;
	}

	const bool bActionBarHandled = RoutePayloadToActionBar(
		ResolvedPayload,
		GhostCenterScreenPosition,
		bCommit,
		bTargetAddressed);
	if (bTargetAddressed)
	{
		return bActionBarHandled;
	}

	if (RoutePayloadToSpatialGrid(ResolvedPayload, ScreenPosition, bCommit))
	{
		return true;
	}

	SwitchActivePointerDropTarget(nullptr);
	return false;
}

bool URpgPlayerInventoryWidget::RoutePayloadToCarrySlot(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	auto TryRouteSlot = [this, &Payload, GhostCenterScreenPosition, bCommit](
		UWidget* BoundWidget,
		FName BindingName,
		bool& bOutAddressed)
	{
		URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false);
		if (!CarrySlot || !IsWidgetUnderScreenPosition(CarrySlot, GhostCenterScreenPosition))
		{
			return false;
		}

		bOutAddressed = true;
		SwitchActivePointerDropTarget(CarrySlot);
		if (bCommit)
		{
			return CarrySlot->CommitPayloadDrop(Payload);
		}

		CarrySlot->PreviewPayloadDrop(Payload);
		return true;
	};

	return TryRouteSlot(Carry_Weapon1, TEXT("Carry_Weapon1"), bOutTargetAddressed) ||
		TryRouteSlot(Carry_Weapon2, TEXT("Carry_Weapon2"), bOutTargetAddressed) ||
		TryRouteSlot(Carry_Offhand, TEXT("Carry_Offhand"), bOutTargetAddressed);
}

bool URpgPlayerInventoryWidget::RoutePayloadToGearSlot(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	auto TryRouteSlot = [&](URpgEquipmentSlotWidget* SlotWidget)
	{
		if (!IsWidgetUnderScreenPosition(SlotWidget, GhostCenterScreenPosition))
		{
			return false;
		}
		bOutTargetAddressed = true;
		SwitchActivePointerDropTarget(SlotWidget);

		if (bCommit)
		{
			return SlotWidget->CommitPayloadDrop(Payload);
		}

		SlotWidget->PreviewPayloadDrop(Payload);
		return true;
	};

	return TryRouteSlot(Gear_Head) ||
		TryRouteSlot(Gear_Chest) ||
		TryRouteSlot(Gear_Hands) ||
		TryRouteSlot(Gear_Legs) ||
		TryRouteSlot(Gear_Feet) ||
		TryRouteSlot(Gear_Backpack) ||
		TryRouteSlot(Gear_Belt) ||
		TryRouteSlot(Gear_Pouch) ||
		TryRouteSlot(Gear_ResourceBag);
}

bool URpgPlayerInventoryWidget::RoutePayloadToActionBar(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	if (!ActionBarTileView ||
		!IsWidgetUnderScreenPosition(ActionBarTileView, GhostCenterScreenPosition) ||
		!ActionBarTileView->HasActionBarSlotAtScreenPosition(GhostCenterScreenPosition))
	{
		return false;
	}
	bOutTargetAddressed = true;
	SwitchActivePointerDropTarget(ActionBarTileView);

	if (bCommit)
	{
		return ActionBarTileView->CommitPayloadAtScreenPosition(Payload, GhostCenterScreenPosition);
	}

	return ActionBarTileView->PreviewPayloadAtScreenPosition(Payload, GhostCenterScreenPosition);
}

bool URpgPlayerInventoryWidget::RoutePayloadToSpatialGrid(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit)
{
	TArray<URpgInventorySpatialGridWidget*> SpatialGrids;
	CollectSpatialGrids(SpatialGrids);
	for (URpgInventorySpatialGridWidget* SpatialGrid : SpatialGrids)
	{
		if (!SpatialGrid || !SpatialGrid->CanAddressPayloadAtScreenPosition(Payload, ScreenPosition))
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

void URpgPlayerInventoryWidget::SwitchActivePointerDropTarget(UWidget* NewTarget)
{
	if (ActivePointerDropTarget.Get() == NewTarget)
	{
		return;
	}

	ClearExternalDragPreviews();
	ActivePointerDropTarget = NewTarget;
}

void URpgPlayerInventoryWidget::CollectSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneContentGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget && GroupWidget->GetSlotGroupHandle().IsValid() && GroupWidget->GetSpatialGridWidget())
		{
			OutGrids.AddUnique(GroupWidget->GetSpatialGridWidget());
		}
	}

	AppendAdditionalSpatialGrids(OutGrids);
}

URpgInventorySlotGroupViewModel* URpgPlayerInventoryWidget::FindEquipmentProvidedContentGroup(FName SourceEquipmentSlotName) const
{
	if (!PlayerInventoryViewModel || SourceEquipmentSlotName.IsNone())
	{
		return nullptr;
	}

	for (URpgInventorySlotGroupViewModel* Group : PlayerInventoryViewModel->GetInventoryGroups())
	{
		if (Group && Group->IsProvidedByEquipment() &&
			Group->GetSourceEquipmentSlotName() == SourceEquipmentSlotName)
		{
			return Group;
		}
	}

	return nullptr;
}

void URpgPlayerInventoryWidget::CollectStandaloneContentGroupWidgets(TArray<URpgInventorySlotGroupWidget*>& OutWidgets) const
{
	OutWidgets.Add(Content_Pockets);
	OutWidgets.Add(Content_Backpack);
	OutWidgets.Add(Content_Belt);
	OutWidgets.Add(Content_Pouch);
	OutWidgets.Add(Content_ResourceBag);
	OutWidgets.Remove(nullptr);
}

URpgInventoryCarrySlotWidget* URpgPlayerInventoryWidget::ResolveCarrySlotWidget(
	UWidget* BoundWidget,
	FName BindingName,
	bool bLogFailure) const
{
	URpgInventoryCarrySlotWidget* CarrySlot = Cast<URpgInventoryCarrySlotWidget>(BoundWidget);
	if (CarrySlot || !bLogFailure || ReportedInvalidPlayerBindings.Contains(BindingName))
	{
		return CarrySlot;
	}

	ReportedInvalidPlayerBindings.Add(BindingName);
	if (BoundWidget)
	{
		UE_LOG(LogRpgPlayerInventoryWidget, Warning,
			TEXT("%s binding %s still uses %s. Reparent that editor widget to RpgInventoryCarrySlotWidget; legacy mini-grid carry widgets are no longer routed."),
			*GetNameSafe(this),
			*BindingName.ToString(),
			*GetNameSafe(BoundWidget->GetClass()));
	}
	else
	{
		UE_LOG(LogRpgPlayerInventoryWidget, Warning,
			TEXT("%s is missing required carry binding %s. Add a widget derived from RpgInventoryCarrySlotWidget."),
			*GetNameSafe(this),
			*BindingName.ToString());
	}
	return nullptr;
}

void URpgPlayerInventoryWidget::ClearExternalDragPreviews()
{
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon1, TEXT("Carry_Weapon1"), false))
	{
		CarrySlot->ClearExternalPreviewPayload();
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon2, TEXT("Carry_Weapon2"), false))
	{
		CarrySlot->ClearExternalPreviewPayload();
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Offhand, TEXT("Carry_Offhand"), false))
	{
		CarrySlot->ClearExternalPreviewPayload();
	}

	if (Gear_Head)
	{
		Gear_Head->ClearExternalPreviewPayload();
	}
	if (Gear_Chest)
	{
		Gear_Chest->ClearExternalPreviewPayload();
	}
	if (Gear_Hands)
	{
		Gear_Hands->ClearExternalPreviewPayload();
	}
	if (Gear_Legs)
	{
		Gear_Legs->ClearExternalPreviewPayload();
	}
	if (Gear_Feet)
	{
		Gear_Feet->ClearExternalPreviewPayload();
	}
	if (Gear_Backpack)
	{
		Gear_Backpack->ClearExternalPreviewPayload();
	}
	if (Gear_Belt)
	{
		Gear_Belt->ClearExternalPreviewPayload();
	}
	if (Gear_Pouch)
	{
		Gear_Pouch->ClearExternalPreviewPayload();
	}
	if (Gear_ResourceBag)
	{
		Gear_ResourceBag->ClearExternalPreviewPayload();
	}

	if (ActionBarTileView)
	{
		ActionBarTileView->ClearExternalPreviewPayloads();
	}

	TArray<URpgInventorySpatialGridWidget*> SpatialGrids;
	CollectSpatialGrids(SpatialGrids);
	for (URpgInventorySpatialGridWidget* SpatialGrid : SpatialGrids)
	{
		if (SpatialGrid)
		{
			SpatialGrid->ClearExternalPreviewPayload();
		}
	}

	if (PlayerDragDropCoordinator)
	{
		PlayerDragDropCoordinator->ClearInteractionPreview();
	}
}

void URpgPlayerInventoryWidget::QueueDeferredPlayerInventoryRefresh()
{
	if (bDeferredPlayerInventoryRefreshQueued)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ExecuteDeferredPlayerInventoryRefresh();
		return;
	}

	bDeferredPlayerInventoryRefreshQueued = true;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::ExecuteDeferredPlayerInventoryRefresh));
}

void URpgPlayerInventoryWidget::ExecuteDeferredPlayerInventoryRefresh()
{
	bDeferredPlayerInventoryRefreshQueued = false;

	ForwardCoordinatorToChildren();
	RegisterPlayerInventoryNavigationPanels();
	RefreshInventoryControllerFocus();
}
