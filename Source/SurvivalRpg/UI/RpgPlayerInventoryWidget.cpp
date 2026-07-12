#include "RpgPlayerInventoryWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryFeedbackToastWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "TimerManager.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryWidget)

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
		TEXT("PlayerInventoryWidget VM=%s Coordinator=%s CarryGroupsList=%s InventoryGroupsList=%s ActionBarTileView=%s CarryGroups=%d InventoryGroups=%d ActionBarSlots=%d"),
		*GetNameSafe(PlayerInventoryViewModel),
		*GetNameSafe(PlayerDragDropCoordinator),
		*GetNameSafe(CarryGroupsList),
		*GetNameSafe(InventoryGroupsList),
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

	TSet<FRpgInventoryContainerHandle> StandaloneGroupHandles;
	auto BindStandaloneGroup = [this, &StandaloneGroupHandles](
		URpgInventorySlotGroupWidget* GroupWidget,
		URpgInventorySlotGroupViewModel* GroupViewModel,
		FName PanelPrefix)
	{
		if (!GroupWidget)
		{
			return;
		}

		GroupWidget->SetDragDropCoordinator(PlayerDragDropCoordinator);
		GroupWidget->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, PanelPrefix);
		GroupWidget->SetSlotGroupViewModel(GroupViewModel);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(GroupWidget->Slot))
		{
			// Spatial children must own their complete title + grid geometry; undersized hosts break Slate hit testing.
			CanvasSlot->SetAutoSize(true);
		}
		GroupWidget->SetVisibility(GroupViewModel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (GroupViewModel && GroupViewModel->GetContainerHandle().IsValid())
		{
			StandaloneGroupHandles.Add(GroupViewModel->GetContainerHandle());
		}
	};

	BindStandaloneGroup(Carry_Weapon1, PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId), TEXT("Carry"));
	BindStandaloneGroup(Carry_Weapon2, PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId), TEXT("Carry"));
	BindStandaloneGroup(Carry_Offhand, PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId), TEXT("Carry"));
	BindStandaloneGroup(Content_Pockets, PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::PocketsGroupId), TEXT("Content"));
	BindStandaloneGroup(Content_Backpack, FindEquipmentProvidedContentGroup(TEXT("Backpack")), TEXT("Content"));
	BindStandaloneGroup(Content_Belt, FindEquipmentProvidedContentGroup(TEXT("Belt")), TEXT("Content"));
	BindStandaloneGroup(Content_Pouch, FindEquipmentProvidedContentGroup(TEXT("Pouch")), TEXT("Content"));
	BindStandaloneGroup(Content_ResourceBag, FindEquipmentProvidedContentGroup(TEXT("ResourceBag")), TEXT("Content"));

	if (CarryGroupsList)
	{
		TArray<URpgInventorySlotGroupViewModel*> RemainingCarryGroups;
		for (URpgInventorySlotGroupViewModel* Group : PlayerInventoryViewModel->GetCarryGroups())
		{
			if (Group && !StandaloneGroupHandles.Contains(Group->GetContainerHandle()))
			{
				RemainingCarryGroups.Add(Group);
			}
		}
		CarryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Carry"));
		CarryGroupsList->SetSlotGroupItems(RemainingCarryGroups);
	}

	if (InventoryGroupsList)
	{
		TArray<URpgInventorySlotGroupViewModel*> RemainingInventoryGroups;
		for (URpgInventorySlotGroupViewModel* Group : PlayerInventoryViewModel->GetInventoryGroups())
		{
			if (Group && !StandaloneGroupHandles.Contains(Group->GetContainerHandle()))
			{
				RemainingInventoryGroups.Add(Group);
			}
		}

		InventoryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Content"));
		InventoryGroupsList->SetSlotGroupItems(RemainingInventoryGroups);
	}
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
	if (!bHasPayload)
	{
		ActivePointerDropTarget.Reset();
		ClearExternalDragPreviews();
		ClearFreePointerDragVisual();
	}
	else if (bHasLastPointerDragScreenPosition && PlayerDragDropCoordinator)
	{
		FRpgInventoryDragPayload Payload = PlayerDragDropCoordinator->GetHeldPayload();
		if (ActivePointerDragOperation)
		{
			ActivePointerDragOperation->SynchronizeFromInteractionSession();
			Payload = ActivePointerDragOperation->InventoryPayload;
		}

		if (!bPendingRequest && !bRoutingPointerPreview)
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

	const TSubclassOf<URpgInventoryFeedbackToastWidget> ToastClass = FeedbackToastWidgetClass
		? FeedbackToastWidgetClass
		: URpgInventoryFeedbackToastWidget::StaticClass();
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
	URpgInventoryDragDropOperation* DragOperation)
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
		FreePointerDragVisual->AddToPlayerScreen(252);
		FreePointerDragVisual->SetAlignmentInViewport(FVector2D::ZeroVector);
		FreePointerDragVisual->SetPositionInViewport(FVector2D::ZeroVector, true);
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
		FreePointerDragConfiguredItem = ResolvedPayload.ItemInstance;
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
		FreePointerDragVisual->SetDesiredSizeInViewport(LocalVisualSize);
	}

	FVector2D PointerFraction(0.5f, 0.5f);
	if (ResolvedPayload.DragAnchor.bValid &&
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
	else if (ResolvedPayload.DragAnchor.bValid &&
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

	const FGeometry PlayerScreenGeometry = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(GetOwningPlayer());
	const FVector2D PlayerScreenLocalSize = PlayerScreenGeometry.GetLocalSize();
	FVector2D ScreenVisualSize = FVector2D::ZeroVector;
	float FallbackViewportScale = 1.0f;
	if (PlayerScreenLocalSize.X > KINDA_SMALL_NUMBER && PlayerScreenLocalSize.Y > KINDA_SMALL_NUMBER)
	{
		const FVector2D ScreenOrigin = PlayerScreenGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ScreenExtent = PlayerScreenGeometry.LocalToAbsolute(LocalVisualSize) - ScreenOrigin;
		ScreenVisualSize = FVector2D(FMath::Abs(ScreenExtent.X), FMath::Abs(ScreenExtent.Y));
	}
	else
	{
		const float ResolvedViewportScale = UWidgetLayoutLibrary::GetViewportScale(FreePointerDragVisual);
		FallbackViewportScale = ResolvedViewportScale > KINDA_SMALL_NUMBER ? ResolvedViewportScale : 1.0f;
		ScreenVisualSize = LocalVisualSize * FallbackViewportScale;
	}
	const FVector2D ScreenTopLeft = PointerScreenPosition - PointerFraction * ScreenVisualSize;
	const FVector2D PlayerLocalTopLeft = PlayerScreenLocalSize.X > KINDA_SMALL_NUMBER && PlayerScreenLocalSize.Y > KINDA_SMALL_NUMBER
		? PlayerScreenGeometry.AbsoluteToLocal(ScreenTopLeft)
		: ScreenTopLeft / FallbackViewportScale;
	FreePointerDragVisual->SetRenderTranslation(PlayerLocalTopLeft);

	const bool bSpatialTargetOwnsGhost = Session && Session->GetSpatialPreviewDescriptor().bValid;
	const ESlateVisibility DesiredVisibility = bSpatialTargetOwnsGhost
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible;
	if (FreePointerDragVisual->GetVisibility() != DesiredVisibility)
	{
		FreePointerDragVisual->SetVisibility(DesiredVisibility);
	}
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
	GearSlotWidget->SetEquipmentSlotViewModel(
		bBagSlot
			? PlayerInventoryViewModel->GetBagSlot(EquipmentSlot)
			: PlayerInventoryViewModel->GetArmorSlot(EquipmentSlot));
}

void URpgPlayerInventoryWidget::ForwardCoordinatorToChildren()
{
	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget)
		{
			GroupWidget->SetDragDropCoordinator(PlayerDragDropCoordinator);
		}
	}

	if (CarryGroupsList)
	{
		CarryGroupsList->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}

	if (ActionBarTileView)
	{
		ActionBarTileView->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}

	if (Gear_Head)
	{
		Gear_Head->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Chest)
	{
		Gear_Chest->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Hands)
	{
		Gear_Hands->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Legs)
	{
		Gear_Legs->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Feet)
	{
		Gear_Feet->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Backpack)
	{
		Gear_Backpack->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Belt)
	{
		Gear_Belt->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Pouch)
	{
		Gear_Pouch->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_ResourceBag)
	{
		Gear_ResourceBag->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
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

	if (CarryGroupsList)
	{
		CarryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Carry"));
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Content"));
	}

	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget)
		{
			const FName PanelPrefix = URpgPlayerInventoryLayoutComponent::IsBuiltInCarryGroupId(GroupWidget->GetSlotGroupId())
				? FName(TEXT("Carry"))
				: FName(TEXT("Content"));
			GroupWidget->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, PanelPrefix);
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
	if (RoutePayloadToGearSlot(ResolvedPayload, GhostCenterScreenPosition, bCommit))
	{
		return true;
	}

	if (RoutePayloadToActionBar(ResolvedPayload, GhostCenterScreenPosition, bCommit))
	{
		return true;
	}

	if (RoutePayloadToSpatialGrid(ResolvedPayload, ScreenPosition, bCommit))
	{
		return true;
	}

	SwitchActivePointerDropTarget(nullptr);
	return false;
}

