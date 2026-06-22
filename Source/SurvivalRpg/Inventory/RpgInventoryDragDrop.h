#pragma once

#include "Blueprint/DragDropOperation.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "UObject/Object.h"

#include "RpgInventoryDragDrop.generated.h"

class APlayerController;
class URpgInventoryEntryViewModel;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgInventoryUiActionComponent;

/** Kind of UI source represented by an inventory drag or controller-held payload. */
UENUM(BlueprintType)
enum class ERpgInventoryDragSourceType : uint8
{
	None,
	InventoryEntry,
	QuickBarSlot,
	EquipmentSlot
};

/** Kind of UI target that can receive an inventory drag or controller-held payload. */
UENUM(BlueprintType)
enum class ERpgInventoryDropTargetType : uint8
{
	None,
	InventorySlot,
	InventoryPanel,
	QuickBarSlot,
	EquipmentSlot,
	ClearSlot
};

/** UI presentation state for one inventory slot while controller pick/place is active. */
UENUM(BlueprintType)
enum class ERpgInventorySlotDragVisualState : uint8
{
	/** No special drag/drop state. */
	Normal,

	/** Slot is currently focused or selected by CommonUI. */
	Focused,

	/** Slot is the source of the currently held controller payload. */
	HeldSource,

	/** Slot can receive the currently held payload. */
	ValidTarget,

	/** Slot is visible but cannot receive the currently held payload. */
	InvalidTarget
};

