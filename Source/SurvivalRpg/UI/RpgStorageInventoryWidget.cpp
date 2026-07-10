#include "RpgStorageInventoryWidget.h"

#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStorageInventoryWidget)

namespace
{
	template <typename WidgetType>
	WidgetType* ReplaceLegacyInventoryWidget(
		APlayerController* OwningPlayer,
		UWidget* LegacyWidget,
		TSubclassOf<WidgetType> ReplacementClass,
		FName ReplacementName)
	{
		UPanelWidget* Parent = LegacyWidget ? LegacyWidget->GetParent() : nullptr;
		const int32 ChildIndex = Parent ? Parent->GetChildIndex(LegacyWidget) : INDEX_NONE;
		if (!OwningPlayer || !Parent || ChildIndex == INDEX_NONE || !ReplacementClass)
		{
			return nullptr;
		}

		WidgetType* Replacement = CreateWidget<WidgetType>(OwningPlayer, ReplacementClass, ReplacementName);
		UPanelSlot* SlotTemplate = LegacyWidget->Slot;
		if (!Replacement || !Parent->RemoveChild(LegacyWidget))
		{
			return nullptr;
		}

		if (!Parent->InsertChildAt(ChildIndex, Replacement, SlotTemplate))
		{
			// Restore the original child when an unexpected custom panel rejects the replacement.
			Parent->InsertChildAt(ChildIndex, LegacyWidget, SlotTemplate);
			return nullptr;
		}

		LegacyWidget->SetVisibility(ESlateVisibility::Collapsed);
		return Replacement;
	}
}

URpgStorageInventoryWidget::URpgStorageInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerGroupsPanelClass = TSoftClassPtr<URpgInventorySlotGroupPanelWidget>(FSoftObjectPath(
		TEXT("/Game/SurvivalRpg/Inventory/UI/SpatialInventory/CUI_InventorySlotGroupPanel.CUI_InventorySlotGroupPanel_C")));
	SecondaryInventoryGridClass = TSoftClassPtr<URpgInventorySpatialGridWidget>(FSoftObjectPath(
		TEXT("/Game/SurvivalRpg/Inventory/UI/SpatialInventory/CUI_SpatialInventoryGrid.CUI_SpatialInventoryGrid_C")));
}

void URpgStorageInventoryWidget::ReceiveScreenPayload_Implementation(UObject* Payload)
{
	ApplyInventoryScreenPayload(Payload);
}

void URpgStorageInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureSpatialReplacementWidgets();
	EnsureSecondaryPanelViewModel();
	if (URpgPlayerInventoryViewModel* PlayerViewModel = GetPlayerInventoryViewModel())
	{
		PlayerViewModel->OnSlotGroupsChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerSlotGroupsChanged);
	}
	ApplyLegacyWidgetVisibility();
}

void URpgStorageInventoryWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (InventoryScreenPayload)
	{
		ApplyInventoryScreenPayload(InventoryScreenPayload);
	}
	else
	{
		ApplyLegacyWidgetVisibility();
	}
}

void URpgStorageInventoryWidget::NativeDestruct()
{
	if (URpgPlayerInventoryViewModel* PlayerViewModel = GetPlayerInventoryViewModel())
	{
		PlayerViewModel->OnSlotGroupsChanged.RemoveDynamic(this, &ThisClass::HandlePlayerSlotGroupsChanged);
	}

	if (SecondaryPanelViewModel)
	{
		SecondaryPanelViewModel->UnbindInventory();
	}
	InventoryScreenPayload = nullptr;
	SecondaryInventory = nullptr;
	SecondaryRootHandle = FRpgInventoryContainerHandle();

	Super::NativeDestruct();
}

void URpgStorageInventoryWidget::ApplyInventoryScreenPayload(UObject* Payload)
{
	InventoryScreenPayload = Cast<URpgInventoryScreenPayload>(Payload);
	EnsureSecondaryPanelViewModel();
	EnsurePlayerInventoryCoordinator();
	EnsurePlayerInventoryPanelNavigator();

	// The aggregate player VM is still resolved from the owning controller; the payload is validation/context only.
	BindPlayerInventoryViewModel();
	RefreshCombinedPlayerGroups();
	BindSecondarySpatialGrid();
	ApplyLegacyWidgetVisibility();

	// Re-register both sides in one Begin/End refresh so active panel and cell identity survive payload updates.
	RefreshPlayerInventoryViews();
	RefreshInventoryControllerFocus();

	const bool bPlayerReady = PlayerGroupsPanel && InventoryScreenPayload && InventoryScreenPayload->PrimaryInventory;
	const bool bSecondaryReady = SecondaryInventoryGrid && SecondaryInventory && SecondaryRootHandle.IsValid();
	BP_OnStorageSpatialPresenterBound(InventoryScreenPayload, SecondaryRootHandle, bPlayerReady, bSecondaryReady);
}

