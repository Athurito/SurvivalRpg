#include "RpgInventorySlotGroupWidget.h"

#include "RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryAddressSlotViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventorySlotGroupViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"

#include "Blueprint/WidgetTree.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySlotGroupWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryLayoutViews, Log, All);

const FName URpgInventorySlotGroupWidget::SlotGroupViewModelSourceName(
	TEXT("RpgInventorySlotGroupViewModel"));

void URpgInventorySlotGroupWidget::BindInventoryPresentation(
	URpgInventorySlotGroupViewModel* InGroupViewModel,
	const FRpgInventoryScreenPresentationContext& InContext,
	FName InPanelIdPrefix)
{
	if (!InContext.IsComplete() || InPanelIdPrefix.IsNone())
	{
		ReleaseInventoryPresentation();
		return;
	}

	SetSlotGroupViewModel(InGroupViewModel);
	SetDragDropCoordinator(InContext.DragDropCoordinator);
	SetPanelNavigationCoordinator(
		InContext.PanelNavigationCoordinator,
		InPanelIdPrefix);
	if (SpatialGrid)
	{
		SpatialGrid->SetInventoryPresentationHost(
			InContext.PresentationHost);
	}
}

void URpgInventorySlotGroupWidget::ReleaseInventoryPresentation()
{
	SetSlotGroupViewModel(nullptr);
	if (SpatialGrid)
	{
		SpatialGrid->ReleaseInventoryPresentation();
	}
	DragDropCoordinator = nullptr;
	PanelNavigationCoordinator = nullptr;
	PanelNavigationIdPrefix = NAME_None;
}

void URpgInventorySlotGroupWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	DragDropCoordinator = InCoordinator;
	EnsureSpatialGrid();
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
	}
}

void URpgInventorySlotGroupWidget::SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix)
{
	PanelNavigationCoordinator = InPanelNavigationCoordinator;
	PanelNavigationIdPrefix = InPanelIdPrefix;
	RegisterPanelNavigationEntry();
}

void URpgInventorySlotGroupWidget::SetSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel)
{
	GroupViewModel = InGroupViewModel;
	InjectSlotGroupViewModelIntoMvvm();
	EnsureSpatialGrid();
	if (SpatialGrid)
	{
		SpatialGrid->SetDragDropCoordinator(DragDropCoordinator);
		SpatialGrid->BindSlotGroupViewModel(GroupViewModel);
	}

	RegisterPanelNavigationEntry();
}

bool URpgInventorySlotGroupWidget::InjectSlotGroupViewModelIntoMvvm()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		if (GetClass() != StaticClass())
		{
			UE_LOG(
				LogRpgInventoryLayoutViews,
				Error,
				TEXT("%s has no compiled MVVM view. Author one optional manual %s source for stable slot-group leaf data."),
				*GetNameSafe(this),
				*SlotGroupViewModelSourceName.ToString());
		}
		return false;
	}

	const FMVVMViewClass_Source* CompiledSource = ViewClass->GetSources().FindByPredicate(
		[](const FMVVMViewClass_Source& Candidate)
		{
			return Candidate.IsViewModel() &&
				Candidate.GetName() == SlotGroupViewModelSourceName;
		});
	if (!CompiledSource ||
		!CompiledSource->CanBeSet() ||
		!CompiledSource->IsOptional() ||
		CompiledSource->GetSourceClass() != URpgInventorySlotGroupViewModel::StaticClass())
	{
		UE_LOG(
			LogRpgInventoryLayoutViews,
			Error,
			TEXT("%s requires one settable manual MVVM source named %s with type RpgInventorySlotGroupViewModel."),
			*GetNameSafe(this),
			*SlotGroupViewModelSourceName.ToString());
		return false;
	}

	if (View->GetViewModel(SlotGroupViewModelSourceName).GetObject() == GroupViewModel)
	{
		return true;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	if (GroupViewModel)
	{
		ViewModelInterface.SetObject(GroupViewModel);
		ViewModelInterface.SetInterface(GroupViewModel.Get());
	}

	if (!View->SetViewModel(SlotGroupViewModelSourceName, ViewModelInterface))
	{
		UE_LOG(
			LogRpgInventoryLayoutViews,
			Error,
			TEXT("%s failed to inject its slot-group VM into MVVM source %s."),
			*GetNameSafe(this),
			*SlotGroupViewModelSourceName.ToString());
		return false;
	}

	return View->GetViewModel(SlotGroupViewModelSourceName).GetObject() ==
		GroupViewModel;
}

void URpgInventorySlotGroupWidget::NativeDestruct()
{
	ReleaseInventoryPresentation();
	Super::NativeDestruct();
}

void URpgInventorySlotGroupWidget::EnsureSpatialGrid()
{
	if (SpatialGrid)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s expects a Blueprint child widget named SpatialGrid using URpgInventorySpatialGridWidget."), *GetNameSafe(this));
}

void URpgInventorySlotGroupWidget::RegisterPanelNavigationEntry()
{
	if (!PanelNavigationCoordinator || !SpatialGrid || !GroupViewModel)
	{
		return;
	}

	URpgInventoryManagerComponent* Inventory = ResolveGroupInventory();
	if (!Inventory)
	{
		return;
	}

	SpatialGrid->SetPanelNavigationCoordinator(PanelNavigationCoordinator, MakePanelNavigationId());
	PanelNavigationCoordinator->RegisterSpatialInventoryPanel(MakePanelNavigationId(), SpatialGrid, Inventory);
}

URpgInventoryManagerComponent* URpgInventorySlotGroupWidget::ResolveGroupInventory() const
{
	if (!GroupViewModel)
	{
		return nullptr;
	}

	for (URpgInventoryAddressSlotViewModel* SlotViewModel : GroupViewModel->GetSlots())
	{
		if (SlotViewModel && SlotViewModel->GetInventoryManager())
		{
			return SlotViewModel->GetInventoryManager();
		}
	}

	return nullptr;
}

FName URpgInventorySlotGroupWidget::MakePanelNavigationId() const
{
	const FRpgInventoryContainerHandle GroupHandle = GroupViewModel
		? GroupViewModel->GetContainerHandle()
		: FRpgInventoryContainerHandle();
	const FName GroupId = GroupHandle.IsValid() ? FName(*GroupHandle.ToString()) : NAME_None;
	if (PanelNavigationIdPrefix.IsNone())
	{
		return GroupId;
	}

	return FName(*FString::Printf(TEXT("%s.%s"), *PanelNavigationIdPrefix.ToString(), *GroupId.ToString()));
}

FName URpgInventorySlotGroupWidget::GetSlotGroupId() const
{
	return GroupViewModel ? GroupViewModel->GetGroupId() : NAME_None;
}

FRpgInventoryContainerHandle URpgInventorySlotGroupWidget::GetSlotGroupHandle() const
{
	return GroupViewModel ? GroupViewModel->GetContainerHandle() : FRpgInventoryContainerHandle();
}
