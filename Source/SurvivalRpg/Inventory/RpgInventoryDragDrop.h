#pragma once

#include "Blueprint/DragDropOperation.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "UObject/Object.h"

#include "RpgInventoryDragDrop.generated.h"

class APlayerController;
class URpgActionBarSlotViewModel;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryEntryViewModel;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgInventoryInteractionSession;
class URpgPlayerInventoryLayoutComponent;
class URpgInventoryUiActionComponent;

/** Input path that currently owns the transient inventory interaction. */
UENUM(BlueprintType)
enum class ERpgInventoryInteractionInputMode : uint8
{
	None,
	Mouse,
	Controller
};

/**
 * Semantic local preview for the current inventory target.
 *
 * These values are presentation-only. The server still performs final placement, equipment, and access validation.
 */
UENUM(BlueprintType)
enum class ERpgInventoryInteractionPreviewState : uint8
{
	/** No payload or target is currently being previewed. */
	None,

	/** The payload will move into an empty compatible target. */
	Move,

	/** The payload will merge into a compatible stack. */
	Merge,

	/** The payload and one occupied target will exchange placements. */
	Swap,

	/** The payload will be assigned to an equipment slot. */
	Equip,

	/** The payload will be assigned to an actionbar slot. */
	Bind,

	/** The payload will clear its current assignment. */
	Clear,

	/** The target is inside the UI but fails local compatibility or capacity checks. */
	Blocked,

	/** The rotated or unrotated footprint extends outside the target grid. */
	OutOfBounds,

	/** A server-authoritative request was dispatched and is awaiting replicated acknowledgement. */
	Pending,

	/** The most recent request was rejected; the held payload remains available for another target. */
	Rejected
};

/** Kind of UI source represented by an inventory drag or controller-held payload. */
UENUM(BlueprintType)
enum class ERpgInventoryDragSourceType : uint8
{
	None,
	InventoryEntry,
	PlayerInventorySlotAddress,
	EquipmentSlot
};

