#include "RpgInventoryDragDrop.h"

#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryEquipmentPlacementPolicy.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryInteractionSession.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDragDrop)

namespace
{
	FRpgInventoryGridSize ResolvePayloadOccupiedSize(const FRpgInventoryDragPayload& Payload, bool bRotated)
	{
		FRpgInventoryGridSize Footprint = Payload.ItemFootprint;
		if (!Footprint.IsValid() && Payload.SourcePlacement.IsValid())
		{
			Footprint = Payload.SourcePlacement.GetUnrotatedSize();
		}
		if (!Footprint.IsValid())
		{
			Footprint.Width = 1;
			Footprint.Height = 1;
		}
		return Footprint.GetRotated(bRotated);
	}

	bool IsInventoryTargetType(ERpgInventoryDropTargetType TargetType)
	{
		return TargetType == ERpgInventoryDropTargetType::InventorySlot ||
			TargetType == ERpgInventoryDropTargetType::InventoryPanel;
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

}

bool FRpgInventorySpatialPreviewDescriptor::IsEquivalentTo(const FRpgInventorySpatialPreviewDescriptor& Other) const
{
	return bValid == Other.bValid &&
		EntryId == Other.EntryId &&
		Target.TargetType == Other.Target.TargetType &&
		Target.TargetInventory == Other.Target.TargetInventory &&
		Target.TargetPlacement.GetContainerHandle() == Other.Target.TargetPlacement.GetContainerHandle() &&
		Target.TargetPlacement.X == Other.Target.TargetPlacement.X &&
		Target.TargetPlacement.Y == Other.Target.TargetPlacement.Y &&
		Target.TargetPlacement.Width == Other.Target.TargetPlacement.Width &&
		Target.TargetPlacement.Height == Other.Target.TargetPlacement.Height &&
		Target.TargetPlacement.bRotated == Other.Target.TargetPlacement.bRotated &&
		Target.SlotAddress == Other.Target.SlotAddress &&
		Target.ActionBarSlotIndex == Other.Target.ActionBarSlotIndex &&
		Target.EquipmentSlot == Other.Target.EquipmentSlot &&
		TargetPlacement.GetContainerHandle() == Other.TargetPlacement.GetContainerHandle() &&
		TargetPlacement.X == Other.TargetPlacement.X &&
		TargetPlacement.Y == Other.TargetPlacement.Y &&
		TargetPlacement.Width == Other.TargetPlacement.Width &&
		TargetPlacement.Height == Other.TargetPlacement.Height &&
		TargetPlacement.bRotated == Other.TargetPlacement.bRotated &&
		PreviewState == Other.PreviewState &&
		SnappedLocalPosition.Equals(Other.SnappedLocalPosition) &&
		SnappedLocalSize.Equals(Other.SnappedLocalSize);
}

void URpgInventoryDragDropOperation::SetInteractionSession(URpgInventoryInteractionSession* InInteractionSession)
{
	InteractionSession = InInteractionSession;
}

FVector2D URpgInventoryDragDropOperation::ResolveDecoratorCenterScreen(FVector2D PointerScreenPosition) const
{
	if (!DefaultDragVisual || Pivot != EDragPivot::TopLeft)
	{
		return URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(InventoryPayload, PointerScreenPosition);
	}

	// FUMGDragDropOp wraps the visual in SDPIScaler before applying Offset, so mirror its desired screen size.
	const float ResolvedViewportScale = UWidgetLayoutLibrary::GetViewportScale(DefaultDragVisual);
	const float ViewportScale = ResolvedViewportScale > KINDA_SMALL_NUMBER ? ResolvedViewportScale : 1.0f;
	const URpgInventoryDragVisualWidget* CanonicalVisual = Cast<URpgInventoryDragVisualWidget>(DefaultDragVisual);
	const FVector2D DecoratorLocalSize = CanonicalVisual
		? CanonicalVisual->GetExactVisualSize()
		: DefaultDragVisual->GetDesiredSize();
	const FVector2D DecoratorSize = DecoratorLocalSize * ViewportScale;
	if (DecoratorSize.X <= KINDA_SMALL_NUMBER || DecoratorSize.Y <= KINDA_SMALL_NUMBER)
	{
		return URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(InventoryPayload, PointerScreenPosition);
	}

	// Matches FUMGDragDropOp's final TopLeft placement; player inventory paints its own non-interpolated ghost.
	return PointerScreenPosition + DecoratorSize * Offset + DecoratorSize * 0.5f;
}

void URpgInventoryDragDropOperation::RefreshDecoratorPointerOffset()
{
	const FRpgInventoryDragAnchor& Anchor = InventoryPayload.DragAnchor;
	if (!Anchor.bValid ||
		Anchor.SourceVisualSize.X <= KINDA_SMALL_NUMBER ||
		Anchor.SourceVisualSize.Y <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	Offset = FVector2D(
		-FMath::Clamp(Anchor.SourcePointerOffset.X / Anchor.SourceVisualSize.X, 0.0f, 1.0f),
		-FMath::Clamp(Anchor.SourcePointerOffset.Y / Anchor.SourceVisualSize.Y, 0.0f, 1.0f));
}

void URpgInventoryDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Dragged_Implementation(PointerEvent);
	SynchronizeFromInteractionSession();
}

void URpgInventoryDragDropOperation::SetScreenOwnedDragVisualActive(bool bInActive)
{
	bScreenOwnedDragVisualActive = bInActive;
	if (DefaultDragVisual)
	{
		const float DesiredOpacity = bScreenOwnedDragVisualActive ? 0.0f : 1.0f;
		if (!FMath::IsNearlyEqual(DefaultDragVisual->GetRenderOpacity(), DesiredOpacity))
		{
			DefaultDragVisual->SetRenderOpacity(DesiredOpacity);
		}
	}
}

void URpgInventoryDragDropOperation::SynchronizeFromInteractionSession()
{
	if (!InteractionSession)
	{
		return;
	}

	InventoryPayload = InteractionSession->GetPayload();
	if (DefaultDragVisual)
	{
		const float DesiredOpacity = bScreenOwnedDragVisualActive || InteractionSession->GetSpatialPreviewDescriptor().bValid
			? 0.0f
			: 1.0f;
		if (!FMath::IsNearlyEqual(DefaultDragVisual->GetRenderOpacity(), DesiredOpacity))
		{
			DefaultDragVisual->SetRenderOpacity(DesiredOpacity);
		}
	}
	if (URpgInventoryDragVisualWidget* DragVisual = Cast<URpgInventoryDragVisualWidget>(DefaultDragVisual))
	{
		DragVisual->SetFootprintRotated(InteractionSession->IsTargetRotated());
		DragVisual->SetPreviewState(InteractionSession->GetPreviewState());
	}
	RefreshDecoratorPointerOffset();
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
		InteractionSession->OnPayloadChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionPayloadChanged);
	}
}

void URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
	FRpgInventoryDragPayload& InOutPayload,
	FVector2D LocalPointerPosition,
	FVector2D SourceVisualSize)
{
	const bool bSourceRotated = InOutPayload.SourcePlacement.IsValid() && InOutPayload.SourcePlacement.bRotated;
	const FRpgInventoryGridSize OccupiedSize = ResolvePayloadOccupiedSize(InOutPayload, bSourceRotated);
	if (!OccupiedSize.IsValid() || SourceVisualSize.X <= KINDA_SMALL_NUMBER || SourceVisualSize.Y <= KINDA_SMALL_NUMBER)
	{
		InOutPayload.DragAnchor = FRpgInventoryDragAnchor();
		return;
	}

	const FVector2D ClampedPointer(
		FMath::Clamp(LocalPointerPosition.X, 0.0f, FMath::Max(0.0f, SourceVisualSize.X - UE_KINDA_SMALL_NUMBER)),
		FMath::Clamp(LocalPointerPosition.Y, 0.0f, FMath::Max(0.0f, SourceVisualSize.Y - UE_KINDA_SMALL_NUMBER)));
	const FVector2D Normalized(
		FMath::Clamp(ClampedPointer.X / SourceVisualSize.X, 0.0f, 1.0f),
		FMath::Clamp(ClampedPointer.Y / SourceVisualSize.Y, 0.0f, 1.0f));
	const FVector2D CellSpace(
		Normalized.X * static_cast<float>(OccupiedSize.Width),
		Normalized.Y * static_cast<float>(OccupiedSize.Height));

	FRpgInventoryDragAnchor& Anchor = InOutPayload.DragAnchor;
	Anchor.bValid = true;
	Anchor.GrabbedCell.X = FMath::Clamp(FMath::FloorToInt(CellSpace.X), 0, OccupiedSize.Width - 1);
	Anchor.GrabbedCell.Y = FMath::Clamp(FMath::FloorToInt(CellSpace.Y), 0, OccupiedSize.Height - 1);
	Anchor.WithinCellNormalized.X = FMath::Clamp(CellSpace.X - Anchor.GrabbedCell.X, 0.0f, 1.0f);
	Anchor.WithinCellNormalized.Y = FMath::Clamp(CellSpace.Y - Anchor.GrabbedCell.Y, 0.0f, 1.0f);
	Anchor.SourceVisualSize = SourceVisualSize;
	Anchor.SourcePointerOffset = ClampedPointer;
	Anchor.SourceScreenVisualSize = FVector2D::ZeroVector;
	Anchor.SourceScreenPointerOffset = FVector2D::ZeroVector;
	Anchor.bRotated = bSourceRotated;

	// Keep legacy Blueprint diagnostics populated while all placement code migrates to DragAnchor.
	InOutPayload.bHasSpatialGrabOffset = true;
	InOutPayload.GrabCellOffsetX = Anchor.GrabbedCell.X;
	InOutPayload.GrabCellOffsetY = Anchor.GrabbedCell.Y;
	InOutPayload.bHasPointerGrabOffset = true;
	InOutPayload.PointerGrabOffset = ClampedPointer;
	InOutPayload.DragVisualSize = SourceVisualSize;
}

void URpgInventoryDragDropCoordinator::CapturePointerDragAnchorScreenGeometry(
	FRpgInventoryDragPayload& InOutPayload,
	FVector2D SourceScreenTopLeft,
	FVector2D PointerScreenPosition,
	FVector2D SourceScreenVisualSize)
{
	FRpgInventoryDragAnchor& Anchor = InOutPayload.DragAnchor;
	if (!Anchor.bValid ||
		SourceScreenVisualSize.X <= KINDA_SMALL_NUMBER ||
		SourceScreenVisualSize.Y <= KINDA_SMALL_NUMBER)
	{
		Anchor.SourceScreenVisualSize = FVector2D::ZeroVector;
		Anchor.SourceScreenPointerOffset = FVector2D::ZeroVector;
		return;
	}

	Anchor.SourceScreenVisualSize = SourceScreenVisualSize;
	Anchor.SourceScreenPointerOffset = FVector2D(
		FMath::Clamp(PointerScreenPosition.X - SourceScreenTopLeft.X, 0.0f, SourceScreenVisualSize.X),
		FMath::Clamp(PointerScreenPosition.Y - SourceScreenTopLeft.Y, 0.0f, SourceScreenVisualSize.Y));
}

