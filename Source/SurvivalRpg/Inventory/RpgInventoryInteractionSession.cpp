#include "RpgInventoryInteractionSession.h"

#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryInteractionSession)

void URpgInventoryInteractionSession::Initialize(UObject* InWorldContextObject, APlayerController* InPlayerController)
{
	UnregisterMessageListeners();
	WorldContextObject = InWorldContextObject;
	PlayerController = InPlayerController;
	CancelInteraction();
	RegisterMessageListeners();
}

bool URpgInventoryInteractionSession::BeginInteraction(const FRpgInventoryDragPayload& InPayload, ERpgInventoryInteractionInputMode InInputMode)
{
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(InPayload) || bPendingRequest)
	{
		return false;
	}

	Payload = InPayload;
	Target = FRpgInventoryDropTarget();
	SpatialPreviewDescriptor = FRpgInventorySpatialPreviewDescriptor();
	PreviewState = ERpgInventoryInteractionPreviewState::None;
	InputMode = InInputMode;
	RequestId.Invalidate();
	PendingActionTag = FGameplayTag();
	PendingSourceInventory = nullptr;
	PendingTargetInventory = nullptr;
	PendingItem = nullptr;
	PendingItemId = FRpgInventoryItemId();
	PendingEntryId.Invalidate();
	bHasPayload = true;
	bTargetRotated = Payload.SourcePlacement.IsValid() && Payload.SourcePlacement.bRotated;
	bPendingRequest = false;
	OnSpatialPreviewChanged.Broadcast(SpatialPreviewDescriptor);
	BroadcastPayloadChanged();
	BroadcastStateChanged();
	return true;
}

void URpgInventoryInteractionSession::CancelInteraction()
{
	Payload = FRpgInventoryDragPayload();
	Target = FRpgInventoryDropTarget();
	SpatialPreviewDescriptor = FRpgInventorySpatialPreviewDescriptor();
	PreviewState = ERpgInventoryInteractionPreviewState::None;
	InputMode = ERpgInventoryInteractionInputMode::None;
	RequestId.Invalidate();
	PendingActionTag = FGameplayTag();
	PendingSourceInventory = nullptr;
	PendingTargetInventory = nullptr;
	PendingItem = nullptr;
	PendingItemId = FRpgInventoryItemId();
	PendingEntryId.Invalidate();
	bHasPayload = false;
	bTargetRotated = false;
	bPendingRequest = false;
	OnSpatialPreviewChanged.Broadcast(SpatialPreviewDescriptor);
	BroadcastPayloadChanged();
	BroadcastStateChanged();
}

void URpgInventoryInteractionSession::SetPreviewTarget(const FRpgInventoryDropTarget& InTarget, ERpgInventoryInteractionPreviewState InPreviewState)
{
	if (!bHasPayload || bPendingRequest)
	{
		return;
	}

	const bool bTargetChanged = Target.TargetType != InTarget.TargetType ||
		Target.TargetInventory != InTarget.TargetInventory ||
		Target.TargetPlacement.GetContainerHandle() != InTarget.TargetPlacement.GetContainerHandle() ||
		Target.TargetPlacement.X != InTarget.TargetPlacement.X ||
		Target.TargetPlacement.Y != InTarget.TargetPlacement.Y ||
		Target.TargetPlacement.bRotated != InTarget.TargetPlacement.bRotated ||
		Target.SlotAddress != InTarget.SlotAddress ||
		Target.ActionBarSlotIndex != InTarget.ActionBarSlotIndex ||
		Target.EquipmentSlot != InTarget.EquipmentSlot;
	if (!bTargetChanged && PreviewState == InPreviewState)
	{
		return;
	}

	Target = InTarget;
	PreviewState = InPreviewState;
	BroadcastStateChanged();
}

void URpgInventoryInteractionSession::ClearPreviewTarget()
{
	if (!bHasPayload || bPendingRequest || (Target.TargetType == ERpgInventoryDropTargetType::None && PreviewState == ERpgInventoryInteractionPreviewState::None))
	{
		return;
	}

	Target = FRpgInventoryDropTarget();
	PreviewState = ERpgInventoryInteractionPreviewState::None;
	ClearSpatialPreviewDescriptor();
	BroadcastStateChanged();
}

