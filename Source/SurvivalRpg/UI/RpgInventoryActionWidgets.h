#pragma once

#include "CommonActivatableWidget.h"
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
class URpgInventoryAddressSlotWidget;
class URpgEquipmentSlotWidget;
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

private:
	void EnsureContextWidgetTree();
	void BuildNativeContextWidgetTree();
	void BindDismissControl();
	void RebuildActionButtons();
	void BindActionButton(UButton* Button, ERpgInventoryContextAction Action);
	void UpdateContextMenuPosition();
	void ResetContextState();
	void HandleContextActionClicked(ERpgInventoryContextAction Action);

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
	TArray<TObjectPtr<UButton>> ActionButtons;

	FVector2D RequestedScreenPosition = FVector2D::ZeroVector;
	bool bContextPositionPending = false;
};
