#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryContextActionTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "UObject/Object.h"

#include "RpgInventoryDragDropCoordinator.generated.h"

class APlayerController;
class URpgActionBarSlotViewModel;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryEntryViewModel;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgInventoryInteractionSession;
class URpgPlayerInventoryLayoutComponent;
class URpgInventoryUiActionComponent;

/**
 * UI-local coordinator shared by mouse drag/drop and controller pick/place flows.
 *
 * The coordinator owns only transient presentation state. Successful commits are routed to the
 * owning player's URpgInventoryUiActionComponent, where the server performs final validation.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryDragDropCoordinator : public UObject
{
	GENERATED_BODY()

public:
	/** Creates and initializes a UI-local coordinator for one inventory screen. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop", meta = (WorldContext = "WorldContextObject"))
	static URpgInventoryDragDropCoordinator* CreateInventoryDragDropCoordinator(UObject* WorldContextObject, APlayerController* InPlayerController);

	/** Initializes routing for a screen owned by the supplied local player controller. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void Initialize(APlayerController* InPlayerController);

	/** Player inventory resolved from the owning controller, used by navigation helpers and fallback transfers. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	URpgInventoryManagerComponent* GetPlayerInventory() const;

	/** Overrides the UI action component used for server requests, useful for testing or custom controllers. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetUiActionComponent(URpgInventoryUiActionComponent* InUiActionComponent);

	/** Shared screen-local interaction state used by pointer drag and controller pick/place. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	URpgInventoryInteractionSession* GetInteractionSession() const { return InteractionSession.Get(); }

	/** Builds an inventory-entry payload from a TileView entry model. Empty slots return an invalid payload. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDragPayload MakeInventoryPayloadFromEntry(URpgInventoryEntryViewModel* EntryViewModel);

	/** Builds an inventory-entry payload from a logical player-inventory address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDragPayload MakeInventoryPayloadFromAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel);

	/** Builds an inventory-slot target from a TileView entry model, including UI-only empty capacity slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeInventoryTargetFromEntry(URpgInventoryEntryViewModel* EntryViewModel);

	/** Builds a target for a logical player-inventory address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakePlayerInventorySlotAddressTarget(URpgInventoryAddressSlotViewModel* SlotViewModel);

	/** Builds a panel-level target for transferring into an inventory without a specific slot index. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeInventoryPanelTarget(URpgInventoryManagerComponent* TargetInventory);

	/**
	 * Builds an equipment payload with the exact owning-inventory entry snapshot captured at drag start.
	 * Returns a payload without a valid source snapshot when the item is no longer managed by an inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDragPayload MakeEquipmentPayload(URpgInventoryItemInstance* ItemInstance, ERpgEquipmentSlot EquipmentSlot);

	/** Builds a target for an equipment slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeEquipmentTarget(ERpgEquipmentSlot EquipmentSlot);

	/** Builds a target for an internal zero-based actionbar index (0..7, displayed to players as 1..8). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeActionBarSlotTarget(int32 ActionBarSlotIndex);

	/** Builds a target from an actionbar slot view model. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeActionBarSlotTargetFromViewModel(URpgActionBarSlotViewModel* SlotViewModel);

	/** Builds a target that holsters active hands or moves physical Gear back into compatible Content. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeClearTarget();

	/** Returns true when the payload has enough data to start a drag or controller hold. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	static bool IsPayloadValid(const FRpgInventoryDragPayload& Payload);

	/** Returns true when the target has enough data to receive a payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	static bool IsTargetValid(const FRpgInventoryDropTarget& Target);

	/** Captures a DPI-independent cell anchor from the original source-widget mouse-down geometry. */
	static void CapturePointerDragAnchor(
		FRpgInventoryDragPayload& InOutPayload,
		FVector2D LocalPointerPosition,
		FVector2D SourceVisualSize);

	/** Adds presentation-only absolute geometry without changing the canonical cell anchor used for placement. */
	static void CapturePointerDragAnchorScreenGeometry(
		FRpgInventoryDragPayload& InOutPayload,
		FVector2D SourceScreenTopLeft,
		FVector2D PointerScreenPosition,
		FVector2D SourceScreenVisualSize);

	/** Rebuilds the grabbed point in target-local pixels using the target grid's own cell metrics. */
	static FVector2D ResolveTargetGrabPixels(
		const FRpgInventoryDragPayload& Payload,
		bool bTargetRotated,
		float CellSize,
		float CellPadding);

	/** Returns the center of the free source-sized decorator for non-spatial target routing. */
	static FVector2D ResolveFreeGhostCenterScreen(const FRpgInventoryDragPayload& Payload, FVector2D PointerScreenPosition);

	/** Starts the controller pick/place flow with a payload. No gameplay RPC is sent. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool BeginHold(const FRpgInventoryDragPayload& Payload);

	/** Starts the pointer drag flow with the same transient payload state used by controller pick/place. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool BeginPointerDrag(const FRpgInventoryDragPayload& Payload);

	/** Starts holding the focused inventory entry for controller pick/place. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool BeginHoldFromEntry(URpgInventoryEntryViewModel* EntryViewModel);

	/** Clears the controller-held payload without changing gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void CancelHold();

	/** Clears the interaction even while a request is pending; intended for screen teardown only. */
	void ForceCancelInteraction();

	/** Returns true while controller pick/place is holding a payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	bool HasHeldPayload() const;

	/** Returns the current controller-held payload for UI previews. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	FRpgInventoryDragPayload GetHeldPayload() const;

	/** Returns the held item instance, or null when controller pick/place is empty-handed. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	URpgInventoryItemInstance* GetHeldItemInstance() const;

	/** Returns true while the current server request is awaiting authoritative feedback or replicated state. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	bool IsInteractionRequestPending() const;

	/** Returns true when the current held payload supports a local rotation toggle. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	bool CanToggleInteractionRotation() const;

	/** Rotates the shared payload and grab offsets in place for mouse and controller placement. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
	bool ToggleInteractionRotation();

	/** Target rotation owned by the shared session when Payload is the active payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	bool GetTargetRotationForPayload(const FRpgInventoryDragPayload& Payload) const;

	/** Returns the current shared payload for matching pointer/controller payload identity, otherwise Payload unchanged. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	FRpgInventoryDragPayload ResolveInteractionPayload(const FRpgInventoryDragPayload& Payload) const;

	/** Semantic preview currently exposed to Blueprint indicators and contextual action text. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	ERpgInventoryInteractionPreviewState GetInteractionPreviewState() const;

	/** Sets the inventory panel that currently owns CommonUI focus. Used for shortcut routing and UI hints. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void SetFocusedInventory(URpgInventoryManagerComponent* InFocusedInventory);

	/** Currently focused inventory panel, if the screen registered one. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	URpgInventoryManagerComponent* GetFocusedInventory() const { return FocusedInventory.Get(); }

	/** Registers or replaces the shortcut transfer target for one source inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void SetQuickTransferTarget(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory);

	/** Removes every shortcut transfer route on this UI-local coordinator. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void ClearQuickTransferTargets();

	/**
	 * Returns the ordered context actions that are locally meaningful for one current inventory entry.
	 *
	 * This is a presentation query only. bSupportsSpatialRotation describes the calling presenter; the server still
	 * owns final access, placement, equipment, and mutation validation.
	 */
	TArray<ERpgInventoryContextAction> GetAvailableContextActions(
		URpgInventoryEntryViewModel* EntryViewModel,
		bool bSupportsSpatialRotation = false) const;

	/** Revalidates one entry action immediately before UI dispatch. Never mutates gameplay state. */
	bool CanExecuteContextAction(
		URpgInventoryEntryViewModel* EntryViewModel,
		ERpgInventoryContextAction Action,
		bool bSupportsSpatialRotation = false) const;

	/**
	 * Returns the ordered context actions for one current player-layout address such as Content, Carry, or Gear.
	 *
	 * bSupportsSpatialRotation is true only when a spatial grid owns the rotation target and commit behavior.
	 */
	TArray<ERpgInventoryContextAction> GetAvailableContextActions(
		URpgInventoryAddressSlotViewModel* SlotViewModel,
		bool bSupportsSpatialRotation = false) const;

	/** Revalidates one player-layout address action immediately before UI dispatch. */
	bool CanExecuteContextAction(
		URpgInventoryAddressSlotViewModel* SlotViewModel,
		ERpgInventoryContextAction Action,
		bool bSupportsSpatialRotation = false) const;

	/** Returns the ordered context actions for the exact item currently represented by an equipment slot. */
	TArray<ERpgInventoryContextAction> GetAvailableContextActions(
		ERpgEquipmentSlot EquipmentSlot,
		const FRpgInventoryItemId& ExpectedItemId) const;

	/** Revalidates one equipment action against current slot ownership and item identity before UI dispatch. */
	bool CanExecuteContextAction(
		ERpgEquipmentSlot EquipmentSlot,
		const FRpgInventoryItemId& ExpectedItemId,
		ERpgInventoryContextAction Action) const;

	/**
	 * Returns the zero-based 0..7 slot currently bound to this payload's semantic Carry role or consumable
	 * definition. Consumables intentionally match by definition so moving a preferred stack does not duplicate it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access")
	int32 FindQuickAccessSlotForPayload(const FRpgInventoryDragPayload& Payload) const;

	/**
	 * Binds an item to one internal zero-based Quick Access index. Carry payloads bind their semantic role;
	 * consumables bind definition plus preferred persistent item id. The server re-resolves the source address.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access", meta = (ClampMin = "0", ClampMax = "7"))
	bool BindPayloadToQuickAccessSlot(const FRpgInventoryDragPayload& Payload, int32 SlotIndex);

	/** Clears the matching semantic binding without allowing stale UI state to clear an unrelated occupied slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access")
	bool ClearQuickAccessBindingForPayload(const FRpgInventoryDragPayload& Payload);

	/** Resolves the shortcut transfer target for a source inventory, including player-inventory fallback. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	URpgInventoryManagerComponent* ResolveQuickTransferTarget(URpgInventoryManagerComponent* SourceInventory) const;

	/** Returns true when the focused entry can send a full-stack shortcut transfer command. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool CanQuickTransferEntry(URpgInventoryEntryViewModel* EntryViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr) const;

	/** Sends a full-stack shortcut transfer command for the focused entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickTransferEntry(URpgInventoryEntryViewModel* EntryViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr);

	/** Returns true when one logical player-inventory address can quick-transfer to the configured target inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool CanQuickTransferAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr) const;

	/** Sends a full-stack shortcut transfer command for one logical player-inventory address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickTransferAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr);

	/** Checks a concrete player-owned Gear/Carry item against the same deterministic content routing policy. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool CanQuickTransferPlayerItem(URpgInventoryItemInstance* ItemInstance) const;

	/** Moves a concrete Gear/Carry item wholly into the first fitting player content container, normally Backpack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickTransferPlayerItem(URpgInventoryItemInstance* ItemInstance);

	/** Returns true when the focused entry can be split into a separate stack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool CanQuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount = 0) const;

	/** Sends a split-stack command. SplitCount <= 0 uses the 50% quick split rule. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount = 0);

	/** Returns true when one logical player-inventory address can be split into separate content space. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool CanQuickSplitAddressSlot(
		URpgInventoryAddressSlotViewModel* SlotViewModel,
		FRpgInventoryGridPlacement TargetPlacement,
		int32 SplitCount = 0) const;

	/** Uses a usable item, otherwise tries to equip it through equipment slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool UseOrEquipEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount = 1);

	/** Executes one explicit item action without falling back to a different intent for hybrid items. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool ExecuteEntryItemAction(
		URpgInventoryEntryViewModel* EntryViewModel,
		ERpgInventoryContextAction Action,
		int32 StackCount = 1);

	/** Uses/equips or unequips one logical player-inventory address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool UseOrEquipAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount = 1);

	/** Executes one explicit item action for a player-layout address without heuristic fallback. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool ExecuteAddressItemAction(
		URpgInventoryAddressSlotViewModel* SlotViewModel,
		ERpgInventoryContextAction Action,
		int32 StackCount = 1);

	/** Quick-splits one logical player-inventory address slot. SplitCount <= 0 performs quick 50%. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickSplitAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount = 0);

	/** Dispatches an exact in-place rotation for a current spatial entry; it can never merge or swap another item. */
	bool RotateEntryInPlace(URpgInventoryEntryViewModel* EntryViewModel);

	/** Dispatches the same exact in-place rotation for a current player-layout content address. */
	bool RotateAddressSlotInPlace(URpgInventoryAddressSlotViewModel* SlotViewModel);

	/** Requests a manual world drop for the focused entry. Confirmed must be true for confirm-protected items. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool DropEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount = 0, bool bConfirmed = false);

	/** Requests a manual world drop for one logical player-inventory address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool DropAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount = 0, bool bConfirmed = false);

	/**
	 * Captures an immutable, caller-correlated manual-drop snapshot for one spatial inventory entry.
	 *
	 * The owning interaction screen arms this snapshot before dispatch so synchronous listen-server feedback cannot
	 * race the confirmation presenter. The server still revalidates every field before changing inventory state.
	 */
	bool PrepareDropEntryRequest(
		URpgInventoryEntryViewModel* EntryViewModel,
		int32 StackCount,
		URpgInventoryManagerComponent*& OutInventory,
		FRpgInventoryManualDropRequest& OutRequest) const;

	/** Captures the same stable drop snapshot for one logical player-inventory address or Carry slot. */
	bool PrepareDropAddressSlotRequest(
		URpgInventoryAddressSlotViewModel* SlotViewModel,
		int32 StackCount,
		URpgInventoryManagerComponent*& OutInventory,
		FRpgInventoryManualDropRequest& OutRequest) const;

	/**
	 * Moves the exact persistent item currently represented by a gear slot into compatible content space.
	 * The item id and current physical gear location are revalidated locally before the server validates again.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment Actions")
	bool UnequipEquipmentItem(ERpgEquipmentSlot EquipmentSlot, FRpgInventoryItemId ExpectedItemId);

	/**
	 * Requests a full-item world drop for the exact persistent item currently represented by a gear slot.
	 * Final ownership, drop-policy, and physical equipment cleanup remain server authoritative.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment Actions")
	bool DropEquipmentItem(ERpgEquipmentSlot EquipmentSlot, FRpgInventoryItemId ExpectedItemId, bool bConfirmed = false);

	/** Captures a stable drop snapshot only while the equipment slot still represents ExpectedItemId. */
	bool PrepareDropEquipmentItemRequest(
		ERpgEquipmentSlot EquipmentSlot,
		FRpgInventoryItemId ExpectedItemId,
		URpgInventoryManagerComponent*& OutInventory,
		FRpgInventoryManualDropRequest& OutRequest) const;

	/**
	 * Revalidates and dispatches a prepared request through the ID-based server-authoritative drop path.
	 *
	 * Confirmation retries call this with a fresh RequestId and bConfirmed=true after atomically consuming the
	 * screen-owned pending intent. No gameplay mutation occurs when the captured entry, source, or quantity is stale.
	 */
	bool DispatchManualDropRequest(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryManualDropRequest Request);

	/** Computes the visual drag/drop state for one inventory entry widget. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	ERpgInventorySlotDragVisualState GetInventoryEntryVisualState(URpgInventoryEntryViewModel* EntryViewModel, bool bIsFocused) const;

	/** Computes the visual drag/drop state for one player layout address slot widget. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	ERpgInventorySlotDragVisualState GetInventoryAddressSlotVisualState(URpgInventoryAddressSlotViewModel* SlotViewModel, bool bIsFocused) const;

	/** Local preview validation for hover/focus feedback. Server validation still owns the final result. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool PreviewDrop(const FRpgInventoryDropTarget& Target) const;

	/** Pure local validation for an explicit payload. Does not mutate or broadcast the shared interaction session. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const;

	/** Updates and broadcasts the shared target/preview state for an actual pointer or controller hover. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
	bool UpdateInteractionPreview(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target);

	/** Publishes a plan already evaluated by this coordinator so spatial widgets do not infer or evaluate it twice. */
	bool PublishInteractionPreview(
		const FRpgInventoryDragPayload& Payload,
		const FRpgInventoryDropTarget& Target,
		const FRpgInventoryInteractionPreviewPlan& PreviewPlan);

	/** Clears the hover/focus target while retaining the active payload and rotation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
	void ClearInteractionPreview();

	/** Resolves Move/Merge/Swap/Equip/Blocked/OOB semantics without sending a gameplay request. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
	ERpgInventoryInteractionPreviewState ResolveInteractionPreview(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const;

	/** Builds the native preview from the same placement query that the authoritative request re-evaluates. */
	FRpgInventoryInteractionPreviewPlan PlanInteractionPreview(
		const FRpgInventoryDragPayload& Payload,
		const FRpgInventoryDropTarget& Target) const;

	/** Dispatches the current payload to a target and retains it until authoritative acknowledgement. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool CommitDrop(const FRpgInventoryDropTarget& Target);

	/** Commits an explicit payload to a target, used by mouse drag/drop operations. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool CommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target);

	/**
	 * Dispatches a locally precomputed accepted plan without evaluating the same UI candidate again.
	 * The authoritative gateway always rebuilds the domain plan from current server state.
	 */
	bool CommitPlannedPayloadToTarget(
		const FRpgInventoryDragPayload& Payload,
		const FRpgInventoryDropTarget& Target,
		const FRpgInventoryInteractionPreviewPlan& PreviewPlan);

	/** Controller Accept helper for inventory slots: pick item when empty-handed, otherwise place on the slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool HandleInventoryEntryAccept(URpgInventoryEntryViewModel* EntryViewModel);

	/** Broadcast whenever controller pick/place starts, changes, or is cancelled. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|DragDrop")
	FRpgInventoryHeldPayloadChanged OnHeldPayloadChanged;

private:
	UFUNCTION()
	void HandleInteractionPayloadChanged(bool bHasPayload, const FRpgInventoryDragPayload& Payload);

	bool CanCommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const;
	bool IsSameInteractionPayload(const FRpgInventoryDragPayload& A, const FRpgInventoryDragPayload& B) const;
	bool IsTargetPlacementOutOfBounds(const FRpgInventoryDropTarget& Target) const;
	FGameplayTag ResolveActionTagForTarget(
		const FRpgInventoryDropTarget& Target,
		ERpgInventoryInteractionPreviewState AcceptedPreviewState) const;
	void EnsureInteractionSession();
	FGuid MarkInteractionRequestPending(
		const FRpgInventoryDragPayload& Payload,
		const FRpgInventoryDropTarget& Target,
		ERpgInventoryInteractionPreviewState AcceptedPreviewState,
		FGameplayTag ExpectedCarrySemanticRole = FGameplayTag());
	bool IsPayloadSourceCurrent(const FRpgInventoryDragPayload& Payload) const;
	bool IsHeldSourceEntry(URpgInventoryEntryViewModel* EntryViewModel) const;
	bool IsHeldSourceAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel) const;
	bool CanRotateEntryInPlace(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* ItemInstance,
		const FGuid& EntryId,
		const FRpgInventoryGridPlacement& SourcePlacement) const;
	bool DispatchRotateEntryInPlace(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* ItemInstance,
		const FGuid& EntryId,
		const FRpgInventoryGridPlacement& SourcePlacement);
	URpgInventoryUiActionComponent* ResolveUiActionComponent() const;
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgPlayerInventoryLayoutComponent* FindPlayerInventoryLayout() const;
	FRpgInventorySlotAddress ResolvePayloadSourceAddress(const FRpgInventoryDragPayload& Payload) const;
	FRpgInventorySlotAddress ResolveEquipmentPayloadSourceAddress(const FRpgInventoryDragPayload& Payload) const;
	URpgInventoryItemInstance* ResolveCurrentEquipmentItem(
		ERpgEquipmentSlot EquipmentSlot,
		const FRpgInventoryItemId& ExpectedItemId) const;
	bool BuildEquipmentIntent(
		const FRpgInventoryDragPayload& Payload,
		ERpgInventoryEquipmentIntentOperation Operation,
		ERpgEquipmentSlot TargetEquipmentSlot,
		const FGuid& RequestId,
		URpgInventoryManagerComponent*& OutInventory,
		FRpgInventoryEquipmentIntent& OutIntent,
		const FRpgInventoryGridPlacement& ExactTargetPlacement =
			FRpgInventoryGridPlacement()) const;
	bool TryResolveHandEquipmentTarget(
		const FRpgInventoryDropTarget& Target,
		ERpgEquipmentSlot& OutEquipmentSlot) const;
	bool BuildManualDropRequest(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* ItemInstance,
		int32 RequestedStackCount,
		FRpgInventoryManualDropRequest& OutRequest) const;
	bool IsManualDropRequestCurrent(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request) const;
	bool CanMoveItemOutOfAddress(const FRpgInventorySlotAddress& SourceAddress) const;
	bool IsPlayerInventory(const URpgInventoryManagerComponent* Inventory) const;
	void BuildPlayerQuickTransferTargets(
		const FRpgInventoryGridPlacement& SourcePlacement,
		TArray<FRpgInventoryContainerHandle>& OutTargets) const;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryUiActionComponent> UiActionComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> FocusedInventory = nullptr;

	UPROPERTY(Transient)
	TArray<FRpgInventoryQuickTransferRoute> QuickTransferRoutes;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionSession> InteractionSession = nullptr;
};
