#include "RpgPlayerInventoryPaneWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventorySlotGroupViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"
#include "SurvivalRpg/UI/RpgActionBarTileView.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventoryCarrySlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"
#include "SurvivalRpg/UI/RpgInventorySlotGroupWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryPaneWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgPlayerInventoryPaneWidget, Log, All);

const FName URpgPlayerInventoryPaneWidget::PlayerInventoryViewModelSourceName(
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

URpgPlayerInventoryPaneWidget::URpgPlayerInventoryPaneWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgPlayerInventoryPaneWidget::NativeOnInitialized()
{
	// Create the pane-owned VM before Super attaches the compiled MVVM view. The source may initialize now or
	// later during construction, but its non-optional PropertyPath can therefore never observe a null owner value.
	EnsurePlayerInventoryViewModel();
	Super::NativeOnInitialized();

	BindViewModelDelegates();
}

void URpgPlayerInventoryPaneWidget::NativeDestruct()
{
	ReleaseInventoryPresentation();
	OnNavigationPanelsChanged.Clear();
	Super::NativeDestruct();
}

void URpgPlayerInventoryPaneWidget::BindPlayerInventory(
	APlayerController* PlayerController,
	const FRpgInventoryScreenPresentationContext& InContext,
	FName InNavigationPanelPrefix)
{
	if (!PlayerController || !InContext.IsComplete())
	{
		ReleaseInventoryPresentation();
		return;
	}

	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();
	InteractionContext = InContext;
	NavigationPanelPrefix = InNavigationPanelPrefix.IsNone()
		? FName(TEXT("Player"))
		: InNavigationPanelPrefix;
	bInventoryPresentationBound = true;
	ValidatePlayerInventoryViewModelMvvmContract();

	if (PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel->BindPlayerController(PlayerController);
	}

	RefreshPlayerInventoryViews();
	ForwardInteractionContextToChildren();
}

void URpgPlayerInventoryPaneWidget::ReleaseInventoryPresentation()
{
	bInventoryPresentationBound = false;
	ClearExternalDragPreviews();
	if (PlayerInventoryViewModel)
	{
		// CommonUI may pool the activatable host. Stop observing replicated gameplay state while its pane is closed.
		PlayerInventoryViewModel->UnbindPlayerInventory();
	}

	// Aggregate unbind emits its final empty projections. Release leaves afterwards so no callback can restore a
	// Manual MVVM source or retain a host-owned interaction object on the inactive pooled pane.
	ReleasePlayerInventoryChildPresentations();
	InteractionContext = FRpgInventoryScreenPresentationContext();
	NavigationPanelPrefix = TEXT("Player");
}

void URpgPlayerInventoryPaneWidget::SetInteractionContext(
	const FRpgInventoryScreenPresentationContext& InContext,
	FName InNavigationPanelPrefix)
{
	InteractionContext = InContext;
	NavigationPanelPrefix = InNavigationPanelPrefix.IsNone()
		? FName(TEXT("Player"))
		: InNavigationPanelPrefix;
	if (bInventoryPresentationBound && InteractionContext.IsComplete())
	{
		RefreshPlayerInventoryViews();
		ForwardInteractionContextToChildren();
	}
}

void URpgPlayerInventoryPaneWidget::RefreshPlayerInventoryViews()
{
	RefreshGearSlots();
	RefreshSlotGroups();
	RefreshActionBar();
}

FString URpgPlayerInventoryPaneWidget::GetPlayerInventoryWidgetDebugSummary() const
{
	const int32 CarryCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetCarryGroups().Num() : INDEX_NONE;
	const int32 InventoryGroupCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetInventoryGroups().Num() : INDEX_NONE;
	const int32 ActionBarCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetActionBarSlots().Num() : INDEX_NONE;

	return FString::Printf(
		TEXT("PlayerInventoryPane VM=%s Coordinator=%s Carry1=%s Carry2=%s Offhand=%s Pockets=%s Backpack=%s ActionBar=%s CarryGroups=%d InventoryGroups=%d ActionBarSlots=%d"),
		*GetNameSafe(PlayerInventoryViewModel),
		*GetNameSafe(InteractionContext.DragDropCoordinator),
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

void URpgPlayerInventoryPaneWidget::RefreshSlotGroups()
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
			UE_LOG(LogRpgPlayerInventoryPaneWidget, Warning,
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

		GroupWidget->BindInventoryPresentation(
			GroupViewModel,
			InteractionContext,
			MakePanelId(TEXT("Content")));
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(GroupWidget->Slot))
		{
			// Spatial children must own their complete title + grid geometry; undersized hosts break Slate hit testing.
			CanvasSlot->SetAutoSize(true);
		}
		GroupWidget->SetVisibility(GroupViewModel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	auto BindCarrySlot = [this](
		URpgInventoryCarrySlotWidget* CarrySlot,
		URpgInventorySlotGroupViewModel* GroupViewModel,
		FName PanelId)
	{
		if (!CarrySlot)
		{
			return;
		}

		CarrySlot->BindInventoryPresentation(
			GroupViewModel,
			InteractionContext,
			PanelId);
		CarrySlot->SetVisibility(
			CarrySlot->GetCarrySlotGroupViewModel()
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
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

	BindCarrySlot(
		Carry_Weapon1,
		ResolveCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary),
		MakePanelId(TEXT("Carry.Weapon1")));
	BindCarrySlot(
		Carry_Weapon2,
		ResolveCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Secondary),
		MakePanelId(TEXT("Carry.Weapon2")));
	BindCarrySlot(
		Carry_Offhand,
		ResolveCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_OffHand),
		MakePanelId(TEXT("Carry.Offhand")));

	BindContentGroup(Content_Pockets, ResolveContentRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Content_Primary), TEXT("Content_Pockets"));
	BindContentGroup(Content_Backpack, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::Backpack), TEXT("Content_Backpack"));
	BindContentGroup(Content_Belt, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::Belt), TEXT("Content_Belt"));
	BindContentGroup(Content_Pouch, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::Pouch), TEXT("Content_Pouch"));
	BindContentGroup(Content_ResourceBag, FindEquipmentProvidedContentGroup(ERpgEquipmentSlot::ResourceBag), TEXT("Content_ResourceBag"));
}

