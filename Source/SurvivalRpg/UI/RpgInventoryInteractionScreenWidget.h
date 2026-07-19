#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/UI/RpgInventoryControllerActionsWidget.h"

#include "RpgInventoryInteractionScreenWidget.generated.h"

class UCanvasPanel;
class UDragDropOperation;
class UWidget;
class URpgEquipmentSlotWidget;
class URpgInventoryAddressSlotWidget;
class URpgInventoryContextMenuWidget;
class URpgInventoryDragVisualWidget;
class URpgInventoryFeedbackToastWidget;
class URpgInventoryInteractionSession;
class URpgInventoryItemInstance;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySplitDialogWidget;
class URpgInventorySpatialGridWidget;
enum class ERpgInventoryContextAction : uint8;

/**
 * Shared CommonUI interaction shell for one inventory screen.
 *
 * The screen owns exactly one transient drag/drop coordinator and one panel navigator. Derived player, storage,
 * crafting, or other inventory presenters provide their read-only view-model binding and authored panel registration
 * through the protected hooks below. Gameplay truth and mutation remain in the replicated inventory/equipment path;
 * successful UI commits continue through URpgInventoryUiActionComponent for server validation.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventoryInteractionScreenWidget : public URpgInventoryControllerActionsWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryInteractionScreenWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Opens the canonical screen-owned context modal for one spatial-grid selection. */
	bool OpenInventoryContextMenu(
		URpgInventorySpatialGridWidget* SourceGrid,
		const TArray<ERpgInventoryContextAction>& Actions,
		FVector2D ScreenPosition);

	/** Opens the canonical screen-owned context modal for one logical address/carry entry. */
	bool OpenInventoryContextMenu(
		URpgInventoryAddressSlotWidget* SourceAddressSlot,
		const TArray<ERpgInventoryContextAction>& Actions,
		FVector2D ScreenPosition);

	/** Opens the canonical screen-owned context modal for one equipped item. */
	bool OpenInventoryContextMenu(
		URpgEquipmentSlotWidget* SourceEquipmentSlot,
		const TArray<ERpgInventoryContextAction>& Actions,
		FVector2D ScreenPosition);

	/** Opens the canonical exact split modal for a stable spatial entry request. */
	bool OpenInventorySplitDialog(
		URpgInventorySpatialGridWidget* SourceGrid,
		FGuid EntryId,
		int32 MinimumCount,
		int32 MaximumCount,
		int32 DefaultCount);

	/** Opens the canonical exact split modal for a stable logical-address item request. */
	bool OpenInventorySplitDialog(
		URpgInventoryAddressSlotWidget* SourceAddressSlot,
		FRpgInventoryItemId ItemId,
		int32 MinimumCount,
		int32 MaximumCount,
		int32 DefaultCount);

	/** Closes only the transient modal opened by a source that is being rebound, released, or pooled. */
	void DismissInventoryPresentationForSource(const UWidget* SourceWidget);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDragOver(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

	/**
	 * Binds this screen's read-only presentation after the screen-owned coordinators exist and before CommonUI action
	 * rows are registered. Derived implementations may rebuild their view-model projections but must not mutate
	 * authoritative inventory state.
	 */
	virtual void BindInventoryScreenPresentation();

	/**
	 * Stops observing gameplay presentation state during deactivation or destruction.
	 * Implementations must be idempotent because CommonUI can deactivate and later destruct the same pooled widget.
	 */
	virtual void UnbindInventoryScreenPresentation();

	/** Forwards the shared coordinator and presentation classes to all currently authored or generated child targets. */
	virtual void ForwardInventoryInteractionContextToChildren();

	/**
	 * Registers all panels owned by the derived screen.
	 * The shared base owns the surrounding BeginPanelRefresh/EndPanelRefresh transaction.
	 */
	virtual void RegisterInventoryScreenNavigationPanels(
		URpgInventoryPanelNavigationCoordinator* Navigator);

	/** Appends every spatial grid that participates in pointer routing and external-preview cleanup. */
	virtual void AppendInventoryScreenSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;

	/**
	 * Routes a payload to non-spatial targets owned by the derived screen, such as gear, carry, or actionbar slots.
	 * Set bOutTargetAddressed whenever the visible ghost addresses a target, even if that target rejects the payload.
	 */
	virtual bool RouteInventoryPayloadToScreenSpecificTarget(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed);

	/** Clears non-spatial external previews owned by the derived screen. */
	virtual void ClearInventoryScreenSpecificDragPreviews();

	/**
	 * Updates a derived screen-owned controller ghost, for example above a carry slot.
	 * Return true only when the derived implementation painted the shared free drag visual.
	 */
	virtual bool UpdateInventoryScreenSpecificControllerDragVisual(
		const FRpgInventoryDragPayload& Payload);

	/** Refreshes semantic presentation on non-spatial targets after the shared interaction session changes. */
	virtual void RefreshInventoryScreenSpecificInteractionPresentation(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest);

	/** Ensures the screen-owned coordinator and navigator exist and are connected to CommonUI action routing. */
	void EnsureInventoryInteractionObjects();

	/** Rebuilds the derived screen's navigation registry inside one focus-preserving refresh transaction. */
	void RefreshInventoryScreenNavigationPanels();

	/** Defers child forwarding, panel registration, and focus until the authored widget tree has completed layout. */
	void QueueDeferredInventoryScreenRefresh();

	/** Shared screen-owned drag/drop coordinator, retained across CommonUI pooling of this widget instance. */
	URpgInventoryDragDropCoordinator* GetScreenDragDropCoordinator() const
	{
		return InventoryDragDropCoordinator.Get();
	}

	/** Shared screen-owned panel navigator, retained across CommonUI pooling of this widget instance. */
	URpgInventoryPanelNavigationCoordinator* GetScreenPanelNavigationCoordinator() const
	{
		return InventoryPanelNavigationCoordinator.Get();
	}

	/** Places or refreshes the free pointer ghost at an exact screen-space position. */
	void UpdateFreePointerDragVisual(
		const FRpgInventoryDragPayload& Payload,
		FVector2D PointerScreenPosition,
		URpgInventoryDragDropOperation* DragOperation,
		bool bCenterVisualOnScreenPosition = false);

	/** Removes the screen-owned free ghost and releases its active UMG drag-operation reference. */
	void ClearFreePointerDragVisual();

	/** Clears derived non-spatial previews, all registered spatial previews, and the shared session preview. */
	void ClearExternalDragPreviews();

	/** Changes the target that owns the current pointer preview, clearing the previous target first. */
	void SwitchActivePointerDropTarget(UWidget* NewTarget);

	/**
	 * Optional free pointer-ghost style. The screen-owned instance avoids UMG drag-start interpolation while the
	 * underlying invisible drag operation continues to carry pointer events.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Interaction")
	TSubclassOf<URpgInventoryDragVisualWidget> FreeDragVisualWidgetClass;

	/** Exact authored context-menu class used by every source on this screen. Missing configuration fails closed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Presentation")
	TSubclassOf<URpgInventoryContextMenuWidget> ContextMenuWidgetClass;

	/** Exact authored split-dialog class used by every stack source on this screen. Missing configuration fails closed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Presentation")
	TSubclassOf<URpgInventorySplitDialogWidget> SplitDialogWidgetClass;

	/**
	 * Authored owner-local result toast retained with the CommonUI screen across pooling.
	 * It is UI-read-only and never owns authoritative inventory state.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventoryFeedbackToastWidget> InventoryFeedbackToast = nullptr;

	/**
	 * Optional fullscreen top-most canvas that owns the free pointer ghost.
	 * Place it last in the screen overlay, set it hit-test-invisible, and do not clip its children.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> DragVisualCanvas = nullptr;

	/**
	 * Presentation-only notification for Move/Merge/Swap/Equip/Blocked/OOB/Pending/Rejected state.
	 * Derived Blueprints may style the state but must not treat it as authoritative gameplay truth.
	 */
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "Inventory|Interaction",
		meta = (DisplayName = "On Inventory Interaction State Changed"))
	void BP_OnInventoryInteractionStateChanged(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest);

