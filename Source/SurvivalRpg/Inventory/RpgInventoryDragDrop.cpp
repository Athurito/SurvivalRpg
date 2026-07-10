#include "RpgInventoryDragDrop.h"

#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_SlotContainerProvider.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryInteractionSession.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDragDrop)

namespace
{
	bool IsInventoryTargetType(ERpgInventoryDropTargetType TargetType)
	{
		return TargetType == ERpgInventoryDropTargetType::InventorySlot ||
			TargetType == ERpgInventoryDropTargetType::InventoryPanel;
	}

	bool IsManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
	{
		return EquipmentSlot == ERpgEquipmentSlot::MainHand ||
			EquipmentSlot == ERpgEquipmentSlot::OffHand ||
			EquipmentSlot == ERpgEquipmentSlot::Head ||
			EquipmentSlot == ERpgEquipmentSlot::Chest ||
			EquipmentSlot == ERpgEquipmentSlot::Hands ||
			EquipmentSlot == ERpgEquipmentSlot::Legs ||
			EquipmentSlot == ERpgEquipmentSlot::Feet ||
			URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(EquipmentSlot);
	}

	bool IsSplittableStackItem(const URpgInventoryItemInstance* ItemInstance)
	{
		const URpgInventoryFragment_ItemTraits* Traits = ItemInstance
			? ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>()
			: nullptr;
		return Traits && Traits->GetMaxStackSize() > 1;
	}

	FRpgInventoryGridSize GetDragPayloadItemFootprint(const URpgInventoryItemInstance* ItemInstance)
	{
		FRpgInventoryGridSize Footprint;
		Footprint.Width = 1;
		Footprint.Height = 1;

		const URpgInventoryFragment_SpatialItem* SpatialFragment = ItemInstance
			? ItemInstance->FindFragmentByClass<URpgInventoryFragment_SpatialItem>()
			: nullptr;
		if (!SpatialFragment)
		{
			return Footprint;
		}

		const FRpgInventoryGridSize SpatialFootprint = SpatialFragment->GetFootprint(false);
		return SpatialFootprint.IsValid() ? SpatialFootprint : Footprint;
	}

	bool CanInventoryItemEquipInSlot(const URpgInventoryItemInstance* ItemInstance, ERpgEquipmentSlot EquipmentSlot)
	{
		if (URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(EquipmentSlot))
		{
			return ItemInstance && ItemInstance->FindFragmentByClass<URpgInventoryFragment_SlotContainerProvider>() != nullptr;
		}

		const URpgInventoryFragment_EquippableItem* EquippableFragment = ItemInstance
			? ItemInstance->FindFragmentByClass<URpgInventoryFragment_EquippableItem>()
			: nullptr;
		const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
		const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
		return EquipmentCDO && EquipmentCDO->CanEquipInSlot(EquipmentSlot);
	}

	bool CanAutoTransferPayloadToInventory(const FRpgInventoryDragPayload& Payload, const URpgInventoryManagerComponent* TargetInventory)
	{
		if (!Payload.SourceInventory || !TargetInventory || !Payload.ItemInstance || Payload.SourceInventory == TargetInventory)
		{
			return false;
		}

		const int32 AvailableCount = Payload.SourceInventory->GetItemStackCount(Payload.ItemInstance);
		const int32 RequestedCount = Payload.StackCount <= 0 ? AvailableCount : Payload.StackCount;
		if (AvailableCount <= 0 || RequestedCount <= 0 || RequestedCount > AvailableCount)
		{
			return false;
		}

		if (RequestedCount >= AvailableCount &&
			TargetInventory->CanAddItemInstance(Payload.ItemInstance, AvailableCount))
		{
			return true;
		}

		return TargetInventory->CanAddItemDefinition(Payload.ItemInstance->GetItemDef(), RequestedCount);
	}

	bool CanTransferPayloadToInventoryPlacement(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target)
	{
		if (!Payload.SourceInventory || !Target.TargetInventory || !Payload.ItemInstance ||
			Payload.SourceInventory == Target.TargetInventory || !Target.TargetPlacement.IsValid())
		{
			return false;
		}

		const int32 AvailableCount = Payload.SourceInventory->GetItemStackCount(Payload.ItemInstance);
		const int32 RequestedCount = Payload.StackCount <= 0 ? AvailableCount : Payload.StackCount;
		if (AvailableCount <= 0 || RequestedCount <= 0 || RequestedCount > AvailableCount)
		{
			return false;
		}

		FRpgInventoryGridPlacement NormalizedTargetPlacement;
		URpgInventoryItemInstance* TargetItem = Target.TargetInventory->GetSingleItemOverlappingPlacementForItem(Payload.ItemInstance, Target.TargetPlacement, NormalizedTargetPlacement);
		if (TargetItem && TargetItem->GetItemDef() == Payload.ItemInstance->GetItemDef())
		{
			return Target.TargetInventory->GetFreeStackCapacity(TargetItem) > 0;
		}

		if (RequestedCount >= AvailableCount &&
			Target.TargetInventory->CanAddItemInstanceToPlacement(Payload.ItemInstance, AvailableCount, Target.TargetPlacement))
		{
			return true;
		}

		if (Target.TargetInventory->CanAddItemDefinitionToPlacement(Payload.ItemInstance->GetItemDef(), RequestedCount, Target.TargetPlacement))
		{
			return true;
		}

		if (!TargetItem || RequestedCount < AvailableCount)
		{
			return false;
		}

		FRpgInventoryGridPlacement SourcePlacement;
		const int32 TargetStackCount = Target.TargetInventory->GetItemStackCount(TargetItem);
		return Payload.SourceInventory->GetItemPlacement(Payload.ItemInstance, SourcePlacement) &&
			TargetStackCount > 0 &&
			Target.TargetInventory->CanAddItemInstanceToPlacementIgnoringItem(Payload.ItemInstance, AvailableCount, Target.TargetPlacement, TargetItem) &&
			Payload.SourceInventory->CanAddItemInstanceToPlacementIgnoringItem(TargetItem, TargetStackCount, SourcePlacement, Payload.ItemInstance);
	}

}

void URpgInventoryDragDropOperation::SetInteractionSession(URpgInventoryInteractionSession* InInteractionSession)
{
	InteractionSession = InInteractionSession;
}

void URpgInventoryDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	if (InteractionSession && !InteractionSession->IsRequestPending())
	{
		InteractionSession->CancelInteraction();
	}

	Super::DragCancelled_Implementation(PointerEvent);
}

