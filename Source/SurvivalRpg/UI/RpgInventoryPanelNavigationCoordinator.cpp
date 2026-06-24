#include "RpgInventoryPanelNavigationCoordinator.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
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
	if (!ActivePanel.TileView)
	{
		bSuppressPanelSelectionNotifications = false;
		return false;
	}

	if (!RestorePanelSelection(ActivePanel))
	{
		ActivePanel.TileView->SelectBestInventoryEntry(PlayerController, true);
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
	if (!ActivePanel.TileView)
	{
		return false;
	}

	bSuppressPanelSelectionNotifications = true;
	ApplyActivePanelState();
	if (!RestorePanelSelection(ActivePanel))
	{
		ActivePanel.TileView->SelectBestInventoryEntry(PlayerController, true);
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

URpgInventoryManagerComponent* URpgInventoryPanelNavigationCoordinator::GetActiveInventory() const
{
	return IsValidPanelIndex(ActivePanelIndex) ? Panels[ActivePanelIndex].Inventory.Get() : nullptr;
}

bool URpgInventoryPanelNavigationCoordinator::IsValidPanelIndex(int32 PanelIndex) const
{
	return Panels.IsValidIndex(PanelIndex) && Panels[PanelIndex].TileView && Panels[PanelIndex].Inventory;
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
	URpgInventoryEntryViewModel* SelectedEntry = Panel.TileView ? Panel.TileView->GetSelectedInventoryEntry() : nullptr;
	if (!SelectedEntry)
	{
		return;
	}

	Panel.LastSelectedItem = SelectedEntry;
	Panel.LastSelectedEntryId = SelectedEntry && !SelectedEntry->IsEmptySlot() ? SelectedEntry->GetEntryId() : FGuid();
	Panel.LastSelectedSlotIndex = SelectedEntry ? SelectedEntry->GetSlotIndex() : INDEX_NONE;
}

bool URpgInventoryPanelNavigationCoordinator::RestorePanelSelection(FRpgInventoryPanelNavigationEntry& Panel) const
{
	if (!Panel.TileView)
	{
		return false;
	}

	if (Panel.TileView->SelectInventoryListItem(Panel.LastSelectedItem, PlayerController))
	{
		return true;
	}

	return Panel.TileView->SelectInventoryEntryByIdentity(Panel.LastSelectedEntryId, Panel.LastSelectedSlotIndex, PlayerController);
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

void URpgInventoryPanelNavigationCoordinator::BroadcastActivePanelChanged(const FRpgInventoryPanelNavigationEntry& ActivePanel)
{
	OnActivePanelChanged.Broadcast(ActivePanel.PanelId, ActivePanelIndex, ActivePanel.TileView, ActivePanel.Inventory);
}
