#include "RpgActionBarSlotWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
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
	if (InSlotViewModel)
	{
		bActionBarSlotStateReleased = false;
	}

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
	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator)
{
	if (InCoordinator)
	{
		bActionBarSlotStateReleased = false;
	}

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

	const FRpgInventoryDropTarget PreviewTarget = MakeDropTarget();
	const bool bCanDrop = DragDropCoordinator->UpdateInteractionPreview(Payload, PreviewTarget);
	bHasExternalPreviewState = true;
	ExternalPreviewActionBarSlotIndex = PreviewTarget.ActionBarSlotIndex;
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
	ExternalPreviewActionBarSlotIndex = INDEX_NONE;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	RefreshDragDropVisualState();
}

void URpgActionBarSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetActionBarSlotViewModel(Cast<URpgActionBarSlotViewModel>(ListItemObject));
}

void URpgActionBarSlotWidget::NativeDestruct()
{
	ReleaseActionBarSlotState();
	Super::NativeDestruct();
}

void URpgActionBarSlotWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	ReleaseActionBarSlotState();
}

void URpgActionBarSlotWidget::ReleaseActionBarSlotState()
{
	if (bActionBarSlotStateReleased)
	{
		return;
	}
	bActionBarSlotStateReleased = true;
	StopAllAnimations();

	URpgInventoryDragDropCoordinator* ReleasedCoordinator = DragDropCoordinator;
	URpgActionBarSlotViewModel* ReleasedViewModel = SlotViewModel;
	bool bOwnsCurrentPreviewTarget = false;
	if (bHasExternalPreviewState && ReleasedCoordinator &&
		ExternalPreviewActionBarSlotIndex != INDEX_NONE)
	{
		if (const URpgInventoryInteractionSession* Session = ReleasedCoordinator->GetInteractionSession())
		{
			const FRpgInventoryDropTarget& Target = Session->GetTarget();
			bOwnsCurrentPreviewTarget =
				Target.TargetType == ERpgInventoryDropTargetType::ActionBarSlot &&
				Target.ActionBarSlotIndex == ExternalPreviewActionBarSlotIndex;
		}
	}

	if (ReleasedViewModel)
	{
		ReleasedViewModel->OnSlotChanged.RemoveDynamic(this, &ThisClass::HandleSlotViewModelChanged);
	}
	if (ReleasedCoordinator)
	{
		ReleasedCoordinator->OnHeldPayloadChanged.RemoveDynamic(this, &ThisClass::HandleHeldPayloadChanged);
	}

	SlotViewModel = nullptr;
	DragDropCoordinator = nullptr;
	InjectActionBarSlotViewModelIntoMvvm();
	bSlotSelected = false;
	bActionBarPanelActive = true;
	bHasExternalPreviewState = false;
	ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	ExternalPreviewActionBarSlotIndex = INDEX_NONE;
	CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	// This leaf may clear only the exact logical target it published. Another actionbar slot may already own
	// the shared screen session, and a pending server request remains intentionally session-owned.
	if (bOwnsCurrentPreviewTarget && ReleasedCoordinator)
	{
		ReleasedCoordinator->ClearInteractionPreview();
	}

	BP_OnActionBarSlotSelectionChanged(false);
	BP_OnActionBarSlotDragDropStateChanged(ERpgInventorySlotDragVisualState::Normal);
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
