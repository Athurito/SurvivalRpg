#include "RpgInventoryPanelNavigationCoordinator.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
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

		if (Panel.AddressTileView)
		{
			Panel.AddressTileView->SetPanelNavigationCoordinator(nullptr, NAME_None);
			Panel.AddressTileView->SetInventoryPanelActive(false);
			Panel.AddressTileView->ClearAddressSelectionVisual();
		}

		if (Panel.ActionBarTileView)
		{
			Panel.ActionBarTileView->SetPanelNavigationCoordinator(nullptr, NAME_None);
			Panel.ActionBarTileView->SetActionBarPanelActive(false);
			Panel.ActionBarTileView->ClearActionBarSelectionVisual();
		}
	}

	Panels.Reset();
	ActivePanelIndex = INDEX_NONE;
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
			Panel.AddressTileView = nullptr;
			Panel.ActionBarTileView = nullptr;
			Panel.EquipmentSlotWidget = nullptr;
			Panel.Inventory = Inventory;
			TileView->SetPanelNavigationCoordinator(this, PanelId);
			UpdatePanelSelectionMemory(Panel);
			if (DragDropCoordinator)
			{
				if (URpgInventoryManagerComponent* PlayerInventory = DragDropCoordinator->GetPlayerInventory())
				{
					if (Inventory != PlayerInventory)
					{
						DragDropCoordinator->SetQuickTransferTarget(Inventory, PlayerInventory);
					}
				}
			}
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.TileView = TileView;
	NewPanel.AddressTileView = nullptr;
	NewPanel.ActionBarTileView = nullptr;
	NewPanel.EquipmentSlotWidget = nullptr;
	NewPanel.Inventory = Inventory;
	TileView->SetPanelNavigationCoordinator(this, PanelId);
	UpdatePanelSelectionMemory(NewPanel);
	TileView->SetInventoryPanelActive(false);

	if (DragDropCoordinator)
	{
		if (URpgInventoryManagerComponent* PlayerInventory = DragDropCoordinator->GetPlayerInventory())
		{
			if (Inventory != PlayerInventory)
			{
				DragDropCoordinator->SetQuickTransferTarget(Inventory, PlayerInventory);
				if (!DragDropCoordinator->ResolveQuickTransferTarget(PlayerInventory))
				{
					DragDropCoordinator->SetQuickTransferTarget(PlayerInventory, Inventory);
				}
			}
		}
	}

	if (ActivePanelIndex == INDEX_NONE)
	{
		ActivatePanelByIndex(0);
	}
}

