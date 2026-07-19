#include "RpgInventoryControllerActionsWidget.h"

#include "CommonUITypes.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryTileView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryControllerActionsWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryControllerActionsWidget, Log, All);

namespace
{
FString DescribeActionRow(const FDataTableRowHandle& ActionRow)
{
	return FString::Printf(TEXT("%s::%s"), *GetNameSafe(ActionRow.DataTable), *ActionRow.RowName.ToString());
}

bool DoesActionRowResolve(const FDataTableRowHandle& ActionRow)
{
	return ActionRow.DataTable != nullptr &&
		ActionRow.RowName != NAME_None &&
		ActionRow.DataTable->GetRowNames().Contains(ActionRow.RowName);
}

void FillMissingActionRow(FDataTableRowHandle& ActionRow, UDataTable* DataTable, FName RowName)
{
	if (!DoesActionRowResolve(ActionRow))
	{
		ActionRow.DataTable = DataTable;
		ActionRow.RowName = RowName;
	}
}
}

URpgInventoryControllerActionsWidget::URpgInventoryControllerActionsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputConfig = ERpgWidgetInputMode::Menu;
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> URpgInventoryControllerActionsWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void URpgInventoryControllerActionsWidget::SetInventoryControllerCoordinators(URpgInventoryPanelNavigationCoordinator* InPanelNavigator, URpgInventoryDragDropCoordinator* InDragDropCoordinator)
{
	URpgInventoryPanelNavigationCoordinator* NewPanelNavigator = InPanelNavigator ? InPanelNavigator : PanelNavigator.Get();
	URpgInventoryDragDropCoordinator* NewDragDropCoordinator = InDragDropCoordinator ? InDragDropCoordinator : DragDropCoordinator.Get();

	if ((!InPanelNavigator || !InDragDropCoordinator) && (PanelNavigator || DragDropCoordinator))
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s preserved existing inventory controller coordinator(s) for a partial assignment. RequestedPanelNavigator=%s RequestedDragDropCoordinator=%s CurrentPanelNavigator=%s CurrentDragDropCoordinator=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InPanelNavigator),
			*GetNameSafe(InDragDropCoordinator),
			*GetNameSafe(PanelNavigator),
			*GetNameSafe(DragDropCoordinator));
	}

	if (PanelNavigator == NewPanelNavigator && DragDropCoordinator == NewDragDropCoordinator)
	{
		return;
	}

	if (PanelNavigator)
	{
		PanelNavigator->OnActivePanelChanged.RemoveDynamic(this, &ThisClass::HandleActivePanelChanged);
		PanelNavigator->OnActiveSelectionChanged.RemoveDynamic(this, &ThisClass::HandleActiveSelectionChanged);
	}
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(
			this,
			&ThisClass::HandleHeldPayloadChanged);
	}

	PanelNavigator = NewPanelNavigator;
	DragDropCoordinator = NewDragDropCoordinator;

	if (PanelNavigator)
	{
		PanelNavigator->OnActivePanelChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePanelChanged);
		PanelNavigator->OnActiveSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveSelectionChanged);
	}
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleHeldPayloadChanged);
	}
	RefreshInventoryActionBindingVisibility();

	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s controller coordinators set. PanelNavigator=%s DragDropCoordinator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator),
		*GetNameSafe(DragDropCoordinator));
}

void URpgInventoryControllerActionsWidget::RegisterInventoryControllerActionBindings()
{
	EnsureDefaultInventoryControllerActionRows();

	if (!IsActivated())
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Verbose, TEXT("%s skipped inventory action binding registration because the widget is not active."),
			*GetNameSafe(this));
		return;
	}
	if (!GetOwningLocalPlayer())
	{
		// Preview and automation worlds can activate authored widgets without a LocalPlayer. Their navigation
		// coordinators remain testable, but CommonUI has no input router on which action rows could be registered.
		UE_LOG(
			LogRpgInventoryControllerActionsWidget,
			Verbose,
			TEXT("%s skipped inventory action binding registration because it has no owning LocalPlayer."),
			*GetNameSafe(this));
		return;
	}

	if (InventoryActionBindings.Num() > 0)
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Verbose, TEXT("%s skipped inventory action binding registration because %d bindings already exist."),
			*GetNameSafe(this),
			InventoryActionBindings.Num());
		return;
	}

	RegisterActionRow(PreviousPanelInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandlePreviousPanelAction));
	RegisterActionRow(NextPanelInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleNextPanelAction));
	QuickTransferActionBinding = RegisterActionRow(QuickTransferInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleQuickTransferAction));
	QuickSplitActionBinding = RegisterActionRow(QuickSplitInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleQuickSplitAction));
	const FCommonInputActionDataBase* UseOrEquipActionData = UseOrEquipInputAction.GetRow<FCommonInputActionDataBase>(TEXT("InventoryUseOrEquip"));
	if (UseOrEquipActionData && UseOrEquipActionData->IsKeyBoundToInputActionData(EKeys::F))
	{
		UseOrEquipActionBinding = RegisterActionRow(
			UseOrEquipInputAction,
			FSimpleDelegate::CreateUObject(this, &ThisClass::HandleUseOrEquipAction));
	}
	else
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning,
			TEXT("%s did not register the legacy UseOrEquip row because it is not bound to F; native F handling remains active and avoids the old duplicate-E conflict."),
			*GetNameSafe(this));
	}
	DropActionBinding = RegisterActionRow(DropInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleDropAction));
	RegisterActionRow(BackInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBackAction));
	RefreshInventoryActionBindingVisibility();

	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s registered %d inventory controller action binding(s). Previous=%s Next=%s Transfer=%s Split=%s UseOrEquip=%s Drop=%s Back=%s"),
		*GetNameSafe(this),
		InventoryActionBindings.Num(),
		*DescribeActionRow(PreviousPanelInputAction),
		*DescribeActionRow(NextPanelInputAction),
		*DescribeActionRow(QuickTransferInputAction),
		*DescribeActionRow(QuickSplitInputAction),
		*DescribeActionRow(UseOrEquipInputAction),
		*DescribeActionRow(DropInputAction),
		*DescribeActionRow(BackInputAction));
}

