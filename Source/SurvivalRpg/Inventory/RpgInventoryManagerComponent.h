// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttributeSet.h"
#include "Components/ActorComponent.h"
#include "Logging/LogMacros.h"
#include "Misc/Guid.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "RpgInventoryGraphTypes.h"

#include "RpgInventoryManagerComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRpgInventoryManager, Log, All);

class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutComponent;
class UAbilitySystemComponent;
class UObject;
struct FOnAttributeChangeData;
struct FFrame;
struct FInventoryPickup;
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

/** Server-authoritative sort modes for shared BaseStorage resource rows. */
UENUM(BlueprintType)
enum class ERpgInventorySortMode : uint8
{
	/** Preserve the current replicated resource-row SortIndex order, including manual row moves. */
	Manual,

	/** Sort resource rows alphabetically by item-definition display name. */
	Name,

	/** Sort resource rows by broad item category, then display name. */
	Category,

	/** Sort resource rows by stored count descending, then display name. */
	StackCount,

	/** Sort resource rows by SortIndex descending so the newest appended row appears first. */
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

	/** Stable replicated identity of this inventory entry, used to reject stale entry snapshots. */
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

/** Gameplay meaning that selects the non-configurable merge, swap, identity, and capacity rules for placement. */
enum class ERpgInventoryPlacementPurpose : uint8
{
	Move,
	Equip,
	Split,
	Add,
	Transfer,
	Restore
};

/** Whether placement evaluates one exact cell or searches the authoritative container order deterministically. */
enum class ERpgInventoryPlacementSearch : uint8
{
	Exact,
	FirstFit
};

/** Concrete outcome selected by the shared read-only placement evaluator. */
enum class ERpgInventoryPlacementResolution : uint8
{
	None,
	NoOp,
	Place,
	Merge,
	Swap
};

/** Factory-authored provenance that fixes identity and merge semantics for a placement subject. */
enum class ERpgInventoryPlacementSubjectKind : uint8
{
	Invalid,
	OwnedEntry,
	IncomingEntry,
	DefinitionGrant,
	GeneratedGrant,
	DetachedInstance,
	StagedRestore
};

/**
 * Trusted read-only subject consumed by the shared placement evaluator.
 *
 * Named factories keep owned-entry snapshots, incoming instances, definition grants, and staged restore rows distinct.
 * This transient C++ type is never accepted as an RPC payload or committed as client-authored authority.
 */
struct SURVIVALRPG_API FRpgInventoryPlacementSubject
{
	/** Builds a subject from one exact entry snapshot already owned by SourceInventory. */
	static FRpgInventoryPlacementSubject FromOwnedEntry(
		const URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryEntryView& Entry,
		int32 Quantity = 0);

	/** Builds a cross-inventory subject while retaining the complete authoritative source snapshot. */
	static FRpgInventoryPlacementSubject FromIncomingInstance(
		const URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryEntryView& Entry,
		int32 Quantity);

	/** Builds a definition-authored grant subject. Concrete runtime instances should be preferred before merging. */
	static FRpgInventoryPlacementSubject FromDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 Quantity);

	/** Builds a freshly initialized grant whose concrete default state may be compared before a merge. */
	static FRpgInventoryPlacementSubject FromGeneratedGrant(
		const URpgInventoryItemInstance* ItemInstance,
		int32 Quantity);

	/** Builds a subject from a concrete detached/bootstrap instance that is not owned by an inventory entry. */
	static FRpgInventoryPlacementSubject FromDetachedInstance(
		const URpgInventoryItemInstance* ItemInstance,
		int32 Quantity);

	/** Builds one exact row while a versioned graph restore is still staged and not observable. */
	static FRpgInventoryPlacementSubject FromStagedRestore(
		const URpgInventoryItemInstance* ItemInstance,
		FRpgInventoryItemId ItemId,
		int32 Quantity);

	/** Factory-authored provenance; callers select a named factory instead of configuring merge behavior. */
	ERpgInventoryPlacementSubjectKind Kind = ERpgInventoryPlacementSubjectKind::Invalid;

	/** Source inventory that owns the exact snapshot, or null for definition grants and staged restore rows. */
	const URpgInventoryManagerComponent* SourceInventory = nullptr;

	/** Concrete runtime item when identity or fragment-state stack compatibility matters. */
	const URpgInventoryItemInstance* ItemInstance = nullptr;