void URpgPlayerInventoryPaneWidget::RefreshActionBar()
{
	if (!PlayerInventoryViewModel || !ActionBarTileView)
	{
		return;
	}

	ActionBarTileView->SetActionBarSlotItems(PlayerInventoryViewModel->GetActionBarSlots());
	ActionBarTileView->SetDragDropCoordinator(
		InteractionContext.DragDropCoordinator);
}

void URpgPlayerInventoryPaneWidget::RefreshGearSlots()
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

void URpgPlayerInventoryPaneWidget::HandleGearSlotsChanged()
{
	RefreshGearSlots();
}

void URpgPlayerInventoryPaneWidget::HandleSlotGroupsChanged()
{
	RefreshSlotGroups();
	OnNavigationPanelsChanged.Broadcast();
}

void URpgPlayerInventoryPaneWidget::HandleActionBarSlotsChanged()
{
	RefreshActionBar();
}

bool URpgPlayerInventoryPaneWidget::ResolveControllerDragVisualAnchor(
	FVector2D& OutAnchorScreenPosition) const
{
	URpgInventoryCarrySlotWidget* CarrySlot = FindControllerPreviewCarrySlot();
	if (!CarrySlot)
	{
		return false;
	}

	const FGeometry CarryGeometry = CarrySlot->GetCachedGeometry();
	const FVector2D CarryLocalSize = CarryGeometry.GetLocalSize();
	if (CarryLocalSize.X <= KINDA_SMALL_NUMBER || CarryLocalSize.Y <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutAnchorScreenPosition = CarryGeometry.LocalToAbsolute(CarryLocalSize * 0.5f);
	return true;
}

void URpgPlayerInventoryPaneWidget::RefreshInteractionPresentation(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	(void)PreviewState;
	(void)bHasPayload;
	(void)bPendingRequest;
	RefreshCarrySlotPresentations();
}

void URpgPlayerInventoryPaneWidget::RefreshCarrySlotPresentations()
{
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

URpgInventoryCarrySlotWidget* URpgPlayerInventoryPaneWidget::FindControllerPreviewCarrySlot() const
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

void URpgPlayerInventoryPaneWidget::EnsurePlayerInventoryViewModel()
{
	if (!PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel = NewObject<URpgPlayerInventoryViewModel>(this);
	}
}

bool URpgPlayerInventoryPaneWidget::ValidatePlayerInventoryViewModelMvvmContract() const
{
	if (!PlayerInventoryViewModel)
	{
		UE_LOG(
			LogRpgPlayerInventoryPaneWidget,
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
			LogRpgPlayerInventoryPaneWidget,
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
			LogRpgPlayerInventoryPaneWidget,
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
			LogRpgPlayerInventoryPaneWidget,
			Error,
			TEXT("%s MVVM source %s did not resolve through GetPlayerInventoryViewModel to the native-owned VM."),
			*GetNameSafe(this),
			*PlayerInventoryViewModelSourceName.ToString());
		return false;
	}

	return true;
}

void URpgPlayerInventoryPaneWidget::BindViewModelDelegates()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	PlayerInventoryViewModel->OnGearSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleGearSlotsChanged);
	PlayerInventoryViewModel->OnSlotGroupsChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotGroupsChanged);
	PlayerInventoryViewModel->OnActionBarSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleActionBarSlotsChanged);
}