void URpgInventoryInteractionSession::SetSpatialPreviewDescriptor(const FRpgInventorySpatialPreviewDescriptor& InDescriptor)
{
	if (!bHasPayload || bPendingRequest || SpatialPreviewDescriptor.IsEquivalentTo(InDescriptor))
	{
		return;
	}

	SpatialPreviewDescriptor = InDescriptor;
	OnSpatialPreviewChanged.Broadcast(SpatialPreviewDescriptor);
}

void URpgInventoryInteractionSession::ClearSpatialPreviewDescriptor()
{
	if (!SpatialPreviewDescriptor.bValid)
	{
		return;
	}

	SpatialPreviewDescriptor = FRpgInventorySpatialPreviewDescriptor();
	OnSpatialPreviewChanged.Broadcast(SpatialPreviewDescriptor);
}

bool URpgInventoryInteractionSession::ToggleTargetRotation()
{
	if (!bHasPayload || bPendingRequest || !Payload.ItemFootprint.IsValid())
	{
		return false;
	}

	const bool bWasRotated = bTargetRotated;
	const FRpgInventoryGridSize OldOccupiedSize = Payload.ItemFootprint.GetRotated(bWasRotated);
	bTargetRotated = !bTargetRotated;

	if (Payload.bHasSpatialGrabOffset)
	{
		const int32 OldX = FMath::Clamp(Payload.GrabCellOffsetX, 0, FMath::Max(0, OldOccupiedSize.Width - 1));
		const int32 OldY = FMath::Clamp(Payload.GrabCellOffsetY, 0, FMath::Max(0, OldOccupiedSize.Height - 1));
		if (!bWasRotated)
		{
			Payload.GrabCellOffsetX = FMath::Max(0, OldOccupiedSize.Height - 1 - OldY);
			Payload.GrabCellOffsetY = OldX;
		}
		else
		{
			Payload.GrabCellOffsetX = OldY;
			Payload.GrabCellOffsetY = FMath::Max(0, OldOccupiedSize.Width - 1 - OldX);
		}
	}

	if (Payload.DragAnchor.bValid)
	{
		const FIntPoint OldCell(
			FMath::Clamp(Payload.DragAnchor.GrabbedCell.X, 0, FMath::Max(0, OldOccupiedSize.Width - 1)),
			FMath::Clamp(Payload.DragAnchor.GrabbedCell.Y, 0, FMath::Max(0, OldOccupiedSize.Height - 1)));
		const FVector2D OldWithin(
			FMath::Clamp(Payload.DragAnchor.WithinCellNormalized.X, 0.0f, 1.0f),
			FMath::Clamp(Payload.DragAnchor.WithinCellNormalized.Y, 0.0f, 1.0f));
		if (!bWasRotated)
		{
			Payload.DragAnchor.GrabbedCell = FIntPoint(OldOccupiedSize.Height - 1 - OldCell.Y, OldCell.X);
			Payload.DragAnchor.WithinCellNormalized = FVector2D(1.0f - OldWithin.Y, OldWithin.X);
		}
		else
		{
			Payload.DragAnchor.GrabbedCell = FIntPoint(OldCell.Y, OldOccupiedSize.Width - 1 - OldCell.X);
			Payload.DragAnchor.WithinCellNormalized = FVector2D(OldWithin.Y, 1.0f - OldWithin.X);
		}
		const FVector2D OldSourceSize = Payload.DragAnchor.SourceVisualSize;
		const FVector2D OldSourceOffset(
			FMath::Clamp(Payload.DragAnchor.SourcePointerOffset.X, 0.0f, OldSourceSize.X),
			FMath::Clamp(Payload.DragAnchor.SourcePointerOffset.Y, 0.0f, OldSourceSize.Y));
		Payload.DragAnchor.SourcePointerOffset = !bWasRotated
			? FVector2D(OldSourceSize.Y - OldSourceOffset.Y, OldSourceOffset.X)
			: FVector2D(OldSourceOffset.Y, OldSourceSize.X - OldSourceOffset.X);
		Payload.DragAnchor.SourceVisualSize = FVector2D(OldSourceSize.Y, OldSourceSize.X);

		const FVector2D OldScreenSize = Payload.DragAnchor.SourceScreenVisualSize;
		if (OldScreenSize.X > KINDA_SMALL_NUMBER && OldScreenSize.Y > KINDA_SMALL_NUMBER)
		{
			const FVector2D OldScreenOffset(
				FMath::Clamp(Payload.DragAnchor.SourceScreenPointerOffset.X, 0.0f, OldScreenSize.X),
				FMath::Clamp(Payload.DragAnchor.SourceScreenPointerOffset.Y, 0.0f, OldScreenSize.Y));
			Payload.DragAnchor.SourceScreenPointerOffset = !bWasRotated
				? FVector2D(OldScreenSize.Y - OldScreenOffset.Y, OldScreenOffset.X)
				: FVector2D(OldScreenOffset.Y, OldScreenSize.X - OldScreenOffset.X);
			Payload.DragAnchor.SourceScreenVisualSize = FVector2D(OldScreenSize.Y, OldScreenSize.X);
		}
		Payload.DragAnchor.bRotated = bTargetRotated;
	}

	if (Payload.bHasPointerGrabOffset && Payload.DragVisualSize.X > KINDA_SMALL_NUMBER && Payload.DragVisualSize.Y > KINDA_SMALL_NUMBER)
	{
		const FVector2D OldVisualSize = Payload.DragVisualSize;
		const FVector2D OldOffset(
			FMath::Clamp(Payload.PointerGrabOffset.X, 0.0f, OldVisualSize.X),
			FMath::Clamp(Payload.PointerGrabOffset.Y, 0.0f, OldVisualSize.Y));
		Payload.PointerGrabOffset = !bWasRotated
			? FVector2D(OldVisualSize.Y - OldOffset.Y, OldOffset.X)
			: FVector2D(OldOffset.Y, OldVisualSize.X - OldOffset.X);
		Payload.DragVisualSize = FVector2D(OldVisualSize.Y, OldVisualSize.X);
	}

	Target = FRpgInventoryDropTarget();
	PreviewState = ERpgInventoryInteractionPreviewState::None;
	const FRpgInventorySpatialPreviewDescriptor PreviousSpatialPreview = SpatialPreviewDescriptor;
	BroadcastPayloadChanged();
	// The currently addressed grid re-resolves from its retained pointer in the payload callback. If no presenter
	// replaced the descriptor (for example while the free ghost is outside every grid), clear the stale candidate.
	if (SpatialPreviewDescriptor.IsEquivalentTo(PreviousSpatialPreview))
	{
		ClearSpatialPreviewDescriptor();
	}
	BroadcastStateChanged();
	return true;
}

