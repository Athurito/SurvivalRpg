#include "RpgInventoryDragDrop.h"

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDragDrop)

namespace
{
	bool IsInventoryTargetType(ERpgInventoryDropTargetType TargetType)
	{
		return TargetType == ERpgInventoryDropTargetType::InventorySlot ||
			TargetType == ERpgInventoryDropTargetType::InventoryPanel;
	}

	bool IsQuickBarEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
	{
		return EquipmentSlot == ERpgEquipmentSlot::MainHand || EquipmentSlot == ERpgEquipmentSlot::OffHand;
	}

	bool IsDedicatedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
	{
		return EquipmentSlot == ERpgEquipmentSlot::Head ||
			EquipmentSlot == ERpgEquipmentSlot::Chest ||
			EquipmentSlot == ERpgEquipmentSlot::Hands ||
			EquipmentSlot == ERpgEquipmentSlot::Legs ||
			EquipmentSlot == ERpgEquipmentSlot::Feet;
	}
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
	CancelHold();
}

URpgInventoryManagerComponent* URpgInventoryDragDropCoordinator::GetPlayerInventory() const
{
	return FindPlayerInventory();
}

void URpgInventoryDragDropCoordinator::SetUiActionComponent(URpgInventoryUiActionComponent* InUiActionComponent)
{
	UiActionComponent = InUiActionComponent;
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
	Payload.SourceSlotIndex = EntryViewModel->GetSlotIndex();
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
	Target.TargetIndex = EntryViewModel->GetSlotIndex();
	return Target;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeInventoryPanelTarget(URpgInventoryManagerComponent* TargetInventory)
{
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventoryPanel;
	Target.TargetInventory = TargetInventory;
	return Target;
}

FRpgInventoryDragPayload URpgInventoryDragDropCoordinator::MakeQuickBarPayload(URpgInventoryItemInstance* ItemInstance, int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot)
{
	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::QuickBarSlot;
	Payload.ItemInstance = ItemInstance;
	Payload.SourceSlotIndex = QuickBarSlotIndex;
	Payload.EquipmentSlot = EquipmentSlot;
	Payload.StackCount = 1;
	return Payload;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeQuickBarTarget(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot)
{
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::QuickBarSlot;
	Target.QuickBarSlotIndex = QuickBarSlotIndex;
	Target.EquipmentSlot = EquipmentSlot;
	return Target;
}

FRpgInventoryDragPayload URpgInventoryDragDropCoordinator::MakeEquipmentPayload(URpgInventoryItemInstance* ItemInstance, ERpgEquipmentSlot EquipmentSlot)
{
	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::EquipmentSlot;
	Payload.ItemInstance = ItemInstance;
	Payload.EquipmentSlot = EquipmentSlot;
	Payload.StackCount = 1;
	return Payload;
}

FRpgInventoryDropTarget URpgInventoryDragDropCoordinator::MakeEquipmentTarget(ERpgEquipmentSlot EquipmentSlot)
{
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::EquipmentSlot;
	Target.EquipmentSlot = EquipmentSlot;
	return Target;
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

	case ERpgInventoryDragSourceType::QuickBarSlot:
		return Payload.ItemInstance != nullptr &&
			Payload.SourceSlotIndex >= 0 &&
			IsQuickBarEquipmentSlot(Payload.EquipmentSlot);

	case ERpgInventoryDragSourceType::EquipmentSlot:
		return Payload.ItemInstance != nullptr &&
			IsDedicatedEquipmentSlot(Payload.EquipmentSlot);

	default:
		return false;
	}
}

bool URpgInventoryDragDropCoordinator::IsTargetValid(const FRpgInventoryDropTarget& Target)
{
	switch (Target.TargetType)
	{
	case ERpgInventoryDropTargetType::InventorySlot:
		return Target.TargetInventory != nullptr && Target.TargetIndex >= 0;

	case ERpgInventoryDropTargetType::InventoryPanel:
		return Target.TargetInventory != nullptr;

	case ERpgInventoryDropTargetType::QuickBarSlot:
		return Target.QuickBarSlotIndex >= 0 && IsQuickBarEquipmentSlot(Target.EquipmentSlot);

	case ERpgInventoryDropTargetType::EquipmentSlot:
		return IsDedicatedEquipmentSlot(Target.EquipmentSlot);

	case ERpgInventoryDropTargetType::ClearSlot:
		return true;

	default:
		return false;
	}
}

bool URpgInventoryDragDropCoordinator::BeginHold(const FRpgInventoryDragPayload& Payload)
{
	if (!IsPayloadValid(Payload))
	{
		return false;
	}

	HeldPayload = Payload;
	bHasHeldPayload = true;
	OnHeldPayloadChanged.Broadcast(bHasHeldPayload, HeldPayload);
	return true;
}

bool URpgInventoryDragDropCoordinator::BeginHoldFromEntry(URpgInventoryEntryViewModel* EntryViewModel)
{
	return BeginHold(MakeInventoryPayloadFromEntry(EntryViewModel));
}

void URpgInventoryDragDropCoordinator::CancelHold()
{
	HeldPayload = FRpgInventoryDragPayload();
	bHasHeldPayload = false;
	OnHeldPayloadChanged.Broadcast(bHasHeldPayload, HeldPayload);
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
	ResolveUiActionComponent()->RequestTransferItemStack(SourceInventory, TargetInventory, EntryViewModel->GetItemInstance(), EntryViewModel->GetStackCount());
	return true;
}

bool URpgInventoryDragDropCoordinator::CanQuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 TargetSlotIndex, int32 SplitCount) const
{
	if (!EntryViewModel || !EntryViewModel->CanDrag() || !ResolveUiActionComponent())
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	if (!Inventory || !EntryViewModel->GetItemInstance() || EntryViewModel->GetStackCount() <= 1)
	{
		return false;
	}

	if (TargetSlotIndex != INDEX_NONE && Inventory->GetItemInSlot(TargetSlotIndex) != nullptr)
	{
		return false;
	}

	if (TargetSlotIndex == INDEX_NONE && !Inventory->IsCapacityUnlimited() && Inventory->GetFreeEntryCount() <= 0)
	{
		return false;
	}

	const int32 RequestedSplitCount = SplitCount <= 0 ? EntryViewModel->GetStackCount() / 2 : SplitCount;
	return RequestedSplitCount > 0 && RequestedSplitCount < EntryViewModel->GetStackCount();
}

