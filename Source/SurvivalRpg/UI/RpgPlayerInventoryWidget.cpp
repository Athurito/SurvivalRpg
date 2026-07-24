#include "RpgPlayerInventoryWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
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
	// Create the screen-owned VM before Super attaches the compiled MVVM view. The source may initialize now or
	// later during construction, but its non-optional PropertyPath can therefore never observe a null owner value.
	EnsurePlayerInventoryViewModel();
	Super::NativeOnInitialized();

	BindViewModelDelegates();
}

void URpgPlayerInventoryWidget::BindInventoryScreenPresentation()
{
	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();
	ValidatePlayerInventoryViewModelMvvmContract();

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
		URpgInventoryCarrySlotWidget* CarrySlot,
		URpgInventorySlotGroupViewModel* GroupViewModel)
	{
		if (!CarrySlot)
		{
			return;
		}

		CarrySlot->SetDragDropCoordinator(GetScreenDragDropCoordinator());
		CarrySlot->SetInventoryPresentationHost(this);
		CarrySlot->SetCarrySlotGroupViewModel(GroupViewModel);
		CarrySlot->SetVisibility(GroupViewModel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	auto ResolveCarryRole = [this](FGameplayTag SemanticRole)
	{
		URpgInventorySlotGroupViewModel* Group =
			PlayerInventoryViewModel->GetSlotGroupBySemanticRole(SemanticRole);
		return Group && Group->IsCarryGroup() ? Group : nullptr;
	};
	auto ResolveContentRole = [this](FGameplayTag SemanticRole)
	{
		URpgInventorySlotGroupViewModel* Group =
			PlayerInventoryViewModel->GetSlotGroupBySemanticRole(SemanticRole);
		return Group && Group->IsContentGroup() ? Group : nullptr;
	};

	BindCarrySlot(Carry_Weapon1, ResolveCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary));
	BindCarrySlot(Carry_Weapon2, ResolveCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Secondary));
	BindCarrySlot(Carry_Offhand, ResolveCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_OffHand));

	BindContentGroup(Content_Pockets, ResolveContentRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Content_Primary), TEXT("Content_Pockets"));
	BindContentGroup(Content_Backpack, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::Backpack), TEXT("Content_Backpack"));
	BindContentGroup(Content_Belt, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::Belt), TEXT("Content_Belt"));
	BindContentGroup(Content_Pouch, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::Pouch), TEXT("Content_Pouch"));
	BindContentGroup(Content_ResourceBag, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::ResourceBag), TEXT("Content_ResourceBag"));
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
	auto RefreshCarryPresentation =
		[](URpgInventoryCarrySlotWidget* CarrySlot)
	{
		if (CarrySlot)
		{
			CarrySlot->RefreshCarrySlotPresentation();
		}
	};
	RefreshCarryPresentation(Carry_Weapon1);
	RefreshCarryPresentation(Carry_Weapon2);
	RefreshCarryPresentation(Carry_Offhand);
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
	auto ResolveTargetedCarrySlot =
		[](URpgInventoryCarrySlotWidget* CarrySlot)
	{
		return CarrySlot &&
			CarrySlot->GetCarryInteractionPreviewState() != ERpgInventoryInteractionPreviewState::None
			? CarrySlot
			: nullptr;
	};

	if (URpgInventoryCarrySlotWidget* CarrySlot =
			ResolveTargetedCarrySlot(Carry_Weapon1))
	{
		return CarrySlot;
	}
	if (URpgInventoryCarrySlotWidget* CarrySlot =
			ResolveTargetedCarrySlot(Carry_Weapon2))
	{
		return CarrySlot;
	}
	return ResolveTargetedCarrySlot(Carry_Offhand);
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryViewModel()
{
	if (!PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel = NewObject<URpgPlayerInventoryViewModel>(this);
	}
}

