#include "RpgPlayerInventoryWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryCarrySlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgPlayerInventoryWidget, Log, All);

const FName URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName(
	TEXT("RpgPlayerInventoryViewModel"));

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
	InjectPlayerInventoryViewModelIntoMvvm();
}

void URpgPlayerInventoryWidget::BindInventoryScreenPresentation()
{
	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();
	// Manual MVVM sources generate an ExposeOnSpawn setter in UE 5.8. A Blueprint Create Widget node can therefore
	// assign that source after NativeOnInitialized. Reassert the native-owned instance at the activation boundary so
	// the presenter and every MVVM leaf always observe the same screen-scoped projection.
	InjectPlayerInventoryViewModelIntoMvvm();

	if (PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel->BindPlayerController(GetOwningPlayer());
	}

	RefreshPlayerInventoryViews();
}

void URpgPlayerInventoryWidget::UnbindInventoryScreenPresentation()
{
	if (PlayerInventoryViewModel)
	{
		// CommonUI may pool an inactive inventory screen. Stop observing replicated gameplay state while the
		// presentation is closed so pooled widgets cannot rebuild projections or retain stale player references.
		PlayerInventoryViewModel->UnbindPlayerInventory();
	}
}

void URpgPlayerInventoryWidget::RefreshPlayerInventoryViews()
{
	RefreshGearSlots();
	RefreshSlotGroups();
	RefreshActionBar();
	RefreshInventoryScreenNavigationPanels();
}

FString URpgPlayerInventoryWidget::GetPlayerInventoryWidgetDebugSummary() const
{
	const int32 CarryCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetCarryGroups().Num() : INDEX_NONE;
	const int32 InventoryGroupCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetInventoryGroups().Num() : INDEX_NONE;
	const int32 ActionBarCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetActionBarSlots().Num() : INDEX_NONE;

	return FString::Printf(
		TEXT("PlayerInventoryWidget VM=%s Coordinator=%s Carry1=%s Carry2=%s Offhand=%s Pockets=%s Backpack=%s ActionBar=%s CarryGroups=%d InventoryGroups=%d ActionBarSlots=%d"),
		*GetNameSafe(PlayerInventoryViewModel),
		*GetNameSafe(GetScreenDragDropCoordinator()),
		*GetNameSafe(Carry_Weapon1),
		*GetNameSafe(Carry_Weapon2),
		*GetNameSafe(Carry_Offhand),
		*GetNameSafe(Content_Pockets),
		*GetNameSafe(Content_Backpack),
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

	auto ReportMissingContentHost = [this](URpgInventorySlotGroupWidget* GroupWidget, FName BindingName)
	{
		if (!GroupWidget && !ReportedInvalidPlayerBindings.Contains(BindingName))
		{
			ReportedInvalidPlayerBindings.Add(BindingName);
			UE_LOG(LogRpgPlayerInventoryWidget, Warning,
				TEXT("%s has no direct %s widget. Add an RpgInventorySlotGroupWidget with that exact BindWidget name; player InventoryGroupsList fallback was removed."),
				*GetNameSafe(this),
				*BindingName.ToString());
		}
	};

	auto BindContentGroup = [this, &ReportMissingContentHost](
		URpgInventorySlotGroupWidget* GroupWidget,
		URpgInventorySlotGroupViewModel* GroupViewModel,
		FName BindingName)
	{
		if (!GroupWidget)
		{
			ReportMissingContentHost(GroupWidget, BindingName);
			return;
		}

		GroupWidget->SetDragDropCoordinator(GetScreenDragDropCoordinator());
		GroupWidget->SetPanelNavigationCoordinator(GetScreenPanelNavigationCoordinator(), TEXT("Content"));
		GroupWidget->SetSlotGroupViewModel(GroupViewModel);
		if (URpgInventorySpatialGridWidget* SpatialGrid = GroupWidget->GetSpatialGridWidget())
		{
			SpatialGrid->SetInventoryPresentationHost(this);
		}
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(GroupWidget->Slot))
		{
			// Spatial children must own their complete title + grid geometry; undersized hosts break Slate hit testing.
			CanvasSlot->SetAutoSize(true);
		}
		GroupWidget->SetVisibility(GroupViewModel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	auto BindCarrySlot = [this](
		UWidget* BoundWidget,
		FName BindingName,
		URpgInventorySlotGroupViewModel* GroupViewModel)
	{
		URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, true);
		if (!CarrySlot)
		{
			return;
		}

		CarrySlot->SetDragDropCoordinator(GetScreenDragDropCoordinator());
		CarrySlot->SetInventoryPresentationHost(this);
		CarrySlot->SetCarrySlotGroupViewModel(GroupViewModel);
		CarrySlot->SetVisibility(GroupViewModel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	BindCarrySlot(Carry_Weapon1, TEXT("Carry_Weapon1"), PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId));
	BindCarrySlot(Carry_Weapon2, TEXT("Carry_Weapon2"), PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId));
	BindCarrySlot(Carry_Offhand, TEXT("Carry_Offhand"), PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId));

	BindContentGroup(Content_Pockets, PlayerInventoryViewModel->GetSlotGroup(URpgPlayerInventoryLayoutComponent::PocketsGroupId), TEXT("Content_Pockets"));
	BindContentGroup(Content_Backpack, FindEquipmentProvidedContentGroup(TEXT("Backpack")), TEXT("Content_Backpack"));
	BindContentGroup(Content_Belt, FindEquipmentProvidedContentGroup(TEXT("Belt")), TEXT("Content_Belt"));
	BindContentGroup(Content_Pouch, FindEquipmentProvidedContentGroup(TEXT("Pouch")), TEXT("Content_Pouch"));
	BindContentGroup(Content_ResourceBag, FindEquipmentProvidedContentGroup(TEXT("ResourceBag")), TEXT("Content_ResourceBag"));
}