void URpgInventoryPanelNavigationCoordinator::RegisterInventoryAddressPanel(FName PanelId, URpgInventoryAddressTileView* TileView, URpgInventoryManagerComponent* Inventory)
{
	if (PanelId.IsNone() || !TileView || !Inventory)
	{
		return;
	}

	for (FRpgInventoryPanelNavigationEntry& Panel : Panels)
	{
		if (Panel.PanelId == PanelId)
		{
			Panel.TileView = nullptr;
			Panel.AddressTileView = TileView;
			Panel.ActionBarTileView = nullptr;
			Panel.EquipmentSlotWidget = nullptr;
			Panel.Inventory = Inventory;
			TileView->SetPanelNavigationCoordinator(this, PanelId);
			UpdatePanelSelectionMemory(Panel);
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.TileView = nullptr;
	NewPanel.AddressTileView = TileView;
	NewPanel.ActionBarTileView = nullptr;
	NewPanel.EquipmentSlotWidget = nullptr;
	NewPanel.Inventory = Inventory;
	TileView->SetPanelNavigationCoordinator(this, PanelId);
	UpdatePanelSelectionMemory(NewPanel);
	TileView->SetInventoryPanelActive(false);

	if (ActivePanelIndex == INDEX_NONE)
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
			Panel.AddressTileView = nullptr;
			Panel.ActionBarTileView = TileView;
			Panel.EquipmentSlotWidget = nullptr;
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
	NewPanel.AddressTileView = nullptr;
	NewPanel.ActionBarTileView = TileView;
	NewPanel.EquipmentSlotWidget = nullptr;
	NewPanel.Inventory = nullptr;
	TileView->SetPanelNavigationCoordinator(this, PanelId);
	UpdatePanelSelectionMemory(NewPanel);
	TileView->SetActionBarPanelActive(false);

	if (ActivePanelIndex == INDEX_NONE)
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
			Panel.AddressTileView = nullptr;
			Panel.ActionBarTileView = nullptr;
			Panel.EquipmentSlotWidget = EquipmentSlotWidget;
			Panel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
			UpdatePanelSelectionMemory(Panel);
			ApplyActivePanelState();
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.TileView = nullptr;
	NewPanel.AddressTileView = nullptr;
	NewPanel.ActionBarTileView = nullptr;
	NewPanel.EquipmentSlotWidget = EquipmentSlotWidget;
	NewPanel.Inventory = DragDropCoordinator ? DragDropCoordinator->GetPlayerInventory() : nullptr;
	UpdatePanelSelectionMemory(NewPanel);

	if (ActivePanelIndex == INDEX_NONE)
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
	UpdateShortcutRoutesForActivePanel(ActivePanel);

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

	if (bPanelChanged)
	{
		BroadcastActivePanelChanged(ActivePanel);
	}
}

void URpgInventoryPanelNavigationCoordinator::NotifyAddressPanelSelectionChanged(URpgInventoryAddressTileView* TileView, UObject* SelectedItem)
{
	if (bSuppressPanelSelectionNotifications || !TileView || !SelectedItem)
	{
		return;
	}

	const int32 PanelIndex = FindPanelIndexForAddressTileView(TileView);
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
	UpdateShortcutRoutesForActivePanel(ActivePanel);

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
	if (!ActivePanel.TileView && !ActivePanel.AddressTileView && !ActivePanel.ActionBarTileView && !ActivePanel.EquipmentSlotWidget)
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
		else if (ActivePanel.AddressTileView)
		{
			ActivePanel.AddressTileView->SelectBestAddressSlot(PlayerController, true);
		}
		else if (ActivePanel.ActionBarTileView)
		{
			ActivePanel.ActionBarTileView->SelectBestActionBarSlot(PlayerController);
		}
		else if (ActivePanel.EquipmentSlotWidget && PlayerController)
		{
			ActivePanel.EquipmentSlotWidget->SetUserFocus(PlayerController);
		}
	}
	bSuppressPanelSelectionNotifications = false;

	UpdatePanelSelectionMemory(ActivePanel);
	UpdateShortcutRoutesForActivePanel(ActivePanel);
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
	if (!ActivePanel.TileView && !ActivePanel.AddressTileView && !ActivePanel.ActionBarTileView && !ActivePanel.EquipmentSlotWidget)
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
		else if (ActivePanel.AddressTileView)
		{
			ActivePanel.AddressTileView->SelectBestAddressSlot(PlayerController, true);
		}
		else if (ActivePanel.ActionBarTileView)
		{
			ActivePanel.ActionBarTileView->SelectBestActionBarSlot(PlayerController);
		}
		else if (ActivePanel.EquipmentSlotWidget && PlayerController)
		{
			ActivePanel.EquipmentSlotWidget->SetUserFocus(PlayerController);
		}
	}
	bSuppressPanelSelectionNotifications = false;

	UpdatePanelSelectionMemory(ActivePanel);
	UpdateShortcutRoutesForActivePanel(ActivePanel);
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

URpgInventoryAddressTileView* URpgInventoryPanelNavigationCoordinator::GetActiveAddressTileView() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].AddressTileView.Get() : nullptr;
}

URpgActionBarTileView* URpgInventoryPanelNavigationCoordinator::GetActiveActionBarTileView() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].ActionBarTileView.Get() : nullptr;
}

URpgEquipmentSlotWidget* URpgInventoryPanelNavigationCoordinator::GetActiveEquipmentSlotWidget() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].EquipmentSlotWidget.Get() : nullptr;
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

	if (Panels[ActivePanelIndex].AddressTileView)
	{
		if (UWidget* RedirectFocusTarget = Panels[ActivePanelIndex].AddressTileView->GetAddressSlotFocusTarget())
		{
			return RedirectFocusTarget;
		}

		return Panels[ActivePanelIndex].AddressTileView.Get();
	}

	if (Panels[ActivePanelIndex].ActionBarTileView)
	{
		return Panels[ActivePanelIndex].ActionBarTileView.Get();
	}

	return Panels[ActivePanelIndex].EquipmentSlotWidget.Get();
}

