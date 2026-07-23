#include "RpgInventoryCarrySlotWidget.h"

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

void URpgInventoryCarrySlotWidget::SetCarrySlotGroupViewModel(
	URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	UnbindCarryAddressObserver();
	CarrySlotGroupViewModel = InGroupViewModel;

	URpgInventoryAddressSlotViewModel* CanonicalAddress = nullptr;
	if (CarrySlotGroupViewModel)
	{
		for (URpgInventoryAddressSlotViewModel* Candidate : CarrySlotGroupViewModel->GetSlots())
		{
			if (!Candidate || !Candidate->IsCarrySlot())
			{
				continue;
			}

			const FRpgInventorySlotAddress Address = Candidate->GetSlotAddress();
			if (Address.X == 0 && Address.Y == 0)
			{
				CanonicalAddress = Candidate;
				break;
			}
		}
	}

	SetAddressSlotViewModel(CanonicalAddress);
	ObservedCarryAddress = CanonicalAddress;
	if (ObservedCarryAddress)
	{
		ObservedCarryAddress->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleCarryAddressSlotChanged);
	}

	RefreshCarrySlotPresentation();
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
	URpgInventoryAddressSlotViewModel* AddressSlotViewModel = GetAddressSlotViewModel();
	URpgInventoryItemInstance* Item = GetCarryItem();
	const bool bOccupied = Item != nullptr;
	const bool bActive = bOccupied && IsCarryItemActive();
	BP_OnCarrySlotPresentationChanged(AddressSlotViewModel, Item, bOccupied, bActive, bOccupied && !bActive);
	RefreshDragDropVisualState();
}

FText URpgInventoryCarrySlotWidget::GetCarrySlotLabel() const
{
	return GetAddressSlotViewModel() ? GetAddressSlotViewModel()->GetSlotLabel() : FText::GetEmpty();
}

URpgInventoryItemInstance* URpgInventoryCarrySlotWidget::GetCarryItem() const
{
	return GetAddressSlotViewModel() ? GetAddressSlotViewModel()->GetItemInstance() : nullptr;
}

TSoftObjectPtr<UTexture2D> URpgInventoryCarrySlotWidget::GetCarryItemIcon() const
{
	return GetAddressSlotViewModel() ? GetAddressSlotViewModel()->GetIcon() : TSoftObjectPtr<UTexture2D>();
}

int32 URpgInventoryCarrySlotWidget::GetCarryStackCount() const
{
	return GetAddressSlotViewModel() ? GetAddressSlotViewModel()->GetStackCount() : 0;
}

bool URpgInventoryCarrySlotWidget::IsCarrySlotOccupied() const
{
	return GetCarryItem() != nullptr;
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

void URpgInventoryCarrySlotWidget::NativeDestruct()
{
	ClearFocusedControllerInteractionTarget();
	UnbindFocusedControllerInteraction();
	UnbindCarryAddressObserver();
	CarrySlotGroupViewModel = nullptr;
	PanelNavigationCoordinator = nullptr;
	PanelNavigationId = NAME_None;
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

void URpgInventoryCarrySlotWidget::HandleCarryAddressSlotChanged(
	URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == ObservedCarryAddress)
	{
		RefreshCarrySlotPresentation();
	}
}

void URpgInventoryCarrySlotWidget::UnbindCarryAddressObserver()
{
	if (ObservedCarryAddress)
	{
		ObservedCarryAddress->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleCarryAddressSlotChanged);
	}
	ObservedCarryAddress = nullptr;
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
