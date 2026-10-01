#pragma once

#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageRequest.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include "RpgStorageInventoryWidget.generated.h"

class URpgInventoryManagerComponent;
class URpgInventoryPanelViewModel;
class URpgInventorySpatialGridWidget;
class URpgPlayerInventoryPaneWidget;
class URpgPlayerInventoryViewModel;
class UTextBlock;
class URpgPhysicalStorageViewModel;
class URpgInventoryUiActionComponent;

/**
 * CommonUI storage/loot screen presenter shared by chests, corpses, and dropped-loot actors.
 *
 * The widget keeps gameplay truth in the two inventory manager components from the screen payload. It projects the
 * complete owning-player inventory pane and one exact secondary root grid through one screen-owned drag session and
 * controller panel navigator. Gameplay mutation remains routed through the shared screen coordinator and UI actions.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgStorageInventoryWidget : public URpgInventoryInteractionScreenWidget, public IRpgUIScreenPayloadReceiver
{
	GENERATED_BODY()

public:
	/** Screen-owned projection for designer-authored chest controls and assignment slot entries. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Physical Storage")
	URpgPhysicalStorageViewModel* GetPhysicalStorageViewModel() const { return PhysicalStorageViewModel; }

	/** Replaces one rule with an exact item or category. The server assigns persistent priority. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Physical Storage")
	bool ReplaceAssignmentSlot(int32 SlotIndex, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, FGameplayTag Category);

	/** Appends one selected item/category rule; empty or ambiguous selections are rejected locally. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Physical Storage")
	bool AddAssignmentSlot(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, FGameplayTag Category);

	/** Requests removal of one confirmed rule; no slot state is optimistically removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Physical Storage")
	bool RemoveAssignmentSlot(int32 SlotIndex);

	/** Requests automatic deposit from this player's inventory using the selected chest's domain. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Physical Storage")
	bool DepositMaterials();

	/** Requests the next authored capacity tier; authority validates and pays the complete cost. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Physical Storage")
	bool UpgradeStorage();

	/** Submits the confirmed placement from a Blueprint preview without changing the actor locally. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Physical Storage")
	bool RelocateStorage(FTransform ProposedTransform);

	/** Current validated inventory screen payload, or null after an invalid/cleared payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	URpgInventoryScreenPayload* GetInventoryScreenPayload() const { return InventoryScreenPayload.Get(); }

	/** Exact root handle rendered by SecondaryInventoryGrid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	FRpgInventoryContainerHandle GetSecondaryInventoryRootHandle() const { return SecondaryRootHandle; }

	/**
	 * Local presentation generation incremented once whenever a staged dual-inventory context is actually bound.
	 * Diagnostic-only: it is never authoritative, replicated, or saved.
	 */
	uint32 GetStoragePresentationBindGeneration() const { return StoragePresentationBindGeneration; }

	/** Deprecated compatibility accessor; the passive player pane now owns the stable aggregate projection. */
	UFUNCTION(
		BlueprintPure,
		Category = "Inventory|Storage",
		meta = (
			DeprecatedFunction,
			DeprecationMessage = "Use PlayerInventoryPane.GetPlayerInventoryViewModel instead."))
	URpgPlayerInventoryViewModel* GetStoragePlayerInventoryViewModel() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	//~IRpgUIScreenPayloadReceiver interface
	virtual void ReceiveScreenPayload_Implementation(UObject* Payload) override;
	//~End of IRpgUIScreenPayloadReceiver interface

	virtual void BindInventoryScreenPresentation() override;
	virtual void UnbindInventoryScreenPresentation() override;
	virtual void ForwardInventoryInteractionContextToChildren() override;
	virtual void RegisterInventoryScreenNavigationPanels(
		URpgInventoryPanelNavigationCoordinator* Navigator) override;
	virtual FName GetInitialInventoryNavigationPanelId() const override;
	virtual void AppendInventoryScreenSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const override;
	virtual bool RouteInventoryPayloadToScreenSpecificTarget(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed) override;
	virtual void ClearInventoryScreenSpecificDragPreviews() override;
	virtual bool UpdateInventoryScreenSpecificControllerDragVisual(
		const FRpgInventoryDragPayload& Payload) override;
	virtual void RefreshInventoryScreenSpecificInteractionPresentation(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest) override;
	virtual void NativeOnInventoryActivePanelChanged(
		FName PanelId,
		int32 PanelIndex) override;
	virtual FText ResolveQuickTransferDisplayName() const override;

	/**
	 * Complete passive player-inventory pane authored in CUI_StorageSpatial.
	 * The pane owns only read-only presentation state; this activatable screen retains interaction and input ownership.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgPlayerInventoryPaneWidget> PlayerInventoryPane = nullptr;

	/**
	 * Canonical authored spatial root grid for the payload's secondary inventory (10x6 by default).
	 * The secondary inventory and root handle are runtime payload state, but the presenter itself is required.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySpatialGridWidget> SecondaryInventoryGrid = nullptr;

	/** Required read-only title for the player side; native code only changes its cosmetic color. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerTitle = nullptr;

	/** Required authored storage title used as the destination name in the quick-transfer action. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StorageTitle = nullptr;

	/** UI-only highlight applied to the title of the inventory that currently owns pointer/controller selection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Presentation")
	FLinearColor ActiveInventoryTitleColor = FLinearColor(1.0f, 0.72f, 0.24f, 1.0f);

	/** UI-only neutral color restored on the inactive title and whenever the screen presentation is released. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Presentation")
	FLinearColor InactiveInventoryTitleColor = FLinearColor(0.65f, 0.65f, 0.65f, 1.0f);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgStorageInventoryWidgetContextLifecycleTest;
	friend class FRpgPlayerStorageInventoryLifecycleIntegrationTest;
#endif

	/** Validates and stages Payload; inactive CommonUI screens defer all coordinator and view-model binding. */
	void ApplyInventoryScreenPayload(UObject* Payload);

	/**
	 * Binds the currently staged context exactly once for this activation or active-context transition.
	 * Returns true only when a complete context was bound during this call.
	 */
	bool BindStorageScreenContext();
	void EnsureSecondaryPanelViewModel();
	void BindPhysicalStoragePresentation();
	void UnbindPhysicalStoragePresentation();
	bool SubmitPhysicalStorageCommand(FRpgPhysicalStorageRequest Request);

	UFUNCTION()
	void HandlePhysicalStorageCommandCompleted(FGuid RequestId, bool bSucceeded, FText Message);
	void BindSecondarySpatialGrid();
	void ResetStorageScreenContext();
	void RefreshStorageTransferPresentation();
	void HandlePlayerInventoryPaneNavigationPanelsChanged();

	/** Read-only metadata projection retained by the screen across activation cycles. */
	UPROPERTY(Transient)
	TObjectPtr<URpgPhysicalStorageViewModel> PhysicalStorageViewModel;

	TWeakObjectPtr<URpgInventoryUiActionComponent> PhysicalStorageActions;
	FGuid PendingPhysicalStorageRequestId;

	/** Validated payload staged during async screen initialization and retained only until deactivation or replacement. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryScreenPayload> InventoryScreenPayload = nullptr;

	/** Validated player inventory belonging to the staged or currently bound local presentation context. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PrimaryInventory = nullptr;

	/** View model filtered to SecondaryRootHandle; it owns presentation state only. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelViewModel> SecondaryPanelViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> SecondaryInventory = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryContainerHandle SecondaryRootHandle;

	/** True after the staged context has passed through BindStorageScreenContext for the current activation. */
	bool bStorageContextBound = false;

	/** Monotonic local diagnostic used to verify that lifecycle transitions do not bind the same context twice. */
	uint32 StoragePresentationBindGeneration = 0;
};
