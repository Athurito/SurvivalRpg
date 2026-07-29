#include "RpgInventorySpatialItemWidget.h"

#include "RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropOperation.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryEntryViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryAddressSlotViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryItemTooltipWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySpatialItemWidget)

URpgInventorySpatialItemWidget::URpgInventorySpatialItemWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	ItemTooltipWidgetClass = URpgInventoryItemTooltipWidget::StaticClass();
}

void URpgInventorySpatialItemWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshPlacedItemVisual();
}

void URpgInventorySpatialItemWidget::NativeDestruct()
{
	ReleaseSpatialItemState();
	Super::NativeDestruct();
}

void URpgInventorySpatialItemWidget::SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid)
{
	if (InOwningGrid)
	{
		bSpatialItemStateReleased = false;
	}

	OwningGrid = InOwningGrid;
	RefreshPlacedItemVisual();
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (InCoordinator)
	{
		bSpatialItemStateReleased = false;
	}

	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	DragDropCoordinator = InCoordinator;
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetAddressSlotViewModel(URpgInventoryAddressSlotViewModel* InSlotViewModel)
{
	if (InSlotViewModel)
	{
		bSpatialItemStateReleased = false;
	}

	const bool bClearedEntryViewModel = EntryViewModel != nullptr;
	if (AddressSlotViewModel)
	{
		AddressSlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleAddressSlotChanged);
	}
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryChanged);
	}

	AddressSlotViewModel = InSlotViewModel;
	EntryViewModel = nullptr;
	if (AddressSlotViewModel)
	{
		AddressSlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleAddressSlotChanged);
	}

	RefreshPlacedItemVisual();
	if (bClearedEntryViewModel)
	{
		BP_OnSpatialEntryItemSet(nullptr);
	}
	BP_OnSpatialAddressItemSet(AddressSlotViewModel);
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetEntryViewModel(URpgInventoryEntryViewModel* InEntryViewModel)
{
	if (InEntryViewModel)
	{
		bSpatialItemStateReleased = false;
	}

	const bool bClearedAddressSlotViewModel = AddressSlotViewModel != nullptr;
	if (AddressSlotViewModel)
	{
		AddressSlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleAddressSlotChanged);
	}
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryChanged);
	}

	AddressSlotViewModel = nullptr;
	EntryViewModel = InEntryViewModel;
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.AddUniqueDynamic(this, &ThisClass::HandleEntryChanged);
	}

	RefreshPlacedItemVisual();
	if (bClearedAddressSlotViewModel)
	{
		BP_OnSpatialAddressItemSet(nullptr);
	}
	BP_OnSpatialEntryItemSet(EntryViewModel);
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::ReleaseSpatialItemState()
{
	if (bSpatialItemStateReleased)
	{
		return;
	}
	bSpatialItemStateReleased = true;
	StopAllAnimations();

	if (AddressSlotViewModel)
	{
		AddressSlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleAddressSlotChanged);
	}
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryChanged);
	}
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	AddressSlotViewModel = nullptr;
	EntryViewModel = nullptr;
	DragDropCoordinator = nullptr;
	OwningGrid = nullptr;
	bInventoryPanelActive = true;
	bPendingLeftClickAccept = false;
	PendingPointerDragAnchor = FRpgInventoryDragAnchor();
	bHasPendingPointerDragAnchor = false;
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	RefreshPlacedItemVisual();
	BP_OnSpatialAddressItemSet(nullptr);
	BP_OnSpatialEntryItemSet(nullptr);
	BP_OnSpatialItemDragDropStateChanged(ERpgInventorySlotDragVisualState::Normal);
}

void URpgInventorySpatialItemWidget::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::RefreshDragDropVisualState()
{
	if (DragDropVisualRefreshBatchDepth > 0)
	{
		bDragDropVisualRefreshPending = true;
		return;
	}

	ApplyDragDropVisualState();
}

void URpgInventorySpatialItemWidget::BeginDragDropVisualRefreshBatch()
{
	++DragDropVisualRefreshBatchDepth;
}

void URpgInventorySpatialItemWidget::EndDragDropVisualRefreshBatch()
{
	check(DragDropVisualRefreshBatchDepth > 0);
	--DragDropVisualRefreshBatchDepth;
	if (DragDropVisualRefreshBatchDepth == 0 && bDragDropVisualRefreshPending)
	{
		bDragDropVisualRefreshPending = false;
		ApplyDragDropVisualState();
	}
}