void URpgInventoryControllerActionsWidget::UnregisterInventoryControllerActionBindings()
{
	for (FUIActionBindingHandle& BindingHandle : InventoryActionBindings)
	{
		if (BindingHandle.IsValid())
		{
			BindingHandle.Unregister();
		}
	}

	InventoryActionBindings.Reset();
	UseOrEquipActionBinding = FUIActionBindingHandle();
	QuickTransferActionBinding = FUIActionBindingHandle();
	QuickSplitActionBinding = FUIActionBindingHandle();
	DropActionBinding = FUIActionBindingHandle();
}

bool URpgInventoryControllerActionsWidget::RefreshInventoryControllerFocus()
{
	if (PanelNavigator && PanelNavigator->RefreshActivePanelFocus())
	{
		RequestRefreshFocus();
		return true;
	}

	UE_LOG(LogRpgInventoryControllerActionsWidget, Verbose, TEXT("%s could not refresh inventory controller focus. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));
	return false;
}

void URpgInventoryControllerActionsWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	RegisterInventoryControllerActionBindings();
	RefreshInventoryControllerFocus();
}

void URpgInventoryControllerActionsWidget::NativeOnDeactivated()
{
	UnregisterInventoryControllerActionBindings();

	Super::NativeOnDeactivated();
}

bool URpgInventoryControllerActionsWidget::NativeOnHandleBackAction()
{
	if (HandleInventoryBackAction())
	{
		return true;
	}

	return Super::NativeOnHandleBackAction();
}

FReply URpgInventoryControllerActionsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Gamepad_LeftTrigger &&
		PanelNavigator &&
		PanelNavigator->RequestContextMenuForActiveSelection())
	{
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::R && DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		return DragDropCoordinator->ToggleInteractionRotation() ? FReply::Handled() : FReply::Unhandled();
	}
	if (InKeyEvent.GetKey() == EKeys::F && !UseOrEquipActionBinding.IsValid())
	{
		HandleUseOrEquipAction();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UWidget* URpgInventoryControllerActionsWidget::NativeGetDesiredFocusTarget() const
{
	if (PanelNavigator)
	{
		if (UWidget* ActiveFocusTarget = PanelNavigator->GetActiveFocusTarget())
		{
			return ActiveFocusTarget;
		}
	}

	return Super::NativeGetDesiredFocusTarget();
}

void URpgInventoryControllerActionsWidget::HandlePreviousPanelAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s previous panel action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator && PanelNavigator->ActivatePreviousPanel())
	{
		RequestRefreshFocus();
	}
}

void URpgInventoryControllerActionsWidget::HandleNextPanelAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s next panel action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator && PanelNavigator->ActivateNextPanel())
	{
		RequestRefreshFocus();
	}
}

void URpgInventoryControllerActionsWidget::HandleQuickTransferAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s quick transfer action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator)
	{
		if (PanelNavigator->QuickTransferActiveSelection())
		{
			return;
		}

		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s quick transfer ignored because the active panel selection does not support it."),
			*GetNameSafe(this));
	}
}

void URpgInventoryControllerActionsWidget::HandleQuickSplitAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s quick split action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator)
	{
		if (PanelNavigator->QuickSplitActiveSelection())
		{
			return;
		}

		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s quick split ignored because the active panel selection does not support it."),
			*GetNameSafe(this));
	}
}

void URpgInventoryControllerActionsWidget::HandleUseOrEquipAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s use/equip action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator)
	{
		if (PanelNavigator->UseOrEquipActiveSelection())
		{
			return;
		}

		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s use/equip ignored because the active panel selection does not support it."),
			*GetNameSafe(this));
	}
}

void URpgInventoryControllerActionsWidget::HandleDropAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s drop action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator)
	{
		if (PanelNavigator->DropActiveSelection())
		{
			return;
		}

		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s drop ignored because the active panel selection does not support it."),
			*GetNameSafe(this));
	}
}

