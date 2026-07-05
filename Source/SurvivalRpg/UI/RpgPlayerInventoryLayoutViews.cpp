#include "RpgPlayerInventoryLayoutViews.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "MVVMSubsystem.h"
#include "Rendering/DrawElements.h"
#include "SCommonTileView.h"
#include "Slate/UMGDragDropOp.h"
#include "Styling/CoreStyle.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgActionBarSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryLayoutViews)

namespace
{
	void ApplyCompactRuntimeGridParentSlot(UPanelSlot* PanelSlot)
	{
		if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
		{
			VerticalBoxSlot->SetHorizontalAlignment(HAlign_Left);
			VerticalBoxSlot->SetVerticalAlignment(VAlign_Top);
			return;
		}

		if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			HorizontalBoxSlot->SetHorizontalAlignment(HAlign_Left);
			HorizontalBoxSlot->SetVerticalAlignment(VAlign_Top);
			return;
		}

		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Left);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			return;
		}

		if (UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(PanelSlot))
		{
			CanvasPanelSlot->SetAutoSize(true);
		}
	}

	template <typename ItemType>
	class SRpgInventoryFixedGridTileView : public SCommonTileView<ItemType>
	{
	public:
		void SetDesiredGridDimensions(int32 InColumns, int32 InRows)
		{
			const int32 NewColumns = FMath::Max(0, InColumns);
			const int32 NewRows = FMath::Max(0, InRows);
			if (DesiredColumns == NewColumns && DesiredRows == NewRows)
			{
				return;
			}

			DesiredColumns = NewColumns;
			DesiredRows = NewRows;
			this->Invalidate(EInvalidateWidgetReason::Layout);
			this->RequestListRefresh();
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			const FVector2D BaseDesiredSize = SCommonTileView<ItemType>::ComputeDesiredSize(LayoutScaleMultiplier);
			if (DesiredColumns <= 0 || DesiredRows <= 0)
			{
				return BaseDesiredSize;
			}

			const FVector2D GridDesiredSize(
				static_cast<float>(DesiredColumns) * this->GetItemWidth(),
				static_cast<float>(DesiredRows) * this->GetItemHeight());
			return FVector2D(
				FMath::Max(BaseDesiredSize.X, GridDesiredSize.X),
				FMath::Max(BaseDesiredSize.Y, GridDesiredSize.Y));
		}

		virtual STableViewBase::FReGenerateResults ReGenerateItems(const FGeometry& MyGeometry) override
		{
			if (DesiredColumns <= 0)
			{
				return SCommonTileView<ItemType>::ReGenerateItems(MyGeometry);
			}

			this->ClearWidgets();

			const TArrayView<const ItemType> Items = this->GetItems();
			if (Items.Num() <= 0)
			{
				return STableViewBase::FReGenerateResults(0, 0, 0, false);
			}

			const FTableViewDimensions TileDimensions = GetTileDimensions();
			const FTableViewDimensions AllottedDimensions(this->Orientation, MyGeometry.GetLocalSize());
			const int32 NumItems = Items.Num();
			const int32 NumItemsPerLine = GetNumItemsPerLine();
			const int32 NumItemsPaddedToFillLastLine = (NumItems % NumItemsPerLine != 0)
				? NumItems + NumItemsPerLine - NumItems % NumItemsPerLine
				: NumItems;

			const double LinesPerScreen = AllottedDimensions.ScrollAxis / TileDimensions.ScrollAxis;
			const double EndOfListOffset = NumItemsPaddedToFillLastLine - NumItemsPerLine * LinesPerScreen;
			const double ClampedScrollOffset = FMath::Clamp(this->CurrentScrollOffset, 0.0, EndOfListOffset);
			const float LayoutScaleMultiplier = MyGeometry.GetAccumulatedLayoutTransform().GetScale();

			FTableViewDimensions DimensionsUsedSoFar(this->Orientation);
			const int32 StartIndex = FMath::Max(0, FMath::FloorToInt32(ClampedScrollOffset / NumItemsPerLine) * NumItemsPerLine);

			this->WidgetGenerator.OnBeginGenerationPass();

			bool bIsAtEndOfList = false;
			bool bHasFilledAvailableArea = false;
			bool bNewLine = true;
			bool bFirstLine = true;
			double NumLinesShownOnScreen = 0;
			int32 NumItemsOnCurrentLine = 0;

			for (int32 ItemIndex = StartIndex; !bHasFilledAvailableArea && ItemIndex < NumItems; ++ItemIndex)
			{
				const ItemType& CurItem = Items[ItemIndex];

				if (bNewLine)
				{
					bNewLine = false;
					NumItemsOnCurrentLine = 0;

					float LineFraction = 1.f;
					if (bFirstLine)
					{
						bFirstLine = false;
						LineFraction -= static_cast<float>(FMath::Fractional(ClampedScrollOffset / NumItemsPerLine));
					}

					DimensionsUsedSoFar.ScrollAxis += TileDimensions.ScrollAxis * LineFraction;

					if (DimensionsUsedSoFar.ScrollAxis > AllottedDimensions.ScrollAxis)
					{
						NumLinesShownOnScreen += FMath::Max(1.0f - ((DimensionsUsedSoFar.ScrollAxis - AllottedDimensions.ScrollAxis) / TileDimensions.ScrollAxis), 0.0f);
					}
					else
					{
						NumLinesShownOnScreen += LineFraction;
					}
				}

				SListView<ItemType>::GenerateWidgetForItem(CurItem, ItemIndex, StartIndex, LayoutScaleMultiplier);

				DimensionsUsedSoFar.LineAxis += TileDimensions.LineAxis;
				++NumItemsOnCurrentLine;

				bIsAtEndOfList = ItemIndex >= NumItems - 1;
				if (NumItemsOnCurrentLine >= NumItemsPerLine)
				{
					DimensionsUsedSoFar.LineAxis = 0;
					bNewLine = true;
				}

				if (bIsAtEndOfList || bNewLine)
				{
					constexpr float FloatPrecisionOffset = 0.001f;
					bHasFilledAvailableArea = DimensionsUsedSoFar.ScrollAxis > AllottedDimensions.ScrollAxis + FloatPrecisionOffset;
				}
			}

			this->WidgetGenerator.OnEndGenerationPass();

			const float TotalGeneratedLineAxisSize = static_cast<float>(FMath::CeilToFloat(NumLinesShownOnScreen) * TileDimensions.ScrollAxis);
			return STableViewBase::FReGenerateResults(ClampedScrollOffset, TotalGeneratedLineAxisSize, NumLinesShownOnScreen, bIsAtEndOfList && !bHasFilledAvailableArea);
		}

	protected:
		virtual int32 GetNumItemsPerLine() const override
		{
			return DesiredColumns > 0 ? DesiredColumns : SCommonTileView<ItemType>::GetNumItemsPerLine();
		}

	private:
		FTableViewDimensions GetTileDimensions() const
		{
			return FTableViewDimensions(this->Orientation, this->GetItemWidth(), this->GetItemHeight());
		}

		int32 DesiredColumns = 0;
		int32 DesiredRows = 0;
	};
}

