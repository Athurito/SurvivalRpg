#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgInventoryUiActionComponent.generated.h"

class ARpgBaseCampActor;
class ARpgBaseConstructionSiteActor;
class ARpgDroppedInventoryActor;
class APlayerController;
class FRpgInventoryUiActionDomainHandler;
class URpgAbilitySystemComponent;
class URpgEquipmentLoadoutComponent;
class URpgBaseBuildableDefinition;
class URpgBaseStorageComponent;
class URpgBaseStorageStationComponent;
class URpgBaseStorageUpgradeDefinition;
class URpgCraftingRecipeDefinition;
class URpgCraftingStationComponent;
class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgActionBarComponent;
class URpgPlayerInventoryLayoutComponent;

/** Owning-client result for an inventory UI command. */
UENUM(BlueprintType)
enum class ERpgInventoryActionFeedbackResult : uint8
{
	/** The action completed or was accepted by the server. */
	Success,

	/** The request was malformed or referenced missing runtime state. */
	InvalidRequest,

	/** The player cannot access one of the referenced inventories or stations. */
	NoAccess,

	/** The referenced item is missing or no longer owned by the source inventory. */
	MissingItem,

	/** The target inventory or slot has no capacity for the requested item. */
	InventoryFull,

	/** The requested slot index is invalid or blocked. */
	InvalidSlot,

	/** The item is not stackable or cannot be split as requested. */
	NotStackable,

	/** The item has no usable behavior or its use ability was rejected. */
	CannotUse,

	/** Manual dropping is disabled for this item. */
	CannotDrop,

	/** The server requires the UI to confirm before repeating the request. */
	RequiresConfirmation,

	/** This action is only valid from the player's own inventory. */
	WrongInventory,

	/** The item cannot be assigned to any supported actionbar or equipment slot. */
	NotEquippable,

	/** No compatible actionbar or equipment slot could be found. */
	NoValidSlot,

	/** GAS rejected the one-shot item ability activation. */
	AbilityRejected,

	/** Fallback for a server-side rejection without a more specific reason. */
	ServerRejected
};

/** Stable, request-correlated command for using one inventory item. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryUseRequest
{
	GENERATED_BODY()

	/** Caller-owned correlation id used for bounded replay-window deduplication and reliable owning-client feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use")
	FGuid RequestId;

	/** Persistent identity resolved against Inventory on the server; no client UObject pointer is trusted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use")
	FRpgInventoryItemId ItemId;

	/** Number of uses requested; the server derives any consumed quantity from the authored usable-item contract. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use", meta = (ClampMin = "1", UIMin = "1"))
	int32 UseCount = 1;

	/** Generates a caller-owned request id when a native presenter has not assigned one yet. */
	void EnsureRequestId()
	{
		if (!RequestId.IsValid())
		{
			RequestId = FGuid::NewGuid();
		}
	}
};

/**
 * Stable, request-correlated split command for one exact replicated inventory entry.
 *
 * The server resolves ItemId again and rejects stale entry identity, placement, quantity, or target state before
 * creating the new stack. SplitCount is always explicit; quick-split policy is resolved by the presenter first.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventorySplitRequest
{
	GENERATED_BODY()

	/** Caller-owned correlation id used for bounded replay-window deduplication and owning-client feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Split")
	FGuid RequestId;

	/** Persistent source item identity resolved against Inventory on the authoritative server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Split")
	FRpgInventoryItemId ItemId;

	/** Stable replicated entry identity captured by the initiating presenter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Split")
	FGuid ExpectedEntryId;

	/** Complete source placement captured by the initiating presenter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Split")
	FRpgInventoryGridPlacement ExpectedSourcePlacement;

	/** Complete source stack count captured independently from SplitCount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Split", meta = (ClampMin = "2", UIMin = "2"))
	int32 ExpectedSourceQuantity = 0;

	/** Exact number of units moved into the new stack; must be smaller than ExpectedSourceQuantity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Split", meta = (ClampMin = "1", UIMin = "1"))
	int32 SplitCount = 0;

	/** Exact empty destination placement in the same inventory, revalidated by the server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Split")
	FRpgInventoryGridPlacement TargetPlacement;

	void EnsureRequestId()
	{
		if (!RequestId.IsValid())
		{
			RequestId = FGuid::NewGuid();
		}
	}
};

/**
 * Equipment behavior requested for one complete player-inventory entry.
 *
 * Gear and Carry locations remain inventory truth. Hand activation is an explicit selection owned by the
 * equipment loadout and never substitutes for a physical move.
 */