void URpgInventoryInteractionSession::MarkRequestPending(const FRpgInventoryDropTarget& InTarget, FGameplayTag InActionTag)
{
	if (!bHasPayload)
	{
		return;
	}

	Target = InTarget;
	PreviewState = ERpgInventoryInteractionPreviewState::Pending;
	RequestId = FGuid::NewGuid();
	PendingActionTag = InActionTag;
	PendingSourceInventory = Payload.SourceInventory;
	PendingTargetInventory = InTarget.TargetInventory;
	PendingItem = Payload.ItemInstance;
	PendingItemId = Payload.ItemInstance ? Payload.ItemInstance->GetItemId() : FRpgInventoryItemId();
	PendingEntryId = Payload.EntryId;
	bPendingRequest = true;
	UpdateSpatialPreviewState(ERpgInventoryInteractionPreviewState::Pending);
	BroadcastStateChanged();
}

void URpgInventoryInteractionSession::RejectRequestLocally()
{
	if (!bHasPayload)
	{
		return;
	}

	bPendingRequest = false;
	PreviewState = ERpgInventoryInteractionPreviewState::Rejected;
	UpdateSpatialPreviewState(ERpgInventoryInteractionPreviewState::Rejected);
	BroadcastStateChanged();
}

void URpgInventoryInteractionSession::BeginDestroy()
{
	UnregisterMessageListeners();
	Super::BeginDestroy();
}

