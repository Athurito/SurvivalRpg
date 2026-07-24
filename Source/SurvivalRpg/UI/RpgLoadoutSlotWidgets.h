#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/UI/RpgInventoryContextActionSource.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"

#include "RpgLoadoutSlotWidgets.generated.h"

class URpgEquipmentSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryDragVisualWidget;
class URpgInventoryInteractionScreenWidget;
class URpgInventoryItemInstance;
class URpgInventoryPanelNavigationCoordinator;
class UDragDropOperation;

/**
 * Native button base for one equipment slot such as MainHand, OffHand, Head, Chest, Hands, Legs, or Feet.
 *
 * Use this as the parent for CUI equipment slot widgets in the player inventory screen.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgEquipmentSlotWidget
	: public UCommonButtonBase
	, public IRpgInventoryContextActionSource
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact optional Manual MVVM source owned by this native equipment-slot presenter. */
	static const FName EquipmentSlotViewModelSourceName;

	/**
	 * Atomically binds one Gear leaf to its stable VM and active screen-owned presentation context.
	 * The operation is presentation-only and may be repeated with the same inputs.
	 */
	void BindInventoryPresentation(
		URpgEquipmentSlotViewModel* InSlotViewModel,
		const FRpgInventoryScreenPresentationContext& InContext);

	/** Releases every screen/VM reference and transient interaction state while retaining the authored widget tree. */
	void ReleaseInventoryPresentation();

	/** Assigns the VM for the dedicated equipment slot represented by this widget. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	void SetEquipmentSlotViewModel(URpgEquipmentSlotViewModel* InSlotViewModel);

	/** Assigns the screen-local drag/drop coordinator shared by inventory and equipment widgets. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Connects this focusable gear leaf to the screen-local active-panel navigator. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot|Navigation")
	void SetPanelNavigationCoordinator(
		URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator);

	/**
	 * Assigns the owning inventory screen that centrally creates and owns the equipment context menu.
	 * The host is transient presentation state and never owns equipped gameplay state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot|Context Menu")
	void SetInventoryPresentationHost(URpgInventoryInteractionScreenWidget* InPresentationHost);

	/** Current dedicated equipment slot VM represented by this widget. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	URpgEquipmentSlotViewModel* GetEquipmentSlotViewModel() const { return SlotViewModel.Get(); }

	/** Screen-local coordinator used for authoritative previews and commands; UI must not mutate equipment directly. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	URpgInventoryDragDropCoordinator* GetDragDropCoordinator() const { return DragDropCoordinator.Get(); }

	/** Current held-item/drop-target presentation state. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	ERpgInventorySlotDragVisualState GetCurrentDragDropVisualState() const { return CurrentDragDropVisualState; }

	/** Equipment slot represented by this widget. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	ERpgEquipmentSlot GetResolvedEquipmentSlot() const;

	/** Item assigned to this slot, or null when empty. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Slot")
	URpgInventoryItemInstance* GetRepresentedItem() const;

	/** Controller Accept helper: pick the slot assignment up or place the currently held payload here. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	bool HandleSlotAccept();

	/**
	 * Unequips the represented item into compatible Content space through the server-validated action path.
	 *
	 * The legacy method name is retained for Blueprint compatibility; physical inventory placement remains truth.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	bool HandleClearAssignment();

	/** Recomputes drag/drop presentation state and calls the Blueprint visual hook. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	void RefreshDragDropVisualState();

	/** Updates mouse-drag hover feedback for an explicit payload. Invalid targets are still visibly handled. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	bool PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload);

	/** Commits an explicit mouse-drag payload to this equipment slot through the server-authoritative action path. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	bool CommitPayloadDrop(const FRpgInventoryDragPayload& Payload);

	/** Clears transient mouse-drag hover feedback without changing gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot")
	void ClearExternalPreviewPayload();

	/** Opens the shared modal at an absolute Slate screen position for this slot's current persistent item. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slot|Context Menu")
	bool RequestEquipmentContextMenu(FVector2D ScreenPosition);

	/** Executes a gear context action only when the slot still represents ExpectedItemId. */
	bool ExecuteEquipmentContextAction(
		ERpgInventoryContextAction Action,
		const FRpgInventoryItemId& ExpectedItemId);

	//~IRpgInventoryContextActionSource interface
	virtual bool QueryInventoryContextActions(
		FRpgInventoryContextActionSnapshot& OutSnapshot) const override;
	virtual bool ExecuteInventoryContextAction(
		const FRpgInventoryContextActionSnapshot& ExpectedSnapshot,
		ERpgInventoryContextAction Action,
		int32 QuickAccessSlotIndex = INDEX_NONE) override;
	//~End of IRpgInventoryContextActionSource interface

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnClicked() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Blueprint presentation hook for held-item/drop-target highlights. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment|Slot", meta = (DisplayName = "On Equipment Slot DragDrop State Changed"))
	void BP_OnEquipmentSlotDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

	/** Optional presentation hook for opening an item-inspect view without mutating equipment state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment|Slot|Context Menu", meta = (DisplayName = "On Inspect Equipment Item Requested"))
	void BP_OnInspectEquipmentItemRequested(URpgInventoryItemInstance* ItemInstance);

	/** Dedicated equipment slot represented before or without a VM assignment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Slot")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::Head;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgEquipmentSlotLifecycleTest;
#endif

	UFUNCTION()
	void HandleSlotViewModelChanged(URpgEquipmentSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	FReply HandlePointerButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);
	FRpgInventoryDragPayload MakeDragPayload() const;
	FRpgInventoryDropTarget MakeDropTarget() const;
	bool IsHeldSource() const;
	void ReleaseEquipmentSlotState();
	bool InjectEquipmentSlotViewModelIntoMvvm();

	/** Exact authored presentation-only drag decorator. Missing configuration fails closed before a drag starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Slot|Drag", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URpgInventoryDragVisualWidget> DragVisualClass;

	UPROPERTY(Transient)
	TObjectPtr<URpgEquipmentSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	/** Screen-local focus owner; it never participates in equipment gameplay state. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	ERpgInventorySlotDragVisualState CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	/** Screen-owned presentation host, cleared whenever this pooled equipment leaf releases its binding. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionScreenWidget> InventoryPresentationHost = nullptr;

	bool bPendingLeftClickAccept = false;
	FRpgInventoryDragAnchor PendingPointerDragAnchor;
	bool bHasPendingPointerDragAnchor = false;
	bool bHasExternalPreviewState = false;
	ERpgInventorySlotDragVisualState ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	bool bEquipmentSlotStateReleased = false;
};
