#include "RpgInventoryPanelNavigationCoordinator.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryCarrySlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryUiGeometry.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryPanelNavigationCoordinator)

URpgInventoryPanelNavigationCoordinator* URpgInventoryPanelNavigationCoordinator::CreateInventoryPanelNavigationCoordinator(UObject* WorldContextObject, APlayerController* InPlayerController, URpgInventoryDragDropCoordinator* InDragDropCoordinator)
{
	UObject* Outer = InPlayerController ? Cast<UObject>(InPlayerController) : WorldContextObject;
	if (!Outer)
	{
		return nullptr;
	}

	URpgInventoryPanelNavigationCoordinator* Coordinator = NewObject<URpgInventoryPanelNavigationCoordinator>(Outer);
	if (Coordinator)
	{
		Coordinator->Initialize(InPlayerController, InDragDropCoordinator);
	}
	return Coordinator;
}

void URpgInventoryPanelNavigationCoordinator::Initialize(APlayerController* InPlayerController, URpgInventoryDragDropCoordinator* InDragDropCoordinator)
{
	PlayerController = InPlayerController;
	DragDropCoordinator = InDragDropCoordinator;
	ClearPanels();
}

void URpgInventoryPanelNavigationCoordinator::ClearPanels()
{
	for (FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (Panel.SpatialGridWidget)
		{
			Panel.SpatialGridWidget->SetPanelNavigationCoordinator(nullptr, NAME_None);
			Panel.SpatialGridWidget->ClearSelectionVisual();
		}

		if (Panel.ActionBarTileView)
		{
			Panel.ActionBarTileView->SetPanelNavigationCoordinator(nullptr, NAME_None);
			Panel.ActionBarTileView->SetActionBarPanelActive(false);
			Panel.ActionBarTileView->ClearActionBarSelectionVisual();
		}

		if (Panel.EquipmentSlotWidget)
		{
			Panel.EquipmentSlotWidget->SetPanelNavigationCoordinator(nullptr);
		}

		if (Panel.CarrySlotWidget)
		{
			Panel.CarrySlotWidget->SetPanelNavigationCoordinator(nullptr, NAME_None);
			Panel.CarrySlotWidget->SetInventoryPanelActive(false);
			Panel.CarrySlotWidget->ClearExternalPreviewPayload();
		}
	}

	Panels.Reset();
	ActivePanelIndex = INDEX_NONE;
	if (!bPanelRefreshInProgress)
	{
		RetainedPanelMemories.Reset();
		RetainedActivePanelId = NAME_None;
	}
}

void URpgInventoryPanelNavigationCoordinator::BeginPanelRefresh()
{
	if (bPanelRefreshInProgress)
	{
		return;
	}

	SaveActivePanelSelection();
	RetainedActivePanelId = GetActivePanelId();
	RetainedPanelMemories.Reset();
	for (const FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (!Panel.PanelId.IsNone())
		{
			RetainedPanelMemories.Add(Panel.PanelId, Panel);
		}
	}

	bPanelRefreshInProgress = true;
	ClearPanels();
}

void URpgInventoryPanelNavigationCoordinator::EndPanelRefresh()
{
	if (!bPanelRefreshInProgress)
	{
		return;
	}

	bPanelRefreshInProgress = false;
	const FName DesiredPanelId = RetainedActivePanelId;
	RetainedActivePanelId = NAME_None;

	bool bRestoredPanel = !DesiredPanelId.IsNone() && ActivatePanelById(DesiredPanelId);
	if (!bRestoredPanel && Panels.Num() > 0)
	{
		bRestoredPanel = ActivatePanelByIndex(0);
	}

	RetainedPanelMemories.Reset();
}