void URpgPlayerInventoryWidget::RefreshActionBar()
{
	if (!PlayerInventoryViewModel || !ActionBarTileView)
	{
		return;
	}

	ActionBarTileView->SetActionBarSlotItems(PlayerInventoryViewModel->GetActionBarSlots());
	if (URpgInventoryPanelNavigationCoordinator* Navigator = GetScreenPanelNavigationCoordinator())
	{
		Navigator->RegisterActionBarPanel(TEXT("Actionbar"), ActionBarTileView);
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
	QueueDeferredInventoryScreenRefresh();
}

void URpgPlayerInventoryWidget::HandleActionBarSlotsChanged()
{
	RefreshActionBar();
}

bool URpgPlayerInventoryWidget::UpdateInventoryScreenSpecificControllerDragVisual(
	const FRpgInventoryDragPayload& Payload)
{
	URpgInventoryCarrySlotWidget* CarrySlot = FindControllerPreviewCarrySlot();
	if (!CarrySlot)
	{
		return false;
	}

	UpdateControllerCarryDragVisual(Payload, CarrySlot);
	return true;
}

void URpgPlayerInventoryWidget::RefreshInventoryScreenSpecificInteractionPresentation(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	(void)PreviewState;
	(void)bHasPayload;
	(void)bPendingRequest;

	// Carry slots expose semantic Pending/Rejected state through their presentation hook. Refresh all three because
	// the interaction session target can change without replacing the held payload delegate they otherwise observe.
	auto RefreshCarryPresentation = [this](UWidget* BoundWidget, FName BindingName)
	{
		if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false))
		{
			CarrySlot->RefreshCarrySlotPresentation();
		}
	};
	RefreshCarryPresentation(Carry_Weapon1, TEXT("Carry_Weapon1"));
	RefreshCarryPresentation(Carry_Weapon2, TEXT("Carry_Weapon2"));
	RefreshCarryPresentation(Carry_Offhand, TEXT("Carry_Offhand"));
}

void URpgPlayerInventoryWidget::UpdateControllerCarryDragVisual(
	const FRpgInventoryDragPayload& Payload,
	URpgInventoryCarrySlotWidget* CarrySlotWidget)
{
	if (!CarrySlotWidget)
	{
		ClearFreePointerDragVisual();
		return;
	}

	const FGeometry CarryGeometry = CarrySlotWidget->GetCachedGeometry();
	const FVector2D CarryLocalSize = CarryGeometry.GetLocalSize();
	if (CarryLocalSize.X <= KINDA_SMALL_NUMBER || CarryLocalSize.Y <= KINDA_SMALL_NUMBER)
	{
		ClearFreePointerDragVisual();
		return;
	}

	const FVector2D CarryCenterScreenPosition = CarryGeometry.LocalToAbsolute(CarryLocalSize * 0.5f);
	UpdateFreePointerDragVisual(Payload, CarryCenterScreenPosition, nullptr, true);
}

