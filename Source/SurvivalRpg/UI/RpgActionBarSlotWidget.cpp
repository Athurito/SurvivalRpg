#include "RpgActionBarSlotWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarSlotWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgActionBarSlotWidget, Log, All);

const FName URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName(
	TEXT("RpgActionBarSlotViewModel"));

URpgActionBarSlotWidget::URpgActionBarSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetIsInteractionEnabled(true);
}

void URpgActionBarSlotWidget::SetActionBarSlotViewModel(URpgActionBarSlotViewModel* InSlotViewModel)
{
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	SlotViewModel = InSlotViewModel;
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}

	InjectActionBarSlotViewModelIntoMvvm();
	BP_OnActionBarSlotViewModelSet(SlotViewModel);
	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	DragDropCoordinator = InCoordinator;
	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::SetActionBarPanelActive(bool bInActionBarPanelActive)
{
	if (bActionBarPanelActive == bInActionBarPanelActive)
	{
		return;
	}

	bActionBarPanelActive = bInActionBarPanelActive;
	RefreshDragDropVisualState();
}

bool URpgActionBarSlotWidget::HandleSlotAccept()
{
	return DragDropCoordinator && DragDropCoordinator->HasHeldPayload() && DragDropCoordinator->CommitDrop(MakeDropTarget());
}

void URpgActionBarSlotWidget::RefreshDragDropVisualState()
{
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;
	if (bHasExternalPreviewState)
	{
		CurrentDragDropVisualState = ExternalPreviewState;
	}
	else if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		CurrentDragDropVisualState = DragDropCoordinator->PreviewDrop(MakeDropTarget())
			? ERpgInventorySlotDragVisualState::ValidTarget
			: ERpgInventorySlotDragVisualState::InvalidTarget;
	}
	else if (bSlotSelected && bActionBarPanelActive)
	{
		CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Focused;
	}

	BP_OnActionBarSlotDragDropStateChanged(CurrentDragDropVisualState);
}

bool URpgActionBarSlotWidget::PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	if (!DragDropCoordinator || !URpgInventoryDragDropCoordinator::IsPayloadValid(Payload))
	{
		ClearExternalPreviewPayload();
		return false;
	}

	const bool bCanDrop = DragDropCoordinator->UpdateInteractionPreview(Payload, MakeDropTarget());
	bHasExternalPreviewState = true;
	ExternalPreviewState = bCanDrop
		? ERpgInventorySlotDragVisualState::ValidTarget
		: ERpgInventorySlotDragVisualState::InvalidTarget;
	RefreshDragDropVisualState();
	return bCanDrop;
}

bool URpgActionBarSlotWidget::CommitPayloadDrop(const FRpgInventoryDragPayload& Payload)
{
	ClearExternalPreviewPayload();
	return DragDropCoordinator && DragDropCoordinator->CommitPayloadToTarget(Payload, MakeDropTarget());
}

void URpgActionBarSlotWidget::ClearExternalPreviewPayload()
{
	if (!bHasExternalPreviewState)
	{
		return;
	}

	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetActionBarSlotViewModel(Cast<URpgActionBarSlotViewModel>(ListItemObject));
}

void URpgActionBarSlotWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();

	if (SlotViewModel)
	{
		SlotViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}
	SlotViewModel = nullptr;
	InjectActionBarSlotViewModelIntoMvvm();

	bSlotSelected = false;
	bActionBarPanelActive = true;
	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;

	if (DragDropCoordinator)
	{
		DragDropCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}
	DragDropCoordinator = nullptr;

	BP_OnActionBarSlotViewModelSet(nullptr);
	BP_OnActionBarSlotSelectionChanged(false);
	RefreshDragDropVisualState();
	BP_OnActionBarSlotReleased();
}

void URpgActionBarSlotWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	bSlotSelected = bIsSelected;
	BP_OnActionBarSlotSelectionChanged(bSlotSelected);
	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	HandleSlotAccept();
}

bool URpgActionBarSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}