void URpgInventoryPanelNavigationCoordinator::RegisterSpatialInventoryPanel(FName PanelId, URpgInventorySpatialGridWidget* SpatialGridWidget, URpgInventoryManagerComponent* Inventory)
{
	if (PanelId.IsNone() || !SpatialGridWidget || !Inventory)
	{
		return;
	}

	for (FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (Panel.PanelId == PanelId)
		{
			Panel.SpatialGridWidget = SpatialGridWidget;
			Panel.ActionBarTileView = nullptr;
			Panel.EquipmentSlotWidget = nullptr;
			Panel.CarrySlotWidget = nullptr;
			Panel.Inventory = Inventory;
			SpatialGridWidget->SetPanelNavigationCoordinator(this, PanelId);
			UpdatePanelSelectionMemory(Panel);
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.SpatialGridWidget = SpatialGridWidget;
	NewPanel.ActionBarTileView = nullptr;
	NewPanel.EquipmentSlotWidget = nullptr;
	NewPanel.CarrySlotWidget = nullptr;
	NewPanel.Inventory = Inventory;
	SpatialGridWidget->SetPanelNavigationCoordinator(this, PanelId);
	ApplyRetainedPanelMemory(NewPanel);
	if (!RetainedPanelMemories.Contains(PanelId))
	{
		UpdatePanelSelectionMemory(NewPanel);
	}
	SpatialGridWidget->SetInventoryPanelActive(false);

	if (ActivePanelIndex == INDEX_NONE && !bPanelRefreshInProgress)
	{
		ActivatePanelByIndex(0);
	}
}

void URpgInventoryPanelNavigationCoordinator::RegisterActionBarPanel(FName PanelId, URpgActionBarTileView* TileView)
{
	if (PanelId.IsNone() || !TileView)
	{
		return;
	}

	for (FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (Panel.PanelId == PanelId)
		{
			Panel.SpatialGridWidget = nullptr;
			Panel.ActionBarTileView = TileView;
			Panel.EquipmentSlotWidget = nullptr;
			Panel.CarrySlotWidget = nullptr;
			Panel.Inventory = nullptr;
			TileView->SetPanelNavigationCoordinator(this, PanelId);
			UpdatePanelSelectionMemory(Panel);
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.SpatialGridWidget = nullptr;
	NewPanel.ActionBarTileView = TileView;
	NewPanel.EquipmentSlotWidget = nullptr;
	NewPanel.CarrySlotWidget = nullptr;
	NewPanel.Inventory = nullptr;
	TileView->SetPanelNavigationCoordinator(this, PanelId);
	ApplyRetainedPanelMemory(NewPanel);
	if (!RetainedPanelMemories.Contains(PanelId))
	{
		UpdatePanelSelectionMemory(NewPanel);
	}
	TileView->SetActionBarPanelActive(false);

	if (ActivePanelIndex == INDEX_NONE && !bPanelRefreshInProgress)
	{
		ActivatePanelByIndex(0);
	}
}

void URpgInventoryPanelNavigationCoordinator::RegisterEquipmentPanel(FName PanelId, URpgEquipmentSlotWidget* EquipmentSlotWidget)
{
	if (PanelId.IsNone() || !EquipmentSlotWidget)
	{
		return;
	}

	for (FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (Panel.PanelId == PanelId)
		{
			Panel.SpatialGridWidget = nullptr;
			Panel.ActionBarTileView = nullptr;
			Panel.EquipmentSlotWidget = EquipmentSlotWidget;
			Panel.CarrySlotWidget = nullptr;
			Panel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
			EquipmentSlotWidget->SetPanelNavigationCoordinator(this);
			UpdatePanelSelectionMemory(Panel);
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.SpatialGridWidget = nullptr;
	NewPanel.ActionBarTileView = nullptr;
	NewPanel.EquipmentSlotWidget = EquipmentSlotWidget;
	NewPanel.CarrySlotWidget = nullptr;
	NewPanel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
	EquipmentSlotWidget->SetPanelNavigationCoordinator(this);
	ApplyRetainedPanelMemory(NewPanel);
	if (!RetainedPanelMemories.Contains(PanelId))
	{
		UpdatePanelSelectionMemory(NewPanel);
	}

	if (ActivePanelIndex == INDEX_NONE && !bPanelRefreshInProgress)
	{
		ActivatePanelByIndex(0);
	}
}

void URpgInventoryPanelNavigationCoordinator::RegisterCarrySlotPanel(
	FName PanelId,
	URpgInventoryCarrySlotWidget* CarrySlotWidget)
{
	if (PanelId.IsNone() || !CarrySlotWidget)
	{
		return;
	}

	for (FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (Panel.PanelId != PanelId)
		{
			continue;
		}

		Panel.SpatialGridWidget = nullptr;
		Panel.ActionBarTileView = nullptr;
		Panel.EquipmentSlotWidget = nullptr;
		Panel.CarrySlotWidget = CarrySlotWidget;
		Panel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
		CarrySlotWidget->SetPanelNavigationCoordinator(this, PanelId);
		UpdatePanelSelectionMemory(Panel);
		ApplyActivePanelState();
		return;
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.CarrySlotWidget = CarrySlotWidget;
	NewPanel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
	CarrySlotWidget->SetPanelNavigationCoordinator(this, PanelId);
	ApplyRetainedPanelMemory(NewPanel);
	if (!RetainedPanelMemories.Contains(PanelId))
	{
		UpdatePanelSelectionMemory(NewPanel);
	}
	CarrySlotWidget->SetInventoryPanelActive(false);

	if (ActivePanelIndex == INDEX_NONE && !bPanelRefreshInProgress)
	{
		ActivatePanelByIndex(0);
	}
}

void URpgInventoryPanelNavigationCoordinator::NotifyActionBarPanelSelectionChanged(URpgActionBarTileView* TileView, UObject* SelectedItem)
{
	if (bSuppressPanelSelectionNotifications || !TileView || !SelectedItem)
	{
		return;
	}

	const int32 PanelIndex = FindPanelIndexForActionBarTileView(TileView);
	if (!IsValidPanelIndex(PanelIndex))
	{
		return;
	}

	const bool bPanelChanged = ActivePanelIndex != PanelIndex;
	if (bPanelChanged)
	{
		SaveActivePanelSelection();
		ActivePanelIndex = PanelIndex;
		ApplyActivePanelState();
	}

	FRpgInventoryPanelNavigationEntry& ActivePanel = Panels[PanelIndex];
	UpdatePanelSelectionMemory(ActivePanel);
	OnActiveSelectionChanged.Broadcast();

	if (bPanelChanged)
	{
		BroadcastActivePanelChanged(ActivePanel);
	}
}

void URpgInventoryPanelNavigationCoordinator::NotifySpatialPanelSelectionChanged(URpgInventorySpatialGridWidget* SpatialGridWidget, UObject* SelectedItem)
{
	if (bSuppressPanelSelectionNotifications || !SpatialGridWidget)
	{
		return;
	}

	const int32 PanelIndex = FindPanelIndexForSpatialGridWidget(SpatialGridWidget);
	if (!IsValidPanelIndex(PanelIndex))
	{
		return;
	}

	const bool bPanelChanged = ActivePanelIndex != PanelIndex;
	if (bPanelChanged)
	{
		SaveActivePanelSelection();
		ActivePanelIndex = PanelIndex;
		ApplyActivePanelState();
	}

	FRpgInventoryPanelNavigationEntry& ActivePanel = Panels[PanelIndex];
	UpdatePanelSelectionMemory(ActivePanel);
	UpdateFocusedInventoryForActivePanel(ActivePanel);
	OnActiveSelectionChanged.Broadcast();

	if (bPanelChanged)
	{
		BroadcastActivePanelChanged(ActivePanel);
	}
}

void URpgInventoryPanelNavigationCoordinator::NotifyCarrySlotFocused(
	URpgInventoryCarrySlotWidget* CarrySlotWidget)
{
	if (bSuppressPanelSelectionNotifications || !CarrySlotWidget)
	{
		return;
	}

	const int32 PanelIndex = FindPanelIndexForCarrySlotWidget(CarrySlotWidget);
	if (!IsValidPanelIndex(PanelIndex))
	{
		return;
	}

	const bool bPanelChanged = ActivePanelIndex != PanelIndex;
	if (bPanelChanged)
	{
		SaveActivePanelSelection();
		ActivePanelIndex = PanelIndex;
		ApplyActivePanelState();
	}

	FRpgInventoryPanelNavigationEntry& ActivePanel = Panels[PanelIndex];
	UpdatePanelSelectionMemory(ActivePanel);
	UpdateFocusedInventoryForActivePanel(ActivePanel);
	OnActiveSelectionChanged.Broadcast();

	if (bPanelChanged)
	{
		BroadcastActivePanelChanged(ActivePanel);
	}
}

void URpgInventoryPanelNavigationCoordinator::NotifyEquipmentSlotFocused(
	URpgEquipmentSlotWidget* EquipmentSlotWidget)
{
	if (bSuppressPanelSelectionNotifications || !EquipmentSlotWidget)
	{
		return;
	}

	const int32 PanelIndex = FindPanelIndexForEquipmentSlotWidget(EquipmentSlotWidget);
	if (!IsValidPanelIndex(PanelIndex))
	{
		return;
	}

	const bool bPanelChanged = ActivePanelIndex != PanelIndex;
	if (bPanelChanged)
	{
		SaveActivePanelSelection();
		ActivePanelIndex = PanelIndex;
		ApplyActivePanelState();
	}

	FRpgInventoryPanelNavigationEntry& ActivePanel = Panels[PanelIndex];
	UpdatePanelSelectionMemory(ActivePanel);
	UpdateFocusedInventoryForActivePanel(ActivePanel);
	OnActiveSelectionChanged.Broadcast();

	if (bPanelChanged)
	{
		BroadcastActivePanelChanged(ActivePanel);
	}
}


bool URpgInventoryPanelNavigationCoordinator::ActivatePanelById(FName PanelId)
{
	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (Panels[PanelIndex].PanelId == PanelId)
		{
			return ActivatePanelByIndex(PanelIndex);
		}
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::ActivatePanelByIndex(int32 PanelIndex)
{
	if (!IsValidPanelIndex(PanelIndex))
	{
		return false;
	}

	SaveActivePanelSelection();
	ActivePanelIndex = PanelIndex;

	bSuppressPanelSelectionNotifications = true;
	ApplyActivePanelState();

	FRpgInventoryPanelNavigationEntry& ActivePanel = Panels[ActivePanelIndex];
	if (!ActivePanel.SpatialGridWidget && !ActivePanel.ActionBarTileView &&
		!ActivePanel.EquipmentSlotWidget && !ActivePanel.CarrySlotWidget)
	{
		bSuppressPanelSelectionNotifications = false;
		return false;
	}

	if (!RestorePanelSelection(ActivePanel))
	{
		if (ActivePanel.SpatialGridWidget)
		{
			ActivePanel.SpatialGridWidget->SelectBestCell(PlayerController, true);
		}
		else if (ActivePanel.ActionBarTileView)
		{
			ActivePanel.ActionBarTileView->SelectBestActionBarSlot(PlayerController);
		}
		else if (ActivePanel.EquipmentSlotWidget && PlayerController)
		{
			ActivePanel.EquipmentSlotWidget->SetUserFocus(PlayerController);
		}
		else if (ActivePanel.CarrySlotWidget && PlayerController)
		{
			ActivePanel.CarrySlotWidget->SetUserFocus(PlayerController);
		}
	}
	bSuppressPanelSelectionNotifications = false;

	UpdatePanelSelectionMemory(ActivePanel);
	UpdateFocusedInventoryForActivePanel(ActivePanel);
	BroadcastActivePanelChanged(ActivePanel);
	return true;
}

bool URpgInventoryPanelNavigationCoordinator::ActivateNextPanel()
{
	if (Panels.IsEmpty())
	{
		return false;
	}

	const int32 NextIndex = ActivePanelIndex == INDEX_NONE
		? 0
		: (ActivePanelIndex + 1) % Panels.Num();
	return ActivatePanelByIndex(NextIndex);
}

bool URpgInventoryPanelNavigationCoordinator::ActivatePreviousPanel()
{
	if (Panels.IsEmpty())
	{
		return false;
	}

	const int32 PreviousIndex = ActivePanelIndex == INDEX_NONE
		? 0
		: (ActivePanelIndex - 1 + Panels.Num()) % Panels.Num();
	return ActivatePanelByIndex(PreviousIndex);
}

bool URpgInventoryPanelNavigationCoordinator::RefreshActivePanelFocus()
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	FRpgInventoryPanelNavigationEntry& ActivePanel = Panels[ActivePanelIndex];
	if (!ActivePanel.SpatialGridWidget && !ActivePanel.ActionBarTileView &&
		!ActivePanel.EquipmentSlotWidget && !ActivePanel.CarrySlotWidget)
	{
		return false;
	}

	bSuppressPanelSelectionNotifications = true;
	ApplyActivePanelState();
	if (!RestorePanelSelection(ActivePanel))
	{
		if (ActivePanel.SpatialGridWidget)
		{
			ActivePanel.SpatialGridWidget->SelectBestCell(PlayerController, true);
		}
		else if (ActivePanel.ActionBarTileView)
		{
			ActivePanel.ActionBarTileView->SelectBestActionBarSlot(PlayerController);
		}
		else if (ActivePanel.EquipmentSlotWidget && PlayerController)
		{
			ActivePanel.EquipmentSlotWidget->SetUserFocus(PlayerController);
		}
		else if (ActivePanel.CarrySlotWidget && PlayerController)
		{
			ActivePanel.CarrySlotWidget->SetUserFocus(PlayerController);
		}
	}
	bSuppressPanelSelectionNotifications = false;

	UpdatePanelSelectionMemory(ActivePanel);
	UpdateFocusedInventoryForActivePanel(ActivePanel);
	return true;
}

FName URpgInventoryPanelNavigationCoordinator::GetActivePanelId() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].PanelId : NAME_None;
}

URpgInventorySpatialGridWidget* URpgInventoryPanelNavigationCoordinator::GetActiveSpatialGridWidget() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].SpatialGridWidget.Get() : nullptr;
}

URpgActionBarTileView* URpgInventoryPanelNavigationCoordinator::GetActiveActionBarTileView() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].ActionBarTileView.Get() : nullptr;
}

URpgEquipmentSlotWidget* URpgInventoryPanelNavigationCoordinator::GetActiveEquipmentSlotWidget() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].EquipmentSlotWidget.Get() : nullptr;
}

