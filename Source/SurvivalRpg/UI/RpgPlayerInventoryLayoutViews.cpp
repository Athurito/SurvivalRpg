#include "RpgPlayerInventoryLayoutViews.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Slate/UMGDragDropOp.h"
#include "Styling/CoreStyle.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgActionBarSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryLayoutViews)

namespace
{
	const FSlateBrush* GetInventoryWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	bool IsSameGridCell(const FRpgInventoryGridPlacement& Placement, FName ContainerId, int32 X, int32 Y)
	{
		return Placement.IsValid() &&
			Placement.ContainerId == ContainerId &&
			Placement.X == X &&
			Placement.Y == Y;
	}

	FRpgInventoryGridPlacement MakeCellPlacement(FName ContainerId, int32 X, int32 Y, bool bRotated)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.ContainerId = ContainerId;
		Placement.X = X;
		Placement.Y = Y;
		Placement.Width = 1;
		Placement.Height = 1;
		Placement.bRotated = bRotated;
		return Placement;
	}
}

URpgInventorySpatialCellWidget::URpgInventorySpatialCellWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void URpgInventorySpatialCellWidget::SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid, int32 InCellX, int32 InCellY)
{
	OwningGrid = InOwningGrid;
	CellX = InCellX;
	CellY = InCellY;
	BP_OnSpatialCellSet(CellX, CellY, AddressSlotViewModel, EntryViewModel);
	ApplyResolvedVisualState();
}

void URpgInventorySpatialCellWidget::SetCellViewModels(URpgInventoryAddressSlotViewModel* InAddressSlotViewModel, URpgInventoryEntryViewModel* InEntryViewModel)
{
	if (AddressSlotViewModel == InAddressSlotViewModel && EntryViewModel == InEntryViewModel)
	{
		return;
	}

	AddressSlotViewModel = InAddressSlotViewModel;
	EntryViewModel = InEntryViewModel;
	BP_OnSpatialCellSet(CellX, CellY, AddressSlotViewModel, EntryViewModel);
}

void URpgInventorySpatialCellWidget::SetCellVisualState(ERpgInventorySpatialCellVisualState InVisualState)
{
	BaseVisualState = InVisualState;
	ApplyResolvedVisualState();
}

FReply URpgInventorySpatialCellWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!OwningGrid)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (!OwningGrid->SelectCell(CellX, CellY, GetOwningPlayer()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OwningGrid->UseOrEquipSelectedCell())
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPendingLeftClickAccept = true;
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventorySpatialCellWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		return OwningGrid && OwningGrid->HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URpgInventorySpatialCellWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	ApplyResolvedVisualState();
}

void URpgInventorySpatialCellWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyResolvedVisualState();
}

bool URpgInventorySpatialCellWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!OwningGrid || !InventoryOperation)
	{
		return false;
	}

	OwningGrid->SelectCell(CellX, CellY, GetOwningPlayer());
	return OwningGrid->PreviewPayloadOnCell(InventoryOperation->InventoryPayload, CellX, CellY);
}

bool URpgInventorySpatialCellWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!OwningGrid || !InventoryOperation)
	{
		return false;
	}

	OwningGrid->SelectCell(CellX, CellY, GetOwningPlayer());
	return OwningGrid->CommitPayloadToCell(InventoryOperation->InventoryPayload, CellX, CellY);
}

void URpgInventorySpatialCellWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (OwningGrid)
	{
		OwningGrid->ClearExternalPreviewPayload();
	}

	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void URpgInventorySpatialCellWidget::ApplyResolvedVisualState()
{
	const ERpgInventorySpatialCellVisualState NewVisualState = ResolveHoveredVisualState();
	if (bHasAppliedVisualState && CurrentVisualState == NewVisualState)
	{
		return;
	}

	bHasAppliedVisualState = true;
	CurrentVisualState = NewVisualState;
	BP_OnSpatialCellStateChanged(CurrentVisualState);
}

ERpgInventorySpatialCellVisualState URpgInventorySpatialCellWidget::ResolveHoveredVisualState() const
{
	return bHovered && BaseVisualState == ERpgInventorySpatialCellVisualState::Normal
		? ERpgInventorySpatialCellVisualState::Hovered
		: BaseVisualState;
}

URpgActionBarTileView::URpgActionBarTileView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDragDrop = true;
	DragDropOperationClass = URpgInventoryDragDropOperation::StaticClass();
}

void URpgActionBarTileView::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyCoordinatorToEntry(EntryWidget);
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgActionBarTileView::SetActionBarSlotItems(const TArray<URpgActionBarSlotViewModel*>& InSlots)
{
	const TArray<UObject*>& CurrentItems = GetListItems();
	if (CurrentItems.Num() == InSlots.Num())
	{
		bool bSameItems = true;
		for (int32 Index = 0; Index < InSlots.Num(); ++Index)
		{
			if (CurrentItems[Index] != InSlots[Index])
			{
				bSameItems = false;
				break;
			}
		}

		if (bSameItems)
		{
			RequestRefresh();
			return;
		}
	}

	TArray<UObject*> NewItems;
	NewItems.Reserve(InSlots.Num());
	for (URpgActionBarSlotViewModel* SlotViewModel : InSlots)
	{
		NewItems.Add(SlotViewModel);
	}

	SetListItems(NewItems);
	RequestRefresh();
}

URpgActionBarSlotViewModel* URpgActionBarTileView::GetSelectedActionBarSlot() const
{
	return Cast<URpgActionBarSlotViewModel>(GetSelectedItem());
}

bool URpgActionBarTileView::SelectActionBarListItem(UObject* Item, APlayerController* OwningPlayer)
{
	if (!Item || !GetListItems().Contains(Item))
	{
		return false;
	}

	SetSelectedItem(Item);
	RequestNavigateToItem(Item);
	if (OwningPlayer)
	{
		SetUserFocus(OwningPlayer);
	}
	return true;
}

bool URpgActionBarTileView::SelectBestActionBarSlot(APlayerController* OwningPlayer)
{
	const TArray<UObject*>& Items = GetListItems();
	if (Items.IsEmpty())
	{
		return false;
	}

	UObject* DesiredItem = GetSelectedItem();
	if (!DesiredItem || !Items.Contains(DesiredItem))
	{
		DesiredItem = Items[0];
	}

	return SelectActionBarListItem(DesiredItem, OwningPlayer);
}

