#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"

#include "RpgInventoryAddressSlotWidget.generated.h"

class UDragDropOperation;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryDragVisualWidget;
class URpgInventoryInteractionScreenWidget;

/**
 * Native CommonUI slot entry for one logical player-inventory address such as Belt[0] or WeaponSlot1[0].
 *
 * Use this as the EntryWidgetClass inside group TileViews. The widget owns no gameplay state; it forwards
 * drag/drop and controller Accept to URpgInventoryDragDropCoordinator.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventoryAddressSlotWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAddressSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact optional Manual MVVM source owned by this native address-slot presenter. */
	static const FName AddressSlotViewModelSourceName;

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

	/** Submits an exact split amount from the shared modal after stable-item revalidation. */
	bool ConfirmAddressSplit(FRpgInventoryItemId ExpectedItemId, int32 SplitCount);

protected:
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

	/** Blueprint presentation hook called when this recycled entry receives a new address slot VM. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Address Slot", meta = (DisplayName = "On Address Slot ViewModel Set"))
	void BP_OnAddressSlotViewModelSet(URpgInventoryAddressSlotViewModel* NewSlotViewModel);

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

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	ERpgInventorySlotDragVisualState CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	/** Screen-owned presentation host, cleared before this pooled entry can represent another address. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionScreenWidget> InventoryPresentationHost = nullptr;

	bool bSlotSelected = false;
	bool bInventoryPanelActive = true;
	bool bPendingLeftClickAccept = false;
	FRpgInventoryDragAnchor PendingPointerDragAnchor;
	bool bHasPendingPointerDragAnchor = false;
	bool bHasExternalPreviewState = false;
	ERpgInventorySlotDragVisualState ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	bool bAddressSlotStateReleased = false;
};