URpgInventoryCarrySlotWidget* URpgInventoryPanelNavigationCoordinator::GetActiveCarrySlotWidget() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].CarrySlotWidget.Get() : nullptr;
}

UWidget* URpgInventoryPanelNavigationCoordinator::GetActiveFocusTarget() const
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return nullptr;
	}

	if (Panels[ActivePanelIndex].SpatialGridWidget)
	{
		return Panels[ActivePanelIndex].SpatialGridWidget.Get();
	}

	if (Panels[ActivePanelIndex].ActionBarTileView)
	{
		return Panels[ActivePanelIndex].ActionBarTileView.Get();
	}

	if (Panels[ActivePanelIndex].CarrySlotWidget)
	{
		return Panels[ActivePanelIndex].CarrySlotWidget.Get();
	}

	return Panels[ActivePanelIndex].EquipmentSlotWidget.Get();
}

URpgInventoryManagerComponent* URpgInventoryPanelNavigationCoordinator::GetActiveInventory() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].Inventory.Get() : nullptr;
}

bool URpgInventoryPanelNavigationCoordinator::CanQuickTransferActiveSelection() const
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	const FRpgInventoryPanelNavigationEntry& Panel = Panels[ActivePanelIndex];
	if (const URpgInventorySpatialGridWidget* Grid = Panel.SpatialGridWidget)
	{
		if (!DragDropCoordinator)
		{
			return false;
		}
		if (URpgInventoryAddressSlotViewModel* AddressSlot = Grid->GetSelectedAddressSlot())
		{
			return DragDropCoordinator->CanQuickTransferAddressSlot(AddressSlot);
		}
		return DragDropCoordinator->CanQuickTransferEntry(Grid->GetSelectedEntryViewModel());
	}
	if (const URpgInventoryCarrySlotWidget* CarrySlot = Panel.CarrySlotWidget)
	{
		return DragDropCoordinator &&
			DragDropCoordinator->CanQuickTransferAddressSlot(CarrySlot->GetAddressSlotViewModel());
	}
	return Panel.EquipmentSlotWidget && DragDropCoordinator &&
		DragDropCoordinator->CanQuickTransferPlayerItem(Panel.EquipmentSlotWidget->GetRepresentedItem());
}