URpgInventoryDragDropCoordinator* URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(UObject* WorldContextObject, APlayerController* InPlayerController)
{
	UObject* Outer = InPlayerController ? Cast<UObject>(InPlayerController) : WorldContextObject;
	if (!Outer)
	{
		return nullptr;
	}

	URpgInventoryDragDropCoordinator* Coordinator = NewObject<URpgInventoryDragDropCoordinator>(Outer);
	if (Coordinator)
	{
		Coordinator->Initialize(InPlayerController);
	}
	return Coordinator;
}

void URpgInventoryDragDropCoordinator::Initialize(APlayerController* InPlayerController)
{
	PlayerController = InPlayerController;
	UiActionComponent = nullptr;
	UiActionComponent = ResolveUiActionComponent();
	FocusedInventory = nullptr;
	QuickTransferRoutes.Reset();
	EnsureInteractionSession();
	if (InteractionSession)
	{
		InteractionSession->Initialize(this, InPlayerController);
	}
}

URpgInventoryManagerComponent* URpgInventoryDragDropCoordinator::GetPlayerInventory() const
{
	return FindPlayerInventory();
}

void URpgInventoryDragDropCoordinator::SetUiActionComponent(URpgInventoryUiActionComponent* InUiActionComponent)
{
	UiActionComponent = InUiActionComponent;
}

void URpgInventoryDragDropCoordinator::EnsureInteractionSession()
{
	if (!InteractionSession)
	{
		InteractionSession = NewObject<URpgInventoryInteractionSession>(this);
	}

	if (InteractionSession)
	{
		InteractionSession->OnInteractionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionSessionChanged);
	}
}

FRpgInventoryDragPayload URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromEntry(URpgInventoryEntryViewModel* EntryViewModel)
{
	FRpgInventoryDragPayload Payload;
	if (!EntryViewModel || !EntryViewModel->CanDrag())
	{
		return Payload;
	}

	Payload.SourceType = ERpgInventoryDragSourceType::InventoryEntry;
	Payload.SourceInventory = EntryViewModel->GetInventoryManager();
	Payload.ItemInstance = EntryViewModel->GetItemInstance();
	Payload.EntryId = EntryViewModel->GetEntryId();
	Payload.StackCount = EntryViewModel->GetStackCount();
	Payload.SourcePlacement = EntryViewModel->GetPlacement();
	Payload.ItemFootprint = GetDragPayloadItemFootprint(Payload.ItemInstance);
	return Payload;
}