UENUM(BlueprintType)
enum class ERpgInventoryEquipmentIntentOperation : uint8
{
	/** Resolves the item's authored default Gear/Carry destination and activates it when that destination is a hand. */
	EquipDefaultAndActivate,

	/** Moves the complete entry to TargetEquipmentSlot and activates it when TargetEquipmentSlot is a hand. */
	EquipToSlot,

	/** Moves the complete entry to the first compatible Carry slot without changing active-hand selection. */
	MoveToCarry,

	/** Moves the complete entry from Gear/Carry to the first compatible Content placement. */
	UnequipToContent,

	/** Clears the selected active hand without moving its concrete item out of Carry. */
	ClearActiveSelection
};

/**
 * Stable, request-correlated equipment intent validated against one exact replicated inventory entry.
 *
 * The server resolves ItemId again and rejects stale EntryId, placement, or quantity snapshots before selecting a
 * destination. Equipment is whole-entry only; partial stack equip/unequip is intentionally unsupported.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryEquipmentIntent
{
	GENERATED_BODY()

	/** Caller-owned correlation id used for bounded replay-window deduplication and owning-client feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment")
	FGuid RequestId;

	/** Persistent identity resolved against Inventory on the authoritative server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment")
	FRpgInventoryItemId ItemId;

	/** Stable replicated entry identity captured when the interaction began. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment")
	FGuid ExpectedEntryId;

	/** Complete source placement captured when the interaction began. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment")
	FRpgInventoryGridPlacement ExpectedSourcePlacement;

	/** Complete source stack count. Equipment intents reject partial or stale quantities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment", meta = (ClampMin = "1", UIMin = "1"))
	int32 ExpectedQuantity = 0;

	/** Physical move or activation-only hand-selection behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment")
	ERpgInventoryEquipmentIntentOperation Operation =
		ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate;

	/**
	 * Explicit semantic target for EquipToSlot or ClearActiveSelection.
	 * Other operations require None so a reused RequestId cannot silently change meaning through ignored payload.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment")
	ERpgEquipmentSlot TargetEquipmentSlot = ERpgEquipmentSlot::None;

	/**
	 * Optional exact physical Carry destination for a hand EquipToSlot request.
	 * Invalid preserves deterministic first-compatible routing; every other operation rejects a populated value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Equipment")
	FRpgInventoryGridPlacement ExactTargetPlacement;

	void EnsureRequestId()
	{
		if (!RequestId.IsValid())
		{
			RequestId = FGuid::NewGuid();
		}
	}
};

/** Deterministic, atomic quick-transfer request evaluated again by the server. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryQuickTransferRequest
{
	GENERATED_BODY()

	/** Client-generated correlation id copied unchanged into the reliable owning-client feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	FGuid RequestId;

	/** Persistent identity of the source item resolved by SourceInventory on the server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	FRpgInventoryItemId ItemId;

	/** Exact replicated entry identity captured by the initiating presenter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	FGuid ExpectedEntryId;

	/** Complete source placement captured by the initiating presenter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	FRpgInventoryGridPlacement ExpectedSourcePlacement;

	/** Complete source stack count captured independently from StackCount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer", meta = (ClampMin = "1", UIMin = "1"))
	int32 ExpectedSourceQuantity = 0;

	/** Requested transfer amount. Same-inventory quick transfer remains whole-entry only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer", meta = (ClampMin = "1", UIMin = "1"))
	int32 StackCount = 0;

	/**
	 * Ordered destination candidates. The server selects the first container that can accept the complete request.
	 * Empty uses the player's content routing policy or the target inventory's default root.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	TArray<FRpgInventoryContainerHandle> PreferredTargetContainers;

	void EnsureRequestId()
	{
		if (!RequestId.IsValid())
		{
			RequestId = FGuid::NewGuid();
		}
	}
};

/** Server-authoritative operation applied to one of the eight shared Quick Access bindings. */
UENUM(BlueprintType)
enum class ERpgQuickAccessMutationOperation : uint8
{
	/** Binds the current item at SourceAddress as a consumable definition plus preferred item id. */
	BindConsumable,