URpgInventoryCarrySlotWidget* URpgPlayerInventoryWidget::FindControllerPreviewCarrySlot() const
{
	auto ResolveTargetedCarrySlot = [this](UWidget* BoundWidget, FName BindingName)
		-> URpgInventoryCarrySlotWidget*
	{
		URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false);
		return CarrySlot &&
			CarrySlot->GetCarryInteractionPreviewState() != ERpgInventoryInteractionPreviewState::None
			? CarrySlot
			: nullptr;
	};

	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveTargetedCarrySlot(Carry_Weapon1, TEXT("Carry_Weapon1")))
	{
		return CarrySlot;
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveTargetedCarrySlot(Carry_Weapon2, TEXT("Carry_Weapon2")))
	{
		return CarrySlot;
	}
	return ResolveTargetedCarrySlot(Carry_Offhand, TEXT("Carry_Offhand"));
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryViewModel()
{
	if (!PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel = NewObject<URpgPlayerInventoryViewModel>(this);
	}
}

bool URpgPlayerInventoryWidget::InjectPlayerInventoryViewModelIntoMvvm()
{
	if (!PlayerInventoryViewModel)
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		UE_LOG(
			LogRpgPlayerInventoryWidget,
			Error,
			TEXT("%s has no compiled MVVM view. Author one manual %s source for the native player-inventory VM."),
			*GetNameSafe(this),
			*PlayerInventoryViewModelSourceName.ToString());
		return false;
	}

	const FMVVMViewClass_Source* CompiledSource = ViewClass->GetSources().FindByPredicate(
		[](const FMVVMViewClass_Source& Candidate)
		{
			return Candidate.IsViewModel() &&
				Candidate.GetName() == PlayerInventoryViewModelSourceName;
		});
	if (!CompiledSource ||
		!CompiledSource->CanBeSet() ||
		!PlayerInventoryViewModel->IsA(CompiledSource->GetSourceClass()))
	{
		UE_LOG(
			LogRpgPlayerInventoryWidget,
			Error,
			TEXT("%s requires one settable manual MVVM source named %s with type RpgPlayerInventoryViewModel."),
			*GetNameSafe(this),
			*PlayerInventoryViewModelSourceName.ToString());
		return false;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(PlayerInventoryViewModel);
	ViewModelInterface.SetInterface(PlayerInventoryViewModel.Get());
	if (View->GetViewModel(PlayerInventoryViewModelSourceName).GetObject() ==
		PlayerInventoryViewModel)
	{
		return true;
	}

	if (!View->SetViewModel(PlayerInventoryViewModelSourceName, ViewModelInterface))
	{
		UE_LOG(
			LogRpgPlayerInventoryWidget,
			Error,
			TEXT("%s failed to inject its native player-inventory VM into MVVM source %s."),
			*GetNameSafe(this),
			*PlayerInventoryViewModelSourceName.ToString());
		return false;
	}

	return View->GetViewModel(PlayerInventoryViewModelSourceName).GetObject() ==
		PlayerInventoryViewModel;
}

void URpgPlayerInventoryWidget::BindViewModelDelegates()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	PlayerInventoryViewModel->OnGearSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleGearSlotsChanged);
	PlayerInventoryViewModel->OnSlotGroupsChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotGroupsChanged);
	PlayerInventoryViewModel->OnActionBarSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleActionBarSlotsChanged);
}

void URpgPlayerInventoryWidget::SetGearSlotViewModel(
	URpgEquipmentSlotWidget* GearSlotWidget,
	ERpgEquipmentSlot EquipmentSlot,
	bool bBagSlot)
{
	if (!GearSlotWidget || !PlayerInventoryViewModel)
	{
		return;
	}

	GearSlotWidget->SetDragDropCoordinator(GetScreenDragDropCoordinator());
	GearSlotWidget->SetInventoryPresentationHost(this);
	GearSlotWidget->SetEquipmentSlotViewModel(
		bBagSlot
			? PlayerInventoryViewModel->GetBagSlot(EquipmentSlot)
			: PlayerInventoryViewModel->GetArmorSlot(EquipmentSlot));
}