URpgInventoryAddressTileView::URpgInventoryAddressTileView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDragDrop = true;
	DragDropOperationClass = URpgInventoryDragDropOperation::StaticClass();
}

void URpgInventoryAddressTileView::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyCoordinatorToEntry(EntryWidget);
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgInventoryAddressTileView::SetAddressSlotItems(const TArray<URpgInventoryAddressSlotViewModel*>& InSlots)
{
	ApplyBoundGridSizeToSlate();

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
	for (URpgInventoryAddressSlotViewModel* SlotViewModel : InSlots)
	{
		NewItems.Add(SlotViewModel);
	}

	SetListItems(NewItems);
	RequestRefresh();
}

void URpgInventoryAddressTileView::BindSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	BoundGroupViewModel = InGroupViewModel;
	ApplyBoundGridSizeToSlate();
	RefreshAddressSlotItems();
}

URpgInventoryAddressSlotViewModel* URpgInventoryAddressTileView::GetSelectedAddressSlot() const
{
	return Cast<URpgInventoryAddressSlotViewModel>(GetSelectedItem());
}

bool URpgInventoryAddressTileView::SelectAddressListItem(UObject* Item, APlayerController* OwningPlayer)
{
	if (!Item || !GetListItems().Contains(Item))
	{
		return false;
	}

	SetSelectedItem(Item);
	RequestNavigateToItem(Item);
	if (OwningPlayer)
	{
		if (UWidget* RedirectFocusTarget = FindAddressSlotFocusRedirect(Item))
		{
			RedirectFocusTarget->SetUserFocus(OwningPlayer);
		}
		else
		{
			SetUserFocus(OwningPlayer);
		}
	}
	return true;
}