bool URpgActionBarTileView::SelectActionBarSlotByIndex(int32 SlotIndex, APlayerController* OwningPlayer)
{
	for (UObject* Item : GetListItems())
	{
		const URpgActionBarSlotViewModel* ActionBarSlot = Cast<URpgActionBarSlotViewModel>(Item);
		if (ActionBarSlot && ActionBarSlot->GetSlotIndex() == SlotIndex)
		{
			return SelectActionBarListItem(Item, OwningPlayer);
		}
	}

	return false;
}

void URpgActionBarTileView::ClearActionBarSelectionVisual()
{
	const bool bWasSuppressingPanelSelectionNotify = bSuppressPanelSelectionNotify;
	bSuppressPanelSelectionNotify = true;
	ITypedUMGListView<UObject*>::ClearSelection();
	bSuppressPanelSelectionNotify = bWasSuppressingPanelSelectionNotify;
}

void URpgActionBarTileView::SetActionBarPanelActive(bool bInActionBarPanelActive)
{
	if (bActionBarPanelActive == bInActionBarPanelActive)
	{
		return;
	}

	bActionBarPanelActive = bInActionBarPanelActive;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgActionBarTileView::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

void URpgActionBarTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
	ApplyPanelActiveStateToEntry(EntryWidget);
}

TOptional<EItemDropZone> URpgActionBarTileView::HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	if (!DragDropCoordinator)
	{
		return NullOpt;
	}

	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (!InventoryOperation)
	{
		return NullOpt;
	}

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakeActionBarSlotTargetFromViewModel(
		Cast<URpgActionBarSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	return DragDropCoordinator->PreviewPayloadDrop(InventoryOperation->InventoryPayload, Target) ? TOptional<EItemDropZone>(DropZone) : NullOpt;
}

FReply URpgActionBarTileView::HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return FReply::Unhandled();
	}

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakeActionBarSlotTargetFromViewModel(
		Cast<URpgActionBarSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	if (!DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, Target))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled().EndDragDrop();
}

void URpgActionBarTileView::OnSelectionChangedInternal(UObject* FirstSelectedItem)
{
	Super::OnSelectionChangedInternal(FirstSelectedItem);

	if (bSuppressPanelSelectionNotify || !PanelNavigationCoordinator || !FirstSelectedItem)
	{
		return;
	}

	PanelNavigationCoordinator->NotifyActionBarPanelSelectionChanged(this, FirstSelectedItem);
}

void URpgActionBarTileView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgActionBarSlotWidget* ActionBarSlotWidget = Cast<URpgActionBarSlotWidget>(EntryWidget))
	{
		ActionBarSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgActionBarTileView::ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const
{
	if (URpgActionBarSlotWidget* ActionBarSlotWidget = Cast<URpgActionBarSlotWidget>(EntryWidget))
	{
		ActionBarSlotWidget->SetActionBarPanelActive(bActionBarPanelActive);
	}
}

URpgInventorySpatialItemWidget::URpgInventorySpatialItemWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URpgInventorySpatialItemWidget::SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid)
{
	OwningGrid = InOwningGrid;
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
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
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(AddressSlotViewModel);
		}
	}

	BP_OnSpatialAddressItemSet(AddressSlotViewModel);
	RefreshDragDropVisualState();
}

void URpgInventorySpatialItemWidget::SetEntryViewModel(URpgInventoryEntryViewModel* InEntryViewModel)
{
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
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(EntryViewModel);
		}
	}

	BP_OnSpatialEntryItemSet(EntryViewModel);
	RefreshDragDropVisualState();
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
	if (!bUseNativeFallbackPaint)
	{
		return PaintedLayer;
	}

	int32 NextLayer = PaintedLayer + 1;
	if (UTexture2D* IconTexture = GetIcon().LoadSynchronous())
	{
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(IconTexture);
		IconBrush.ImageSize = AllottedGeometry.GetLocalSize();
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer++,
			AllottedGeometry.ToPaintGeometry(),
			&IconBrush,
			ESlateDrawEffect::None,
			InWidgetStyle.GetColorAndOpacityTint());
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

	return NextLayer;
}

FReply URpgInventorySpatialItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (OwningGrid)
	{
		OwningGrid->SelectCellFromScreenPosition(InMouseEvent.GetScreenSpacePosition(), GetOwningPlayer());
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OwningGrid && OwningGrid->UseOrEquipSelectedCell())
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && URpgInventoryDragDropCoordinator::IsPayloadValid(MakeDragPayload()))
	{
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
		return OwningGrid && OwningGrid->HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URpgInventorySpatialItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPendingLeftClickAccept = false;

	const FRpgInventoryDragPayload Payload = MakeDragPayload();
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return;
	}

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		return;
	}

	InventoryOperation->Pivot = EDragPivot::MouseDown;
	InventoryOperation->InventoryPayload = Payload;
	InventoryOperation->Payload = AddressSlotViewModel ? Cast<UObject>(AddressSlotViewModel.Get()) : Cast<UObject>(EntryViewModel.Get());

	TSubclassOf<UUserWidget> VisualClass = DragVisualClass;
	if (!VisualClass)
	{
		VisualClass = GetClass();
	}
	if (VisualClass)
	{
		UUserWidget* DragVisual = CreateWidget<UUserWidget>(GetWorld(), VisualClass);
		if (URpgInventorySpatialItemWidget* SpatialDragVisual = Cast<URpgInventorySpatialItemWidget>(DragVisual))
		{
			SpatialDragVisual->SetDragDropCoordinator(DragDropCoordinator);
			if (AddressSlotViewModel)
			{
				SpatialDragVisual->SetAddressSlotViewModel(AddressSlotViewModel);
			}
			else
			{
				SpatialDragVisual->SetEntryViewModel(EntryViewModel);
			}
		}
		InventoryOperation->DefaultDragVisual = DragVisual;
	}

	OutOperation = InventoryOperation;
}

bool URpgInventorySpatialItemWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	return OwningGrid &&
		InventoryOperation &&
		OwningGrid->PreviewPayloadOnItemWidget(InventoryOperation->InventoryPayload, this);
}

bool URpgInventorySpatialItemWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	return OwningGrid &&
		InventoryOperation &&
		OwningGrid->CommitPayloadToItemWidget(InventoryOperation->InventoryPayload, this);
}