FRpgInventoryDragPayload URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel)
{
	FRpgInventoryDragPayload Payload;
	if (!SlotViewModel)
	{
		return Payload;
	}

	Payload.SourceType = SlotViewModel->CanDrag()
		? ERpgInventoryDragSourceType::InventoryEntry
		: ERpgInventoryDragSourceType::PlayerInventorySlotAddress;
	Payload.SourceInventory = SlotViewModel->GetInventoryManager();
	Payload.ItemInstance = SlotViewModel->GetItemInstance();
	Payload.EntryId = SlotViewModel->GetEntryId();
	Payload.StackCount = SlotViewModel->GetStackCount();
	Payload.SourcePlacement = SlotViewModel->GetItemPlacement().IsValid()
		? SlotViewModel->GetItemPlacement()
		: SlotViewModel->GetPlacement();
	Payload.ItemFootprint = GetDragPayloadItemFootprint(Payload.ItemInstance);
	Payload.SourceSlotAddress = SlotViewModel->GetSlotAddress();
	if (SlotViewModel->GetItemInstance() && SlotViewModel->GetItemPlacement().IsValid())
	{
		Payload.SourceSlotAddress.SetContainerHandle(SlotViewModel->GetItemPlacement().GetContainerHandle());
		Payload.SourceSlotAddress.X = SlotViewModel->GetItemPlacement().X;
		Payload.SourceSlotAddress.Y = SlotViewModel->GetItemPlacement().Y;
	}
	return Payload;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeInventoryTargetFromEntry(URpgInventoryEntryViewModel* EntryViewModel)
{
	FRpgInventoryDropTarget Target;
	if (!EntryViewModel)
	{
		return Target;
	}

	Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	Target.TargetInventory = EntryViewModel->GetInventoryManager();
	Target.TargetPlacement = EntryViewModel->GetPlacement();
	return Target;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(URpgInventoryAddressSlotViewModel* SlotViewModel)
{
	FRpgInventoryDropTarget Target;
	if (!SlotViewModel)
	{
		return Target;
	}

	Target.TargetType = ERpgInventoryDropTargetType::PlayerInventorySlotAddress;
	Target.TargetInventory = SlotViewModel->GetInventoryManager();
	Target.TargetPlacement = SlotViewModel->GetPlacement();
	Target.SlotAddress = SlotViewModel->GetSlotAddress();
	return Target;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeInventoryPanelTarget(URpgInventoryManagerComponent* TargetInventory)
{
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventoryPanel;
	Target.TargetInventory = TargetInventory;
	return Target;
}

FRpgInventoryDragPayload URpgInventoryDragDropCoordinator::MakeEquipmentPayload(URpgInventoryItemInstance* ItemInstance, ERpgEquipmentSlot EquipmentSlot)
{
	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::EquipmentSlot;
	Payload.ItemInstance = ItemInstance;
	Payload.EquipmentSlot = EquipmentSlot;
	Payload.StackCount = 1;
	Payload.ItemFootprint = GetDragPayloadItemFootprint(Payload.ItemInstance);
	return Payload;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeEquipmentTarget(ERpgEquipmentSlot EquipmentSlot)
{
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::EquipmentSlot;
	Target.EquipmentSlot = EquipmentSlot;
	return Target;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeActionBarSlotTarget(int32 ActionBarSlotIndex)
{
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::ActionBarSlot;
	Target.ActionBarSlotIndex = ActionBarSlotIndex;
	return Target;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeActionBarSlotTargetFromViewModel(URpgActionBarSlotViewModel* SlotViewModel)
{
	return MakeActionBarSlotTarget(SlotViewModel ? SlotViewModel->GetSlotIndex() : INDEX_NONE);
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeClearTarget()
{
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::ClearSlot;
	return Target;
}

bool URpgInventoryDragDropCoordinator::IsPayloadValid(const FRpgInventoryDragPayload& Payload)
{
	switch (Payload.SourceType)
	{
	case ERpgInventoryDragSourceType::InventoryEntry:
		return Payload.SourceInventory != nullptr &&
			Payload.ItemInstance != nullptr &&
			Payload.EntryId.IsValid() &&
			Payload.StackCount > 0;

	case ERpgInventoryDragSourceType::PlayerInventorySlotAddress:
		return Payload.SourceInventory != nullptr &&
			Payload.SourceSlotAddress.IsValid();

	case ERpgInventoryDragSourceType::EquipmentSlot:
		return Payload.ItemInstance != nullptr &&
			IsManagedEquipmentSlot(Payload.EquipmentSlot);

	default:
		return false;
	}
}

bool URpgInventoryDragDropCoordinator::IsTargetValid(const FRpgInventoryDropTarget& Target)
{
	switch (Target.TargetType)
	{
	case ERpgInventoryDropTargetType::InventorySlot:
		return Target.TargetInventory != nullptr && Target.TargetPlacement.IsValid();

	case ERpgInventoryDropTargetType::InventoryPanel:
		return Target.TargetInventory != nullptr;

	case ERpgInventoryDropTargetType::EquipmentSlot:
		return IsManagedEquipmentSlot(Target.EquipmentSlot);

	case ERpgInventoryDropTargetType::PlayerInventorySlotAddress:
		return Target.SlotAddress.IsValid();

	case ERpgInventoryDropTargetType::ActionBarSlot:
		return Target.ActionBarSlotIndex >= 0;

	case ERpgInventoryDropTargetType::ClearSlot:
		return true;

	default:
		return false;
	}
}

bool URpgInventoryDragDropCoordinator::BeginHold(const FRpgInventoryDragPayload& Payload)
{
	EnsureInteractionSession();
	return InteractionSession && InteractionSession->BeginInteraction(Payload, ERpgInventoryInteractionInputMode::Controller);
}

bool URpgInventoryDragDropCoordinator::BeginPointerDrag(const FRpgInventoryDragPayload& Payload)
{
	EnsureInteractionSession();
	return InteractionSession && InteractionSession->BeginInteraction(Payload, ERpgInventoryInteractionInputMode::Mouse);
}

bool URpgInventoryDragDropCoordinator::BeginHoldFromEntry(URpgInventoryEntryViewModel* EntryViewModel)
{
	return BeginHold(MakeInventoryPayloadFromEntry(EntryViewModel));
}

void URpgInventoryDragDropCoordinator::CancelHold()
{
	if (InteractionSession && !InteractionSession->IsRequestPending())
	{
		InteractionSession->CancelInteraction();
	}
}

void URpgInventoryDragDropCoordinator::ForceCancelInteraction()
{
	if (InteractionSession)
	{
		InteractionSession->CancelInteraction();
	}
}

bool URpgInventoryDragDropCoordinator::HasHeldPayload() const
{
	return InteractionSession && InteractionSession->HasPayload();
}

FRpgInventoryDragPayload URpgInventoryDragDropCoordinator::GetHeldPayload() const
{
	return InteractionSession ? InteractionSession->GetPayload() : FRpgInventoryDragPayload();
}

URpgInventoryItemInstance* URpgInventoryDragDropCoordinator::GetHeldItemInstance() const
{
	return InteractionSession && InteractionSession->HasPayload()
		? InteractionSession->GetPayload().ItemInstance.Get()
		: nullptr;
}

bool URpgInventoryDragDropCoordinator::IsInteractionRequestPending() const
{
	return InteractionSession && InteractionSession->IsRequestPending();
}

bool URpgInventoryDragDropCoordinator::ToggleInteractionRotation()
{
	if (!InteractionSession || !InteractionSession->HasPayload())
	{
		return false;
	}

	const FRpgInventoryDragPayload Payload = InteractionSession->GetPayload();
	const URpgInventoryFragment_SpatialItem* SpatialFragment = Payload.ItemInstance
		? Payload.ItemInstance->FindFragmentByClass<URpgInventoryFragment_SpatialItem>()
		: nullptr;
	return SpatialFragment && SpatialFragment->bAllowRotation && InteractionSession->ToggleTargetRotation();

}

bool URpgInventoryDragDropCoordinator::GetTargetRotationForPayload(const FRpgInventoryDragPayload& Payload) const
{
	return InteractionSession && InteractionSession->HasPayload() && IsSameInteractionPayload(Payload, InteractionSession->GetPayload())
		? InteractionSession->IsTargetRotated()
		: (Payload.SourcePlacement.IsValid() && Payload.SourcePlacement.bRotated);
}

FRpgInventoryDragPayload URpgInventoryDragDropCoordinator::ResolveInteractionPayload(const FRpgInventoryDragPayload& Payload) const
{
	return InteractionSession && InteractionSession->HasPayload() && IsSameInteractionPayload(Payload, InteractionSession->GetPayload())
		? InteractionSession->GetPayload()
		: Payload;
}

ERpgInventoryInteractionPreviewState URpgInventoryDragDropCoordinator::GetInteractionPreviewState() const
{
	return InteractionSession ? InteractionSession->GetPreviewState() : ERpgInventoryInteractionPreviewState::None;
}

void URpgInventoryDragDropCoordinator::SetFocusedInventory(URpgInventoryManagerComponent* InFocusedInventory)
{
	FocusedInventory = InFocusedInventory;
}

void URpgInventoryDragDropCoordinator::SetQuickTransferTarget(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory)
{
	if (!SourceInventory)
	{
		return;
	}

	for (FRpgInventoryQuickTransferRoute& Route : QuickTransferRoutes)
	{
		if (Route.SourceInventory == SourceInventory)
		{
			Route.TargetInventory = TargetInventory;
			return;
		}
	}

	FRpgInventoryQuickTransferRoute& NewRoute = QuickTransferRoutes.AddDefaulted_GetRef();
	NewRoute.SourceInventory = SourceInventory;
	NewRoute.TargetInventory = TargetInventory;
}

void URpgInventoryDragDropCoordinator::ClearQuickTransferTargets()
{
	QuickTransferRoutes.Reset();
}

URpgInventoryManagerComponent* URpgInventoryDragDropCoordinator::ResolveQuickTransferTarget(URpgInventoryManagerComponent* SourceInventory) const
{
	if (!SourceInventory)
	{
		return nullptr;
	}

	for (const FRpgInventoryQuickTransferRoute& Route : QuickTransferRoutes)
	{
		if (Route.SourceInventory == SourceInventory && Route.TargetInventory && Route.TargetInventory != SourceInventory)
		{
			return Route.TargetInventory.Get();
		}
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (PlayerInventory && SourceInventory != PlayerInventory)
	{
		return PlayerInventory;
	}

	return nullptr;
}

bool URpgInventoryDragDropCoordinator::CanQuickTransferEntry(URpgInventoryEntryViewModel* EntryViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory) const
{
	if (!EntryViewModel || !EntryViewModel->CanDrag() || !ResolveUiActionComponent())
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = EntryViewModel->GetInventoryManager();
	URpgInventoryManagerComponent* TargetInventory = ExplicitTargetInventory ? ExplicitTargetInventory : ResolveQuickTransferTarget(SourceInventory);
	return SourceInventory &&
		TargetInventory &&
		SourceInventory != TargetInventory &&
		EntryViewModel->GetItemInstance() &&
		EntryViewModel->GetStackCount() > 0;
}

bool URpgInventoryDragDropCoordinator::QuickTransferEntry(URpgInventoryEntryViewModel* EntryViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory)
{
	if (!CanQuickTransferEntry(EntryViewModel, ExplicitTargetInventory))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = EntryViewModel->GetInventoryManager();
	URpgInventoryManagerComponent* TargetInventory = ExplicitTargetInventory ? ExplicitTargetInventory : ResolveQuickTransferTarget(SourceInventory);
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	const int32 StackCount = EntryViewModel->GetStackCount();
	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	if (!ActionComponent || !SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !ItemInstance || StackCount <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	ActionComponent->RequestTransferItemStack(SourceInventory, TargetInventory, ItemInstance, StackCount);
	return true;
}

bool URpgInventoryDragDropCoordinator::CanQuickTransferAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory) const
{
	if (!SlotViewModel || !SlotViewModel->CanDrag() || !ResolveUiActionComponent())
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = SlotViewModel->GetInventoryManager();
	URpgInventoryManagerComponent* TargetInventory = ExplicitTargetInventory ? ExplicitTargetInventory : ResolveQuickTransferTarget(SourceInventory);
	return SourceInventory &&
		TargetInventory &&
		SourceInventory != TargetInventory &&
		SlotViewModel->GetItemInstance() &&
		SlotViewModel->GetStackCount() > 0;
}

bool URpgInventoryDragDropCoordinator::QuickTransferAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory)
{
	if (!CanQuickTransferAddressSlot(SlotViewModel, ExplicitTargetInventory))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = SlotViewModel->GetInventoryManager();
	URpgInventoryManagerComponent* TargetInventory = ExplicitTargetInventory ? ExplicitTargetInventory : ResolveQuickTransferTarget(SourceInventory);
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	const int32 StackCount = SlotViewModel->GetStackCount();
	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	if (!ActionComponent || !SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !ItemInstance || StackCount <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	ActionComponent->RequestTransferItemStack(SourceInventory, TargetInventory, ItemInstance, StackCount);
	return true;
}

bool URpgInventoryDragDropCoordinator::CanQuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount) const
{
	if (!EntryViewModel || !EntryViewModel->CanDrag() || !ResolveUiActionComponent())
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	if (!Inventory || !ItemInstance || !IsSplittableStackItem(ItemInstance) || EntryViewModel->GetStackCount() <= 1)
	{
		return false;
	}

	if (TargetPlacement.IsValid() &&
		Inventory->GetItemAtContainerCell(TargetPlacement.GetContainerHandle(), TargetPlacement.X, TargetPlacement.Y) != nullptr)
	{
		return false;
	}

	if (!TargetPlacement.IsValid() && !Inventory->IsCapacityUnlimited() && Inventory->GetFreeEntryCount() <= 0)
	{
		return false;
	}

	const int32 RequestedSplitCount = SplitCount <= 0 ? EntryViewModel->GetStackCount() / 2 : SplitCount;
	return RequestedSplitCount > 0 && RequestedSplitCount < EntryViewModel->GetStackCount();
}

bool URpgInventoryDragDropCoordinator::QuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount)
{
	if (!CanQuickSplitEntry(EntryViewModel, TargetPlacement, SplitCount))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	if (!ActionComponent || !Inventory || !ItemInstance)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	ActionComponent->RequestSplitItemStack(Inventory, ItemInstance, SplitCount, TargetPlacement);
	return true;
}

bool URpgInventoryDragDropCoordinator::UseOrEquipEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount)
{
	if (!EntryViewModel || !EntryViewModel->CanDrag())
	{
		return false;
	}

	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	if (!ActionComponent || !Inventory || !ItemInstance || EntryViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	if (ItemInstance->FindFragmentByClass<URpgInventoryFragment_UsableItem>() != nullptr)
	{
		ActionComponent->RequestUseInventoryItem(Inventory, ItemInstance, FMath::Max(1, StackCount));
	}
	else
	{
		ActionComponent->RequestEquipInventoryItem(ItemInstance);
	}

	return true;
}

bool URpgInventoryDragDropCoordinator::UseOrEquipAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount)
{
	if (!SlotViewModel || !SlotViewModel->CanDrag())
	{
		return false;
	}

	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	if (!ActionComponent || !Inventory || !ItemInstance || SlotViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	if (ItemInstance->FindFragmentByClass<URpgInventoryFragment_UsableItem>() != nullptr)
	{
		ActionComponent->RequestUseInventoryItem(Inventory, ItemInstance, FMath::Max(1, StackCount));
		return true;
	}

	if (SlotViewModel->IsGearSlot() || SlotViewModel->IsCarrySlot())
	{
		ActionComponent->RequestUnequipInventoryItemToContentSlot(ItemInstance);
		return true;
	}

	ActionComponent->RequestEquipInventoryItem(ItemInstance);
	return true;
}

bool URpgInventoryDragDropCoordinator::QuickSplitAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount)
{
	if (!SlotViewModel || !SlotViewModel->CanDrag() || SlotViewModel->IsGearSlot() || SlotViewModel->IsCarrySlot())
	{
		return false;
	}

	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	if (!ActionComponent || !Inventory || !ItemInstance || !IsSplittableStackItem(ItemInstance) || SlotViewModel->GetStackCount() <= 1)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	ActionComponent->RequestSplitItemStack(Inventory, ItemInstance, SplitCount, TargetPlacement);
	return true;
}

bool URpgInventoryDragDropCoordinator::DropEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount, bool bConfirmed)
{
	if (!EntryViewModel || !EntryViewModel->CanDrag())
	{
		return false;
	}

	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	if (!ActionComponent || !Inventory || !ItemInstance || EntryViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	const int32 RequestedStackCount = StackCount <= 0 ? EntryViewModel->GetStackCount() : StackCount;
	ActionComponent->RequestDropInventoryItem(Inventory, ItemInstance, RequestedStackCount, bConfirmed);
	return true;
}

bool URpgInventoryDragDropCoordinator::DropAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount, bool bConfirmed)
{
	if (!SlotViewModel || !SlotViewModel->CanDrag())
	{
		return false;
	}

	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	if (!ActionComponent || !Inventory || !ItemInstance || SlotViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	const int32 RequestedStackCount = StackCount <= 0 ? SlotViewModel->GetStackCount() : StackCount;
	ActionComponent->RequestDropInventoryItem(Inventory, ItemInstance, RequestedStackCount, bConfirmed);
	return true;
}

ERpgInventorySlotDragVisualState URpgInventoryDragDropCoordinator::GetInventoryEntryVisualState(URpgInventoryEntryViewModel* EntryViewModel, bool bIsFocused) const
{
	if (!EntryViewModel)
	{
		return bIsFocused ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal;
	}

	if (!HasHeldPayload())
	{
		return bIsFocused ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal;
	}

	if (IsHeldSourceEntry(EntryViewModel))
	{
		return ERpgInventorySlotDragVisualState::HeldSource;
	}

	const FRpgInventoryDropTarget Target = MakeInventoryTargetFromEntry(EntryViewModel);
	return PreviewDrop(Target)
		? ERpgInventorySlotDragVisualState::ValidTarget
		: ERpgInventorySlotDragVisualState::InvalidTarget;
}

ERpgInventorySlotDragVisualState URpgInventoryDragDropCoordinator::GetInventoryAddressSlotVisualState(URpgInventoryAddressSlotViewModel* SlotViewModel, bool bIsFocused) const
{
	if (!SlotViewModel)
	{
		return bIsFocused ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal;
	}

	if (!HasHeldPayload())
	{
		return bIsFocused ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal;
	}

	if (IsHeldSourceAddressSlot(SlotViewModel))
	{
		return ERpgInventorySlotDragVisualState::HeldSource;
	}

	const FRpgInventoryDropTarget Target = MakePlayerInventorySlotAddressTarget(SlotViewModel);
	return PreviewDrop(Target)
		? ERpgInventorySlotDragVisualState::ValidTarget
		: ERpgInventorySlotDragVisualState::InvalidTarget;
}

bool URpgInventoryDragDropCoordinator::PreviewDrop(const FRpgInventoryDropTarget& Target) const
{
	return HasHeldPayload() && CanCommitPayloadToTarget(GetHeldPayload(), Target);
}

bool URpgInventoryDragDropCoordinator::PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const
{
	const FRpgInventoryDragPayload ResolvedPayload = ResolveInteractionPayload(Payload);
	const ERpgInventoryInteractionPreviewState PreviewState = ResolveInteractionPreview(ResolvedPayload, Target);
	if (InteractionSession && (!InteractionSession->HasPayload() || IsSameInteractionPayload(ResolvedPayload, InteractionSession->GetPayload())))
	{
		if (!InteractionSession->HasPayload())
		{
			InteractionSession->BeginInteraction(ResolvedPayload, ERpgInventoryInteractionInputMode::Mouse);
		}
		InteractionSession->SetPreviewTarget(Target, PreviewState);
	}
	return PreviewState != ERpgInventoryInteractionPreviewState::Blocked &&
		PreviewState != ERpgInventoryInteractionPreviewState::OutOfBounds &&
		PreviewState != ERpgInventoryInteractionPreviewState::Pending &&
		PreviewState != ERpgInventoryInteractionPreviewState::Rejected &&
		PreviewState != ERpgInventoryInteractionPreviewState::None;
}

bool URpgInventoryDragDropCoordinator::UpdateInteractionPreview(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target)
{
	return PreviewPayloadDrop(Payload, Target);
}

void URpgInventoryDragDropCoordinator::ClearInteractionPreview()
{
	if (InteractionSession)
	{
		InteractionSession->ClearPreviewTarget();
	}
}

bool URpgInventoryDragDropCoordinator::CommitDrop(const FRpgInventoryDropTarget& Target)
{
	if (!HasHeldPayload() || IsInteractionRequestPending())
	{
		return false;
	}

	return CommitPayloadToTarget(GetHeldPayload(), Target);
}

bool URpgInventoryDragDropCoordinator::CommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target)
{
	if (!CanCommitPayloadToTarget(Payload, Target))
	{
		return false;
	}

	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!Actions)
	{
		return false;
	}

	if (IsInventoryTargetType(Target.TargetType))
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry)
		{
			if (Payload.SourceInventory == Target.TargetInventory)
			{
				if (!Target.TargetPlacement.IsValid() ||
					(Target.TargetPlacement.GetContainerHandle() == Payload.SourcePlacement.GetContainerHandle() &&
						Target.TargetPlacement.X == Payload.SourcePlacement.X &&
						Target.TargetPlacement.Y == Payload.SourcePlacement.Y &&
						Target.TargetPlacement.bRotated == Payload.SourcePlacement.bRotated))
				{
					return false;
				}

				MarkInteractionRequestPending(Payload, Target);
				Actions->RequestMoveInventoryEntryToPlacement(Payload.SourceInventory, Payload.EntryId, Target.TargetPlacement);
				return true;
			}

			if (Target.TargetType == ERpgInventoryDropTargetType::InventorySlot)
			{
				MarkInteractionRequestPending(Payload, Target);
				Actions->RequestTransferItemStackToPlacement(Payload.SourceInventory, Target.TargetInventory, Payload.ItemInstance, Payload.StackCount, Target.TargetPlacement);
				return true;
			}

			MarkInteractionRequestPending(Payload, Target);
			Actions->RequestTransferItemStack(Payload.SourceInventory, Target.TargetInventory, Payload.ItemInstance, Payload.StackCount);
			return true;
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			MarkInteractionRequestPending(Payload, Target);
			Actions->RequestClearEquipmentSlot(Payload.EquipmentSlot);
			return true;
		}
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::PlayerInventorySlotAddress)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry ||
			Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
			if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry &&
				Payload.SourceInventory &&
				PlayerInventory &&
				Payload.SourceInventory != PlayerInventory)
			{
				MarkInteractionRequestPending(Payload, Target);
				Actions->RequestTransferItemStackToPlacement(
					Payload.SourceInventory,
					PlayerInventory,
					Payload.ItemInstance,
					Payload.StackCount,
					Target.TargetPlacement);
				return true;
			}

			MarkInteractionRequestPending(Payload, Target);
			Actions->RequestMoveItemToInventorySlotAddress(Payload.ItemInstance, Target.SlotAddress);
			return true;
		}
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::ActionBarSlot)
	{
		const FRpgInventorySlotAddress SourceAddress = ResolvePayloadSourceAddress(Payload);
		if (!SourceAddress.IsValid())
		{
			return false;
		}

		URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
		if (!InventoryLayout || !InventoryLayout->CanBindSlotAddressToActionbar(SourceAddress, Payload.ItemInstance))
		{
			return false;
		}

		if (InventoryLayout->IsCarrySlotAddress(SourceAddress))
		{
			MarkInteractionRequestPending(Payload, Target);
			Actions->RequestBindActionBarToCarrySlot(Target.ActionBarSlotIndex, SourceAddress);
		}
		else
		{
			MarkInteractionRequestPending(Payload, Target);
			Actions->RequestBindActionBarToInventorySlot(Target.ActionBarSlotIndex, SourceAddress);
		}
		return true;
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::EquipmentSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry ||
			Payload.SourceType == ERpgInventoryDragSourceType::PlayerInventorySlotAddress ||
			Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot && Payload.EquipmentSlot == Target.EquipmentSlot)
			{
				return false;
			}

			MarkInteractionRequestPending(Payload, Target);
			Actions->RequestAssignItemToEquipmentSlot(Target.EquipmentSlot, Payload.ItemInstance);
			return true;
		}
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::ClearSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			MarkInteractionRequestPending(Payload, Target);
			Actions->RequestClearEquipmentSlot(Payload.EquipmentSlot);
			return true;
		}
	}

	return false;
}

