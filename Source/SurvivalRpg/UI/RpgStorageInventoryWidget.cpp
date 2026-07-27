#include "RpgStorageInventoryWidget.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryPanelViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventorySlotGroupViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySlotGroupPanelWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStorageInventoryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgStorageInventoryWidget, Log, All);

void URpgStorageInventoryWidget::ReceiveScreenPayload_Implementation(UObject* Payload)
{
	ApplyInventoryScreenPayload(Payload);
}

void URpgStorageInventoryWidget::BindInventoryScreenPresentation()
{
	// CommonGame normally delivers the payload during Initialize, before the activatable widget is pushed.
	// The shared base has now created the screen-owned interaction objects; this hook performs the one actual bind.
	BindStorageScreenContext();
}

void URpgStorageInventoryWidget::UnbindInventoryScreenPresentation()
{
	ResetStorageScreenContext();
}

void URpgStorageInventoryWidget::ForwardInventoryInteractionContextToChildren()
{
	URpgInventoryDragDropCoordinator* Coordinator = GetScreenDragDropCoordinator();
	URpgInventoryPanelNavigationCoordinator* Navigator = GetScreenPanelNavigationCoordinator();
	if (PlayerGroupsPanel)
	{
		PlayerGroupsPanel->SetDragDropCoordinator(Coordinator);
		PlayerGroupsPanel->SetPanelNavigationCoordinator(Navigator, TEXT("Player"));

		TArray<URpgInventorySpatialGridWidget*> PlayerGrids;
		PlayerGroupsPanel->GetSpatialGridWidgets(PlayerGrids);
		for (URpgInventorySpatialGridWidget* PlayerGrid : PlayerGrids)
		{
			if (PlayerGrid)
			{
				PlayerGrid->SetInventoryPresentationHost(this);
			}
		}
	}

	if (SecondaryInventoryGrid)
	{
		SecondaryInventoryGrid->SetDragDropCoordinator(Coordinator);
		SecondaryInventoryGrid->SetPanelNavigationCoordinator(
			Navigator,
			TEXT("Secondary.Root"));
		SecondaryInventoryGrid->SetInventoryPresentationHost(this);
	}
}

void URpgStorageInventoryWidget::ApplyInventoryScreenPayload(UObject* Payload)
{
	URpgInventoryScreenPayload* NewPayload = Cast<URpgInventoryScreenPayload>(Payload);
	if (!NewPayload ||
		!NewPayload->PrimaryInventory ||
		!NewPayload->SecondaryInventory ||
		NewPayload->PrimaryInventory == NewPayload->SecondaryInventory)
	{
		// A dual-inventory screen must never alias both presentation sides to the same mutation source.
		ResetStorageScreenContext();
		return;
	}

	const bool bContextChanged =
		InventoryScreenPayload != NewPayload ||
		PrimaryInventory != NewPayload->PrimaryInventory ||
		SecondaryInventory != NewPayload->SecondaryInventory;
	if (bContextChanged)
	{
		ResetStorageScreenContext();
	}

	InventoryScreenPayload = NewPayload;
	PrimaryInventory = NewPayload->PrimaryInventory;
	SecondaryInventory = NewPayload->SecondaryInventory;

	// Async screen initialization can deliver the payload before NativeOnActivated. Keep that path side-effect-free:
	// activation owns the first coordinator/view-model bind, while active context switches bind immediately.
	if (!IsActivated() || bStorageContextBound)
	{
		return;
	}

	if (BindStorageScreenContext())
	{
		// Unlike initial activation, an active payload transition is not followed by the shared base's
		// presentation pass, so reconnect the authored leaves and focus registry here.
		ForwardInventoryInteractionContextToChildren();
		RefreshInventoryScreenNavigationPanels();
		RefreshInventoryControllerFocus();
	}
}

bool URpgStorageInventoryWidget::BindStorageScreenContext()
{
	if (bStorageContextBound ||
		!IsActivated() ||
		!InventoryScreenPayload ||
		!PrimaryInventory ||
		!SecondaryInventory)
	{
		return false;
	}

	EnsureInventoryInteractionObjects();
	URpgInventoryDragDropCoordinator* Coordinator = GetScreenDragDropCoordinator();
	if (GetOwningPlayer())
	{
		URpgInventoryManagerComponent* CanonicalPlayerInventory =
			Coordinator ? Coordinator->GetPlayerInventory() : nullptr;
		if (!CanonicalPlayerInventory || CanonicalPlayerInventory != PrimaryInventory)
		{
			UE_LOG(
				LogRpgStorageInventoryWidget,
				Warning,
				TEXT("%s rejected Storage payload: PrimaryInventory [%s] does not match the owning player's canonical inventory [%s]."),
				*GetNameSafe(this),
				*GetNameSafe(PrimaryInventory),
				*GetNameSafe(CanonicalPlayerInventory));
			ResetStorageScreenContext();
			return false;
		}

		// Both player presentation and transfer routing now consume the same PlayerState-owned inventory.
		PrimaryInventory = CanonicalPlayerInventory;
	}

	// Set the guard before view-model callbacks can run so an incidental reentrant payload delivery cannot double-bind.
	bStorageContextBound = true;

	EnsureStoragePlayerViewModel();
	if (StoragePlayerInventoryViewModel)
	{
		StoragePlayerInventoryViewModel->BindPlayerController(GetOwningPlayer());
	}
	EnsureSecondaryPanelViewModel();
	RefreshCombinedPlayerGroups();
	BindSecondarySpatialGrid();

	++StoragePresentationBindGeneration;
	return true;
}

void URpgStorageInventoryWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (!Navigator || !bStorageContextBound)
	{
		return;
	}

	if (PlayerGroupsPanel)
	{
		PlayerGroupsPanel->SetDragDropCoordinator(GetScreenDragDropCoordinator());
		PlayerGroupsPanel->SetPanelNavigationCoordinator(Navigator, TEXT("Player"));
	}

	if (SecondaryInventoryGrid && SecondaryInventory)
	{
		SecondaryInventoryGrid->SetDragDropCoordinator(GetScreenDragDropCoordinator());
		SecondaryInventoryGrid->SetPanelNavigationCoordinator(Navigator, TEXT("Secondary.Root"));
		Navigator->RegisterSpatialInventoryPanel(TEXT("Secondary.Root"), SecondaryInventoryGrid, SecondaryInventory);
	}
}

void URpgStorageInventoryWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (PlayerGroupsPanel)
	{
		PlayerGroupsPanel->GetSpatialGridWidgets(OutGrids);
	}

	if (SecondaryInventoryGrid)
	{
		OutGrids.AddUnique(SecondaryInventoryGrid);
	}
}

void URpgStorageInventoryWidget::HandleStoragePlayerSlotGroupsChanged()
{
	RefreshCombinedPlayerGroups();
	QueueDeferredInventoryScreenRefresh();
}

void URpgStorageInventoryWidget::EnsureStoragePlayerViewModel()
{
	if (StoragePlayerInventoryViewModel)
	{
		return;
	}

	StoragePlayerInventoryViewModel = NewObject<URpgPlayerInventoryViewModel>(this);
	if (StoragePlayerInventoryViewModel)
	{
		StoragePlayerInventoryViewModel->OnSlotGroupsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleStoragePlayerSlotGroupsChanged);
	}
}

void URpgStorageInventoryWidget::EnsureSecondaryPanelViewModel()
{
	if (!SecondaryPanelViewModel)
	{
		SecondaryPanelViewModel = NewObject<URpgInventoryPanelViewModel>(this);
	}
}

void URpgStorageInventoryWidget::RefreshCombinedPlayerGroups()
{
	if (!PlayerGroupsPanel)
	{
		return;
	}

	TArray<URpgInventorySlotGroupViewModel*> CombinedGroups;
	if (InventoryScreenPayload && InventoryScreenPayload->PrimaryInventory)
	{
		if (StoragePlayerInventoryViewModel)
		{
			CombinedGroups = StoragePlayerInventoryViewModel->GetCarryGroups();
			CombinedGroups.Append(StoragePlayerInventoryViewModel->GetInventoryGroups());
		}
	}

	PlayerGroupsPanel->SetDragDropCoordinator(GetScreenDragDropCoordinator());
	PlayerGroupsPanel->SetPanelNavigationCoordinator(
		GetScreenPanelNavigationCoordinator(),
		TEXT("Player"));
	PlayerGroupsPanel->SetSlotGroupItems(CombinedGroups);
}

void URpgStorageInventoryWidget::BindSecondarySpatialGrid()
{
	SecondaryRootHandle = SecondaryInventory
		? FRpgInventoryContainerHandle::MakeRoot(SecondaryInventory->GetDefaultContainerId())
		: FRpgInventoryContainerHandle();

	EnsureSecondaryPanelViewModel();
	if (SecondaryPanelViewModel)
	{
		SecondaryPanelViewModel->BindInventoryContainer(SecondaryInventory, SecondaryRootHandle);
	}

	if (SecondaryInventoryGrid)
	{
		SecondaryInventoryGrid->SetDragDropCoordinator(GetScreenDragDropCoordinator());
		SecondaryInventoryGrid->BindInventoryContainerPanelViewModel(
			SecondaryPanelViewModel,
			SecondaryInventory,
			SecondaryRootHandle);
	}

	URpgInventoryDragDropCoordinator* Coordinator = GetScreenDragDropCoordinator();
	if (Coordinator && PrimaryInventory && SecondaryInventory && PrimaryInventory != SecondaryInventory)
	{
		Coordinator->SetQuickTransferTarget(PrimaryInventory, SecondaryInventory);
		Coordinator->SetQuickTransferTarget(SecondaryInventory, PrimaryInventory);
	}
}

void URpgStorageInventoryWidget::ResetStorageScreenContext()
{
	bStorageContextBound = false;

	if (URpgInventoryDragDropCoordinator* Coordinator = GetScreenDragDropCoordinator())
	{
		Coordinator->ForceCancelInteraction();
		Coordinator->ClearQuickTransferTargets();
		Coordinator->SetFocusedInventory(nullptr);
	}

	if (URpgInventoryPanelNavigationCoordinator* Navigator = GetScreenPanelNavigationCoordinator())
	{
		Navigator->ClearPanels();
	}

	if (PlayerGroupsPanel)
	{
		PlayerGroupsPanel->SetPanelNavigationCoordinator(nullptr, NAME_None);
		PlayerGroupsPanel->SetDragDropCoordinator(nullptr);
		PlayerGroupsPanel->SetSlotGroupItems({});
	}

	if (SecondaryInventoryGrid)
	{
		SecondaryInventoryGrid->SetPanelNavigationCoordinator(nullptr, NAME_None);
		SecondaryInventoryGrid->SetDragDropCoordinator(nullptr);
		SecondaryInventoryGrid->BindInventoryContainerPanelViewModel(
			nullptr,
			nullptr,
			FRpgInventoryContainerHandle());
	}

	if (SecondaryPanelViewModel)
	{
		SecondaryPanelViewModel->UnbindInventory();
	}
	if (StoragePlayerInventoryViewModel)
	{
		StoragePlayerInventoryViewModel->UnbindPlayerInventory();
	}

	InventoryScreenPayload = nullptr;
	PrimaryInventory = nullptr;
	SecondaryInventory = nullptr;
	SecondaryRootHandle = FRpgInventoryContainerHandle();
}
