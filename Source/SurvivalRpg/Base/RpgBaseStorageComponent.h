#pragma once

#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include "RpgBaseStorageComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
struct FNetDeltaSerializeInfo;

/** Resource capacity contribution for one material definition. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceCapacity
{
	GENERATED_BODY()

	/** Material item definition this capacity applies to. Static designer data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of units this base or station can hold for the item definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage", meta = (ClampMin = "0", UIMin = "0"))
	int32 Capacity = 0;
};

/** Read-only resource row exposed to UI and crafting. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceEntryView
{
	GENERATED_BODY()

	/** Material item definition represented by this resource row. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Current stored resource count. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Count = 0;

	/** Current total capacity after base and station bonuses. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Capacity = 0;

	/** Shared replicated order key for UI sorting. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 SortIndex = 0;
};

/** GameplayMessage payload for replicated base-resource changes. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceChangeMessage
{
	GENERATED_BODY()

	/** Storage component whose resource row changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	TObjectPtr<UActorComponent> StorageOwner = nullptr;

	/** Material item definition that changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** New stored count after the change. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 NewCount = 0;

	/** Difference between new and previous count. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Delta = 0;

	/** Current capacity for this resource. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Capacity = 0;

	/** Shared replicated order key for UI refresh. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 SortIndex = 0;

	/** True when capacity changed without necessarily changing count. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	bool bCapacityChanged = false;

	/** True when resource order changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	bool bOrderChanged = false;
};

/** One replicated material resource count in a base storage pool. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

private:
	friend struct FRpgBaseResourceList;

	UPROPERTY()
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	int32 Capacity = 0;

	UPROPERTY()
	int32 SortIndex = 0;

	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;

	UPROPERTY(NotReplicated)
	int32 LastObservedCapacity = INDEX_NONE;
};

/** Replicated resource list backing a base camp storage component. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRpgBaseResourceList()
		: OwnerComponent(nullptr)
	{
	}

	explicit FRpgBaseResourceList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	TArray<FRpgBaseResourceEntryView> GetAllResources() const;
	int32 GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	int32 GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	int32 GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	bool CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;

	bool StoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);
	bool WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);
	void AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity);
	bool ApplySort(ERpgInventorySortMode SortMode);
	bool MoveResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex);

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgBaseResourceEntry, FRpgBaseResourceList>(Entries, DeltaParms, *this);
	}

private:
	FRpgBaseResourceEntry* FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);
	const FRpgBaseResourceEntry* FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	FRpgBaseResourceEntry& FindOrAddEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);
	void BroadcastChangeMessage(FRpgBaseResourceEntry& Entry, int32 OldCount, int32 OldCapacity, bool bOrderChanged = false);
	int32 GetNextSortIndex() const;
	void SortEntriesBySortIndex();
	bool SetOrderFromSortedEntryPointers(const TArray<FRpgBaseResourceEntry*>& SortedEntries);

private:
	UPROPERTY()
	TArray<FRpgBaseResourceEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FRpgBaseResourceList> : public TStructOpsTypeTraitsBase2<FRpgBaseResourceList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * Server-authoritative shared resource pool for a player base.
 *
 * Resources are stored as counts for performance and UI clarity. Instance-based gear belongs in the
 * base camp's separate armory inventory manager.
 */
UCLASS(Blueprintable, ClassGroup = (Base), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgBaseStorageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgBaseStorageComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns all known resource rows sorted by replicated SortIndex. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure = false)
	TArray<FRpgBaseResourceEntryView> GetAllResources() const;

	/** Returns the current stored count for one material definition. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	int32 GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns current total capacity for one material definition. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	int32 GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns unused capacity for one material definition. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	int32 GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns true if this many resources can be stored without exceeding capacity. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	bool CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;

	/**
	 * Returns whether a concrete material can be losslessly projected into this definition/count pool.
	 * Any semantic StatTag or fragment-owned payload fails closed because the pool cannot rehydrate instance state.
	 */
	bool CanStoreResourceInstance(const URpgInventoryItemInstance* Item, int32 Count) const;

	/**
	 * Adds a trusted synthetic/default resource credit to the base pool. Native-only and server-authoritative.
	 * This is reserved for recipe output, refunds, and rollback; concrete inventory items must use
	 * StoreResourceInstance so runtime variants and container subtrees cannot lose state.
	 */
	bool StoreDefinitionResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);

	/**
	 * Adds a validated stateless-material credit to the definition/count pool. Server-authoritative and fail-closed.
	 * This does not consume Item or verify inventory ownership/quantity; the authoritative caller owns consume and
	 * rollback around this credit write.
	 */
	bool StoreResourceInstance(const URpgInventoryItemInstance* Item, int32 Count);

	/** Removes resource counts from the base pool. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage")
	bool WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);

	/** Adds or removes resource capacity, usually from physical storage stations. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage")
	void AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity);

	/** Applies a shared server-side sort to base resources. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Sorting")
	bool ApplyResourceSort(ERpgInventorySortMode SortMode);

	/** Moves one resource row to a shared replicated index. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Sorting")
	bool MoveResourceEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex);

protected:
	/** Base capacities available before station bonuses are applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TArray<FRpgBaseResourceCapacity> DefaultResourceCapacities;

private:
	UPROPERTY(Replicated)
	FRpgBaseResourceList ResourceList;
};