bool URpgInventoryPanelNavigationCoordinator::CanQuickSplitActiveSelection() const
{
	if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		return DragDropCoordinator->CanToggleInteractionRotation();
	}
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	const FRpgInventoryPanelNavigationEntry& Panel = Panels[ActivePanelIndex];
	if (const URpgInventorySpatialGridWidget* Grid = Panel.SpatialGridWidget)
	{
		return Grid->GetSelectedContextActions().Contains(ERpgInventoryContextAction::Split);
	}
	if (const URpgInventoryCarrySlotWidget* CarrySlot = Panel.CarrySlotWidget)
	{
		return CarrySlot->GetAddressContextActions().Contains(ERpgInventoryContextAction::Split);
	}
	return false;
}

bool URpgInventoryPanelNavigationCoordinator::CanUseOrEquipActiveSelection() const
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	const FRpgInventoryPanelNavigationEntry& Panel = Panels[ActivePanelIndex];
	if (const URpgInventorySpatialGridWidget* Grid = Panel.SpatialGridWidget)
	{
		const TArray<ERpgInventoryContextAction> Actions = Grid->GetSelectedContextActions();
		return Actions.Contains(ERpgInventoryContextAction::Use) ||
			Actions.Contains(ERpgInventoryContextAction::EquipAndActivate);
	}
	if (const URpgInventoryCarrySlotWidget* CarrySlot = Panel.CarrySlotWidget)
	{
		const TArray<ERpgInventoryContextAction> Actions = CarrySlot->GetAddressContextActions();
		return Actions.Contains(ERpgInventoryContextAction::Use) ||
			Actions.Contains(ERpgInventoryContextAction::EquipAndActivate);
	}
	if (const URpgEquipmentSlotWidget* EquipmentSlot = Panel.EquipmentSlotWidget)
	{
		const URpgInventoryItemInstance* Item = EquipmentSlot->GetRepresentedItem();
		return DragDropCoordinator && Item &&
			DragDropCoordinator->CanExecuteContextAction(
				EquipmentSlot->GetResolvedEquipmentSlot(),
				Item->GetItemId(),
				ERpgInventoryContextAction::Unequip);
	}
	return false;
}