void URpgPlayerInventoryPaneWidget::SetGearSlotViewModel(
	URpgEquipmentSlotWidget* GearSlotWidget,
	ERpgEquipmentSlot EquipmentSlot,
	bool bBagSlot)
{
	if (!GearSlotWidget || !PlayerInventoryViewModel)
	{
		return;
	}

	GearSlotWidget->BindInventoryPresentation(
		bBagSlot
			? PlayerInventoryViewModel->GetBagSlot(EquipmentSlot)
			: PlayerInventoryViewModel->GetArmorSlot(EquipmentSlot),
		InteractionContext);
}

void URpgPlayerInventoryPaneWidget::ReleasePlayerInventoryChildPresentations()
{
	TArray<URpgInventorySlotGroupWidget*> ContentGroups;
	CollectStandaloneContentGroupWidgets(ContentGroups);
	for (URpgInventorySlotGroupWidget* ContentGroup :
		ContentGroups)
	{
		if (ContentGroup)
		{
			ContentGroup->ReleaseInventoryPresentation();
		}
	}

	TArray<URpgInventoryCarrySlotWidget*> CarrySlots;
	CollectCarrySlotWidgets(CarrySlots);
	for (URpgInventoryCarrySlotWidget* CarrySlot :
		CarrySlots)
	{
		if (CarrySlot)
		{
			CarrySlot->ReleaseInventoryPresentation();
		}
	}

	TArray<URpgEquipmentSlotWidget*> GearSlots;
	CollectGearSlotWidgets(GearSlots);
	for (URpgEquipmentSlotWidget* GearSlot : GearSlots)
	{
		if (GearSlot)
		{
			GearSlot->ReleaseInventoryPresentation();
		}
	}

	if (ActionBarTileView)
	{
		ActionBarTileView->SetDragDropCoordinator(nullptr);
	}
}

void URpgPlayerInventoryPaneWidget::ForwardInteractionContextToChildren()
{
	if (ActionBarTileView)
	{
		ActionBarTileView->SetDragDropCoordinator(
			InteractionContext.DragDropCoordinator);
	}
}

