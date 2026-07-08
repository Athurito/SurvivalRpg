#include "RpgPlayerInventoryWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "TimerManager.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryWidget)

namespace
{
	bool IsWidgetUnderScreenPosition(const UWidget* Widget, FVector2D ScreenPosition)
	{
		if (!Widget || Widget->GetVisibility() == ESlateVisibility::Collapsed || Widget->GetVisibility() == ESlateVisibility::Hidden)
		{
			return false;
		}

		const FGeometry Geometry = Widget->GetCachedGeometry();
		const FVector2D LocalPosition = Geometry.AbsoluteToLocal(ScreenPosition);
		const FVector2D LocalSize = Geometry.GetLocalSize();
		return LocalSize.X > KINDA_SMALL_NUMBER &&
			LocalSize.Y > KINDA_SMALL_NUMBER &&
			LocalPosition.X >= 0.0f &&
			LocalPosition.Y >= 0.0f &&
			LocalPosition.X <= LocalSize.X &&
			LocalPosition.Y <= LocalSize.Y;
	}
}

URpgPlayerInventoryWidget::URpgPlayerInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgPlayerInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();
	BP_OnPlayerInventoryViewModelReady(PlayerInventoryViewModel);
}

void URpgPlayerInventoryWidget::NativeOnActivated()
{
	EnsurePlayerInventoryCoordinator();
	EnsurePlayerInventoryPanelNavigator();
	BindPlayerInventoryViewModel();

	Super::NativeOnActivated();
	RefreshInventoryControllerFocus();
	QueueDeferredPlayerInventoryRefresh();
}

void URpgPlayerInventoryWidget::NativeOnDeactivated()
{
	if (PlayerDragDropCoordinator && PlayerDragDropCoordinator->HasHeldPayload())
	{
		PlayerDragDropCoordinator->CancelHold();
	}

	Super::NativeOnDeactivated();
}

bool URpgPlayerInventoryWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation || !URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}

	RouteInventoryPayloadAtScreenPosition(InventoryOperation->InventoryPayload, InDragDropEvent.GetScreenSpacePosition(), false);
	return true;
}

bool URpgPlayerInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const URpgInventoryDragDropOperation* InventoryOperation = Cast<URpgInventoryDragDropOperation>(InOperation);
	if (!InventoryOperation || !URpgInventoryDragDropCoordinator::IsPayloadValid(InventoryOperation->InventoryPayload))
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	RouteInventoryPayloadAtScreenPosition(InventoryOperation->InventoryPayload, InDragDropEvent.GetScreenSpacePosition(), true);
	return true;
}

void URpgPlayerInventoryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearExternalDragPreviews();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryCoordinator()
{
	if (!PlayerDragDropCoordinator)
	{
		PlayerDragDropCoordinator = URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(this, GetOwningPlayer());
	}

	ForwardCoordinatorToChildren();
	SetInventoryControllerCoordinators(PlayerPanelNavigationCoordinator, PlayerDragDropCoordinator);
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryPanelNavigator()
{
	if (!PlayerPanelNavigationCoordinator)
	{
		PlayerPanelNavigationCoordinator = URpgInventoryPanelNavigationCoordinator::CreateInventoryPanelNavigationCoordinator(this, GetOwningPlayer(), PlayerDragDropCoordinator);
	}

	SetInventoryControllerCoordinators(PlayerPanelNavigationCoordinator, PlayerDragDropCoordinator);
	RegisterPlayerInventoryNavigationPanels();
}

void URpgPlayerInventoryWidget::BindPlayerInventoryViewModel()
{
	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();

	if (PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel->BindPlayerController(GetOwningPlayer());
	}

	RefreshPlayerInventoryViews();
}

void URpgPlayerInventoryWidget::RefreshPlayerInventoryViews()
{
	RegisterPlayerInventoryNavigationPanels();
	RefreshGearSlots();
	RefreshSlotGroups();
	RefreshActionBar();
}

FString URpgPlayerInventoryWidget::GetPlayerInventoryWidgetDebugSummary() const
{
	const int32 CarryCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetCarryGroups().Num() : INDEX_NONE;
	const int32 InventoryGroupCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetInventoryGroups().Num() : INDEX_NONE;
	const int32 ActionBarCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetActionBarSlots().Num() : INDEX_NONE;

	return FString::Printf(
		TEXT("PlayerInventoryWidget VM=%s Coordinator=%s CarryGroupsList=%s InventoryGroupsList=%s ActionBarTileView=%s CarryGroups=%d InventoryGroups=%d ActionBarSlots=%d"),
		*GetNameSafe(PlayerInventoryViewModel),
		*GetNameSafe(PlayerDragDropCoordinator),
		*GetNameSafe(CarryGroupsList),
		*GetNameSafe(InventoryGroupsList),
		*GetNameSafe(ActionBarTileView),
		CarryCount,
		InventoryGroupCount,
		ActionBarCount);
}

void URpgPlayerInventoryWidget::RefreshSlotGroups()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	if (CarryGroupsList)
	{
		CarryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Carry"));
		CarryGroupsList->SetSlotGroupItems(PlayerInventoryViewModel->GetCarryGroups());
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Content"));
		InventoryGroupsList->SetSlotGroupItems(PlayerInventoryViewModel->GetInventoryGroups());
	}
}