bool URpgInventoryPanelNavigationCoordinator::CanDropActiveSelection() const
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	const FRpgInventoryPanelNavigationEntry& Panel = Panels[ActivePanelIndex];
	if (const URpgInventorySpatialGridWidget* Grid = Panel.SpatialGridWidget)
	{
		return Grid->GetSelectedContextActions().Contains(ERpgInventoryContextAction::Drop);
	}
	if (const URpgInventoryCarrySlotWidget* CarrySlot = Panel.CarrySlotWidget)
	{
		return CarrySlot->GetAddressContextActions().Contains(ERpgInventoryContextAction::Drop);
	}
	if (const URpgEquipmentSlotWidget* EquipmentSlot = Panel.EquipmentSlotWidget)
	{
		const URpgInventoryItemInstance* Item = EquipmentSlot->GetRepresentedItem();
		return DragDropCoordinator && Item &&
			DragDropCoordinator->CanExecuteContextAction(
				EquipmentSlot->GetResolvedEquipmentSlot(),
				Item->GetItemId(),
				ERpgInventoryContextAction::Drop);
	}
	return false;
}

bool URpgInventoryPanelNavigationCoordinator::QuickTransferActiveSelection()
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	if (URpgInventorySpatialGridWidget* SpatialGridWidget = Panels[ActivePanelIndex].SpatialGridWidget)
	{
		return SpatialGridWidget->QuickTransferSelectedCell();
	}
	if (URpgEquipmentSlotWidget* EquipmentSlotWidget = Panels[ActivePanelIndex].EquipmentSlotWidget)
	{
		return DragDropCoordinator && DragDropCoordinator->QuickTransferPlayerItem(EquipmentSlotWidget->GetRepresentedItem());
	}
	if (URpgInventoryCarrySlotWidget* CarrySlotWidget = Panels[ActivePanelIndex].CarrySlotWidget)
	{
		return DragDropCoordinator &&
			DragDropCoordinator->QuickTransferAddressSlot(CarrySlotWidget->GetAddressSlotViewModel());
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::QuickSplitActiveSelection(int32 SplitCount)
{
	if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		return DragDropCoordinator->CanToggleInteractionRotation() &&
			DragDropCoordinator->ToggleInteractionRotation();
	}

	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	if (URpgInventorySpatialGridWidget* SpatialGridWidget = Panels[ActivePanelIndex].SpatialGridWidget)
	{
		return SpatialGridWidget->QuickSplitSelectedCell(SplitCount);
	}
	if (URpgInventoryCarrySlotWidget* CarrySlotWidget = Panels[ActivePanelIndex].CarrySlotWidget)
	{
		URpgInventoryAddressSlotViewModel* AddressSlot = CarrySlotWidget->GetAddressSlotViewModel();
		const int32 ResolvedSplitCount = SplitCount > 0
			? SplitCount
			: (AddressSlot ? FMath::Max(1, AddressSlot->GetStackCount() / 2) : 0);
		return DragDropCoordinator && AddressSlot &&
			DragDropCoordinator->CanExecuteContextAction(
				AddressSlot,
				ERpgInventoryContextAction::Split) &&
			DragDropCoordinator->QuickSplitAddressSlot(AddressSlot, FRpgInventoryGridPlacement(), ResolvedSplitCount);
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::UseOrEquipActiveSelection(int32 StackCount)
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	if (URpgInventorySpatialGridWidget* SpatialGridWidget = Panels[ActivePanelIndex].SpatialGridWidget)
	{
		return SpatialGridWidget->UseOrEquipSelectedCell(StackCount);
	}

	if (URpgEquipmentSlotWidget* EquipmentSlotWidget = Panels[ActivePanelIndex].EquipmentSlotWidget)
	{
		return EquipmentSlotWidget->HandleClearAssignment();
	}
	if (URpgInventoryCarrySlotWidget* CarrySlotWidget = Panels[ActivePanelIndex].CarrySlotWidget)
	{
		return DragDropCoordinator &&
			DragDropCoordinator->UseOrEquipAddressSlot(CarrySlotWidget->GetAddressSlotViewModel(), StackCount);
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::DropActiveSelection(int32 StackCount, bool bConfirmed)
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	if (URpgInventorySpatialGridWidget* SpatialGridWidget = Panels[ActivePanelIndex].SpatialGridWidget)
	{
		return SpatialGridWidget->DropSelectedCell(StackCount, bConfirmed);
	}
	if (URpgInventoryCarrySlotWidget* CarrySlotWidget = Panels[ActivePanelIndex].CarrySlotWidget)
	{
		return CarrySlotWidget->RequestAddressItemDrop(
			StackCount,
			bConfirmed);
	}
	if (URpgEquipmentSlotWidget* EquipmentSlotWidget = Panels[ActivePanelIndex].EquipmentSlotWidget)
	{
		URpgInventoryItemInstance* Item = EquipmentSlotWidget->GetRepresentedItem();
		if (!Item)
		{
			return false;
		}

		// Equipment slots represent whole item instances, so StackCount is intentionally not applied.
		return bConfirmed
			? DragDropCoordinator &&
				DragDropCoordinator->DropEquipmentItem(
					EquipmentSlotWidget->GetResolvedEquipmentSlot(),
					Item->GetItemId(),
					true)
			: EquipmentSlotWidget->ExecuteEquipmentContextAction(
				ERpgInventoryContextAction::Drop,
				Item->GetItemId());
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::RequestContextMenuForActiveSelection()
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	FRpgInventoryPanelNavigationEntry& Panel = Panels[ActivePanelIndex];
	FVector2D AbsoluteScreenAnchor;
	bool bHasSelectionAnchor = false;
	if (Panel.SpatialGridWidget)
	{
		bHasSelectionAnchor =
			Panel.SpatialGridWidget->TryGetSelectedContextMenuScreenAnchor(AbsoluteScreenAnchor);
	}
	else if (Panel.CarrySlotWidget)
	{
		bHasSelectionAnchor = RpgInventoryUiGeometry::TryResolveAbsoluteCenter(
			Panel.CarrySlotWidget->GetCachedGeometry(),
			AbsoluteScreenAnchor);
	}
	else if (Panel.EquipmentSlotWidget)
	{
		bHasSelectionAnchor = RpgInventoryUiGeometry::TryResolveAbsoluteCenter(
			Panel.EquipmentSlotWidget->GetCachedGeometry(),
			AbsoluteScreenAnchor);
	}
	else
	{
		return false;
	}

	if (!bHasSelectionAnchor &&
		!RpgInventoryUiGeometry::TryResolvePlayerScreenCenter(
			PlayerController,
			AbsoluteScreenAnchor))
	{
		return false;
	}

	if (Panel.SpatialGridWidget)
	{
		return Panel.SpatialGridWidget->RequestContextMenuForSelectedCell(AbsoluteScreenAnchor);
	}
	if (Panel.CarrySlotWidget)
	{
		return Panel.CarrySlotWidget->RequestAddressContextMenu(AbsoluteScreenAnchor);
	}
	if (Panel.EquipmentSlotWidget)
	{
		return Panel.EquipmentSlotWidget->RequestEquipmentContextMenu(AbsoluteScreenAnchor);
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::IsValidPanelIndex(int32 PanelIndex) const
{
	if (!Panels.IsValidIndex(PanelIndex))
	{
		return false;
	}

	const FRpgInventoryPanelNavigationEntry& Panel = Panels[PanelIndex];
	return (Panel.SpatialGridWidget || Panel.ActionBarTileView ||
		Panel.EquipmentSlotWidget || Panel.CarrySlotWidget) &&
		(Panel.Inventory || Panel.ActionBarTileView || Panel.EquipmentSlotWidget || Panel.CarrySlotWidget);
}

void URpgInventoryPanelNavigationCoordinator::SaveActivePanelSelection()
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return;
	}

	UpdatePanelSelectionMemory(Panels[ActivePanelIndex]);
}

void URpgInventoryPanelNavigationCoordinator::UpdatePanelSelectionMemory(FRpgInventoryPanelNavigationEntry& Panel) const
{
	UObject* SelectedItem = nullptr;
	FGuid SelectedEntryId;
	int32 SelectedSlotIndex = INDEX_NONE;

	if (Panel.SpatialGridWidget)
	{
		SelectedItem = Panel.SpatialGridWidget->GetSelectedAddressSlot();
		if (!SelectedItem)
		{
			SelectedItem = Panel.SpatialGridWidget->GetSelectedEntryViewModel();
		}
		SelectedEntryId = Panel.SpatialGridWidget->GetSelectedEntryId();
		SelectedSlotIndex = Panel.SpatialGridWidget->GetSelectedSlotIndex();
	}
	else if (Panel.ActionBarTileView)
	{
		if (URpgActionBarSlotViewModel* SelectedSlot = Panel.ActionBarTileView->GetSelectedActionBarSlot())
		{
			SelectedItem = SelectedSlot;
			SelectedSlotIndex = SelectedSlot->GetSlotIndex();
		}
	}
	else if (Panel.EquipmentSlotWidget)
	{
		SelectedItem = Panel.EquipmentSlotWidget;
	}
	else if (Panel.CarrySlotWidget)
	{
		SelectedItem = Panel.CarrySlotWidget->GetAddressSlotViewModel();
		if (!SelectedItem)
		{
			SelectedItem = Panel.CarrySlotWidget;
		}
		if (URpgInventoryAddressSlotViewModel* AddressSlot = Panel.CarrySlotWidget->GetAddressSlotViewModel())
		{
			SelectedEntryId = AddressSlot->GetEntryId();
			SelectedSlotIndex = 0;
		}
	}

	if (!SelectedItem)
	{
		return;
	}

	Panel.LastSelectedItem = SelectedItem;
	Panel.LastSelectedEntryId = SelectedEntryId;
	Panel.LastSelectedSlotIndex = SelectedSlotIndex;
}

bool URpgInventoryPanelNavigationCoordinator::RestorePanelSelection(FRpgInventoryPanelNavigationEntry& Panel) const
{
	if (Panel.SpatialGridWidget)
	{
		return Panel.SpatialGridWidget->SelectCellByIdentity(Panel.LastSelectedEntryId, Panel.LastSelectedSlotIndex, PlayerController);
	}

	if (Panel.ActionBarTileView && Panel.ActionBarTileView->SelectActionBarListItem(Panel.LastSelectedItem, PlayerController))
	{
		return true;
	}

	if (Panel.ActionBarTileView)
	{
		return Panel.ActionBarTileView->SelectActionBarSlotByIndex(Panel.LastSelectedSlotIndex, PlayerController);
	}

	if (Panel.EquipmentSlotWidget && PlayerController)
	{
		Panel.EquipmentSlotWidget->SetUserFocus(PlayerController);
		return true;
	}

	if (Panel.CarrySlotWidget && PlayerController)
	{
		Panel.CarrySlotWidget->SetUserFocus(PlayerController);
		return true;
	}

	return false;
}

void URpgInventoryPanelNavigationCoordinator::ApplyActivePanelState()
{
	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (URpgInventorySpatialGridWidget* SpatialGridWidget = Panels[PanelIndex].SpatialGridWidget)
		{
			const bool bIsActivePanel = PanelIndex == ActivePanelIndex;
			SpatialGridWidget->SetInventoryPanelActive(bIsActivePanel);
			if (!bIsActivePanel)
			{
				SpatialGridWidget->ClearSelectionVisual();
			}
		}

		if (URpgActionBarTileView* ActionBarTileView = Panels[PanelIndex].ActionBarTileView)
		{
			const bool bIsActivePanel = PanelIndex == ActivePanelIndex;
			ActionBarTileView->SetActionBarPanelActive(bIsActivePanel);
			if (!bIsActivePanel)
			{
				ActionBarTileView->ClearActionBarSelectionVisual();
			}
		}

		if (URpgInventoryCarrySlotWidget* CarrySlotWidget = Panels[PanelIndex].CarrySlotWidget)
		{
			CarrySlotWidget->SetInventoryPanelActive(PanelIndex == ActivePanelIndex);
		}
	}
}

void URpgInventoryPanelNavigationCoordinator::UpdateFocusedInventoryForActivePanel(const FRpgInventoryPanelNavigationEntry& ActivePanel)
{
	if (!DragDropCoordinator || !ActivePanel.Inventory)
	{
		return;
	}

	DragDropCoordinator->SetFocusedInventory(ActivePanel.Inventory);
}

int32 URpgInventoryPanelNavigationCoordinator::FindPanelIndexForSpatialGridWidget(const URpgInventorySpatialGridWidget* SpatialGridWidget) const
{
	if (!SpatialGridWidget)
	{
		return INDEX_NONE;
	}

	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (Panels[PanelIndex].SpatialGridWidget == SpatialGridWidget)
		{
			return PanelIndex;
		}
	}

	return INDEX_NONE;
}

int32 URpgInventoryPanelNavigationCoordinator::FindPanelIndexForActionBarTileView(const URpgActionBarTileView* TileView) const
{
	if (!TileView)
	{
		return INDEX_NONE;
	}

	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (Panels[PanelIndex].ActionBarTileView == TileView)
		{
			return PanelIndex;
		}
	}

	return INDEX_NONE;
}

