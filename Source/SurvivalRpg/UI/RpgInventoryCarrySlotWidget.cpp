#include "RpgInventoryCarrySlotWidget.h"

#include "CommonLazyImage.h"
#include "Components/Border.h"
#include "InputCoreTypes.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryCarrySlotWidget)

void URpgInventoryCarrySlotWidget::BindInventoryPresentation(
	URpgInventorySlotGroupViewModel* InGroupViewModel,
	const FRpgInventoryScreenPresentationContext& InContext,
	FName InPanelId)
{
	if (!InContext.IsComplete() || InPanelId.IsNone())
	{
		ReleaseInventoryPresentation();
		return;
	}

	// Resolve the exact address first so a previous source-owned modal is dismissed through the previous host.
	SetCarrySlotGroupViewModel(InGroupViewModel);
	if (!CarrySlotGroupViewModel)
	{
		ReleaseInventoryPresentation();
		return;
	}

	SetDragDropCoordinator(InContext.DragDropCoordinator);
	SetInventoryPresentationHost(InContext.PresentationHost);
	SetPanelNavigationCoordinator(
		InContext.PanelNavigationCoordinator,
		InPanelId);
}

void URpgInventoryCarrySlotWidget::ReleaseInventoryPresentation()
{
	ClearFocusedControllerInteractionTarget();
	UnbindFocusedControllerInteraction();
	SetCarrySlotGroupViewModel(nullptr);
	SetCarryItemVisualVisible(false);
	SetCarryItemIcon(TSoftObjectPtr<UTexture2D>());
	PanelNavigationCoordinator = nullptr;
	PanelNavigationId = NAME_None;
	Super::ReleaseInventoryPresentation();
	bCarryPresentationStateInitialized = false;
}

void URpgInventoryCarrySlotWidget::SetCarrySlotGroupViewModel(
	URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	CarrySlotGroupViewModel = nullptr;
	URpgInventoryAddressSlotViewModel* CanonicalAddress = nullptr;
	if (InGroupViewModel &&
		InGroupViewModel->IsCarryGroup() &&
		InGroupViewModel->GetContainerHandle().IsValid() &&
		InGroupViewModel->GetGridSize().Width == 1 &&
		InGroupViewModel->GetGridSize().Height == 1 &&
		(InGroupViewModel->GetEquipmentSlotRole() == ERpgEquipmentSlot::MainHand ||
			InGroupViewModel->GetEquipmentSlotRole() == ERpgEquipmentSlot::OffHand))
	{
		const TArray<URpgInventoryAddressSlotViewModel*> Slots =
			InGroupViewModel->GetSlots();
		if (Slots.Num() == 1)
		{
			URpgInventoryAddressSlotViewModel* Candidate = Slots[0];
			if (Candidate &&
				Candidate->IsCarrySlot() &&
				Candidate->GetEquipmentSlotRole() ==
					InGroupViewModel->GetEquipmentSlotRole())
			{
				const FRpgInventorySlotAddress Address =
					Candidate->GetSlotAddress();
				if (Address.IsValid() &&
					Address.GetContainerHandle() ==
						InGroupViewModel->GetContainerHandle() &&
					Address.X == 0 &&
					Address.Y == 0)
				{
					CarrySlotGroupViewModel = InGroupViewModel;
					CanonicalAddress = Candidate;
				}
			}
		}
	}

	SetAddressSlotViewModel(CanonicalAddress);
}

