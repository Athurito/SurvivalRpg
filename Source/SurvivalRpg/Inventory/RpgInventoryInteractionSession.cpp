#include "RpgInventoryInteractionSession.h"

#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
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
	PreviewState = ERpgInventoryInteractionPreviewState::None;
	InputMode = InInputMode;
	RequestId.Invalidate();
	PendingActionTag = FGameplayTag();
	PendingSourceInventory = nullptr;
	PendingTargetInventory = nullptr;
	PendingItem = nullptr;
	PendingEntryId.Invalidate();
	bHasPayload = true;
	bTargetRotated = Payload.SourcePlacement.IsValid() && Payload.SourcePlacement.bRotated;
	bPendingRequest = false;
	BroadcastStateChanged();
	return true;
}

void URpgInventoryInteractionSession::CancelInteraction()
{
	Payload = FRpgInventoryDragPayload();
	Target = FRpgInventoryDropTarget();
	PreviewState = ERpgInventoryInteractionPreviewState::None;
	InputMode = ERpgInventoryInteractionInputMode::None;
	RequestId.Invalidate();
	PendingActionTag = FGameplayTag();
	PendingSourceInventory = nullptr;
	PendingTargetInventory = nullptr;
	PendingItem = nullptr;
	PendingEntryId.Invalidate();
	bHasPayload = false;
	bTargetRotated = false;
	bPendingRequest = false;
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
	BroadcastStateChanged();
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
	PendingEntryId = Payload.EntryId;
	bPendingRequest = true;
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
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandleInventoryChanged);
	EquipmentChangedHandle = MessageSubsystem.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
		RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
		this,
		&ThisClass::HandleEquipmentChanged);
	ActionBarChangedHandle = MessageSubsystem.RegisterListener<FRpgActionBarSlotsChangedMessage>(
		RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
		this,
		&ThisClass::HandleActionBarChanged);
}

void URpgInventoryInteractionSession::UnregisterMessageListeners()
{
	if (ActionFeedbackHandle.IsValid())
	{
		ActionFeedbackHandle.Unregister();
	}
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}
	if (EquipmentChangedHandle.IsValid())
	{
		EquipmentChangedHandle.Unregister();
	}
	if (ActionBarChangedHandle.IsValid())
	{
		ActionBarChangedHandle.Unregister();
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
	PendingEntryId.Invalidate();
	PreviewState = ERpgInventoryInteractionPreviewState::Rejected;
	BroadcastStateChanged();
}

void URpgInventoryInteractionSession::BroadcastStateChanged()
{
	OnInteractionStateChanged.Broadcast(PreviewState, bHasPayload, bPendingRequest);
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
	if (!bPendingRequest || (PendingActionTag.IsValid() && Message.ActionTag != PendingActionTag) ||
		!IsPendingMessageRelevant(Message.InventoryOwner.Get(), Message.Item.Get()))
	{
		return;
	}

	ResolvePendingRequest(Message.Result == ERpgInventoryActionFeedbackResult::Success);
}

void URpgInventoryInteractionSession::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	if (!bPendingRequest || !IsPendingMessageRelevant(Message.InventoryOwner.Get(), Message.Instance.Get()))
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

void URpgInventoryInteractionSession::HandleActionBarChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message)
{
	if (bPendingRequest && Target.TargetType == ERpgInventoryDropTargetType::ActionBarSlot &&
		(!PlayerController || Message.Owner == PlayerController))
	{
		ResolvePendingRequest(true);
	}
}