/**
 * UI-only description of the item or assignment currently being dragged or held by controller input.
 *
 * This payload is never authoritative. It is only translated into server-validated requests on
 * URpgInventoryUiActionComponent.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryDragPayload
{
	GENERATED_BODY()

	/** Source UI area that produced this payload. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	ERpgInventoryDragSourceType SourceType = ERpgInventoryDragSourceType::None;

	/** Inventory that owns ItemInstance when SourceType is InventoryEntry. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	TObjectPtr<URpgInventoryManagerComponent> SourceInventory = nullptr;

	/** Item instance referenced by this UI action. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Stable entry id used for same-inventory manual reordering. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FGuid EntryId;

	/** Stack count requested by this payload. Values <= 0 mean full stack. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	int32 StackCount = 0;

	/** Source visual slot index for inventory or quickbar payloads. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	int32 SourceSlotIndex = INDEX_NONE;

	/** Source equipment slot for quickbar hand slots or dedicated equipment slots. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
};

/**
 * UI-only description of a target slot or panel that can receive a drag/drop commit.
 *
 * Widgets should build this from the focused or hovered slot, then let the coordinator translate it
 * into the existing server-authoritative inventory UI action component.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryDropTarget
{
	GENERATED_BODY()

	/** Target UI area that receives the payload. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	ERpgInventoryDropTargetType TargetType = ERpgInventoryDropTargetType::None;

	/** Inventory that receives a transfer or manual reorder. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	TObjectPtr<URpgInventoryManagerComponent> TargetInventory = nullptr;

	/** Target visual slot index for inventory reorder or capacity-slot placement. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	int32 TargetIndex = INDEX_NONE;

	/** Target quickbar slot index when TargetType is QuickBarSlot. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	int32 QuickBarSlotIndex = INDEX_NONE;

	/** Target equipment slot for quickbar hand slots or dedicated equipment slots. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgInventoryHeldPayloadChanged, bool, bHasHeldPayload, const FRpgInventoryDragPayload&, HeldPayload);

/** One quick-transfer route used by UI shortcuts such as Ctrl+Click or controller X. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryQuickTransferRoute
{
	GENERATED_BODY()

	/** Inventory whose focused entry should be transferred by the shortcut. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	TObjectPtr<URpgInventoryManagerComponent> SourceInventory = nullptr;

	/** Inventory that receives SourceInventory's shortcut transfers. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	TObjectPtr<URpgInventoryManagerComponent> TargetInventory = nullptr;
};

/** Native drag operation used by mouse drag/drop inventory widgets. */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/** UI-only payload carried by the mouse drag operation. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop", meta = (ExposeOnSpawn = "true"))
	FRpgInventoryDragPayload InventoryPayload;
};

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

	/** Builds an inventory-entry payload from a TileView entry model. Empty slots return an invalid payload. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDragPayload MakeInventoryPayloadFromEntry(URpgInventoryEntryViewModel* EntryViewModel);

	/** Builds an inventory-slot target from a TileView entry model, including UI-only empty capacity slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeInventoryTargetFromEntry(URpgInventoryEntryViewModel* EntryViewModel);

	/** Builds a panel-level target for transferring into an inventory without a specific slot index. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeInventoryPanelTarget(URpgInventoryManagerComponent* TargetInventory);

	/** Builds a payload from an existing quickbar assignment. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDragPayload MakeQuickBarPayload(URpgInventoryItemInstance* ItemInstance, int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot);

	/** Builds a target for a quickbar hand slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeQuickBarTarget(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot);

	/** Builds a payload from an existing dedicated equipment assignment. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDragPayload MakeEquipmentPayload(URpgInventoryItemInstance* ItemInstance, ERpgEquipmentSlot EquipmentSlot);

	/** Builds a target for a dedicated equipment slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeEquipmentTarget(ERpgEquipmentSlot EquipmentSlot);

	/** Builds a target that clears quickbar or equipment assignments without moving the owned item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeClearTarget();

	/** Returns true when the payload has enough data to start a drag or controller hold. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	static bool IsPayloadValid(const FRpgInventoryDragPayload& Payload);

	/** Returns true when the target has enough data to receive a payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	static bool IsTargetValid(const FRpgInventoryDropTarget& Target);

	/** Starts the controller pick/place flow with a payload. No gameplay RPC is sent. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool BeginHold(const FRpgInventoryDragPayload& Payload);

	/** Starts holding the focused inventory entry for controller pick/place. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool BeginHoldFromEntry(URpgInventoryEntryViewModel* EntryViewModel);

	/** Clears the controller-held payload without changing gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void CancelHold();

	/** Returns true while controller pick/place is holding a payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	bool HasHeldPayload() const { return bHasHeldPayload; }

	/** Returns the current controller-held payload for UI previews. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	FRpgInventoryDragPayload GetHeldPayload() const { return HeldPayload; }

	/** Returns the held item instance, or null when controller pick/place is empty-handed. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	URpgInventoryItemInstance* GetHeldItemInstance() const { return bHasHeldPayload ? HeldPayload.ItemInstance.Get() : nullptr; }

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

	/** Resolves the shortcut transfer target for a source inventory, including player-inventory fallback. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	URpgInventoryManagerComponent* ResolveQuickTransferTarget(URpgInventoryManagerComponent* SourceInventory) const;

	/** Returns true when the focused entry can send a full-stack shortcut transfer command. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool CanQuickTransferEntry(URpgInventoryEntryViewModel* EntryViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr) const;

	/** Sends a full-stack shortcut transfer command for the focused entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickTransferEntry(URpgInventoryEntryViewModel* EntryViewModel, URpgInventoryManagerComponent* ExplicitTargetInventory = nullptr);

	/** Returns true when the focused entry can be split into a separate stack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool CanQuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 TargetSlotIndex = -1, int32 SplitCount = 0) const;

	/** Sends a split-stack command. SplitCount <= 0 uses the V1 50% quick split rule. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickSplitEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 TargetSlotIndex = -1, int32 SplitCount = 0);

	/** Computes the visual drag/drop state for one inventory entry widget. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	ERpgInventorySlotDragVisualState GetInventoryEntryVisualState(URpgInventoryEntryViewModel* EntryViewModel, bool bIsFocused) const;

	/** Local preview validation for hover/focus feedback. Server validation still owns the final result. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool PreviewDrop(const FRpgInventoryDropTarget& Target) const;

	/** Local preview validation for an explicit mouse drag payload. No gameplay RPC is sent. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const;

	/** Commits the current controller-held payload to a target and clears it on success. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool CommitDrop(const FRpgInventoryDropTarget& Target);

	/** Commits an explicit payload to a target, used by mouse drag/drop operations. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool CommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target);

	/** Controller Accept helper for inventory slots: pick item when empty-handed, otherwise place on the slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool HandleInventoryEntryAccept(URpgInventoryEntryViewModel* EntryViewModel);

	/** Broadcast whenever controller pick/place starts, changes, or is cancelled. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|DragDrop")
	FRpgInventoryHeldPayloadChanged OnHeldPayloadChanged;

private:
	bool CanCommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const;
	bool IsHeldSourceEntry(URpgInventoryEntryViewModel* EntryViewModel) const;
	URpgInventoryUiActionComponent* ResolveUiActionComponent() const;
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	bool IsPlayerInventory(const URpgInventoryManagerComponent* Inventory) const;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryUiActionComponent> UiActionComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> FocusedInventory = nullptr;

	UPROPERTY(Transient)
	TArray<FRpgInventoryQuickTransferRoute> QuickTransferRoutes;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryDragPayload HeldPayload;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	bool bHasHeldPayload = false;
};