void URpgInventoryCarrySlotWidget::SetPanelNavigationCoordinator(
	URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator,
	FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

void URpgInventoryCarrySlotWidget::RefreshCarrySlotPresentation()
{
	RefreshAddressSlotPresentation();
}

void URpgInventoryCarrySlotWidget::RefreshAddressSlotPresentation()
{
	ApplyCarryPresentationState(
		ResolveCarryPresentationState(),
		GetCarryInteractionPreviewState());
	Super::RefreshAddressSlotPresentation();
}

URpgInventoryItemInstance* URpgInventoryCarrySlotWidget::GetCarryItem() const
{
	return GetAddressSlotViewModel() ? GetAddressSlotViewModel()->GetItemInstance() : nullptr;
}

bool URpgInventoryCarrySlotWidget::IsCarryItemActive() const
{
	const URpgInventoryAddressSlotViewModel* AddressSlotViewModel = GetAddressSlotViewModel();
	URpgInventoryItemInstance* Item = GetCarryItem();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = AddressSlotViewModel
		? AddressSlotViewModel->GetInventoryLayout()
		: nullptr;
	const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwningPlayer());
	const URpgEquipmentLoadoutComponent* EquipmentLoadout = PlayerController
		? PlayerController->GetEquipmentLoadoutComponent()
		: nullptr;
	if (!AddressSlotViewModel || !Item || !InventoryLayout || !EquipmentLoadout)
	{
		return false;
	}

	const FRpgInventorySlotAddress Address = AddressSlotViewModel->GetSlotAddress();
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
	return InventoryLayout->TryGetEquipmentSlotRoleForAddress(Address, EquipmentSlot) &&
		(EquipmentSlot == ERpgEquipmentSlot::MainHand ||
			EquipmentSlot == ERpgEquipmentSlot::OffHand) &&
		EquipmentLoadout->GetItemInEquipmentSlot(EquipmentSlot) == Item;
}

ERpgInventoryCarryPresentationState URpgInventoryCarrySlotWidget::ResolveCarryPresentationState() const
{
	if (!GetCarryItem())
	{
		return ERpgInventoryCarryPresentationState::Empty;
	}

	return IsCarryItemActive()
		? ERpgInventoryCarryPresentationState::Active
		: ERpgInventoryCarryPresentationState::Holstered;
}

ERpgInventoryInteractionPreviewState URpgInventoryCarrySlotWidget::GetCarryInteractionPreviewState() const
{
	const URpgInventoryDragDropCoordinator* Coordinator = GetDragDropCoordinator();
	const URpgInventoryInteractionSession* Session = Coordinator ? Coordinator->GetInteractionSession() : nullptr;
	const URpgInventoryAddressSlotViewModel* AddressSlotViewModel = GetAddressSlotViewModel();
	if (!Session || !AddressSlotViewModel)
	{
		return ERpgInventoryInteractionPreviewState::None;
	}

	const FRpgInventoryDropTarget Target = Session->GetTarget();
	return Target.TargetType == ERpgInventoryDropTargetType::PlayerInventorySlotAddress &&
		Target.SlotAddress == AddressSlotViewModel->GetSlotAddress()
		? Session->GetPreviewState()
		: ERpgInventoryInteractionPreviewState::None;
}

