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
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDragDrop)

namespace
{
	constexpr ERpgInventoryContextAction ContextActionDisplayOrder[] =
	{
		ERpgInventoryContextAction::OpenContainer,
		ERpgInventoryContextAction::Inspect,
		ERpgInventoryContextAction::Unequip,
		ERpgInventoryContextAction::Use,
		ERpgInventoryContextAction::EquipAndActivate,
		ERpgInventoryContextAction::MoveToCarry,
		ERpgInventoryContextAction::Split,
		ERpgInventoryContextAction::Rotate,
		ERpgInventoryContextAction::QuickAccessBind,
		ERpgInventoryContextAction::QuickAccessUnbind,
		ERpgInventoryContextAction::Transfer,
		ERpgInventoryContextAction::Drop
	};

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
		return ItemInstance &&
			URpgInventoryManagerComponent::
				GetEffectiveMaxStackSizeForDefinition(
					ItemInstance->GetItemDef()) > 1;
	}

	bool IsManualDropLocallyAllowed(const URpgInventoryItemInstance* ItemInstance)
	{
		const URpgInventoryFragment_ItemTraits* Traits = ItemInstance
			? ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>()
			: nullptr;
		return ItemInstance &&
			(!Traits || Traits->GetResolvedManualDropPolicy() != ERpgInventoryManualDropPolicy::Disabled);
	}

	bool IsEntryViewCurrent(const URpgInventoryEntryViewModel* EntryViewModel)
	{
		const URpgInventoryManagerComponent* Inventory = EntryViewModel
			? EntryViewModel->GetInventoryManager()
			: nullptr;
		const URpgInventoryItemInstance* ItemInstance = EntryViewModel
			? EntryViewModel->GetItemInstance()
			: nullptr;
		if (!Inventory || !ItemInstance || !EntryViewModel->GetEntryId().IsValid() ||
			!ItemInstance->GetItemId().IsValid() || EntryViewModel->GetStackCount() <= 0)
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.EntryId == EntryViewModel->GetEntryId())
			{
				return Entry.Instance == ItemInstance &&
					Entry.ItemId == ItemInstance->GetItemId() &&
					Entry.StackCount == EntryViewModel->GetStackCount() &&
					Entry.Placement == EntryViewModel->GetPlacement();
			}
		}
		return false;
	}

	bool IsAddressViewCurrent(const URpgInventoryAddressSlotViewModel* SlotViewModel)
	{
		const URpgInventoryManagerComponent* Inventory = SlotViewModel
			? SlotViewModel->GetInventoryManager()
			: nullptr;
		const URpgPlayerInventoryLayoutComponent* InventoryLayout = SlotViewModel
			? SlotViewModel->GetInventoryLayout()
			: nullptr;
		const URpgInventoryItemInstance* ItemInstance = SlotViewModel
			? SlotViewModel->GetItemInstance()
			: nullptr;
		if (!Inventory || !InventoryLayout || !ItemInstance ||
			!SlotViewModel->GetSlotAddress().IsValid() ||
			!SlotViewModel->GetEntryId().IsValid() ||
			!ItemInstance->GetItemId().IsValid() ||
			SlotViewModel->GetStackCount() <= 0 ||
			InventoryLayout->GetItemInSlotAddress(SlotViewModel->GetSlotAddress()) != ItemInstance)
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.EntryId == SlotViewModel->GetEntryId())
			{
				return Entry.Instance == ItemInstance &&
					Entry.ItemId == ItemInstance->GetItemId() &&
					Entry.StackCount == SlotViewModel->GetStackCount() &&
					Entry.Placement == SlotViewModel->GetItemPlacement();
			}
		}
		return false;
	}

	bool TryCaptureSourceSnapshot(
		const URpgInventoryManagerComponent* Inventory,
		const URpgInventoryItemInstance* ItemInstance,
		FGuid& OutEntryId,
		FRpgInventoryGridPlacement& OutPlacement,
		int32& OutSourceQuantity)
	{
		OutEntryId.Invalidate();
		OutPlacement = FRpgInventoryGridPlacement();
		OutSourceQuantity = 0;
		if (!Inventory || !ItemInstance || !ItemInstance->GetItemId().IsValid())
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.Instance == ItemInstance &&
				Entry.ItemId == ItemInstance->GetItemId() &&
				Entry.EntryId.IsValid() && Entry.Placement.IsValid() &&
				Entry.StackCount > 0)
			{
				OutEntryId = Entry.EntryId;
				OutPlacement = Entry.Placement;
				OutSourceQuantity = Entry.StackCount;
				return true;
			}
		}
		return false;
	}

	bool CanUseItemLocally(
		const URpgInventoryItemInstance* ItemInstance,
		const URpgInventoryManagerComponent* SourceInventory,
		const URpgInventoryManagerComponent* PlayerInventory,
		int32 AvailableStackCount)
	{
		const URpgInventoryFragment_UsableItem* Usable = ItemInstance
			? ItemInstance->FindFragmentByClass<URpgInventoryFragment_UsableItem>()
			: nullptr;
		return Usable &&
			Usable->UseAbility &&
			(!Usable->bOnlyFromPlayerInventory || SourceInventory == PlayerInventory) &&
			FMath::Max(0, Usable->ConsumeCount) <= AvailableStackCount;
	}

	bool CanMoveItemToCarryLocally(const URpgInventoryItemInstance* ItemInstance)
	{
		return FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
				ItemInstance,
				ERpgEquipmentSlot::MainHand) ||
			FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
				ItemInstance,
				ERpgEquipmentSlot::OffHand);
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

	bool IsEquipmentPlacement(
		const URpgPlayerInventoryLayoutComponent* InventoryLayout,
		const FRpgInventoryGridPlacement& Placement)
	{
		FRpgInventorySlotAddress Address;
		return InventoryLayout && Placement.IsValid() &&
			InventoryLayout->TryMakeSlotAddressFromPlacement(
				Placement,
				Address) &&
			(InventoryLayout->IsGearSlotAddress(Address) ||
				InventoryLayout->IsCarrySlotAddress(Address));
	}

	bool IsExactPlacementSnapshot(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool TryGetExactPayloadEntry(
		const FRpgInventoryDragPayload& Payload,
		FRpgInventoryEntryView& OutEntry)
	{
		OutEntry = FRpgInventoryEntryView();
		if (!Payload.SourceInventory || !Payload.ItemInstance ||
			!Payload.EntryId.IsValid() ||
			!Payload.SourcePlacement.IsValid() ||
			Payload.StackCount <= 0)
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry :
			 Payload.SourceInventory->GetAllEntries())
		{
			if (Entry.EntryId == Payload.EntryId)
			{
				if (Entry.Instance != Payload.ItemInstance ||
					Entry.ItemId != Payload.ItemInstance->GetItemId() ||
					Entry.StackCount != Payload.StackCount ||
					!IsExactPlacementSnapshot(
						Entry.Placement,
						Payload.SourcePlacement))
				{
					return false;
				}
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	FRpgInventoryInteractionPreviewPlan MakeSimpleInteractionPreview(
		ERpgInventoryInteractionPreviewState State)
	{
		FRpgInventoryInteractionPreviewPlan Result;
		Result.State = State;
		return Result;
	}

	FRpgInventoryInteractionPreviewPlan ProjectPlacementPreview(
		FRpgInventoryPlacementPlan Plan,
		ERpgInventoryInteractionPreviewState PlacementState,
		bool bAcceptNoOp)
	{
		FRpgInventoryInteractionPreviewPlan Result;
		Result.bUsesPlacementPlan = true;
		Result.PlacementPlan = MoveTemp(Plan);
		if (!Result.PlacementPlan.IsCompleteSuccess())
		{
			Result.State =
				Result.PlacementPlan.Code ==
					ERpgInventoryMutationResultCode::OutOfBounds
					? ERpgInventoryInteractionPreviewState::OutOfBounds
					: ERpgInventoryInteractionPreviewState::Blocked;
			return Result;
		}

		bool bHasPlace = false;
		bool bHasMerge = false;
		bool bHasSwap = false;
		bool bHasNoOp = false;
		for (const FRpgInventoryPlacementStep& Step :
			 Result.PlacementPlan.Steps)
		{
			switch (Step.Resolution)
			{
			case ERpgInventoryPlacementResolution::Place:
				bHasPlace = true;
				if (!Result.ResolvedTargetPlacement.IsValid())
				{
					Result.ResolvedTargetPlacement = Step.Placement;
				}
				break;
			case ERpgInventoryPlacementResolution::Merge:
				bHasMerge = true;
				break;
			case ERpgInventoryPlacementResolution::Swap:
				bHasSwap = true;
				Result.ResolvedTargetPlacement = Step.Placement;
				break;
			case ERpgInventoryPlacementResolution::NoOp:
				bHasNoOp = true;
				break;
			default:
				break;
			}
		}
		if (!Result.ResolvedTargetPlacement.IsValid() &&
			!Result.PlacementPlan.Steps.IsEmpty())
		{
			Result.ResolvedTargetPlacement =
				Result.PlacementPlan.Steps[0].Placement;
		}

		Result.State = bHasSwap
			? ERpgInventoryInteractionPreviewState::Swap
			: bHasPlace
				? PlacementState
				: bHasMerge
					? ERpgInventoryInteractionPreviewState::Merge
					: bHasNoOp && bAcceptNoOp
						? PlacementState
						: ERpgInventoryInteractionPreviewState::Blocked;
		return Result;
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
	Payload.ItemFootprint = GetDragPayloadItemFootprint(Payload.ItemInstance);

	AActor* InventoryOwner =
		ItemInstance ? Cast<AActor>(ItemInstance->GetOuter()) : nullptr;
	TArray<URpgInventoryManagerComponent*> Inventories;
	if (InventoryOwner)
	{
		InventoryOwner->GetComponents(Inventories);
	}
	for (URpgInventoryManagerComponent* Inventory : Inventories)
	{
		if (!Inventory || !Inventory->ContainsItemInstance(ItemInstance))
		{
			continue;
		}

		for (const FRpgInventoryEntryView& Entry :
			Inventory->GetAllEntries())
		{
			if (Entry.Instance != ItemInstance ||
				Entry.ItemId != ItemInstance->GetItemId() ||
				!Entry.EntryId.IsValid() ||
				!Entry.Placement.IsValid() ||
				Entry.StackCount <= 0)
			{
				continue;
			}

			Payload.SourceInventory = Inventory;
			Payload.EntryId = Entry.EntryId;
			Payload.StackCount = Entry.StackCount;
			Payload.SourcePlacement = Entry.Placement;
			return Payload;
		}
	}

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
		return Payload.SourceInventory != nullptr &&
			Payload.ItemInstance != nullptr &&
			Payload.EntryId.IsValid() &&
			Payload.SourcePlacement.IsValid() &&
			Payload.StackCount > 0 &&
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

bool URpgInventoryDragDropCoordinator::CanToggleInteractionRotation() const
{
	if (!InteractionSession || !InteractionSession->HasPayload() ||
		InteractionSession->IsRequestPending())
	{
		return false;
	}

	const FRpgInventoryDragPayload Payload = InteractionSession->GetPayload();
	const URpgInventoryFragment_SpatialItem* SpatialFragment = Payload.ItemInstance
		? Payload.ItemInstance->FindFragmentByClass<URpgInventoryFragment_SpatialItem>()
		: nullptr;
	return IsPayloadSourceCurrent(Payload) &&
		SpatialFragment &&
		SpatialFragment->bAllowRotation;
}

bool URpgInventoryDragDropCoordinator::ToggleInteractionRotation()
{
	if (!CanToggleInteractionRotation())
	{
		return false;
	}

	return InteractionSession->ToggleTargetRotation();
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

TArray<ERpgInventoryContextAction> URpgInventoryDragDropCoordinator::GetAvailableContextActions(
	URpgInventoryEntryViewModel* EntryViewModel,
	bool bSupportsSpatialRotation) const
{
	TArray<ERpgInventoryContextAction> Actions;
	Actions.Reserve(UE_ARRAY_COUNT(ContextActionDisplayOrder));
	for (const ERpgInventoryContextAction Action : ContextActionDisplayOrder)
	{
		if (CanExecuteContextAction(EntryViewModel, Action, bSupportsSpatialRotation))
		{
			Actions.Add(Action);
		}
	}
	return Actions;
}

bool URpgInventoryDragDropCoordinator::CanExecuteContextAction(
	URpgInventoryEntryViewModel* EntryViewModel,
	ERpgInventoryContextAction Action,
	bool bSupportsSpatialRotation) const
{
	if (!IsEntryViewCurrent(EntryViewModel))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	const bool bCanDispatch = EntryViewModel->CanDrag() && ResolveUiActionComponent() != nullptr;
	const bool bCanMutate = bCanDispatch && !IsInteractionRequestPending();
	switch (Action)
	{
	case ERpgInventoryContextAction::OpenContainer:
		return ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;

	case ERpgInventoryContextAction::Inspect:
		return true;

	case ERpgInventoryContextAction::Use:
		return bCanMutate &&
			CanUseItemLocally(ItemInstance, Inventory, FindPlayerInventory(), EntryViewModel->GetStackCount());

	case ERpgInventoryContextAction::EquipAndActivate:
		return bCanMutate && IsPlayerInventory(Inventory) &&
			(ItemInstance->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() != nullptr ||
				ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr);

	case ERpgInventoryContextAction::MoveToCarry:
		return bCanMutate && IsPlayerInventory(Inventory) &&
			CanMoveItemToCarryLocally(ItemInstance);

	case ERpgInventoryContextAction::Split:
		return CanQuickSplitEntry(EntryViewModel, FRpgInventoryGridPlacement(), 0);

	case ERpgInventoryContextAction::Rotate:
		return bSupportsSpatialRotation && bCanMutate && !HasHeldPayload() &&
			CanRotateEntryInPlace(
				Inventory,
				ItemInstance,
				EntryViewModel->GetEntryId(),
				EntryViewModel->GetPlacement());

	case ERpgInventoryContextAction::Transfer:
		return CanQuickTransferEntry(EntryViewModel);

	case ERpgInventoryContextAction::Drop:
	{
		bool bCanClearEquipmentSource = true;
		if (IsPlayerInventory(Inventory))
		{
			FRpgInventorySlotAddress SourceAddress;
			if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
				InventoryLayout &&
				InventoryLayout->TryMakeSlotAddressFromPlacement(
					EntryViewModel->GetPlacement(),
					SourceAddress))
			{
				bCanClearEquipmentSource = CanMoveItemOutOfAddress(SourceAddress);
			}
		}
		return bCanMutate &&
			bCanClearEquipmentSource &&
			IsManualDropLocallyAllowed(ItemInstance);
	}

	case ERpgInventoryContextAction::QuickAccessBind:
	case ERpgInventoryContextAction::QuickAccessUnbind:
	case ERpgInventoryContextAction::Unequip:
	default:
		return false;
	}
}

TArray<ERpgInventoryContextAction> URpgInventoryDragDropCoordinator::GetAvailableContextActions(
	URpgInventoryAddressSlotViewModel* SlotViewModel,
	bool bSupportsSpatialRotation) const
{
	TArray<ERpgInventoryContextAction> Actions;
	Actions.Reserve(UE_ARRAY_COUNT(ContextActionDisplayOrder));
	for (const ERpgInventoryContextAction Action : ContextActionDisplayOrder)
	{
		if (CanExecuteContextAction(SlotViewModel, Action, bSupportsSpatialRotation))
		{
			Actions.Add(Action);
		}
	}
	return Actions;
}

bool URpgInventoryDragDropCoordinator::CanExecuteContextAction(
	URpgInventoryAddressSlotViewModel* SlotViewModel,
	ERpgInventoryContextAction Action,
	bool bSupportsSpatialRotation) const
{
	if (!IsAddressViewCurrent(SlotViewModel))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = SlotViewModel->GetInventoryLayout();
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	const bool bCanDispatch = SlotViewModel->CanDrag() && ResolveUiActionComponent() != nullptr;
	const bool bCanMutate = bCanDispatch && !IsInteractionRequestPending();
	const bool bIsGear = SlotViewModel->IsGearSlot();
	const bool bIsCarry = SlotViewModel->IsCarrySlot();
	switch (Action)
	{
	case ERpgInventoryContextAction::OpenContainer:
		return !bIsGear &&
			ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;

	case ERpgInventoryContextAction::Inspect:
		return true;

	case ERpgInventoryContextAction::Unequip:
	{
		FRpgInventoryGridPlacement ContentTarget;
		return bIsGear && bCanMutate &&
			CanMoveItemOutOfAddress(SlotViewModel->GetSlotAddress()) &&
			ResolveUiActionComponent()->CanMoveItemToFirstCompatibleContentSlot(
				ItemInstance,
				ContentTarget);
	}

	case ERpgInventoryContextAction::Use:
		return !bIsGear && bCanMutate &&
			CanUseItemLocally(ItemInstance, Inventory, FindPlayerInventory(), SlotViewModel->GetStackCount());

	case ERpgInventoryContextAction::EquipAndActivate:
		return !bIsGear && bCanMutate && IsPlayerInventory(Inventory) &&
			(ItemInstance->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() != nullptr ||
				ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr);

	case ERpgInventoryContextAction::MoveToCarry:
		return !bIsGear && !bIsCarry && bCanMutate && IsPlayerInventory(Inventory) &&
			CanMoveItemToCarryLocally(ItemInstance);

	case ERpgInventoryContextAction::Split:
		return !bIsGear && !bIsCarry &&
			CanQuickSplitAddressSlot(SlotViewModel, FRpgInventoryGridPlacement(), 0);

	case ERpgInventoryContextAction::Rotate:
		return !bIsGear && !bIsCarry && bSupportsSpatialRotation &&
			bCanMutate && !HasHeldPayload() &&
			CanRotateEntryInPlace(
				Inventory,
				ItemInstance,
				SlotViewModel->GetEntryId(),
				SlotViewModel->GetItemPlacement());

	case ERpgInventoryContextAction::QuickAccessBind:
		return !bIsGear && bCanMutate && SlotViewModel->IsActionbarBindable() &&
			InventoryLayout->CanBindSlotAddressToActionbar(
				SlotViewModel->GetSlotAddress(),
				ItemInstance);

	case ERpgInventoryContextAction::QuickAccessUnbind:
		return !bIsGear && bCanMutate && SlotViewModel->IsActionbarBindable() &&
			InventoryLayout->CanBindSlotAddressToActionbar(
				SlotViewModel->GetSlotAddress(),
				ItemInstance) &&
			FindQuickAccessSlotForPayload(MakeInventoryPayloadFromAddressSlot(SlotViewModel)) != INDEX_NONE;

	case ERpgInventoryContextAction::Transfer:
		return !bIsGear && CanQuickTransferAddressSlot(SlotViewModel);

	case ERpgInventoryContextAction::Drop:
		return bCanMutate &&
			CanMoveItemOutOfAddress(SlotViewModel->GetSlotAddress()) &&
			IsManualDropLocallyAllowed(ItemInstance);

	default:
		return false;
	}
}

TArray<ERpgInventoryContextAction> URpgInventoryDragDropCoordinator::GetAvailableContextActions(
	ERpgEquipmentSlot EquipmentSlot,
	const FRpgInventoryItemId& ExpectedItemId) const
{
	TArray<ERpgInventoryContextAction> Actions;
	Actions.Reserve(UE_ARRAY_COUNT(ContextActionDisplayOrder));
	for (const ERpgInventoryContextAction Action : ContextActionDisplayOrder)
	{
		if (CanExecuteContextAction(EquipmentSlot, ExpectedItemId, Action))
		{
			Actions.Add(Action);
		}
	}
	return Actions;
}

bool URpgInventoryDragDropCoordinator::CanExecuteContextAction(
	ERpgEquipmentSlot EquipmentSlot,
	const FRpgInventoryItemId& ExpectedItemId,
	ERpgInventoryContextAction Action) const
{
	URpgInventoryItemInstance* ItemInstance =
		ResolveCurrentEquipmentItem(EquipmentSlot, ExpectedItemId);
	if (!ItemInstance)
	{
		return false;
	}

	const bool bCanDispatch = ResolveUiActionComponent() != nullptr;
	const bool bCanMutate = bCanDispatch && !IsInteractionRequestPending();
	switch (Action)
	{
	case ERpgInventoryContextAction::Inspect:
		return true;

	case ERpgInventoryContextAction::Unequip:
	{
		FRpgInventoryGridPlacement ContentTarget;
		return bCanMutate &&
			CanMoveItemOutOfAddress(
				ResolveEquipmentPayloadSourceAddress(
					MakeEquipmentPayload(ItemInstance, EquipmentSlot))) &&
			ResolveUiActionComponent()->CanMoveItemToFirstCompatibleContentSlot(
				ItemInstance,
				ContentTarget);
	}

	case ERpgInventoryContextAction::Drop:
		return bCanMutate &&
			CanMoveItemOutOfAddress(
				ResolveEquipmentPayloadSourceAddress(
					MakeEquipmentPayload(ItemInstance, EquipmentSlot))) &&
			IsManualDropLocallyAllowed(ItemInstance) &&
			FindPlayerInventory() &&
			FindPlayerInventory()->GetItemStackCount(ItemInstance) > 0;

	default:
		return false;
	}
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

	const FRpgInventoryContainerHandle SourceContainer =
		SourceAddress.GetContainerHandle();
	const bool bCarryBinding = InventoryLayout->IsCarrySlotAddress(SourceAddress);
	if (bCarryBinding &&
		!SourceContainer.IsRoot())
	{
		return INDEX_NONE;
	}
	const TSubclassOf<URpgInventoryItemDefinition> ConsumableDefinition = CurrentItem->GetItemDef();
	for (int32 SlotIndex = 0; SlotIndex < ActionBar->GetNumSlots(); ++SlotIndex)
	{
		const FRpgActionBarSlot Slot = ActionBar->GetSlot(SlotIndex);
		const bool bMatchesCarry = bCarryBinding &&
			(Slot.SlotType == ERpgActionBarSlotType::CarrySlot || Slot.SlotType == ERpgActionBarSlotType::CarrySlotBinding) &&
			Slot.CarryRole == SourceContainer.Root;
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
		!Payload.ItemInstance->GetItemId().IsValid() ||
		IsInteractionRequestPending())
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
		const FRpgInventoryContainerHandle SourceContainer =
			SourceAddress.GetContainerHandle();
		if (!SourceContainer.IsRoot())
		{
			return false;
		}

		Request.Operation = ERpgQuickAccessMutationOperation::BindCarry;
		Request.ExpectedCarryRole = SourceContainer.Root;
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
	if (IsInteractionRequestPending())
	{
		return false;
	}

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
	if (!EntryViewModel || !EntryViewModel->CanDrag() || !Actions ||
		IsInteractionRequestPending() ||
		!IsEntryViewCurrent(EntryViewModel))
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
	Request.ExpectedEntryId = EntryViewModel->GetEntryId();
	Request.ExpectedSourcePlacement = EntryViewModel->GetPlacement();
	Request.ExpectedSourceQuantity = EntryViewModel->GetStackCount();
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
	Request.ExpectedEntryId = EntryViewModel->GetEntryId();
	Request.ExpectedSourcePlacement = EntryViewModel->GetPlacement();
	Request.ExpectedSourceQuantity = StackCount;
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
	if (!SlotViewModel || !SlotViewModel->CanDrag() || !Actions ||
		IsInteractionRequestPending() ||
		!IsAddressViewCurrent(SlotViewModel))
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
	Request.ExpectedEntryId = SlotViewModel->GetEntryId();
	Request.ExpectedSourcePlacement = SlotViewModel->GetItemPlacement();
	Request.ExpectedSourceQuantity = SlotViewModel->GetStackCount();
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
	Request.ExpectedEntryId = SlotViewModel->GetEntryId();
	Request.ExpectedSourcePlacement = SlotViewModel->GetItemPlacement();
	Request.ExpectedSourceQuantity = StackCount;
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
	if (!PlayerInventory || !Actions || !ItemInstance ||
		IsInteractionRequestPending() ||
		!PlayerInventory->ContainsItemInstance(ItemInstance))
	{
		return false;
	}

	FGuid ExpectedEntryId;
	FRpgInventoryGridPlacement SourcePlacement;
	int32 ExpectedSourceQuantity = 0;
	if (!TryCaptureSourceSnapshot(
			PlayerInventory,
			ItemInstance,
			ExpectedEntryId,
			SourcePlacement,
			ExpectedSourceQuantity))
	{
		return false;
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.ItemId = ItemInstance->GetItemId();
	Request.ExpectedEntryId = ExpectedEntryId;
	Request.ExpectedSourcePlacement = SourcePlacement;
	Request.ExpectedSourceQuantity = ExpectedSourceQuantity;
	Request.StackCount = ExpectedSourceQuantity;
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
	FGuid ExpectedEntryId;
	FRpgInventoryGridPlacement SourcePlacement;
	int32 ExpectedSourceQuantity = 0;
	if (!PlayerInventory || !Actions ||
		!TryCaptureSourceSnapshot(
			PlayerInventory,
			ItemInstance,
			ExpectedEntryId,
			SourcePlacement,
			ExpectedSourceQuantity))
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
	Request.ExpectedEntryId = ExpectedEntryId;
	Request.ExpectedSourcePlacement = SourcePlacement;
	Request.ExpectedSourceQuantity = ExpectedSourceQuantity;
	Request.StackCount = ExpectedSourceQuantity;
	BuildPlayerQuickTransferTargets(SourcePlacement, Request.PreferredTargetContainers);
	Actions->RequestQuickTransferItem(PlayerInventory, PlayerInventory, Request);
	return true;
}

bool URpgInventoryDragDropCoordinator::CanQuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount) const
{
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!EntryViewModel || !EntryViewModel->CanDrag() ||
		!Actions ||
		IsInteractionRequestPending() ||
		!IsEntryViewCurrent(EntryViewModel))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = EntryViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	if (!Inventory || !ItemInstance || !IsSplittableStackItem(ItemInstance) || EntryViewModel->GetStackCount() <= 1)
	{
		return false;
	}

	int32 ResolvedSplitCount = 0;
	FRpgInventoryGridPlacement ResolvedTargetPlacement;
	return Actions->CanSplitItemStack(
		Inventory,
		ItemInstance,
		SplitCount,
		TargetPlacement,
		ResolvedSplitCount,
		ResolvedTargetPlacement);
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

bool URpgInventoryDragDropCoordinator::CanQuickSplitAddressSlot(
	URpgInventoryAddressSlotViewModel* SlotViewModel,
	FRpgInventoryGridPlacement TargetPlacement,
	int32 SplitCount) const
{
	if (!SlotViewModel || !SlotViewModel->CanDrag() ||
		SlotViewModel->IsGearSlot() || SlotViewModel->IsCarrySlot() ||
		IsInteractionRequestPending())
	{
		return false;
	}

	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	if (!Actions || !Inventory || !ItemInstance || !IsAddressViewCurrent(SlotViewModel) ||
		!IsSplittableStackItem(ItemInstance) || SlotViewModel->GetStackCount() <= 1)
	{
		return false;
	}

	int32 ResolvedSplitCount = 0;
	FRpgInventoryGridPlacement ResolvedTargetPlacement;
	return Actions->CanSplitItemStack(
		Inventory,
		ItemInstance,
		SplitCount,
		TargetPlacement,
		ResolvedSplitCount,
		ResolvedTargetPlacement);
}

bool URpgInventoryDragDropCoordinator::UseOrEquipEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount)
{
	if (!EntryViewModel)
	{
		return false;
	}

	URpgInventoryItemInstance* ItemInstance = EntryViewModel->GetItemInstance();
	if (!ItemInstance)
	{
		return false;
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
	const ERpgInventoryContextAction ContextAction =
		Intent == ERpgInventoryItemActionIntent::Use
			? ERpgInventoryContextAction::Use
			: Intent == ERpgInventoryItemActionIntent::MoveToCarry
				? ERpgInventoryContextAction::MoveToCarry
				: ERpgInventoryContextAction::EquipAndActivate;
	if (!CanExecuteContextAction(EntryViewModel, ContextAction))
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

	const FGuid RequestId = FGuid::NewGuid();
	if (Intent != ERpgInventoryItemActionIntent::Use)
	{
		URpgInventoryManagerComponent* EquipmentInventory = nullptr;
		FRpgInventoryEquipmentIntent EquipmentIntent;
		if (!BuildEquipmentIntent(
				MakeInventoryPayloadFromEntry(EntryViewModel),
				Intent == ERpgInventoryItemActionIntent::MoveToCarry
					? ERpgInventoryEquipmentIntentOperation::
						MoveToCarry
					: ERpgInventoryEquipmentIntentOperation::
						EquipDefaultAndActivate,
				ERpgEquipmentSlot::None,
				RequestId,
				EquipmentInventory,
				EquipmentIntent))
		{
			return false;
		}
		Actions->RequestApplyInventoryEquipmentIntent(
			EquipmentInventory,
			EquipmentIntent);
		return true;
	}

	FRpgInventoryItemActionRequest Request;
	Request.RequestId = RequestId;
	Request.ItemId = Item->GetItemId();
	Request.Intent = Intent;
	Request.StackCount = FMath::Max(1, StackCount);
	Actions->RequestExecuteInventoryItemAction(
		Inventory,
		Request);
	return true;
}

bool URpgInventoryDragDropCoordinator::UseOrEquipAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount)
{
	if (!SlotViewModel)
	{
		return false;
	}

	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	if (!ItemInstance)
	{
		return false;
	}

	if (SlotViewModel->IsGearSlot())
	{
		if (!CanExecuteContextAction(SlotViewModel, ERpgInventoryContextAction::Unequip))
		{
			return false;
		}
		URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
		if (!ActionComponent)
		{
			return false;
		}
		if (HasHeldPayload())
		{
			CancelHold();
		}
		URpgInventoryManagerComponent* Inventory = nullptr;
		FRpgInventoryEquipmentIntent EquipmentIntent;
		if (!BuildEquipmentIntent(
				MakeInventoryPayloadFromAddressSlot(SlotViewModel),
				ERpgInventoryEquipmentIntentOperation::
					UnequipToContent,
				ERpgEquipmentSlot::None,
				FGuid::NewGuid(),
				Inventory,
				EquipmentIntent))
		{
			return false;
		}
		ActionComponent->RequestApplyInventoryEquipmentIntent(
			Inventory,
			EquipmentIntent);
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
	const ERpgInventoryContextAction ContextAction =
		Intent == ERpgInventoryItemActionIntent::Use
			? ERpgInventoryContextAction::Use
			: Intent == ERpgInventoryItemActionIntent::MoveToCarry
				? ERpgInventoryContextAction::MoveToCarry
				: ERpgInventoryContextAction::EquipAndActivate;
	if (!CanExecuteContextAction(SlotViewModel, ContextAction))
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

	const FGuid RequestId = FGuid::NewGuid();
	if (Intent != ERpgInventoryItemActionIntent::Use)
	{
		URpgInventoryManagerComponent* EquipmentInventory = nullptr;
		FRpgInventoryEquipmentIntent EquipmentIntent;
		if (!BuildEquipmentIntent(
				MakeInventoryPayloadFromAddressSlot(SlotViewModel),
				Intent == ERpgInventoryItemActionIntent::MoveToCarry
					? ERpgInventoryEquipmentIntentOperation::
						MoveToCarry
					: ERpgInventoryEquipmentIntentOperation::
						EquipDefaultAndActivate,
				ERpgEquipmentSlot::None,
				RequestId,
				EquipmentInventory,
				EquipmentIntent))
		{
			return false;
		}
		Actions->RequestApplyInventoryEquipmentIntent(
			EquipmentInventory,
			EquipmentIntent);
		return true;
	}

	FRpgInventoryItemActionRequest Request;
	Request.RequestId = RequestId;
	Request.ItemId = Item->GetItemId();
	Request.Intent = Intent;
	Request.StackCount = FMath::Max(1, StackCount);
	Actions->RequestExecuteInventoryItemAction(
		Inventory,
		Request);
	return true;
}

bool URpgInventoryDragDropCoordinator::QuickSplitAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount)
{
	if (!CanQuickSplitAddressSlot(SlotViewModel, TargetPlacement, SplitCount))
	{
		return false;
	}

	URpgInventoryUiActionComponent* ActionComponent = ResolveUiActionComponent();
	URpgInventoryManagerComponent* Inventory = SlotViewModel->GetInventoryManager();
	URpgInventoryItemInstance* ItemInstance = SlotViewModel->GetItemInstance();
	if (!ActionComponent || !Inventory || !ItemInstance)
	{
		return false;
	}

	if (HasHeldPayload())
	{
		CancelHold();
	}

	const int32 RequestedSplitCount =
		SplitCount <= 0 ? SlotViewModel->GetStackCount() / 2 : SplitCount;
	ActionComponent->RequestSplitItemStackById(
		Inventory,
		ItemInstance->GetItemId(),
		RequestedSplitCount,
		TargetPlacement,
		FGuid::NewGuid());
	return true;
}

bool URpgInventoryDragDropCoordinator::RotateEntryInPlace(
	URpgInventoryEntryViewModel* EntryViewModel)
{
	if (!CanExecuteContextAction(
			EntryViewModel,
			ERpgInventoryContextAction::Rotate,
			true))
	{
		return false;
	}

	return DispatchRotateEntryInPlace(
		EntryViewModel->GetInventoryManager(),
		EntryViewModel->GetItemInstance(),
		EntryViewModel->GetEntryId(),
		EntryViewModel->GetPlacement());
}

bool URpgInventoryDragDropCoordinator::RotateAddressSlotInPlace(
	URpgInventoryAddressSlotViewModel* SlotViewModel)
{
	if (!CanExecuteContextAction(
			SlotViewModel,
			ERpgInventoryContextAction::Rotate,
			true))
	{
		return false;
	}

	return DispatchRotateEntryInPlace(
		SlotViewModel->GetInventoryManager(),
		SlotViewModel->GetItemInstance(),
		SlotViewModel->GetEntryId(),
		SlotViewModel->GetItemPlacement());
}

bool URpgInventoryDragDropCoordinator::DropEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount, bool bConfirmed)
{
	if (!CanExecuteContextAction(EntryViewModel, ERpgInventoryContextAction::Drop))
	{
		return false;
	}

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
	if (!CanExecuteContextAction(SlotViewModel, ERpgInventoryContextAction::Drop))
	{
		return false;
	}

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
		OutRequest.ExpectedSourceQuantity != EntryViewModel->GetStackCount() ||
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
		OutRequest.ExpectedSourceQuantity != SlotViewModel->GetStackCount() ||
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
	if (!CanExecuteContextAction(
			EquipmentSlot,
			ExpectedItemId,
			ERpgInventoryContextAction::Unequip))
	{
		return false;
	}

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

	URpgInventoryManagerComponent* Inventory = nullptr;
	FRpgInventoryEquipmentIntent EquipmentIntent;
	if (!BuildEquipmentIntent(
			MakeEquipmentPayload(ItemInstance, EquipmentSlot),
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent,
			ERpgEquipmentSlot::None,
			FGuid::NewGuid(),
			Inventory,
			EquipmentIntent))
	{
		return false;
	}
	ActionComponent->RequestApplyInventoryEquipmentIntent(
		Inventory,
		EquipmentIntent);
	return true;
}

bool URpgInventoryDragDropCoordinator::DropEquipmentItem(
	ERpgEquipmentSlot EquipmentSlot,
	FRpgInventoryItemId ExpectedItemId,
	bool bConfirmed)
{
	if (!CanExecuteContextAction(
			EquipmentSlot,
			ExpectedItemId,
			ERpgInventoryContextAction::Drop))
	{
		return false;
	}

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
	return PlanInteractionPreview(ResolvedPayload, Target).IsAccepted();
}

bool URpgInventoryDragDropCoordinator::UpdateInteractionPreview(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target)
{
	const FRpgInventoryDragPayload ResolvedPayload = ResolveInteractionPayload(Payload);
	const FRpgInventoryInteractionPreviewPlan PreviewPlan =
		PlanInteractionPreview(ResolvedPayload, Target);
	return PublishInteractionPreview(
		ResolvedPayload,
		Target,
		PreviewPlan);
}

bool URpgInventoryDragDropCoordinator::PublishInteractionPreview(
	const FRpgInventoryDragPayload& Payload,
	const FRpgInventoryDropTarget& Target,
	const FRpgInventoryInteractionPreviewPlan& PreviewPlan)
{
	const FRpgInventoryDragPayload ResolvedPayload =
		ResolveInteractionPayload(Payload);
	const ERpgInventoryInteractionPreviewState PreviewState =
		PreviewPlan.State;
	if (InteractionSession && (!InteractionSession->HasPayload() || IsSameInteractionPayload(ResolvedPayload, InteractionSession->GetPayload())))
	{
		if (!InteractionSession->HasPayload() &&
			!InteractionSession->BeginInteraction(ResolvedPayload, ERpgInventoryInteractionInputMode::Mouse))
		{
			return false;
		}
		InteractionSession->SetPreviewTarget(Target, PreviewState);
	}

	return PreviewPlan.IsAccepted();
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
	const FRpgInventoryInteractionPreviewPlan PreviewPlan =
		PlanInteractionPreview(Payload, Target);
	return CommitPlannedPayloadToTarget(
		Payload,
		Target,
		PreviewPlan);
}

bool URpgInventoryDragDropCoordinator::CommitPlannedPayloadToTarget(
	const FRpgInventoryDragPayload& Payload,
	const FRpgInventoryDropTarget& Target,
	const FRpgInventoryInteractionPreviewPlan& PreviewPlan)
{
	if (!PreviewPlan.IsAccepted() || IsInteractionRequestPending())
	{
		return false;
	}

	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!Actions)
	{
		return false;
	}
	auto SubmitExactPlacementMutation =
		[this, Actions, &Payload, &Target, &PreviewPlan](
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryGridPlacement& SourcePlacement)
	{
		if (!Inventory || !Payload.ItemInstance || !SourcePlacement.IsValid() || !Target.TargetPlacement.IsValid())
		{
			return false;
		}

		FRpgInventoryMoveIntent Intent;
		Intent.RequestId =
			MarkInteractionRequestPending(
				Payload,
				Target,
				PreviewPlan.State);
		Intent.ItemId = Payload.ItemInstance->GetItemId();
		Intent.ExpectedEntryId = Payload.EntryId;
		Intent.ExpectedSourcePlacement =
			SourcePlacement;
		Intent.ExpectedQuantity = Payload.StackCount;
		Intent.TargetPlacement = Target.TargetPlacement;
		if (!Intent.RequestId.IsValid())
		{
			return false;
		}
		Actions->RequestMoveInventoryItem(
			Inventory,
			Intent);
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
					Payload.SourcePlacement);
			}

			if (Target.TargetType == ERpgInventoryDropTargetType::InventorySlot)
			{
				FRpgInventoryTransferIntent Intent;
				Intent.RequestId =
					MarkInteractionRequestPending(
						Payload,
						Target,
						PreviewPlan.State);
				Intent.ItemId = Payload.ItemInstance->GetItemId();
				Intent.ExpectedEntryId = Payload.EntryId;
				Intent.ExpectedSourcePlacement =
					Payload.SourcePlacement;
				Intent.ExpectedSourceQuantity = Payload.StackCount;
				Intent.TargetContainer =
					Target.TargetPlacement.GetContainerHandle();
				Intent.TargetPlacement = Target.TargetPlacement;
				Intent.Quantity = Payload.StackCount;
				if (!Intent.RequestId.IsValid())
				{
					return false;
				}
				Actions->RequestTransferInventoryItem(
					Payload.SourceInventory,
					Target.TargetInventory,
					Intent);
				return true;
			}

			FRpgInventoryQuickTransferRequest Request;
			Request.RequestId =
				MarkInteractionRequestPending(
					Payload,
					Target,
					PreviewPlan.State);
			Request.ItemId = Payload.ItemInstance->GetItemId();
			Request.ExpectedEntryId = Payload.EntryId;
			Request.ExpectedSourcePlacement =
				Payload.SourcePlacement;
			Request.ExpectedSourceQuantity = Payload.StackCount;
			Request.StackCount = Payload.StackCount;
			if (!Request.RequestId.IsValid())
			{
				return false;
			}
			Actions->RequestQuickTransferItem(
				Payload.SourceInventory,
				Target.TargetInventory,
				Request);
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
			return PlayerInventory &&
				SubmitExactPlacementMutation(
					PlayerInventory,
					Payload.SourcePlacement);
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
				FRpgInventoryTransferIntent Intent;
				Intent.RequestId =
					MarkInteractionRequestPending(
						Payload,
						Target,
						PreviewPlan.State);
				Intent.ItemId = Payload.ItemInstance->GetItemId();
				Intent.ExpectedEntryId = Payload.EntryId;
				Intent.ExpectedSourcePlacement =
					Payload.SourcePlacement;
				Intent.ExpectedSourceQuantity = Payload.StackCount;
				Intent.TargetContainer =
					Target.TargetPlacement.GetContainerHandle();
				Intent.TargetPlacement =
					Target.TargetPlacement;
				Intent.Quantity = Payload.StackCount;
				if (!Intent.RequestId.IsValid())
				{
					return false;
				}
				Actions->RequestTransferInventoryItem(
					Payload.SourceInventory,
					PlayerInventory,
					Intent);
				return true;
			}

			return PlayerInventory &&
				SubmitExactPlacementMutation(
					PlayerInventory,
					Payload.SourcePlacement);
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
		const bool bSourceIsCarry =
			InventoryLayout->IsCarrySlotAddress(SourceAddress);
		const FRpgInventoryContainerHandle SourceContainer =
			SourceAddress.GetContainerHandle();
		if (bSourceIsCarry &&
			!SourceContainer.IsRoot())
		{
			return false;
		}

		FRpgQuickAccessMutationRequest Request;
		Request.RequestId = MarkInteractionRequestPending(
			Payload,
			Target,
			PreviewPlan.State);
		Request.SlotIndex = Target.ActionBarSlotIndex;
		Request.SourceAddress = SourceAddress;
		Request.ContextItemId = Payload.ItemInstance->GetItemId();
		if (!Request.RequestId.IsValid())
		{
			return false;
		}

		if (bSourceIsCarry)
		{
			Request.Operation = ERpgQuickAccessMutationOperation::BindCarry;
			Request.ExpectedCarryRole = SourceContainer.Root;
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

			const FGuid RequestId =
				MarkInteractionRequestPending(
					Payload,
					Target,
					PreviewPlan.State);
			URpgInventoryManagerComponent* EquipmentInventory =
				nullptr;
			FRpgInventoryEquipmentIntent EquipmentIntent;
			if (!BuildEquipmentIntent(
					Payload,
					ERpgInventoryEquipmentIntentOperation::
						EquipToSlot,
					Target.EquipmentSlot,
					RequestId,
					EquipmentInventory,
					EquipmentIntent))
			{
				if (InteractionSession)
				{
					InteractionSession->RejectRequestLocally();
				}
				return false;
			}
			Actions->RequestApplyInventoryEquipmentIntent(
				EquipmentInventory,
				EquipmentIntent);
			return true;
		}
	}

	if (Target.TargetType == ERpgInventoryDropTargetType::ClearSlot)
	{
		if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
		{
			const FGuid RequestId =
				MarkInteractionRequestPending(
					Payload,
					Target,
					PreviewPlan.State);
			URpgInventoryManagerComponent* EquipmentInventory =
				nullptr;
			FRpgInventoryEquipmentIntent EquipmentIntent;
			const bool bClearsActiveSelection =
				FRpgInventoryEquipmentPlacementPolicy::
					IsHandEquipmentSlot(Payload.EquipmentSlot);
			if (!BuildEquipmentIntent(
					Payload,
					bClearsActiveSelection
						? ERpgInventoryEquipmentIntentOperation::
							ClearActiveSelection
						: ERpgInventoryEquipmentIntentOperation::
							UnequipToContent,
					bClearsActiveSelection
						? Payload.EquipmentSlot
						: ERpgEquipmentSlot::None,
					RequestId,
					EquipmentInventory,
					EquipmentIntent))
			{
				if (InteractionSession)
				{
					InteractionSession->RejectRequestLocally();
				}
				return false;
			}
			Actions->RequestApplyInventoryEquipmentIntent(
				EquipmentInventory,
				EquipmentIntent);
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
	return PlanInteractionPreview(Payload, Target).State;
}

FRpgInventoryInteractionPreviewPlan
URpgInventoryDragDropCoordinator::PlanInteractionPreview(
	const FRpgInventoryDragPayload& Payload,
	const FRpgInventoryDropTarget& Target) const
{
	if (InteractionSession && InteractionSession->IsRequestPending())
	{
		return MakeSimpleInteractionPreview(
			ERpgInventoryInteractionPreviewState::Pending);
	}

	const bool bTargetPlacementOutOfBounds =
		IsTargetPlacementOutOfBounds(Target);
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!IsPayloadValid(Payload) || !IsTargetValid(Target) ||
		!Actions || !IsPayloadSourceCurrent(Payload))
	{
		return MakeSimpleInteractionPreview(
			bTargetPlacementOutOfBounds
				? ERpgInventoryInteractionPreviewState::OutOfBounds
				: ERpgInventoryInteractionPreviewState::Blocked);
	}

	auto ProjectPlan =
		[bTargetPlacementOutOfBounds](
			FRpgInventoryPlacementPlan Plan,
			ERpgInventoryInteractionPreviewState PlacementState,
			bool bAcceptNoOp = false)
	{
		FRpgInventoryInteractionPreviewPlan Result =
			ProjectPlacementPreview(
				MoveTemp(Plan),
				PlacementState,
				bAcceptNoOp);
		if (!Result.IsAccepted() && bTargetPlacementOutOfBounds)
		{
			Result.State =
				ERpgInventoryInteractionPreviewState::OutOfBounds;
		}
		return Result;
	};

	auto PlanOwnedExactPlacement =
		[this, Actions, &Payload, &ProjectPlan](
			URpgInventoryManagerComponent* Inventory,
			const FRpgInventoryGridPlacement& TargetPlacement)
	{
		if (!Inventory || Payload.SourceInventory != Inventory ||
			!TargetPlacement.IsValid() ||
			!Actions->CanAccessInventory(Inventory))
		{
			return MakeSimpleInteractionPreview(
				ERpgInventoryInteractionPreviewState::Blocked);
		}

		FRpgInventoryEntryView SourceEntry;
		if (!TryGetExactPayloadEntry(Payload, SourceEntry))
		{
			return MakeSimpleInteractionPreview(
				ERpgInventoryInteractionPreviewState::Blocked);
		}

		const URpgPlayerInventoryLayoutComponent* InventoryLayout =
			FindPlayerInventoryLayout();
		const bool bPreservesEquipmentIdentity =
			Inventory == FindPlayerInventory() &&
			(IsEquipmentPlacement(
				InventoryLayout,
				SourceEntry.Placement) ||
			 IsEquipmentPlacement(
				InventoryLayout,
				TargetPlacement));
		FRpgInventoryPlacementQuery Query;
		Query.Purpose = bPreservesEquipmentIdentity
			? ERpgInventoryPlacementPurpose::Equip
			: ERpgInventoryPlacementPurpose::Move;
		Query.Search = ERpgInventoryPlacementSearch::Exact;
		Query.Subject =
			FRpgInventoryPlacementSubject::FromOwnedEntry(
				Inventory,
				SourceEntry,
				SourceEntry.StackCount);
		Query.TargetContainer = TargetPlacement.GetContainerHandle();
		Query.ExactPlacement = TargetPlacement;
		return ProjectPlan(
			Inventory->EvaluatePlacement(Query),
			ERpgInventoryInteractionPreviewState::Move);
	};

	switch (Target.TargetType)
	{
	case ERpgInventoryDropTargetType::EquipmentSlot:
	{
		if ((Payload.SourceType !=
				 ERpgInventoryDragSourceType::InventoryEntry &&
			 Payload.SourceType !=
				 ERpgInventoryDragSourceType::PlayerInventorySlotAddress &&
			 Payload.SourceType !=
				 ERpgInventoryDragSourceType::EquipmentSlot) ||
			(Payload.SourceType ==
				 ERpgInventoryDragSourceType::EquipmentSlot &&
			 Payload.EquipmentSlot == Target.EquipmentSlot))
		{
			return MakeSimpleInteractionPreview(
				ERpgInventoryInteractionPreviewState::Blocked);
		}

		FRpgInventoryEntryView SourceEntry;
		if (!TryGetExactPayloadEntry(Payload, SourceEntry))
		{
			return MakeSimpleInteractionPreview(
				ERpgInventoryInteractionPreviewState::Blocked);
		}
		FRpgInventoryEquipmentIntent Intent;
		Intent.ItemId = SourceEntry.ItemId;
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedQuantity = SourceEntry.StackCount;
		Intent.Operation =
			ERpgInventoryEquipmentIntentOperation::EquipToSlot;
		Intent.TargetEquipmentSlot = Target.EquipmentSlot;
		FRpgInventoryGridPlacement ResolvedPlacement;
		return ProjectPlan(
			Actions->PlanEquipmentIntentPlacement(
				Payload.SourceInventory,
				Intent,
				ResolvedPlacement),
			ERpgInventoryInteractionPreviewState::Equip,
			true);
	}

	case ERpgInventoryDropTargetType::ActionBarSlot:
	{
		if ((Payload.SourceType !=
				 ERpgInventoryDragSourceType::InventoryEntry &&
			 Payload.SourceType !=
				 ERpgInventoryDragSourceType::PlayerInventorySlotAddress &&
			 Payload.SourceType !=
				 ERpgInventoryDragSourceType::EquipmentSlot) ||
			Target.ActionBarSlotIndex < 0 ||
			(Payload.SourceType !=
				 ERpgInventoryDragSourceType::EquipmentSlot &&
			 !IsPlayerInventory(Payload.SourceInventory)))
		{
			return MakeSimpleInteractionPreview(
				ERpgInventoryInteractionPreviewState::Blocked);
		}

		const FRpgInventorySlotAddress SourceAddress =
			ResolvePayloadSourceAddress(Payload);
		const URpgPlayerInventoryLayoutComponent* InventoryLayout =
			FindPlayerInventoryLayout();
		return MakeSimpleInteractionPreview(
			InventoryLayout && SourceAddress.IsValid() &&
				InventoryLayout->CanBindSlotAddressToActionbar(
					SourceAddress,
					Payload.ItemInstance)
				? ERpgInventoryInteractionPreviewState::Bind
				: ERpgInventoryInteractionPreviewState::Blocked);
	}

	case ERpgInventoryDropTargetType::ClearSlot:
	{
		FRpgInventoryEntryView SourceEntry;
		if (Payload.SourceType !=
				ERpgInventoryDragSourceType::EquipmentSlot ||
			!TryGetExactPayloadEntry(Payload, SourceEntry))
		{
			return MakeSimpleInteractionPreview(
				ERpgInventoryInteractionPreviewState::Blocked);
		}

		if (FRpgInventoryEquipmentPlacementPolicy::
				IsHandEquipmentSlot(Payload.EquipmentSlot))
		{
			return MakeSimpleInteractionPreview(
				ResolveCurrentEquipmentItem(
					Payload.EquipmentSlot,
					SourceEntry.ItemId) == SourceEntry.Instance
					? ERpgInventoryInteractionPreviewState::Clear
					: ERpgInventoryInteractionPreviewState::Blocked);
		}

		FRpgInventoryEquipmentIntent Intent;
		Intent.ItemId = SourceEntry.ItemId;
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedQuantity = SourceEntry.StackCount;
		Intent.Operation =
			ERpgInventoryEquipmentIntentOperation::UnequipToContent;
		FRpgInventoryGridPlacement ResolvedPlacement;
		return ProjectPlan(
			Actions->PlanEquipmentIntentPlacement(
				Payload.SourceInventory,
				Intent,
				ResolvedPlacement),
			ERpgInventoryInteractionPreviewState::Clear);
	}

	case ERpgInventoryDropTargetType::InventorySlot:
	case ERpgInventoryDropTargetType::InventoryPanel:
	{
		if (Payload.SourceType ==
			ERpgInventoryDragSourceType::InventoryEntry)
		{
			if (!Payload.SourceInventory || !Target.TargetInventory ||
				!Payload.ItemInstance)
			{
				return MakeSimpleInteractionPreview(
					ERpgInventoryInteractionPreviewState::Blocked);
			}

			if (Payload.SourceInventory == Target.TargetInventory)
			{
				return Target.TargetType ==
						ERpgInventoryDropTargetType::InventorySlot
					? PlanOwnedExactPlacement(
						Payload.SourceInventory,
						Target.TargetPlacement)
					: MakeSimpleInteractionPreview(
						ERpgInventoryInteractionPreviewState::Blocked);
			}

			if (Target.TargetType ==
				ERpgInventoryDropTargetType::InventorySlot)
			{
				FRpgInventoryTransferIntent Intent;
				Intent.ItemId = Payload.ItemInstance->GetItemId();
				Intent.ExpectedEntryId = Payload.EntryId;
				Intent.ExpectedSourcePlacement =
					Payload.SourcePlacement;
				Intent.ExpectedSourceQuantity = Payload.StackCount;
				Intent.TargetContainer =
					Target.TargetPlacement.GetContainerHandle();
				Intent.TargetPlacement = Target.TargetPlacement;
				Intent.Quantity = Payload.StackCount;
				return ProjectPlan(
					Actions->PlanExactTransferPlacement(
						Payload.SourceInventory,
						Target.TargetInventory,
						Intent),
					ERpgInventoryInteractionPreviewState::Move);
			}

			FRpgInventoryQuickTransferRequest Request;
			Request.ItemId = Payload.ItemInstance->GetItemId();
			Request.ExpectedEntryId = Payload.EntryId;
			Request.ExpectedSourcePlacement = Payload.SourcePlacement;
			Request.ExpectedSourceQuantity = Payload.StackCount;
			Request.StackCount = Payload.StackCount;
			FRpgInventoryContainerHandle ResolvedContainer;
			FRpgInventoryGridPlacement ResolvedPlacement;
			return ProjectPlan(
				Actions->PlanQuickTransferDestination(
					Payload.SourceInventory,
					Target.TargetInventory,
					Request,
					ResolvedContainer,
					ResolvedPlacement),
				ERpgInventoryInteractionPreviewState::Move);
		}

		if (Payload.SourceType ==
			ERpgInventoryDragSourceType::EquipmentSlot)
		{
			URpgInventoryManagerComponent* PlayerInventory =
				FindPlayerInventory();
			if (Target.TargetType !=
					ERpgInventoryDropTargetType::InventorySlot ||
				Target.TargetInventory != PlayerInventory)
			{
				return MakeSimpleInteractionPreview(
					ERpgInventoryInteractionPreviewState::Blocked);
			}
			return PlanOwnedExactPlacement(
				PlayerInventory,
				Target.TargetPlacement);
		}

		return MakeSimpleInteractionPreview(
			ERpgInventoryInteractionPreviewState::Blocked);
	}

	case ERpgInventoryDropTargetType::PlayerInventorySlotAddress:
	{
		if ((Payload.SourceType !=
				 ERpgInventoryDragSourceType::InventoryEntry &&
			 Payload.SourceType !=
				 ERpgInventoryDragSourceType::EquipmentSlot) ||
			!Payload.ItemInstance || !Target.SlotAddress.IsValid() ||
			!Target.TargetPlacement.IsValid())
		{
			return MakeSimpleInteractionPreview(
				bTargetPlacementOutOfBounds
					? ERpgInventoryInteractionPreviewState::OutOfBounds
					: ERpgInventoryInteractionPreviewState::Blocked);
		}

		URpgInventoryManagerComponent* PlayerInventory =
			FindPlayerInventory();
		const URpgPlayerInventoryLayoutComponent* InventoryLayout =
			FindPlayerInventoryLayout();
		if (!PlayerInventory || Target.TargetInventory != PlayerInventory ||
			!InventoryLayout ||
			!InventoryLayout->CanItemUseSlotAddress(
				Payload.ItemInstance,
				Target.SlotAddress))
		{
			return MakeSimpleInteractionPreview(
				ERpgInventoryInteractionPreviewState::Blocked);
		}

		const FRpgInventorySlotAddress SourceAddress =
			ResolvePayloadSourceAddress(Payload);
		if (Payload.SourceType ==
			ERpgInventoryDragSourceType::EquipmentSlot)
		{
			if (!SourceAddress.IsValid() ||
				!InventoryLayout->IsContentSlotAddress(
					Target.SlotAddress))
			{
				return MakeSimpleInteractionPreview(
					ERpgInventoryInteractionPreviewState::Blocked);
			}

			ERpgEquipmentSlot SourceEquipmentSlot =
				ERpgEquipmentSlot::None;
			const bool bSourceIsSlotContainer =
				InventoryLayout->IsGearSlotAddress(SourceAddress) &&
				URpgPlayerInventoryLayoutComponent::
					TryGetEquipmentSlotForGearContainer(
						SourceAddress.GetContainerHandle(),
						SourceEquipmentSlot) &&
				URpgPlayerInventoryLayoutComponent::
					IsSlotContainerEquipmentSlot(
						SourceEquipmentSlot);
			if (bSourceIsSlotContainer &&
				!InventoryLayout->CanUnequipSlotContainer(
					SourceEquipmentSlot))
			{
				return MakeSimpleInteractionPreview(
					ERpgInventoryInteractionPreviewState::Blocked);
			}

			if (bSourceIsSlotContainer)
			{
				bool bTargetIsStaticContent = false;
				for (const FRpgInventorySlotGroupView& Group :
					 InventoryLayout->GetSlotGroups())
				{
					if (Group.ContainerHandle.IsValid() &&
						Group.ContainerHandle ==
							Target.SlotAddress.GetContainerHandle() &&
						Group.ContainsCell(
							Target.SlotAddress.X,
							Target.SlotAddress.Y))
					{
						bTargetIsStaticContent =
							Group.GroupKind ==
								ERpgInventorySlotGroupKind::Content &&
							!Group.bProvidedByEquipment;
						break;
					}
				}
				if (!bTargetIsStaticContent)
				{
					return MakeSimpleInteractionPreview(
						ERpgInventoryInteractionPreviewState::Blocked);
				}
			}
		}

		if (Payload.SourceInventory == PlayerInventory)
		{
			return PlanOwnedExactPlacement(
				PlayerInventory,
				Target.TargetPlacement);
		}

		FRpgInventoryTransferIntent Intent;
		Intent.ItemId = Payload.ItemInstance->GetItemId();
		Intent.ExpectedEntryId = Payload.EntryId;
		Intent.ExpectedSourcePlacement = Payload.SourcePlacement;
		Intent.ExpectedSourceQuantity = Payload.StackCount;
		Intent.TargetContainer =
			Target.TargetPlacement.GetContainerHandle();
		Intent.TargetPlacement = Target.TargetPlacement;
		Intent.Quantity = Payload.StackCount;
		return ProjectPlan(
			Actions->PlanExactTransferPlacement(
				Payload.SourceInventory,
				PlayerInventory,
				Intent),
			ERpgInventoryInteractionPreviewState::Move);
	}

	default:
		return MakeSimpleInteractionPreview(
			ERpgInventoryInteractionPreviewState::Blocked);
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
	const FRpgInventoryDropTarget& Target,
	ERpgInventoryInteractionPreviewState AcceptedPreviewState)
{
	EnsureInteractionSession();
	if (!InteractionSession || InteractionSession->IsRequestPending())
	{
		return FGuid();
	}

	if (!InteractionSession->HasPayload() || !IsSameInteractionPayload(Payload, InteractionSession->GetPayload()))
	{
		if (!InteractionSession->BeginInteraction(
				Payload,
				ERpgInventoryInteractionInputMode::Mouse))
		{
			return FGuid();
		}
	}
	InteractionSession->SetPreviewTarget(Target, AcceptedPreviewState);
	InteractionSession->MarkRequestPending(Target, ResolveActionTagForTarget(Target));
	return InteractionSession->IsRequestPending()
		? InteractionSession->GetRequestId()
		: FGuid();
}

bool URpgInventoryDragDropCoordinator::CanCommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const
{
	return PlanInteractionPreview(Payload, Target).IsAccepted();
}

bool URpgInventoryDragDropCoordinator::IsPayloadSourceCurrent(
	const FRpgInventoryDragPayload& Payload) const
{
	if (!IsPayloadValid(Payload) || !Payload.ItemInstance ||
		!Payload.ItemInstance->GetItemId().IsValid())
	{
		return false;
	}

	if (Payload.SourceType == ERpgInventoryDragSourceType::InventoryEntry)
	{
		if (!Payload.SourceInventory || !Payload.EntryId.IsValid() ||
			!Payload.SourcePlacement.IsValid() || Payload.StackCount <= 0)
		{
			return false;
		}

		bool bEntryMatches = false;
		for (const FRpgInventoryEntryView& Entry : Payload.SourceInventory->GetAllEntries())
		{
			if (Entry.EntryId == Payload.EntryId)
			{
				bEntryMatches =
					Entry.Instance == Payload.ItemInstance &&
					Entry.ItemId == Payload.ItemInstance->GetItemId() &&
					Entry.StackCount == Payload.StackCount &&
					IsExactPlacementSnapshot(
						Entry.Placement,
						Payload.SourcePlacement);
				break;
			}
		}
		if (!bEntryMatches)
		{
			return false;
		}

		if (Payload.SourceSlotAddress.IsValid())
		{
			const URpgPlayerInventoryLayoutComponent* InventoryLayout =
				FindPlayerInventoryLayout();
			return IsPlayerInventory(Payload.SourceInventory) &&
				InventoryLayout &&
				InventoryLayout->GetItemInSlotAddress(
					Payload.SourceSlotAddress) == Payload.ItemInstance;
		}
		return true;
	}

	if (Payload.SourceType == ERpgInventoryDragSourceType::EquipmentSlot)
	{
		if (ResolveCurrentEquipmentItem(
				Payload.EquipmentSlot,
				Payload.ItemInstance->GetItemId()) !=
			Payload.ItemInstance)
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry :
			Payload.SourceInventory->GetAllEntries())
		{
			if (Entry.EntryId == Payload.EntryId)
			{
				return Entry.Instance == Payload.ItemInstance &&
					Entry.ItemId ==
						Payload.ItemInstance->GetItemId() &&
					Entry.StackCount == Payload.StackCount &&
					IsExactPlacementSnapshot(
						Entry.Placement,
						Payload.SourcePlacement);
			}
		}
		return false;
	}

	if (Payload.SourceType ==
		ERpgInventoryDragSourceType::PlayerInventorySlotAddress)
	{
		const URpgPlayerInventoryLayoutComponent* InventoryLayout =
			FindPlayerInventoryLayout();
		return IsPlayerInventory(Payload.SourceInventory) &&
			InventoryLayout &&
			Payload.SourceSlotAddress.IsValid() &&
			InventoryLayout->GetItemInSlotAddress(
				Payload.SourceSlotAddress) == Payload.ItemInstance;
	}

	return false;
}