bool URpgActionBarSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation || !DragDropCoordinator ||
		!URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
	{
		return false;
	}

	const FVector2D GhostCenterScreenPosition =
		InventoryOperation->ResolveDecoratorCenterScreen(InDragDropEvent.GetScreenSpacePosition());
	const FVector2D LocalGhostCenter = InGeometry.AbsoluteToLocal(GhostCenterScreenPosition);
	const FVector2D SlotSize = InGeometry.GetLocalSize();
	const bool bGhostAddressesThisSlot =
		LocalGhostCenter.X >= 0.0f &&
		LocalGhostCenter.Y >= 0.0f &&
		LocalGhostCenter.X <= SlotSize.X &&
		LocalGhostCenter.Y <= SlotSize.Y;
	if (!bGhostAddressesThisSlot)
	{
		// The player-screen resolver still handles edge grabs whose pointer and visible ghost address
		// different widgets. This fallback only commits when this exact displayed slot owns the ghost center.
		return false;
	}

	const FRpgInventoryDragPayload ResolvedPayload =
		DragDropCoordinator->ResolveInteractionPayload(InventoryOperation->InventoryPayload);
	return CommitPayloadDrop(ResolvedPayload);
}

void URpgActionBarSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void URpgActionBarSlotWidget::HandleSlotViewModelChanged(URpgActionBarSlotViewModel* ChangedSlotViewModel)
{
	if (ChangedSlotViewModel == SlotViewModel)
	{
		BP_OnActionBarSlotViewModelSet(SlotViewModel);
		RefreshDragDropVisualState();
	}
}

void URpgActionBarSlotWidget::HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload)
{
	RefreshDragDropVisualState();
}

bool URpgActionBarSlotWidget::InjectActionBarSlotViewModelIntoMvvm()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		if (GetClass() != StaticClass())
		{
			UE_LOG(
				LogRpgActionBarSlotWidget,
				Error,
				TEXT("%s has no compiled MVVM view. Author one optional manual %s source for actionbar slot data."),
				*GetNameSafe(this),
				*ActionBarSlotViewModelSourceName.ToString());
		}
		return false;
	}

	const FMVVMViewClass_Source* CompiledSource = ViewClass->GetSources().FindByPredicate(
		[](const FMVVMViewClass_Source& Candidate)
		{
			return Candidate.IsViewModel() &&
				Candidate.GetName() == ActionBarSlotViewModelSourceName;
		});
	if (!CompiledSource ||
		!CompiledSource->CanBeSet() ||
		!CompiledSource->IsOptional() ||
		CompiledSource->GetSourceClass() != URpgActionBarSlotViewModel::StaticClass())
	{
		UE_LOG(
			LogRpgActionBarSlotWidget,
			Error,
			TEXT("%s requires one settable optional manual MVVM source named %s with type RpgActionBarSlotViewModel."),
			*GetNameSafe(this),
			*ActionBarSlotViewModelSourceName.ToString());
		return false;
	}

	if (View->GetViewModel(ActionBarSlotViewModelSourceName).GetObject() == SlotViewModel)
	{
		return true;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	if (SlotViewModel)
	{
		ViewModelInterface.SetObject(SlotViewModel);
		ViewModelInterface.SetInterface(SlotViewModel.Get());
	}

	if (!View->SetViewModel(ActionBarSlotViewModelSourceName, ViewModelInterface))
	{
		UE_LOG(
			LogRpgActionBarSlotWidget,
			Error,
			TEXT("%s failed to inject its actionbar slot VM into MVVM source %s."),
			*GetNameSafe(this),
			*ActionBarSlotViewModelSourceName.ToString());
		return false;
	}

	return View->GetViewModel(ActionBarSlotViewModelSourceName).GetObject() == SlotViewModel;
}

FRpgInventoryDropTarget URpgActionBarSlotWidget::MakeDropTarget() const
{
	return URpgInventoryDragDropCoordinator::MakeActionBarSlotTargetFromViewModel(SlotViewModel);
}