FVector2D URpgInventoryDragDropCoordinator::ResolveTargetGrabPixels(
	const FRpgInventoryDragPayload& Payload,
	bool bTargetRotated,
	float CellSize,
	float CellPadding)
{
	const FRpgInventoryGridSize OccupiedSize = ResolvePayloadOccupiedSize(Payload, bTargetRotated);
	const float SafeCellSize = FMath::Max(1.0f, CellSize);
	const float SafePadding = FMath::Max(0.0f, CellPadding);
	const float Stride = SafeCellSize + SafePadding;
	if (!Payload.DragAnchor.bValid)
	{
		return FVector2D(SafeCellSize * 0.5f, SafeCellSize * 0.5f);
	}

	const FIntPoint GrabbedCell(
		FMath::Clamp(Payload.DragAnchor.GrabbedCell.X, 0, FMath::Max(0, OccupiedSize.Width - 1)),
		FMath::Clamp(Payload.DragAnchor.GrabbedCell.Y, 0, FMath::Max(0, OccupiedSize.Height - 1)));
	const FVector2D Within(
		FMath::Clamp(Payload.DragAnchor.WithinCellNormalized.X, 0.0f, 1.0f),
		FMath::Clamp(Payload.DragAnchor.WithinCellNormalized.Y, 0.0f, 1.0f));
	return FVector2D(
		GrabbedCell.X * Stride + Within.X * SafeCellSize,
		GrabbedCell.Y * Stride + Within.Y * SafeCellSize);
}

