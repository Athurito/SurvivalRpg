#include "RpgInventorySpatialPaneWidget.h"

#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryPanelViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySpatialPaneWidget)

URpgInventorySpatialPaneWidget::URpgInventorySpatialPaneWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgInventorySpatialPaneWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsurePanelViewModel();
	SetVisibility(ESlateVisibility::Collapsed);
}

void URpgInventorySpatialPaneWidget::NativeDestruct()
{
	ReleaseInventoryPresentation();
	Super::NativeDestruct();
}

void URpgInventorySpatialPaneWidget::BindInventoryContainer(
	URpgInventoryManagerComponent* InInventory,
	FRpgInventoryContainerHandle InContainerHandle)
{
	if (!InInventory || !InContainerHandle.IsValid())
	{
		ReleaseInventoryPresentation();
		return;
	}

	EnsurePanelViewModel();
	if (!PanelViewModel || !SpatialGrid)
	{
		ReleaseInventoryPresentation();
		return;
	}

	BoundInventory = InInventory;
	BoundContainerHandle = InContainerHandle;
	SpatialGrid->BindInventoryContainerPanelViewModel(
		PanelViewModel,
		BoundInventory,
		BoundContainerHandle);
	ApplyInteractionContextToGrid();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URpgInventorySpatialPaneWidget::SetInteractionContext(
	URpgInventoryDragDropCoordinator* InDragDropCoordinator,
	URpgInventoryPanelNavigationCoordinator* InPanelNavigator,
	FName InPanelId,
	URpgInventoryInteractionScreenWidget* InPresentationHost)
{
	DragDropCoordinator = InDragDropCoordinator;
	PanelNavigator = InPanelNavigator;
	PanelId = InPanelId;
	InventoryPresentationHost = InPresentationHost;
	ApplyInteractionContextToGrid();
}

void URpgInventorySpatialPaneWidget::RegisterNavigationPanel(
	URpgInventoryPanelNavigationCoordinator* InPanelNavigator)
{
	URpgInventoryPanelNavigationCoordinator* ResolvedNavigator =
		InPanelNavigator ? InPanelNavigator : PanelNavigator.Get();
	if (!ResolvedNavigator ||
		!SpatialGrid ||
		!BoundInventory ||
		!BoundContainerHandle.IsValid() ||
		PanelId.IsNone())
	{
		return;
	}

	PanelNavigator = ResolvedNavigator;
	SpatialGrid->SetPanelNavigationCoordinator(ResolvedNavigator, PanelId);
	ResolvedNavigator->RegisterSpatialInventoryPanel(
		PanelId,
		SpatialGrid,
		BoundInventory);
}

URpgInventoryPanelViewModel*
URpgInventorySpatialPaneWidget::GetPanelViewModel()
{
	EnsurePanelViewModel();
	return PanelViewModel.Get();
}

void URpgInventorySpatialPaneWidget::ReleaseInventoryPresentation()
{
	if (SpatialGrid)
	{
		SpatialGrid->ReleaseInventoryPresentation();
		SpatialGrid->SetInventoryPresentationHost(nullptr);
	}

	if (PanelViewModel)
	{
		PanelViewModel->UnbindInventory();
	}

	BoundInventory = nullptr;
	BoundContainerHandle = FRpgInventoryContainerHandle();
	DragDropCoordinator = nullptr;
	PanelNavigator = nullptr;
	PanelId = NAME_None;
	InventoryPresentationHost = nullptr;
	SetVisibility(ESlateVisibility::Collapsed);
}

void URpgInventorySpatialPaneWidget::EnsurePanelViewModel()
{
	if (!PanelViewModel)
	{
		PanelViewModel = NewObject<URpgInventoryPanelViewModel>(this);
	}
}

void URpgInventorySpatialPaneWidget::ApplyInteractionContextToGrid()
{
	if (!SpatialGrid)
	{
		return;
	}

	SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
	SpatialGrid->SetPanelNavigationCoordinator(PanelNavigator, PanelId);
	SpatialGrid->SetInventoryPresentationHost(InventoryPresentationHost);
}
