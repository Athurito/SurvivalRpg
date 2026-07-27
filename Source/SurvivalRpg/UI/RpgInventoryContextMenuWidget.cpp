#include "RpgInventoryContextMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "PrimaryGameLayout.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/UI/RpgInventoryContextActionEntryWidget.h"
#include "SurvivalRpg/UI/RpgInventoryUiGeometry.h"
#include "SurvivalRpg/UI/RpgQuickAccessSlotPickerEntryWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryContextMenuWidget)

#define LOCTEXT_NAMESPACE "RpgInventoryActionWidgets"

namespace
{
FText GetContextActionLabel(ERpgInventoryContextAction Action)
{
	switch (Action)
	{
	case ERpgInventoryContextAction::OpenContainer:
		return LOCTEXT("OpenContainerAction", "Open Container");
	case ERpgInventoryContextAction::Inspect:
		return LOCTEXT("InspectAction", "Inspect");
	case ERpgInventoryContextAction::Unequip:
		return LOCTEXT("UnequipAction", "Unequip");
	case ERpgInventoryContextAction::Use:
		return LOCTEXT("UseAction", "Use");
	case ERpgInventoryContextAction::EquipAndActivate:
		return LOCTEXT("EquipAndActivateAction", "Equip & Activate");
	case ERpgInventoryContextAction::MoveToCarry:
		return LOCTEXT("MoveToCarryAction", "Move to Carry");
	case ERpgInventoryContextAction::Split:
		return LOCTEXT("SplitAction", "Split");
	case ERpgInventoryContextAction::Rotate:
		return LOCTEXT("RotateAction", "Rotate");
	case ERpgInventoryContextAction::QuickAccessBind:
		return LOCTEXT("QuickAccessBindAction", "Bind Quick Access");
	case ERpgInventoryContextAction::QuickAccessUnbind:
		return LOCTEXT("QuickAccessUnbindAction", "Unbind Quick Access");
	case ERpgInventoryContextAction::Transfer:
		return LOCTEXT("TransferAction", "Transfer");
	case ERpgInventoryContextAction::Drop:
		return LOCTEXT("DropAction", "Drop");
	default:
		return LOCTEXT("UnknownContextAction", "Unknown Action");
	}
}

bool RemoveContextMenuFromOwningLayer(
	UCommonActivatableWidget& Modal)
{
	ULocalPlayer* LocalPlayer = Modal.GetOwningLocalPlayer();
	UPrimaryGameLayout* RootLayout = LocalPlayer
		? UPrimaryGameLayout::GetPrimaryGameLayout(LocalPlayer)
		: nullptr;
	UCommonActivatableWidgetContainerBase* ModalLayer = RootLayout
		? RootLayout->GetLayerWidget(RpgGameplayTags::UI_Layer_Modal)
		: nullptr;
	if (!ModalLayer || !ModalLayer->GetWidgetList().Contains(&Modal))
	{
		return false;
	}

	ModalLayer->RemoveWidget(Modal);
	return true;
}
}

URpgInventoryContextMenuWidget::URpgInventoryContextMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true);
}

TOptional<FUIInputConfig> URpgInventoryContextMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void URpgInventoryContextMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BindDismissControl();
	if (QuickAccessSlotsBox)
	{
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URpgInventoryContextMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	bContextPositionPending = true;
	RequestRefreshFocus();
}

void URpgInventoryContextMenuWidget::NativeOnDeactivated()
{
	ResetContextState();
	Super::NativeOnDeactivated();
}

void URpgInventoryContextMenuWidget::NativeDestruct()
{
	ResetContextState();
	Super::NativeDestruct();
}

void URpgInventoryContextMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bContextPositionPending)
	{
		UpdateContextMenuPosition();
	}
}

bool URpgInventoryContextMenuWidget::NativeOnHandleBackAction()
{
	if (bShowingQuickAccessPicker)
	{
		ShowContextActionPage();
		return true;
	}
	CloseContextMenu();
	return true;
}

