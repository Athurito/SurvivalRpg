#include "RpgInventorySlotEntryWidget.h"

#include "MVVMSubsystem.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySlotEntryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventorySlotEntryWidget, Log, All);

const FName URpgInventorySlotEntryWidget::InventoryEntryViewModelSourceName(
	TEXT("RpgInventoryEntryViewModel"));

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

	InjectInventoryEntryViewModelIntoMvvm();
	BP_OnInventoryEntryViewModelSet(EntryViewModel);
	RefreshDragDropVisualState();
}

void URpgInventorySlotEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();

	if (EntryViewModel)
	{
		EntryViewModel->OnEntryChanged.RemoveDynamic(this, &ThisClass::HandleEntryViewModelChanged);
	}

	EntryViewModel = nullptr;
	InjectInventoryEntryViewModelIntoMvvm();
	bEntrySelected = false;
	bInventoryPanelActive = true;

	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}
	DragDropCoordinator = nullptr;

	BP_OnInventoryEntryViewModelSet(nullptr);
	BP_OnInventoryEntrySelectionChanged(false);
	RefreshDragDropVisualState();
	BP_OnInventoryEntryReleased();
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

bool URpgInventorySlotEntryWidget::InjectInventoryEntryViewModelIntoMvvm()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		if (GetClass() != StaticClass())
		{
			UE_LOG(
				LogRpgInventorySlotEntryWidget,
				Error,
				TEXT("%s has no compiled MVVM view. Author one optional manual %s source for inventory entry data."),
				*GetNameSafe(this),
				*InventoryEntryViewModelSourceName.ToString());
		}
		return false;
	}

	const FMVVMViewClass_Source* CompiledSource = ViewClass->GetSources().FindByPredicate(
		[](const FMVVMViewClass_Source& Candidate)
		{
			return Candidate.IsViewModel() &&
				Candidate.GetName() == InventoryEntryViewModelSourceName;
		});
	if (!CompiledSource ||
		!CompiledSource->CanBeSet() ||
		!CompiledSource->IsOptional() ||
		CompiledSource->GetSourceClass() != URpgInventoryEntryViewModel::StaticClass())
	{
		UE_LOG(
			LogRpgInventorySlotEntryWidget,
			Error,
			TEXT("%s requires one settable optional manual MVVM source named %s with type RpgInventoryEntryViewModel."),
			*GetNameSafe(this),
			*InventoryEntryViewModelSourceName.ToString());
		return false;
	}

	if (View->GetViewModel(InventoryEntryViewModelSourceName).GetObject() == EntryViewModel)
	{
		return true;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	if (EntryViewModel)
	{
		ViewModelInterface.SetObject(EntryViewModel);
		ViewModelInterface.SetInterface(EntryViewModel.Get());
	}

	if (!View->SetViewModel(InventoryEntryViewModelSourceName, ViewModelInterface))
	{
		UE_LOG(
			LogRpgInventorySlotEntryWidget,
			Error,
			TEXT("%s failed to inject its inventory entry VM into MVVM source %s."),
			*GetNameSafe(this),
			*InventoryEntryViewModelSourceName.ToString());
		return false;
	}

	return View->GetViewModel(InventoryEntryViewModelSourceName).GetObject() == EntryViewModel;
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