void URpgInventorySpatialItemWidget::HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == AddressSlotViewModel)
	{
		BP_OnSpatialAddressItemSet(AddressSlotViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgInventorySpatialItemWidget::HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	if (ChangedEntryViewModel == EntryViewModel)
	{
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

bool URpgInventorySpatialItemWidget::IsFocusedItem() const
{
	return OwningGrid && OwningGrid->IsItemWidgetFocused(this);
}

URpgInventorySpatialGridWidget::URpgInventorySpatialGridWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SpatialItemWidgetClass = URpgInventorySpatialItemWidget::StaticClass();
	SpatialCellWidgetClass = URpgInventorySpatialCellWidget::StaticClass();
}

void URpgInventorySpatialGridWidget::BindSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	if (GroupViewModel == InGroupViewModel)
	{
		UpdateGridSizeFromBinding();
		RebuildItemOverlay();
		return;
	}

	ClearObservedSlotDelegates();
	ClearObservedEntryDelegates();
	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}

	GroupViewModel = InGroupViewModel;
	PanelViewModel = nullptr;
	Inventory = nullptr;
	ContainerId = GroupViewModel ? GroupViewModel->GetGroupId() : NAME_None;
	UpdateGridSizeFromBinding();
	ObserveSlotDelegates();
	RebuildItemOverlay();
	SelectBestCell(GetOwningPlayer(), false);
}

void URpgInventorySpatialGridWidget::BindInventoryPanelViewModel(URpgInventoryPanelViewModel* InPanelViewModel, URpgInventoryManagerComponent* InInventory, FName InContainerId)
{
	if (PanelViewModel == InPanelViewModel && Inventory == InInventory && ContainerId == InContainerId)
	{
		UpdateGridSizeFromBinding();
		RebuildItemOverlay();
		return;
	}

	ClearObservedSlotDelegates();
	ClearObservedEntryDelegates();
	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}

	GroupViewModel = nullptr;
	PanelViewModel = InPanelViewModel;
	Inventory = InInventory ? InInventory : (PanelViewModel ? PanelViewModel->GetObservedInventory() : nullptr);
	ContainerId = InContainerId;
	if (ContainerId.IsNone() && Inventory)
	{
		ContainerId = Inventory->GetDefaultContainerId();
	}

	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.AddUniqueDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}

	UpdateGridSizeFromBinding();
	ObserveEntryDelegates();
	RebuildItemOverlay();
	SelectBestCell(GetOwningPlayer(), false);
}

void URpgInventorySpatialGridWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	DragDropCoordinator = InCoordinator;
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->SetDragDropCoordinator(DragDropCoordinator);
		}
	}
	UpdateCellVisualStates();
}

void URpgInventorySpatialGridWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

void URpgInventorySpatialGridWidget::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->SetInventoryPanelActive(bInventoryPanelActive);
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

void URpgInventorySpatialGridWidget::SetCellMetrics(float InCellSize, float InCellPadding)
{
	CellSize = FMath::Max(1.0f, InCellSize);
	CellPadding = FMath::Max(0.0f, InCellPadding);
	UpdateDesiredGridSize();
	RebuildCellLayer();
	RebuildItemOverlay();
}