bool URpgInventoryAddressTileView::SelectBestAddressSlot(APlayerController* OwningPlayer, bool bPreferOccupiedSlot)
{
	const TArray<UObject*>& Items = GetListItems();
	if (Items.IsEmpty())
	{
		return false;
	}

	UObject* DesiredItem = GetSelectedItem();
	if (!DesiredItem || !Items.Contains(DesiredItem))
	{
		DesiredItem = nullptr;
	}

	if (!DesiredItem && bPreferOccupiedSlot)
	{
		for (UObject* Item : Items)
		{
			const URpgInventoryAddressSlotViewModel* AddressSlot = Cast<URpgInventoryAddressSlotViewModel>(Item);
			if (AddressSlot && !AddressSlot->IsEmptySlot())
			{
				DesiredItem = Item;
				break;
			}
		}
	}

	if (!DesiredItem)
	{
		DesiredItem = Items[0];
	}

	return SelectAddressListItem(DesiredItem, OwningPlayer);
}

bool URpgInventoryAddressTileView::SelectAddressSlotByIdentity(FGuid EntryId, int32 GlobalSlotIndex, APlayerController* OwningPlayer)
{
	const TArray<UObject*>& Items = GetListItems();
	if (Items.IsEmpty())
	{
		return false;
	}

	if (EntryId.IsValid())
	{
		for (UObject* Item : Items)
		{
			const URpgInventoryAddressSlotViewModel* AddressSlot = Cast<URpgInventoryAddressSlotViewModel>(Item);
			if (AddressSlot && !AddressSlot->IsEmptySlot() && AddressSlot->GetEntryId() == EntryId)
			{
				return SelectAddressListItem(Item, OwningPlayer);
			}
		}
	}

	if (GlobalSlotIndex != INDEX_NONE)
	{
		for (UObject* Item : Items)
		{
			const URpgInventoryAddressSlotViewModel* AddressSlot = Cast<URpgInventoryAddressSlotViewModel>(Item);
			if (AddressSlot && AddressSlot->GetGlobalSlotIndex() == GlobalSlotIndex)
			{
				return SelectAddressListItem(Item, OwningPlayer);
			}
		}
	}

	return false;
}

void URpgInventoryAddressTileView::ClearAddressSelectionVisual()
{
	const bool bWasSuppressingPanelSelectionNotify = bSuppressPanelSelectionNotify;
	bSuppressPanelSelectionNotify = true;
	ITypedUMGListView<UObject*>::ClearSelection();
	bSuppressPanelSelectionNotify = bWasSuppressingPanelSelectionNotify;
}

bool URpgInventoryAddressTileView::MirrorAddressSlotSelection(UObject* Item)
{
	if (!Item || !GetListItems().Contains(Item))
	{
		return false;
	}

	SetSelectedItem(Item);
	return true;
}

void URpgInventoryAddressTileView::RegisterAddressSlotFocusRedirect(UObject* Item, UWidget* FocusWidget)
{
	if (Item && FocusWidget)
	{
		AddressSlotFocusRedirects.Add(Item, FocusWidget);
	}
}

void URpgInventoryAddressTileView::ClearAddressSlotFocusRedirects()
{
	AddressSlotFocusRedirects.Reset();
}

UWidget* URpgInventoryAddressTileView::GetAddressSlotFocusTarget() const
{
	if (UWidget* SelectedFocusTarget = FindAddressSlotFocusRedirect(GetSelectedItem()))
	{
		return SelectedFocusTarget;
	}

	for (const TPair<TWeakObjectPtr<UObject>, TWeakObjectPtr<UWidget>>& RedirectPair : AddressSlotFocusRedirects)
	{
		if (UWidget* FocusWidget = RedirectPair.Value.Get())
		{
			return FocusWidget;
		}
	}

	return nullptr;
}

void URpgInventoryAddressTileView::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyPanelActiveStateToEntry(EntryWidget);
	}
}

void URpgInventoryAddressTileView::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationId = InPanelId;
}