void URpgPlayerInventoryWidget::RefreshActionBar()
{
	if (!PlayerInventoryViewModel || !ActionBarTileView)
	{
		return;
	}

	ActionBarTileView->SetActionBarSlotItems(PlayerInventoryViewModel->GetActionBarSlots());
	if (PlayerPanelNavigationCoordinator)
	{
		PlayerPanelNavigationCoordinator->RegisterActionBarPanel(TEXT("Actionbar"), ActionBarTileView);
	}
}

void URpgPlayerInventoryWidget::RefreshGearSlots()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	SetGearSlotViewModel(Gear_Head, ERpgEquipmentSlot::Head, false);
	SetGearSlotViewModel(Gear_Chest, ERpgEquipmentSlot::Chest, false);
	SetGearSlotViewModel(Gear_Hands, ERpgEquipmentSlot::Hands, false);
	SetGearSlotViewModel(Gear_Legs, ERpgEquipmentSlot::Legs, false);
	SetGearSlotViewModel(Gear_Feet, ERpgEquipmentSlot::Feet, false);

	SetGearSlotViewModel(Gear_Backpack, ERpgEquipmentSlot::Backpack, true);
	SetGearSlotViewModel(Gear_Belt, ERpgEquipmentSlot::Belt, true);
	SetGearSlotViewModel(Gear_Pouch, ERpgEquipmentSlot::Pouch, true);
	SetGearSlotViewModel(Gear_ResourceBag, ERpgEquipmentSlot::ResourceBag, true);
}

void URpgPlayerInventoryWidget::HandleGearSlotsChanged()
{
	RefreshGearSlots();
}

void URpgPlayerInventoryWidget::HandleSlotGroupsChanged()
{
	RefreshSlotGroups();
	QueueDeferredPlayerInventoryRefresh();
}

void URpgPlayerInventoryWidget::HandleActionBarSlotsChanged()
{
	RefreshActionBar();
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryViewModel()
{
	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->InitializeSources();
		for (const FMVVMView_Source& Source : View->GetSources())
		{
			if (URpgPlayerInventoryViewModel* ExistingViewModel = Cast<URpgPlayerInventoryViewModel>(Source.Source))
			{
				if (PlayerInventoryViewModel != ExistingViewModel)
				{
					bViewModelDelegatesBound = false;
				}
				PlayerInventoryViewModel = ExistingViewModel;
				return;
			}
		}
	}

	if (!PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel = NewObject<URpgPlayerInventoryViewModel>(this);
	}
}

void URpgPlayerInventoryWidget::BindViewModelDelegates()
{
	if (!PlayerInventoryViewModel || bViewModelDelegatesBound)
	{
		return;
	}

	PlayerInventoryViewModel->OnGearSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleGearSlotsChanged);
	PlayerInventoryViewModel->OnSlotGroupsChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotGroupsChanged);
	PlayerInventoryViewModel->OnActionBarSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleActionBarSlotsChanged);
	bViewModelDelegatesBound = true;
}

