#include "RpgInventoryControllerActionsWidget.h"

#include "Input/CommonUIInputTypes.h"
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
	}

	PanelNavigator = NewPanelNavigator;
	DragDropCoordinator = NewDragDropCoordinator;

	if (PanelNavigator)
	{
		PanelNavigator->OnActivePanelChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePanelChanged);
	}

	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s controller coordinators set. PanelNavigator=%s DragDropCoordinator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator),
		*GetNameSafe(DragDropCoordinator));
}

void URpgInventoryControllerActionsWidget::RegisterInventoryControllerActionBindings()
{
	if (!IsActivated())
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Verbose, TEXT("%s skipped inventory action binding registration because the widget is not active."),
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
	RegisterActionRow(QuickTransferInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleQuickTransferAction));
	RegisterActionRow(QuickSplitInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleQuickSplitAction));
	RegisterActionRow(UseOrEquipInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleUseOrEquipAction));
	RegisterActionRow(DropInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleDropAction));
	RegisterActionRow(BackInputAction, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBackAction));

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

UWidget* URpgInventoryControllerActionsWidget::NativeGetDesiredFocusTarget() const
{
	if (PanelNavigator)
	{
		if (URpgInventoryTileView* ActiveTileView = PanelNavigator->GetActiveTileView())
		{
			return ActiveTileView;
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
		if (URpgInventoryTileView* ActiveTileView = PanelNavigator->GetActiveTileView())
		{
			ActiveTileView->QuickTransferSelectedEntry();
		}
		else
		{
			UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s quick transfer ignored because there is no active inventory tile view."),
				*GetNameSafe(this));
		}
	}
}

void URpgInventoryControllerActionsWidget::HandleQuickSplitAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s quick split action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator)
	{
		if (URpgInventoryTileView* ActiveTileView = PanelNavigator->GetActiveTileView())
		{
			ActiveTileView->QuickSplitSelectedEntry();
		}
		else
		{
			UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s quick split ignored because there is no active inventory tile view."),
				*GetNameSafe(this));
		}
	}
}

void URpgInventoryControllerActionsWidget::HandleUseOrEquipAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s use/equip action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator)
	{
		if (URpgInventoryTileView* ActiveTileView = PanelNavigator->GetActiveTileView())
		{
			ActiveTileView->UseOrEquipSelectedEntry();
		}
		else
		{
			UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s use/equip ignored because there is no active inventory tile view."),
				*GetNameSafe(this));
		}
	}
}

void URpgInventoryControllerActionsWidget::HandleDropAction()
{
	UE_LOG(LogRpgInventoryControllerActionsWidget, Log, TEXT("%s drop action fired. PanelNavigator=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PanelNavigator));

	if (PanelNavigator)
	{
		if (URpgInventoryTileView* ActiveTileView = PanelNavigator->GetActiveTileView())
		{
			ActiveTileView->DropSelectedEntry();
		}
		else
		{
			UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s drop ignored because there is no active inventory tile view."),
				*GetNameSafe(this));
		}
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
	BP_OnInventoryActivePanelChanged(PanelId, PanelIndex, TileView);
}

void URpgInventoryControllerActionsWidget::RegisterActionRow(const FDataTableRowHandle& ActionRow, const FSimpleDelegate& Delegate)
{
	if (!IsActionRowValid(ActionRow))
	{
		UE_LOG(LogRpgInventoryControllerActionsWidget, Warning, TEXT("%s skipped invalid inventory controller action row %s."),
			*GetNameSafe(this),
			*DescribeActionRow(ActionRow));
		return;
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
	return ActionRow.DataTable != nullptr && ActionRow.RowName != NAME_None;
}
