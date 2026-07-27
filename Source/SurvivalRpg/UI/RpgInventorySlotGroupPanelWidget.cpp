#include "RpgInventorySlotGroupPanelWidget.h"

#include "RpgInventorySlotGroupWidget.h"
#include "RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventorySlotGroupViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySlotGroupPanelWidget)

URpgInventorySlotGroupPanelWidget::URpgInventorySlotGroupPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GroupWidgetClass = URpgInventorySlotGroupWidget::StaticClass();
}

void URpgInventorySlotGroupPanelWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		ApplyCoordinatorToGroup(GroupWidget);
	}
}

void URpgInventorySlotGroupPanelWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationIdPrefix = InPanelIdPrefix;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		ApplyNavigationToGroup(GroupWidget);
	}
}

void URpgInventorySlotGroupPanelWidget::SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups)
{
	GroupItems.Reset();
	GroupItems.Reserve(InGroups.Num());
	for (URpgInventorySlotGroupViewModel* GroupViewModel : InGroups)
	{
		GroupItems.Add(GroupViewModel);
	}

	RebuildGroupWidgets();
}

void URpgInventorySlotGroupPanelWidget::GetSpatialGridWidgets(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		if (GroupWidget && GroupWidget->GetSpatialGridWidget())
		{
			OutGrids.Add(GroupWidget->GetSpatialGridWidget());
		}
	}
}

void URpgInventorySlotGroupPanelWidget::EnsureGroupsPanel()
{
	if (GroupsPanel)
	{
		return;
	}

	GroupsPanel = Cast<UPanelWidget>(GetRootWidget());
	if (GroupsPanel || !WidgetTree)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint panel named GroupsPanel or a panel root to receive spatial group widgets."), *GetNameSafe(this));
}

void URpgInventorySlotGroupPanelWidget::RebuildGroupWidgets()
{
	EnsureGroupsPanel();
	if (!GroupsPanel)
	{
		GroupWidgets.Reset();
		return;
	}

	TSubclassOf<URpgInventorySlotGroupWidget> EntryClass = GroupWidgetClass;
	if (!EntryClass)
	{
		EntryClass = URpgInventorySlotGroupWidget::StaticClass();
	}

	TMap<FRpgInventoryContainerHandle, TObjectPtr<URpgInventorySlotGroupWidget>> ExistingGroups;
	for (URpgInventorySlotGroupWidget* GroupWidget : GroupWidgets)
	{
		if (GroupWidget && GroupWidget->GetSlotGroupHandle().IsValid())
		{
			ExistingGroups.Add(GroupWidget->GetSlotGroupHandle(), GroupWidget);
		}
	}

	TArray<TObjectPtr<URpgInventorySlotGroupWidget>> ReconciledGroups;
	TSet<FRpgInventoryContainerHandle> AddedGroupHandles;
	for (URpgInventorySlotGroupViewModel* GroupViewModel : GroupItems)
	{
		if (!GroupViewModel)
		{
			continue;
		}

		const FRpgInventoryContainerHandle GroupHandle = GroupViewModel->GetContainerHandle();
		if (!GroupHandle.IsValid() || AddedGroupHandles.Contains(GroupHandle))
		{
			continue;
		}

		URpgInventorySlotGroupWidget* GroupWidget = ExistingGroups.FindRef(GroupHandle);
		if (GroupWidget && !GroupWidget->IsA(EntryClass))
		{
			GroupWidget = nullptr;
		}
		if (!GroupWidget)
		{
			GroupWidget = CreateWidget<URpgInventorySlotGroupWidget>(this, EntryClass);
		}
		if (!GroupWidget)
		{
			continue;
		}

		ReconciledGroups.Add(GroupWidget);
		AddedGroupHandles.Add(GroupHandle);
		ApplyCoordinatorToGroup(GroupWidget);
		ApplyNavigationToGroup(GroupWidget);
		GroupWidget->SetSlotGroupViewModel(GroupViewModel);
	}

	bool bOrderMatches = ReconciledGroups.Num() == GroupWidgets.Num();
	if (bOrderMatches)
	{
		for (int32 Index = 0; Index < ReconciledGroups.Num(); ++Index)
		{
			if (ReconciledGroups[Index] != GroupWidgets[Index] || ReconciledGroups[Index]->GetParent() != GroupsPanel)
			{
				bOrderMatches = false;
				break;
			}
		}
	}

	if (!bOrderMatches)
	{
		// Strong local references keep keyed widgets alive while the panel order is reconciled.
		GroupsPanel->ClearChildren();
		for (URpgInventorySlotGroupWidget* GroupWidget : ReconciledGroups)
		{
			if (GroupWidget)
			{
				GroupsPanel->AddChild(GroupWidget);
			}
		}
	}

	GroupWidgets = MoveTemp(ReconciledGroups);
}

void URpgInventorySlotGroupPanelWidget::ApplyCoordinatorToGroup(URpgInventorySlotGroupWidget* GroupWidget) const
{
	if (GroupWidget)
	{
		GroupWidget->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupPanelWidget::ApplyNavigationToGroup(URpgInventorySlotGroupWidget* GroupWidget) const
{
	if (GroupWidget)
	{
		GroupWidget->SetPanelNavigationCoordinator(PanelNavigationCoordinator, PanelNavigationIdPrefix);
	}
}
