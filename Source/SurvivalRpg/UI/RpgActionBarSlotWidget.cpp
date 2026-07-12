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

void URpgActionBarSlotWidget::SetActionBarPanelActive(bool bInActionBarPanelActive)
{
	if (bActionBarPanelActive == bInActionBarPanelActive)
	{
		return;
	}

	bActionBarPanelActive = bInActionBarPanelActive;
	RefreshDragDropVisualState();
}

bool URpgActionBarSlotWidget::HandleSlotAccept()
{
	return DragDropCoordinator && DragDropCoordinator->HasHeldPayload() && DragDropCoordinator->CommitDrop(MakeDropTarget());
}

void URpgActionBarSlotWidget::RefreshDragDropVisualState()
{
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;
	if (bHasExternalPreviewState)
	{
		CurrentDragDropVisualState = ExternalPreviewState;
	}
	else if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		CurrentDragDropVisualState = DragDropCoordinator->PreviewDrop(MakeDropTarget())
			? ERpgInventorySlotDragVisualState::ValidTarget
			: ERpgInventorySlotDragVisualState::InvalidTarget;
	}
	else if (bSlotSelected && bActionBarPanelActive)
	{
		CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Focused;
	}

	BP_OnActionBarSlotDragDropStateChanged(CurrentDragDropVisualState);
}

bool URpgActionBarSlotWidget::PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	if (!DragDropCoordinator || !URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	const bool bCanDrop = DragDropCoordinator->UpdateInteractionPreview(Payload, MakeDropTarget());
	bHasExternalPreviewState = true;
	ExternalPreviewState = bCanDrop
		? ERpgInventorySlotDragVisualState::ValidTarget
		: ERpgInventorySlotDragVisualState::InvalidTarget;
	RefreshDragDropVisualState();
	return bCanDrop;
}

bool URpgActionBarSlotWidget::CommitPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	ClearExternalPreviewPayload();
	return DragDropCoordinator && DragDropCoordinator->CommitPayloadToTarget(Payload, MakeDropTarget());
}

void URpgActionBarSlotWidget::ClearExternalPreviewPayload()
{
	if (!bHasExternalPreviewState)
	{
		return;
	}

	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	RefreshDragDropVisualState();
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

bool URpgActionBarSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

bool URpgActionBarSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

void URpgActionBarSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
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