bool URpgInventorySpatialGridWidget::SelectBestCell(APlayerController* OwningPlayer, bool bPreferOccupiedSlot)
{
	if (!GridSize.IsValid())
	{
		return false;
	}

	if (IsValidCell(CursorX, CursorY))
	{
		return SelectCell(CursorX, CursorY, OwningPlayer);
	}

	if (bPreferOccupiedSlot)
	{
		if (GroupViewModel)
		{
			for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
			{
				if (SlotViewModel && SlotViewModel->ShouldRenderItemVisual())
				{
					const FRpgInventoryGridPlacement Placement = SlotViewModel->GetItemPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
		else if (PanelViewModel)
		{
			for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
			{
				if (EntryViewModel && !EntryViewModel->IsEmptySlot() && EntryViewModel->GetPlacement().ContainerId == ResolveContainerId())
				{
					const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
	}

	return SelectCell(0, 0, OwningPlayer);
}

bool URpgInventorySpatialGridWidget::SelectCellByIdentity(FGuid EntryId, int32 SlotIndex, APlayerController* OwningPlayer)
{
	if (EntryId.IsValid())
	{
		if (GroupViewModel)
		{
			for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
			{
				if (SlotViewModel && !SlotViewModel->IsEmptySlot() && SlotViewModel->GetEntryId() == EntryId)
				{
					const FRpgInventoryGridPlacement Placement = SlotViewModel->GetItemPlacement().IsValid()
						? SlotViewModel->GetItemPlacement()
						: SlotViewModel->GetPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
		else if (PanelViewModel)
		{
			for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
			{
				if (EntryViewModel && !EntryViewModel->IsEmptySlot() && EntryViewModel->GetEntryId() == EntryId)
				{
					const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
					return SelectCell(Placement.X, Placement.Y, OwningPlayer);
				}
			}
		}
	}

	if (SlotIndex != INDEX_NONE)
	{
		const int32 X = SlotIndex % 1000;
		const int32 Y = SlotIndex / 1000;
		return SelectCell(X, Y, OwningPlayer);
	}

	return false;
}

bool URpgInventorySpatialGridWidget::SelectCell(int32 X, int32 Y, APlayerController* OwningPlayer)
{
	if (!IsValidCell(X, Y))
	{
		return false;
	}

	CursorX = X;
	CursorY = Y;
	if (OwningPlayer)
	{
		SetUserFocus(OwningPlayer);
	}

	NotifySelectionChanged();
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->RefreshDragDropVisualState();
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
	return true;
}

void URpgInventorySpatialGridWidget::ClearSelectionVisual()
{
	bSelectionVisualSuppressed = true;
	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->RefreshDragDropVisualState();
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

bool URpgInventorySpatialGridWidget::HandleAcceptSelectedCell()
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (DragDropCoordinator->HasHeldPayload())
	{
		return DragDropCoordinator->CommitDrop(MakeDropTargetAtCursor());
	}

	const FRpgInventoryDragPayload Payload = MakePayloadFromSelectedItem();
	return URpgInventoryDragDropCoordinator::IsPayloadValid(Payload) && DragDropCoordinator->BeginHold(Payload);
}

bool URpgInventorySpatialGridWidget::QuickTransferSelectedCell(URpgInventoryManagerComponent* ExplicitTargetInventory)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->QuickTransferAddressSlot(AddressSlot, ExplicitTargetInventory);
	}

	return DragDropCoordinator->QuickTransferEntry(GetSelectedEntryViewModel(), ExplicitTargetInventory);
}

bool URpgInventorySpatialGridWidget::QuickSplitSelectedCell(int32 SplitCount)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (DragDropCoordinator->HasHeldPayload())
	{
		return ToggleHeldItemRotation();
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->QuickSplitAddressSlot(AddressSlot, FRpgInventoryGridPlacement(), SplitCount);
	}

	return DragDropCoordinator->QuickSplitEntry(GetSelectedEntryViewModel(), FRpgInventoryGridPlacement(), SplitCount);
}

bool URpgInventorySpatialGridWidget::UseOrEquipSelectedCell(int32 StackCount)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->UseOrEquipAddressSlot(AddressSlot, StackCount);
	}

	return DragDropCoordinator->UseOrEquipEntry(GetSelectedEntryViewModel(), StackCount);
}

bool URpgInventorySpatialGridWidget::DropSelectedCell(int32 StackCount, bool bConfirmed)
{
	if (!DragDropCoordinator)
	{
		return false;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return DragDropCoordinator->DropAddressSlot(AddressSlot, StackCount, bConfirmed);
	}

	return DragDropCoordinator->DropEntry(GetSelectedEntryViewModel(), StackCount, bConfirmed);
}

bool URpgInventorySpatialGridWidget::ToggleHeldItemRotation()
{
	if (!DragDropCoordinator || !DragDropCoordinator->HasHeldPayload())
	{
		return false;
	}

	bHeldTargetRotated = !bHeldTargetRotated;
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
	return true;
}

URpgInventoryAddressSlotViewModel* URpgInventorySpatialGridWidget::GetSelectedAddressSlot() const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	if (URpgInventoryAddressSlotViewModel* OccupyingItem = FindAddressItemAtCell(CursorX, CursorY))
	{
		return OccupyingItem;
	}

	return FindAddressCell(CursorX, CursorY);
}

URpgInventoryEntryViewModel* URpgInventorySpatialGridWidget::GetSelectedEntryViewModel() const
{
	return PanelViewModel ? FindEntryAtCell(CursorX, CursorY) : nullptr;
}

FGuid URpgInventorySpatialGridWidget::GetSelectedEntryId() const
{
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return !AddressSlot->IsEmptySlot() ? AddressSlot->GetEntryId() : FGuid();
	}

	if (URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel())
	{
		return !EntryViewModel->IsEmptySlot() ? EntryViewModel->GetEntryId() : FGuid();
	}

	return FGuid();
}

int32 URpgInventorySpatialGridWidget::GetSelectedSlotIndex() const
{
	return IsValidCell(CursorX, CursorY) ? CursorY * 1000 + CursorX : INDEX_NONE;
}

bool URpgInventorySpatialGridWidget::SelectCellFromScreenPosition(FVector2D ScreenPosition, APlayerController* OwningPlayer)
{
	const FVector2D LocalPosition = GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	if (!TryGetCellFromLocalPosition(LocalPosition, CellX, CellY))
	{
		return false;
	}

	return SelectCell(CellX, CellY, OwningPlayer);
}

bool URpgInventorySpatialGridWidget::CommitPayloadToCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y)
{
	bHasExternalPreviewPayload = false;
	ExternalPreviewPayload = FRpgInventoryDragPayload();
	UpdateCellVisualStates();

	return DragDropCoordinator &&
		IsValidCell(X, Y) &&
		DragDropCoordinator->CommitPayloadToTarget(Payload, MakeDropTargetForCell(X, Y));
}

bool URpgInventorySpatialGridWidget::PreviewPayloadOnCell(const FRpgInventoryDragPayload& Payload, int32 X, int32 Y)
{
	if (!IsValidCell(X, Y))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	bHasExternalPreviewPayload = true;
	ExternalPreviewPayload = Payload;
	UpdateCellVisualStates();

	return DragDropCoordinator &&
		DragDropCoordinator->PreviewPayloadDrop(Payload, MakeDropTargetForCell(X, Y));
}

void URpgInventorySpatialGridWidget::ClearExternalPreviewPayload()
{
	if (!bHasExternalPreviewPayload)
	{
		return;
	}

	bHasExternalPreviewPayload = false;
	ExternalPreviewPayload = FRpgInventoryDragPayload();
	UpdateCellVisualStates();
}

bool URpgInventorySpatialGridWidget::CommitPayloadToItemWidget(const FRpgInventoryDragPayload& Payload, const URpgInventorySpatialItemWidget* ItemWidget)
{
	bHasExternalPreviewPayload = false;
	ExternalPreviewPayload = FRpgInventoryDragPayload();
	UpdateCellVisualStates();

	return DragDropCoordinator &&
		ItemWidget &&
		DragDropCoordinator->CommitPayloadToTarget(Payload, MakeDropTargetForItemWidget(ItemWidget));
}

bool URpgInventorySpatialGridWidget::PreviewPayloadOnItemWidget(const FRpgInventoryDragPayload& Payload, const URpgInventorySpatialItemWidget* ItemWidget) const
{
	return DragDropCoordinator &&
		ItemWidget &&
		DragDropCoordinator->PreviewPayloadDrop(Payload, MakeDropTargetForItemWidget(ItemWidget));
}

bool URpgInventorySpatialGridWidget::IsItemWidgetFocused(const URpgInventorySpatialItemWidget* ItemWidget) const
{
	if (!ItemWidget)
	{
		return false;
	}

	if (const URpgInventoryAddressSlotViewModel* AddressSlot = ItemWidget->GetAddressSlotViewModel())
	{
		const FRpgInventoryGridPlacement Placement = AddressSlot->GetItemPlacement().IsValid()
			? AddressSlot->GetItemPlacement()
			: AddressSlot->GetPlacement();
		return Placement.ContainsCell(CursorX, CursorY);
	}

	if (const URpgInventoryEntryViewModel* EntryViewModel = ItemWidget->GetEntryViewModel())
	{
		return EntryViewModel->GetPlacement().ContainsCell(CursorX, CursorY);
	}

	return false;
}

void URpgInventorySpatialGridWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureRuntimeWidgets();
	UpdateDesiredGridSize();
}

void URpgInventorySpatialGridWidget::NativeDestruct()
{
	ClearObservedSlotDelegates();
	ClearObservedEntryDelegates();
	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::RefreshFromPanelViewModel);
	}
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	Super::NativeDestruct();
}

int32 URpgInventorySpatialGridWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 NextLayer = LayerId;
	const FSlateBrush* WhiteBrush = GetInventoryWhiteBrush();
	if (bUseNativeDebugPaint && WhiteBrush && GridSize.IsValid())
	{
		for (int32 Y = 0; Y < GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < GridSize.Width; ++X)
			{
				const FVector2D Position = GetCellPosition(X, Y);
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					NextLayer,
					AllottedGeometry.ToPaintGeometry(FVector2f(CellSize, CellSize), FSlateLayoutTransform(FVector2f(Position))),
					WhiteBrush,
					ESlateDrawEffect::None,
					CellFillColor);

				TArray<FVector2f> BorderPoints;
				BorderPoints.Add(FVector2f(Position));
				BorderPoints.Add(FVector2f(Position.X + CellSize, Position.Y));
				BorderPoints.Add(FVector2f(Position.X + CellSize, Position.Y + CellSize));
				BorderPoints.Add(FVector2f(Position.X, Position.Y + CellSize));
				BorderPoints.Add(FVector2f(Position));
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					NextLayer + 1,
					AllottedGeometry.ToPaintGeometry(),
					BorderPoints,
					ESlateDrawEffect::None,
					CellBorderColor,
					true,
					1.0f);
			}
		}

		NextLayer += 2;
		if (bInventoryPanelActive && !bSelectionVisualSuppressed && IsValidCell(CursorX, CursorY))
		{
			const FVector2D Position = GetCellPosition(CursorX, CursorY);
			TArray<FVector2f> CursorPoints;
			CursorPoints.Add(FVector2f(Position));
			CursorPoints.Add(FVector2f(Position.X + CellSize, Position.Y));
			CursorPoints.Add(FVector2f(Position.X + CellSize, Position.Y + CellSize));
			CursorPoints.Add(FVector2f(Position.X, Position.Y + CellSize));
			CursorPoints.Add(FVector2f(Position));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer++,
				AllottedGeometry.ToPaintGeometry(),
				CursorPoints,
				ESlateDrawEffect::None,
				CursorColor,
				true,
				2.0f);
		}
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NextLayer, InWidgetStyle, bParentEnabled);
}

FReply URpgInventorySpatialGridWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		return MoveCursorBy(-1, 0, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		return MoveCursorBy(1, 0, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		return MoveCursorBy(0, -1, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		return MoveCursorBy(0, 1, GetOwningPlayer()) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		return HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URpgInventorySpatialGridWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	if (!TryGetCellFromLocalPosition(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), CellX, CellY))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	SelectCell(CellX, CellY, GetOwningPlayer());

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && UseOrEquipSelectedCell())
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPendingLeftClickAccept = true;
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventorySpatialGridWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingLeftClickAccept)
	{
		bPendingLeftClickAccept = false;
		return HandleAcceptSelectedCell() ? FReply::Handled() : FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

bool URpgInventorySpatialGridWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	if (TryGetCellFromLocalPosition(InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition()), CellX, CellY))
	{
		SelectCell(CellX, CellY, GetOwningPlayer());
	}

	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	return InventoryOperation && PreviewPayloadOnCell(InventoryOperation->InventoryPayload, CursorX, CursorY);
}

bool URpgInventorySpatialGridWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	if (TryGetCellFromLocalPosition(InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition()), CellX, CellY))
	{
		SelectCell(CellX, CellY, GetOwningPlayer());
	}

	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	return InventoryOperation && CommitPayloadToCell(InventoryOperation->InventoryPayload, CursorX, CursorY);
}

void URpgInventorySpatialGridWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearExternalPreviewPayload();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

FReply URpgInventorySpatialGridWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	bSelectionVisualSuppressed = false;
	NotifySelectionChanged();
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
	return FReply::Handled();
}

void URpgInventorySpatialGridWidget::RefreshFromPanelViewModel()
{
	UpdateGridSizeFromBinding();
	ObserveEntryDelegates();
	RebuildItemOverlay();
}

void URpgInventorySpatialGridWidget::HandleAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel)
{
	RebuildItemOverlay();
}

void URpgInventorySpatialGridWidget::HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	RebuildItemOverlay();
}

void URpgInventorySpatialGridWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	if (!bHasHeldPayload)
	{
		bHeldTargetRotated = false;
	}

	for (URpgInventorySpatialItemWidget* ItemWidget : ItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->RefreshDragDropVisualState();
		}
	}
	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

void URpgInventorySpatialGridWidget::EnsureRuntimeWidgets()
{
	if (!RootSizeBox)
	{
		RootSizeBox = Cast<USizeBox>(GetRootWidget());
	}

	if (!CellCanvas && CellLayer)
	{
		CellCanvas = CellLayer;
	}

	if (ItemCanvas)
	{
		ItemCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (!RootSizeBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint SizeBox named RootSizeBox to enforce fixed spatial grid dimensions."), *GetNameSafe(this));
	}

	if (!CellCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint CanvasPanel named CellCanvas or CellLayer for designable spatial slots."), *GetNameSafe(this));
	}

	if (!ItemCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint CanvasPanel named ItemCanvas for spatial item overlays."), *GetNameSafe(this));
	}
}

void URpgInventorySpatialGridWidget::UpdateGridSizeFromBinding()
{
	if (GroupViewModel)
	{
		GridSize = GroupViewModel->GetGridSize();
		ContainerId = GroupViewModel->GetGroupId();
	}
	else if (Inventory)
	{
		FRpgInventoryGridSize ResolvedGridSize;
		const FName ResolvedContainerId = ResolveContainerId();
		GridSize = Inventory->GetGridSizeForContainer(ResolvedContainerId, ResolvedGridSize)
			? ResolvedGridSize
			: Inventory->GetDefaultGridSize();
		ContainerId = ResolvedContainerId;
	}
	else
	{
		GridSize = FRpgInventoryGridSize();
	}

	if (!GridSize.IsValid())
	{
		GridSize.Width = 1;
		GridSize.Height = 1;
	}

	CursorX = FMath::Clamp(CursorX, 0, GridSize.Width - 1);
	CursorY = FMath::Clamp(CursorY, 0, GridSize.Height - 1);
	UpdateDesiredGridSize();
	RebuildCellLayer();
}

void URpgInventorySpatialGridWidget::UpdateDesiredGridSize()
{
	EnsureRuntimeWidgets();

	const float DesiredWidth = GridSize.Width * CellSize + FMath::Max(0, GridSize.Width - 1) * CellPadding;
	const float DesiredHeight = GridSize.Height * CellSize + FMath::Max(0, GridSize.Height - 1) * CellPadding;
	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(DesiredWidth);
		RootSizeBox->SetHeightOverride(DesiredHeight);
	}
}