void URpgPlayerInventoryWidget::ForwardInventoryInteractionContextToChildren()
{
	URpgInventoryDragDropCoordinator* Coordinator = GetScreenDragDropCoordinator();

	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneContentGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget)
		{
			GroupWidget->SetDragDropCoordinator(Coordinator);
			if (URpgInventorySpatialGridWidget* SpatialGrid = GroupWidget->GetSpatialGridWidget())
			{
				SpatialGrid->SetInventoryPresentationHost(this);
			}
		}
	}

	auto ForwardCarrySlot = [this](UWidget* BoundWidget, FName BindingName)
	{
		if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false))
		{
			CarrySlot->SetDragDropCoordinator(GetScreenDragDropCoordinator());
			CarrySlot->SetInventoryPresentationHost(this);
		}
	};
	ForwardCarrySlot(Carry_Weapon1, TEXT("Carry_Weapon1"));
	ForwardCarrySlot(Carry_Weapon2, TEXT("Carry_Weapon2"));
	ForwardCarrySlot(Carry_Offhand, TEXT("Carry_Offhand"));

	if (ActionBarTileView)
	{
		ActionBarTileView->SetDragDropCoordinator(Coordinator);
	}

	auto ForwardGearSlot = [this, Coordinator](URpgEquipmentSlotWidget* GearSlot)
	{
		if (GearSlot)
		{
			GearSlot->SetDragDropCoordinator(Coordinator);
			GearSlot->SetInventoryPresentationHost(this);
		}
	};
	ForwardGearSlot(Gear_Head);
	ForwardGearSlot(Gear_Chest);
	ForwardGearSlot(Gear_Hands);
	ForwardGearSlot(Gear_Legs);
	ForwardGearSlot(Gear_Feet);
	ForwardGearSlot(Gear_Backpack);
	ForwardGearSlot(Gear_Belt);
	ForwardGearSlot(Gear_Pouch);
	ForwardGearSlot(Gear_ResourceBag);
}

void URpgPlayerInventoryWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (!Navigator)
	{
		return;
	}

	if (Gear_Head)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Head"), Gear_Head);
	}
	if (Gear_Chest)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Chest"), Gear_Chest);
	}
	if (Gear_Hands)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Hands"), Gear_Hands);
	}
	if (Gear_Legs)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Legs"), Gear_Legs);
	}
	if (Gear_Feet)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Feet"), Gear_Feet);
	}
	if (Gear_Backpack)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Backpack"), Gear_Backpack);
	}
	if (Gear_Belt)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Belt"), Gear_Belt);
	}
	if (Gear_Pouch)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.Pouch"), Gear_Pouch);
	}
	if (Gear_ResourceBag)
	{
		Navigator->RegisterEquipmentPanel(TEXT("Gear.ResourceBag"), Gear_ResourceBag);
	}

	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon1, TEXT("Carry_Weapon1"), false))
	{
		Navigator->RegisterCarrySlotPanel(TEXT("Carry.Weapon1"), CarrySlot);
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon2, TEXT("Carry_Weapon2"), false))
	{
		Navigator->RegisterCarrySlotPanel(TEXT("Carry.Weapon2"), CarrySlot);
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Offhand, TEXT("Carry_Offhand"), false))
	{
		Navigator->RegisterCarrySlotPanel(TEXT("Carry.Offhand"), CarrySlot);
	}

	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneContentGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget)
		{
			GroupWidget->SetPanelNavigationCoordinator(Navigator, TEXT("Content"));
		}
	}

	if (ActionBarTileView)
	{
		Navigator->RegisterActionBarPanel(TEXT("Actionbar"), ActionBarTileView);
	}
}

void URpgPlayerInventoryWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneContentGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget &&
			GroupWidget->GetSlotGroupHandle().IsValid() &&
			GroupWidget->GetSpatialGridWidget())
		{
			OutGrids.AddUnique(GroupWidget->GetSpatialGridWidget());
		}
	}
}

bool URpgPlayerInventoryWidget::RouteInventoryPayloadToScreenSpecificTarget(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	const bool bGearHandled = RoutePayloadToGearSlot(
		Payload,
		GhostCenterScreenPosition,
		bCommit,
		bOutTargetAddressed);
	if (bOutTargetAddressed)
	{
		return bGearHandled;
	}

	const bool bCarryHandled = RoutePayloadToCarrySlot(
		Payload,
		GhostCenterScreenPosition,
		bCommit,
		bOutTargetAddressed);
	if (bOutTargetAddressed)
	{
		return bCarryHandled;
	}

	return RoutePayloadToActionBar(
		Payload,
		GhostCenterScreenPosition,
		bCommit,
		bOutTargetAddressed);
}

bool URpgPlayerInventoryWidget::RoutePayloadToCarrySlot(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	auto TryRouteSlot = [this, &Payload, GhostCenterScreenPosition, bCommit](
		UWidget* BoundWidget,
		FName BindingName,
		bool& bOutAddressed)
	{
		URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(BoundWidget, BindingName, false);
		if (!CarrySlot || !IsWidgetUnderScreenPosition(CarrySlot, GhostCenterScreenPosition))
		{
			return false;
		}

		bOutAddressed = true;
		SwitchActivePointerDropTarget(CarrySlot);
		if (bCommit)
		{
			return CarrySlot->CommitPayloadDrop(Payload);
		}

		CarrySlot->PreviewPayloadDrop(Payload);
		return true;
	};

	return TryRouteSlot(Carry_Weapon1, TEXT("Carry_Weapon1"), bOutTargetAddressed) ||
		TryRouteSlot(Carry_Weapon2, TEXT("Carry_Weapon2"), bOutTargetAddressed) ||
		TryRouteSlot(Carry_Offhand, TEXT("Carry_Offhand"), bOutTargetAddressed);
}