	/** Static definition used for footprint and rules; derived from ItemInstance by the named factories when possible. */
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Persistent concrete identity; invalid only for a definition grant that has not created its runtime instance yet. */
	FRpgInventoryItemId ItemId;

	/** Stable source entry identity captured with the read-only snapshot. */
	FGuid ExpectedEntryId;

	/** Complete source placement captured with the read-only snapshot. */
	FRpgInventoryGridPlacement ExpectedSourcePlacement;

	/** Complete source stack count captured independently from the amount being placed. */
	int32 ExpectedSourceQuantity = 0;

	/** Exact amount this evaluation must place or merge. */
	int32 Quantity = 0;
};

/** One exact or deterministic-first-fit request evaluated against replicated inventory state without mutation. */
struct SURVIVALRPG_API FRpgInventoryPlacementQuery
{
	/** Server-selected gameplay semantic; callers cannot toggle individual merge or swap rules. */
	ERpgInventoryPlacementPurpose Purpose = ERpgInventoryPlacementPurpose::Move;

	/** Exact evaluates one authored placement; FirstFit uses deterministic authoritative container/cell order. */
	ERpgInventoryPlacementSearch Search = ERpgInventoryPlacementSearch::Exact;

	/** Trusted source snapshot or staged incoming item. UObject pointers never cross an RPC boundary. */
	FRpgInventoryPlacementSubject Subject;

	/** Full root or item-owned destination. Invalid is supported only for global FirstFit content search. */
	FRpgInventoryContainerHandle TargetContainer;

	/** Requested top-left cell and orientation for Exact search; its full handle must match TargetContainer. */
	FRpgInventoryGridPlacement ExactPlacement;
};

/** One resolved action in a shared placement plan. */
struct SURVIVALRPG_API FRpgInventoryPlacementStep
{
	/** Derived gameplay action. Commit code consumes this value instead of re-deriving merge/swap policy. */
	ERpgInventoryPlacementResolution Resolution = ERpgInventoryPlacementResolution::None;

	/** Normalized authoritative destination footprint. */
	FRpgInventoryGridPlacement Placement;

	/** Number of units placed or merged by this step. */
	int32 Quantity = 0;

	/** Existing merge receiver, or the moving/placed concrete identity when it already exists. */
	FRpgInventoryItemId TargetItemId;

	/** Existing merge receiver or moving source entry id; invalid for a not-yet-created add/split row. */
	FGuid TargetEntryId;

	/** Concrete target displaced by Swap; invalid for Place, Merge, and NoOp. */
	FRpgInventoryItemId DisplacedItemId;

	/** Stable entry id displaced by Swap. */
	FGuid DisplacedEntryId;

	/** Fully normalized destination chosen for the displaced entry during Swap. */
	FRpgInventoryGridPlacement DisplacedPlacement;
};

/**
 * Complete side-effect-free placement decision shared by gameplay commits and UI preview.
 *
 * Revisions document which replicated graphs were read. They are diagnostic only; every authoritative gateway evaluates
 * the query again and never trusts a plan returned by a client.
 */
struct SURVIVALRPG_API FRpgInventoryPlacementPlan
{
	/** Stable rejection/success reason produced without mutating either inventory graph. */
	ERpgInventoryMutationResultCode Code = ERpgInventoryMutationResultCode::InvalidRequest;

	/** Source graph revision read while validating an owned subject, or INDEX_NONE for detached/definition subjects. */
	int32 SourceRevision = INDEX_NONE;

	/** Target graph revision read by this evaluator. Authority re-evaluates instead of trusting client plans. */
	int32 TargetRevision = INDEX_NONE;

	/** Exact quantity supplied by the immutable query. */
	int32 RequestedQuantity = 0;

	/** Quantity covered by Steps; a smaller value describes a partial fit that the purpose-specific commit may reject. */
	int32 AppliedQuantity = 0;

	/** Ordered deterministic actions; compatible stacks precede new placements for FirstFit. */
	TArray<FRpgInventoryPlacementStep> Steps;

	bool IsSuccess() const
	{
		return Code == ERpgInventoryMutationResultCode::Success ||
			Code == ERpgInventoryMutationResultCode::PartiallyApplied;
	}

