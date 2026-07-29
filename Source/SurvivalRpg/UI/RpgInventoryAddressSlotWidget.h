#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "SurvivalRpg/UI/RpgInventoryContextActionSource.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"

#include "RpgInventoryAddressSlotWidget.generated.h"

class UDragDropOperation;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryDragVisualWidget;
class URpgInventoryInteractionScreenWidget;
class URpgInventoryItemTooltipWidget;

/**
 * Native CommonUI slot entry for one logical player-inventory address such as Belt[0] or WeaponSlot1[0].
 *
 * Use this as the EntryWidgetClass inside group TileViews. The widget owns no gameplay state; it forwards
 * drag/drop and controller Accept to URpgInventoryDragDropCoordinator.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventoryAddressSlotWidget
	: public UCommonButtonBase
	, public IUserObjectListEntry
	, public IRpgInventoryContextActionSource
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAddressSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact optional Manual MVVM source owned by this native address-slot presenter. */
	static const FName AddressSlotViewModelSourceName;

	/**
	 * Atomically binds one logical address leaf to its exact VM and active screen-owned presentation context.
	 * This native composition seam is idempotent and never changes inventory gameplay state.
	 */
	void BindInventoryPresentation(
		URpgInventoryAddressSlotViewModel* InSlotViewModel,
		const FRpgInventoryScreenPresentationContext& InContext);

	/**
	 * Releases VM, MVVM source, delegates, coordinator, host, selection, and preview state for pooling.
	 * Subclasses may extend this cleanup but must call the base implementation.
	 */
	virtual void ReleaseInventoryPresentation();

	/** Assigns the screen-local drag/drop coordinator shared by player inventory, gear slots, and actionbar. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Assigns the view model represented by this address slot entry or drag visual. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	void SetAddressSlotViewModel(URpgInventoryAddressSlotViewModel* InSlotViewModel);

	/** Marks whether the owning address panel is the active controller panel. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	void SetInventoryPanelActive(bool bInInventoryPanelActive);

	/** Current logical slot VM assigned by the owning list/tile view. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Address Slot")
	URpgInventoryAddressSlotViewModel* GetAddressSlotViewModel() const { return SlotViewModel.Get(); }

	/** Screen-local coordinator used for authoritative previews and commands; UI must not mutate inventory directly. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Address Slot")
	URpgInventoryDragDropCoordinator* GetDragDropCoordinator() const { return DragDropCoordinator.Get(); }

	/**
	 * Assigns the owning inventory screen that centrally creates and owns context menus and split dialogs.
	 * The host is transient presentation state and never owns the represented inventory item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot|Context Menu")
	void SetInventoryPresentationHost(URpgInventoryInteractionScreenWidget* InPresentationHost);

	/** Controller/CommonUI Accept: pick this item up or place the held item onto this address. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	bool HandleSlotAccept();

	/** Recomputes held-item/drop-target visuals and calls the Blueprint visual hook. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot")
	void RefreshDragDropVisualState();

	/** Current drag/drop presentation state for this slot widget. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Address Slot")
	ERpgInventorySlotDragVisualState GetCurrentDragDropVisualState() const { return CurrentDragDropVisualState; }

	/** Updates mouse-drag feedback for an explicit payload while the whole widget remains the target surface. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot|Drag")
	bool PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload);

	/** Commits an explicit payload to this logical address through the server-authoritative coordinator. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot|Drag")
	bool CommitPayloadDrop(const FRpgInventoryDragPayload& Payload);

	/** Clears target-local pointer feedback without changing held payload or gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot|Drag")
	void ClearExternalPreviewPayload();

	/** Context actions available for this legacy address entry, using the same semantics as spatial items. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot|Context Menu")
	TArray<ERpgInventoryContextAction> GetAddressContextActions() const;

	/** Executes one context action after revalidating the persistent item identity captured by the menu. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot|Context Menu")
	bool ExecuteAddressContextAction(
		ERpgInventoryContextAction Action,
		FRpgInventoryItemId ExpectedItemId,
		int32 QuickAccessSlotIndex = -1);

	/** Routes manual drop through the screen-owned confirmation contract, or the legacy coordinator when confirmed. */
	bool RequestAddressItemDrop(
		int32 StackCount = 0,
		bool bConfirmed = false);

	/** Internal zero-based 0..7 Quick Access index matching this semantic Carry role or consumable definition. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Address Slot|Context Menu")
	int32 GetQuickAccessSlotIndex() const;

	/** Opens the configured context menu at an absolute Slate screen position for the represented item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Address Slot|Context Menu")
	bool RequestAddressContextMenu(FVector2D ScreenPosition);

	//~IRpgInventoryContextActionSource interface
	virtual bool QueryInventoryContextActions(
		FRpgInventoryContextActionSnapshot& OutSnapshot) const override;
	virtual bool ExecuteInventoryContextAction(
		const FRpgInventoryContextActionSnapshot& ExpectedSnapshot,
		ERpgInventoryContextAction Action,
		int32 QuickAccessSlotIndex = INDEX_NONE) override;
	//~End of IRpgInventoryContextActionSource interface

	/** Submits an exact split amount from the shared modal after stable-item revalidation. */
	bool ConfirmAddressSplit(FRpgInventoryItemId ExpectedItemId, int32 SplitCount);