bool URpgInventoryDragDropCoordinator::HandleInventoryEntryAccept(URpgInventoryEntryViewModel* EntryViewModel)
{
	if (!EntryViewModel)
	{
		return false;
	}

	if (!HasHeldPayload())
	{
		return BeginHoldFromEntry(EntryViewModel);
	}

	return CommitDrop(MakeInventoryTargetFromEntry(EntryViewModel));
}

ERpgInventoryInteractionPreviewState URpgInventoryDragDropCoordinator::ResolveInteractionPreview(
	const FRpgInventoryDragPayload& Payload,
	const FRpgInventoryDropTarget& Target) const
{
	if (InteractionSession && InteractionSession->IsRequestPending() &&
		IsSameInteractionPayload(Payload, InteractionSession->GetPayload()))
	{
		return ERpgInventoryInteractionPreviewState::Pending;
	}

	if (!IsPayloadValid(Payload) || !IsTargetValid(Target))
	{
		return IsTargetPlacementOutOfBounds(Target)
			? ERpgInventoryInteractionPreviewState::OutOfBounds
			: ERpgInventoryInteractionPreviewState::Blocked;
	}

	if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry &&
		Payload.SourceInventory == Target.TargetInventory &&
		Target.TargetPlacement.GetContainerHandle() == Payload.SourcePlacement.GetContainerHandle() &&
		Target.TargetPlacement.X == Payload.SourcePlacement.X &&
		Target.TargetPlacement.Y == Payload.SourcePlacement.Y &&
		Target.TargetPlacement.bRotated == Payload.SourcePlacement.bRotated)
	{
		return ERpgInventoryInteractionPreviewState::Blocked;
	}

	if (!CanCommitPayloadToTarget(Payload, Target))
	{
		return IsTargetPlacementOutOfBounds(Target)
			? ERpgInventoryInteractionPreviewState::OutOfBounds
			: ERpgInventoryInteractionPreviewState::Blocked;
	}

	switch (Target.TargetType)
	{
	case ERpgInventoryDropTargetType::EquipmentSlot:
		return ERpgInventoryInteractionPreviewState::Equip;

	case ERpgInventoryDropTargetType::ActionBarSlot:
		return ERpgInventoryInteractionPreviewState::Bind;

	case ERpgInventoryDropTargetType::ClearSlot:
		return ERpgInventoryInteractionPreviewState::Clear;

	case ERpgInventoryDropTargetType::InventorySlot:
	case ERpgInventoryDropTargetType::PlayerInventorySlotAddress:
		if (Target.TargetInventory && Target.TargetPlacement.IsValid() && Payload.ItemInstance)
		{
			URpgInventoryItemInstance* OverlappingItem = nullptr;
			for (const FRpgInventoryEntryView& Entry : Target.TargetInventory->GetAllEntries())
			{
				if (Entry.Instance && Entry.Instance != Payload.ItemInstance && Entry.Placement.Overlaps(Target.TargetPlacement))
				{
					OverlappingItem = Entry.Instance;
					break;
				}
			}
			if (OverlappingItem && OverlappingItem != Payload.ItemInstance)
			{
				if (OverlappingItem->GetItemDef() == Payload.ItemInstance->GetItemDef() &&
					Target.TargetInventory->GetFreeStackCapacity(OverlappingItem) > 0)
				{
					return ERpgInventoryInteractionPreviewState::Merge;
				}
				return ERpgInventoryInteractionPreviewState::Swap;
			}
		}
		return ERpgInventoryInteractionPreviewState::Move;

	case ERpgInventoryDropTargetType::InventoryPanel:
		return ERpgInventoryInteractionPreviewState::Move;

	default:
		return ERpgInventoryInteractionPreviewState::Blocked;
	}
}

