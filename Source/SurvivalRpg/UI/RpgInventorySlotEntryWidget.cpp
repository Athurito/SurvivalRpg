#include "RpgInventorySlotEntryWidget.h"

#include "MVVMSubsystem.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySlotEntryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventorySlotEntryWidget, Log, All);

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

bool URpgInventorySlotEntryWidget::HandleEntryQuickTransfer(URpgInventoryManagerComponent* ExplicitTargetInventory)
{
	return DragDropCoordinator && EntryViewModel && DragDropCoordinator->QuickTransferEntry(EntryViewModel, ExplicitTargetInventory);
}

bool URpgInventorySlotEntryWidget::HandleEntryQuickSplit(int32 SplitCount, int32 TargetSlotIndex)
{
	return DragDropCoordinator && EntryViewModel && DragDropCoordinator->QuickSplitEntry(EntryViewModel, FRpgInventoryGridPlacement(), SplitCount);
}

bool URpgInventorySlotEntryWidget::HandleEntryUseOrEquip(int32 StackCount)
{
	return DragDropCoordinator && EntryViewModel && DragDropCoordinator->UseOrEquipEntry(EntryViewModel, StackCount);
}

bool URpgInventorySlotEntryWidget::HandleEntryDrop(int32 StackCount, bool bConfirmed)
{
	return DragDropCoordinator && EntryViewModel && DragDropCoordinator->DropEntry(EntryViewModel, StackCount, bConfirmed);
}

void URpgInventorySlotEntryWidget::SetInventoryPanelActive(bool bInInventoryPanelActive)
{
	if (bInventoryPanelActive == bInInventoryPanelActive)
	{
		return;
	}

	bInventoryPanelActive = bInInventoryPanelActive;
	BP_OnInventoryEntrySelectionChanged(bEntrySelected && bInventoryPanelActive);
	RefreshDragDropVisualState();
}

void URpgInventorySlotEntryWidget::RefreshDragDropVisualState()
{
	const bool bShowFocusedState = bEntrySelected && bInventoryPanelActive;
	CurrentDragDropVisualState = DragDropCoordinator
		? DragDropCoordinator->GetInventoryEntryVisualState(EntryViewModel, bShowFocusedState)
		: (bShowFocusedState ? ERpgInventorySlotDragVisualState::Focused : ERpgInventorySlotDragVisualState::Normal);

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

	if (EntryViewModel)
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(EntryViewModel);
		}
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

	BP_OnInventoryEntrySelectionChanged(false);
	BP_OnInventoryEntryReleased();
	RefreshDragDropVisualState();
}

void URpgInventorySlotEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	bEntrySelected = bIsSelected;
	BP_OnInventoryEntrySelectionChanged(bIsSelected && bInventoryPanelActive);
	RefreshDragDropVisualState();
}

void URpgInventorySlotEntryWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	HandleEntryAccept();
}

FReply URpgInventorySlotEntryWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && HandleEntryUseOrEquip())
	{
		return FReply::Handled();
	}

	if (TryHandleModifiedLeftMouseButtonDown(InMouseEvent, true))
	{
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URpgInventorySlotEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && HandleEntryUseOrEquip())
	{
		return FReply::Handled();
	}

	if (TryHandleModifiedLeftMouseButtonDown(InMouseEvent, false))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
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

bool URpgInventorySlotEntryWidget::TryHandleModifiedLeftMouseButtonDown(const FPointerEvent& InMouseEvent, bool bLogFailure)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return false;
	}

	const bool bWantsQuickTransfer = InMouseEvent.IsControlDown();
	const bool bWantsQuickSplit = !bWantsQuickTransfer && InMouseEvent.IsShiftDown();
	if (!bWantsQuickTransfer && !bWantsQuickSplit)
	{
		return false;
	}

	const bool bHandledShortcut = bWantsQuickTransfer ? HandleEntryQuickTransfer() : HandleEntryQuickSplit();
	if (!bHandledShortcut && bLogFailure)
	{
		URpgInventoryManagerComponent* SourceInventory = EntryViewModel ? EntryViewModel->GetInventoryManager() : nullptr;
		URpgInventoryManagerComponent* TargetInventory = bWantsQuickTransfer && DragDropCoordinator ? DragDropCoordinator->ResolveQuickTransferTarget(SourceInventory) : nullptr;
		UE_LOG(LogRpgInventorySlotEntryWidget, Warning,
			TEXT("%s shortcut ignored: Widget=%s Coordinator=%s Entry=%s Source=%s Target=%s Item=%s Stack=%d CanDrag=%s"),
			bWantsQuickTransfer ? TEXT("Ctrl+Click quick transfer") : TEXT("Shift+Click quick split"),
			*GetNameSafe(this),
			*GetNameSafe(DragDropCoordinator.Get()),
			*GetNameSafe(EntryViewModel.Get()),
			*GetNameSafe(SourceInventory),
			*GetNameSafe(TargetInventory),
			*GetNameSafe(EntryViewModel ? EntryViewModel->GetItemInstance() : nullptr),
			EntryViewModel ? EntryViewModel->GetStackCount() : 0,
			EntryViewModel && EntryViewModel->CanDrag() ? TEXT("true") : TEXT("false"));
	}

	return bHandledShortcut;
}