void URpgInventorySpatialGridWidget::RebuildCellLayer()
{
	EnsureRuntimeWidgets();
	CellWidgets.Reset();
	UCanvasPanel* ResolvedCellCanvas = CellCanvas ? CellCanvas.Get() : CellLayer.Get();
	if (!ResolvedCellCanvas || !SpatialCellWidgetClass || !GridSize.IsValid())
	{
		return;
	}

	ResolvedCellCanvas->ClearChildren();
	for (int32 Y = 0; Y < GridSize.Height; ++Y)
	{
		for (int32 X = 0; X < GridSize.Width; ++X)
		{
			URpgInventorySpatialCellWidget* CellWidget = CreateWidget<URpgInventorySpatialCellWidget>(GetWorld(), SpatialCellWidgetClass);
			if (!CellWidget)
			{
				continue;
			}

			CellWidget->SetOwningSpatialGrid(this, X, Y);
			CellWidget->SetCellViewModels(FindAddressCell(X, Y), FindEntryAtCell(X, Y));
			CellWidget->SetCellVisualState(GetCellVisualState(X, Y));

			if (UCanvasPanelSlot* CanvasSlot = ResolvedCellCanvas->AddChildToCanvas(CellWidget))
			{
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetPosition(GetCellPosition(X, Y));
				CanvasSlot->SetSize(FVector2D(CellSize, CellSize));
				CanvasSlot->SetZOrder(0);
			}

			CellWidgets.Add(CellWidget);
		}
	}
}

void URpgInventorySpatialGridWidget::UpdateCellVisualStates()
{
	for (URpgInventorySpatialCellWidget* CellWidget : CellWidgets)
	{
		if (!CellWidget)
		{
			continue;
		}

		const int32 X = CellWidget->GetCellX();
		const int32 Y = CellWidget->GetCellY();
		CellWidget->SetCellViewModels(FindAddressCell(X, Y), FindEntryAtCell(X, Y));
		CellWidget->SetCellVisualState(GetCellVisualState(X, Y));
	}
}

ERpgInventorySpatialCellVisualState URpgInventorySpatialGridWidget::GetCellVisualState(int32 X, int32 Y) const
{
	if (!IsValidCell(X, Y))
	{
		return ERpgInventorySpatialCellVisualState::Normal;
	}

	const bool bIsSelectedCell = X == CursorX && Y == CursorY;
	const bool bCanShowCursor = bInventoryPanelActive && !bSelectionVisualSuppressed && bIsSelectedCell;
	if (bCanShowCursor)
	{
		if (bHasExternalPreviewPayload)
		{
			return DragDropCoordinator && DragDropCoordinator->PreviewPayloadDrop(ExternalPreviewPayload, MakeDropTargetForCell(X, Y))
				? ERpgInventorySpatialCellVisualState::ValidPreview
				: ERpgInventorySpatialCellVisualState::InvalidPreview;
		}

		if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
		{
			return DragDropCoordinator->PreviewPayloadDrop(DragDropCoordinator->GetHeldPayload(), MakeDropTargetForCell(X, Y))
				? ERpgInventorySpatialCellVisualState::ValidPreview
				: ERpgInventorySpatialCellVisualState::InvalidPreview;
		}

		return ERpgInventorySpatialCellVisualState::Selected;
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = FindAddressItemAtCell(X, Y))
	{
		const FRpgInventoryGridPlacement Placement = AddressSlot->GetItemPlacement().IsValid()
			? AddressSlot->GetItemPlacement()
			: AddressSlot->GetPlacement();
		return Placement.X == X && Placement.Y == Y
			? ERpgInventorySpatialCellVisualState::Occupied
			: ERpgInventorySpatialCellVisualState::Covered;
	}

	if (URpgInventoryEntryViewModel* EntryViewModel = FindEntryAtCell(X, Y))
	{
		const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
		return Placement.X == X && Placement.Y == Y
			? ERpgInventorySpatialCellVisualState::Occupied
			: ERpgInventorySpatialCellVisualState::Covered;
	}

	return ERpgInventorySpatialCellVisualState::Normal;
}

void URpgInventorySpatialGridWidget::RebuildItemOverlay()
{
	EnsureRuntimeWidgets();
	ItemWidgets.Reset();
	if (!ItemCanvas || !SpatialItemWidgetClass)
	{
		UpdateCellVisualStates();
		return;
	}

	ItemCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ItemCanvas->ClearChildren();

	auto AddSpatialItem = [this](const FRpgInventoryGridPlacement& Placement, URpgInventoryAddressSlotViewModel* AddressSlot, URpgInventoryEntryViewModel* EntryViewModel)
	{
		if (!Placement.IsValid())
		{
			return;
		}

		URpgInventorySpatialItemWidget* ItemWidget = CreateWidget<URpgInventorySpatialItemWidget>(GetWorld(), SpatialItemWidgetClass);
		if (!ItemWidget)
		{
			return;
		}

		ItemWidget->SetVisibility(ESlateVisibility::Visible);
		ItemWidget->SetIsEnabled(true);
		ItemWidget->SetOwningSpatialGrid(this);
		ItemWidget->SetDragDropCoordinator(DragDropCoordinator);
		ItemWidget->SetInventoryPanelActive(bInventoryPanelActive);
		if (AddressSlot)
		{
			ItemWidget->SetAddressSlotViewModel(AddressSlot);
		}
		else
		{
			ItemWidget->SetEntryViewModel(EntryViewModel);
		}

		if (UCanvasPanelSlot* CanvasSlot = ItemCanvas->AddChildToCanvas(ItemWidget))
		{
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetPosition(GetCellPosition(Placement.X, Placement.Y));
			CanvasSlot->SetSize(GetPlacementSize(Placement));
			CanvasSlot->SetZOrder(10);
		}

		ItemWidgets.Add(ItemWidget);
	};

	if (GroupViewModel)
	{
		for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
		{
			if (!SlotViewModel || !SlotViewModel->ShouldRenderItemVisual())
			{
				continue;
			}

			AddSpatialItem(SlotViewModel->GetItemPlacement(), SlotViewModel, nullptr);
		}
	}
	else if (PanelViewModel)
	{
		for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
		{
			if (!EntryViewModel || EntryViewModel->IsEmptySlot())
			{
				continue;
			}

			const FRpgInventoryGridPlacement Placement = EntryViewModel->GetPlacement();
			if (Placement.ContainerId == ResolveContainerId())
			{
				AddSpatialItem(Placement, nullptr, EntryViewModel);
			}
		}
	}

	UpdateCellVisualStates();
	InvalidateLayoutAndVolatility();
}