bool URpgInventoryAddressTileView::QuickTransferSelectedAddressSlot(URpgInventoryManagerComponent* ExplicitTargetInventory)
{
	return DragDropCoordinator && DragDropCoordinator->QuickTransferAddressSlot(GetSelectedAddressSlot(), ExplicitTargetInventory);
}

bool URpgInventoryAddressTileView::QuickSplitSelectedAddressSlot(int32 SplitCount, int32 TargetSlotIndex)
{
	return DragDropCoordinator && DragDropCoordinator->QuickSplitAddressSlot(GetSelectedAddressSlot(), FRpgInventoryGridPlacement(), SplitCount);
}

bool URpgInventoryAddressTileView::UseOrEquipSelectedAddressSlot(int32 StackCount)
{
	return DragDropCoordinator && DragDropCoordinator->UseOrEquipAddressSlot(GetSelectedAddressSlot(), StackCount);
}

bool URpgInventoryAddressTileView::DropSelectedAddressSlot(int32 StackCount, bool bConfirmed)
{
	return DragDropCoordinator && DragDropCoordinator->DropAddressSlot(GetSelectedAddressSlot(), StackCount, bConfirmed);
}

TSharedRef<STableViewBase> URpgInventoryAddressTileView::RebuildListWidget()
{
	TSharedRef<SRpgInventoryFixedGridTileView<UObject*>> TileView = ConstructTileView<SRpgInventoryFixedGridTileView>();
	ApplyBoundGridSizeToSlate();
	return TileView;
}

void URpgInventoryAddressTileView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
	ApplyPanelActiveStateToEntry(EntryWidget);
}

UDragDropOperation* URpgInventoryAddressTileView::HandleListEntryDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UUserWidget& EntryWidget)
{
	UObject* ListItem = GetListObjectFromEntry(EntryWidget);
	URpgInventoryAddressSlotViewModel* SlotViewModel = Cast<URpgInventoryAddressSlotViewModel>(ListItem);
	if (!SlotViewModel || !DragDropCoordinator || (!SlotViewModel->CanDrag() && !SlotViewModel->IsActionbarBindable()))
	{
		return nullptr;
	}

	const FRpgInventoryDragPayload Payload = URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(SlotViewModel);
	if (!URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		return nullptr;
	}

	URpgInventoryDragDropOperation* InventoryOperation = NewObject<URpgInventoryDragDropOperation>(this);
	if (!InventoryOperation)
	{
		return nullptr;
	}

	InventoryOperation->Pivot = DragDropVisualPivot;
	InventoryOperation->Offset = DragDropVisualOffset;
	InventoryOperation->Payload = ListItem;
	if (DragVisualWidget)
	{
		InventoryOperation->DefaultDragVisual = DragVisualWidget;
	}
	else
	{
		const TSubclassOf<UUserWidget> DragVisualClass = DragDropVisualEntryClass ? DragDropVisualEntryClass : EntryWidgetClass;
		if (DragVisualClass)
		{
			UUserWidget* DefaultDragVisual = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass);
			if (URpgInventoryAddressSlotWidget* AddressSlotDragVisual = Cast<URpgInventoryAddressSlotWidget>(DefaultDragVisual))
			{
				AddressSlotDragVisual->SetAddressSlotViewModel(SlotViewModel);
				AddressSlotDragVisual->SetDragDropCoordinator(DragDropCoordinator);
			}

			InventoryOperation->DefaultDragVisual = DefaultDragVisual;
		}
	}

	InventoryOperation->InventoryPayload = Payload;
	return InventoryOperation;
}

TOptional<EItemDropZone> URpgInventoryAddressTileView::HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
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

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(
		Cast<URpgInventoryAddressSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	return DragDropCoordinator->PreviewPayloadDrop(InventoryOperation->InventoryPayload, Target) ? TOptional<EItemDropZone>(DropZone) : NullOpt;
}

FReply URpgInventoryAddressTileView::HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget)
{
	const TSharedPtr<FUMGDragDropOp> NativeOp = DropEvent.GetOperationAs<FUMGDragDropOp>();
	const URpgInventoryDragDropOperation* InventoryOperation = NativeOp.IsValid()
		? Cast<URpgInventoryDragDropOperation>(NativeOp->GetOperation())
		: nullptr;
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return FReply::Unhandled();
	}

	const FRpgInventoryDropTarget Target = URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(
		Cast<URpgInventoryAddressSlotViewModel>(GetListObjectFromEntry(EntryWidget)));
	if (!DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, Target))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled().EndDragDrop();
}