/** Kind of UI target that can receive an inventory drag or controller-held payload. */
UENUM(BlueprintType)
enum class ERpgInventoryDropTargetType : uint8
{
	None,
	InventorySlot,
	InventoryPanel,
	EquipmentSlot,
	PlayerInventorySlotAddress,
	ActionBarSlot,
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
 * DPI-independent point inside a dragged spatial item that stays attached to the pointer.
 *
 * The source widget is sampled on the original mouse-down. Placement later rebuilds pixels from the target grid's
 * cell metrics, so differently sized gear widgets and spatial grids cannot disagree about the item origin.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryDragAnchor
{
	GENERATED_BODY()

	/** Whether this payload captured a usable pointer anchor. Controller-held payloads may leave this false. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	bool bValid = false;

	/** Cell inside the currently oriented footprint that was grabbed on mouse-down. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FIntPoint GrabbedCell = FIntPoint::ZeroValue;

	/** Normalized 0..1 point inside GrabbedCell. It is transformed together with the cell on rotation. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FVector2D WithinCellNormalized = FVector2D(0.5f, 0.5f);

	/** Presentation-only source size used to position the free-floating decorator; never used for grid placement. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FVector2D SourceVisualSize = FVector2D::ZeroVector;

	/** Presentation-only source pixel offset captured before Slate crosses its drag trigger threshold. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FVector2D SourcePointerOffset = FVector2D::ZeroVector;

	/** Source visual size in Slate absolute screen units, used only to route the free ghost across DPI-scaled targets. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FVector2D SourceScreenVisualSize = FVector2D::ZeroVector;

	/** Pointer offset in Slate absolute screen units, paired with SourceScreenVisualSize for DPI-safe ghost routing. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FVector2D SourceScreenPointerOffset = FVector2D::ZeroVector;

	/** Orientation represented by GrabbedCell and WithinCellNormalized. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	bool bRotated = false;
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

	/** Source spatial placement for inventory payloads. UI-only, server revalidates before moving. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FRpgInventoryGridPlacement SourcePlacement;

	/** Unrotated definition footprint for content grids. Gear and carry slots may store the same item as 1x1. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FRpgInventoryGridSize ItemFootprint;

	/** Canonical pointer anchor captured on mouse-down and projected through the target grid's own cell metrics. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FRpgInventoryDragAnchor DragAnchor;

	/** True when spatial UI should keep a clicked/selected cell inside the item footprint anchored to the cursor. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	bool bHasSpatialGrabOffset = false;

	/** X cell offset inside SourcePlacement's occupied footprint that should stay under the cursor during placement. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop", meta = (ClampMin = "0", UIMin = "0"))
	int32 GrabCellOffsetX = 0;

	/** Y cell offset inside SourcePlacement's occupied footprint that should stay under the cursor during placement. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop", meta = (ClampMin = "0", UIMin = "0"))
	int32 GrabCellOffsetY = 0;

	/** True when mouse drag placement should derive the target origin from the visible item ghost's top-left corner. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop", meta = (DeprecatedProperty, DeprecationMessage = "Use DragAnchor; raw source pixels are not placement truth."))
	bool bHasPointerGrabOffset = false;

	/** Pixel offset from the dragged item's top-left to the pointer at mouse drag start. UI-only and never authoritative. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop", meta = (DeprecatedProperty, DeprecationMessage = "Use DragAnchor; raw source pixels are not placement truth."))
	FVector2D PointerGrabOffset = FVector2D::ZeroVector;

	/** Pixel size of the dragged item widget at mouse drag start. Used only for responsive UI preview and diagnostics. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop", meta = (DeprecatedProperty, DeprecationMessage = "Use DragAnchor.SourceVisualSize for decorator presentation only."))
	FVector2D DragVisualSize = FVector2D::ZeroVector;

	/** Logical source address when the payload came from the player inventory layout UI. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FRpgInventorySlotAddress SourceSlotAddress;

	/** Source equipment slot for equipped hand or armor slots. */
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

	/** Target spatial placement for inventory reorder or exact capacity placement. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FRpgInventoryGridPlacement TargetPlacement;

	/** Target logical address for player-inventory layout slots or actionbar slot-source binding. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	FRpgInventorySlotAddress SlotAddress;

	/** Target 1..8 actionbar slot index for actionbar binding drops. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	int32 ActionBarSlotIndex = INDEX_NONE;

	/** Target equipment slot for equipped hand or armor slots. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
};

/**
 * One cached spatial candidate shared by target ghost, cell indicators, interaction feedback, and final commit.
 *
 * This is transient owning-client presentation state. The server still validates Target and TargetPlacement.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySpatialPreviewDescriptor
{
	GENERATED_BODY()

	/** True when a spatial grid currently owns this pointer candidate. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	bool bValid = false;

	/** Stable replicated entry identity of the payload being previewed. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	FGuid EntryId;

	/** Exact server-validatable target built once for both preview and commit. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	FRpgInventoryDropTarget Target;

	/** Placement retained even when it extends out of bounds so the UI can render a red edge footprint. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	FRpgInventoryGridPlacement TargetPlacement;

	/** Semantic result calculated once for this candidate. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	ERpgInventoryInteractionPreviewState PreviewState = ERpgInventoryInteractionPreviewState::None;

	/** Snapped top-left in the target grid's local Slate units. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	FVector2D SnappedLocalPosition = FVector2D::ZeroVector;

	/** Exact occupied footprint size using the target grid's cell size and padding. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	FVector2D SnappedLocalSize = FVector2D::ZeroVector;

	/** Most recent pointer position used to resolve this candidate, retained for rotation without mouse movement. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Interaction")
	FVector2D PointerScreenPosition = FVector2D::ZeroVector;

	/** Returns whether both descriptors address the same payload, target placement, and semantic state. */
	bool IsEquivalentTo(const FRpgInventorySpatialPreviewDescriptor& Other) const;
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

	/** Connects drag cancellation to the same screen-local interaction used by controller pick/place. */
	void SetInteractionSession(URpgInventoryInteractionSession* InInteractionSession);
	URpgInventoryInteractionSession* GetInteractionSession() const { return InteractionSession.Get(); }

	/** Mirrors UE's TopLeft decorator placement so target routing uses the center of the visible free ghost. */
	FVector2D ResolveDecoratorCenterScreen(FVector2D PointerScreenPosition) const;

	/** Pulls rotation/state from the shared session immediately, including when no pointer-move event is generated. */
	void SynchronizeFromInteractionSession();

	/** Suppresses UE's interpolated decorator while a screen paints the canonical free ghost itself. */
	void SetScreenOwnedDragVisualActive(bool bInActive);

	virtual void Dragged_Implementation(const FPointerEvent& PointerEvent) override;
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

private:
	void RefreshDecoratorPointerOffset();
	bool bScreenOwnedDragVisualActive = false;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionSession> InteractionSession = nullptr;
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

	/** Builds a payload from an existing equipment assignment. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDragPayload MakeEquipmentPayload(URpgInventoryItemInstance* ItemInstance, ERpgEquipmentSlot EquipmentSlot);

	/** Builds a target for an equipment slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeEquipmentTarget(ERpgEquipmentSlot EquipmentSlot);

	/** Builds a target for binding a held player-inventory slot address to one 1..8 actionbar slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeActionBarSlotTarget(int32 ActionBarSlotIndex);

	/** Builds a target from an actionbar slot view model. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	static FRpgInventoryDropTarget MakeActionBarSlotTargetFromViewModel(URpgActionBarSlotViewModel* SlotViewModel);

	/** Builds a target that clears equipment assignments without moving the owned item. */
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

	/** Uses a usable item, otherwise tries to equip it through equipment slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool UseOrEquipEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount = 1);

	/** Executes one explicit item action without falling back to a different intent for hybrid items. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool ExecuteEntryItemAction(
		URpgInventoryEntryViewModel* EntryViewModel,
		ERpgInventoryItemActionIntent Intent,
		int32 StackCount = 1);

	/** Uses/equips or unequips one logical player-inventory address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool UseOrEquipAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount = 1);

	/** Executes one explicit item action for a player-layout address without heuristic fallback. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool ExecuteAddressItemAction(
		URpgInventoryAddressSlotViewModel* SlotViewModel,
		ERpgInventoryItemActionIntent Intent,
		int32 StackCount = 1);

	/** Quick-splits one logical player-inventory address slot. SplitCount <= 0 performs quick 50%. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool QuickSplitAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, FRpgInventoryGridPlacement TargetPlacement, int32 SplitCount = 0);

	/** Requests a manual world drop for the focused entry. Confirmed must be true for confirm-protected items. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool DropEntry(URpgInventoryEntryViewModel* EntryViewModel, int32 StackCount = 0, bool bConfirmed = false);

	/** Requests a manual world drop for one logical player-inventory address slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool DropAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel, int32 StackCount = 0, bool bConfirmed = false);

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

	/** Clears the hover/focus target while retaining the active payload and rotation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
	void ClearInteractionPreview();

	/** Resolves Move/Merge/Swap/Equip/Blocked/OOB semantics without sending a gameplay request. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
	ERpgInventoryInteractionPreviewState ResolveInteractionPreview(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const;

	/** Dispatches the current payload to a target and retains it until authoritative acknowledgement. */
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
	UFUNCTION()
	void HandleInteractionPayloadChanged(bool bHasPayload, const FRpgInventoryDragPayload& Payload);

	bool CanCommitPayloadToTarget(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target) const;
	bool IsSameInteractionPayload(const FRpgInventoryDragPayload& A, const FRpgInventoryDragPayload& B) const;
	bool IsTargetPlacementOutOfBounds(const FRpgInventoryDropTarget& Target) const;
	FGameplayTag ResolveActionTagForTarget(const FRpgInventoryDropTarget& Target) const;
	void EnsureInteractionSession();
	FGuid MarkInteractionRequestPending(const FRpgInventoryDragPayload& Payload, const FRpgInventoryDropTarget& Target);
	bool IsHeldSourceEntry(URpgInventoryEntryViewModel* EntryViewModel) const;
	bool IsHeldSourceAddressSlot(URpgInventoryAddressSlotViewModel* SlotViewModel) const;
	URpgInventoryUiActionComponent* ResolveUiActionComponent() const;
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgPlayerInventoryLayoutComponent* FindPlayerInventoryLayout() const;
	FRpgInventorySlotAddress ResolvePayloadSourceAddress(const FRpgInventoryDragPayload& Payload) const;
	FRpgInventorySlotAddress ResolveEquipmentPayloadSourceAddress(const FRpgInventoryDragPayload& Payload) const;
	URpgInventoryItemInstance* ResolveCurrentEquipmentItem(
		ERpgEquipmentSlot EquipmentSlot,
		const FRpgInventoryItemId& ExpectedItemId) const;
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