void URpgInventorySpatialGridWidget::ClearObservedSlotDelegates()
{
	for (URpgInventoryAddressSlotViewModel* SlotViewModel : ObservedAddressSlots)
	{
		if (SlotViewModel)
		{
			SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleAddressSlotChanged);
		}
	}
	ObservedAddressSlots.Reset();
}

void URpgInventorySpatialGridWidget::ObserveSlotDelegates()
{
	ClearObservedSlotDelegates();
	if (!GroupViewModel)
	{
		return;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel)
		{
			SlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleAddressSlotChanged);
			ObservedAddressSlots.Add(SlotViewModel);
		}
	}
}

void URpgInventorySpatialGridWidget::ClearObservedEntryDelegates()
{
	for (URpgInventoryEntryViewModel* EntryViewModel : ObservedEntries)
	{
		if (EntryViewModel)
		{
			EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryChanged);
		}
	}
	ObservedEntries.Reset();
}

void URpgInventorySpatialGridWidget::ObserveEntryDelegates()
{
	ClearObservedEntryDelegates();
	if (!PanelViewModel)
	{
		return;
	}

	for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
	{
		if (EntryViewModel)
		{
			EntryViewModel->OnEntryChanged.AddUniqueDynamic(this, &ThisClass::HandleEntryChanged);
			ObservedEntries.Add(EntryViewModel);
		}
	}
}

void URpgInventorySpatialGridWidget::NotifySelectionChanged()
{
	bSelectionVisualSuppressed = false;
	if (!PanelNavigationCoordinator)
	{
		return;
	}

	UObject* SelectedObject = nullptr;
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		SelectedObject = AddressSlot;
	}
	else if (URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel())
	{
		SelectedObject = EntryViewModel;
	}

	PanelNavigationCoordinator->NotifySpatialPanelSelectionChanged(this, SelectedObject);
}

bool URpgInventorySpatialGridWidget::MoveCursorBy(int32 DeltaX, int32 DeltaY, APlayerController* OwningPlayer)
{
	return SelectCell(
		FMath::Clamp(CursorX + DeltaX, 0, FMath::Max(0, GridSize.Width - 1)),
		FMath::Clamp(CursorY + DeltaY, 0, FMath::Max(0, GridSize.Height - 1)),
		OwningPlayer);
}

bool URpgInventorySpatialGridWidget::TryGetCellFromLocalPosition(FVector2D LocalPosition, int32& OutX, int32& OutY) const
{
	OutX = INDEX_NONE;
	OutY = INDEX_NONE;
	if (!GridSize.IsValid() || LocalPosition.X < 0.0f || LocalPosition.Y < 0.0f)
	{
		return false;
	}

	const float Stride = CellSize + CellPadding;
	if (Stride <= 0.0f)
	{
		return false;
	}

	const int32 X = FMath::FloorToInt(LocalPosition.X / Stride);
	const int32 Y = FMath::FloorToInt(LocalPosition.Y / Stride);
	if (!IsValidCell(X, Y))
	{
		return false;
	}

	const float RemainderX = LocalPosition.X - X * Stride;
	const float RemainderY = LocalPosition.Y - Y * Stride;
	if (RemainderX > CellSize || RemainderY > CellSize)
	{
		return false;
	}

	OutX = X;
	OutY = Y;
	return true;
}

FRpgInventoryDropTarget URpgInventorySpatialGridWidget::MakeDropTargetAtCursor() const
{
	return MakeDropTargetForCell(CursorX, CursorY);
}

FRpgInventoryDropTarget URpgInventorySpatialGridWidget::MakeDropTargetForCell(int32 X, int32 Y) const
{
	if (GroupViewModel)
	{
		return URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(FindAddressCell(X, Y));
	}

	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	Target.TargetInventory = Inventory.Get();
	Target.TargetPlacement = MakeCellPlacement(ResolveContainerId(), X, Y, bHeldTargetRotated);
	return Target;
}

FRpgInventoryDropTarget URpgInventorySpatialGridWidget::MakeDropTargetForItemWidget(const URpgInventorySpatialItemWidget* ItemWidget) const
{
	if (!ItemWidget)
	{
		return FRpgInventoryDropTarget();
	}

	if (URpgInventoryAddressSlotViewModel* AddressSlot = ItemWidget->GetAddressSlotViewModel())
	{
		return URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(AddressSlot);
	}

	if (URpgInventoryEntryViewModel* EntryViewModel = ItemWidget->GetEntryViewModel())
	{
		FRpgInventoryDropTarget Target;
		Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
		Target.TargetInventory = EntryViewModel->GetInventoryManager();
		Target.TargetPlacement = EntryViewModel->GetPlacement();
		return Target;
	}

	return FRpgInventoryDropTarget();
}

FRpgInventoryDragPayload URpgInventorySpatialGridWidget::MakePayloadFromSelectedItem() const
{
	if (URpgInventoryAddressSlotViewModel* AddressSlot = GetSelectedAddressSlot())
	{
		return AddressSlot->GetItemInstance()
			? URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(AddressSlot)
			: FRpgInventoryDragPayload();
	}

	if (URpgInventoryEntryViewModel* EntryViewModel = GetSelectedEntryViewModel())
	{
		return URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromEntry(EntryViewModel);
	}

	return FRpgInventoryDragPayload();
}

URpgInventoryAddressSlotViewModel* URpgInventorySpatialGridWidget::FindAddressCell(int32 X, int32 Y) const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel && IsSameGridCell(SlotViewModel->GetPlacement(), ResolveContainerId(), X, Y))
		{
			return SlotViewModel;
		}
	}

	return nullptr;
}

URpgInventoryAddressSlotViewModel* URpgInventorySpatialGridWidget::FindAddressItemAtCell(int32 X, int32 Y) const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel &&
			SlotViewModel->ShouldRenderItemVisual() &&
			SlotViewModel->GetItemPlacement().ContainsCell(X, Y))
		{
			return SlotViewModel;
		}
	}

	return nullptr;
}

URpgInventoryEntryViewModel* URpgInventorySpatialGridWidget::FindEntryAtCell(int32 X, int32 Y) const
{
	if (!PanelViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryEntryViewModel* EntryViewModel : PanelViewModel->GetEntries())
	{
		if (EntryViewModel &&
			!EntryViewModel->IsEmptySlot() &&
			EntryViewModel->GetPlacement().ContainerId == ResolveContainerId() &&
			EntryViewModel->GetPlacement().ContainsCell(X, Y))
		{
			return EntryViewModel;
		}
	}

	return nullptr;
}