void URpgInventoryControllerActionsWidget::HandleBackAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s back action fired. DragDropCoordinator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(DragDropCoordinator));

	if (!HandleInventoryBackAction())
	{
		DeactivateWidget();
	}
}

void URpgInventoryControllerActionsWidget::HandleActivePanelChanged(FName PanelId, int32 PanelIndex, URpgInventoryTileView* TileView, URpgInventoryManagerComponent* Inventory)
{
	RefreshInventoryActionBindingVisibility();
	BP_OnInventoryActivePanelChanged(PanelId, PanelIndex, TileView);
}

void URpgInventoryControllerActionsWidget::HandleActiveSelectionChanged()
{
	RefreshInventoryActionBindingVisibility();
}

void URpgInventoryControllerActionsWidget::HandleHeldPayloadChanged(
	bool bHasPayload,
	const FRpgInventoryDragPayload& Payload)
{
	RefreshInventoryActionBindingVisibility();
}

void URpgInventoryControllerActionsWidget::RefreshInventoryActionBindingVisibility()
{
	const bool bShowTransfer = bDisplayInventoryActionsInActionBar && PanelNavigator && PanelNavigator->CanQuickTransferActiveSelection();
	const bool bShowSplit = bDisplayInventoryActionsInActionBar && PanelNavigator && PanelNavigator->CanQuickSplitActiveSelection();
	const bool bShowUseOrEquip = bDisplayInventoryActionsInActionBar && PanelNavigator && PanelNavigator->CanUseOrEquipActiveSelection();
	const bool bShowDrop = bDisplayInventoryActionsInActionBar && PanelNavigator && PanelNavigator->CanDropActiveSelection();

	if (QuickTransferActionBinding.IsValid())
	{
		QuickTransferActionBinding.SetDisplayInActionBar(bShowTransfer);
	}
	if (QuickSplitActionBinding.IsValid())
	{
		QuickSplitActionBinding.SetDisplayInActionBar(bShowSplit);
	}
	if (UseOrEquipActionBinding.IsValid())
	{
		UseOrEquipActionBinding.SetDisplayInActionBar(bShowUseOrEquip);
	}
	if (DropActionBinding.IsValid())
	{
		DropActionBinding.SetDisplayInActionBar(bShowDrop);
	}
}

FUIActionBindingHandle URpgInventoryControllerActionsWidget::RegisterActionRow(
	const FDataTableRowHandle& ActionRow,
	const FSimpleDelegate& Delegate)
{
	if (!IsActionRowValid(ActionRow))
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s skipped invalid inventory controller action row %s."),
			*GetNameSafe(this),
			*DescribeActionRow(ActionRow));
		return FUIActionBindingHandle();
	}

	FBindUIActionArgs BindArgs(ActionRow, bDisplayInventoryActionsInActionBar, Delegate);
	FUIActionBindingHandle BindingHandle = RegisterUIActionBinding(BindArgs);
	if (BindingHandle.IsValid())
	{
		InventoryActionBindings.Add(BindingHandle);
	}
	else
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s failed to register inventory controller action row %s."),
			*GetNameSafe(this),
			*DescribeActionRow(ActionRow));
	}
	return BindingHandle;
}

void URpgInventoryControllerActionsWidget::EnsureDefaultInventoryControllerActionRows()
{
	if (IsActionRowValid(PreviousPanelInputAction) &&
		IsActionRowValid(NextPanelInputAction) &&
		IsActionRowValid(QuickTransferInputAction) &&
		IsActionRowValid(QuickSplitInputAction) &&
		IsActionRowValid(UseOrEquipInputAction) &&
		IsActionRowValid(DropInputAction))
	{
		return;
	}

	UDataTable* ActionTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/SurvivalRpg/UI/Input/DT_RpgUIActions_Inventory.DT_RpgUIActions_Inventory"));
	if (!ActionTable)
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s could not load default inventory controller action table."), *GetNameSafe(this));
		return;
	}

	FillMissingActionRow(PreviousPanelInputAction, ActionTable, TEXT("UI.Inventory.PreviousPanel"));
	FillMissingActionRow(NextPanelInputAction, ActionTable, TEXT("UI.Inventory.NextPanel"));
	FillMissingActionRow(QuickTransferInputAction, ActionTable, TEXT("UI.Inventory.QuickTransfer"));
	FillMissingActionRow(QuickSplitInputAction, ActionTable, TEXT("UI.Inventory.QuickSplit"));
	FillMissingActionRow(UseOrEquipInputAction, ActionTable, TEXT("UI.Inventory.UseOrEquip"));
	FillMissingActionRow(DropInputAction, ActionTable, TEXT("UI.Inventory.Drop"));
}

bool URpgInventoryControllerActionsWidget::HandleInventoryBackAction()
{
	if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		DragDropCoordinator->CancelHold();
		return true;
	}

	return false;
}

bool URpgInventoryControllerActionsWidget::IsActionRowValid(const FDataTableRowHandle& ActionRow)
{
	return DoesActionRowResolve(ActionRow);
}
