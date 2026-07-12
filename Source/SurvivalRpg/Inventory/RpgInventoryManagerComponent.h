// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttributeSet.h"
#include "Components/ActorComponent.h"
#include "Misc/Guid.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "RpgInventorySpatialTypes.h"

#include "RpgInventoryManagerComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutComponent;
class UAbilitySystemComponent;
class UObject;
struct FOnAttributeChangeData;
struct FFrame;
struct FRpgInventoryList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;
class FCustomPropertyConditionState;

/** Connection audience for the replicated inventory graph and item subobjects. */
UENUM(BlueprintType)
enum class ERpgInventoryReplicationPolicy : uint8
{
	/** Private player inventory: only the owning connection receives entries and item state. */
	OwnerOnly,

	/** World storage/loot: every connection for which the owning actor is relevant receives the graph. */
	ActorRelevant
};

/** Capacity source used by an inventory manager when accepting new entries. */
UENUM(BlueprintType)
enum class ERpgInventoryCapacityMode : uint8
{
	/** No entry limit. Useful for loot proxies, debug containers, or temporary piles. */
	Unlimited,

	/** Use FixedMaxEntries as the authoritative entry limit. */
	FixedEntries,

	/** Read the entry limit from a GAS attribute on the owning actor's ASC, falling back to FixedMaxEntries. */
	AbilitySystemAttribute
};

/** Server-authoritative sort modes that can rewrite shared inventory order. */
UENUM(BlueprintType)
enum class ERpgInventorySortMode : uint8
{
	/** Preserve the current replicated grid placement order, including manual moves. */
	Manual,

	/** Sort alphabetically by item display name. */
	Name,

	/** Sort by broad item category, then display name. */
	Category,

	/** Sort by stack count descending, then display name. */
	StackCount,

	/** Sort by current server order descending so the newest appended entries appear first. */
	Recent
};

/** Read-only inventory row exposed to UI and server-side transfer systems. */
USTRUCT(BlueprintType)
struct FRpgInventoryEntryView
{
	GENERATED_BODY()

	/** Inventory component that owns this entry. UI should treat it as read-only source context. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	/** Concrete replicated item instance represented by this entry. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TObjectPtr<URpgInventoryItemInstance> Instance = nullptr;

	/** Stable replicated id for this entry, used by shared UI sorting and future save snapshots. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	FGuid EntryId;

	/** Persistent identity of the concrete item, independent of this inventory entry. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	FRpgInventoryItemId ItemId;

	/** Authoritative replicated stack count for this item entry. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	int32 StackCount = 0;

	/** Server-authored spatial placement for this inventory entry. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	FRpgInventoryGridPlacement Placement;
};

/** One save-ready inventory row used for session export/import and future world container saves. */
USTRUCT(BlueprintType)
struct FRpgInventorySnapshotEntry
{
	GENERATED_BODY()

	/** Stable entry id preserved when restoring a saved container order. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	FGuid EntryId;

	/** Persistent concrete item identity preserved across containers and disk restore. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Inventory|Snapshot")
	FRpgInventoryItemId ItemId;

	/** Static item definition to recreate for this saved stack. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Saved stack count for this entry. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	int32 StackCount = 0;

	/** Saved spatial placement for this entry. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	FRpgInventoryGridPlacement Placement;
};

/** Save-ready inventory snapshot for a player inventory or world container. */
USTRUCT(BlueprintType)
struct FRpgInventorySnapshot
{
	GENERATED_BODY()

	/** Stable id for the container that owns these entries. Player inventories may leave this None. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	FName ContainerId;

	/** Saved item entries in shared order. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	TArray<FRpgInventorySnapshotEntry> Entries;
};

/** A message when an item is added to the inventory */
USTRUCT(BlueprintType)
struct FRpgInventoryChangeMessage
{
	GENERATED_BODY()

	//@TODO: Tag based names+owning actors for inventories instead of directly exposing the component?
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TObjectPtr<URpgInventoryItemInstance> Instance = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	FGuid EntryId;

	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 NewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 Delta = 0;

	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	FRpgInventoryGridPlacement Placement;

	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	bool bOrderChanged = false;

	/** True when the inventory refreshed because capacity changed rather than an item stack changing. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	bool bCapacityChanged = false;
};

/** A single entry in an inventory */
USTRUCT(BlueprintType)
struct FRpgInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FRpgInventoryEntry()
	{}

	FString GetDebugString() const;

private:
	friend FRpgInventoryList;
	friend URpgInventoryManagerComponent;

	UPROPERTY()
	TObjectPtr<URpgInventoryItemInstance> Instance = nullptr;

	UPROPERTY()
	FGuid EntryId;