	/** Binds the semantic Carry role represented by SourceAddress, never the concrete carried item. */
	BindCarry,

	/** Clears a consumable only when the authoritative binding still matches the expected definition and item id. */
	ClearConsumable,

	/** Clears a Carry binding only when the authoritative binding still matches the expected semantic role. */
	ClearCarry
};

/**
 * Request-correlated Quick Access bind/clear command.
 *
 * Expected fields are snapshots of the binding semantics displayed by the owning client. The server resolves the
 * current inventory address and actionbar slot again, rejects stale commands, and echoes RequestId in every result.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgQuickAccessMutationRequest
{
	GENERATED_BODY()

	/** Client-generated correlation id echoed unchanged in owning-client action feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access")
	FGuid RequestId;

	/** Bind or clear operation whose semantic fields are validated by the server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access")
	ERpgQuickAccessMutationOperation Operation = ERpgQuickAccessMutationOperation::BindConsumable;

	/** Internal zero-based Quick Access index in the fixed range 0..7. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access", meta = (ClampMin = "0", ClampMax = "7", UIMin = "0", UIMax = "7"))
	int32 SlotIndex = INDEX_NONE;

	/** Player-inventory address re-resolved for bind requests; ignored by clear requests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access")
	FRpgInventorySlotAddress SourceAddress;

	/** Expected explicit Carry group role for bind/clear; concrete Carry items and container ids may change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access", meta = (Categories = "Rpg.Inventory.Layout.Role.Carry"))
	FGameplayTag ExpectedCarrySemanticRole;

	/** Expected consumable definition for consumable bind/clear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access")
	TSubclassOf<URpgInventoryItemDefinition> ExpectedConsumableDefinition;

	/** Expected preferred stack stored in the consumable binding, used to reject stale clear commands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access")
	FRpgInventoryItemId ExpectedPreferredItemId;

	/** Item that initiated the UI command, retained only for feedback correlation and stale bind validation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access")
	FRpgInventoryItemId ContextItemId;

	void EnsureRequestId()
	{
		if (!RequestId.IsValid())
		{
			RequestId = FGuid::NewGuid();
		}
	}
};

/**
 * Stable, request-correlated manual world-drop command.
 *
 * Every field is a snapshot of the exact replicated entry the owning client presented. The server resolves that
 * entry again and rejects the command when identity, placement, or requested quantity has become stale.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryManualDropRequest
{
	GENERATED_BODY()

	/**
	 * Client-generated id used for owning-client feedback and bounded server-side replay deduplication.
	 * It must be unique to this command and must not be reused by another inventory operation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	FGuid RequestId;

	/** Stable replicated entry identity captured by the initiating inventory presenter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	FGuid EntryId;

	/** Persistent identity of the concrete item expected in EntryId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	FRpgInventoryItemId ItemId;

	/**
	 * Complete source placement captured before dispatch.
	 * Confirmed retries fail closed if the entry moved container, cell, footprint, or rotation meanwhile.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	FRpgInventoryGridPlacement ExpectedSourcePlacement;

	/** Complete source stack count captured independently from the requested drop amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop", meta = (ClampMin = "1", UIMin = "1"))
	int32 ExpectedSourceQuantity = 0;

	/** Exact number of units to drop. The server rejects non-positive or no-longer-available quantities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop", meta = (ClampMin = "1", UIMin = "1"))
	int32 StackCount = 1;

	/**
	 * UI acknowledgement for items whose current authoritative manual-drop policy requires confirmation.
	 * A confirmed follow-up is a new command and must therefore use a fresh RequestId.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	bool bConfirmed = false;
};

/** Gameplay message broadcast on the owning client after inventory UI commands succeed, fail, or need confirmation. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryActionFeedbackMessage
{
	GENERATED_BODY()

	/**
	 * Owning local controller that should present this result.
	 * Assigned on the receiving client before the gameplay message is broadcast.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	TObjectPtr<APlayerController> Recipient = nullptr;

	/** Correlation id supplied by the initiating request. Feature commands without caller correlation may emit an invalid id. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	FGuid RequestId;

	/** Persistent item identity retained even when a transfer removes or reconstructs the source UObject. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	FRpgInventoryItemId ItemId;

	/** Semantic action that produced this feedback, such as Rpg.Inventory.Action.Drop. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	FGameplayTag ActionTag;

	/** Result code intended for UI sounds, toasts, confirmation modals, or slot flashes. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	ERpgInventoryActionFeedbackResult Result = ERpgInventoryActionFeedbackResult::ServerRejected;

	/** Inventory involved in the request, when relevant. UI should read this only as context. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	/** Item involved while its UObject is still owned by InventoryOwner; use ItemId after full transfer/removal. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	TObjectPtr<URpgInventoryItemInstance> Item = nullptr;

	/** Requested or affected count. Zero means not count-specific. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	int32 StackCount = 0;

	/** Returns whether this owner-local result may be consumed by the given controller. Null remains legacy-local broadcast behavior. */
	bool IsAddressedTo(const APlayerController* Controller) const
	{
		return !Recipient || Recipient.Get() == Controller;
	}
};