void URpgStorageInventoryWidget::RegisterAdditionalInventoryNavigationPanels(URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (!Navigator)
	{
		return;
	}

	if (PlayerGroupsPanel)
	{
		PlayerGroupsPanel->SetDragDropCoordinator(GetInventoryDragDropCoordinator());
		PlayerGroupsPanel->SetPanelNavigationCoordinator(Navigator, TEXT("Player"));
	}

	if (SecondaryInventoryGrid && SecondaryInventory)
	{
		SecondaryInventoryGrid->SetDragDropCoordinator(GetInventoryDragDropCoordinator());
		SecondaryInventoryGrid->SetPanelNavigationCoordinator(Navigator, TEXT("Secondary.Root"));
		Navigator->RegisterSpatialInventoryPanel(TEXT("Secondary.Root"), SecondaryInventoryGrid, SecondaryInventory);
	}
}

void URpgStorageInventoryWidget::AppendAdditionalSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
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

void URpgStorageInventoryWidget::HandlePlayerSlotGroupsChanged()
{
	RefreshCombinedPlayerGroups();
}

void URpgStorageInventoryWidget::EnsureSecondaryPanelViewModel()
{
	if (!SecondaryPanelViewModel)
	{
		SecondaryPanelViewModel = NewObject<URpgInventoryPanelViewModel>(this);
	}
}

void URpgStorageInventoryWidget::EnsureSpatialReplacementWidgets()
{
	if (!LegacyPlayerInventoryWidget)
	{
		LegacyPlayerInventoryWidget = GetWidgetFromName(TEXT("CUI_Inventory_PlayerInventory"));
	}
	if (!LegacyStorageInventoryWidget)
	{
		LegacyStorageInventoryWidget = GetWidgetFromName(TEXT("CUI_Inventory_StorageInventory"));
	}

	if (!PlayerGroupsPanel && LegacyPlayerInventoryWidget)
	{
		PlayerGroupsPanel = ReplaceLegacyInventoryWidget<URpgInventorySlotGroupPanelWidget>(
			GetOwningPlayer(),
			LegacyPlayerInventoryWidget,
			PlayerGroupsPanelClass.LoadSynchronous(),
			TEXT("PlayerGroupsPanel"));
	}

	if (!SecondaryInventoryGrid && LegacyStorageInventoryWidget)
	{
		SecondaryInventoryGrid = ReplaceLegacyInventoryWidget<URpgInventorySpatialGridWidget>(
			GetOwningPlayer(),
			LegacyStorageInventoryWidget,
			SecondaryInventoryGridClass.LoadSynchronous(),
			TEXT("SecondaryInventoryGrid"));
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
		if (URpgPlayerInventoryViewModel* PlayerViewModel = GetPlayerInventoryViewModel())
		{
			CombinedGroups = PlayerViewModel->GetCarryGroups();
			CombinedGroups.Append(PlayerViewModel->GetInventoryGroups());
		}
	}

	PlayerGroupsPanel->SetDragDropCoordinator(GetInventoryDragDropCoordinator());
	PlayerGroupsPanel->SetPanelNavigationCoordinator(GetInventoryPanelNavigator(), TEXT("Player"));
	PlayerGroupsPanel->SetSlotGroupItems(CombinedGroups);
}

void URpgStorageInventoryWidget::BindSecondarySpatialGrid()
{
	SecondaryInventory = InventoryScreenPayload ? InventoryScreenPayload->SecondaryInventory.Get() : nullptr;
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
		SecondaryInventoryGrid->SetDragDropCoordinator(GetInventoryDragDropCoordinator());
		SecondaryInventoryGrid->BindInventoryContainerPanelViewModel(
			SecondaryPanelViewModel,
			SecondaryInventory,
			SecondaryRootHandle);
	}

	URpgInventoryDragDropCoordinator* Coordinator = GetInventoryDragDropCoordinator();
	URpgInventoryManagerComponent* PrimaryInventory = InventoryScreenPayload
		? InventoryScreenPayload->PrimaryInventory.Get()
		: nullptr;
	if (Coordinator && PrimaryInventory && SecondaryInventory && PrimaryInventory != SecondaryInventory)
	{
		Coordinator->SetQuickTransferTarget(PrimaryInventory, SecondaryInventory);
		Coordinator->SetQuickTransferTarget(SecondaryInventory, PrimaryInventory);
	}
}

void URpgStorageInventoryWidget::ApplyLegacyWidgetVisibility()
{
	const bool bPlayerSpatialReady = PlayerGroupsPanel && InventoryScreenPayload && InventoryScreenPayload->PrimaryInventory;
	const bool bSecondarySpatialReady = SecondaryInventoryGrid && InventoryScreenPayload && InventoryScreenPayload->SecondaryInventory;

	if (LegacyPlayerInventoryWidget)
	{
		LegacyPlayerInventoryWidget->SetVisibility(bPlayerSpatialReady ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (LegacyStorageInventoryWidget)
	{
		LegacyStorageInventoryWidget->SetVisibility(bSecondarySpatialReady ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}