	UPROPERTY()
	int32 StackCount = 0;

	UPROPERTY()
	FRpgInventoryGridPlacement Placement;

	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;
};

/** List of inventory items */
USTRUCT(BlueprintType)
struct FRpgInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRpgInventoryList()
		: OwnerComponent(nullptr)
	{
	}

	FRpgInventoryList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	TArray<URpgInventoryItemInstance*> GetAllItems() const;
	TArray<FRpgInventoryEntryView> GetAllEntries() const;
	URpgInventoryItemInstance* GetItemAtCell(FName ContainerId, int32 X, int32 Y) const;
	URpgInventoryItemInstance* GetItemAtCell(const FRpgInventoryContainerHandle& ContainerHandle, int32 X, int32 Y) const;
	bool GetPlacementForItem(URpgInventoryItemInstance* Instance, FRpgInventoryGridPlacement& OutPlacement) const;
	int32 GetStackCount(URpgInventoryItemInstance* Instance) const;
	int32 GetFreeStackCapacity(URpgInventoryItemInstance* Instance) const;
	int32 GetUsedEntryCount() const;
	int32 GetRequiredNewEntryCount(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const;
	/** Simulates every stack merge and spatial allocation without mutating the replicated list. */
	bool CanFullyAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const;
	bool ContainsItemInstance(URpgInventoryItemInstance* Instance) const;
	bool ContainsEntry(FGuid EntryId) const;

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgInventoryEntry, FRpgInventoryList>(Entries, DeltaParms, *this);
	}

	URpgInventoryItemInstance* AddEntry(TSubclassOf<URpgInventoryItemDefinition> ItemClass, int32 StackCount, TArray<URpgInventoryItemInstance*>& OutNewInstances);
	URpgInventoryItemInstance* AddEntryAtPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemClass, int32 StackCount, const FRpgInventoryGridPlacement& Placement, TArray<URpgInventoryItemInstance*>& OutNewInstances);
	void AddEntry(URpgInventoryItemInstance* Instance, int32 StackCount = 1);
	void AddEntryAtPlacement(URpgInventoryItemInstance* Instance, int32 StackCount, const FRpgInventoryGridPlacement& Placement);
	bool AddStackToEntry(URpgInventoryItemInstance* Instance, int32 StackCount);