	/** Returns true only when every requested unit is covered by at least one deterministic plan step. */
	bool IsCompleteSuccess() const
	{
		return Code == ERpgInventoryMutationResultCode::Success &&
			RequestedQuantity > 0 &&
			AppliedQuantity == RequestedQuantity &&
			!Steps.IsEmpty();
	}
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
	URpgInventoryItemInstance* GetItemAtCell(const FRpgInventoryContainerHandle& ContainerHandle, int32 X, int32 Y) const;
	bool GetPlacementForItem(URpgInventoryItemInstance* Instance, FRpgInventoryGridPlacement& OutPlacement) const;
	int32 GetStackCount(URpgInventoryItemInstance* Instance) const;
	int32 GetFreeStackCapacity(URpgInventoryItemInstance* Instance) const;
	int32 GetUsedEntryCount() const;
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

	bool AddEntry(URpgInventoryItemInstance* Instance, int32 StackCount = 1);
	bool AddEntryAtPlacement(URpgInventoryItemInstance* Instance, int32 StackCount, const FRpgInventoryGridPlacement& Placement);
	bool AddStackToEntry(URpgInventoryItemInstance* Instance, int32 StackCount);

	void RemoveEntry(URpgInventoryItemInstance* Instance);
	bool RemoveEntryStack(URpgInventoryItemInstance* Instance, int32 StackCount, bool& bOutRemovedEntry);
	bool MoveEntryToPlacement(
		FGuid EntryId,
		const FRpgInventoryGridPlacement& TargetPlacement,
		bool bAllowStackMerge = true);

private:
	FRpgInventoryEntry* FindEntryByInstance(URpgInventoryItemInstance* Instance);
	const FRpgInventoryEntry* FindEntryByInstance(URpgInventoryItemInstance* Instance) const;
	FRpgInventoryEntry* FindEntryByEntryId(FGuid EntryId);
	const FRpgInventoryEntry* FindEntryByEntryId(FGuid EntryId) const;
	FRpgInventoryEntry* FindEntryByItemId(const FRpgInventoryItemId& ItemId);
	const FRpgInventoryEntry* FindEntryByItemId(const FRpgInventoryItemId& ItemId) const;
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
	void RebaseDescendantContainerDepths(const FRpgInventoryItemId& AncestorItemId, int32 DepthDelta);
	/** Rejects raw insertion unless the instance belongs to this inventory actor and has unique runtime identity. */
	bool CanInsertOwnedInstance(const URpgInventoryItemInstance* Instance) const;

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
 * Sole server-authoritative owner of one replicated RPG inventory graph.
 *
 * The component owns FastArray storage, item-subobject replication, mutation
 * replay state, and persistence publication. Member implementations may be
 * organized into domain translation units, but those files are never
 * independent gameplay authorities. UI and ViewModels may observe this
 * component but never own or commit inventory state.
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