void URpgInventoryCarrySlotWidget::SetCarryItemVisualVisible(bool bVisible)
{
	if (ItemIcon)
	{
		ItemIcon->SetVisibility(
			bVisible
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

void URpgInventoryCarrySlotWidget::SetCarryItemIcon(
	TSoftObjectPtr<UTexture2D> Icon)
{
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromLazyTexture(Icon);
	}
}

void URpgInventoryCarrySlotWidget::ApplyCarryPresentationState(
	ERpgInventoryCarryPresentationState NewState,
	ERpgInventoryInteractionPreviewState NewInteractionPreviewState)
{
	if (ActiveIndicator)
	{
		ActiveIndicator->SetVisibility(
			NewState == ERpgInventoryCarryPresentationState::Active
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (HolsteredIndicator)
	{
		HolsteredIndicator->SetVisibility(
			NewState == ERpgInventoryCarryPresentationState::Holstered
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	const bool bStateChanged =
		!bCarryPresentationStateInitialized ||
		CarryPresentationState != NewState ||
		CarryInteractionPreviewState != NewInteractionPreviewState;
	CarryPresentationState = NewState;
	CarryInteractionPreviewState = NewInteractionPreviewState;
	bCarryPresentationStateInitialized = true;

	if (bStateChanged)
	{
		BP_OnCarrySlotStateChanged(
			CarryPresentationState,
			CarryInteractionPreviewState);
	}
}

void URpgInventoryCarrySlotWidget::NativeDestruct()
{
	// The Address base owns the lifecycle hook and dispatches our virtual release exactly once.
	Super::NativeDestruct();
}

void URpgInventoryCarrySlotWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	if (PanelNavigationCoordinator)
	{
		PanelNavigationCoordinator->NotifyCarrySlotFocused(this);
	}

	BindFocusedControllerInteraction();
	RefreshFocusedControllerInteractionTarget();
}

void URpgInventoryCarrySlotWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	ClearFocusedControllerInteractionTarget();
	UnbindFocusedControllerInteraction();
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
}

FReply URpgInventoryCarrySlotWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
		PanelNavigationCoordinator)
	{
		PanelNavigationCoordinator->NotifyCarrySlotFocused(this);
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void URpgInventoryCarrySlotWidget::HandleFocusedControllerHeldPayloadChanged(
	bool bHasHeldPayload,
	const FRpgInventoryDragPayload& HeldPayload)
{
	if (!bHasHeldPayload)
	{
		ClearExternalPreviewPayload();
		return;
	}

	RefreshFocusedControllerInteractionTarget();
}

void URpgInventoryCarrySlotWidget::BindFocusedControllerInteraction()
{
	URpgInventoryDragDropCoordinator* Coordinator = GetDragDropCoordinator();
	if (FocusedControllerDragDropCoordinator == Coordinator)
	{
		return;
	}

	UnbindFocusedControllerInteraction();
	FocusedControllerDragDropCoordinator = Coordinator;
	if (FocusedControllerDragDropCoordinator)
	{
		FocusedControllerDragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleFocusedControllerHeldPayloadChanged);
	}
}

void URpgInventoryCarrySlotWidget::UnbindFocusedControllerInteraction()
{
	if (FocusedControllerDragDropCoordinator)
	{
		FocusedControllerDragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(
			this,
			&ThisClass::HandleFocusedControllerHeldPayloadChanged);
	}
	FocusedControllerDragDropCoordinator = nullptr;
}

void URpgInventoryCarrySlotWidget::RefreshFocusedControllerInteractionTarget()
{
	URpgInventoryDragDropCoordinator* Coordinator = FocusedControllerDragDropCoordinator
		? FocusedControllerDragDropCoordinator.Get()
		: GetDragDropCoordinator();
	URpgInventoryInteractionSession* Session = Coordinator ? Coordinator->GetInteractionSession() : nullptr;
	if (!Session || !Session->HasPayload() || Session->IsRequestPending() ||
		Session->GetInputMode() != ERpgInventoryInteractionInputMode::Controller)
	{
		return;
	}

	// A controller cursor can move directly from a spatial grid to this single-address surface. Retire the old
	// grid-owned descriptor first so its local ghost disappears and the screen-wide carry ghost is allowed to paint.
	Session->ClearSpatialPreviewDescriptor();

	// Use the inherited address target so controller focus, mouse hover, preview, and commit all share one candidate.
	PreviewPayloadDrop(Session->GetPayload());
}

void URpgInventoryCarrySlotWidget::ClearFocusedControllerInteractionTarget()
{
	URpgInventoryDragDropCoordinator* Coordinator = FocusedControllerDragDropCoordinator
		? FocusedControllerDragDropCoordinator.Get()
		: GetDragDropCoordinator();
	URpgInventoryInteractionSession* Session = Coordinator ? Coordinator->GetInteractionSession() : nullptr;
	const URpgInventoryAddressSlotViewModel* AddressSlotViewModel = GetAddressSlotViewModel();
	if (Session && AddressSlotViewModel &&
		Session->GetInputMode() == ERpgInventoryInteractionInputMode::Controller)
	{
		const FRpgInventoryDropTarget& Target = Session->GetTarget();
		if (Target.TargetType == ERpgInventoryDropTargetType::PlayerInventorySlotAddress &&
			Target.SlotAddress == AddressSlotViewModel->GetSlotAddress())
		{
			Coordinator->ClearInteractionPreview();
		}
	}

	ClearExternalPreviewPayload();
}