	void RemoveEntry(URpgInventoryItemInstance* Instance);
	bool RemoveEntryStack(URpgInventoryItemInstance* Instance, int32 StackCount, bool& bOutRemovedEntry);
	bool ApplySort(ERpgInventorySortMode SortMode, FRpgInventoryContainerHandle ContainerFilter = FRpgInventoryContainerHandle());
	bool MoveEntry(FGuid EntryId, int32 TargetIndex);
	bool CanMoveEntryToPlacement(FGuid EntryId, const FRpgInventoryGridPlacement& TargetPlacement, FRpgInventoryGridPlacement* OutNormalizedTargetPlacement = nullptr) const;
	bool MoveEntryToPlacement(FGuid EntryId, const FRpgInventoryGridPlacement& TargetPlacement);
	FRpgInventorySnapshot ExportSnapshot(FName ContainerId) const;
	void ImportSnapshot(const FRpgInventorySnapshot& Snapshot);

private:
	FRpgInventoryEntry* FindEntryByInstance(URpgInventoryItemInstance* Instance);
	const FRpgInventoryEntry* FindEntryByInstance(URpgInventoryItemInstance* Instance) const;
	FRpgInventoryEntry* FindEntryByEntryId(FGuid EntryId);
	const FRpgInventoryEntry* FindEntryByEntryId(FGuid EntryId) const;
	FRpgInventoryEntry* FindEntryByItemId(const FRpgInventoryItemId& ItemId);
	const FRpgInventoryEntry* FindEntryByItemId(const FRpgInventoryItemId& ItemId) const;
	FRpgInventoryEntry* FindEntryAtCell(FName ContainerId, int32 X, int32 Y);
	const FRpgInventoryEntry* FindEntryAtCell(FName ContainerId, int32 X, int32 Y) const;
	FRpgInventoryEntry* FindEntryOverlapping(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry = nullptr);
	const FRpgInventoryEntry* FindEntryOverlapping(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry = nullptr) const;
	void FindEntriesOverlapping(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry, TArray<const FRpgInventoryEntry*>& OutEntries) const;
	bool IsPlacementWithinGrid(const FRpgInventoryGridPlacement& Placement) const;
	bool CanPlaceEntryAt(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntry = nullptr) const;
	bool CanPlaceEntryAt(const FRpgInventoryGridPlacement& Placement, const FRpgInventoryEntry* IgnoredEntryA, const FRpgInventoryEntry* IgnoredEntryB) const;
	bool CanEntryUsePlacement(const FRpgInventoryEntry& Entry, const FRpgInventoryGridPlacement& Placement) const;
	bool NormalizePlacementForEntry(const FRpgInventoryEntry& Entry, const FRpgInventoryGridPlacement& TargetPlacement, FRpgInventoryGridPlacement& OutNormalizedPlacement) const;
	/** Finds a collision-free destination for an entry displaced by a size-asymmetric spatial swap. */
	bool TryResolveDisplacedEntryPlacement(
		const FRpgInventoryEntry& MovingEntry,
		const FRpgInventoryGridPlacement& MovingTargetPlacement,
		const FRpgInventoryEntry& DisplacedEntry,
		FRpgInventoryGridPlacement& OutDisplacedPlacement) const;
	void BroadcastChangeMessage(FRpgInventoryEntry& Entry, int32 OldCount, int32 NewCount, bool bOrderChanged = false);
	bool FindFirstFitPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, FRpgInventoryGridPlacement& OutPlacement) const;
	bool FindFirstFitPlacement(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement& OutPlacement) const;
	bool FindFirstFitPlacement(
		TSubclassOf<URpgInventoryItemDefinition> ItemDef,
		FRpgInventoryGridPlacement& OutPlacement,
		const TArray<FRpgInventoryGridPlacement>& AdditionalOccupancy) const;
	bool FindFirstFitPlacementInContainer(
		TSubclassOf<URpgInventoryItemDefinition> ItemDef,
		const FRpgInventoryContainerHandle& ContainerHandle,
		const TArray<FRpgInventoryGridPlacement>& ScratchOccupancy,
		FRpgInventoryGridPlacement& OutPlacement) const;
	int32 GetLinearOrder(const FRpgInventoryGridPlacement& Placement) const;
	void SortEntriesByPlacement();
	bool SetOrderFromSortedEntryPointers(const TArray<FRpgInventoryEntry*>& SortedEntries);
	void RebaseDescendantContainerDepths(const FRpgInventoryItemId& AncestorItemId, int32 DepthDelta);

private:
	friend URpgInventoryManagerComponent;

private:
	// Replicated list of items
	UPROPERTY()
	TArray<FRpgInventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FRpgInventoryList> : public TStructOpsTypeTraitsBase2<FRpgInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};










/**
 * Manages an inventory
 */
