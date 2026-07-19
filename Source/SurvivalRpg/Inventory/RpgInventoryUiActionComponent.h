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
class URpgAbilitySystemComponent;
class URpgEquipmentLoadoutComponent;
class URpgBaseBuildableDefinition;
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

/** Explicit server-side behavior requested for one concrete inventory item. */
UENUM(BlueprintType)
enum class ERpgInventoryItemActionIntent : uint8
{
	/** Activates only the item's configured usable behavior and never equips it as a fallback. */
	Use,

	/** Moves an equippable item to its default Gear/Carry destination and activates a Carry item when applicable. */
	EquipAndActivate,

	/** Moves an equippable item into the first compatible Carry slot without changing the active hand selection. */
	MoveToCarry
};

/** Stable, request-correlated item action sent by the owning inventory UI. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryItemActionRequest
{
	GENERATED_BODY()

	/** Client-generated correlation id copied unchanged into the reliable owning-client feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Actions")
	FGuid RequestId;

	/** Persistent identity resolved against Inventory on the server; no client UObject pointer is trusted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Actions")
	FRpgInventoryItemId ItemId;

	/** Exact behavior requested by the UI; hybrid usable/equippable items never rely on implicit fallback order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Actions")
	ERpgInventoryItemActionIntent Intent = ERpgInventoryItemActionIntent::Use;

	/** Number of uses requested for Use. Equipment intents always operate on the whole concrete item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Actions", meta = (ClampMin = "1", UIMin = "1"))
	int32 StackCount = 1;
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

	/** Requested amount. Values <= 0 mean the complete current stack; same-inventory transfer is whole-entry only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	int32 StackCount = 0;

	/**
	 * Ordered destination candidates. The server selects the first container that can accept the complete request.
	 * Empty uses the player's content routing policy or the target inventory's default root.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Transfer")
	TArray<FRpgInventoryContainerHandle> PreferredTargetContainers;
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

	/** Expected semantic Carry role for Carry bind/clear; concrete Carry items may change after binding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Quick Access")
	FName ExpectedCarryRole = NAME_None;

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
	 * Client-generated id used for owning-client feedback and bounded server-side exactly-once handling.
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

	/** Correlation id supplied by the initiating request. Legacy pointer APIs may emit an invalid id. */
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

	/**
	 * Executes placement and stack-management mutations inside one accessible inventory graph.
	 * Pickup, cross-inventory transfer, and physical drop use their dedicated validated request APIs.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Transactions")
	void RequestInventoryMutation(URpgInventoryManagerComponent* Inventory, FRpgInventoryMutationRequest Request);

	/** Executes one explicit Use, EquipAndActivate, or MoveToCarry intent using stable item identity. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestExecuteInventoryItemAction(URpgInventoryManagerComponent* Inventory, FRpgInventoryItemActionRequest Request);

	/**
	 * Quick-transfers a complete entry (or a cross-inventory stack amount) to the first candidate with real capacity.
	 * Source and target may be the same player inventory; the server always rescans and commits atomically.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Quick Transfer")
	void RequestQuickTransferItem(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryQuickTransferRequest Request);

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
	 * Predicts whether an accessible target inventory can accept the requested cross-inventory stack.
	 * This mirrors the authoritative UI-transfer direction policy; crafting outputs are withdrawal-only.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Transfer")
	bool CanTransferItemStack(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount) const;

	/**
	 * Predicts an exact cross-inventory placement using the same direction and capacity rules as the server.
	 * Occupied unlike-item placements are rejected because cross-inventory swaps are not a supported transaction.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Transfer")
	bool CanTransferItemStackToPlacement(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		FRpgInventoryGridPlacement TargetPlacement) const;

	/** Assigns an owned inventory item to an equipment slot such as MainHand, OffHand, Head, or Chest. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Clears one dedicated equipment slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Transfers a whole item entry or partial stack between two accessible inventories. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Transfers a stack into one exact target grid placement. Explicit drag/drop uses this instead of auto-stacking. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestTransferItemStackToPlacement(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, FRpgInventoryGridPlacement TargetPlacement);

	/** Applies a shared server-side sort to an accessible inventory such as storage or loot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestApplyInventorySort(URpgInventoryManagerComponent* Inventory, ERpgInventorySortMode SortMode);

	/** Moves one accessible inventory entry to a shared replicated index for manual storage ordering. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveInventoryEntry(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetIndex);

	/** Moves, swaps, or stack-merges an accessible inventory entry into one exact grid placement. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveInventoryEntryToPlacement(URpgInventoryManagerComponent* Inventory, FGuid EntryId, FRpgInventoryGridPlacement TargetPlacement);

	/** Moves an owned player-inventory item into a logical player grid address such as WeaponSlot1[0,0] or Belt[2,1]. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveItemToInventorySlotAddress(URpgInventoryItemInstance* Item, FRpgInventorySlotAddress TargetAddress);

	/** Assigns a bag, belt, pouch, or resource bag item to a slot-container equipment slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestEquipSlotContainerItem(ERpgEquipmentSlot ContainerSlot, URpgInventoryItemInstance* Item);

	/**
	 * Moves a bag, belt, pouch, or resource bag out of its physical Gear slot into compatible inventory content.
	 * Item-owned contents remain attached to the provider item and are not flattened into the player inventory.
	 * ExpectedProviderItemId prevents a stale client action from unequipping a newer item that now occupies the slot.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestUnequipSlotContainerItem(
		ERpgEquipmentSlot ContainerSlot,
		FRpgInventoryItemId ExpectedProviderItemId);

	/** Activates a carry slot as MainHand or OffHand without moving the item out of the inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestActivateCarrySlot(FRpgInventorySlotAddress CarrySlotAddress);

	/** Clears the active MainHand/OffHand runtime state. Items remain in their inventory carry slots. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearActiveHands();

	/** Canonical bind/clear path with stable request correlation and server-validated expected binding semantics. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Quick Access")
	void RequestMutateQuickAccessBinding(FRpgQuickAccessMutationRequest Request);

	/** Compatibility path; new UI should use RequestMutateQuickAccessBinding for caller-owned request correlation. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestBindActionBarToInventorySlot(int32 ActionBarSlotIndex, FRpgInventorySlotAddress SlotAddress);

	/** Compatibility path; new UI should use RequestMutateQuickAccessBinding for caller-owned request correlation. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestBindActionBarToCarrySlot(int32 ActionBarSlotIndex, FRpgInventorySlotAddress CarrySlotAddress);

	/** Compatibility path that still validates ExpectedCarryRole and emits correlated feedback. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearActionBarCarryBinding(int32 ActionBarSlotIndex, FName ExpectedCarryRole);

	/** Compatibility path that snapshots the current preferred item id before authoritative validation. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearActionBarConsumableBinding(
		int32 ActionBarSlotIndex,
		TSubclassOf<URpgInventoryItemDefinition> ExpectedConsumableDefinition);

	/** Splits one stack into a new stack in the same inventory. SplitCount <= 0 performs the quick 50% split. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestSplitItemStack(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 SplitCount, FRpgInventoryGridPlacement TargetPlacement);

	/** ID-based split request whose reliable feedback retains the caller's correlation id and Split action tag. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestSplitItemStackById(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryItemId ItemId,
		int32 SplitCount,
		FRpgInventoryGridPlacement TargetPlacement,
		FGuid RequestId);

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

	/** Uses a usable inventory item by granting and activating its configured one-shot ability. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestUseInventoryItem(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount = 1);

	/** Assigns an owned item to its default equipment destination, including MainHand, OffHand, and armor slots. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestEquipInventoryItem(URpgInventoryItemInstance* Item);

	/** Moves an owned gear/carry item back into the first compatible content slot and clears stale runtime equipment. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestUnequipInventoryItemToContentSlot(URpgInventoryItemInstance* Item);

	/**
	 * Legacy pointer-based wrapper retained for existing Blueprint callers.
	 * New presenters should capture a stable snapshot and call RequestDropInventoryItemById.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestDropInventoryItem(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount, bool bConfirmed);

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

	/** Stores an instance-based player item in the linked base armory inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestStoreItemInstanceInBase(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Takes an instance-based item from the linked base armory inventory into the player inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Base Storage")
	void RequestTakeItemInstanceFromBase(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount);

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
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgEquipmentLoadoutComponent* FindEquipmentLoadout() const;
	URpgPlayerInventoryLayoutComponent* FindPlayerInventoryLayout() const;
	URpgActionBarComponent* FindActionBar() const;
	URpgAbilitySystemComponent* FindPlayerAbilitySystem() const;
	bool TryFindTransferPlacementInContainer(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FRpgInventoryContainerHandle& TargetContainer,
		FRpgInventoryGridPlacement& OutPlacement) const;
	void BuildDefaultQuickTransferTargets(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryManagerComponent* TargetInventory,
		URpgInventoryItemInstance* Item,
		TArray<FRpgInventoryContainerHandle>& OutTargets) const;
	bool FindFirstEmptyInventoryPlacement(URpgInventoryManagerComponent* Inventory, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, FRpgInventoryGridPlacement& OutPlacement) const;
	bool CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const;
	bool ClearPlayerAssignmentsForItem(URpgInventoryItemInstance* Item) const;
	bool TryAssignItemToDefaultEquipmentDestination(URpgInventoryItemInstance* Item);
	bool TryMoveAndActivateItemInCarry(URpgInventoryItemInstance* Item, ERpgEquipmentSlot PreferredHandSlot);
	bool TryMoveItemToGearSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);
	bool TryMoveItemToFirstCompatibleCarrySlot(URpgInventoryItemInstance* Item);
	bool TryMoveItemToFirstCompatibleContentSlot(URpgInventoryItemInstance* Item);
	bool CanMoveItemOutOfGearSlot(const FRpgInventorySlotAddress& SourceAddress) const;
	void SyncEquipmentLoadoutFromGearSlots() const;
	void SyncActiveHandsFromCarrySlots() const;
	bool TryTransferManualDrop(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FGuid& RequestId);
	void ExecuteUseInventoryItem(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		int32 StackCount,
		const FGuid& RequestId);
	struct FRecentManualDropResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory;
		FRpgInventoryManualDropRequest Request;
		ERpgInventoryActionFeedbackResult Result =
			ERpgInventoryActionFeedbackResult::ServerRejected;
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
		int32 FeedbackStackCount);
	FTransform GetManualDropTransform() const;
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

	/** Server-local replay cache; transient results are bounded and never replicated or saved. */
	TMap<FGuid, FRecentManualDropResult> RecentManualDropResults;
	TArray<FGuid> RecentManualDropOrder;
	static constexpr int32 MaxRecentManualDropResults = 64;
};