bool URpgInventoryDragDropCoordinator::QuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 TargetSlotIndex, int32 SplitCount)
{
	if (!CanQuickSplitEntry(EntryViewModel, TargetSlotIndex, SplitCount))
	{
		return false;
	}

	ResolveUiActionComponent()->RequestSplitItemStack(EntryViewModel->GetInventoryManager(), EntryViewModel->GetItemInstance(), SplitCount, TargetSlotIndex);
	return true;
}

ERpgInventorySlotDragVisualState URpgInventoryDragDropCoordinator::GetInventoryEntryVisualState(URpgInventoryEntryViewModel* EntryViewModel, bool bIsFocused) const
{
	if (!EntryViewModel)
	{
		return bIsFocused ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal;
	}

	if (!bHasHeldPayload)
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

bool URpgInventoryDragDropCoordinator::PreviewDrop(const FRpgInventoryDropTarget& Target) const
{
	return bHasHeldPayload && CanCommitPayloadToTarget(HeldPayload, Target);
}

bool URpgInventoryDragDropCoordinator::PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const
{
	return CanCommitPayloadToTarget(Payload, Target);
}

bool URpgInventoryDragDropCoordinator::CommitDrop(const FRpgInventoryDropTarget& Target)
{
	if (!bHasHeldPayload)
	{
		return false;
	}

	const bool bCommitted = CommitPayloadToTarget(HeldPayload, Target);
	if (bCommitted)
	{
		CancelHold();
	}
	return bCommitted;
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
				if (Target.TargetIndex == INDEX_NONE || Target.TargetIndex == Payload.SourceSlotIndex)
				{
					return true;
				}

				Actions->RequestMoveInventoryEntryToSlot(Payload.SourceInventory, Payload.EntryId, Target.TargetIndex);
				return true;
			}

			if (Target.TargetType == ERpgInventoryDropTargetType::InventorySlot)
			{
				Actions->RequestTransferItemStackToInventorySlot(Payload.SourceInventory, Target.TargetInventory, Payload.ItemInstance, Payload.StackCount, Target.TargetIndex);
				return true;
			}

			Actions->RequestTransferItemStack(Payload.SourceInventory, Target.TargetInventory, Payload.ItemInstance, Payload.StackCount);
			return true;
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::QuickBarSlot)
		{
			Actions->RequestClearQuickBarSlot(Payload.SourceSlotIndex, Payload.EquipmentSlot);
			return true;
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			Actions->RequestClearEquipmentSlot(Payload.EquipmentSlot);
			return true;
		}
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::QuickBarSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry)
		{
			Actions->RequestAssignItemToQuickBar(Target.QuickBarSlotIndex, Target.EquipmentSlot, Payload.ItemInstance);
			return true;
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::QuickBarSlot)
		{
			if (Payload.SourceSlotIndex == Target.QuickBarSlotIndex && Payload.EquipmentSlot == Target.EquipmentSlot)
			{
				return true;
			}

			Actions->RequestSwapQuickBarSlots(Payload.SourceSlotIndex, Payload.EquipmentSlot, Target.QuickBarSlotIndex, Target.EquipmentSlot);
			return true;
		}
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::EquipmentSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry ||
			Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot && Payload.EquipmentSlot == Target.EquipmentSlot)
			{
				return true;
			}

			Actions->RequestAssignItemToEquipmentSlot(Target.EquipmentSlot, Payload.ItemInstance);
			return true;
		}
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::ClearSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::QuickBarSlot)
		{
			Actions->RequestClearQuickBarSlot(Payload.SourceSlotIndex, Payload.EquipmentSlot);
			return true;
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
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

	if (!bHasHeldPayload)
	{
		return BeginHoldFromEntry(EntryViewModel);
	}

	return CommitDrop(MakeInventoryTargetFromEntry(EntryViewModel));
}

