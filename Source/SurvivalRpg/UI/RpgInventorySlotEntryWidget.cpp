#include "RpgInventorySlotEntryWidget.h"

#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySlotEntryWidget)

URpgInventorySlotEntryWidget::URpgInventorySlotEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgInventorySlotEntryWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
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

bool URpgInventorySlotEntryWidget::HandleEntryAccept()
{
	return DragDropCoordinator && EntryViewModel && DragDropCoordinator->HandleInventoryEntryAccept(EntryViewModel);
}

void URpgInventorySlotEntryWidget::RefreshDragDropVisualState()
{
	CurrentDragDropVisualState = DragDropCoordinator
		? DragDropCoordinator->GetInventoryEntryVisualState(EntryViewModel, bEntrySelected)
		: (bEntrySelected ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal);

	BP_OnInventoryEntryDragDropStateChanged(CurrentDragDropVisualState);
}

void URpgInventorySlotEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryViewModelChanged);
	}

	EntryViewModel = Cast<URpgInventoryEntryViewModel>(ListItemObject);
	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.AddUniqueDynamic(this, &ThisClass::HandleEntryViewModelChanged);
	}

	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->SetViewModelByClass(EntryViewModel);
	}

	BP_OnInventoryEntryViewModelSet(EntryViewModel);
	RefreshDragDropVisualState();
}

void URpgInventorySlotEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();

	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryViewModelChanged);
	}

	EntryViewModel = nullptr;
	bEntrySelected = false;
	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->SetViewModelByClass(nullptr);
	}

	BP_OnInventoryEntryReleased();
	RefreshDragDropVisualState();
}

void URpgInventorySlotEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	bEntrySelected = bIsSelected;
	BP_OnInventoryEntrySelectionChanged(bIsSelected);
	RefreshDragDropVisualState();
}

void URpgInventorySlotEntryWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	HandleEntryAccept();
}

void URpgInventorySlotEntryWidget::HandleEntryViewModelChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel)
{
	if (ChangedEntryViewModel == EntryViewModel)
	{
		BP_OnInventoryEntryViewModelSet(ChangedEntryViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgInventorySlotEntryWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}