void URpgPlayerInventoryPaneWidget::RegisterNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (!Navigator)
	{
		return;
	}

	if (Gear_Head)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Head")), Gear_Head);
	}
	if (Gear_Chest)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Chest")), Gear_Chest);
	}
	if (Gear_Hands)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Hands")), Gear_Hands);
	}
	if (Gear_Legs)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Legs")), Gear_Legs);
	}
	if (Gear_Feet)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Feet")), Gear_Feet);
	}
	if (Gear_Backpack)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Backpack")), Gear_Backpack);
	}
	if (Gear_Belt)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Belt")), Gear_Belt);
	}
	if (Gear_Pouch)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.Pouch")), Gear_Pouch);
	}
	if (Gear_ResourceBag)
	{
		Navigator->RegisterEquipmentPanel(MakePanelId(TEXT("Gear.ResourceBag")), Gear_ResourceBag);
	}

	if (Carry_Weapon1 &&
		Carry_Weapon1->GetCarrySlotGroupViewModel())
	{
		Navigator->RegisterCarrySlotPanel(MakePanelId(TEXT("Carry.Weapon1")), Carry_Weapon1);
	}
	if (Carry_Weapon2 &&
		Carry_Weapon2->GetCarrySlotGroupViewModel())
	{
		Navigator->RegisterCarrySlotPanel(MakePanelId(TEXT("Carry.Weapon2")), Carry_Weapon2);
	}
	if (Carry_Offhand &&
		Carry_Offhand->GetCarrySlotGroupViewModel())
	{
		Navigator->RegisterCarrySlotPanel(MakePanelId(TEXT("Carry.Offhand")), Carry_Offhand);
	}

	TArray<URpgInventorySlotGroupWidget*> StandaloneContentGroups;
	CollectStandaloneContentGroupWidgets(StandaloneContentGroups);
	for (URpgInventorySlotGroupWidget* GroupWidget : StandaloneContentGroups)
	{
		if (GroupWidget)
		{
			GroupWidget->SetPanelNavigationCoordinator(
				Navigator,
				MakePanelId(TEXT("Content")));
		}
	}

	if (ActionBarTileView)
	{
		Navigator->RegisterActionBarPanel(
			MakePanelId(TEXT("Actionbar")),
			ActionBarTileView);
	}
}