/**
 * Owned controller component that turns UI drag-and-drop intents into server-validated inventory actions.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgInventoryUiActionComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgInventoryUiActionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Commits one whole-entry move whose merge, swap, or rotation behavior is derived by the server planner. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Transactions")
	void RequestMoveInventoryItem(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryMoveIntent Intent);

	/** Commits one stable ID-based cross-inventory transfer and echoes the caller's request id in all feedback. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Transactions")
	void RequestTransferInventoryItem(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryTransferIntent Intent);

	/** Resolves and uses one stable item identity through the authoritative capability and GAS path. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Use")
	void RequestUseInventoryItemById(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryUseRequest Request);

	/**
	 * Applies one whole-entry equipment intent against authoritative inventory state.
	 * Physical operations use the inventory planner; identical retries replay and RequestId collisions are rejected.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Equipment")
	void RequestApplyInventoryEquipmentIntent(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryEquipmentIntent Intent);

	/**
	 * Quick-transfers a complete entry (or a cross-inventory stack amount) from one exact presented source snapshot.
	 * Source and target may be the same player inventory; the server rejects stale entry, placement, or stack state.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Quick Transfer")
	void RequestQuickTransferItem(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryQuickTransferRequest Request);

	/**
	 * Builds the exact cross-inventory placement plan used by drag preview and re-evaluated by the server commit.
	 * Access, direction, source snapshot, player-loadout removal, concrete occupancy, and stack compatibility are read-only.
	 */
	FRpgInventoryPlacementPlan PlanExactTransferPlacement(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent) const;

	/**
	 * Selects the same deterministic destination and full placement plan as RequestQuickTransferItem.
	 * A fully merged cross-inventory result may leave OutTargetPlacement invalid while retaining OutTargetContainer.
	 */
	FRpgInventoryPlacementPlan PlanQuickTransferDestination(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		FRpgInventoryContainerHandle& OutTargetContainer,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;

	/**
	 * Resolves one explicit Gear/hand equip or physical unequip intent to its concrete destination and placement plan.
	 * Hand targets validate the current two-hand/offhand conflict; unequip excludes content supplied by the removed item.
	 */
	FRpgInventoryPlacementPlan PlanEquipmentIntentPlacement(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;

	/**
	 * Resolves the first deterministic quick-transfer destination without mutating inventory state.
	 * OutTargetPlacement is valid for same-inventory moves and for a cross-inventory new-entry location; a fully
	 * mergeable cross-inventory stack may return only OutTargetContainer.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Quick Transfer")
	bool FindQuickTransferDestination(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		FRpgInventoryContainerHandle& OutTargetContainer,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;

	/**
	 * Activates the server-authoritative Carry binding in one Quick Access slot.
	 * The expected semantic role rejects stale or forged client input; physical addresses are resolved on the server.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestActivateCarrySlot(int32 ActionBarSlotIndex, FGameplayTag ExpectedCarrySemanticRole);

	/** Clears the active MainHand/OffHand runtime state. Items remain in their inventory carry slots. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearActiveHands();

	/** Canonical bind/clear path with stable request correlation and server-validated expected binding semantics. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Quick Access")
	void RequestMutateQuickAccessBinding(FRpgQuickAccessMutationRequest Request);

	/** Splits one exact stable-ID source snapshot and deduplicates identical retries inside the bounded replay window. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestSplitItemStackById(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventorySplitRequest Request);

	/**
	 * Resolves split count and placement with the same read-only validation used by the authoritative request path.
	 *
	 * UI policy may use this for accurate availability hints; gameplay state is not mutated.
	 */
	bool CanSplitItemStack(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 SplitCount,
		FRpgInventoryGridPlacement TargetPlacement,
		int32& OutSplitCount,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;

	/**
	 * Finds a non-swapping player Content destination using the same rules as the authoritative unequip path.
	 *
	 * Provider-owned content that would disappear with the source equipment is excluded.
	 */
	bool CanMoveItemToFirstCompatibleContentSlot(
		URpgInventoryItemInstance* Item,
		FRpgInventoryGridPlacement& OutTargetPlacement) const;

	/**
	 * Drops the exact entry snapshot after server-side identity, placement, quantity, policy, and access validation.
	 * Reusing RequestId with a different payload is rejected; an identical retry replays the cached result.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestDropInventoryItemById(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryManualDropRequest Request);

	/** Deposits all material stacks from the player inventory into the linked base storage station. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestDepositAllMaterialsToBase(URpgBaseStorageStationComponent* Station);

	/** Deposits one material stack from the player inventory into the linked base storage station. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestDepositMaterialStackToBase(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Withdraws resources from the linked base storage station into the player inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestWithdrawResourceFromBase(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount);

	/** Installs a data-driven upgrade on a base storage station after paying material costs. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestInstallBaseStorageUpgrade(URpgBaseStorageStationComponent* Station, URpgBaseStorageUpgradeDefinition* UpgradeDefinition);

	/** Applies a shared server-side sort to the linked base resource rows. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestApplyBaseResourceSort(URpgBaseStorageStationComponent* Station, ERpgInventorySortMode SortMode);

	/** Moves one linked base resource row to a shared replicated index. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestMoveBaseResourceEntry(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex);

	/** Places a replicated construction site for a buildable near a base camp. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Building")
	void RequestPlaceBaseBuildable(ARpgBaseCampActor* BaseCamp, URpgBaseBuildableDefinition* BuildableDefinition, FTransform BuildTransform, bool bAutoContributeFromBase);

	/** Contributes all available matching resources to a construction site. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Building")
	void RequestContributeAllToBaseConstructionSite(ARpgBaseConstructionSiteActor* ConstructionSite, bool bAllowBaseStorage);

	/** Contributes one resource type to a construction site. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Building")
	void RequestContributeMaterialToBaseConstructionSite(ARpgBaseConstructionSiteActor* ConstructionSite, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, bool bAllowBaseStorage);

	/** Queues one or more units of a recipe through an accessible crafting station. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestCraftRecipe(URpgCraftingStationComponent* CraftingStation, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity = 1);

	/** Cancels one queued or active crafting job and refunds the unfinished resource credits. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestCancelCraftJob(URpgCraftingStationComponent* CraftingStation, FGuid JobId);

	/** Pauses a whole crafting station queue. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestPauseCraftingStation(URpgCraftingStationComponent* CraftingStation);

	/** Resumes a paused crafting station queue. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestResumeCraftingStation(URpgCraftingStationComponent* CraftingStation);

	/** Toggles whether an accessible crafting station auto-deposits finished outputs into linked base storage. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Crafting")
	void RequestSetCraftingOutputAutoDepositEnabled(URpgCraftingStationComponent* CraftingStation, bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|UI Actions")
	bool CanAccessInventory(URpgInventoryManagerComponent* Inventory) const;

private:
	friend class FRpgInventoryUiActionDomainHandler;
	friend class FRpgInventoryUseRequestReplayCacheContractTest;

	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgEquipmentLoadoutComponent* FindEquipmentLoadout() const;
	URpgPlayerInventoryLayoutComponent* FindPlayerInventoryLayout() const;
	URpgActionBarComponent* FindActionBar() const;
	URpgAbilitySystemComponent* FindPlayerAbilitySystem() const;
	bool CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const;
	bool IsPlayerEquipmentPlacement(
		const FRpgInventoryGridPlacement& Placement) const;
	void SyncEquipmentLoadoutFromGearSlots() const;
	void SyncActiveHandsFromCarrySlots() const;

	enum class EUseRequestAdmissionDisposition : uint8
	{
		Execute,
		Replay,
		InFlight,
		Reject
	};

	struct FUseRequestAdmission
	{
		EUseRequestAdmissionDisposition Disposition =
			EUseRequestAdmissionDisposition::Reject;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::InvalidRequest;
		TWeakObjectPtr<URpgInventoryItemInstance> FeedbackItem;
		int32 FeedbackUseCount = 0;
	};

	struct FRecentUseRequestResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory;
		bool bHadInventory = false;
		uint64 InventoryMutationEpoch = 0;
		FRpgInventoryUseRequest Request;
		bool bInFlight = true;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::ServerRejected;
		/** Item context authorized by the original execution; replay never resolves a fresh pointer. */
		TWeakObjectPtr<URpgInventoryItemInstance> FeedbackItem;
		int32 FeedbackUseCount = 0;
	};

	static bool AreUseRequestsEquivalent(
		const FRpgInventoryUseRequest& A,
		const FRpgInventoryUseRequest& B);
	FUseRequestAdmission AdmitUseRequest(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryUseRequest& Request);
	void FinalizeUseRequest(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryUseRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* FeedbackItem,
		int32 FeedbackUseCount);
	void RemoveRecentUseRequest(const FGuid& RequestId);
	bool MakeRoomForUseRequest();

	struct FRecentSplitRequestResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory;
		bool bHadInventory = false;
		uint64 InventoryMutationEpoch = 0;
		FRpgInventorySplitRequest Request;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::ServerRejected;
		/** Item context authorized by the original execution; replay never resolves a fresh pointer. */
		TWeakObjectPtr<URpgInventoryItemInstance> FeedbackItem;
		int32 FeedbackStackCount = 0;
	};

	static bool AreSplitRequestsEquivalent(
		const FRpgInventorySplitRequest& A,
		const FRpgInventorySplitRequest& B);
	bool TryReplayRecentSplitResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventorySplitRequest& Request);
	void SendAndCacheSplitFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventorySplitRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount);

	struct FRecentManualDropResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory;
		TWeakObjectPtr<URpgInventoryManagerComponent> TargetInventory;
		bool bHadInventory = false;
		bool bHadTargetInventory = false;
		uint64 InventoryMutationEpoch = 0;
		uint64 TargetMutationEpoch = 0;
		FRpgInventoryManualDropRequest Request;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::ServerRejected;
		/** Item context authorized by the original execution; replay never resolves a fresh pointer. */
		TWeakObjectPtr<URpgInventoryItemInstance> FeedbackItem;
		int32 FeedbackStackCount = 0;
	};

	static bool AreManualDropRequestsEquivalent(
		const FRpgInventoryManualDropRequest& A,
		const FRpgInventoryManualDropRequest& B);
	bool TryReplayRecentManualDropResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request);
	void SendAndCacheManualDropFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryManualDropRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount,
		URpgInventoryManagerComponent* TargetInventory = nullptr);
	struct FRecentExactTransferResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> SourceInventory;
		TWeakObjectPtr<URpgInventoryManagerComponent> TargetInventory;
		bool bHadSourceInventory = false;
		bool bHadTargetInventory = false;
		uint64 SourceMutationEpoch = 0;
		uint64 TargetMutationEpoch = 0;
		FRpgInventoryTransferIntent Intent;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::ServerRejected;
		/** Item context authorized by the original execution; replay never resolves a fresh pointer. */
		TWeakObjectPtr<URpgInventoryItemInstance> FeedbackItem;
		int32 FeedbackStackCount = 0;
	};
	static bool AreExactTransferIntentsEquivalent(
		const FRpgInventoryTransferIntent& A,
		const FRpgInventoryTransferIntent& B);
	bool TryReplayRecentExactTransferResult(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent);
	void SendAndCacheExactTransferFeedback(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryTransferIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount);
	struct FRecentQuickTransferResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> SourceInventory;
		TWeakObjectPtr<URpgInventoryManagerComponent> TargetInventory;
		bool bHadSourceInventory = false;
		bool bHadTargetInventory = false;
		uint64 SourceMutationEpoch = 0;
		uint64 TargetMutationEpoch = 0;
		FRpgInventoryQuickTransferRequest Request;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::ServerRejected;
		/** Item context authorized by the original execution; replay never resolves a fresh pointer. */
		TWeakObjectPtr<URpgInventoryItemInstance> FeedbackItem;
		int32 FeedbackStackCount = 0;
	};
	static bool AreQuickTransferRequestsEquivalent(
		const FRpgInventoryQuickTransferRequest& A,
		const FRpgInventoryQuickTransferRequest& B);
	bool TryReplayRecentQuickTransferResult(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request);
	void SendAndCacheQuickTransferFeedback(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		const FRpgInventoryQuickTransferRequest& Request,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount);
	struct FRecentEquipmentIntentResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory;
		bool bHadInventory = false;
		uint64 InventoryMutationEpoch = 0;
		FRpgInventoryEquipmentIntent Intent;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::ServerRejected;
		/** Item context authorized by the original execution; replay never resolves a fresh pointer. */
		TWeakObjectPtr<URpgInventoryItemInstance> FeedbackItem;
		int32 FeedbackStackCount = 0;
	};
	static bool AreEquipmentIntentsEquivalent(
		const FRpgInventoryEquipmentIntent& A,
		const FRpgInventoryEquipmentIntent& B);
	bool TryReplayRecentEquipmentIntentResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent);
	void SendAndCacheEquipmentIntentFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount);
	void SendActionFeedback(
		FGameplayTag ActionTag,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FGuid& RequestId = FGuid(),
		FRpgInventoryItemId ItemId = FRpgInventoryItemId()) const;

	UFUNCTION(Client, Reliable)
	void ClientBroadcastInventoryActionFeedback(const FRpgInventoryActionFeedbackMessage& Message);