void URpgInventoryAddressTileView::OnSelectionChangedInternal(UObject* FirstSelectedItem)
{
	Super::OnSelectionChangedInternal(FirstSelectedItem);

	if (bSuppressPanelSelectionNotify || !PanelNavigationCoordinator || !FirstSelectedItem)
	{
		return;
	}

	PanelNavigationCoordinator->NotifyAddressPanelSelectionChanged(this, FirstSelectedItem);
}

void URpgInventoryAddressTileView::RefreshAddressSlotItems()
{
	ApplyBoundGridSizeToSlate();
	SetAddressSlotItems(BoundGroupViewModel ? BoundGroupViewModel->GetSlots() : TArray<URpgInventoryAddressSlotViewModel*>());
}

void URpgInventoryAddressTileView::ApplyBoundGridSizeToSlate()
{
	if (!MyTileView.IsValid())
	{
		return;
	}

	TSharedPtr<SRpgInventoryFixedGridTileView<UObject*>> FixedGridTileView = StaticCastSharedPtr<SRpgInventoryFixedGridTileView<UObject*>>(MyTileView);
	if (!FixedGridTileView.IsValid())
	{
		return;
	}

	if (!BoundGroupViewModel)
	{
		FixedGridTileView->SetDesiredGridDimensions(0, 0);
		return;
	}

	const FRpgInventoryGridSize GridSize = BoundGroupViewModel->GetGridSize();
	FixedGridTileView->SetDesiredGridDimensions(GridSize.Width, GridSize.Height);
}

void URpgInventoryAddressTileView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventoryAddressSlotWidget* AddressSlotWidget = Cast<URpgInventoryAddressSlotWidget>(EntryWidget))
	{
		AddressSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventoryAddressTileView::ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventoryAddressSlotWidget* AddressSlotWidget = Cast<URpgInventoryAddressSlotWidget>(EntryWidget))
	{
		AddressSlotWidget->SetInventoryPanelActive(bInventoryPanelActive);
	}
}

UWidget* URpgInventoryAddressTileView::FindAddressSlotFocusRedirect(UObject* Item) const
{
	if (!Item)
	{
		return nullptr;
	}

	const TWeakObjectPtr<UWidget>* RedirectWidget = AddressSlotFocusRedirects.Find(Item);
	return RedirectWidget ? RedirectWidget->Get() : nullptr;
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

	const URpgInventoryAddressSlotViewModel* AddressSlotViewModel = GetAddressSlotViewModel();
	if (!AddressSlotViewModel || !AddressSlotViewModel->ShouldRenderItemVisual())
	{
		return PaintedLayer;
	}

	int32 NextLayer = PaintedLayer + 1;
	if (UTexture2D* IconTexture = AddressSlotViewModel->GetIcon().LoadSynchronous())
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

	const int32 StackCount = AddressSlotViewModel->GetStackCount();
	if (StackCount > 0)
	{
		const FString StackText = FString::Printf(TEXT("%dx"), StackCount);
		const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", 12);
		const FVector2D TextPosition(FMath::Max(0.0f, AllottedGeometry.GetLocalSize().X - 30.0f), FMath::Max(0.0f, AllottedGeometry.GetLocalSize().Y - 18.0f));
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

URpgInventorySlotGroupWidget::URpgInventorySlotGroupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SpatialItemWidgetClass = URpgInventorySpatialItemWidget::StaticClass();
}

void URpgInventorySlotGroupWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	if (SlotTileView)
	{
		SlotTileView->SetDragDropCoordinator(DragDropCoordinator);
	}

	for (URpgInventoryAddressSlotWidget* AddressSlotWidget : RuntimeAddressSlotWidgets)
	{
		if (AddressSlotWidget)
		{
			AddressSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
		}
	}

	for (URpgInventorySpatialItemWidget* SpatialItemWidget : RuntimeSpatialItemWidgets)
	{
		if (SpatialItemWidget)
		{
			SpatialItemWidget->SetDragDropCoordinator(DragDropCoordinator);
		}
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

	if (GroupViewModel)
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(GroupViewModel);
		}
	}

	EnsureRuntimeSlotGridPanel();
	if (RuntimeSlotGridPanel)
	{
		if (SlotTileView)
		{
			SlotTileView->SetDragDropCoordinator(DragDropCoordinator);
			SlotTileView->BindSlotGroupViewModel(GroupViewModel);
			SlotTileView->SetVisibility(ESlateVisibility::Collapsed);
		}

		RebuildRuntimeSlotGrid();
	}
	else if (SlotTileView)
	{
		SlotTileView->SetDragDropCoordinator(DragDropCoordinator);
		SlotTileView->BindSlotGroupViewModel(GroupViewModel);
	}

	RegisterPanelNavigationEntry();
	BP_OnSlotGroupViewModelSet(GroupViewModel);
}

void URpgInventorySlotGroupWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetSlotGroupViewModel(Cast<URpgInventorySlotGroupViewModel>(ListItemObject));
}

void URpgInventorySlotGroupWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	SetSlotGroupViewModel(nullptr);
	RuntimeAddressSlotWidgets.Reset();
	RuntimeSpatialItemWidgets.Reset();
	if (RuntimeSlotGridPanel)
	{
		RuntimeSlotGridPanel->ClearChildren();
	}
	if (RuntimeItemOverlayPanel)
	{
		RuntimeItemOverlayPanel->ClearChildren();
	}
	BP_OnSlotGroupReleased();
}

void URpgInventorySlotGroupWidget::EnsureRuntimeSlotGridPanel()
{
	if (RuntimeSlotGridPanel || !SlotTileView || !WidgetTree)
	{
		return;
	}

	UPanelWidget* ParentPanel = SlotTileView->GetParent();
	if (!ParentPanel)
	{
		return;
	}

	RuntimeGridOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		MakeUniqueObjectName(this, UOverlay::StaticClass(), TEXT("RuntimeGridOverlay")));
	RuntimeSlotGridPanel = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass(),
		MakeUniqueObjectName(this, UUniformGridPanel::StaticClass(), TEXT("RuntimeSlotGridPanel")));
	RuntimeItemOverlayPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		MakeUniqueObjectName(this, UCanvasPanel::StaticClass(), TEXT("RuntimeItemOverlayPanel")));
	if (!RuntimeGridOverlay || !RuntimeSlotGridPanel || !RuntimeItemOverlayPanel)
	{
		return;
	}

	RuntimeSlotGridPanel->SetMinDesiredSlotWidth(SlotTileView->GetEntryWidth());
	RuntimeSlotGridPanel->SetMinDesiredSlotHeight(SlotTileView->GetEntryHeight());
	RuntimeSlotGridPanel->SetSlotPadding(FMargin(
		FMath::Max(0.0f, SlotTileView->GetHorizontalEntrySpacing() * 0.5f),
		FMath::Max(0.0f, SlotTileView->GetVerticalEntrySpacing() * 0.5f)));

	if (UOverlaySlot* GridOverlaySlot = RuntimeGridOverlay->AddChildToOverlay(RuntimeSlotGridPanel))
	{
		GridOverlaySlot->SetHorizontalAlignment(HAlign_Left);
		GridOverlaySlot->SetVerticalAlignment(VAlign_Top);
	}

	RuntimeItemOverlayPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UOverlaySlot* ItemOverlaySlot = RuntimeGridOverlay->AddChildToOverlay(RuntimeItemOverlayPanel))
	{
		ItemOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		ItemOverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	SlotTileView->SetVisibility(ESlateVisibility::Collapsed);
	ApplyCompactRuntimeGridParentSlot(ParentPanel->AddChild(RuntimeGridOverlay));
}

