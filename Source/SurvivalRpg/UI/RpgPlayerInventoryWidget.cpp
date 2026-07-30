#include "RpgPlayerInventoryWidget.h"

#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryPaneWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryWidget)

const FName URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName =
	URpgPlayerInventoryPaneWidget::PlayerInventoryViewModelSourceName;

URpgPlayerInventoryViewModel*
URpgPlayerInventoryWidget::GetPlayerInventoryViewModel() const
{
	return PlayerInventoryPane
		? PlayerInventoryPane->GetPlayerInventoryViewModel()
		: nullptr;
}

FString URpgPlayerInventoryWidget::GetPlayerInventoryWidgetDebugSummary() const
{
	return PlayerInventoryPane
		? PlayerInventoryPane->GetPlayerInventoryWidgetDebugSummary()
		: FString::Printf(
			TEXT("PlayerInventoryScreen Pane=%s"),
			*GetNameSafe(PlayerInventoryPane));
}

void URpgPlayerInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
	}
}

void URpgPlayerInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (PlayerInventoryPane)
	{
		// CommonUI can reconstruct the same pooled widget after NativeDestruct; restore this presentation-only link.
		PlayerInventoryPane->OnNavigationPanelsChanged.RemoveAll(this);
		PlayerInventoryPane->OnNavigationPanelsChanged.AddUObject(
			this,
			&ThisClass::HandlePlayerInventoryPaneNavigationPanelsChanged);
	}
}

void URpgPlayerInventoryWidget::NativeDestruct()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->OnNavigationPanelsChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

UWidget* URpgPlayerInventoryWidget::NativeGetDesiredFocusTarget() const
{
	if (PlayerInventoryPane)
	{
		if (UWidget* Preferred = PlayerInventoryPane->GetPreferredFocusTarget())
		{
			return Preferred;
		}
	}
	return Super::NativeGetDesiredFocusTarget();
}

void URpgPlayerInventoryWidget::BindInventoryScreenPresentation()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->BindPlayerInventory(
			GetOwningPlayer(),
			MakePlayerPaneContext(),
			TEXT("Player"));
	}
}

void URpgPlayerInventoryWidget::UnbindInventoryScreenPresentation()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
	}
}

void URpgPlayerInventoryWidget::ForwardInventoryInteractionContextToChildren()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->SetInteractionContext(
			MakePlayerPaneContext(),
			TEXT("Player"));
	}
}

void URpgPlayerInventoryWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->RegisterNavigationPanels(Navigator);
	}
}

FName URpgPlayerInventoryWidget::GetInitialInventoryNavigationPanelId() const
{
	return PlayerInventoryPane
		? PlayerInventoryPane->GetPreferredNavigationPanelId()
		: NAME_None;
}

void URpgPlayerInventoryWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->AppendSpatialGrids(OutGrids);
	}
}

bool URpgPlayerInventoryWidget::RouteInventoryPayloadToScreenSpecificTarget(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	UWidget* Target = nullptr;
	if (!PlayerInventoryPane ||
		!PlayerInventoryPane->ResolveNonSpatialDropTarget(
			GhostCenterScreenPosition,
			Target) ||
		!Target)
	{
		return false;
	}

	bOutTargetAddressed = true;
	SwitchActivePointerDropTarget(Target);
	return PlayerInventoryPane->ApplyPayloadToNonSpatialDropTarget(
		Target,
		Payload,
		GhostCenterScreenPosition,
		bCommit);
}

void URpgPlayerInventoryWidget::ClearInventoryScreenSpecificDragPreviews()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ClearExternalDragPreviews();
	}
}

bool URpgPlayerInventoryWidget::UpdateInventoryScreenSpecificControllerDragVisual(
	const FRpgInventoryDragPayload& Payload)
{
	FVector2D AnchorScreenPosition = FVector2D::ZeroVector;
	if (!PlayerInventoryPane ||
		!PlayerInventoryPane->ResolveControllerDragVisualAnchor(
			AnchorScreenPosition))
	{
		return false;
	}

	UpdateFreePointerDragVisual(
		Payload,
		AnchorScreenPosition,
		nullptr,
		true);
	return true;
}

void URpgPlayerInventoryWidget::RefreshInventoryScreenSpecificInteractionPresentation(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->RefreshInteractionPresentation(
			PreviewState,
			bHasPayload,
			bPendingRequest);
	}
}

FRpgInventoryScreenPresentationContext
URpgPlayerInventoryWidget::MakePlayerPaneContext() const
{
	FRpgInventoryScreenPresentationContext Context;
	Context.DragDropCoordinator = GetScreenDragDropCoordinator();
	Context.PanelNavigationCoordinator =
		GetScreenPanelNavigationCoordinator();
	Context.PresentationHost =
		const_cast<URpgPlayerInventoryWidget*>(this);
	return Context;
}

void URpgPlayerInventoryWidget::HandlePlayerInventoryPaneNavigationPanelsChanged()
{
	if (IsActivated())
	{
		QueueDeferredInventoryScreenRefresh();
	}
}