FVector2D URpgInventoryDragDropCoordinator::ResolveFreeGhostCenterScreen(
	const FRpgInventoryDragPayload& Payload,
	FVector2D PointerScreenPosition)
{
	if (!Payload.DragAnchor.bValid)
	{
		return PointerScreenPosition;
	}

	if (Payload.DragAnchor.SourceScreenVisualSize.X > KINDA_SMALL_NUMBER &&
		Payload.DragAnchor.SourceScreenVisualSize.Y > KINDA_SMALL_NUMBER)
	{
		return PointerScreenPosition - Payload.DragAnchor.SourceScreenPointerOffset +
			Payload.DragAnchor.SourceScreenVisualSize * 0.5f;
	}

	if (Payload.DragAnchor.SourceVisualSize.X > KINDA_SMALL_NUMBER &&
		Payload.DragAnchor.SourceVisualSize.Y > KINDA_SMALL_NUMBER)
	{
		return PointerScreenPosition - Payload.DragAnchor.SourcePointerOffset +
			Payload.DragAnchor.SourceVisualSize * 0.5f;
	}

	return PointerScreenPosition;
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
			FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(Payload.EquipmentSlot);

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
		return FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(Target.EquipmentSlot);

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

int32 URpgInventoryDragDropCoordinator::FindQuickAccessSlotForPayload(const FRpgInventoryDragPayload& Payload) const
{
	if (!Payload.ItemInstance || !Payload.ItemInstance->GetItemId().IsValid())
	{
		return INDEX_NONE;
	}

	const FRpgInventorySlotAddress SourceAddress = ResolvePayloadSourceAddress(Payload);
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(PlayerController.Get());
	const URpgActionBarComponent* ActionBar = RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr;
	const URpgInventoryItemInstance* CurrentItem = InventoryLayout && SourceAddress.IsValid()
		? InventoryLayout->GetItemInSlotAddress(SourceAddress)
		: nullptr;
	if (!InventoryLayout || !ActionBar || !CurrentItem ||
		CurrentItem->GetItemId() != Payload.ItemInstance->GetItemId() ||
		!InventoryLayout->CanBindSlotAddressToActionbar(SourceAddress, CurrentItem))
	{
		return INDEX_NONE;
	}

	const bool bCarryBinding = InventoryLayout->IsCarrySlotAddress(SourceAddress);
	const TSubclassOf<URpgInventoryItemDefinition> ConsumableDefinition = CurrentItem->GetItemDef();
	for (int32 SlotIndex = 0; SlotIndex < ActionBar->GetNumSlots(); ++SlotIndex)
	{
		const FRpgActionBarSlot Slot = ActionBar->GetSlot(SlotIndex);
		const bool bMatchesCarry = bCarryBinding &&
			(Slot.SlotType == ERpgActionBarSlotType::CarrySlot || Slot.SlotType == ERpgActionBarSlotType::CarrySlotBinding) &&
			Slot.CarryRole == SourceAddress.ContainerId;
		const bool bMatchesConsumable = !bCarryBinding &&
			(Slot.SlotType == ERpgActionBarSlotType::Consumable || Slot.SlotType == ERpgActionBarSlotType::InventorySlotBinding) &&
			Slot.ConsumableDefinition == ConsumableDefinition;
		if (bMatchesCarry || bMatchesConsumable)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

bool URpgInventoryDragDropCoordinator::BindPayloadToQuickAccessSlot(
	const FRpgInventoryDragPayload& Payload,
	int32 SlotIndex)
{
	if (!FMath::IsWithinInclusive(SlotIndex, 0, 7) || !Payload.ItemInstance ||
		!Payload.ItemInstance->GetItemId().IsValid())
	{
		return false;
	}

	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	const FRpgInventorySlotAddress SourceAddress = ResolvePayloadSourceAddress(Payload);
	URpgInventoryItemInstance* CurrentItem = InventoryLayout && SourceAddress.IsValid()
		? InventoryLayout->GetItemInSlotAddress(SourceAddress)
		: nullptr;
	if (!Actions || !InventoryLayout || !CurrentItem ||
		CurrentItem->GetItemId() != Payload.ItemInstance->GetItemId() ||
		!InventoryLayout->CanBindSlotAddressToActionbar(SourceAddress, CurrentItem))
	{
		return false;
	}

	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.SlotIndex = SlotIndex;
	Request.SourceAddress = SourceAddress;
	Request.ContextItemId = CurrentItem->GetItemId();
	if (InventoryLayout->IsCarrySlotAddress(SourceAddress))
	{
		Request.Operation = ERpgQuickAccessMutationOperation::BindCarry;
		Request.ExpectedCarryRole = SourceAddress.ContainerId;
	}
	else
	{
		Request.Operation = ERpgQuickAccessMutationOperation::BindConsumable;
		Request.ExpectedConsumableDefinition = CurrentItem->GetItemDef();
		Request.ExpectedPreferredItemId = CurrentItem->GetItemId();
	}
	Actions->RequestMutateQuickAccessBinding(Request);
	return true;
}

bool URpgInventoryDragDropCoordinator::ClearQuickAccessBindingForPayload(const FRpgInventoryDragPayload& Payload)
{
	const int32 SlotIndex = FindQuickAccessSlotForPayload(Payload);
	if (SlotIndex == INDEX_NONE || !Payload.ItemInstance)
	{
		return false;
	}

	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	const FRpgInventorySlotAddress SourceAddress = ResolvePayloadSourceAddress(Payload);
	if (!Actions || !InventoryLayout || !SourceAddress.IsValid())
	{
		return false;
	}

	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(PlayerController.Get());
	const URpgActionBarComponent* ActionBar = RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr;
	if (!ActionBar)
	{
		return false;
	}

	const FRpgActionBarSlot CurrentSlot = ActionBar->GetSlot(SlotIndex);
	FRpgQuickAccessMutationRequest Request;
	Request.EnsureRequestId();
	Request.SlotIndex = SlotIndex;
	Request.ContextItemId = Payload.ItemInstance->GetItemId();
	if (InventoryLayout->IsCarrySlotAddress(SourceAddress))
	{
		Request.Operation = ERpgQuickAccessMutationOperation::ClearCarry;
		Request.ExpectedCarryRole = CurrentSlot.CarryRole;
	}
	else
	{
		Request.Operation = ERpgQuickAccessMutationOperation::ClearConsumable;
		Request.ExpectedConsumableDefinition = CurrentSlot.ConsumableDefinition;
		Request.ExpectedPreferredItemId = CurrentSlot.PreferredItemId;
	}
	Actions->RequestMutateQuickAccessBinding(Request);
	return true;
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
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!EntryViewModel || !EntryViewModel->CanDrag() || !Actions)
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = EntryViewModel->GetInventoryManager();
	URpgInventoryManagerComponent* TargetInventory = ExplicitTargetInventory ? ExplicitTargetInventory : ResolveQuickTransferTarget(SourceInventory);
	URpgInventoryItemInstance* Item = EntryViewModel->GetItemInstance();
	if (!TargetInventory && IsPlayerInventory(SourceInventory))
	{
		TargetInventory = SourceInventory;
	}
	if (!SourceInventory || !TargetInventory || !Item || EntryViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.ItemId = Item->GetItemId();
	Request.StackCount = EntryViewModel->GetStackCount();
	if (SourceInventory == TargetInventory)
	{
		BuildPlayerQuickTransferTargets(EntryViewModel->GetPlacement(), Request.PreferredTargetContainers);
		if (Request.PreferredTargetContainers.IsEmpty())
		{
			return false;
		}
	}
	FRpgInventoryContainerHandle TargetContainer;
	FRpgInventoryGridPlacement TargetPlacement;
	return Actions->FindQuickTransferDestination(SourceInventory, TargetInventory, Request, TargetContainer, TargetPlacement);
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
	if (!TargetInventory && IsPlayerInventory(SourceInventory))
	{
		TargetInventory = SourceInventory;
	}
	if (!ActionComponent || !SourceInventory || !TargetInventory || !ItemInstance || StackCount <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = ItemInstance->GetItemId();
	Request.StackCount = StackCount;
	if (SourceInventory == TargetInventory)
	{
		BuildPlayerQuickTransferTargets(EntryViewModel->GetPlacement(), Request.PreferredTargetContainers);
	}
	ActionComponent->RequestQuickTransferItem(SourceInventory, TargetInventory, Request);
	return true;
}

bool URpgInventoryDragDropCoordinator::CanQuickTransferAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory) const
{
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!SlotViewModel || !SlotViewModel->CanDrag() || !Actions)
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = SlotViewModel->GetInventoryManager();
	URpgInventoryManagerComponent* TargetInventory = ExplicitTargetInventory ? ExplicitTargetInventory : ResolveQuickTransferTarget(SourceInventory);
	URpgInventoryItemInstance* Item = SlotViewModel->GetItemInstance();
	if (!TargetInventory && IsPlayerInventory(SourceInventory))
	{
		TargetInventory = SourceInventory;
	}
	if (!SourceInventory || !TargetInventory || !Item || SlotViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.ItemId = Item->GetItemId();
	Request.StackCount = SlotViewModel->GetStackCount();
	if (SourceInventory == TargetInventory)
	{
		const FRpgInventoryGridPlacement SourcePlacement = SlotViewModel->GetItemPlacement().IsValid()
			? SlotViewModel->GetItemPlacement()
			: SlotViewModel->GetPlacement();
		BuildPlayerQuickTransferTargets(SourcePlacement, Request.PreferredTargetContainers);
		if (Request.PreferredTargetContainers.IsEmpty())
		{
			return false;
		}
	}
	FRpgInventoryContainerHandle TargetContainer;
	FRpgInventoryGridPlacement TargetPlacement;
	return Actions->FindQuickTransferDestination(SourceInventory, TargetInventory, Request, TargetContainer, TargetPlacement);
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
	if (!TargetInventory && IsPlayerInventory(SourceInventory))
	{
		TargetInventory = SourceInventory;
	}
	if (!ActionComponent || !SourceInventory || !TargetInventory || !ItemInstance || StackCount <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = ItemInstance->GetItemId();
	Request.StackCount = StackCount;
	if (SourceInventory == TargetInventory)
	{
		const FRpgInventoryGridPlacement SourcePlacement = SlotViewModel->GetItemPlacement().IsValid()
			? SlotViewModel->GetItemPlacement()
			: SlotViewModel->GetPlacement();
		BuildPlayerQuickTransferTargets(SourcePlacement, Request.PreferredTargetContainers);
	}
	ActionComponent->RequestQuickTransferItem(SourceInventory, TargetInventory, Request);
	return true;
}

bool URpgInventoryDragDropCoordinator::CanQuickTransferPlayerItem(URpgInventoryItemInstance* ItemInstance) const
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!PlayerInventory || !Actions || !ItemInstance || !PlayerInventory->ContainsItemInstance(ItemInstance))
	{
		return false;
	}

	FRpgInventoryGridPlacement SourcePlacement;
	if (!PlayerInventory->GetItemPlacement(ItemInstance, SourcePlacement))
	{
		return false;
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.ItemId = ItemInstance->GetItemId();
	Request.StackCount = PlayerInventory->GetItemStackCount(ItemInstance);
	BuildPlayerQuickTransferTargets(SourcePlacement, Request.PreferredTargetContainers);
	if (Request.StackCount <= 0 || Request.PreferredTargetContainers.IsEmpty())
	{
		return false;
	}

	FRpgInventoryContainerHandle TargetContainer;
	FRpgInventoryGridPlacement TargetPlacement;
	return Actions->FindQuickTransferDestination(
		PlayerInventory,
		PlayerInventory,
		Request,
		TargetContainer,
		TargetPlacement);
}

bool URpgInventoryDragDropCoordinator::QuickTransferPlayerItem(URpgInventoryItemInstance* ItemInstance)
{
	if (!CanQuickTransferPlayerItem(ItemInstance))
	{
		return false;
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	FRpgInventoryGridPlacement SourcePlacement;
	if (!PlayerInventory || !Actions || !PlayerInventory->GetItemPlacement(ItemInstance, SourcePlacement))
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = ItemInstance->GetItemId();
	Request.StackCount = PlayerInventory->GetItemStackCount(ItemInstance);
	BuildPlayerQuickTransferTargets(SourcePlacement, Request.PreferredTargetContainers);
	Actions->RequestQuickTransferItem(PlayerInventory, PlayerInventory, Request);
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

	ActionComponent->RequestSplitItemStackById(
		Inventory,
		ItemInstance->GetItemId(),
		SplitCount,
		TargetPlacement,
		FGuid::NewGuid());
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

	const URpgInventoryFragment_UsableItem* Usable = ItemInstance->FindFragmentByClass<URpgInventoryFragment_UsableItem>();
	const bool bEquippable = ItemInstance->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() != nullptr ||
		ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;
	const ERpgInventoryItemActionIntent QuickIntent = Usable && bEquippable
		? (Usable->HybridQuickAction == ERpgInventoryHybridQuickAction::EquipAndActivate
			? ERpgInventoryItemActionIntent::EquipAndActivate
			: ERpgInventoryItemActionIntent::Use)
		: (bEquippable ? ERpgInventoryItemActionIntent::EquipAndActivate : ERpgInventoryItemActionIntent::Use);
	return ExecuteEntryItemAction(
		EntryViewModel,
		QuickIntent,
		StackCount);
}

bool URpgInventoryDragDropCoordinator::ExecuteEntryItemAction(
	URpgInventoryEntryViewModel* EntryViewModel,
	ERpgInventoryItemActionIntent Intent,
	int32 StackCount)
{
	if (!EntryViewModel || !EntryViewModel->CanDrag())
	{
		return false;
	}

	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* Item = EntryViewModel->GetItemInstance();
	if (!Actions || !Inventory || !Item || EntryViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	FRpgInventoryItemActionRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = Item->GetItemId();
	Request.Intent = Intent;
	Request.StackCount = FMath::Max(1, StackCount);
	Actions->RequestExecuteInventoryItemAction(Inventory, Request);
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

	if (SlotViewModel->IsGearSlot())
	{
		ActionComponent->RequestUnequipInventoryItemToContentSlot(ItemInstance);
		return true;
	}

	const URpgInventoryFragment_UsableItem* Usable = ItemInstance->FindFragmentByClass<URpgInventoryFragment_UsableItem>();
	const bool bEquippable = ItemInstance->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() != nullptr ||
		ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;
	const ERpgInventoryItemActionIntent QuickIntent = Usable && bEquippable
		? (Usable->HybridQuickAction == ERpgInventoryHybridQuickAction::EquipAndActivate
			? ERpgInventoryItemActionIntent::EquipAndActivate
			: ERpgInventoryItemActionIntent::Use)
		: (bEquippable ? ERpgInventoryItemActionIntent::EquipAndActivate : ERpgInventoryItemActionIntent::Use);
	return ExecuteAddressItemAction(
		SlotViewModel,
		QuickIntent,
		StackCount);
}

bool URpgInventoryDragDropCoordinator::ExecuteAddressItemAction(
	URpgInventoryAddressSlotViewModel* SlotViewModel,
	ERpgInventoryItemActionIntent Intent,
	int32 StackCount)
{
	if (!SlotViewModel || !SlotViewModel->CanDrag())
	{
		return false;
	}

	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgInventoryItemInstance* Item = SlotViewModel->GetItemInstance();
	if (!Actions || !Inventory || !Item || SlotViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	FRpgInventoryItemActionRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = Item->GetItemId();
	Request.Intent = Intent;
	Request.StackCount = FMath::Max(1, StackCount);
	Actions->RequestExecuteInventoryItemAction(Inventory, Request);
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

	ActionComponent->RequestSplitItemStackById(
		Inventory,
		ItemInstance->GetItemId(),
		SplitCount,
		TargetPlacement,
		FGuid::NewGuid());
	return true;
}

bool URpgInventoryDragDropCoordinator::DropEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount, bool bConfirmed)
{
	URpgInventoryManagerComponent* Inventory = nullptr;
	FRpgInventoryManualDropRequest Request;
	if (!PrepareDropEntryRequest(EntryViewModel, StackCount, Inventory, Request))
	{
		return false;
	}

	Request.bConfirmed = bConfirmed;
	return DispatchManualDropRequest(Inventory, Request);
}

bool URpgInventoryDragDropCoordinator::DropAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount, bool bConfirmed)
{
	URpgInventoryManagerComponent* Inventory = nullptr;
	FRpgInventoryManualDropRequest Request;
	if (!PrepareDropAddressSlotRequest(SlotViewModel, StackCount, Inventory, Request))
	{
		return false;
	}

	Request.bConfirmed = bConfirmed;
	return DispatchManualDropRequest(Inventory, Request);
}

bool URpgInventoryDragDropCoordinator::PrepareDropEntryRequest(
	URpgInventoryEntryViewModel* EntryViewModel,
	int32 StackCount,
	URpgInventoryManagerComponent*& OutInventory,
	FRpgInventoryManualDropRequest& OutRequest) const
{
	OutInventory = nullptr;
	OutRequest = FRpgInventoryManualDropRequest();
	if (!EntryViewModel || !EntryViewModel->CanDrag() || EntryViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	const int32 RequestedStackCount = StackCount <= 0 ? EntryViewModel->GetStackCount() : StackCount;
	if (!BuildManualDropRequest(Inventory, ItemInstance, RequestedStackCount, OutRequest) ||
		OutRequest.EntryId != EntryViewModel->GetEntryId() ||
		OutRequest.ExpectedSourcePlacement != EntryViewModel->GetPlacement() ||
		RequestedStackCount > EntryViewModel->GetStackCount())
	{
		OutRequest = FRpgInventoryManualDropRequest();
		return false;
	}

	OutInventory = Inventory;
	return true;
}

bool URpgInventoryDragDropCoordinator::PrepareDropAddressSlotRequest(
	URpgInventoryAddressSlotViewModel* SlotViewModel,
	int32 StackCount,
	URpgInventoryManagerComponent*& OutInventory,
	FRpgInventoryManualDropRequest& OutRequest) const
{
	OutInventory = nullptr;
	OutRequest = FRpgInventoryManualDropRequest();
	if (!SlotViewModel || !SlotViewModel->CanDrag() || SlotViewModel->GetStackCount() <= 0)
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	const int32 RequestedStackCount = StackCount <= 0 ? SlotViewModel->GetStackCount() : StackCount;
	const FRpgInventoryGridPlacement ExpectedViewPlacement = SlotViewModel->GetItemPlacement();
	if (!BuildManualDropRequest(Inventory, ItemInstance, RequestedStackCount, OutRequest) ||
		OutRequest.EntryId != SlotViewModel->GetEntryId() ||
		!ExpectedViewPlacement.IsValid() ||
		OutRequest.ExpectedSourcePlacement != ExpectedViewPlacement ||
		RequestedStackCount > SlotViewModel->GetStackCount())
	{
		OutRequest = FRpgInventoryManualDropRequest();
		return false;
	}

	OutInventory = Inventory;
	return true;
}

bool URpgInventoryDragDropCoordinator::UnequipEquipmentItem(
	ERpgEquipmentSlot EquipmentSlot,
	FRpgInventoryItemId ExpectedItemId)
{
	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	URpgInventoryItemInstance* ItemInstance = ResolveCurrentEquipmentItem(EquipmentSlot, ExpectedItemId);
	if (!ActionComponent || !ItemInstance)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	// The server resolves the item's current physical gear/carry address again and rejects stale requests.
	ActionComponent->RequestUnequipInventoryItemToContentSlot(ItemInstance);
	return true;
}

bool URpgInventoryDragDropCoordinator::DropEquipmentItem(
	ERpgEquipmentSlot EquipmentSlot,
	FRpgInventoryItemId ExpectedItemId,
	bool bConfirmed)
{
	URpgInventoryManagerComponent* Inventory = nullptr;
	FRpgInventoryManualDropRequest Request;
	if (!PrepareDropEquipmentItemRequest(EquipmentSlot, ExpectedItemId, Inventory, Request))
	{
		return false;
	}

	Request.bConfirmed = bConfirmed;
	return DispatchManualDropRequest(Inventory, Request);
}

bool URpgInventoryDragDropCoordinator::PrepareDropEquipmentItemRequest(
	ERpgEquipmentSlot EquipmentSlot,
	FRpgInventoryItemId ExpectedItemId,
	URpgInventoryManagerComponent*& OutInventory,
	FRpgInventoryManualDropRequest& OutRequest) const
{
	OutInventory = nullptr;
	OutRequest = FRpgInventoryManualDropRequest();
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryItemInstance* ItemInstance =
		ResolveCurrentEquipmentItem(EquipmentSlot, ExpectedItemId);
	const int32 StackCount = PlayerInventory && ItemInstance
		? PlayerInventory->GetItemStackCount(ItemInstance)
		: 0;
	if (!ExpectedItemId.IsValid() || !ItemInstance ||
		ItemInstance->GetItemId() != ExpectedItemId ||
		!BuildManualDropRequest(PlayerInventory, ItemInstance, StackCount, OutRequest))
	{
		OutRequest = FRpgInventoryManualDropRequest();
		return false;
	}

	OutInventory = PlayerInventory;
	return true;
}

bool URpgInventoryDragDropCoordinator::DispatchManualDropRequest(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryManualDropRequest Request)
{
	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	if (!ActionComponent || IsInteractionRequestPending() ||
		!IsManualDropRequestCurrent(Inventory, Request))
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
		if (HasHeldPayload())
		{
			return false;
		}
	}

	ActionComponent->RequestDropInventoryItemById(Inventory, Request);
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
	return PreviewState != ERpgInventoryInteractionPreviewState::Blocked &&
		PreviewState != ERpgInventoryInteractionPreviewState::OutOfBounds &&
		PreviewState != ERpgInventoryInteractionPreviewState::Pending &&
		PreviewState != ERpgInventoryInteractionPreviewState::Rejected &&
		PreviewState != ERpgInventoryInteractionPreviewState::None;
}

bool URpgInventoryDragDropCoordinator::UpdateInteractionPreview(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target)
{
	const FRpgInventoryDragPayload ResolvedPayload = ResolveInteractionPayload(Payload);
	const ERpgInventoryInteractionPreviewState PreviewState = ResolveInteractionPreview(ResolvedPayload, Target);
	if (InteractionSession && (!InteractionSession->HasPayload() || IsSameInteractionPayload(ResolvedPayload, InteractionSession->GetPayload())))
	{
		if (!InteractionSession->HasPayload() &&
			!InteractionSession->BeginInteraction(ResolvedPayload, ERpgInventoryInteractionInputMode::Mouse))
		{
			return false;
		}
		InteractionSession->SetPreviewTarget(Target, PreviewState);
	}

	return PreviewState != ERpgInventoryInteractionPreviewState::Blocked &&
		PreviewState != ERpgInventoryInteractionPreviewState::OutOfBounds &&
		PreviewState != ERpgInventoryInteractionPreviewState::Pending &&
		PreviewState != ERpgInventoryInteractionPreviewState::Rejected &&
		PreviewState != ERpgInventoryInteractionPreviewState::None;
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
	auto SubmitExactPlacementMutation = [this, Actions, &Payload, &Target](
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryGridPlacement& SourcePlacement,
		ERpgInventoryMutationOperation Operation)
	{
		if (!Inventory || !Payload.ItemInstance || !SourcePlacement.IsValid() || !Target.TargetPlacement.IsValid())
		{
			return false;
		}

		FRpgInventoryMutationRequest Request;
		Request.RequestId = MarkInteractionRequestPending(Payload, Target);
		Request.Operation = Operation;
		Request.ItemId = Payload.ItemInstance->GetItemId();
		Request.Source = SourcePlacement.GetContainerHandle();
		Request.Target = Target.TargetPlacement.GetContainerHandle();
		Request.TargetPlacement = Target.TargetPlacement;
		Request.Quantity = Payload.StackCount;
		if (!Request.RequestId.IsValid())
		{
			return false;
		}
		Actions->RequestInventoryMutation(Inventory, Request);
		return true;
	};

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

				return SubmitExactPlacementMutation(
					Payload.SourceInventory,
					Payload.SourcePlacement,
					ERpgInventoryMutationOperation::Move);
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
			URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
			FRpgInventorySlotAddress TargetAddress;
			if (!Target.TargetPlacement.IsValid() || !InventoryLayout ||
				!InventoryLayout->TryMakeSlotAddressFromPlacement(Target.TargetPlacement, TargetAddress))
			{
				return false;
			}

			URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
			FRpgInventoryGridPlacement SourcePlacement;
			return PlayerInventory && PlayerInventory->GetItemPlacement(Payload.ItemInstance, SourcePlacement) &&
				SubmitExactPlacementMutation(PlayerInventory, SourcePlacement, ERpgInventoryMutationOperation::Move);
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

			FRpgInventoryGridPlacement SourcePlacement;
			return PlayerInventory && PlayerInventory->GetItemPlacement(Payload.ItemInstance, SourcePlacement) &&
				SubmitExactPlacementMutation(PlayerInventory, SourcePlacement, ERpgInventoryMutationOperation::Move);
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

		FRpgQuickAccessMutationRequest Request;
		Request.RequestId = MarkInteractionRequestPending(Payload, Target);
		Request.SlotIndex = Target.ActionBarSlotIndex;
		Request.SourceAddress = SourceAddress;
		Request.ContextItemId = Payload.ItemInstance->GetItemId();
		if (!Request.RequestId.IsValid())
		{
			return false;
		}

		if (InventoryLayout->IsCarrySlotAddress(SourceAddress))
		{
			Request.Operation = ERpgQuickAccessMutationOperation::BindCarry;
			Request.ExpectedCarryRole = SourceAddress.ContainerId;
		}
		else
		{
			Request.Operation = ERpgQuickAccessMutationOperation::BindConsumable;
			Request.ExpectedConsumableDefinition = Payload.ItemInstance->GetItemDef();
			Request.ExpectedPreferredItemId = Payload.ItemInstance->GetItemId();
		}
		Actions->RequestMutateQuickAccessBinding(Request);
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

void URpgInventoryDragDropCoordinator::HandleInteractionPayloadChanged(
	bool bHasPayload,
	const FRpgInventoryDragPayload& Payload)
{
	OnHeldPayloadChanged.Broadcast(bHasPayload, bHasPayload ? Payload : FRpgInventoryDragPayload());
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

FGuid URpgInventoryDragDropCoordinator::MarkInteractionRequestPending(
	const FRpgInventoryDragPayload& Payload,
	const FRpgInventoryDropTarget& Target)
{
	EnsureInteractionSession();
	if (!InteractionSession)
	{
		return FGuid();
	}

	if (!InteractionSession->HasPayload() || !IsSameInteractionPayload(Payload, InteractionSession->GetPayload()))
	{
		InteractionSession->BeginInteraction(Payload, ERpgInventoryInteractionInputMode::Mouse);
	}
	InteractionSession->SetPreviewTarget(Target, ResolveInteractionPreview(Payload, Target));
	InteractionSession->MarkRequestPending(Target, ResolveActionTagForTarget(Target));
	return InteractionSession->GetRequestId();
}

bool URpgInventoryDragDropCoordinator::CanCommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const
{
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!IsPayloadValid(Payload) || !IsTargetValid(Target) || !Actions ||
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
				? Actions->CanTransferItemStackToPlacement(
					Payload.SourceInventory,
					Target.TargetInventory,
					Payload.ItemInstance,
					Payload.StackCount,
					Target.TargetPlacement)
				: Actions->CanTransferItemStack(
					Payload.SourceInventory,
					Target.TargetInventory,
					Payload.ItemInstance,
					Payload.StackCount);
		}

		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
			URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
			FRpgInventorySlotAddress TargetAddress;
			if (!PlayerInventory || Target.TargetInventory != PlayerInventory || !InventoryLayout ||
				!Payload.ItemInstance || !Target.TargetPlacement.IsValid() ||
				!InventoryLayout->TryMakeSlotAddressFromPlacement(Target.TargetPlacement, TargetAddress) ||
				!InventoryLayout->CanItemUseSlotAddress(Payload.ItemInstance, TargetAddress))
			{
				return false;
			}

			const TArray<FRpgInventoryEntryView> Entries = PlayerInventory->GetAllEntries();
			const FRpgInventoryEntryView* SourceEntry = Entries.FindByPredicate(
				[&Payload](const FRpgInventoryEntryView& Entry)
				{
					return Entry.Instance == Payload.ItemInstance;
				});
			return SourceEntry &&
				PlayerInventory->CanMoveInventoryEntryToPlacement(SourceEntry->EntryId, Target.TargetPlacement);
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
				FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(Target.EquipmentSlot) &&
				FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
					Payload.ItemInstance,
					Target.EquipmentSlot);
		}

		return Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot &&
			FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(Target.EquipmentSlot) &&
			FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
				Payload.ItemInstance,
				Target.EquipmentSlot);
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

URpgInventoryItemInstance* URpgInventoryDragDropCoordinator::ResolveCurrentEquipmentItem(
	ERpgEquipmentSlot EquipmentSlot,
	const FRpgInventoryItemId& ExpectedItemId) const
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!PlayerInventory || !InventoryLayout || !ExpectedItemId.IsValid())
	{
		return nullptr;
	}

	URpgInventoryItemInstance* ItemInstance = PlayerInventory->FindItemById(ExpectedItemId);
	if (!ItemInstance)
	{
		return nullptr;
	}

	FRpgInventorySlotAddress GearAddress;
	if (URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, GearAddress))
	{
		return InventoryLayout->GetItemInSlotAddress(GearAddress) == ItemInstance
			? ItemInstance
			: nullptr;
	}

	// Main-/Offhand projections resolve through their currently selected carry address.
	const FRpgInventoryDragPayload Payload = MakeEquipmentPayload(ItemInstance, EquipmentSlot);
	return ResolveEquipmentPayloadSourceAddress(Payload).IsValid()
		? ItemInstance
		: nullptr;
}