void URpgInventoryDragDropCoordinator::HandleInteractionSessionChanged(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	OnHeldPayloadChanged.Broadcast(bHasPayload, bHasPayload && InteractionSession
		? InteractionSession->GetPayload()
		: FRpgInventoryDragPayload());
}

bool URpgInventoryDragDropCoordinator::IsSameInteractionPayload(
	const FRpgInventoryDragPayload& A,
	const FRpgInventoryDragPayload& B) const
{
	if (A.SourceType != B.SourceType || A.ItemInstance != B.ItemInstance)
	{
		return false;
	}

	if (A.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
	{
		return A.EquipmentSlot == B.EquipmentSlot;
	}

	return A.SourceInventory == B.SourceInventory &&
		((A.EntryId.IsValid() && A.EntryId == B.EntryId) ||
			(A.SourceSlotAddress.IsValid() && A.SourceSlotAddress == B.SourceSlotAddress));
}

bool URpgInventoryDragDropCoordinator::IsTargetPlacementOutOfBounds(const FRpgInventoryDropTarget& Target) const
{
	if (Target.TargetType != ERpgInventoryDropTargetType::InventorySlot &&
		Target.TargetType != ERpgInventoryDropTargetType::PlayerInventorySlotAddress)
	{
		return false;
	}

	if (!Target.TargetPlacement.IsValid())
	{
		return true;
	}

	FRpgInventoryGridSize GridSize;
	if (!Target.TargetInventory ||
		!Target.TargetInventory->GetGridSizeForContainerHandle(Target.TargetPlacement.GetContainerHandle(), GridSize) ||
		!GridSize.IsValid())
	{
		return false;
	}

	const FRpgInventoryGridSize OccupiedSize = Target.TargetPlacement.GetOccupiedSize();
	return Target.TargetPlacement.X < 0 ||
		Target.TargetPlacement.Y < 0 ||
		Target.TargetPlacement.X + OccupiedSize.Width > GridSize.Width ||
		Target.TargetPlacement.Y + OccupiedSize.Height > GridSize.Height;
}

FGameplayTag URpgInventoryDragDropCoordinator::ResolveActionTagForTarget(const FRpgInventoryDropTarget& Target) const
{
	return Target.TargetType == ERpgInventoryDropTargetType::EquipmentSlot ||
		Target.TargetType == ERpgInventoryDropTargetType::ClearSlot
		? RpgGameplayTags::Rpg_Inventory_Action_Equip
		: RpgGameplayTags::Rpg_Inventory_Action_Transfer;
}

void URpgInventoryDragDropCoordinator::MarkInteractionRequestPending(
	const FRpgInventoryDragPayload& Payload,
	const FRpgInventoryDropTarget& Target)
{
	EnsureInteractionSession();
	if (!InteractionSession)
	{
		return;
	}

	if (!InteractionSession->HasPayload() || !IsSameInteractionPayload(Payload, InteractionSession->GetPayload()))
	{
		InteractionSession->BeginInteraction(Payload, ERpgInventoryInteractionInputMode::Mouse);
	}
	InteractionSession->SetPreviewTarget(Target, ResolveInteractionPreview(Payload, Target));
	InteractionSession->MarkRequestPending(Target, ResolveActionTagForTarget(Target));
}

bool URpgInventoryDragDropCoordinator::CanCommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const
{
	if (!IsPayloadValid(Payload) || !IsTargetValid(Target) || !ResolveUiActionComponent() ||
		(InteractionSession && InteractionSession->IsRequestPending()))
	{
		return false;
	}

	if (IsInventoryTargetType(Target.TargetType))
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry)
		{
			if (!Payload.SourceInventory || !Target.TargetInventory || !Payload.ItemInstance)
			{
				return false;
			}

			if (Payload.SourceInventory == Target.TargetInventory)
			{
				return Target.TargetType == ERpgInventoryDropTargetType::InventorySlot &&
					Payload.EntryId.IsValid() &&
					Target.TargetPlacement.IsValid() &&
					Payload.SourceInventory->CanMoveInventoryEntryToPlacement(Payload.EntryId, Target.TargetPlacement);
			}

			return Target.TargetType == ERpgInventoryDropTargetType::InventorySlot
				? CanTransferPayloadToInventoryPlacement(Payload, Target)
				: CanAutoTransferPayloadToInventory(Payload, Target.TargetInventory);
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			return IsPlayerInventory(Target.TargetInventory);
		}

		return false;
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::EquipmentSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry ||
			Payload.SourceType == ERpgInventoryDragSourceType::PlayerInventorySlotAddress)
		{
			return IsPlayerInventory(Payload.SourceInventory) &&
				Payload.ItemInstance &&
				IsManagedEquipmentSlot(Target.EquipmentSlot) &&
				CanInventoryItemEquipInSlot(Payload.ItemInstance, Target.EquipmentSlot);
		}

		return Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot &&
			IsManagedEquipmentSlot(Target.EquipmentSlot) &&
			CanInventoryItemEquipInSlot(Payload.ItemInstance, Target.EquipmentSlot);
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::PlayerInventorySlotAddress)
	{
		if ((Payload.SourceType != ERpgInventoryDragSourceType::InventoryEntry &&
				Payload.SourceType != ERpgInventoryDragSourceType::EquipmentSlot) ||
			!Payload.ItemInstance ||
			!Target.SlotAddress.IsValid())
		{
			return false;
		}

		const bool bSourceIsPlayerInventory = Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry &&
			IsPlayerInventory(Payload.SourceInventory);
		const bool bTargetIsPlayerInventory = IsPlayerInventory(Target.TargetInventory);
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry && !bSourceIsPlayerInventory && !bTargetIsPlayerInventory)
		{
			return false;
		}

		const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
		if (!InventoryLayout || !InventoryLayout->CanItemUseSlotAddress(Payload.ItemInstance, Target.SlotAddress))
		{
			return false;
		}

		const FRpgInventorySlotAddress SourceAddress = ResolvePayloadSourceAddress(Payload);
		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			if (!SourceAddress.IsValid() || !InventoryLayout->IsContentSlotAddress(Target.SlotAddress))
			{
				return false;
			}

			ERpgEquipmentSlot SourceEquipmentSlot = ERpgEquipmentSlot::None;
			if (InventoryLayout->IsGearSlotAddress(SourceAddress) &&
				URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearGroupId(SourceAddress.GetContainerHandle().ContainerId, SourceEquipmentSlot) &&
				URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(SourceEquipmentSlot) &&
				!InventoryLayout->CanUnequipSlotContainer(SourceEquipmentSlot))
			{
				return false;
			}

			if (InventoryLayout->IsGearSlotAddress(SourceAddress) &&
				URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(SourceEquipmentSlot))
			{
				bool bTargetIsStaticContent = false;
				for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
				{
					const FRpgInventoryContainerHandle GroupHandle = Group.ContainerHandle.IsValid()
						? Group.ContainerHandle
						: FRpgInventoryContainerHandle::MakeRoot(Group.ContainerId);
					if (GroupHandle == Target.SlotAddress.GetContainerHandle() &&
						Group.ContainsCell(Target.SlotAddress.X, Target.SlotAddress.Y))
					{
						bTargetIsStaticContent = Group.GroupKind == ERpgInventorySlotGroupKind::Content && !Group.bProvidedByEquipment;
						break;
					}
				}

				if (!bTargetIsStaticContent)
				{
					return false;
				}
			}
		}

		if (bSourceIsPlayerInventory && Payload.EntryId.IsValid())
		{
			return Payload.SourceInventory &&
				Payload.SourceInventory->CanMoveInventoryEntryToPlacement(Payload.EntryId, Target.TargetPlacement);
		}

		if (URpgInventoryItemInstance* TargetItem = InventoryLayout->GetItemInSlotAddress(Target.SlotAddress))
		{
			return SourceAddress.IsValid() && InventoryLayout->CanItemUseSlotAddress(TargetItem, SourceAddress);
		}

		return true;
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::ActionBarSlot)
	{
		if ((Payload.SourceType != ERpgInventoryDragSourceType::InventoryEntry &&
				Payload.SourceType != ERpgInventoryDragSourceType::PlayerInventorySlotAddress &&
				Payload.SourceType != ERpgInventoryDragSourceType::EquipmentSlot) ||
			Target.ActionBarSlotIndex < 0)
		{
			return false;
		}

		if (Payload.SourceType != ERpgInventoryDragSourceType::EquipmentSlot && !IsPlayerInventory(Payload.SourceInventory))
		{
			return false;
		}

		const FRpgInventorySlotAddress SourceAddress = ResolvePayloadSourceAddress(Payload);
		const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
		return InventoryLayout &&
			SourceAddress.IsValid() &&
			InventoryLayout->CanBindSlotAddressToActionbar(SourceAddress, Payload.ItemInstance);
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::ClearSlot)
	{
		return Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot;
	}

	return false;
}