bool URpgInventoryDragDropCoordinator::CanCommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const
{
	if (!IsPayloadValid(Payload) || !IsTargetValid(Target) || !ResolveUiActionComponent())
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
					Target.TargetIndex >= 0;
			}

			return true;
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::QuickBarSlot ||
			Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			return IsPlayerInventory(Target.TargetInventory);
		}

		return false;
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::QuickBarSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry)
		{
			return IsPlayerInventory(Payload.SourceInventory);
		}

		return Payload.SourceType == ERpgInventoryDragSourceType::QuickBarSlot;
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::EquipmentSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry)
		{
			return IsPlayerInventory(Payload.SourceInventory);
		}

		return Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot;
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::ClearSlot)
	{
		return Payload.SourceType == ERpgInventoryDragSourceType::QuickBarSlot ||
			Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot;
	}

	return false;
}

bool URpgInventoryDragDropCoordinator::IsHeldSourceEntry(URpgInventoryEntryViewModel* EntryViewModel) const
{
	if (!bHasHeldPayload || HeldPayload.SourceType != ERpgInventoryDragSourceType::InventoryEntry || !EntryViewModel)
	{
		return false;
	}

	return HeldPayload.SourceInventory == EntryViewModel->GetInventoryManager() &&
		HeldPayload.EntryId == EntryViewModel->GetEntryId() &&
		HeldPayload.SourceSlotIndex == EntryViewModel->GetSlotIndex();
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

bool URpgInventoryDragDropCoordinator::IsPlayerInventory(const URpgInventoryManagerComponent* Inventory) const
{
	return Inventory != nullptr && Inventory == FindPlayerInventory();
}