bool URpgInventoryDragDropCoordinator::IsHeldSourceEntry(URpgInventoryEntryViewModel* EntryViewModel) const
{
	const FRpgInventoryDragPayload HeldPayload = GetHeldPayload();
	if (!HasHeldPayload() ||
		HeldPayload.SourceType != ERpgInventoryDragSourceType::InventoryEntry ||
		!IsPayloadSourceCurrent(HeldPayload) ||
		!IsEntryViewCurrent(EntryViewModel))
	{
		return false;
	}

	return HeldPayload.SourceInventory == EntryViewModel->GetInventoryManager() &&
		HeldPayload.ItemInstance == EntryViewModel->GetItemInstance() &&
		HeldPayload.ItemInstance->GetItemId() ==
			EntryViewModel->GetItemInstance()->GetItemId() &&
		HeldPayload.EntryId == EntryViewModel->GetEntryId() &&
		HeldPayload.StackCount == EntryViewModel->GetStackCount() &&
		HeldPayload.SourcePlacement == EntryViewModel->GetPlacement();
}

bool URpgInventoryDragDropCoordinator::IsHeldSourceAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel) const
{
	const FRpgInventoryDragPayload HeldPayload = GetHeldPayload();
	if (!HasHeldPayload() ||
		(HeldPayload.SourceType != ERpgInventoryDragSourceType::InventoryEntry &&
			HeldPayload.SourceType != ERpgInventoryDragSourceType::PlayerInventorySlotAddress &&
			HeldPayload.SourceType != ERpgInventoryDragSourceType::EquipmentSlot) ||
		!IsPayloadSourceCurrent(HeldPayload) ||
		!IsAddressViewCurrent(SlotViewModel))
	{
		return false;
	}

	const FRpgInventorySlotAddress HeldAddress = ResolvePayloadSourceAddress(HeldPayload);
	if (!HeldAddress.IsValid() ||
		HeldAddress != SlotViewModel->GetSlotAddress() ||
		HeldPayload.ItemInstance != SlotViewModel->GetItemInstance() ||
		HeldPayload.ItemInstance->GetItemId() !=
			SlotViewModel->GetItemInstance()->GetItemId())
	{
		return false;
	}

	if (HeldPayload.SourceType == ERpgInventoryDragSourceType::InventoryEntry)
	{
		return HeldPayload.SourceInventory ==
				SlotViewModel->GetInventoryManager() &&
			HeldPayload.EntryId == SlotViewModel->GetEntryId() &&
			HeldPayload.StackCount == SlotViewModel->GetStackCount() &&
			HeldPayload.SourcePlacement ==
				SlotViewModel->GetItemPlacement();
	}

	return true;
}