	/**
	 * Returns the authoritative stack limit used by inventory mutations and UI preflight.
	 * ItemContainer providers always resolve to one concrete instance per entry, even when
	 * legacy trait data advertises a larger stack.
	 */
	static int32 GetEffectiveMaxStackSizeForDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDef);

	/** Returns how many new entries this item definition would need after filling compatible existing stacks. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	int32 GetRequiredNewEntryCountForItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1) const;

	/** Returns how many new entries this concrete item instance would need. Instance moves do not merge. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity", BlueprintPure)
	int32 GetRequiredNewEntryCountForItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1) const;

	/**
	 * Evaluates one trusted placement query without mutating inventory, item, replication, or replay state.
	 * Gameplay commits must evaluate again on authority; UI may consume the returned normalized steps as preview only.
	 */
	FRpgInventoryPlacementPlan EvaluatePlacement(const FRpgInventoryPlacementQuery& Query) const;

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

	/** Returns true when the stack can be placed or merged into one exact grid placement. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Spatial")
	bool CanAddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement) const;

	/** Spatial-only preflight for an instance that remains owned by a source inventory until an atomic transfer commits. */
	bool CanReceiveTransferredItemInstance(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1) const;

	/** Place-only transfer preflight for one empty exact target placement; source ownership is intentionally preserved. */
	bool CanReceiveTransferredItemInstanceToPlacement(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement) const;

	/** Transfer/swap preflight after releasing exactly one known target item. */
	bool CanReceiveTransferredItemInstanceToPlacementIgnoringItem(URpgInventoryItemInstance* ItemInstance, int32 StackCount, FRpgInventoryGridPlacement Placement, URpgInventoryItemInstance* IgnoredItemInstance) const;

	/** Resolves the single item overlapped by this item's normalized footprint, or null for empty/multi-overlap targets. */
	URpgInventoryItemInstance* GetSingleItemOverlappingPlacementForItem(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement Placement, FRpgInventoryGridPlacement& OutNormalizedPlacement) const;

	/**
	 * Grants definition-authored items through the canonical authority-owned bootstrap path.
	 * The inventory creates all concrete instances under its owning actor and returns the first created or merged stack.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Intent")
	URpgInventoryItemInstance* GrantItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	/**
	 * Bootstraps a detached concrete item into this inventory.
	 * Foreign detached instances are cloned under the inventory actor with a fresh identity; inventory-owned instances
	 * must use an explicit cross-inventory transfer instead.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Intent")
	URpgInventoryItemInstance* BootstrapItemInstance(URpgInventoryItemInstance* SourceItemInstance, int32 StackCount = 1);

	/** Returns whether a detached concrete item can be bootstrapped without mutating either object. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Intent", BlueprintPure)
	bool CanBootstrapItemInstance(URpgInventoryItemInstance* SourceItemInstance, int32 StackCount = 1) const;

	/**
	 * Validates an all-or-nothing pickup payload against one shared stack, entry-budget, and spatial scratch graph.
	 * This is a server-local preflight for interaction abilities; the authoritative commit always plans again.
	 */
	bool CanAddPickupBatch(const FInventoryPickup& Pickup) const;

	/**
	 * Adds an all-or-nothing pickup payload as one server-authoritative inventory commit.
	 * Detached foreign instances are cloned under the inventory actor, runtime save/import is never used, and
	 * OutAffectedItemIds contains one representative result per payload row for optional post-commit equipment routing.
	 */
	FRpgInventoryMutationResult AddPickupBatch(
		const FInventoryPickup& Pickup,
		TArray<FRpgInventoryItemId>& OutAffectedItemIds);

	/** Trusted native compatibility/setup seam; gameplay grants should use GrantItemDefinition. */
	URpgInventoryItemInstance* AddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	/** Trusted native setup/testing seam that deterministically grants a definition at one exact placement. */
	URpgInventoryItemInstance* AddItemDefinitionToPlacement(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, FRpgInventoryGridPlacement Placement);

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<URpgInventoryItemInstance*> GetAllItems() const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<FRpgInventoryEntryView> GetAllEntries() const;

	/** Returns the item occupying a cell in an unambiguous root or item-owned container. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	URpgInventoryItemInstance* GetItemAtContainerCell(FRpgInventoryContainerHandle ContainerHandle, int32 X, int32 Y) const;

	/** Default grid id used by non-player inventories such as storage containers. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	FName GetDefaultContainerId() const { return DefaultContainerId; }

	/** Default grid size used by non-player inventories such as storage containers. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial", BlueprintPure)
	FRpgInventoryGridSize GetDefaultGridSize() const { return DefaultGridSize; }

	/**
	 * Expands the non-player root grid to at least MinimumSize without moving existing entries.
	 * Native and server-authoritative at runtime; intended for durable loot proxies that must not discard overflow.
	 */
	bool ExpandDefaultGridToMinimum(FRpgInventoryGridSize MinimumSize);

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

	/** Returns whether the exact quantity can be consumed without orphaning an item-owned container subtree. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Intent", BlueprintPure)
	bool CanConsumeItemById(FRpgInventoryItemId ItemId, int32 Quantity = 1) const;

	/**
	 * Consumes an exact quantity from one persistent item identity.
	 * Full container entries remove their complete descendant subtree atomically; partial container consumption fails closed.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Intent")
	FRpgInventoryMutationResult ConsumeItemById(FRpgInventoryItemId ItemId, int32 Quantity = 1);

	/**
	 * Atomically consumes ordinary stacks matching one definition.
	 * Container-provider definitions are never selected by this broad resource intent and require explicit item-id consumption.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Intent")
	bool ConsumeItemsByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 NumToConsume);

	/** Plans a stable whole-entry move from a complete replicated source snapshot without exposing kernel operations. */
	FRpgInventoryMutationResult PlanMoveItem(FRpgInventoryMoveIntent Intent) const;

	/** Moves one whole entry from its complete expected source snapshot to an exact target placement. */
	FRpgInventoryMutationResult MoveItem(FRpgInventoryMoveIntent Intent);

	/**
	 * Plans a trusted equipment placement that preserves the moving ItemId.
	 * Compatible occupied targets use the atomic swap path instead of merging stacks.
	 */
	FRpgInventoryMutationResult PlanEquipmentMove(
		FRpgInventoryMoveIntent Intent) const;

	/**
	 * Commits a trusted equipment placement while preserving both concrete item identities.
	 * This C++-only seam is selected by the validated server equipment gateway, never by an RPC payload.
	 */
	FRpgInventoryMutationResult MoveEquipmentItem(
		FRpgInventoryMoveIntent Intent);

	/** Transfers an exact stack amount or complete provider subtree from a complete source snapshot. */
	FRpgInventoryMutationResult TransferItem(
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryTransferIntent Intent);

	/** Collects an item into another authoritative inventory; only this intent may explicitly accept a partial stack. */
	FRpgInventoryMutationResult PickupItem(
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryTransferIntent Intent,
		bool bAllowPartialStack);

	/**
	 * Collects as many top-level source roots as fit into the ordered target containers using one shared scratch plan.
	 * Ordinary stacks may transfer partially; item-container providers and their descendants transfer only as a whole.
	 * RequestId identifies one immutable server-side command, and affected ids resolve the changed target root rows.
	 */
	FRpgInventoryMutationResult CollectRootItemsBatch(
		URpgInventoryManagerComponent* TargetInventory,
		const TArray<FRpgInventoryContainerHandle>& TargetContainers,
		FGuid RequestId,
		TArray<FRpgInventoryItemId>& OutAffectedTargetItemIds);

	/** Plans subtree-safe removal before a physical dropped actor is selected or spawned. */
	FRpgInventoryMutationResult PlanDropItem(FRpgInventoryTransferIntent Intent) const;

	/**
	 * Transfers an item into a durable drop inventory.
	 * Callers must create or select the physical world actor before invoking this C++-only gameplay seam.
	 */
	FRpgInventoryMutationResult DropItem(
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryTransferIntent Intent);

	/**
	 * Native planning kernel underlying the typed move, consume, split, transfer, pickup, and drop planners.
	 * Gameplay/UI callers should expose a narrow typed intent instead of accepting a raw mutation operation.
	 */
	FRpgInventoryMutationResult PlanInventoryMutation(FRpgInventoryMutationRequest Request) const;

	/** Authority-only native commit kernel underlying typed inventory commands. */
	FRpgInventoryMutationResult ExecuteInventoryMutation(FRpgInventoryMutationRequest Request);

	/** Authority-only native cross-inventory kernel underlying TransferItem, PickupItem, and DropItem. */
	FRpgInventoryMutationResult ExecuteCrossInventoryTransfer(
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryMutationRequest Request,
		bool bAllowPartialStackPickup = false);

	/** Exports the complete flattened inventory graph for validated versioned disk persistence. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Persistence")
	FRpgInventoryGraphSaveData ExportInventoryGraph() const;

	/**
	 * Restores a complete graph after full validation and atomically replaces runtime inventory state.
	 * Existing items with matching persistent ids and definitions retain their runtime instance and entry identity.
	 */
	bool RestoreInventoryGraph(const FRpgInventoryGraphSaveData& SaveData, FRpgInventoryMutationResult& OutResult);

	/**
	 * Returns the server-local command epoch used to scope idempotent inventory requests.
	 * Only a successful profile/disk restore advances this value; runtime transaction imports stay in the current epoch.
	 */
	uint64 GetMutationEpoch() const { return MutationEpoch; }

	/** Returns the replicated state revision; authoritative commits advance it, while cached command replays do not. */
	int32 GetInventoryRevision() const { return InventoryRevision; }

	/**
	 * Restores a previously exported runtime checkpoint without starting a new command epoch.
	 * This native-only seam is reserved for rollback inside one authoritative transaction; disk/profile loads use
	 * RestoreInventoryGraph so stale request replays cannot cross the persistence boundary.
	 */
	bool RestoreRuntimeCheckpoint(
		const FRpgInventoryGraphSaveData& SaveData,
		FRpgInventoryMutationResult& OutResult);

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
	bool ShouldUseSingleCellPlacementForContainer(const FRpgInventoryContainerHandle& ContainerHandle) const;
	bool GetItemContainerDefinition(const FRpgInventoryContainerHandle& ContainerHandle, struct FRpgInventoryItemContainerDefinition& OutDefinition) const;
	bool ValidatePlacementGraphRules(const FRpgInventoryEntry& Entry, const FRpgInventoryGridPlacement& Placement, ERpgInventoryMutationResultCode& OutCode) const;
	bool WouldCreateContainerCycle(const FRpgInventoryItemId& MovingItemId, const FRpgInventoryContainerHandle& TargetContainer) const;
	bool AddOwnedItemInstance(
		URpgInventoryItemInstance* ItemInstance,
		int32 StackCount,
		const FRpgInventoryGridPlacement* Placement = nullptr,
		URpgInventoryItemInstance** OutResultInstance = nullptr);
	URpgInventoryItemInstance* CommitAddPlacementPlan(
		URpgInventoryItemInstance* StagedInstance,
		const FRpgInventoryPlacementPlan& Plan,
		bool bMayCreateAdditionalInstances);
	struct FPreparedPickupBatch;
	bool PreparePickupBatch(
		const FInventoryPickup& Pickup,
		UObject* StagingOuter,
		FPreparedPickupBatch& OutPrepared,
		ERpgInventoryMutationResultCode& OutCode) const;
	bool RevalidatePickupBatch(
		const FPreparedPickupBatch& Prepared,
		ERpgInventoryMutationResultCode& OutCode) const;
	struct FPreparedCollectBatch;
	bool PrepareCollectRootItemsBatch(
		URpgInventoryManagerComponent* TargetInventory,
		const TArray<FRpgInventoryContainerHandle>& TargetContainers,
		FGuid RequestId,
		FPreparedCollectBatch& OutPrepared,
		ERpgInventoryMutationResultCode& OutCode);
	bool RevalidateCollectRootItemsBatch(
		const FPreparedCollectBatch& Prepared,
		ERpgInventoryMutationResultCode& OutCode) const;
	FRpgInventoryMutationResult CommitCollectRootItemsBatch(
		FPreparedCollectBatch& Prepared,
		TArray<FRpgInventoryItemId>& OutAffectedTargetItemIds);
	/** Shared evaluator seam used by staged restore after its batch-local owner/rule resolution. */
	FRpgInventoryPlacementPlan EvaluatePlacementInternal(
		const FRpgInventoryPlacementQuery& Query,
		const FRpgInventoryGridSize* StagedRestoreGridSize,
		const TArray<FRpgInventoryGridPlacement>* StagedRestoreOccupancy) const;
	bool TryNormalizePlacementForDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDef,
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y,
		bool bRotated,
		FRpgInventoryGridPlacement& OutPlacement) const;
	bool IsItemManagedByAnyInventory(const URpgInventoryItemInstance* ItemInstance) const;
	bool HasItemIdentityConflictInAnyInventory(const URpgInventoryItemInstance* ItemInstance) const;
	/** Canonical read-only index produced only after every row in one complete inventory graph passed validation. */
	struct FValidatedInventoryGraph
	{
		TMap<FRpgInventoryItemId, int32> IndexByItemId;
		TMap<FGuid, int32> IndexByEntryId;
		TArray<int32> ParentIndexByEntry;
		TArray<int32> RootIndexByEntry;
		TArray<uint8> DeepestRelativeDepthByEntry;

		/** Appends the root and every transitive descendant in stable source-array order. */
		bool GatherSubtreeIndices(int32 RootIndex, TArray<int32>& OutIndices) const;
	};
	/**
	 * Validates one complete current or prospective graph without mutating gameplay state.
	 *
	 * Capacity is enforced for imports and target projections. Source projections may opt out so a graph whose capacity
	 * was reduced at runtime can still shrink through egress. Entry identity remains inventory-local; item identity,
	 * ancestry, provider rules, canonical footprints, bounds, and occupancy are validated for every row.
	 */
	bool ValidateInventoryGraph(
		const TArray<FRpgInventoryEntry>& Entries,
		const UObject* ExpectedInstanceOuter,
		bool bEnforceCapacity,
		FValidatedInventoryGraph& OutGraph,
		ERpgInventoryMutationResultCode& OutCode) const;
	/** Applies the canonical graph contract to the component's current replicated FastArray. */
	bool ValidateLiveInventoryGraph(
		bool bEnforceCapacity,
		FValidatedInventoryGraph& OutGraph,
		ERpgInventoryMutationResultCode& OutCode) const;
	/** True while a multi-step authoritative operation must reject nested inventory mutation. */
	bool IsInventoryMutationLocked() const;
	bool TryBuildRemovalDeltas(
		const FRpgInventoryEntry& RootEntry,
		int32 Quantity,
		TArray<FRpgInventoryMutationDelta>& OutDeltas,
		ERpgInventoryMutationResultCode& OutCode) const;
	bool CommitRemovalDeltas(const TArray<FRpgInventoryMutationDelta>& Deltas);
	FRpgInventoryMutationRequest BuildMoveMutationRequest(const FRpgInventoryMoveIntent& Intent) const;
	FRpgInventoryMutationRequest BuildEquipmentMoveMutationRequest(
		const FRpgInventoryMoveIntent& Intent) const;
	static FRpgInventoryMutationRequest BuildTransferMutationRequest(
		const FRpgInventoryTransferIntent& Intent,
		ERpgInventoryMutationOperation Operation);
	FRpgInventoryMutationResult ExecuteTransferIntent(
		URpgInventoryManagerComponent* TargetInventory,
		FRpgInventoryTransferIntent Intent,
		ERpgInventoryMutationOperation Operation,
		bool bAllowPartialStack);
	bool RestoreInventoryGraphInternal(
		const FRpgInventoryGraphSaveData& SaveData,
		FRpgInventoryMutationResult& OutResult,
		bool bEstablishNewMutationEpoch);
	struct FRecentMutationRecord
	{
		enum class EKind : uint8
		{
			SingleMutation,
			CollectRootBatch
		};

		EKind Kind = EKind::SingleMutation;
		FRpgInventoryMutationRequest Request;
		TArray<FRpgInventoryContainerHandle> TargetContainers;
		TArray<FRpgInventoryItemId> AffectedTargetItemIds;
		TWeakObjectPtr<URpgInventoryManagerComponent> TargetInventory;
		bool bHadTargetInventory = false;
		bool bAllowPartialStack = false;
		uint64 SourceMutationEpoch = 0;
		uint64 TargetMutationEpoch = 0;
		FRpgInventoryMutationResult Result;
	};
	static bool AreMutationRequestsEquivalent(
		const FRpgInventoryMutationRequest& A,
		const FRpgInventoryMutationRequest& B);
	bool TryReplayRecentMutation(
		const FRpgInventoryMutationRequest& Request,
		URpgInventoryManagerComponent* TargetInventory,
		bool bAllowPartialStack,
		FRpgInventoryMutationResult& OutResult);
	FRpgInventoryMutationResult CacheRecentMutationResult(
		const FRpgInventoryMutationRequest& Request,
		URpgInventoryManagerComponent* TargetInventory,
		bool bAllowPartialStack,
		FRpgInventoryMutationResult Result);
	bool TryReplayRecentCollectRootBatch(
		FGuid RequestId,
		URpgInventoryManagerComponent* TargetInventory,
		const TArray<FRpgInventoryContainerHandle>& TargetContainers,
		FRpgInventoryMutationResult& OutResult,
		TArray<FRpgInventoryItemId>& OutAffectedTargetItemIds);
	FRpgInventoryMutationResult CacheRecentCollectRootBatchResult(
		URpgInventoryManagerComponent* TargetInventory,
		const TArray<FRpgInventoryContainerHandle>& TargetContainers,
		FRpgInventoryMutationResult Result,
		const TArray<FRpgInventoryItemId>& AffectedTargetItemIds);

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
	TMap<FGuid, FRecentMutationRecord> RecentMutationResults;
	TArray<FGuid> RecentMutationOrder;

	/** Server-local generation that invalidates command replay state after a successful profile/disk restore. */
	uint64 MutationEpoch = 0;

	/** Prevents one synchronous pickup notification from committing the same non-RPC batch reentrantly. */
	bool bIsApplyingPickupBatch = false;

	/** Prevents fragment initialization/copy hooks from recursively planning or applying another pickup batch. */
	mutable bool bIsPlanningPickupBatch = false;

	/** Prevents a multi-root collect from being reentered through fragment hooks or synchronous inventory listeners. */
	bool bIsApplyingCollectBatch = false;

	/** Prevents fragment hooks and listeners from reentering either side of a cross-inventory transfer. */
	bool bIsApplyingCrossInventoryTransfer = false;

	/** Prevents fragment restore hooks and synchronous load notifications from reentering an authoritative mutation. */
	bool bIsRestoringInventoryGraph = false;
};
