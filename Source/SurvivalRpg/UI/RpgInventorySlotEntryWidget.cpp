#include "RpgInventorySlotEntryWidget.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySlotEntryWidget)

URpgInventorySlotEntryWidget::URpgInventorySlotEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgInventorySlotEntryWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
}

bool URpgInventorySlotEntryWidget::HandleEntryAccept()
{
	return DragDropCoordinator && EntryViewModel && DragDropCoordinator->HandleInventoryEntryAccept(EntryViewModel);
}

void URpgInventorySlotEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	EntryViewModel = Cast<URpgInventoryEntryViewModel>(ListItemObject);
	BP_OnInventoryEntryViewModelSet(EntryViewModel);
}

void URpgInventorySlotEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();

	EntryViewModel = nullptr;
	BP_OnInventoryEntryReleased();
}

void URpgInventorySlotEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	BP_OnInventoryEntrySelectionChanged(bIsSelected);
}

void URpgInventorySlotEntryWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	HandleEntryAccept();
}