bool URpgPlayerInventoryWidget::RoutePayloadToGearSlot(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	auto TryRouteSlot = [&](URpgEquipmentSlotWidget* SlotWidget)
	{
		if (!IsWidgetUnderScreenPosition(SlotWidget, GhostCenterScreenPosition))
		{
			return false;
		}
		bOutTargetAddressed = true;
		SwitchActivePointerDropTarget(SlotWidget);

		if (bCommit)
		{
			return SlotWidget->CommitPayloadDrop(Payload);
		}

		SlotWidget->PreviewPayloadDrop(Payload);
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

bool URpgPlayerInventoryWidget::RoutePayloadToActionBar(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	if (!ActionBarTileView ||
		!IsWidgetUnderScreenPosition(ActionBarTileView, GhostCenterScreenPosition) ||
		!ActionBarTileView->HasActionBarSlotAtScreenPosition(GhostCenterScreenPosition))
	{
		return false;
	}
	bOutTargetAddressed = true;
	SwitchActivePointerDropTarget(ActionBarTileView);

	if (bCommit)
	{
		return ActionBarTileView->CommitPayloadAtScreenPosition(Payload, GhostCenterScreenPosition);
	}

	return ActionBarTileView->PreviewPayloadAtScreenPosition(Payload, GhostCenterScreenPosition);
}

URpgInventorySlotGroupViewModel* URpgPlayerInventoryWidget::FindEquipmentProvidedContentGroup(FName SourceEquipmentSlotName) const
{
	if (!PlayerInventoryViewModel || SourceEquipmentSlotName.IsNone())
	{
		return nullptr;
	}

	for (URpgInventorySlotGroupViewModel* Group : PlayerInventoryViewModel->GetInventoryGroups())
	{
		if (Group && Group->IsProvidedByEquipment() &&
			Group->GetSourceEquipmentSlotName() == SourceEquipmentSlotName)
		{
			return Group;
		}
	}

	return nullptr;
}

void URpgPlayerInventoryWidget::CollectStandaloneContentGroupWidgets(TArray<URpgInventorySlotGroupWidget*>& OutWidgets) const
{
	OutWidgets.Add(Content_Pockets);
	OutWidgets.Add(Content_Backpack);
	OutWidgets.Add(Content_Belt);
	OutWidgets.Add(Content_Pouch);
	OutWidgets.Add(Content_ResourceBag);
	OutWidgets.Remove(nullptr);
}

URpgInventoryCarrySlotWidget* URpgPlayerInventoryWidget::ResolveCarrySlotWidget(
	UWidget* BoundWidget,
	FName BindingName,
	bool bLogFailure) const
{
	URpgInventoryCarrySlotWidget* CarrySlot = Cast<URpgInventoryCarrySlotWidget>(BoundWidget);
	if (CarrySlot || !bLogFailure || ReportedInvalidPlayerBindings.Contains(BindingName))
	{
		return CarrySlot;
	}

	ReportedInvalidPlayerBindings.Add(BindingName);
	if (BoundWidget)
	{
		UE_LOG(LogRpgPlayerInventoryWidget, Warning,
			TEXT("%s binding %s still uses %s. Reparent that editor widget to RpgInventoryCarrySlotWidget; legacy mini-grid carry widgets are no longer routed."),
			*GetNameSafe(this),
			*BindingName.ToString(),
			*GetNameSafe(BoundWidget->GetClass()));
	}
	else
	{
		UE_LOG(LogRpgPlayerInventoryWidget, Warning,
			TEXT("%s is missing required carry binding %s. Add a widget derived from RpgInventoryCarrySlotWidget."),
			*GetNameSafe(this),
			*BindingName.ToString());
	}
	return nullptr;
}

void URpgPlayerInventoryWidget::ClearInventoryScreenSpecificDragPreviews()
{
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon1, TEXT("Carry_Weapon1"), false))
	{
		CarrySlot->ClearExternalPreviewPayload();
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Weapon2, TEXT("Carry_Weapon2"), false))
	{
		CarrySlot->ClearExternalPreviewPayload();
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot = ResolveCarrySlotWidget(Carry_Offhand, TEXT("Carry_Offhand"), false))
	{
		CarrySlot->ClearExternalPreviewPayload();
	}

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
}
