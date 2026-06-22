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
			Panel.TileView = TileView;
			Panel.Inventory = Inventory;
			Panel.LastSelectedItem = TileView->GetSelectedInventoryEntry();
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
			return;
		}
	}

	FRpgInventoryPanelNavigationEntry& NewPanel = Panels.AddDefaulted_GetRef();
	NewPanel.PanelId = PanelId;
	NewPanel.TileView = TileView;
	NewPanel.Inventory = Inventory;
	NewPanel.LastSelectedItem = TileView->GetSelectedInventoryEntry();

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

	FRpgInventoryPanelNavigationEntry& ActivePanel = Panels[ActivePanelIndex];
	if (!ActivePanel.TileView)
	{
		return false;
	}

	if (!ActivePanel.TileView->SelectInventoryListItem(ActivePanel.LastSelectedItem, PlayerController))
	{
		ActivePanel.TileView->SelectBestInventoryEntry(PlayerController, true);
	}

	ActivePanel.LastSelectedItem = ActivePanel.TileView->GetSelectedInventoryEntry();
	UpdateShortcutRoutesForActivePanel(ActivePanel);
	OnActivePanelChanged.Broadcast(ActivePanel.PanelId, ActivePanelIndex, ActivePanel.TileView, ActivePanel.Inventory);
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

	if (!ActivePanel.TileView->SelectInventoryListItem(ActivePanel.LastSelectedItem, PlayerController))
	{
		ActivePanel.TileView->SelectBestInventoryEntry(PlayerController, true);
	}

	ActivePanel.LastSelectedItem = ActivePanel.TileView->GetSelectedInventoryEntry();
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

	Panels[ActivePanelIndex].LastSelectedItem = Panels[ActivePanelIndex].TileView->GetSelectedInventoryEntry();
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
