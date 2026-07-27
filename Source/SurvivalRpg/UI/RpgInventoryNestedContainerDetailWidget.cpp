#include "RpgInventoryNestedContainerDetailWidget.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryNestedContainerDetailWidget)

URpgInventoryNestedContainerDetailWidget::URpgInventoryNestedContainerDetailWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgInventoryNestedContainerDetailWidget::SetNestedContainerViewModel(
	URpgInventoryNestedContainerViewModel* InViewModel)
{
	if (NestedContainerViewModel == InViewModel)
	{
		RefreshNativePresentation();
		return;
	}

	if (NestedContainerViewModel)
	{
		NestedContainerViewModel->OnPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged);
		if (bOwnsViewModel)
		{
			NestedContainerViewModel->CloseContainer();
		}
	}

	NestedContainerViewModel = InViewModel;
	bOwnsViewModel = false;
	if (NestedContainerViewModel)
	{
		NestedContainerViewModel->OnPresentationChanged.AddUniqueDynamic(this, &ThisClass::HandlePresentationChanged);
	}
	RefreshNativePresentation();
}

bool URpgInventoryNestedContainerDetailWidget::OpenContainerByItemId(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemId ItemId,
	FName LocalContainerId)
{
	EnsureViewModel();
	return NestedContainerViewModel &&
		NestedContainerViewModel->OpenContainerByItemId(Inventory, ItemId, LocalContainerId);
}

bool URpgInventoryNestedContainerDetailWidget::OpenContainerHandle(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryContainerHandle ContainerHandle)
{
	EnsureViewModel();
	return NestedContainerViewModel &&
		NestedContainerViewModel->OpenContainerHandle(Inventory, ContainerHandle);
}

bool URpgInventoryNestedContainerDetailWidget::NavigateToBreadcrumb(int32 BreadcrumbIndex)
{
	return NestedContainerViewModel && NestedContainerViewModel->NavigateToBreadcrumb(BreadcrumbIndex);
}

void URpgInventoryNestedContainerDetailWidget::CloseContainer()
{
	if (NestedContainerViewModel)
	{
		NestedContainerViewModel->CloseContainer();
	}
}

void URpgInventoryNestedContainerDetailWidget::SetFilterQuery(const FText& NewFilterQuery)
{
	EnsureViewModel();
	if (NestedContainerViewModel)
	{
		NestedContainerViewModel->SetFilterQuery(NewFilterQuery);
	}
}

void URpgInventoryNestedContainerDetailWidget::SetDragDropCoordinator(
	URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventoryNestedContainerDetailWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureViewModel();
	RefreshNativePresentation();
}

void URpgInventoryNestedContainerDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureViewModel();
	if (NestedContainerViewModel)
	{
		NestedContainerViewModel->OnPresentationChanged.AddUniqueDynamic(this, &ThisClass::HandlePresentationChanged);
	}
	RefreshNativePresentation();
}

void URpgInventoryNestedContainerDetailWidget::NativeDestruct()
{
	if (NestedContainerViewModel)
	{
		NestedContainerViewModel->OnPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged);
		if (bOwnsViewModel)
		{
			NestedContainerViewModel->CloseContainer();
		}
	}
	if (SpatialGrid)
	{
		SpatialGrid->ClearEntryDimming();
	}
	Super::NativeDestruct();
}

void URpgInventoryNestedContainerDetailWidget::HandlePresentationChanged()
{
	RefreshNativePresentation();
}

void URpgInventoryNestedContainerDetailWidget::EnsureViewModel()
{
	if (NestedContainerViewModel)
	{
		return;
	}

	NestedContainerViewModel = NewObject<URpgInventoryNestedContainerViewModel>(this);
	bOwnsViewModel = true;
	NestedContainerViewModel->OnPresentationChanged.AddUniqueDynamic(this, &ThisClass::HandlePresentationChanged);
}

void URpgInventoryNestedContainerDetailWidget::RefreshNativePresentation()
{
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
		if (NestedContainerViewModel && NestedContainerViewModel->IsOpen())
		{
			SpatialGrid->BindInventoryContainerPanelViewModel(
				NestedContainerViewModel->GetPanelViewModel(),
				NestedContainerViewModel->GetObservedInventory(),
				NestedContainerViewModel->GetOpenContainerHandle());
			SpatialGrid->SetDimmedEntryIds(
				NestedContainerViewModel->GetDimmedEntryIds(),
				NonMatchOpacity);
		}
		else
		{
			SpatialGrid->ClearEntryDimming();
			SpatialGrid->BindInventoryContainerPanelViewModel(
				nullptr,
				nullptr,
				FRpgInventoryContainerHandle());
		}
	}

	BP_OnNestedContainerDetailChanged(NestedContainerViewModel);
}
