#pragma once

#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"

#include "RpgInventoryActionWidgets.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class USlider;
class USpinBox;
class UVerticalBox;
class UWidget;
class UTextBlock;
class URpgInventoryAddressSlotWidget;
class URpgEquipmentSlotWidget;
class URpgInventoryContextMenuWidget;
struct FUIInputConfig;

/**
 * Modal exact-stack split dialog with a fully functional native widget-tree fallback.
 *
 * A Blueprint subclass may supply widgets using the optional binding names below. When it does not,
 * the class builds a slider, integer spin box, confirm button, and cancel button at runtime. The
 * selected inventory entry is captured by stable replicated entry id and revalidated before commit.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventorySplitDialogWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySplitDialogWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Initializes one split request already opened by the source grid.
	 *
	 * @param InSourceGrid Grid whose RequestSplitDialogForSelectedCell call established the pending request.
	 * @param InEntryId Stable replicated entry id that must still be selected when the dialog commits.
	 * @param InMinimumCount Smallest legal split amount, normally one.
	 * @param InMaximumCount Largest legal split amount, normally current stack count minus one.
	 * @param InDefaultCount Initially selected amount, normally floor(current stack count / two).
	 * @return True when the request and current grid selection were valid and the controls were initialized.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Split")
	bool InitializeSplitDialog(
		URpgInventorySpatialGridWidget* InSourceGrid,
		FGuid InEntryId,
		int32 InMinimumCount,
		int32 InMaximumCount,
		int32 InDefaultCount);

	/** Initializes the same exact split modal for a legacy logical address entry. */
	bool InitializeAddressSplitDialog(
		URpgInventoryAddressSlotWidget* InSourceAddressSlot,
		FRpgInventoryItemId InItemId,
		int32 InMinimumCount,
		int32 InMaximumCount,
		int32 InDefaultCount);

	/** Revalidates the captured entry and submits the currently selected exact amount through the source grid. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Split")
	bool ConfirmSplitDialog();

	/** Cancels the pending grid split without mutating inventory state, then closes the dialog. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Split")
	void CancelSplitDialog();

	/** Sets the exact split amount after clamping it to the initialized legal range. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Split")
	void SetSelectedSplitCount(int32 InSplitCount);

	/** Returns the exact integer amount currently displayed by the slider and numeric input. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Split")
	int32 GetSelectedSplitCount() const { return SelectedSplitCount; }

	/** Stable replicated entry id captured when this modal was initialized. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Split")
	FGuid GetSplitEntryId() const { return SplitEntryId; }

	//~UCommonActivatableWidget interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Exact-value slider. A native fallback is created when this optional Blueprint binding is absent. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Split|Controls")
	TObjectPtr<USlider> Slider_Amount = nullptr;

	/** Editable numeric value synchronized with Slider_Amount and rounded to whole stack units. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Split|Controls")
	TObjectPtr<USpinBox> SpinBox_Amount = nullptr;

	/** Commits the exact value after stable-entry and range revalidation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Split|Controls")
	TObjectPtr<UButton> Button_Confirm = nullptr;

	/** Cancels the pending split and closes the modal. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Split|Controls")
	TObjectPtr<UButton> Button_Cancel = nullptr;

private:
	void EnsureSplitWidgetTree();
	void BuildNativeSplitWidgetTree();
	void BindSplitControls();
	void CloseSplitDialog();
	void ResetSplitState(bool bCancelGridRequest);

	/** Synchronizes the numeric control when the slider changes. */
	UFUNCTION()
	void HandleSliderValueChanged(float NewValue);

	/** Synchronizes the slider when the editable numeric control changes. */
	UFUNCTION()
	void HandleSpinBoxValueChanged(float NewValue);

	/** Handles the native or Blueprint-bound confirm button. */
	UFUNCTION()
	void HandleConfirmClicked();

	/** Handles the native or Blueprint-bound cancel button. */
	UFUNCTION()
	void HandleCancelClicked();

	/** Source grid remains UI-only; gameplay mutation still routes through its authoritative coordinator path. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySpatialGridWidget> SourceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotWidget> SourceAddressSlot = nullptr;

	/** Stable replicated entry identity used to prevent committing against a different selected stack. */
	UPROPERTY(Transient)
	FGuid SplitEntryId;

	/** Persistent identity used when the split source is a logical address entry instead of a storage entry id. */
	UPROPERTY(Transient)
	FRpgInventoryItemId SplitItemId;

	int32 MinimumSplitCount = 1;
	int32 MaximumSplitCount = 1;
	int32 SelectedSplitCount = 1;
	bool bHasOpenSplitRequest = false;
	bool bSynchronizingAmountControls = false;
};