FVector2D URpgInventorySpatialGridWidget::GetCellPosition(int32 X, int32 Y) const
{
	const float Stride = CellSize + CellPadding;
	return FVector2D(X * Stride, Y * Stride);
}

FVector2D URpgInventorySpatialGridWidget::GetPlacementSize(const FRpgInventoryGridPlacement& Placement) const
{
	const FRpgInventoryGridSize OccupiedSize = Placement.GetOccupiedSize();
	return FVector2D(
		OccupiedSize.Width * CellSize + FMath::Max(0, OccupiedSize.Width - 1) * CellPadding,
		OccupiedSize.Height * CellSize + FMath::Max(0, OccupiedSize.Height - 1) * CellPadding);
}

FName URpgInventorySpatialGridWidget::ResolveContainerId() const
{
	if (!ContainerId.IsNone())
	{
		return ContainerId;
	}

	return Inventory ? Inventory->GetDefaultContainerId() : NAME_None;
}

bool URpgInventorySpatialGridWidget::IsValidCell(int32 X, int32 Y) const
{
	return GridSize.IsValid() &&
		X >= 0 &&
		Y >= 0 &&
		X < GridSize.Width &&
		Y < GridSize.Height;
}

URpgInventorySlotGroupWidget::URpgInventorySlotGroupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SpatialGridWidgetClass = URpgInventorySpatialGridWidget::StaticClass();
}

void URpgInventorySlotGroupWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	EnsureSpatialGrid();
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationIdPrefix = InPanelIdPrefix;
	RegisterPanelNavigationEntry();
}

void URpgInventorySlotGroupWidget::SetSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	GroupViewModel = InGroupViewModel;
	EnsureSpatialGrid();
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
		SpatialGrid->BindSlotGroupViewModel(GroupViewModel);
	}

	RegisterPanelNavigationEntry();
	BP_OnSlotGroupViewModelSet(GroupViewModel);
}

void URpgInventorySlotGroupWidget::NativeDestruct()
{
	if (SpatialGrid)
	{
		SpatialGrid->SetPanelNavigationCoordinator(nullptr, NAME_None);
	}

	Super::NativeDestruct();
}

void URpgInventorySlotGroupWidget::EnsureSpatialGrid()
{
	if (SpatialGrid)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint child widget named SpatialGrid using URpgInventorySpatialGridWidget."), *GetNameSafe(this));
}

void URpgInventorySlotGroupWidget::RegisterPanelNavigationEntry()
{
	if (!PanelNavigationCoordinator || !SpatialGrid || !GroupViewModel)
	{
		return;
	}

	URpgInventoryManagerComponent* Inventory = ResolveGroupInventory();
	if (!Inventory)
	{
		return;
	}

	SpatialGrid->SetPanelNavigationCoordinator(PanelNavigationCoordinator, MakePanelNavigationId());
	PanelNavigationCoordinator->RegisterSpatialInventoryPanel(MakePanelNavigationId(), SpatialGrid, Inventory);
}

URpgInventoryManagerComponent* URpgInventorySlotGroupWidget::ResolveGroupInventory() const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel && SlotViewModel->GetInventoryManager())
		{
			return SlotViewModel->GetInventoryManager();
		}
	}

	return nullptr;
}

FName URpgInventorySlotGroupWidget::MakePanelNavigationId() const
{
	const FName GroupId = GroupViewModel ? GroupViewModel->GetGroupId() : NAME_None;
	if (PanelNavigationIdPrefix.IsNone())
	{
		return GroupId;
	}

	return FName(*FString::Printf(TEXT("%s.%s"), *PanelNavigationIdPrefix.ToString(), *GroupId.ToString()));
}

URpgInventorySlotGroupPanelWidget::URpgInventorySlotGroupPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GroupWidgetClass = URpgInventorySlotGroupWidget::StaticClass();
}

void URpgInventorySlotGroupPanelWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		ApplyCoordinatorToGroup(GroupWidget);
	}
}

void URpgInventorySlotGroupPanelWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationIdPrefix = InPanelIdPrefix;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		ApplyNavigationToGroup(GroupWidget);
	}
}

void URpgInventorySlotGroupPanelWidget::SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups)
{
	GroupItems.Reset();
	GroupItems.Reserve(InGroups.Num());
	for (URpgInventorySlotGroupViewModel* GroupViewModel : InGroups)
	{
		GroupItems.Add(GroupViewModel);
	}

	RebuildGroupWidgets();
}

void URpgInventorySlotGroupPanelWidget::EnsureGroupsPanel()
{
	if (GroupsPanel)
	{
		return;
	}

	GroupsPanel = Cast<UPanelWidget>(GetRootWidget());
	if (GroupsPanel || !WidgetTree)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint panel named GroupsPanel or a panel root to receive spatial group widgets."), *GetNameSafe(this));
}

void URpgInventorySlotGroupPanelWidget::RebuildGroupWidgets()
{
	EnsureGroupsPanel();
	GroupWidgets.Reset();
	if (!GroupsPanel)
	{
		return;
	}

	GroupsPanel->ClearChildren();
	TSubclassOf<URpgInventorySlotGroupWidget> EntryClass = GroupWidgetClass;
	if (!EntryClass)
	{
		EntryClass = URpgInventorySlotGroupWidget::StaticClass();
	}
	for (URpgInventorySlotGroupViewModel* GroupViewModel : GroupItems)
	{
		if (!GroupViewModel)
		{
			continue;
		}

		URpgInventorySlotGroupWidget* GroupWidget = CreateWidget<URpgInventorySlotGroupWidget>(GetWorld(), EntryClass);
		if (!GroupWidget)
		{
			continue;
		}

		GroupsPanel->AddChild(GroupWidget);
		GroupWidgets.Add(GroupWidget);
		ApplyCoordinatorToGroup(GroupWidget);
		ApplyNavigationToGroup(GroupWidget);
		GroupWidget->SetSlotGroupViewModel(GroupViewModel);
	}
}

void URpgInventorySlotGroupPanelWidget::ApplyCoordinatorToGroup(URpgInventorySlotGroupWidget* GroupWidget) const
{
	if (GroupWidget)
	{
		GroupWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupPanelWidget::ApplyNavigationToGroup(URpgInventorySlotGroupWidget* GroupWidget) const
{
	if (GroupWidget)
	{
		GroupWidget->SetPanelNavigationCoordinator(PanelNavigationCoordinator, PanelNavigationIdPrefix);
	}
}