FReply URpgInventoryContextMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bShowingQuickAccessPicker)
		{
			ShowContextActionPage();
		}
		else
		{
			CloseContextMenu();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UWidget* URpgInventoryContextMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (bShowingQuickAccessPicker)
	{
		return QuickAccessSlotButtons.IsEmpty() ? nullptr : QuickAccessSlotButtons[0].Get();
	}
	return ActionButtons.IsEmpty() ? nullptr : ActionButtons[0].Get();
}

int32 URpgInventoryContextMenuWidget::ToQuickAccessDisplayNumber(int32 SlotIndex)
{
	return FMath::IsWithinInclusive(SlotIndex, 0, 7) ? SlotIndex + 1 : INDEX_NONE;
}

int32 URpgInventoryContextMenuWidget::ToQuickAccessSlotIndex(int32 DisplaySlotNumber)
{
	return FMath::IsWithinInclusive(DisplaySlotNumber, 1, 8) ? DisplaySlotNumber - 1 : INDEX_NONE;
}

bool URpgInventoryContextMenuWidget::InitializeContextMenu(
	UWidget* InContextSource,
	FVector2D InScreenPosition)
{
	BindDismissControl();

	const IRpgInventoryContextActionSource* Source =
		Cast<IRpgInventoryContextActionSource>(
			InContextSource);
	FRpgInventoryContextActionSnapshot Snapshot;
	if (!Source ||
		!Source->QueryInventoryContextActions(Snapshot) ||
		!Snapshot.IsValid() ||
		!Button_Dismiss || !ContextMenuCanvas || !ContextMenuBorder || !ActionsBox ||
		!QuickAccessSlotsBox || !Button_QuickAccessBack || !ActionEntryWidgetClass ||
		!QuickAccessSlotEntryWidgetClass)
	{
		return false;
	}

	ContextSource = InContextSource;
	ContextSnapshot = Snapshot;
	ContextEntryId = Snapshot.EntryId;
	ContextItemId = Snapshot.ItemId;
	RequestedScreenPosition = InScreenPosition;
	ContextActions.Reset(Snapshot.Actions.Num());
	for (ERpgInventoryContextAction Action : Snapshot.Actions)
	{
		ContextActions.AddUnique(Action);
	}
	ContextSnapshot.Actions = ContextActions;

	ContextQuickAccessSlotIndex =
		Snapshot.QuickAccessSlotIndex;
	RebuildActionButtons();
	if (ActionButtons.IsEmpty())
	{
		ResetContextState();
		return false;
	}

	bContextPositionPending = true;
	RequestRefreshFocus();
	return true;
}

bool URpgInventoryContextMenuWidget::ExecuteContextAction(ERpgInventoryContextAction Action)
{
	const bool bActionWasDisplayed = ContextActions.Contains(Action);
	if (!bActionWasDisplayed)
	{
		CloseContextMenu();
		return false;
	}
	if (Action == ERpgInventoryContextAction::QuickAccessBind)
	{
		return ShowQuickAccessSlotPicker();
	}
	if (Action == ERpgInventoryContextAction::QuickAccessUnbind &&
		(ContextQuickAccessSlotIndex == INDEX_NONE ||
			ResolveCurrentQuickAccessSlotIndex() != ContextQuickAccessSlotIndex))
	{
		CloseContextMenu();
		return false;
	}

	TWeakObjectPtr<UWidget> SourceWidget =
		ContextSource;
	const FRpgInventoryContextActionSnapshot ExpectedSnapshot =
		ContextSnapshot;
	// Remove this modal before an action such as Split opens the next modal on the same CommonUI layer.
	CloseContextMenu();
	IRpgInventoryContextActionSource* ResolvedSource =
		Cast<IRpgInventoryContextActionSource>(
			SourceWidget.Get());
	return ResolvedSource &&
		ResolvedSource->ExecuteInventoryContextAction(
			ExpectedSnapshot,
			Action);
}