bool URpgInventoryDragDropCoordinator::BuildManualDropRequest(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* ItemInstance,
	int32 RequestedStackCount,
	FRpgInventoryManualDropRequest& OutRequest) const
{
	OutRequest = FRpgInventoryManualDropRequest();
	if (!Inventory || !ItemInstance || RequestedStackCount <= 0 ||
		!ItemInstance->GetItemId().IsValid())
	{
		return false;
	}

	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		if (Entry.Instance != ItemInstance ||
			Entry.ItemId != ItemInstance->GetItemId() ||
			!Entry.EntryId.IsValid() ||
			!Entry.Placement.IsValid() ||
			RequestedStackCount > Entry.StackCount)
		{
			continue;
		}

		OutRequest.RequestId = FGuid::NewGuid();
		OutRequest.EntryId = Entry.EntryId;
		OutRequest.ItemId = Entry.ItemId;
		OutRequest.ExpectedSourcePlacement = Entry.Placement;
		OutRequest.StackCount = RequestedStackCount;
		OutRequest.bConfirmed = false;
		return true;
	}

	return false;
}

bool URpgInventoryDragDropCoordinator::IsManualDropRequestCurrent(
	const URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryManualDropRequest& Request) const
{
	if (!Inventory || !Request.RequestId.IsValid() ||
		!Request.EntryId.IsValid() || !Request.ItemId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.StackCount <= 0)
	{
		return false;
	}

	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		if (Entry.EntryId == Request.EntryId &&
			Entry.ItemId == Request.ItemId &&
			Entry.Instance &&
			Entry.Instance->GetItemId() == Request.ItemId)
		{
			return Entry.Placement == Request.ExpectedSourcePlacement &&
				Entry.StackCount >= Request.StackCount;
		}
	}

	return false;
}