void URpgInventoryInteractionSession::RegisterMessageListeners()
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	ActionFeedbackHandle = MessageSubsystem.RegisterListener<FRpgInventoryActionFeedbackMessage>(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		this,
		&ThisClass::HandleActionFeedback);
	ActionBarChangedHandle = MessageSubsystem.RegisterListener<FRpgActionBarSlotsChangedMessage>(
		RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
		this,
		&ThisClass::HandleActionBarChanged);
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandleInventoryChanged);
	EquipmentChangedHandle = MessageSubsystem.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
		RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
		this,
		&ThisClass::HandleEquipmentChanged);
}

void URpgInventoryInteractionSession::UnregisterMessageListeners()
{
	if (ActionFeedbackHandle.IsValid())
	{
		ActionFeedbackHandle.Unregister();
	}
	if (ActionBarChangedHandle.IsValid())
	{
		ActionBarChangedHandle.Unregister();
	}
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}
	if (EquipmentChangedHandle.IsValid())
	{
		EquipmentChangedHandle.Unregister();
	}
}

void URpgInventoryInteractionSession::ResolvePendingRequest(bool bSucceeded)
{
	if (!bPendingRequest)
	{
		return;
	}

	if (bSucceeded)
	{
		CancelInteraction();
		return;
	}

	bPendingRequest = false;
	RequestId.Invalidate();
	PendingActionTag = FGameplayTag();
	PendingSourceInventory = nullptr;
	PendingTargetInventory = nullptr;
	PendingItem = nullptr;
	PendingItemId = FRpgInventoryItemId();
	PendingEntryId.Invalidate();
	PreviewState = ERpgInventoryInteractionPreviewState::Rejected;
	UpdateSpatialPreviewState(ERpgInventoryInteractionPreviewState::Rejected);
	BroadcastStateChanged();
}

void URpgInventoryInteractionSession::BroadcastStateChanged()
{
	OnInteractionStateChanged.Broadcast(PreviewState, bHasPayload, bPendingRequest);
}

void URpgInventoryInteractionSession::BroadcastPayloadChanged()
{
	OnPayloadChanged.Broadcast(bHasPayload, bHasPayload ? Payload : FRpgInventoryDragPayload());
}

void URpgInventoryInteractionSession::UpdateSpatialPreviewState(ERpgInventoryInteractionPreviewState InPreviewState)
{
	if (!SpatialPreviewDescriptor.bValid || SpatialPreviewDescriptor.PreviewState == InPreviewState)
	{
		return;
	}

	SpatialPreviewDescriptor.PreviewState = InPreviewState;
	OnSpatialPreviewChanged.Broadcast(SpatialPreviewDescriptor);
}

bool URpgInventoryInteractionSession::IsPendingMessageRelevant(UActorComponent* InventoryOwner, const UObject* Item) const
{
	if (!bPendingRequest)
	{
		return false;
	}

	const bool bInventoryMatches = !InventoryOwner || InventoryOwner == PendingSourceInventory || InventoryOwner == PendingTargetInventory;
	const bool bItemMatches = !Item || Item == PendingItem;
	return bInventoryMatches && bItemMatches;
}

void URpgInventoryInteractionSession::HandleActionFeedback(FGameplayTag Channel, const FRpgInventoryActionFeedbackMessage& Message)
{
	if (!Message.IsAddressedTo(PlayerController.Get()) ||
		!bPendingRequest || !DoesFeedbackMatchPendingRequest(
			RequestId,
			PendingActionTag,
		PendingItemId,
		Message,
		Target.TargetType == ERpgInventoryDropTargetType::ActionBarSlot))
	{
		return;
	}
	if (!Message.RequestId.IsValid() && !IsPendingMessageRelevant(Message.InventoryOwner.Get(), Message.Item.Get()))
	{
		return;
	}

	ResolvePendingRequest(Message.Result == ERpgInventoryActionFeedbackResult::Success);
}