bool URpgInventoryDragDropCoordinator::IsHeldSourceEntry(URpgInventoryEntryViewModel* EntryViewModel) const
{
	const FRpgInventoryDragPayload HeldPayload = GetHeldPayload();
	if (!HasHeldPayload() || HeldPayload.SourceType != ERpgInventoryDragSourceType::InventoryEntry || !EntryViewModel)
	{
		return false;
	}

	return HeldPayload.SourceInventory == EntryViewModel->GetInventoryManager() &&
		HeldPayload.EntryId == EntryViewModel->GetEntryId() &&
		HeldPayload.SourcePlacement.GetContainerHandle() == EntryViewModel->GetPlacement().GetContainerHandle() &&
		HeldPayload.SourcePlacement.X == EntryViewModel->GetPlacement().X &&
		HeldPayload.SourcePlacement.Y == EntryViewModel->GetPlacement().Y;
}

bool URpgInventoryDragDropCoordinator::IsHeldSourceAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel) const
{
	const FRpgInventoryDragPayload HeldPayload = GetHeldPayload();
	if (!HasHeldPayload() ||
		(HeldPayload.SourceType != ERpgInventoryDragSourceType::InventoryEntry &&
			HeldPayload.SourceType != ERpgInventoryDragSourceType::PlayerInventorySlotAddress &&
			HeldPayload.SourceType != ERpgInventoryDragSourceType::EquipmentSlot) ||
		!SlotViewModel)
	{
		return false;
	}

	const FRpgInventorySlotAddress HeldAddress = ResolvePayloadSourceAddress(HeldPayload);
	return HeldAddress.IsValid() &&
		HeldAddress == SlotViewModel->GetSlotAddress();
}