bool URpgInventoryDragDropCoordinator::CanRotateEntryInPlace(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* ItemInstance,
	const FGuid& EntryId,
	const FRpgInventoryGridPlacement& SourcePlacement) const
{
	const URpgInventoryFragment_SpatialItem* SpatialFragment = ItemInstance
		? ItemInstance->FindFragmentByClass<URpgInventoryFragment_SpatialItem>()
		: nullptr;
	if (!Inventory || !ItemInstance || !EntryId.IsValid() ||
		!SourcePlacement.IsValid() || !SpatialFragment ||
		!SpatialFragment->bAllowRotation)
	{
		return false;
	}

	FRpgInventoryMoveIntent Intent;
	Intent.ItemId = ItemInstance->GetItemId();
	Intent.ExpectedEntryId = EntryId;
	Intent.ExpectedSourcePlacement = SourcePlacement;
	Intent.ExpectedQuantity =
		Inventory->GetItemStackCount(ItemInstance);
	Intent.TargetPlacement = SourcePlacement;
	Intent.TargetPlacement.bRotated = !SourcePlacement.bRotated;
	return Inventory->GetItemStackCount(ItemInstance) > 0 &&
		Inventory->PlanMoveItem(Intent).IsSuccess();
}

bool URpgInventoryDragDropCoordinator::DispatchRotateEntryInPlace(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* ItemInstance,
	const FGuid& EntryId,
	const FRpgInventoryGridPlacement& SourcePlacement)
{
	URpgInventoryUiActionComponent* Actions = ResolveUiActionComponent();
	if (!Actions || IsInteractionRequestPending() || HasHeldPayload() ||
		!CanRotateEntryInPlace(
			Inventory,
			ItemInstance,
			EntryId,
			SourcePlacement))
	{
		return false;
	}

	FRpgInventoryMoveIntent Intent;
	Intent.RequestId = FGuid::NewGuid();
	Intent.ItemId = ItemInstance->GetItemId();
	Intent.ExpectedEntryId = EntryId;
	Intent.ExpectedSourcePlacement = SourcePlacement;
	Intent.ExpectedQuantity =
		Inventory->GetItemStackCount(ItemInstance);
	Intent.TargetPlacement = SourcePlacement;
	Intent.TargetPlacement.bRotated = !SourcePlacement.bRotated;
	Actions->RequestMoveInventoryItem(Inventory, Intent);
	return true;
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

	// Main-/Offhand widgets represent the active loadout assignment, not every compatible holstered Carry item.
	const APlayerController* Controller = PlayerController.Get();
	const ARpgPlayerController* RpgPlayerController =
		Cast<ARpgPlayerController>(Controller);
	const URpgEquipmentLoadoutComponent* EquipmentLoadout =
		RpgPlayerController
			? RpgPlayerController->GetEquipmentLoadoutComponent()
			: (Controller
				? Controller->FindComponentByClass<URpgEquipmentLoadoutComponent>()
				: nullptr);
	const FRpgInventorySlotAddress PhysicalSourceAddress =
		ResolveEquipmentPayloadSourceAddress(
			MakeEquipmentPayload(ItemInstance, EquipmentSlot));
	return EquipmentLoadout &&
		EquipmentLoadout->GetItemInEquipmentSlot(EquipmentSlot) ==
			ItemInstance &&
		PhysicalSourceAddress.IsValid() &&
		InventoryLayout->IsCarrySlotAddress(PhysicalSourceAddress)
		? ItemInstance
		: nullptr;
}

