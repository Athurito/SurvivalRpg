#include "RpgInventoryPanelNavigationCoordinator.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryCarrySlotWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "SurvivalRpg/UI/RpgInventoryTileView.h"

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
		if (Panel.TileView)
		{
			Panel.TileView->SetPanelNavigationCoordinator(nullptr, NAME_None);
			Panel.TileView->SetInventoryPanelActive(false);
			Panel.TileView->ClearInventorySelectionVisual();
		}

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

void URpgInventoryPanelNavigationCoordinator::RegisterInventoryPanel(FName PanelId, URpgInventoryTileView* TileView, URpgInventoryManagerComponent* Inventory)
{
	if (PanelId.IsNone() || !TileView || !Inventory)
	{
		return;
	}

	for (FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (Panel.PanelId == PanelId)
		{
			if (Panel.TileView && Panel.TileView != TileView)
			{
				Panel.TileView->SetPanelNavigationCoordinator(nullptr, NAME_None);
			}

			Panel.TileView = TileView;
			Panel.SpatialGridWidget = nullptr;
			Panel.ActionBarTileView = nullptr;
			Panel.EquipmentSlotWidget = nullptr;
			Panel.CarrySlotWidget = nullptr;
			Panel.Inventory = Inventory;
			TileView->SetPanelNavigationCoordinator(this, PanelId);
			UpdatePanelSelectionMemory(Panel);
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.TileView = TileView;
	NewPanel.SpatialGridWidget = nullptr;
	NewPanel.ActionBarTileView = nullptr;
	NewPanel.EquipmentSlotWidget = nullptr;
	NewPanel.CarrySlotWidget = nullptr;
	NewPanel.Inventory = Inventory;
	TileView->SetPanelNavigationCoordinator(this, PanelId);
	ApplyRetainedPanelMemory(NewPanel);
	if (!RetainedPanelMemories.Contains(PanelId))
	{
		UpdatePanelSelectionMemory(NewPanel);
	}
	TileView->SetInventoryPanelActive(false);

	if (ActivePanelIndex == INDEX_NONE && !bPanelRefreshInProgress)
	{
		ActivatePanelByIndex(0);
	}
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
			Panel.TileView = nullptr;
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
	NewPanel.TileView = nullptr;
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
			Panel.TileView = nullptr;
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
	NewPanel.TileView = nullptr;
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
			Panel.TileView = nullptr;
			Panel.SpatialGridWidget = nullptr;
			Panel.ActionBarTileView = nullptr;
			Panel.EquipmentSlotWidget = EquipmentSlotWidget;
			Panel.CarrySlotWidget = nullptr;
			Panel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
			UpdatePanelSelectionMemory(Panel);
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.TileView = nullptr;
	NewPanel.SpatialGridWidget = nullptr;
	NewPanel.ActionBarTileView = nullptr;
	NewPanel.EquipmentSlotWidget = EquipmentSlotWidget;
	NewPanel.CarrySlotWidget = nullptr;
	NewPanel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
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

		Panel.TileView = nullptr;
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

void URpgInventoryPanelNavigationCoordinator::NotifyPanelSelectionChanged(URpgInventoryTileView* TileView, UObject* SelectedItem)
{
	if (bSuppressPanelSelectionNotifications || !TileView || !SelectedItem)
	{
		return;
	}

	const int32 PanelIndex = FindPanelIndexForTileView(TileView);
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
	if (!ActivePanel.TileView && !ActivePanel.SpatialGridWidget && !ActivePanel.ActionBarTileView &&
		!ActivePanel.EquipmentSlotWidget && !ActivePanel.CarrySlotWidget)
	{
		bSuppressPanelSelectionNotifications = false;
		return false;
	}

	if (!RestorePanelSelection(ActivePanel))
	{
		if (ActivePanel.TileView)
		{
			ActivePanel.TileView->SelectBestInventoryEntry(PlayerController, true);
		}
		else if (ActivePanel.SpatialGridWidget)
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
	if (!ActivePanel.TileView && !ActivePanel.SpatialGridWidget && !ActivePanel.ActionBarTileView &&
		!ActivePanel.EquipmentSlotWidget && !ActivePanel.CarrySlotWidget)
	{
		return false;
	}

	bSuppressPanelSelectionNotifications = true;
	ApplyActivePanelState();
	if (!RestorePanelSelection(ActivePanel))
	{
		if (ActivePanel.TileView)
		{
			ActivePanel.TileView->SelectBestInventoryEntry(PlayerController, true);
		}
		else if (ActivePanel.SpatialGridWidget)
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

URpgInventoryTileView* URpgInventoryPanelNavigationCoordinator::GetActiveTileView() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].TileView.Get() : nullptr;
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

	if (Panels[ActivePanelIndex].TileView)
	{
		return Panels[ActivePanelIndex].TileView.Get();
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
	if (const URpgInventoryTileView* TileView = Panel.TileView)
	{
		return DragDropCoordinator &&
			DragDropCoordinator->CanQuickTransferEntry(TileView->GetSelectedInventoryEntry());
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
		return true;
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
	if (const URpgInventoryTileView* TileView = Panel.TileView)
	{
		const URpgInventoryEntryViewModel* Entry = TileView->GetSelectedInventoryEntry();
		return Entry && Entry->CanDrag() && Entry->GetStackCount() > 1;
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
	if (const URpgInventoryTileView* TileView = Panel.TileView)
	{
		const URpgInventoryEntryViewModel* Entry = TileView->GetSelectedInventoryEntry();
		return Entry && Entry->CanDrag() && Entry->GetItemInstance();
	}
	if (const URpgInventoryCarrySlotWidget* CarrySlot = Panel.CarrySlotWidget)
	{
		const TArray<ERpgInventoryContextAction> Actions = CarrySlot->GetAddressContextActions();
		return Actions.Contains(ERpgInventoryContextAction::Use) ||
			Actions.Contains(ERpgInventoryContextAction::EquipAndActivate);
	}
	return Panel.EquipmentSlotWidget && Panel.EquipmentSlotWidget->GetRepresentedItem();
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
	if (const URpgInventoryTileView* TileView = Panel.TileView)
	{
		const URpgInventoryEntryViewModel* Entry = TileView->GetSelectedInventoryEntry();
		return Entry && Entry->CanDrag();
	}
	if (const URpgInventoryCarrySlotWidget* CarrySlot = Panel.CarrySlotWidget)
	{
		return CarrySlot->GetAddressContextActions().Contains(ERpgInventoryContextAction::Drop);
	}
	return false;
}

bool URpgInventoryPanelNavigationCoordinator::QuickTransferActiveSelection()
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	if (URpgInventoryTileView* TileView = Panels[ActivePanelIndex].TileView)
	{
		return TileView->QuickTransferSelectedEntry();
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
		return DragDropCoordinator->ToggleInteractionRotation();
	}

	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	if (URpgInventoryTileView* TileView = Panels[ActivePanelIndex].TileView)
	{
		return TileView->QuickSplitSelectedEntry(SplitCount);
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
		return DragDropCoordinator && AddressSlot && ResolvedSplitCount < AddressSlot->GetStackCount() &&
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

	if (URpgInventoryTileView* TileView = Panels[ActivePanelIndex].TileView)
	{
		return TileView->UseOrEquipSelectedEntry(StackCount);
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

	if (URpgInventoryTileView* TileView = Panels[ActivePanelIndex].TileView)
	{
		return TileView->DropSelectedEntry(StackCount, bConfirmed);
	}

	if (URpgInventorySpatialGridWidget* SpatialGridWidget = Panels[ActivePanelIndex].SpatialGridWidget)
	{
		return SpatialGridWidget->DropSelectedCell(StackCount, bConfirmed);
	}
	if (URpgInventoryCarrySlotWidget* CarrySlotWidget = Panels[ActivePanelIndex].CarrySlotWidget)
	{
		return DragDropCoordinator &&
			DragDropCoordinator->DropAddressSlot(CarrySlotWidget->GetAddressSlotViewModel(), StackCount, bConfirmed);
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::RequestContextMenuForActiveSelection(FVector2D ScreenPosition)
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	FRpgInventoryPanelNavigationEntry& Panel = Panels[ActivePanelIndex];
	if (Panel.SpatialGridWidget)
	{
		return Panel.SpatialGridWidget->RequestContextMenuForSelectedCell(ScreenPosition);
	}
	if (Panel.CarrySlotWidget)
	{
		return Panel.CarrySlotWidget->RequestAddressContextMenu(ScreenPosition);
	}
	if (Panel.EquipmentSlotWidget)
	{
		return Panel.EquipmentSlotWidget->RequestEquipmentContextMenu(ScreenPosition);
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
	return (Panel.TileView || Panel.SpatialGridWidget || Panel.ActionBarTileView ||
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

	if (Panel.TileView)
	{
		if (URpgInventoryEntryViewModel* SelectedEntry = Panel.TileView->GetSelectedInventoryEntry())
		{
			SelectedItem = SelectedEntry;
			SelectedEntryId = !SelectedEntry->IsEmptySlot() ? SelectedEntry->GetEntryId() : FGuid();
			SelectedSlotIndex = SelectedEntry->GetSlotIndex();
		}
	}
	else if (Panel.SpatialGridWidget)
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
	if (Panel.TileView && Panel.TileView->SelectInventoryListItem(Panel.LastSelectedItem, PlayerController))
	{
		return true;
	}

	if (Panel.TileView)
	{
		return Panel.TileView->SelectInventoryEntryByIdentity(Panel.LastSelectedEntryId, Panel.LastSelectedSlotIndex, PlayerController);
	}

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
		if (URpgInventoryTileView* TileView = Panels[PanelIndex].TileView)
		{
			const bool bIsActivePanel = PanelIndex == ActivePanelIndex;
			TileView->SetInventoryPanelActive(bIsActivePanel);
			if (!bIsActivePanel)
			{
				TileView->ClearInventorySelectionVisual();
			}
		}

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

int32 URpgInventoryPanelNavigationCoordinator::FindPanelIndexForTileView(const URpgInventoryTileView* TileView) const
{
	if (!TileView)
	{
		return INDEX_NONE;
	}

	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (Panels[PanelIndex].TileView == TileView)
		{
			return PanelIndex;
		}
	}

	return INDEX_NONE;
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
	OnActivePanelChanged.Broadcast(ActivePanel.PanelId, ActivePanelIndex, ActivePanel.TileView, ActivePanel.Inventory);
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