URpgInventoryUiActionComponent* URpgInventoryDragDropCoordinator::ResolveUiActionComponent() const
{
	if (UiActionComponent)
	{
		return UiActionComponent;
	}

	if (!PlayerController)
	{
		return nullptr;
	}

	if (const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(PlayerController))
	{
		if (URpgInventoryUiActionComponent* Actions = RpgPlayerController->GetInventoryUiActionComponent())
		{
			return Actions;
		}
	}

	return PlayerController->FindComponentByClass<URpgInventoryUiActionComponent>();
}

URpgInventoryManagerComponent* URpgInventoryDragDropCoordinator::FindPlayerInventory() const
{
	const APlayerController* Controller = PlayerController;
	if (!Controller)
	{
		return nullptr;
	}

	if (const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(Controller))
	{
		if (const ARpgPlayerState* RpgPlayerState = RpgPlayerController->GetRpgPlayerState())
		{
			return RpgPlayerState->GetInventoryManagerComponent();
		}
	}

	if (const ARpgPlayerState* RpgPlayerState = Controller->GetPlayerState<ARpgPlayerState>())
	{
		return RpgPlayerState->GetInventoryManagerComponent();
	}

	return nullptr;
}

URpgPlayerInventoryLayoutComponent* URpgInventoryDragDropCoordinator::FindPlayerInventoryLayout() const
{
	const APlayerController* Controller = PlayerController;
	if (!Controller)
	{
		return nullptr;
	}

	if (const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(Controller))
	{
		return RpgPlayerController->GetPlayerInventoryLayoutComponent();
	}

	return Controller->FindComponentByClass<URpgPlayerInventoryLayoutComponent>();
}