bool URpgPlayerInventoryWidget::RoutePayloadToGearSlot(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit)
{
	auto TryRouteSlot = [&](URpgEquipmentSlotWidget* SlotWidget)
	{
		if (!IsWidgetUnderScreenPosition(SlotWidget, GhostCenterScreenPosition))
		{
			return false;
		}
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
	bool bCommit)
{
	if (!ActionBarTileView || !IsWidgetUnderScreenPosition(ActionBarTileView, GhostCenterScreenPosition))
	{
		return false;
	}
	SwitchActivePointerDropTarget(ActionBarTileView);

	return bCommit
		? ActionBarTileView->CommitPayloadAtScreenPosition(Payload, GhostCenterScreenPosition)
		: ActionBarTileView->PreviewPayloadAtScreenPosition(Payload, GhostCenterScreenPosition);
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
	if (CarryGroupsList)
	{
		CarryGroupsList->GetSpatialGridWidgets(OutGrids);
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->GetSpatialGridWidgets(OutGrids);
	}

	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneGroupWidgets(StandaloneContentGroups);
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

void URpgPlayerInventoryWidget::CollectStandaloneGroupWidgets(TArray<URpgInventorySlotGroupWidget*>& OutWidgets) const
{
	OutWidgets.Add(Carry_Weapon1);
	OutWidgets.Add(Carry_Weapon2);
	OutWidgets.Add(Carry_Offhand);
	OutWidgets.Add(Content_Pockets);
	OutWidgets.Add(Content_Backpack);
	OutWidgets.Add(Content_Belt);
	OutWidgets.Add(Content_Pouch);
	OutWidgets.Add(Content_ResourceBag);
	OutWidgets.Remove(nullptr);
}

void URpgPlayerInventoryWidget::ClearExternalDragPreviews()
{
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
