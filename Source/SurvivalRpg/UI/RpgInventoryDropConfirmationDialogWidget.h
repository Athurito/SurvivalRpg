#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"

#include "RpgInventoryDropConfirmationDialogWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class URpgInventoryInteractionScreenWidget;
struct FUIInputConfig;

/**
 * Authored CommonUI modal for one server-requested manual-drop confirmation.
 *
 * The owning interaction screen retains the pending intent and authoritative request context. This
 * widget only presents the item name/count and returns a request-correlated confirm or cancel choice.
 * Every control is required through BindWidget; incomplete assets fail closed without generating a
 * replacement hierarchy at runtime.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryDropConfirmationDialogWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryDropConfirmationDialogWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Initializes one request-correlated drop confirmation owned by the active interaction screen.
	 *
	 * @param InInteractionScreen Screen that owns and revalidates the pending server-authoritative drop intent.
	 * @param InInitialRequestId Correlation id of the unconfirmed request that received RequiresConfirmation.
	 * @param InItemName Read-only item display name shown in the authored prompt.
	 * @param InStackCount Exact positive amount that the screen will revalidate before its confirmed retry.
	 * @return True only when the host, request identity, amount, and all required authored controls are valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop Confirmation")
	bool InitializeDropConfirmation(
		URpgInventoryInteractionScreenWidget* InInteractionScreen,
		FGuid InInitialRequestId,
		const FText& InItemName,
		int32 InStackCount);

	/**
	 * Consumes this modal exactly once and asks the owning screen to revalidate and dispatch the confirmed retry.
	 *
	 * @return True when the screen accepted the request-correlated confirmation for dispatch.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop Confirmation")
	bool ConfirmDropConfirmation();

	/** Discards the request-correlated pending drop without mutating inventory state, then closes the modal. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop Confirmation")
	void CancelDropConfirmation();

	/** Correlation id of the initial unconfirmed request, or an invalid guid after this modal was consumed. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Drop Confirmation")
	FGuid GetInitialRequestId() const { return InitialRequestId; }

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

	/** Full-screen authored backdrop that cancels the pending drop without changing gameplay state. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Drop Confirmation|Controls")
	TObjectPtr<UButton> Button_Backdrop = nullptr;

	/** Read-only authored prompt populated from the stable pending drop description. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Drop Confirmation|Controls")
	TObjectPtr<UTextBlock> Text_Message = nullptr;

	/** Confirms the exact request after the owning screen revalidates source, item identity, and quantity. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Drop Confirmation|Controls")
	TObjectPtr<UButton> Button_Confirm = nullptr;

	/** Cancels and fully discards the pending drop intent. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Drop Confirmation|Controls")
	TObjectPtr<UButton> Button_Cancel = nullptr;

private:
	void BindDropConfirmationControls();
	void CloseDropConfirmation();
	void ResetDropConfirmationState(bool bNotifyHost);
	FText BuildDropConfirmationMessage(const FText& InItemName, int32 InStackCount) const;

	/** Handles the native or Blueprint-authored confirm button. */
	UFUNCTION()
	void HandleConfirmClicked();

	/** Handles the backdrop and explicit cancel buttons. */
	UFUNCTION()
	void HandleCancelClicked();

	/** Weak presentation host; the screen remains the sole owner of the pending drop intent. */
	UPROPERTY(Transient)
	TWeakObjectPtr<URpgInventoryInteractionScreenWidget> InteractionScreenHost;

	/** Initial unconfirmed request id used to reject callbacks from stale pooled modal instances. */
	UPROPERTY(Transient)
	FGuid InitialRequestId;

	bool bHasOpenDropConfirmation = false;
	bool bResettingDropConfirmationState = false;
};
