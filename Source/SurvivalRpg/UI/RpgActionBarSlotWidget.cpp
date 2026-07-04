#include "RpgActionBarSlotWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarSlotWidget)

URpgActionBarSlotWidget::URpgActionBarSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgActionBarSlotWidget::SetActionBarSlotViewModel(URpgActionBarSlotViewModel* InSlotViewModel)
{
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	SlotViewModel = InSlotViewModel;
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	if (SlotViewModel)
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(SlotViewModel);
		}
	}

	BP_OnActionBarSlotViewModelSet(SlotViewModel);
	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
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

bool URpgActionBarSlotWidget::HandleSlotAccept()
{
	return DragDropCoordinator && DragDropCoordinator->HasHeldPayload() && DragDropCoordinator->CommitDrop(MakeDropTarget());
}

void URpgActionBarSlotWidget::RefreshDragDropVisualState()
{
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;
	if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		CurrentDragDropVisualState = DragDropCoordinator->PreviewDrop(MakeDropTarget())
			? ERpgInventorySlotDragVisualState::ValidTarget
			: ERpgInventorySlotDragVisualState::InvalidTarget;
	}
	else if (bSlotSelected)
	{
		CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Focused;
	}

	BP_OnActionBarSlotDragDropStateChanged(CurrentDragDropVisualState);
}

void URpgActionBarSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetActionBarSlotViewModel(Cast<URpgActionBarSlotViewModel>(ListItemObject));
}

void URpgActionBarSlotWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	SetActionBarSlotViewModel(nullptr);
	bSlotSelected = false;
	BP_OnActionBarSlotSelectionChanged(false);
	BP_OnActionBarSlotReleased();
}

void URpgActionBarSlotWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	bSlotSelected = bIsSelected;
	BP_OnActionBarSlotSelectionChanged(bSlotSelected);
	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	HandleSlotAccept();
}

bool URpgActionBarSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!DragDropCoordinator || !InventoryOperation)
	{
		return false;
	}

	return DragDropCoordinator->CommitPayloadToTarget(InventoryOperation->InventoryPayload, MakeDropTarget());
}

void URpgActionBarSlotWidget::HandleSlotViewModelChanged(URpgActionBarSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == SlotViewModel)
	{
		if (SlotViewModel)
		{
			if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
			{
				View->SetViewModelByClass(SlotViewModel);
			}
		}

		BP_OnActionBarSlotViewModelSet(SlotViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgActionBarSlotWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

FRpgInventoryDropTarget URpgActionBarSlotWidget::MakeDropTarget() const
{
	return URpgInventoryDragDropCoordinator::MakeActionBarSlotTargetFromViewModel(SlotViewModel);
}