void URpgPlayerInventoryPaneWidget::AppendSpatialGrids(
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

bool URpgPlayerInventoryPaneWidget::ResolveNonSpatialDropTarget(
	FVector2D GhostCenterScreenPosition,
	UWidget*& OutTarget) const
{
	OutTarget = nullptr;

	TArray<URpgEquipmentSlotWidget*> GearSlots;
	CollectGearSlotWidgets(GearSlots);
	for (URpgEquipmentSlotWidget* GearSlot : GearSlots)
	{
		if (IsWidgetUnderScreenPosition(GearSlot, GhostCenterScreenPosition))
		{
			OutTarget = GearSlot;
			return true;
		}
	}

	TArray<URpgInventoryCarrySlotWidget*> CarrySlots;
	CollectCarrySlotWidgets(CarrySlots);
	for (URpgInventoryCarrySlotWidget* CarrySlot : CarrySlots)
	{
		if (IsWidgetUnderScreenPosition(CarrySlot, GhostCenterScreenPosition))
		{
			OutTarget = CarrySlot;
			return true;
		}
	}

	if (ActionBarTileView &&
		IsWidgetUnderScreenPosition(ActionBarTileView, GhostCenterScreenPosition) &&
		ActionBarTileView->HasActionBarSlotAtScreenPosition(
			GhostCenterScreenPosition))
	{
		OutTarget = ActionBarTileView;
		return true;
	}
	return false;
}

bool URpgPlayerInventoryPaneWidget::ApplyPayloadToNonSpatialDropTarget(
	UWidget* Target,
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit)
{
	if (URpgEquipmentSlotWidget* GearSlot =
			Cast<URpgEquipmentSlotWidget>(Target))
	{
		return bCommit
			? GearSlot->CommitPayloadDrop(Payload)
			: (GearSlot->PreviewPayloadDrop(Payload), true);
	}

	if (URpgInventoryCarrySlotWidget* CarrySlot =
			Cast<URpgInventoryCarrySlotWidget>(Target))
	{
		return bCommit
			? CarrySlot->CommitPayloadDrop(Payload)
			: (CarrySlot->PreviewPayloadDrop(Payload), true);
	}

	if (Target == ActionBarTileView && ActionBarTileView)
	{
		return bCommit
			? ActionBarTileView->CommitPayloadAtScreenPosition(
				Payload,
				GhostCenterScreenPosition)
			: ActionBarTileView->PreviewPayloadAtScreenPosition(
				Payload,
				GhostCenterScreenPosition);
	}
	return false;
}

URpgInventorySlotGroupViewModel* URpgPlayerInventoryPaneWidget::FindEquipmentProvidedContentGroup(
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

void URpgPlayerInventoryPaneWidget::CollectStandaloneContentGroupWidgets(TArray<URpgInventorySlotGroupWidget*>& OutWidgets) const
{
	OutWidgets.Add(Content_Pockets);
	OutWidgets.Add(Content_Backpack);
	OutWidgets.Add(Content_Belt);
	OutWidgets.Add(Content_Pouch);
	OutWidgets.Add(Content_ResourceBag);
	OutWidgets.Remove(nullptr);
}

void URpgPlayerInventoryPaneWidget::CollectCarrySlotWidgets(
	TArray<URpgInventoryCarrySlotWidget*>& OutWidgets) const
{
	OutWidgets.Add(Carry_Weapon1);
	OutWidgets.Add(Carry_Weapon2);
	OutWidgets.Add(Carry_Offhand);
	OutWidgets.Remove(nullptr);
}

void URpgPlayerInventoryPaneWidget::CollectGearSlotWidgets(
	TArray<URpgEquipmentSlotWidget*>& OutWidgets) const
{
	OutWidgets.Add(Gear_Head);
	OutWidgets.Add(Gear_Chest);
	OutWidgets.Add(Gear_Hands);
	OutWidgets.Add(Gear_Legs);
	OutWidgets.Add(Gear_Feet);
	OutWidgets.Add(Gear_Backpack);
	OutWidgets.Add(Gear_Belt);
	OutWidgets.Add(Gear_Pouch);
	OutWidgets.Add(Gear_ResourceBag);
	OutWidgets.Remove(nullptr);
}

void URpgPlayerInventoryPaneWidget::ClearExternalDragPreviews()
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


UWidget* URpgPlayerInventoryPaneWidget::GetPreferredFocusTarget() const
{
	return Content_Pockets && Content_Pockets->GetSpatialGridWidget()
		? static_cast<UWidget*>(Content_Pockets->GetSpatialGridWidget())
		: static_cast<UWidget*>(Gear_Head.Get());
}

FName URpgPlayerInventoryPaneWidget::GetPreferredNavigationPanelId() const
{
	if (!Content_Pockets)
	{
		return NAME_None;
	}

	const FRpgInventoryContainerHandle Handle =
		Content_Pockets->GetSlotGroupHandle();
	if (!Handle.IsValid())
	{
		return NAME_None;
	}

	return FName(*FString::Printf(
		TEXT("%s.%s"),
		*MakePanelId(TEXT("Content")).ToString(),
		*Handle.ToString()));
}

FName URpgPlayerInventoryPaneWidget::MakePanelId(const TCHAR* Suffix) const
{
	const FName SuffixName(Suffix);
	if (NavigationPanelPrefix.IsNone())
	{
		return SuffixName;
	}
	return FName(*FString::Printf(
		TEXT("%s.%s"),
		*NavigationPanelPrefix.ToString(),
		Suffix));
}
