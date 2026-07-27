#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"

#include "RpgInventoryDragDropTypes.generated.h"

class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;

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

	/** Creates an empty payload without an implicit 1x1 item footprint. */
	FRpgInventoryDragPayload()
	{
		ItemFootprint.Width = 0;
		ItemFootprint.Height = 0;
	}

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

	/** Internal zero-based actionbar index (0..7); UI labels display the corresponding number 1..8. */
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

/**
 * Native, read-only projection of the gameplay placement plan for one UI candidate.
 *
 * The full plan remains outside Blueprint/MVVM state. Widgets consume only its semantic state and normalized target,
 * while the authoritative request re-evaluates the same query against current server state.
 */
struct SURVIVALRPG_API FRpgInventoryInteractionPreviewPlan
{
	/** Presentation semantic derived from PlacementPlan or a non-spatial UI policy. */
	ERpgInventoryInteractionPreviewState State =
		ERpgInventoryInteractionPreviewState::None;

	/** True when this result was projected from the inventory domain evaluator. */
	bool bUsesPlacementPlan = false;

	/** Concrete side-effect-free domain decision, including merge, swap, and displaced placement details. */
	FRpgInventoryPlacementPlan PlacementPlan;

	/** Normalized destination selected by the evaluator for presentation only. */
	FRpgInventoryGridPlacement ResolvedTargetPlacement;

	/** Returns whether this candidate may dispatch an authoritative request. */
	bool IsAccepted() const
	{
		return State != ERpgInventoryInteractionPreviewState::None &&
			State != ERpgInventoryInteractionPreviewState::Blocked &&
			State != ERpgInventoryInteractionPreviewState::OutOfBounds &&
			State != ERpgInventoryInteractionPreviewState::Pending &&
			State != ERpgInventoryInteractionPreviewState::Rejected;
	}
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