bool URpgInventoryContextMenuWidget::ShowQuickAccessSlotPicker()
{
	const IRpgInventoryContextActionSource* Source =
		Cast<IRpgInventoryContextActionSource>(
			ContextSource.Get());
	FRpgInventoryContextActionSnapshot CurrentSnapshot;
	if (!ContextActions.Contains(
			ERpgInventoryContextAction::QuickAccessBind) ||
		!Source ||
		!Source->QueryInventoryContextActions(
			CurrentSnapshot) ||
		!ContextSnapshot.MatchesStableSource(
			CurrentSnapshot) ||
		!CurrentSnapshot.Actions.Contains(
			ERpgInventoryContextAction::QuickAccessBind) ||
		CurrentSnapshot.QuickAccessSlotIndex !=
			ContextQuickAccessSlotIndex)
	{
		// A pooled menu must not remain interactive after its exact source changed while it was open.
		CloseContextMenu();
		return false;
	}

	bShowingQuickAccessPicker = true;
	RebuildQuickAccessSlotButtons();
	if (QuickAccessSlotButtons.Num() != 8)
	{
		bShowingQuickAccessPicker = false;
		ShowContextActionPage();
		return false;
	}

	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		ActionsBox->SetVisibility(ESlateVisibility::Collapsed);
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Visible);
	}

	// The picker has a different desired height than the action page. Re-clamp on the
	// following tick, after Slate has incorporated the rebuilt rows into its layout.
	bContextPositionPending = true;
	RequestRefreshFocus();
	return true;
}

void URpgInventoryContextMenuWidget::ShowContextActionPage()
{
	bShowingQuickAccessPicker = false;
	QuickAccessSlotButtons.Reset();
	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		QuickAccessSlotsBox->ClearChildren();
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
		ActionsBox->SetVisibility(ESlateVisibility::Visible);
	}
	RebuildActionButtons();
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Returning to the shorter action page changes the menu's desired size as well.
	// Defer the viewport clamp until the rebuilt page has completed a layout pass.
	bContextPositionPending = true;
	RequestRefreshFocus();
}

bool URpgInventoryContextMenuWidget::SelectQuickAccessSlot(int32 SlotIndex)
{
	if (!bShowingQuickAccessPicker || !FMath::IsWithinInclusive(SlotIndex, 0, 7))
	{
		return false;
	}

	TWeakObjectPtr<UWidget> SourceWidget =
		ContextSource;
	const FRpgInventoryContextActionSnapshot ExpectedSnapshot =
		ContextSnapshot;
	// A picker click consumes this modal even when the source became stale while the page was open.
	CloseContextMenu();
	IRpgInventoryContextActionSource* Source =
		Cast<IRpgInventoryContextActionSource>(
			SourceWidget.Get());
	return Source &&
		Source->ExecuteInventoryContextAction(
			ExpectedSnapshot,
			ERpgInventoryContextAction::QuickAccessBind,
			SlotIndex);
}

void URpgInventoryContextMenuWidget::CloseContextMenu()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
	else
	{
		ResetContextState();
		if (!RemoveContextMenuFromOwningLayer(*this))
		{
			RemoveFromParent();
		}
	}
}

void URpgInventoryContextMenuWidget::BindDismissControl()
{
	if (Button_Dismiss)
	{
		Button_Dismiss->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDismissClicked);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuickAccessBackClicked);
	}
}