private:
	void DismissActiveContextMenuPresentation();
	void DismissActiveSplitDialogPresentation();
	void DismissInventoryModalPresentation();
	void HandleContextMenuDeactivated(URpgInventoryContextMenuWidget* DeactivatedMenu);
	void HandleSplitDialogDeactivated(URpgInventorySplitDialogWidget* DeactivatedDialog);
	void RegisterInventoryFeedbackListener();
	void UnregisterInventoryFeedbackListener();
	void HandleInventoryActionFeedback(
		FGameplayTag Channel,
		const FRpgInventoryActionFeedbackMessage& Message);

	UFUNCTION()
	void HandleInventoryInteractionStateChanged(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest);

	bool RouteInventoryPayloadAtScreenPosition(
		const FRpgInventoryDragPayload& Payload,
		FVector2D ScreenPosition,
		bool bCommit,
		const URpgInventoryDragDropOperation* DragOperation);
	bool RoutePayloadToSpatialGrid(
		const FRpgInventoryDragPayload& Payload,
		FVector2D ScreenPosition,
		bool bCommit);
	void CollectSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;
	void ExecuteDeferredInventoryScreenRefresh();

	/** Interaction coordinator whose UObject Outer is this screen rather than the owning PlayerController. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> InventoryDragDropCoordinator = nullptr;

	/** Focus/navigation coordinator whose UObject Outer is this screen. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> InventoryPanelNavigationCoordinator = nullptr;

	/** Screen-owned free ghost; target-local spatial ghosts remain owned by their grids. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragVisualWidget> FreePointerDragVisual = nullptr;

	/** Active invisible UMG operation retained so rotation can refresh its canonical payload without pointer motion. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropOperation> ActivePointerDragOperation = nullptr;

	FGameplayMessageListenerHandle InventoryActionFeedbackHandle;

	bool bDeferredInventoryScreenRefreshQueued = false;
	bool bHasLastPointerDragScreenPosition = false;
	bool bRoutingPointerPreview = false;
	FVector2D LastPointerDragScreenPosition = FVector2D::ZeroVector;
	FVector2D FreePointerDragVisualSize = FVector2D::ZeroVector;
	float FreePointerDragCellSize = 70.0f;
	float FreePointerDragCellPadding = 2.0f;
	TWeakObjectPtr<URpgInventoryItemInstance> FreePointerDragConfiguredItem;
	FGuid FreePointerDragConfiguredEntryId;
	FRpgInventoryGridSize FreePointerDragConfiguredFootprint;
	int32 FreePointerDragConfiguredStackCount = INDEX_NONE;
	ERpgInventoryDragSourceType FreePointerDragConfiguredSourceType =
		ERpgInventoryDragSourceType::None;
	bool bFreePointerDragVisualConfigured = false;
	TWeakObjectPtr<UWidget> ActivePointerDropTarget;
	TWeakObjectPtr<URpgInventoryContextMenuWidget> ActiveContextMenu;
	TWeakObjectPtr<URpgInventorySplitDialogWidget> ActiveSplitDialog;
	TWeakObjectPtr<UWidget> ActiveContextMenuSource;
	TWeakObjectPtr<UWidget> ActiveSplitDialogSource;
};