void URpgInventorySpatialItemWidget::ApplyDragDropVisualState()
{
	const bool bIsFocused = bInventoryPanelActive && IsFocusedItem();
	if (DragDropCoordinator && AddressSlotViewModel)
	{
		CurrentDragDropVisualState = DragDropCoordinator->GetInventoryAddressSlotVisualState(AddressSlotViewModel, bIsFocused);
	}
	else if (DragDropCoordinator && EntryViewModel)
	{
		CurrentDragDropVisualState = DragDropCoordinator->GetInventoryEntryVisualState(EntryViewModel, bIsFocused);
	}
	else
	{
		CurrentDragDropVisualState = bIsFocused ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal;
	}

	BP_OnSpatialItemDragDropStateChanged(CurrentDragDropVisualState);
}

int32 URpgInventorySpatialItemWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 PaintedLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (ItemVisual || !bUseNativeFallbackPaint)
	{
		return PaintedLayer;
	}

	int32 NextLayer = PaintedLayer + 1;
	if (UTexture2D* IconTexture = GetIcon().LoadSynchronous())
	{
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(IconTexture);
		const bool bRotated = IsPlacedItemRotated();
		FVector2D IconPosition;
		FVector2D IconPaintSize;
		URpgInventoryDragVisualWidget::CalculateIconPaintGeometry(
			AllottedGeometry.GetLocalSize(),
			bRotated,
			0.0f,
			IconPosition,
			IconPaintSize);
		IconBrush.ImageSize = IconPaintSize;
		if (bRotated)
		{
			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				NextLayer++,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(IconPaintSize),
					FSlateLayoutTransform(FVector2f(IconPosition))),
				&IconBrush,
				ESlateDrawEffect::None,
				UE_HALF_PI,
				FVector2f(IconPaintSize * 0.5f),
				FSlateDrawElement::RelativeToElement,
				InWidgetStyle.GetColorAndOpacityTint());
		}
		else
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				NextLayer++,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(IconPaintSize),
					FSlateLayoutTransform(FVector2f(IconPosition))),
				&IconBrush,
				ESlateDrawEffect::None,
				InWidgetStyle.GetColorAndOpacityTint());
		}
	}

	const int32 StackCount = GetStackCount();
	if (StackCount > 0)
	{
		const FString StackText = FString::Printf(TEXT("%dx"), StackCount);
		const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12);
		const FVector2D TextPosition(
			FMath::Max(0.0f, AllottedGeometry.GetLocalSize().X - 30.0f),
			FMath::Max(0.0f, AllottedGeometry.GetLocalSize().Y - 18.0f));
		FSlateDrawElement::MakeText(
			OutDrawElements,
			NextLayer++,
			AllottedGeometry.ToPaintGeometry(FVector2f(30.0f, 18.0f), FSlateLayoutTransform(FVector2f(TextPosition))),
			StackText,
			FontInfo,
			ESlateDrawEffect::None,
			FLinearColor::White);
	}

	return FMath::Max(PaintedLayer, NextLayer - 1);
}