void URpgInventoryContextMenuWidget::RebuildActionButtons()
{
	ActionButtons.Reset();
	if (!ActionsBox || !WidgetTree || !ActionEntryWidgetClass)
	{
		return;
	}

	bShowingQuickAccessPicker = false;
	ActionsBox->SetVisibility(ESlateVisibility::Visible);
	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		QuickAccessSlotsBox->ClearChildren();
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActionsBox->ClearChildren();
	for (ERpgInventoryContextAction Action : ContextActions)
	{
		URpgInventoryContextActionEntryWidget* ActionButton =
			WidgetTree->ConstructWidget<URpgInventoryContextActionEntryWidget>(
				ActionEntryWidgetClass);
		if (!ActionButton)
		{
			continue;
		}
		ActionButton->InitializeContextAction(this, Action, ResolveContextActionLabel(Action));
		if (UVerticalBoxSlot* ActionSlot = ActionsBox->AddChildToVerticalBox(ActionButton))
		{
			ActionSlot->SetPadding(FMargin(0.0f, 2.0f));
			ActionSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		ActionButtons.Add(ActionButton);
	}
}

void URpgInventoryContextMenuWidget::RebuildQuickAccessSlotButtons()
{
	QuickAccessSlotButtons.Reset();
	UVerticalBox* PickerHost = QuickAccessSlotsBox.Get();
	if (!PickerHost || !WidgetTree || !QuickAccessSlotEntryWidgetClass)
	{
		return;
	}

	PickerHost->ClearChildren();
	const int32 CurrentSlotIndex = ResolveCurrentQuickAccessSlotIndex();
	for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
	{
		bool bOccupied = false;
		const FText BindingLabel = ResolveQuickAccessBindingLabel(SlotIndex, bOccupied);
		URpgQuickAccessSlotPickerEntryWidget* SlotButton =
			WidgetTree->ConstructWidget<URpgQuickAccessSlotPickerEntryWidget>(
				QuickAccessSlotEntryWidgetClass);
		if (!SlotButton)
		{
			continue;
		}
		SlotButton->InitializeQuickAccessSlot(this, SlotIndex, BindingLabel, bOccupied, SlotIndex == CurrentSlotIndex);
		if (UVerticalBoxSlot* PickerSlot = PickerHost->AddChildToVerticalBox(SlotButton))
		{
			PickerSlot->SetPadding(FMargin(0.0f, 2.0f));
			PickerSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		QuickAccessSlotButtons.Add(SlotButton);
	}

	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuickAccessBackClicked);
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Visible);
	}
}

FText URpgInventoryContextMenuWidget::ResolveContextActionLabel(ERpgInventoryContextAction Action) const
{
	const int32 CurrentSlotIndex = ResolveCurrentQuickAccessSlotIndex();
	if (Action == ERpgInventoryContextAction::QuickAccessBind && CurrentSlotIndex != INDEX_NONE)
	{
		return LOCTEXT("QuickAccessChangeAction", "Change Quick Access Slot");
	}
	if (Action == ERpgInventoryContextAction::QuickAccessUnbind && CurrentSlotIndex != INDEX_NONE)
	{
		return FText::Format(
			LOCTEXT("QuickAccessUnbindSlotAction", "Unbind Quick Access ({0})"),
			FText::AsNumber(ToQuickAccessDisplayNumber(CurrentSlotIndex)));
	}
	return GetContextActionLabel(Action);
}

int32 URpgInventoryContextMenuWidget::ResolveCurrentQuickAccessSlotIndex() const
{
	const IRpgInventoryContextActionSource* Source =
		Cast<IRpgInventoryContextActionSource>(
			ContextSource.Get());
	FRpgInventoryContextActionSnapshot CurrentSnapshot;
	return Source &&
		Source->QueryInventoryContextActions(
			CurrentSnapshot) &&
		ContextSnapshot.MatchesStableSource(
			CurrentSnapshot)
		? CurrentSnapshot.QuickAccessSlotIndex
		: INDEX_NONE;
}

