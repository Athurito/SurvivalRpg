#include "RpgStorageInventoryWidget.h"

#include "Components/TextBlock.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryPanelViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryPaneWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStorageInventoryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgStorageInventoryWidget, Log, All);

URpgPlayerInventoryViewModel*
URpgStorageInventoryWidget::GetStoragePlayerInventoryViewModel() const
{
	return PlayerInventoryPane
		? PlayerInventoryPane->GetPlayerInventoryViewModel()
		: nullptr;
}

void URpgStorageInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RefreshStorageTransferPresentation();

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
	}
}

void URpgStorageInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshStorageTransferPresentation();

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->OnNavigationPanelsChanged.RemoveAll(this);
		PlayerInventoryPane->OnNavigationPanelsChanged.AddUObject(
			this,
			&ThisClass::HandlePlayerInventoryPaneNavigationPanelsChanged);
	}
}

void URpgStorageInventoryWidget::NativeDestruct()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->OnNavigationPanelsChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

UWidget* URpgStorageInventoryWidget::NativeGetDesiredFocusTarget() const
{
	return SecondaryInventoryGrid
		? SecondaryInventoryGrid
		: Super::NativeGetDesiredFocusTarget();
}

void URpgStorageInventoryWidget::ReceiveScreenPayload_Implementation(UObject* Payload)
{
	ApplyInventoryScreenPayload(Payload);
}

void URpgStorageInventoryWidget::BindInventoryScreenPresentation()
{
	// CommonGame normally delivers the payload during Initialize, before the activatable widget is pushed.
	// The shared base has now created the screen-owned interaction objects; this hook performs the one actual bind.
	if (!BindStorageScreenContext())
	{
		ResetStorageScreenContext();
	}
}

void URpgStorageInventoryWidget::UnbindInventoryScreenPresentation()
{
	ResetStorageScreenContext();
}

void URpgStorageInventoryWidget::ForwardInventoryInteractionContextToChildren()
{
	if (!bStorageContextBound)
	{
		return;
	}

	URpgInventoryDragDropCoordinator* Coordinator = GetScreenDragDropCoordinator();
	URpgInventoryPanelNavigationCoordinator* Navigator = GetScreenPanelNavigationCoordinator();
	if (PlayerInventoryPane)
	{
		FRpgInventoryScreenPresentationContext Context;
		Context.DragDropCoordinator = Coordinator;
		Context.PanelNavigationCoordinator = Navigator;
		Context.PresentationHost = this;
		PlayerInventoryPane->SetInteractionContext(Context, TEXT("Player"));
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
	URpgInventoryPanelNavigationCoordinator* Navigator = GetScreenPanelNavigationCoordinator();
	if (!PlayerInventoryPane || !Coordinator || !Navigator)
	{
		UE_LOG(
			LogRpgStorageInventoryWidget,
			Error,
			TEXT("%s rejected Storage presentation because the required player pane or screen interaction context is missing."),
			*GetNameSafe(this));
		ResetStorageScreenContext();
		return false;
	}

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

	// Establish the storage root before binding the reusable player pane. Pane binding can synchronously register
	// dynamic content panels; registering Secondary.Root first preserves the screen's canonical initial source.
	EnsureSecondaryPanelViewModel();
	BindSecondarySpatialGrid();
	if (SecondaryInventoryGrid && SecondaryInventory)
	{
		Navigator->RegisterSpatialInventoryPanel(
			TEXT("Secondary.Root"),
			SecondaryInventoryGrid,
			SecondaryInventory);
	}

	FRpgInventoryScreenPresentationContext PanePresentationContext;
	PanePresentationContext.DragDropCoordinator = Coordinator;
	PanePresentationContext.PanelNavigationCoordinator = Navigator;
	PanePresentationContext.PresentationHost = this;
	PlayerInventoryPane->BindPlayerInventory(
		GetOwningPlayer(),
		PanePresentationContext,
		TEXT("Player"));

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

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->RegisterNavigationPanels(Navigator);
	}

	if (SecondaryInventoryGrid && SecondaryInventory)
	{
		SecondaryInventoryGrid->SetDragDropCoordinator(GetScreenDragDropCoordinator());
		SecondaryInventoryGrid->SetPanelNavigationCoordinator(Navigator, TEXT("Secondary.Root"));
		Navigator->RegisterSpatialInventoryPanel(TEXT("Secondary.Root"), SecondaryInventoryGrid, SecondaryInventory);
	}
}

FName URpgStorageInventoryWidget::GetInitialInventoryNavigationPanelId() const
{
	return TEXT("Secondary.Root");
}

void URpgStorageInventoryWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->AppendSpatialGrids(OutGrids);
	}

	if (SecondaryInventoryGrid)
	{
		OutGrids.AddUnique(SecondaryInventoryGrid);
	}
}

bool URpgStorageInventoryWidget::RouteInventoryPayloadToScreenSpecificTarget(
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

void URpgStorageInventoryWidget::ClearInventoryScreenSpecificDragPreviews()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ClearExternalDragPreviews();
	}
}

bool URpgStorageInventoryWidget::UpdateInventoryScreenSpecificControllerDragVisual(
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

void URpgStorageInventoryWidget::RefreshInventoryScreenSpecificInteractionPresentation(
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

void URpgStorageInventoryWidget::NativeOnInventoryActivePanelChanged(
	FName PanelId,
	int32 PanelIndex)
{
	Super::NativeOnInventoryActivePanelChanged(PanelId, PanelIndex);
	RefreshStorageTransferPresentation();
}

FText URpgStorageInventoryWidget::ResolveQuickTransferDisplayName() const
{
	const URpgInventoryPanelNavigationCoordinator* Navigator =
		GetInventoryPanelNavigator();
	URpgInventoryManagerComponent* ActiveInventory = Navigator
		? Navigator->GetActiveInventory()
		: nullptr;
	const bool bPlayerPanelActive = Navigator &&
		Navigator->GetActivePanelId().ToString().StartsWith(TEXT("Player."));
	if (Navigator && Navigator->GetActiveEquipmentSlotWidget())
	{
		// Equipment quick-transfer is intentionally a player-internal unequip into Backpack/Pockets. It does not
		// consume the screen's Player -> Storage route, so its destination must remain the player inventory.
		return NSLOCTEXT(
			"RpgStorageInventoryWidget",
			"QuickTransferEquipmentToInventory",
			"Transfer \u2192 Inventory");
	}
	if (bPlayerPanelActive ||
		(ActiveInventory && ActiveInventory == PrimaryInventory))
	{
		const FText AuthoredStorageTitle = StorageTitle
			? StorageTitle->GetText()
			: FText::GetEmpty();
		const FText DestinationName = !AuthoredStorageTitle.IsEmptyOrWhitespace()
			? AuthoredStorageTitle
			: NSLOCTEXT(
				"RpgStorageInventoryWidget",
				"DefaultStorageDestination",
				"Storage");
		return FText::Format(
			NSLOCTEXT(
				"RpgStorageInventoryWidget",
				"QuickTransferToStorageFormat",
				"Transfer \u2192 {0}"),
			DestinationName);
	}
	if (ActiveInventory && ActiveInventory == SecondaryInventory)
	{
		return NSLOCTEXT(
			"RpgStorageInventoryWidget",
			"QuickTransferToInventory",
			"Transfer \u2192 Inventory");
	}

	return Super::ResolveQuickTransferDisplayName();
}

void URpgStorageInventoryWidget::EnsureSecondaryPanelViewModel()
{
	if (!SecondaryPanelViewModel)
	{
		SecondaryPanelViewModel = NewObject<URpgInventoryPanelViewModel>(this);
	}
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

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
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
	InventoryScreenPayload = nullptr;
	PrimaryInventory = nullptr;
	SecondaryInventory = nullptr;
	SecondaryRootHandle = FRpgInventoryContainerHandle();
	RefreshStorageTransferPresentation();
}

void URpgStorageInventoryWidget::RefreshStorageTransferPresentation()
{
	const URpgInventoryPanelNavigationCoordinator* Navigator =
		GetInventoryPanelNavigator();
	URpgInventoryManagerComponent* ActiveInventory = Navigator
		? Navigator->GetActiveInventory()
		: nullptr;
	const bool bPlayerSourceActive =
		Navigator &&
		(Navigator->GetActivePanelId().ToString().StartsWith(TEXT("Player.")) ||
			(PrimaryInventory && ActiveInventory == PrimaryInventory));
	const bool bStorageSourceActive =
		SecondaryInventory &&
		ActiveInventory == SecondaryInventory;

	if (PlayerTitle)
	{
		PlayerTitle->SetColorAndOpacity(FSlateColor(
			bPlayerSourceActive
				? ActiveInventoryTitleColor
				: InactiveInventoryTitleColor));
	}
	if (StorageTitle)
	{
		StorageTitle->SetColorAndOpacity(FSlateColor(
			bStorageSourceActive
				? ActiveInventoryTitleColor
				: InactiveInventoryTitleColor));
	}
}

void URpgStorageInventoryWidget::HandlePlayerInventoryPaneNavigationPanelsChanged()
{
	if (bStorageContextBound)
	{
		QueueDeferredInventoryScreenRefresh();
	}
}
