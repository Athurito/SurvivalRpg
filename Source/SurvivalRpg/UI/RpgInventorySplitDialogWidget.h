#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgInventorySplitDialogWidget.generated.h"

class UButton;
class USlider;
class USpinBox;
class UWidget;
class URpgInventoryAddressSlotWidget;
class URpgInventorySpatialGridWidget;
struct FUIInputConfig;

/**
 * Modal exact-stack split dialog backed by one authored, fail-closed Widget Blueprint.
 *
 * Every control below is required through BindWidget. The selected inventory entry is captured by
 * stable replicated identity and revalidated before commit; an incomplete presentation never replaces
 * its authored hierarchy with a runtime-generated root.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
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
	virtual void NativeDestruct() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Full-screen authored backdrop that cancels the modal without changing inventory state. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Split|Controls")
	TObjectPtr<UButton> Button_Backdrop = nullptr;

	/** Exact-value slider authored by the canonical split-dialog Widget Blueprint. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Split|Controls")
	TObjectPtr<USlider> Slider_Amount = nullptr;

	/** Editable numeric value synchronized with Slider_Amount and rounded to whole stack units. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Split|Controls")
	TObjectPtr<USpinBox> SpinBox_Amount = nullptr;

	/** Commits the exact value after stable-entry and range revalidation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Split|Controls")
	TObjectPtr<UButton> Button_Confirm = nullptr;

	/** Cancels the pending split and closes the modal. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Split|Controls")
	TObjectPtr<UButton> Button_Cancel = nullptr;

private:
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

	/** Weak UI-only source; gameplay mutation still routes through its authoritative coordinator path. */
	UPROPERTY(Transient)
	TWeakObjectPtr<URpgInventorySpatialGridWidget> SourceGrid;

	/** Weak logical-address source so a pooled modal cannot retain its previous screen. */
	UPROPERTY(Transient)
	TWeakObjectPtr<URpgInventoryAddressSlotWidget> SourceAddressSlot;

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