FRpgInventorySlotAddress URpgInventoryDragDropCoordinator::ResolvePayloadSourceAddress(const FRpgInventoryDragPayload& Payload) const
{
	if (Payload.SourceSlotAddress.IsValid())
	{
		return Payload.SourceSlotAddress;
	}

	if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
	{
		return ResolveEquipmentPayloadSourceAddress(Payload);
	}

	FRpgInventorySlotAddress Address;
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (InventoryLayout && IsPlayerInventory(Payload.SourceInventory) && Payload.SourcePlacement.IsValid())
	{
		InventoryLayout->TryMakeSlotAddressFromPlacement(Payload.SourcePlacement, Address);
	}

	return Address;
}

FRpgInventorySlotAddress URpgInventoryDragDropCoordinator::ResolveEquipmentPayloadSourceAddress(const FRpgInventoryDragPayload& Payload) const
{
	FRpgInventorySlotAddress Address;
	if (Payload.SourceType != ERpgInventoryDragSourceType::EquipmentSlot || !Payload.ItemInstance)
	{
		return Address;
	}

	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!InventoryLayout)
	{
		return Address;
	}

	if (URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(Payload.EquipmentSlot, Address) &&
		InventoryLayout->GetItemInSlotAddress(Address) == Payload.ItemInstance)
	{
		return Address;
	}

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (Group.GroupKind != ERpgInventorySlotGroupKind::Carry || !Group.Rule.bCarrySlot)
		{
			continue;
		}

		for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < Group.GridSize.Width; ++X)
			{
				const FRpgInventorySlotAddress CandidateAddress = Group.MakeAddress(X, Y);
				if (InventoryLayout->GetItemInSlotAddress(CandidateAddress) == Payload.ItemInstance)
				{
					return CandidateAddress;
				}
			}
		}
	}

	return FRpgInventorySlotAddress();
}

bool URpgInventoryDragDropCoordinator::IsPlayerInventory(const URpgInventoryManagerComponent* Inventory) const
{
	return Inventory != nullptr && Inventory == FindPlayerInventory();
}