bool URpgPlayerInventoryWidget::ValidatePlayerInventoryViewModelMvvmContract() const
{
	if (!PlayerInventoryViewModel)
	{
		UE_LOG(
			LogRpgPlayerInventoryWidget,
			Error,
			TEXT("%s has no native-owned player-inventory VM before MVVM validation."),
			*GetNameSafe(this));
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		UE_LOG(
			LogRpgPlayerInventoryWidget,
			Error,
			TEXT("%s has no compiled MVVM view. Author one read-only PropertyPath source named %s."),
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
		CompiledSource->CanBeSet() ||
		CompiledSource->IsOptional() ||
		CompiledSource->GetSourceClass() !=
			URpgPlayerInventoryViewModel::StaticClass())
	{
		UE_LOG(
			LogRpgPlayerInventoryWidget,
			Error,
			TEXT("%s requires one non-optional, non-settable PropertyPath MVVM source named %s with type RpgPlayerInventoryViewModel."),
			*GetNameSafe(this),
			*PlayerInventoryViewModelSourceName.ToString());
		return false;
	}

	// CommonUI permits activation before the first Slate construction. In that valid state the compiled contract
	// already exists, but MVVM has not evaluated any runtime source yet. Pointer identity becomes meaningful only
	// after InitializeSources and is checked on every later activation.
	if (!View->AreSourcesInitialized())
	{
		return true;
	}

	if (View->GetViewModel(PlayerInventoryViewModelSourceName).GetObject() !=
		PlayerInventoryViewModel)
	{
		UE_LOG(
			LogRpgPlayerInventoryWidget,
			Error,
			TEXT("%s MVVM source %s did not resolve through GetPlayerInventoryViewModel to the native-owned VM."),
			*GetNameSafe(this),
			*PlayerInventoryViewModelSourceName.ToString());
		return false;
	}

	return true;
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

	auto ForwardCarrySlot =
		[this](URpgInventoryCarrySlotWidget* CarrySlot)
	{
		if (CarrySlot)
		{
			CarrySlot->SetDragDropCoordinator(GetScreenDragDropCoordinator());
			CarrySlot->SetInventoryPresentationHost(this);
		}
	};
	ForwardCarrySlot(Carry_Weapon1);
	ForwardCarrySlot(Carry_Weapon2);
	ForwardCarrySlot(Carry_Offhand);

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

	if (Carry_Weapon1)
	{
		Navigator->RegisterCarrySlotPanel(TEXT("Carry.Weapon1"), Carry_Weapon1);
	}
	if (Carry_Weapon2)
	{
		Navigator->RegisterCarrySlotPanel(TEXT("Carry.Weapon2"), Carry_Weapon2);
	}
	if (Carry_Offhand)
	{
		Navigator->RegisterCarrySlotPanel(TEXT("Carry.Offhand"), Carry_Offhand);
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
		URpgInventoryCarrySlotWidget* CarrySlot,
		bool& bOutAddressed)
	{
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

	return TryRouteSlot(Carry_Weapon1, bOutTargetAddressed) ||
		TryRouteSlot(Carry_Weapon2, bOutTargetAddressed) ||
		TryRouteSlot(Carry_Offhand, bOutTargetAddressed);
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

URpgInventorySlotGroupViewModel* URpgPlayerInventoryWidget::FindEquipmentProvidedContentGroup(
	ERpgEquipmentSlot SourceEquipmentSlot) const
{
	if (!PlayerInventoryViewModel || SourceEquipmentSlot == ERpgEquipmentSlot::None)
	{
		return nullptr;
	}

	for (URpgInventorySlotGroupViewModel* Group : PlayerInventoryViewModel->GetInventoryGroups())
	{
		if (Group && Group->IsProvidedByEquipment() &&
			Group->GetSourceEquipmentSlot() == SourceEquipmentSlot)
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

void URpgPlayerInventoryWidget::ClearInventoryScreenSpecificDragPreviews()
{
	if (Carry_Weapon1)
	{
		Carry_Weapon1->ClearExternalPreviewPayload();
	}
	if (Carry_Weapon2)
	{
		Carry_Weapon2->ClearExternalPreviewPayload();
	}
	if (Carry_Offhand)
	{
		Carry_Offhand->ClearExternalPreviewPayload();
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