bool URpgInventoryDragDropCoordinator::BuildEquipmentIntent(
	const FRpgInventoryDragPayload& Payload,
	ERpgInventoryEquipmentIntentOperation Operation,
	ERpgEquipmentSlot TargetEquipmentSlot,
	const FGuid& RequestId,
	URpgInventoryManagerComponent*& OutInventory,
	FRpgInventoryEquipmentIntent& OutIntent) const
{
	OutInventory = nullptr;
	OutIntent = FRpgInventoryEquipmentIntent();
	URpgInventoryManagerComponent* Inventory =
		Payload.SourceInventory.Get();
	URpgInventoryItemInstance* Item = Payload.ItemInstance;
	if (!Inventory || !Item || !RequestId.IsValid() ||
		!Item->GetItemId().IsValid() ||
		!Payload.EntryId.IsValid() ||
		!Payload.SourcePlacement.IsValid() ||
		Payload.StackCount <= 0 ||
		!Inventory->ContainsItemInstance(Item))
	{
		return false;
	}

	for (const FRpgInventoryEntryView& Entry :
		Inventory->GetAllEntries())
	{
		if (Entry.Instance != Item ||
			Entry.ItemId != Item->GetItemId() ||
			!Entry.EntryId.IsValid() ||
			!Entry.Placement.IsValid() ||
			Entry.StackCount <= 0)
		{
			continue;
		}

		if (Payload.EntryId != Entry.EntryId ||
				!IsExactPlacementSnapshot(
					Payload.SourcePlacement,
					Entry.Placement) ||
				Payload.StackCount != Entry.StackCount)
		{
			return false;
		}

		OutIntent.RequestId = RequestId;
		OutIntent.ItemId = Entry.ItemId;
		OutIntent.ExpectedEntryId = Entry.EntryId;
		OutIntent.ExpectedSourcePlacement = Entry.Placement;
		OutIntent.ExpectedQuantity = Entry.StackCount;
		OutIntent.Operation = Operation;
		OutIntent.TargetEquipmentSlot =
			TargetEquipmentSlot;
		OutInventory = Inventory;
		return true;
	}

	return false;
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
		OutRequest.ExpectedSourceQuantity = Entry.StackCount;
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
		Request.ExpectedSourceQuantity <= 0 || Request.StackCount <= 0 ||
		Request.StackCount > Request.ExpectedSourceQuantity)
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
				Entry.StackCount == Request.ExpectedSourceQuantity;
		}
	}

	return false;
}

bool URpgInventoryDragDropCoordinator::CanMoveItemOutOfAddress(
	const FRpgInventorySlotAddress& SourceAddress) const
{
	if (!SourceAddress.IsValid())
	{
		return false;
	}

	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
	if (!URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearContainer(
			SourceAddress.GetContainerHandle(),
			EquipmentSlot) ||
		!URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(EquipmentSlot))
	{
		return true;
	}

	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	return InventoryLayout &&
		InventoryLayout->CanUnequipSlotContainer(EquipmentSlot);
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
			return Group.ContainerHandle == FRpgInventoryContainerHandle::MakeRoot(
				URpgPlayerInventoryLayoutComponent::PocketsGroupId);
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
