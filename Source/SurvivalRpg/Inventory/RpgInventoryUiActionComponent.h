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

/** Gameplay message broadcast on the owning client after inventory UI commands succeed, fail, or need confirmation. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryActionFeedbackMessage
{
	GENERATED_BODY()

	/** Semantic action that produced this feedback, such as Rpg.Inventory.Action.Drop. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	FGameplayTag ActionTag;

	/** Result code intended for UI sounds, toasts, confirmation modals, or slot flashes. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	ERpgInventoryActionFeedbackResult Result = ERpgInventoryActionFeedbackResult::ServerRejected;

	/** Inventory involved in the request, when relevant. UI should read this only as context. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	/** Item involved in the request, when relevant. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	TObjectPtr<URpgInventoryItemInstance> Item = nullptr;

	/** Requested or affected count. Zero means not count-specific. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Feedback")
	int32 StackCount = 0;
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

	/** Assigns an owned inventory item to an equipment slot such as MainHand, OffHand, Head, or Chest. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Clears one dedicated equipment slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Transfers a whole item entry or partial stack between two accessible inventories. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount);

	/** Transfers a stack into one exact target slot. Explicit drag/drop uses this instead of auto-stacking. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestTransferItemStackToInventorySlot(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex);

	/** Applies a shared server-side sort to an accessible inventory such as storage or loot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestApplyInventorySort(URpgInventoryManagerComponent* Inventory, ERpgInventorySortMode SortMode);

	/** Moves one accessible inventory entry to a shared replicated index for manual storage ordering. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveInventoryEntry(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetIndex);

	/** Moves, swaps, or stack-merges an accessible inventory entry into one exact slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveInventoryEntryToSlot(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetSlotIndex);

	/** Moves an owned player-inventory item into a logical player slot address such as WeaponSlot1[0] or Belt[2]. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestMoveItemToInventorySlotAddress(URpgInventoryItemInstance* Item, FRpgInventorySlotAddress TargetAddress);

	/** Assigns a bag, belt, pouch, or resource bag item to a slot-container equipment slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestEquipSlotContainerItem(ERpgEquipmentSlot ContainerSlot, URpgInventoryItemInstance* Item);

	/** Clears a bag, belt, pouch, or resource bag equipment slot if its provided slots are empty. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestUnequipSlotContainerItem(ERpgEquipmentSlot ContainerSlot);

	/** Activates a carry slot as MainHand or OffHand without moving the item out of the inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestActivateCarrySlot(FRpgInventorySlotAddress CarrySlotAddress);

	/** Clears the active MainHand/OffHand runtime state. Items remain in their inventory carry slots. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestClearActiveHands();

	/** Binds one 1..8 actionbar slot to a bindable non-carry inventory slot address. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestBindActionBarToInventorySlot(int32 ActionBarSlotIndex, FRpgInventorySlotAddress SlotAddress);

	/** Binds one 1..8 actionbar slot to a carry slot address. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestBindActionBarToCarrySlot(int32 ActionBarSlotIndex, FRpgInventorySlotAddress CarrySlotAddress);

	/** Splits one stack into a new stack in the same inventory. SplitCount <= 0 performs the V1 quick 50% split. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestSplitItemStack(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 SplitCount, int32 TargetSlotIndex);

	/** Uses a usable inventory item by granting and activating its configured one-shot ability. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestUseInventoryItem(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount = 1);

	/** Assigns an owned item to its default equipment destination, including MainHand, OffHand, and armor slots. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestEquipInventoryItem(URpgInventoryItemInstance* Item);

	/** Drops a stack or whole item entry into the world near the owning pawn. Confirmed must be true for confirm-protected items. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|UI Actions")
	void RequestDropInventoryItem(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount, bool bConfirmed);

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
	bool CanTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount) const;
	bool CanTransferItemStackToInventorySlot(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex) const;
	bool CanSplitItemStack(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 SplitCount, int32 TargetSlotIndex, int32& OutSplitCount, int32& OutTargetSlotIndex) const;
	bool FindFirstEmptyInventorySlot(URpgInventoryManagerComponent* Inventory, int32& OutSlotIndex) const;
	bool CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const;
	bool ClearPlayerAssignmentsForItem(URpgInventoryItemInstance* Item) const;
	bool TryAssignItemToDefaultEquipmentDestination(URpgInventoryItemInstance* Item);
	bool TryMoveItemToFirstCompatibleCarrySlot(URpgInventoryItemInstance* Item);
	bool TryMoveItemToFirstCompatibleContentSlot(URpgInventoryItemInstance* Item);
	bool CanMoveItemOutOfGearSlot(const FRpgInventorySlotAddress& SourceAddress) const;
	void SyncEquipmentLoadoutFromGearSlots() const;
	bool TrySpawnManualDrop(URpgInventoryItemInstance* Item, int32 StackCount, bool bDropAsInstance);
	bool TryMergeManualDrop(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, const FVector& SpawnLocation) const;
	FTransform GetManualDropTransform() const;
	void SendActionFeedback(FGameplayTag ActionTag, ERpgInventoryActionFeedbackResult Result, URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount) const;

	UFUNCTION(Client, Unreliable)
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
};