FReply URpgInventorySpatialItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (OwningGrid)
	{
		OwningGrid->SelectCellFromScreenPosition(InMouseEvent.GetScreenSpacePosition(), GetOwningPlayer());
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OwningGrid &&
		OwningGrid->RequestContextMenuForSelectedCell(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && URpgInventoryDragDropCoordinator::IsPayloadValid(MakeDragPayload()))
	{
		if (InMouseEvent.IsControlDown())
		{
			return OwningGrid && OwningGrid->QuickTransferSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsAltDown())
		{
			return OwningGrid && OwningGrid->UseOrEquipSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}
		if (InMouseEvent.IsShiftDown())
		{
			return OwningGrid && OwningGrid->RequestSplitDialogForSelectedCell() ? FReply::Handled() : FReply::Unhandled();
		}

		FRpgInventoryDragPayload PointerPayload = MakeDragPayload();
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
			PointerPayload,
			InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()),
			InGeometry.GetLocalSize());
		const FVector2D ScreenTopLeft = InGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ScreenBottomRight = InGeometry.LocalToAbsolute(InGeometry.GetLocalSize());
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchorScreenGeometry(
			PointerPayload,
			ScreenTopLeft,
			InMouseEvent.GetScreenSpacePosition(),
			FVector2D(
				FMath::Abs(ScreenBottomRight.X - ScreenTopLeft.X),
				FMath::Abs(ScreenBottomRight.Y - ScreenTopLeft.Y)));
		PendingPointerDragAnchor = PointerPayload.DragAnchor;
		bHasPendingPointerDragAnchor = PendingPointerDragAnchor.bValid;
		bPendingLeftClickAccept = true;
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventorySpatialItemWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		bHasPendingPointerDragAnchor = false;
		return OwningGrid && OwningGrid->HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URpgInventorySpatialItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPendingLeftClickAccept = false;

	FRpgInventoryDragPayload Payload = MakeDragPayload();
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return;
	}
	if (bHasPendingPointerDragAnchor)
	{
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
			Payload,
			PendingPointerDragAnchor.SourcePointerOffset,
			PendingPointerDragAnchor.SourceVisualSize);
		Payload.DragAnchor.SourceScreenPointerOffset = PendingPointerDragAnchor.SourceScreenPointerOffset;
		Payload.DragAnchor.SourceScreenVisualSize = PendingPointerDragAnchor.SourceScreenVisualSize;
	}
	else
	{
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
			Payload,
			InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()),
			InGeometry.GetLocalSize());
		const FVector2D ScreenTopLeft = InGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ScreenBottomRight = InGeometry.LocalToAbsolute(InGeometry.GetLocalSize());
		URpgInventoryDragDropCoordinator::CapturePointerDragAnchorScreenGeometry(
			Payload,
			ScreenTopLeft,
			InMouseEvent.GetScreenSpacePosition(),
			FVector2D(
				FMath::Abs(ScreenBottomRight.X - ScreenTopLeft.X),
				FMath::Abs(ScreenBottomRight.Y - ScreenTopLeft.Y)));
	}
	bHasPendingPointerDragAnchor = false;
	if (!DragDropCoordinator || !DragVisualClass)
	{
		return;
	}
	if (!DragDropCoordinator->BeginPointerDrag(Payload))
	{
		return;
	}
	Payload = DragDropCoordinator->ResolveInteractionPayload(Payload);

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		DragDropCoordinator->CancelHold();
		return;
	}

	InventoryOperation->Pivot = EDragPivot::TopLeft;
	if (Payload.DragAnchor.SourceVisualSize.X > KINDA_SMALL_NUMBER && Payload.DragAnchor.SourceVisualSize.Y > KINDA_SMALL_NUMBER)
	{
		InventoryOperation->Offset = FVector2D(
			-Payload.DragAnchor.SourcePointerOffset.X / Payload.DragAnchor.SourceVisualSize.X,
			-Payload.DragAnchor.SourcePointerOffset.Y / Payload.DragAnchor.SourceVisualSize.Y);
	}
	InventoryOperation->InventoryPayload = Payload;
	InventoryOperation->SetInteractionSession(DragDropCoordinator->GetInteractionSession());
	InventoryOperation->Payload = AddressSlotViewModel ? Cast<UObject>(AddressSlotViewModel.Get()) : Cast<UObject>(EntryViewModel.Get());

	URpgInventoryDragVisualWidget* DragVisual =
		CreateWidget<URpgInventoryDragVisualWidget>(this, DragVisualClass);
	if (!DragVisual)
	{
		DragDropCoordinator->CancelHold();
		return;
	}

	DragVisual->ConfigureFromPayload(
		Payload,
		OwningGrid ? OwningGrid->GetSpatialCellSize() : 70.0f,
		OwningGrid ? OwningGrid->GetSpatialCellPadding() : 2.0f);
	InventoryOperation->DefaultDragVisual = DragVisual;
	OutOperation = InventoryOperation;
}

bool URpgInventorySpatialItemWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

bool URpgInventorySpatialItemWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