private:
	/** Pickup actor class used when players manually drop inventory items. Runtime spawn is server-authoritative. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ARpgDroppedInventoryActor> ManualDropActorClass;

	/** Distance in centimeters in front of the pawn where manual drops are spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0", Units = "cm"))
	float ManualDropForwardDistance = 120.0f;

	/** Height offset in centimeters applied to manual drop spawn location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float ManualDropUpOffset = 30.0f;

	/** Radius in centimeters used to merge stackable manual drops into nearby dropped inventory actors. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0", Units = "cm"))
	float ManualDropMergeRadius = 250.0f;

	/** Server-local bounded replay-window guard for item use; in-flight records are never capacity-evicted. */
	TMap<FGuid, FRecentUseRequestResult> RecentUseRequestResults;
	TArray<FGuid> RecentUseRequestOrder;
	static constexpr int32 MaxRecentUseRequestResults = 64;

	/** Server-local bounded replay cache for exact split commands and their one-time equipment side effects. */
	TMap<FGuid, FRecentSplitRequestResult> RecentSplitRequestResults;
	TArray<FGuid> RecentSplitRequestOrder;
	static constexpr int32 MaxRecentSplitRequestResults = 64;

	/** Server-local replay cache; transient results are bounded and never replicated or saved. */
	TMap<FGuid, FRecentManualDropResult> RecentManualDropResults;
	TArray<FGuid> RecentManualDropOrder;
	static constexpr int32 MaxRecentManualDropResults = 64;

	/** Server-local replay cache for exact transfers and their one-time equipment side effects. */
	TMap<FGuid, FRecentExactTransferResult> RecentExactTransferResults;
	TArray<FGuid> RecentExactTransferOrder;
	static constexpr int32 MaxRecentExactTransferResults = 64;

	/** Server-local replay cache for quick-transfer commands whose exact destination is derived once. */
	TMap<FGuid, FRecentQuickTransferResult> RecentQuickTransferResults;
	TArray<FGuid> RecentQuickTransferOrder;
	static constexpr int32 MaxRecentQuickTransferResults = 64;

	/** Server-local replay cache covering the equipment command and its one-time hand/loadout side effects. */
	TMap<FGuid, FRecentEquipmentIntentResult> RecentEquipmentIntentResults;
	TArray<FGuid> RecentEquipmentIntentOrder;
	static constexpr int32 MaxRecentEquipmentIntentResults = 64;
};