FText URpgInventoryContextMenuWidget::ResolveQuickAccessBindingLabel(int32 SlotIndex, bool& bOutOccupied) const
{
	bOutOccupied = false;
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(GetOwningPlayer());
	const URpgActionBarComponent* ActionBar = RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr;
	if (!ActionBar || !FMath::IsWithinInclusive(SlotIndex, 0, 7))
	{
		return LOCTEXT("EmptyQuickAccessSlot", "Empty");
	}

	const FRpgActionBarSlot ActionBarSlot = ActionBar->GetSlot(SlotIndex);
	bOutOccupied = !ActionBarSlot.IsEmpty();
	switch (ActionBarSlot.SlotType)
	{
	case ERpgActionBarSlotType::Consumable:
	case ERpgActionBarSlotType::InventorySlotBinding:
		if (const URpgInventoryItemDefinition* Definition =
			ActionBarSlot.ConsumableDefinition.GetDefaultObject())
		{
			return Definition->DisplayName;
		}
		return LOCTEXT("MissingConsumableQuickAccessSlot", "Missing Consumable");
	case ERpgActionBarSlotType::CarrySlot:
	case ERpgActionBarSlotType::CarrySlotBinding:
	{
		const URpgPlayerInventoryLayoutComponent* InventoryLayout =
			RpgPlayerController
				? RpgPlayerController->GetPlayerInventoryLayoutComponent()
				: nullptr;
		FRpgInventorySlotGroupView CarryGroup;
		return InventoryLayout &&
				InventoryLayout->TryGetSlotGroupBySemanticRole(
					ActionBarSlot.CarrySemanticRole,
					CarryGroup) &&
			CarryGroup.GroupKind == ERpgInventorySlotGroupKind::Carry
			? CarryGroup.DisplayName
			: LOCTEXT("MissingCarryQuickAccessSlot", "Missing Carry Role");
	}
	case ERpgActionBarSlotType::Ability:
		return ActionBarSlot.AbilityId.IsValid()
			? FText::FromString(ActionBarSlot.AbilityId.ToString())
			: LOCTEXT("MissingAbilityQuickAccessSlot", "Missing Ability");
	case ERpgActionBarSlotType::Empty:
	default:
		bOutOccupied = false;
		return LOCTEXT("EmptyQuickAccessSlot", "Empty");
	}
}

void URpgInventoryContextMenuWidget::UpdateContextMenuPosition()
{
	if (!ContextMenuCanvas || !ContextMenuBorder)
	{
		bContextPositionPending = false;
		return;
	}

	ForceLayoutPrepass();
	const FVector2D MenuSize = ContextMenuBorder->GetDesiredSize();
	FVector2D LocalPosition;
	if (!RpgInventoryUiGeometry::TryResolveClampedMenuCanvasPosition(
		ContextMenuCanvas->GetCachedGeometry(),
		RequestedScreenPosition,
		MenuSize,
		LocalPosition))
	{
		return;
	}

	if (UCanvasPanelSlot* MenuSlot = Cast<UCanvasPanelSlot>(ContextMenuBorder->Slot))
	{
		MenuSlot->SetPosition(LocalPosition);
		bContextPositionPending = false;
	}
}

void URpgInventoryContextMenuWidget::ResetContextState()
{
	if (ActionsBox)
	{
		ActionsBox->SetVisibility(ESlateVisibility::Visible);
		ActionsBox->ClearChildren();
	}
	if (QuickAccessSlotsBox && QuickAccessSlotsBox != ActionsBox)
	{
		QuickAccessSlotsBox->ClearChildren();
		QuickAccessSlotsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_QuickAccessBack)
	{
		Button_QuickAccessBack->SetVisibility(ESlateVisibility::Collapsed);
	}
	ContextSource = nullptr;
	ContextSnapshot =
		FRpgInventoryContextActionSnapshot();
	ContextEntryId.Invalidate();
	ContextItemId = FRpgInventoryItemId();
	ContextQuickAccessSlotIndex = INDEX_NONE;
	ContextActions.Reset();
	ActionButtons.Reset();
	QuickAccessSlotButtons.Reset();
	RequestedScreenPosition = FVector2D::ZeroVector;
	bContextPositionPending = false;
	bShowingQuickAccessPicker = false;
}

void URpgInventoryContextMenuWidget::HandleContextActionClicked(ERpgInventoryContextAction Action)
{
	ExecuteContextAction(Action);
}

void URpgInventoryContextMenuWidget::HandleDismissClicked()
{
	CloseContextMenu();
}

void URpgInventoryContextMenuWidget::HandleQuickAccessBackClicked()
{
	ShowContextActionPage();
}

#undef LOCTEXT_NAMESPACE