void URpgInventorySlotGroupWidget::RebuildRuntimeSlotGrid()
{
	RuntimeAddressSlotWidgets.Reset();
	RuntimeSpatialItemWidgets.Reset();
	if (!RuntimeSlotGridPanel)
	{
		return;
	}

	RuntimeSlotGridPanel->ClearChildren();
	if (RuntimeItemOverlayPanel)
	{
		RuntimeItemOverlayPanel->ClearChildren();
	}
	if (SlotTileView)
	{
		SlotTileView->ClearAddressSlotFocusRedirects();
	}

	if (!GroupViewModel)
	{
		return;
	}

	const TSubclassOf<UUserWidget> EntryClass = GetAddressSlotEntryWidgetClass();
	if (!EntryClass)
	{
		return;
	}

	if (SlotTileView)
	{
		RuntimeSlotGridPanel->SetMinDesiredSlotWidth(SlotTileView->GetEntryWidth());
		RuntimeSlotGridPanel->SetMinDesiredSlotHeight(SlotTileView->GetEntryHeight());
	}

	const float SlotCellWidth = SlotTileView ? SlotTileView->GetEntryWidth() : 0.0f;
	const float SlotCellHeight = SlotTileView ? SlotTileView->GetEntryHeight() : 0.0f;
	const FRpgInventoryGridSize GridSize = GroupViewModel->GetGridSize();
	const int32 GridWidth = FMath::Max(1, GridSize.Width);
	const TArray<URpgInventoryAddressSlotViewModel*> Slots = GroupViewModel->GetSlots();
	RuntimeAddressSlotWidgets.Reserve(Slots.Num());

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		URpgInventoryAddressSlotViewModel* SlotViewModel = Slots[SlotIndex];
		if (!SlotViewModel)
		{
			continue;
		}

		UUserWidget* EntryWidget = CreateWidget<UUserWidget>(GetWorld(), EntryClass);
		URpgInventoryAddressSlotWidget* AddressSlotWidget = Cast<URpgInventoryAddressSlotWidget>(EntryWidget);
		if (!AddressSlotWidget)
		{
			continue;
		}

		AddressSlotWidget->SetAddressSlotViewModel(SlotViewModel);
		AddressSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
		AddressSlotWidget->SetInventoryPanelActive(true);
		AddressSlotWidget->SetSelectionMirrorTileView(SlotTileView);

		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			MakeUniqueObjectName(this, USizeBox::StaticClass(), TEXT("RuntimeSlotSizeBox")));
		if (!SlotSizeBox)
		{
			continue;
		}

		if (SlotCellWidth > 0.0f)
		{
			SlotSizeBox->SetWidthOverride(SlotCellWidth);
		}
		if (SlotCellHeight > 0.0f)
		{
			SlotSizeBox->SetHeightOverride(SlotCellHeight);
		}
		SlotSizeBox->AddChild(AddressSlotWidget);

		const FRpgInventoryGridPlacement SlotPlacement = SlotViewModel->GetPlacement();
		const int32 Column = SlotPlacement.IsValid() ? SlotPlacement.X : SlotIndex % GridWidth;
		const int32 Row = SlotPlacement.IsValid() ? SlotPlacement.Y : SlotIndex / GridWidth;

		if (UUniformGridSlot* GridSlot = RuntimeSlotGridPanel->AddChildToUniformGrid(SlotSizeBox, Row, Column))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Left);
			GridSlot->SetVerticalAlignment(VAlign_Top);
		}

		if (SlotTileView)
		{
			SlotTileView->RegisterAddressSlotFocusRedirect(SlotViewModel, AddressSlotWidget);
		}

		RuntimeAddressSlotWidgets.Add(AddressSlotWidget);
	}

	RebuildRuntimeItemOverlay(SlotCellWidth, SlotCellHeight);
}