UCLASS(BlueprintType)
class URpgInventoryManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Sets the static replication audience. Configure during actor construction before replication begins. */
	void SetReplicationPolicy(ERpgInventoryReplicationPolicy NewPolicy) { ReplicationPolicy = NewPolicy; }

	/** Returns true when this inventory is not limited by entry count. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	bool IsCapacityUnlimited() const;

	/** Returns the max entry count, or INDEX_NONE when capacity is unlimited. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	int32 GetMaxEntries() const;

	/** Returns the number of currently occupied inventory entries. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	int32 GetUsedEntryCount() const;

	/** Returns free entries, or INDEX_NONE when capacity is unlimited. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	int32 GetFreeEntryCount() const;

	/** Returns how many new entries this item definition would need after filling compatible existing stacks. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	int32 GetRequiredNewEntryCountForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1) const;

	/** Returns how many new entries this concrete item instance would need. Instance moves do not merge. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	int32 GetRequiredNewEntryCountForItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1) const;

	/** Sets the capacity mode. Server-authoritative for runtime changes; normally configured on archetypes. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Capacity")
	void SetCapacityMode(ERpgInventoryCapacityMode NewCapacityMode);

	/** Sets the fixed fallback/max entry count. Zero means no entries when FixedEntries is active. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Capacity")
	void SetFixedMaxEntries(int32 NewFixedMaxEntries);

	/** Sets the GAS attribute used when CapacityMode is AbilitySystemAttribute. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Capacity")
	void SetCapacityAttribute(FGameplayAttribute NewCapacityAttribute);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool CanAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool CanAddItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1) const;

	/** Returns true when the stack can be placed or merged into one exact grid placement. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Spatial")
	bool CanAddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement) const;

	/** Returns true when this concrete item instance can be moved into one exact grid placement. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Spatial")
	bool CanAddItemInstanceToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement) const;

	/** Returns true when an instance would fit into a placement after removing exactly one known swap counterpart. */
	bool CanAddItemInstanceToPlacementIgnoringItem(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement, URpgInventoryItemInstance* IgnoredItemInstance) const;

	/** Resolves the single item overlapped by this item's normalized footprint, or null for empty/multi-overlap targets. */
	URpgInventoryItemInstance* GetSingleItemOverlappingPlacementForItem(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement Placement, FRpgInventoryGridPlacement& OutNormalizedPlacement) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	URpgInventoryItemInstance* AddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	/** Adds a definition-created stack to an exact grid placement instead of auto-placing into the inventory. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Spatial")
	URpgInventoryItemInstance* AddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddItemInstance(URpgInventoryItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddItemInstanceWithStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1);

	/** Adds an existing item instance to an exact grid placement, preserving runtime instance data such as durability. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Spatial")
	void AddItemInstanceWithStackToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement);

	/** Adds stack count to an existing stack entry without creating or moving an entry. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Slots")
	bool AddStackToExistingItem(URpgInventoryItemInstance* ItemInstance, int32 StackCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void RemoveItemInstance(URpgInventoryItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool RemoveItemInstanceStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<URpgInventoryItemInstance*> GetAllItems() const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<FRpgInventoryEntryView> GetAllEntries() const;

	/** Returns the item occupying one replicated grid cell, or nullptr for empty/invalid cells. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	URpgInventoryItemInstance* GetItemAtCell(FName ContainerId, int32 X, int32 Y) const;

	/** Returns the item occupying a cell in an unambiguous root or item-owned container. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	URpgInventoryItemInstance* GetItemAtContainerCell(FRpgInventoryContainerHandle ContainerHandle, int32 X, int32 Y) const;

	/** Default grid id used by non-player inventories such as storage containers. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	FName GetDefaultContainerId() const { return DefaultContainerId; }

	/** Default grid size used by non-player inventories such as storage containers. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	FRpgInventoryGridSize GetDefaultGridSize() const { return DefaultGridSize; }

	/** Resolves the grid dimensions for a player layout container or this inventory's default storage container. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	bool GetGridSizeForContainer(FName ContainerId, FRpgInventoryGridSize& OutGridSize) const;

	/** Resolves grid dimensions for a root or concrete item-owned container. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	bool GetGridSizeForContainerHandle(FRpgInventoryContainerHandle ContainerHandle, FRpgInventoryGridSize& OutGridSize) const;

	/** Returns the replicated spatial placement of an owned item entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	bool GetItemPlacement(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement& OutPlacement) const;

	/** Returns how many units can still merge into this stack before reaching its definition max stack size. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slots", BlueprintPure)
	int32 GetFreeStackCapacity(URpgInventoryItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	bool ContainsEntry(FGuid EntryId) const;

	/** Resolves a concrete item by its persistent identity. */
	UFUNCTION(BlueprintCallable, Category = Inventory, BlueprintPure)
	URpgInventoryItemInstance* FindItemById(FRpgInventoryItemId ItemId) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	URpgInventoryItemInstance* FindFirstItemStackByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	bool ContainsItemInstance(URpgInventoryItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	int32 GetItemStackCount(URpgInventoryItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	int32 GetTotalItemCountByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool ConsumeItemsByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 NumToConsume);

	/** Rewrites shared replicated order for this inventory on the server. UI should request this through the owning controller component. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Sorting")
	bool ApplyInventorySort(ERpgInventorySortMode SortMode);

	/** Moves one replicated entry to a target shared index on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Sorting")
	bool MoveInventoryEntry(FGuid EntryId, int32 TargetIndex);

	/** Moves, swaps, or stack-merges one entry into an exact replicated grid placement on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Spatial")
	bool MoveInventoryEntryToPlacement(FGuid EntryId, FRpgInventoryGridPlacement TargetPlacement);

	/** Returns whether a replicated entry can move, merge, or swap into an exact grid placement using server rules. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	bool CanMoveInventoryEntryToPlacement(FGuid EntryId, FRpgInventoryGridPlacement TargetPlacement) const;

	/** Exports a save-ready snapshot containing item definitions, stack counts, entry ids, and shared order. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Snapshot")
	FRpgInventorySnapshot ExportInventorySnapshot(FName ContainerId) const;

	/** Replaces this inventory with a save-ready snapshot. Server-authoritative and intended for future world-save restore paths. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Snapshot")
	void ImportInventorySnapshot(const FRpgInventorySnapshot& Snapshot);

	/** Simulates one item-id based mutation using the same validation rules as the authoritative commit. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Transaction", BlueprintPure = false)
	FRpgInventoryMutationResult PlanInventoryMutation(FRpgInventoryMutationRequest Request) const;

	/** Sole public authoritative item-id mutation path used by migrated UI and gameplay systems. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Transaction")
	FRpgInventoryMutationResult ExecuteInventoryMutation(FRpgInventoryMutationRequest Request);

	/** Atomically transfers a stack or complete item-owned subtree between two authoritative inventories. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Transaction")
	FRpgInventoryMutationResult ExecuteCrossInventoryTransfer(
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryMutationRequest Request,
		bool bAllowPartialStackPickup = false);

	/** Exports the complete flattened inventory graph for validated versioned disk persistence. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Persistence")
	FRpgInventoryGraphSaveData ExportInventoryGraph() const;

	/** Validates the complete graph before atomically replacing runtime inventory state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Persistence")
	bool ImportInventoryGraph(const FRpgInventoryGraphSaveData& SaveData, FRpgInventoryMutationResult& OutResult);

	//~UObject interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;
	virtual void GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const override;
	//~End of UObject interface

private:
	friend FRpgInventoryList;

	UFUNCTION()
	void OnRep_InventoryRevision();

	UFUNCTION()
	void OnRep_CapacitySettings();

	void MarkInventoryStateDirty();
	void BroadcastInventoryStateChanged() const;
	UAbilitySystemComponent* FindCapacityAbilitySystem() const;
	void RefreshCapacityAttributeBinding();
	void ClearCapacityAttributeBinding();
	void HandleCapacityAttributeChanged(const FOnAttributeChangeData& Data);
	void BroadcastCapacityChanged() const;
	const URpgPlayerInventoryLayoutComponent* FindOwningPlayerInventoryLayout() const;
	bool ShouldUseSingleCellPlacementForContainer(FName ContainerId) const;
	bool GetItemContainerDefinition(const FRpgInventoryContainerHandle& ContainerHandle, struct FRpgInventoryItemContainerDefinition& OutDefinition) const;
	bool ValidatePlacementGraphRules(const FRpgInventoryEntry& Entry, const FRpgInventoryGridPlacement& Placement, ERpgInventoryMutationResultCode& OutCode) const;
	bool WouldCreateContainerCycle(const FRpgInventoryItemId& MovingItemId, const FRpgInventoryContainerHandle& TargetContainer) const;
	bool TryMakePlacementForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, FName ContainerId, int32 X, int32 Y, bool bRotated, FRpgInventoryGridPlacement& OutPlacement) const;
	bool TryMakePlacementForItemInstance(URpgInventoryItemInstance* ItemInstance, FName ContainerId, int32 X, int32 Y, bool bRotated, FRpgInventoryGridPlacement& OutPlacement) const;
	FRpgInventoryGridPlacement MakePlacementForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, FName ContainerId, int32 X, int32 Y, bool bRotated) const;
	FRpgInventoryGridPlacement MakePlacementForItemInstance(URpgInventoryItemInstance* ItemInstance, FName ContainerId, int32 X, int32 Y, bool bRotated) const;

private:
	/** Source used to determine how many entries this inventory may hold. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_CapacitySettings, BlueprintReadOnly, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryCapacityMode CapacityMode = ERpgInventoryCapacityMode::Unlimited;

	/** Fixed entry limit and fallback when the configured GAS attribute is unavailable. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_CapacitySettings, BlueprintReadOnly, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 FixedMaxEntries = 0;

	/** GAS attribute used as entry capacity when CapacityMode is AbilitySystemAttribute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	FGameplayAttribute CapacityAttribute;

	/** Default grid dimensions for non-player inventories such as world storage, loot, and crafting outputs. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_CapacitySettings, BlueprintReadOnly, Category = "Inventory|Spatial", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryGridSize DefaultGridSize;

	/** Stable grid id for non-player inventories. Player inventory grid ids come from URpgPlayerInventoryLayoutComponent. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_CapacitySettings, BlueprintReadOnly, Category = "Inventory|Spatial", meta = (AllowPrivateAccess = "true"))
	FName DefaultContainerId = TEXT("Storage");

	/** Static audience policy; private player graphs are owner-only while world containers follow actor relevancy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Replication", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryReplicationPolicy ReplicationPolicy = ERpgInventoryReplicationPolicy::ActorRelevant;

	UPROPERTY(Replicated)
	FRpgInventoryList InventoryList;

	/** Lightweight replicated pulse used to wake already-open UI panels after any server inventory mutation. */
	UPROPERTY(ReplicatedUsing = OnRep_InventoryRevision)
	int32 InventoryRevision = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundCapacityAbilitySystem;
	FDelegateHandle CapacityAttributeChangedHandle;

	/** Short authority-side idempotency cache for retried reliable transaction requests. */
	TMap<FGuid, FRpgInventoryMutationResult> RecentMutationResults;
	TArray<FGuid> RecentMutationOrder;
};