/**
 * Designer-owned CommonUI row for one semantic inventory context action.
 *
 * Blueprint subclasses may bind Text_ActionLabel and style the CommonButton normally. The native class
 * remains a functional fallback and forwards clicks to the owning context menu without per-action delegates.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryContextActionEntryWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	explicit URpgInventoryContextActionEntryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Configures this recycled row for one action. Called by URpgInventoryContextMenuWidget. */
	void InitializeContextAction(
		URpgInventoryContextMenuWidget* InOwningMenu,
		ERpgInventoryContextAction InAction,
		const FText& InLabel);

	/** Semantic command represented by this row. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	ERpgInventoryContextAction GetContextAction() const { return ContextAction; }

	/** Localized label resolved by the menu, including Bind/Change/Unbind context. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	FText GetActionLabel() const { return ActionLabel; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnClicked() override;

	/** Optional label binding for editor-authored action entry Blueprints. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu")
	TObjectPtr<UTextBlock> Text_ActionLabel = nullptr;

	/** Presentation hook for icons, colors, animations, or custom label widgets. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Context Menu", meta = (DisplayName = "On Context Action Configured"))
	void BP_OnContextActionConfigured(ERpgInventoryContextAction Action, const FText& Label);

private:
	void RefreshActionPresentation();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryContextMenuWidget> OwningMenu = nullptr;

	UPROPERTY(Transient)
	ERpgInventoryContextAction ContextAction = ERpgInventoryContextAction::Inspect;

	UPROPERTY(Transient)
	FText ActionLabel;
};

/**
 * Designer-owned CommonUI row for one of the eight shared Quick Access slots.
 *
 * SlotIndex is always the internal zero-based index (0..7). DisplaySlotNumber is the player-facing
 * number (1..8), preventing keyboard/radial labels from becoming a second binding truth.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgQuickAccessSlotPickerEntryWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	explicit URpgQuickAccessSlotPickerEntryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Configures a selectable 1..8 destination. Occupied slots are intentionally overwriteable. */
	void InitializeQuickAccessSlot(
		URpgInventoryContextMenuWidget* InOwningMenu,
		int32 InSlotIndex,
		const FText& InBindingLabel,
		bool bInOccupied,
		bool bInCurrentBinding);

	/** Internal actionbar array index in the inclusive range 0..7. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	int32 GetSlotIndex() const { return SlotIndex; }

	/** Player-facing slot number in the inclusive range 1..8. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	int32 GetDisplaySlotNumber() const { return SlotIndex + 1; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	bool IsOccupied() const { return bOccupied; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	bool IsCurrentBinding() const { return bCurrentBinding; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnClicked() override;

	/** Optional combined fallback label binding. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Quick Access")
	TObjectPtr<UTextBlock> Text_SlotLabel = nullptr;

	/** Presentation hook for a custom number, occupant icon/name, and current-binding indicator. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Quick Access", meta = (DisplayName = "On Quick Access Slot Configured"))
	void BP_OnQuickAccessSlotConfigured(
		int32 DisplaySlotNumber,
		const FText& InBindingLabel,
		bool bIsOccupied,
		bool bIsCurrentBinding);

private:
	void RefreshSlotPresentation();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryContextMenuWidget> OwningMenu = nullptr;

	UPROPERTY(Transient)
	FText BindingLabel;

	int32 SlotIndex = INDEX_NONE;
	bool bOccupied = false;
	bool bCurrentBinding = false;
};

/**
 * Mouse-first inventory context menu with a fully functional native action-list fallback.
 *
 * The widget captures the selected entry id, builds one button per supplied semantic action, positions
 * the menu at an absolute Slate screen position, and delegates every click back to its grid or gear-slot source.
 * Clicking outside the menu or invoking Back/Escape closes it without a gameplay mutation.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryContextMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryContextMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Initializes and populates the context menu for the grid's current stable selection.
	 *
	 * @param InSourceGrid Grid that owns the selected inventory item and executes semantic actions.
	 * @param InActions Locally meaningful action rows to display; duplicate rows are removed.
	 * @param InScreenPosition Absolute Slate screen position at which the menu should open.
	 * @return True when the grid, selected entry, and action list are valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Context Menu")
	bool InitializeContextMenu(
		URpgInventorySpatialGridWidget* InSourceGrid,
		const TArray<ERpgInventoryContextAction>& InActions,
		FVector2D InScreenPosition);

	/**
	 * Initializes the same modal action list for one dedicated gear slot.
	 *
	 * The represented item's persistent id is captured at open time. Every action revalidates that
	 * the source slot still represents that exact item before a gameplay request or Inspect hook runs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Context Menu")
	bool InitializeEquipmentContextMenu(
		URpgEquipmentSlotWidget* InSourceEquipmentSlot,
		const TArray<ERpgInventoryContextAction>& InActions,
		FVector2D InScreenPosition);

	/** Initializes the shared context menu for a legacy logical address entry. */
	bool InitializeAddressContextMenu(
		URpgInventoryAddressSlotWidget* InSourceAddressSlot,
		const TArray<ERpgInventoryContextAction>& InActions,
		FVector2D InScreenPosition);

	/** Executes one displayed semantic action after stable-entry revalidation, then closes the menu. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Context Menu")
	bool ExecuteContextAction(ERpgInventoryContextAction Action);

	/** Opens the shared eight-slot picker for the captured item without mutating gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access")
	bool ShowQuickAccessSlotPicker();

	/** Returns from the 1..8 picker to the normal context-action page. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access")
	void ShowContextActionPage();

	/** Binds the captured item to a zero-based actionbar slot, then closes the menu on dispatch. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access", meta = (ClampMin = "0", ClampMax = "7"))
	bool SelectQuickAccessSlot(int32 SlotIndex);

	/** Converts an internal 0..7 index to its player-facing 1..8 number, or INDEX_NONE when invalid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	static int32 ToQuickAccessDisplayNumber(int32 SlotIndex);

	/** Converts a player-facing 1..8 number to its internal 0..7 index, or INDEX_NONE when invalid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	static int32 ToQuickAccessSlotIndex(int32 DisplaySlotNumber);

	/** Closes the context menu without executing an inventory action. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Context Menu")
	void CloseContextMenu();

	/** Stable replicated entry id captured when this context menu was initialized. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	FGuid GetContextEntryId() const { return ContextEntryId; }

	/** Persistent gear-item identity captured when an equipment-source menu was initialized. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	FRpgInventoryItemId GetContextItemId() const { return ContextItemId; }

	//~UCommonActivatableWidget interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Full-screen dismiss hit target placed behind the menu; native fallback is created when absent. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UButton> Button_Dismiss = nullptr;

	/** Canvas that positions the menu independently from the inventory screen's designer-authored layout. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UCanvasPanel> ContextMenuCanvas = nullptr;

	/** Visual menu panel whose desired size is used to clamp the menu inside the viewport. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UBorder> ContextMenuBorder = nullptr;

	/** Vertical host rebuilt with one native button per supplied semantic inventory action. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UVerticalBox> ActionsBox = nullptr;

	/** Optional designer host for the eight slot-picker rows. ActionsBox is reused as a functional fallback. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UVerticalBox> QuickAccessSlotsBox = nullptr;

	/** Optional back button shown only on the Quick Access picker page. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UButton> Button_QuickAccessBack = nullptr;

	/** Editor-authored CommonButton row class used for semantic actions. Native row remains the fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Context Menu|Entries")
	TSubclassOf<URpgInventoryContextActionEntryWidget> ActionEntryWidgetClass;

	/** Editor-authored CommonButton row class used for each 1..8 picker destination. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Context Menu|Entries")
	TSubclassOf<URpgQuickAccessSlotPickerEntryWidget> QuickAccessSlotEntryWidgetClass;

private:
	void EnsureContextWidgetTree();
	void BuildNativeContextWidgetTree();
	void BindDismissControl();
	void RebuildActionButtons();
	void RebuildQuickAccessSlotButtons();
	void NormalizeQuickAccessActions();
	FText ResolveContextActionLabel(ERpgInventoryContextAction Action) const;
	int32 ResolveCurrentQuickAccessSlotIndex() const;
	FText ResolveQuickAccessBindingLabel(int32 SlotIndex, bool& bOutOccupied) const;
	void UpdateContextMenuPosition();
	void ResetContextState();
	void HandleContextActionClicked(ERpgInventoryContextAction Action);
	friend class URpgInventoryContextActionEntryWidget;
	friend class URpgQuickAccessSlotPickerEntryWidget;

	/** Handles clicks on the full-screen area outside the action panel. */
	UFUNCTION()
	void HandleDismissClicked();

	/** Individual reflected handlers preserve the semantic action for ordinary UButton delegates. */
	UFUNCTION()
	void HandleOpenContainerClicked();
	UFUNCTION()
	void HandleInspectClicked();
	UFUNCTION()
	void HandleUnequipClicked();
	UFUNCTION()
	void HandleUseClicked();
	UFUNCTION()
	void HandleEquipAndActivateClicked();
	UFUNCTION()
	void HandleMoveToCarryClicked();
	UFUNCTION()
	void HandleSplitClicked();
	UFUNCTION()
	void HandleRotateClicked();
	UFUNCTION()
	void HandleQuickAccessBindClicked();
	UFUNCTION()
	void HandleQuickAccessUnbindClicked();
	UFUNCTION()
	void HandleQuickAccessBackClicked();
	UFUNCTION()
	void HandleTransferClicked();
	UFUNCTION()
	void HandleDropClicked();

	/** Grid source that resolves and dispatches semantic actions; null for equipment-source menus. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySpatialGridWidget> SourceGrid = nullptr;

	/** Gear slot source used instead of SourceGrid for equipment-context actions. */
	UPROPERTY(Transient)
	TObjectPtr<URpgEquipmentSlotWidget> SourceEquipmentSlot = nullptr;

	/** Legacy address source; spatial player UI normally uses SourceGrid instead. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotWidget> SourceAddressSlot = nullptr;

	/** Stable selected entry identity captured before any menu click can change focus. */
	UPROPERTY(Transient)
	FGuid ContextEntryId;

	/** Persistent item identity used to reject stale gear-menu actions after slot replication changes. */
	UPROPERTY(Transient)
	FRpgInventoryItemId ContextItemId;

	/** Deduplicated action order supplied by the presenter and rendered by the native fallback. */
	UPROPERTY(Transient)
	TArray<ERpgInventoryContextAction> ContextActions;

	/** Runtime-created buttons retained for focus selection and safe rebuilds. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonButtonBase>> ActionButtons;

	/** Runtime-created picker entries retained for focus and safe page rebuilds. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgQuickAccessSlotPickerEntryWidget>> QuickAccessSlotButtons;

	FVector2D RequestedScreenPosition = FVector2D::ZeroVector;
	bool bContextPositionPending = false;
	bool bShowingQuickAccessPicker = false;
};