void URpgInventorySpatialItemWidget::HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == AddressSlotViewModel)
	{
		RefreshPlacedItemVisual();
		BP_OnSpatialAddressItemSet(AddressSlotViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgInventorySpatialItemWidget::HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	if (ChangedEntryViewModel == EntryViewModel)
	{
		RefreshPlacedItemVisual();
		BP_OnSpatialEntryItemSet(EntryViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgInventorySpatialItemWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

FRpgInventoryDragPayload URpgInventorySpatialItemWidget::MakeDragPayload() const
{
	if (AddressSlotViewModel)
	{
		return URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(AddressSlotViewModel);
	}

	if (EntryViewModel)
	{
		return URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromEntry(EntryViewModel);
	}

	return FRpgInventoryDragPayload();
}

void URpgInventorySpatialItemWidget::RefreshPlacedItemVisual()
{
	RefreshItemTooltip();

	if (!ItemVisual)
	{
		return;
	}

	const FRpgInventoryDragPayload Payload = MakeDragPayload();
	const float GridCellSize = OwningGrid ? OwningGrid->GetSpatialCellSize() : 70.0f;
	const float GridCellPadding = OwningGrid ? OwningGrid->GetSpatialCellPadding() : 2.0f;
	if (Payload.ItemInstance)
	{
		ItemVisual->ConfigureFromPayload(
			Payload,
			GridCellSize,
			GridCellPadding,
			ERpgInventoryInteractionPreviewState::None);
	}
	else
	{
		FRpgInventoryGridSize EmptyFootprint;
		EmptyFootprint.Width = 1;
		EmptyFootprint.Height = 1;
		ItemVisual->ConfigureVisual(
			TSoftObjectPtr<UTexture2D>(),
			0,
			EmptyFootprint,
			GridCellSize,
			GridCellPadding,
			ERpgInventoryInteractionPreviewState::None,
			false);
	}

	// The child is presentation-only; this outer spatial item remains the drag, context-menu, and focus target.
	ItemVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URpgInventorySpatialItemWidget::RefreshItemTooltip()
{
	URpgInventoryItemInstance* ItemInstance = AddressSlotViewModel
		? AddressSlotViewModel->GetItemInstance()
		: (EntryViewModel ? EntryViewModel->GetItemInstance() : nullptr);
	if (!ItemInstance || !ItemTooltipWidgetClass)
	{
		if (ItemTooltipWidget)
		{
			ItemTooltipWidget->ClearItem();
		}
		SetToolTip(nullptr);
		return;
	}

	if (!ItemTooltipWidget || !ItemTooltipWidget->IsA(ItemTooltipWidgetClass))
	{
		if (ItemTooltipWidget)
		{
			ItemTooltipWidget->ClearItem();
		}
		ItemTooltipWidget = URpgInventoryItemTooltipWidget::CreateForHost(
			this,
			ItemTooltipWidgetClass);
	}
	if (!ItemTooltipWidget)
	{
		SetToolTip(nullptr);
		return;
	}

	if (EntryViewModel)
	{
		ItemTooltipWidget->SetEntryViewModel(EntryViewModel);
	}
	else
	{
		ItemTooltipWidget->SetItemInstance(ItemInstance, GetStackCount());
	}
	SetToolTip(ItemTooltipWidget);
}

bool URpgInventorySpatialItemWidget::IsPlacedItemRotated() const
{
	if (AddressSlotViewModel)
	{
		const FRpgInventoryGridPlacement Placement = AddressSlotViewModel->GetItemPlacement();
		return Placement.IsValid() && Placement.bRotated;
	}

	if (EntryViewModel)
	{
		const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
		return Placement.IsValid() && Placement.bRotated;
	}

	return false;
}

TSoftObjectPtr<UTexture2D> URpgInventorySpatialItemWidget::GetIcon() const
{
	if (AddressSlotViewModel)
	{
		return AddressSlotViewModel->GetIcon();
	}

	return EntryViewModel ? EntryViewModel->GetIcon() : TSoftObjectPtr<UTexture2D>();
}

int32 URpgInventorySpatialItemWidget::GetStackCount() const
{
	if (AddressSlotViewModel)
	{
		return AddressSlotViewModel->GetStackCount();
	}

	return EntryViewModel ? EntryViewModel->GetStackCount() : 0;
}

FGuid URpgInventorySpatialItemWidget::GetRepresentedEntryId() const
{
	if (AddressSlotViewModel)
	{
		return AddressSlotViewModel->GetEntryId();
	}

	return EntryViewModel ? EntryViewModel->GetEntryId() : FGuid();
}

bool URpgInventorySpatialItemWidget::IsFocusedItem() const
{
	return OwningGrid && OwningGrid->IsItemWidgetFocused(this);
}