void URpgPlayerInventoryWidget::SetGearSlotViewModel(URpgEquipmentSlotWidget* GearSlotWidget, ERpgEquipmentSlot EquipmentSlot, bool bBagSlot) const
{
	if (!GearSlotWidget || !PlayerInventoryViewModel)
	{
		return;
	}

	GearSlotWidget->SetDragDropCoordinator(PlayerDragDropCoordinator);
	GearSlotWidget->SetEquipmentSlotViewModel(
		bBagSlot
			? PlayerInventoryViewModel->GetBagSlot(EquipmentSlot)
			: PlayerInventoryViewModel->GetArmorSlot(EquipmentSlot));
}

void URpgPlayerInventoryWidget::ForwardCoordinatorToChildren()
{
	if (CarryGroupsList)
	{
		CarryGroupsList->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}

	if (ActionBarTileView)
	{
		ActionBarTileView->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}

	if (Gear_Head)
	{
		Gear_Head->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Chest)
	{
		Gear_Chest->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Hands)
	{
		Gear_Hands->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Legs)
	{
		Gear_Legs->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Feet)
	{
		Gear_Feet->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Backpack)
	{
		Gear_Backpack->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Belt)
	{
		Gear_Belt->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_Pouch)
	{
		Gear_Pouch->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
	if (Gear_ResourceBag)
	{
		Gear_ResourceBag->SetDragDropCoordinator(PlayerDragDropCoordinator);
	}
}

void URpgPlayerInventoryWidget::RegisterPlayerInventoryNavigationPanels()
{
	if (!PlayerPanelNavigationCoordinator)
	{
		return;
	}

	PlayerPanelNavigationCoordinator->ClearPanels();

	if (Gear_Head)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Head"), Gear_Head);
	}
	if (Gear_Chest)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Chest"), Gear_Chest);
	}
	if (Gear_Hands)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Hands"), Gear_Hands);
	}
	if (Gear_Legs)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Legs"), Gear_Legs);
	}
	if (Gear_Feet)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Feet"), Gear_Feet);
	}
	if (Gear_Backpack)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Backpack"), Gear_Backpack);
	}
	if (Gear_Belt)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Belt"), Gear_Belt);
	}
	if (Gear_Pouch)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.Pouch"), Gear_Pouch);
	}
	if (Gear_ResourceBag)
	{
		PlayerPanelNavigationCoordinator->RegisterEquipmentPanel(TEXT("Gear.ResourceBag"), Gear_ResourceBag);
	}

	if (CarryGroupsList)
	{
		CarryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Carry"));
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->SetPanelNavigationCoordinator(PlayerPanelNavigationCoordinator, TEXT("Content"));
	}

	if (ActionBarTileView)
	{
		PlayerPanelNavigationCoordinator->RegisterActionBarPanel(TEXT("Actionbar"), ActionBarTileView);
	}
}

bool URpgPlayerInventoryWidget::RouteInventoryPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit)
{
	ClearExternalDragPreviews();

	if (RoutePayloadToGearSlot(Payload, ScreenPosition, bCommit))
	{
		return true;
	}

	if (RoutePayloadToActionBar(Payload, ScreenPosition, bCommit))
	{
		return true;
	}

	if (RoutePayloadToSpatialGrid(Payload, ScreenPosition, bCommit))
	{
		return true;
	}

	return false;
}

bool URpgPlayerInventoryWidget::RoutePayloadToGearSlot(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit)
{
	auto TryRouteSlot = [&](URpgEquipmentSlotWidget* SlotWidget)
	{
		if (!IsWidgetUnderScreenPosition(SlotWidget, ScreenPosition))
		{
			return false;
		}

		if (bCommit)
		{
			SlotWidget->CommitPayloadDrop(Payload);
		}
		else
		{
			SlotWidget->PreviewPayloadDrop(Payload);
		}
		return true;
	};

	return TryRouteSlot(Gear_Head) ||
		TryRouteSlot(Gear_Chest) ||
		TryRouteSlot(Gear_Hands) ||
		TryRouteSlot(Gear_Legs) ||
		TryRouteSlot(Gear_Feet) ||
		TryRouteSlot(Gear_Backpack) ||
		TryRouteSlot(Gear_Belt) ||
		TryRouteSlot(Gear_Pouch) ||
		TryRouteSlot(Gear_ResourceBag);
}