void URpgInventorySlotGroupWidget::RebuildRuntimeItemOverlay(float SlotCellWidth, float SlotCellHeight)
{
	if (!RuntimeItemOverlayPanel || !GroupViewModel || !SpatialItemWidgetClass || SlotCellWidth <= 0.0f || SlotCellHeight <= 0.0f)
	{
		return;
	}

	const float HorizontalSpacing = SlotTileView ? FMath::Max(0.0f, SlotTileView->GetHorizontalEntrySpacing()) : 0.0f;
	const float VerticalSpacing = SlotTileView ? FMath::Max(0.0f, SlotTileView->GetVerticalEntrySpacing()) : 0.0f;
	const float SlotStrideX = SlotCellWidth + HorizontalSpacing;
	const float SlotStrideY = SlotCellHeight + VerticalSpacing;

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (!SlotViewModel || !SlotViewModel->ShouldRenderItemVisual() || !SlotViewModel->GetItemPlacement().IsValid())
		{
			continue;
		}

		URpgInventorySpatialItemWidget* SpatialItemWidget = CreateWidget<URpgInventorySpatialItemWidget>(GetWorld(), SpatialItemWidgetClass);
		if (!SpatialItemWidget)
		{
			continue;
		}

		SpatialItemWidget->SetAddressSlotViewModel(SlotViewModel);
		SpatialItemWidget->SetDragDropCoordinator(DragDropCoordinator);
		SpatialItemWidget->SetInventoryPanelActive(true);
		SpatialItemWidget->SetSelectionMirrorTileView(SlotTileView);

		const int32 OccupiedWidth = FMath::Max(1, SlotViewModel->GetItemOccupiedWidth());
		const int32 OccupiedHeight = FMath::Max(1, SlotViewModel->GetItemOccupiedHeight());
		const FRpgInventoryGridPlacement ItemPlacement = SlotViewModel->GetItemPlacement();
		if (UCanvasPanelSlot* ItemCanvasSlot = RuntimeItemOverlayPanel->AddChildToCanvas(SpatialItemWidget))
		{
			ItemCanvasSlot->SetAutoSize(false);
			ItemCanvasSlot->SetPosition(FVector2D(ItemPlacement.X * SlotStrideX, ItemPlacement.Y * SlotStrideY));
			ItemCanvasSlot->SetSize(FVector2D(
				OccupiedWidth * SlotCellWidth + FMath::Max(0, OccupiedWidth - 1) * HorizontalSpacing,
				OccupiedHeight * SlotCellHeight + FMath::Max(0, OccupiedHeight - 1) * VerticalSpacing));
			ItemCanvasSlot->SetZOrder(10);
		}

		RuntimeSpatialItemWidgets.Add(SpatialItemWidget);
	}
}

void URpgInventorySlotGroupWidget::RegisterPanelNavigationEntry()
{
	if (!PanelNavigationCoordinator || !SlotTileView || !GroupViewModel)
	{
		return;
	}

	URpgInventoryManagerComponent* Inventory = ResolveGroupInventory();
	if (!Inventory)
	{
		return;
	}

	PanelNavigationCoordinator->RegisterInventoryAddressPanel(MakePanelNavigationId(), SlotTileView, Inventory);
}

TSubclassOf<UUserWidget> URpgInventorySlotGroupWidget::GetAddressSlotEntryWidgetClass() const
{
	return SlotTileView ? SlotTileView->GetEntryWidgetClass() : TSubclassOf<UUserWidget>();
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

URpgInventorySlotGroupListView::URpgInventorySlotGroupListView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgInventorySlotGroupListView::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyCoordinatorToEntry(EntryWidget);
	}
}

void URpgInventorySlotGroupListView::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationIdPrefix = InPanelIdPrefix;

	for (UUserWidget* EntryWidget : GetDisplayedEntryWidgets())
	{
		ApplyPanelNavigationToEntry(EntryWidget);
	}
}

void URpgInventorySlotGroupListView::SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups)
{
	const TArray<UObject*>& CurrentItems = GetListItems();
	if (CurrentItems.Num() == InGroups.Num())
	{
		bool bSameItems = true;
		for (int32 Index = 0; Index < InGroups.Num(); ++Index)
		{
			if (CurrentItems[Index] != InGroups[Index])
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
	NewItems.Reserve(InGroups.Num());
	for (URpgInventorySlotGroupViewModel* GroupViewModel : InGroups)
	{
		NewItems.Add(GroupViewModel);
	}

	SetListItems(NewItems);
	RequestRefresh();
}

void URpgInventorySlotGroupListView::NativeOnEntryGenerated(UUserWidget* EntryWidget)
{
	Super::NativeOnEntryGenerated(EntryWidget);
	ApplyCoordinatorToEntry(EntryWidget);
	ApplyPanelNavigationToEntry(EntryWidget);
}

void URpgInventorySlotGroupListView::ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventorySlotGroupWidget* GroupWidget = Cast<URpgInventorySlotGroupWidget>(EntryWidget))
	{
		GroupWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupListView::ApplyPanelNavigationToEntry(UUserWidget* EntryWidget) const
{
	if (URpgInventorySlotGroupWidget* GroupWidget = Cast<URpgInventorySlotGroupWidget>(EntryWidget))
	{
		GroupWidget->SetPanelNavigationCoordinator(PanelNavigationCoordinator, PanelNavigationIdPrefix);
	}
}