bool URpgInventoryInteractionSession::DoesActionBarSlotConfirmPendingPayload(
	const FRpgActionBarSlot& AppliedSlot,
	const FRpgInventoryDragPayload& PendingPayload,
	const FRpgInventoryItemId& PendingItemId)
{
	const bool bConsumableApplied =
		(AppliedSlot.SlotType == ERpgActionBarSlotType::Consumable ||
			AppliedSlot.SlotType == ERpgActionBarSlotType::InventorySlotBinding) &&
		PendingPayload.ItemInstance &&
		AppliedSlot.ConsumableDefinition == PendingPayload.ItemInstance->GetItemDef() &&
		AppliedSlot.PreferredItemId == PendingItemId;
	const FName SourceCarryRole = PendingPayload.SourceSlotAddress.IsValid()
		? PendingPayload.SourceSlotAddress.ContainerId
		: PendingPayload.SourcePlacement.GetContainerHandle().ContainerId;
	const bool bCarryApplied =
		(AppliedSlot.SlotType == ERpgActionBarSlotType::CarrySlot ||
			AppliedSlot.SlotType == ERpgActionBarSlotType::CarrySlotBinding) &&
		!SourceCarryRole.IsNone() &&
		AppliedSlot.CarryRole == SourceCarryRole;
	return bConsumableApplied || bCarryApplied;
}

void URpgInventoryInteractionSession::HandleActionBarChanged(
	FGameplayTag Channel,
	const FRpgActionBarSlotsChangedMessage& Message)
{
	if (!bPendingRequest ||
		Target.TargetType != ERpgInventoryDropTargetType::ActionBarSlot ||
		Target.ActionBarSlotIndex < 0 ||
		Message.Owner != PlayerController)
	{
		return;
	}

	const URpgActionBarComponent* ActionBar = Cast<URpgActionBarComponent>(Message.ActionBarComponent);
	if (!ActionBar)
	{
		return;
	}

	const FRpgActionBarSlot AppliedSlot = ActionBar->GetSlot(Target.ActionBarSlotIndex);
	if (DoesActionBarSlotConfirmPendingPayload(AppliedSlot, Payload, PendingItemId))
	{
		// Replicated/locally broadcast actionbar state is authoritative enough to release the held ghost even
		// when it arrives before the reliable request-correlated feedback RPC.
		ResolvePendingRequest(true);
	}
}

void URpgInventoryInteractionSession::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	// Quick Access never mutates inventory ownership. It resolves from exact feedback or the matching actionbar
	// binding message above, never from unrelated stack/placement replication while its request is in flight.
	if (!bPendingRequest || Target.TargetType == ERpgInventoryDropTargetType::ActionBarSlot ||
		!IsPendingMessageRelevant(Message.InventoryOwner.Get(), Message.Instance.Get()))
	{
		return;
	}

	const bool bEntryMatches = !PendingEntryId.IsValid() || Message.EntryId == PendingEntryId;
	const bool bItemMatches = !PendingItem || Message.Instance == PendingItem;
	if (bEntryMatches || bItemMatches)
	{
		ResolvePendingRequest(true);
	}
}

void URpgInventoryInteractionSession::HandleEquipmentChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
{
	if (bPendingRequest && Target.TargetType == ERpgInventoryDropTargetType::EquipmentSlot &&
		(!PlayerController || Message.Owner == PlayerController))
	{
		ResolvePendingRequest(true);
	}
}

bool URpgInventoryInteractionSession::DoesFeedbackMatchPendingRequest(
	const FGuid& PendingRequestId,
	FGameplayTag PendingActionTag,
	const FRpgInventoryItemId& PendingItemId,
	const FRpgInventoryActionFeedbackMessage& Message,
	bool bRequireValidRequestId)
{
	if (bRequireValidRequestId && (!PendingRequestId.IsValid() || Message.RequestId != PendingRequestId))
	{
		return false;
	}
	if (PendingRequestId.IsValid() && Message.RequestId.IsValid() && Message.RequestId != PendingRequestId)
	{
		return false;
	}
	if (PendingActionTag.IsValid() && Message.ActionTag != PendingActionTag)
	{
		return false;
	}
	if (PendingItemId.IsValid() && Message.ItemId.IsValid() && Message.ItemId != PendingItemId)
	{
		return false;
	}
	return true;
}