protected:
	/**
	 * Refreshes presentation derived from the currently assigned address VM.
	 * The base owns the sole OnSlotChanged observer; specialized address presenters extend this seam instead of
	 * subscribing to the same VM again.
	 */
	virtual void RefreshAddressSlotPresentation();

	/**
	 * Drops this presenter's local preview ownership and clears the shared target only when it still names this
	 * exact address. Specialized address presenters call this before replacing their represented source.
	 */
	void ClearOwnedAddressInteractionPreview();

	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnClicked() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Blueprint presentation hook called when this entry is released for reuse. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot Released"))
	void BP_OnAddressSlotReleased();

	/** Blueprint presentation hook for CommonUI selection/focus. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot Selection Changed"))
	void BP_OnAddressSlotSelectionChanged(bool bIsSelected);

	/** Blueprint presentation hook for held-item/drop-target highlights. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot DragDrop State Changed"))
	void BP_OnAddressSlotDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

	/** Presentation-only fallback for Inspect/Open/Quick-Access actions that require a project detail/slot chooser. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Deferred Address Context Action"))
	void BP_OnDeferredAddressContextAction(ERpgInventoryContextAction Action, URpgInventoryItemInstance* Item);

	/** Exact authored presentation-only drag decorator. Missing configuration fails closed before a drag starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Address Slot|Drag")
	TSubclassOf<URpgInventoryDragVisualWidget> DragVisualClass;

	/**
	 * Tooltip class used for replicated item details. Defaults to the native read-only tooltip and may be replaced by
	 * a Widget Blueprint subclass without changing inventory authority.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Address Slot|Tooltip")
	TSubclassOf<URpgInventoryItemTooltipWidget> ItemTooltipWidgetClass;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgInventoryAddressSlotEntryPoolingTest;
#endif

	UFUNCTION()
	void HandleSlotViewModelChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	FReply HandlePointerButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);
	void ReleaseAddressSlotState();
	bool InjectAddressSlotViewModelIntoMvvm();
	bool RequestAddressSplitDialog();
	FRpgInventoryDragPayload MakeDragPayload(bool bAllowEmptyAddressPayload) const;
	FRpgInventoryDropTarget MakeDropTarget() const;
	void RefreshItemTooltip();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	ERpgInventorySlotDragVisualState CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	/** Screen-owned presentation host, cleared before this pooled entry can represent another address. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionScreenWidget> InventoryPresentationHost = nullptr;

	/** Reused UI-only tooltip, cleared before this pooled slot can represent another address. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryItemTooltipWidget> ItemTooltipWidget = nullptr;

	bool bSlotSelected = false;
	bool bInventoryPanelActive = true;
	bool bPendingLeftClickAccept = false;
	FRpgInventoryDragAnchor PendingPointerDragAnchor;
	bool bHasPendingPointerDragAnchor = false;
	bool bHasExternalPreviewState = false;
	ERpgInventorySlotDragVisualState ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	bool bAddressSlotStateReleased = false;
};