bool URpgInventoryDragDropCoordinator::IsPlayerInventory(const URpgInventoryManagerComponent* Inventory) const
{
	return Inventory != nullptr && Inventory == FindPlayerInventory();
}

void URpgInventoryDragDropCoordinator::BuildPlayerQuickTransferTargets(
	const FRpgInventoryGridPlacement& SourcePlacement,
	TArray<FRpgInventoryContainerHandle>& OutTargets) const
{
	OutTargets.Reset();
	URpgPlayerInventoryLayoutComponent* Layout = FindPlayerInventoryLayout();
	if (!Layout || !SourcePlacement.GetContainerHandle().IsValid())
	{
		return;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = Layout->GetSlotGroups();
	const FRpgInventorySlotGroupView* SourceGroup = Groups.FindByPredicate([&SourcePlacement](const FRpgInventorySlotGroupView& Group)
	{
		return Group.ContainerHandle == SourcePlacement.GetContainerHandle();
	});
	const bool bSourceIsBackpack = SourceGroup && SourceGroup->SourceEquipmentSlotName == TEXT("Backpack");

	auto AddMatchingGroups = [&Groups, &OutTargets](TFunctionRef<bool(const FRpgInventorySlotGroupView&)> Predicate)
	{
		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			if (Group.GroupKind == ERpgInventorySlotGroupKind::Content && Group.ContainerHandle.IsValid() && Predicate(Group))
			{
				OutTargets.AddUnique(Group.ContainerHandle);
			}
		}
	};

	if (bSourceIsBackpack)
	{
		AddMatchingGroups([](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerId == URpgPlayerInventoryLayoutComponent::PocketsGroupId;
		});
		AddMatchingGroups([](const FRpgInventorySlotGroupView& Group)
		{
			return Group.SourceEquipmentSlotName == TEXT("Belt");
		});
		AddMatchingGroups([](const FRpgInventorySlotGroupView& Group)
		{
			return Group.SourceEquipmentSlotName == TEXT("Pouch");
		});
		return;
	}

	AddMatchingGroups([](const FRpgInventorySlotGroupView& Group)
	{
		return Group.SourceEquipmentSlotName == TEXT("Backpack");
	});
}