int32 URpgInventoryPanelNavigationCoordinator::FindPanelIndexForEquipmentSlotWidget(const URpgEquipmentSlotWidget* EquipmentSlotWidget) const
{
	if (!EquipmentSlotWidget)
	{
		return INDEX_NONE;
	}

	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (Panels[PanelIndex].EquipmentSlotWidget == EquipmentSlotWidget)
		{
			return PanelIndex;
		}
	}

	return INDEX_NONE;
}

int32 URpgInventoryPanelNavigationCoordinator::FindPanelIndexForCarrySlotWidget(
	const URpgInventoryCarrySlotWidget* CarrySlotWidget) const
{
	if (!CarrySlotWidget)
	{
		return INDEX_NONE;
	}

	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (Panels[PanelIndex].CarrySlotWidget == CarrySlotWidget)
		{
			return PanelIndex;
		}
	}

	return INDEX_NONE;
}

void URpgInventoryPanelNavigationCoordinator::BroadcastActivePanelChanged(const FRpgInventoryPanelNavigationEntry& ActivePanel)
{
	OnActivePanelChanged.Broadcast(ActivePanel.PanelId, ActivePanelIndex);
}

void URpgInventoryPanelNavigationCoordinator::ApplyRetainedPanelMemory(FRpgInventoryPanelNavigationEntry& Panel) const
{
	const FRpgInventoryPanelNavigationEntry* RetainedPanel = RetainedPanelMemories.Find(Panel.PanelId);
	if (!RetainedPanel)
	{
		return;
	}

	Panel.LastSelectedItem = RetainedPanel->LastSelectedItem;
	Panel.LastSelectedEntryId = RetainedPanel->LastSelectedEntryId;
	Panel.LastSelectedSlotIndex = RetainedPanel->LastSelectedSlotIndex;
}