URpgInventoryManagerComponent* URpgInventoryPanelNavigationCoordinator::GetActiveInventory() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].Inventory.Get() : nullptr;
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

	if (URpgInventoryAddressTileView* AddressTileView = Panels[ActivePanelIndex].AddressTileView)
	{
		return AddressTileView->QuickTransferSelectedAddressSlot();
	}

	return false;
}

bool URpgInventoryPanelNavigationCoordinator::QuickSplitActiveSelection(int32 SplitCount)
{
	if (!IsValidPanelIndex(ActivePanelIndex))
	{
		return false;
	}

	if (URpgInventoryTileView* TileView = Panels[ActivePanelIndex].TileView)
	{
		return TileView->QuickSplitSelectedEntry(SplitCount);
	}

	if (URpgInventoryAddressTileView* AddressTileView = Panels[ActivePanelIndex].AddressTileView)
	{
		return AddressTileView->QuickSplitSelectedAddressSlot(SplitCount);
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

	if (URpgInventoryAddressTileView* AddressTileView = Panels[ActivePanelIndex].AddressTileView)
	{
		return AddressTileView->UseOrEquipSelectedAddressSlot(StackCount);
	}

	if (URpgEquipmentSlotWidget* EquipmentSlotWidget = Panels[ActivePanelIndex].EquipmentSlotWidget)
	{
		return EquipmentSlotWidget->HandleClearAssignment();
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

	if (URpgInventoryAddressTileView* AddressTileView = Panels[ActivePanelIndex].AddressTileView)
	{
		return AddressTileView->DropSelectedAddressSlot(StackCount, bConfirmed);
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
	return (Panel.TileView || Panel.AddressTileView || Panel.ActionBarTileView || Panel.EquipmentSlotWidget) &&
		(Panel.Inventory || Panel.ActionBarTileView || Panel.EquipmentSlotWidget);
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
	else if (Panel.AddressTileView)
	{
		if (URpgInventoryAddressSlotViewModel* SelectedSlot = Panel.AddressTileView->GetSelectedAddressSlot())
		{
			SelectedItem = SelectedSlot;
			SelectedEntryId = !SelectedSlot->IsEmptySlot() ? SelectedSlot->GetEntryId() : FGuid();
			SelectedSlotIndex = SelectedSlot->GetGlobalSlotIndex();
		}
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

	if (Panel.AddressTileView && Panel.AddressTileView->SelectAddressListItem(Panel.LastSelectedItem, PlayerController))
	{
		return true;
	}

	if (Panel.AddressTileView)
	{
		return Panel.AddressTileView->SelectAddressSlotByIdentity(Panel.LastSelectedEntryId, Panel.LastSelectedSlotIndex, PlayerController);
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

		if (URpgInventoryAddressTileView* AddressTileView = Panels[PanelIndex].AddressTileView)
		{
			const bool bIsActivePanel = PanelIndex == ActivePanelIndex;
			AddressTileView->SetInventoryPanelActive(bIsActivePanel);
			if (!bIsActivePanel)
			{
				AddressTileView->ClearAddressSelectionVisual();
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
	}
}

void URpgInventoryPanelNavigationCoordinator::UpdateShortcutRoutesForActivePanel(const FRpgInventoryPanelNavigationEntry& ActivePanel)
{
	if (!DragDropCoordinator || !ActivePanel.Inventory)
	{
		return;
	}

	DragDropCoordinator->SetFocusedInventory(ActivePanel.Inventory);

	URpgInventoryManagerComponent* PlayerInventory = DragDropCoordinator->GetPlayerInventory();
	if (PlayerInventory && ActivePanel.Inventory != PlayerInventory)
	{
		DragDropCoordinator->SetQuickTransferTarget(PlayerInventory, ActivePanel.Inventory);
		DragDropCoordinator->SetQuickTransferTarget(ActivePanel.Inventory, PlayerInventory);
	}
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

int32 URpgInventoryPanelNavigationCoordinator::FindPanelIndexForAddressTileView(const URpgInventoryAddressTileView* TileView) const
{
	if (!TileView)
	{
		return INDEX_NONE;
	}

	for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
	{
		if (Panels[PanelIndex].AddressTileView == TileView)
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

void URpgInventoryPanelNavigationCoordinator::BroadcastActivePanelChanged(const FRpgInventoryPanelNavigationEntry& ActivePanel)
{
	OnActivePanelChanged.Broadcast(ActivePanel.PanelId, ActivePanelIndex, ActivePanel.TileView, ActivePanel.Inventory);
}