bool URpgPlayerInventoryWidget::RoutePayloadToActionBar(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit)
{
	if (!ActionBarTileView || !IsWidgetUnderScreenPosition(ActionBarTileView, ScreenPosition))
	{
		return false;
	}

	return bCommit
		? ActionBarTileView->CommitPayloadAtScreenPosition(Payload, ScreenPosition)
		: ActionBarTileView->PreviewPayloadAtScreenPosition(Payload, ScreenPosition);
}

bool URpgPlayerInventoryWidget::RoutePayloadToSpatialGrid(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition, bool bCommit)
{
	TArray<URpgInventorySpatialGridWidget*> SpatialGrids;
	CollectSpatialGrids(SpatialGrids);
	for (URpgInventorySpatialGridWidget* SpatialGrid : SpatialGrids)
	{
		if (!SpatialGrid || !SpatialGrid->ContainsScreenPosition(ScreenPosition))
		{
			continue;
		}

		if (bCommit)
		{
			SpatialGrid->CommitPayloadAtScreenPosition(Payload, ScreenPosition);
		}
		else
		{
			SpatialGrid->PreviewPayloadAtScreenPosition(Payload, ScreenPosition);
		}
		return true;
	}

	return false;
}

void URpgPlayerInventoryWidget::CollectSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (CarryGroupsList)
	{
		CarryGroupsList->GetSpatialGridWidgets(OutGrids);
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->GetSpatialGridWidgets(OutGrids);
	}
}

void URpgPlayerInventoryWidget::ClearExternalDragPreviews()
{
	if (Gear_Head)
	{
		Gear_Head->ClearExternalPreviewPayload();
	}
	if (Gear_Chest)
	{
		Gear_Chest->ClearExternalPreviewPayload();
	}
	if (Gear_Hands)
	{
		Gear_Hands->ClearExternalPreviewPayload();
	}
	if (Gear_Legs)
	{
		Gear_Legs->ClearExternalPreviewPayload();
	}
	if (Gear_Feet)
	{
		Gear_Feet->ClearExternalPreviewPayload();
	}
	if (Gear_Backpack)
	{
		Gear_Backpack->ClearExternalPreviewPayload();
	}
	if (Gear_Belt)
	{
		Gear_Belt->ClearExternalPreviewPayload();
	}
	if (Gear_Pouch)
	{
		Gear_Pouch->ClearExternalPreviewPayload();
	}
	if (Gear_ResourceBag)
	{
		Gear_ResourceBag->ClearExternalPreviewPayload();
	}

	if (ActionBarTileView)
	{
		ActionBarTileView->ClearExternalPreviewPayloads();
	}

	TArray<URpgInventorySpatialGridWidget*> SpatialGrids;
	CollectSpatialGrids(SpatialGrids);
	for (URpgInventorySpatialGridWidget* SpatialGrid : SpatialGrids)
	{
		if (SpatialGrid)
		{
			SpatialGrid->ClearExternalPreviewPayload();
		}
	}
}

void URpgPlayerInventoryWidget::QueueDeferredPlayerInventoryRefresh()
{
	if (bDeferredPlayerInventoryRefreshQueued)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ExecuteDeferredPlayerInventoryRefresh();
		return;
	}

	bDeferredPlayerInventoryRefreshQueued = true;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::ExecuteDeferredPlayerInventoryRefresh));
}

void URpgPlayerInventoryWidget::ExecuteDeferredPlayerInventoryRefresh()
{
	bDeferredPlayerInventoryRefreshQueued = false;

	ForwardCoordinatorToChildren();
	RefreshPlayerInventoryViews();
	RefreshInventoryControllerFocus();
}
