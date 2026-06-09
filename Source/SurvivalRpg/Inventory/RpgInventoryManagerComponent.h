// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Misc/Guid.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "RpgInventoryManagerComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class UObject;
struct FFrame;
struct FRpgInventoryList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

/** Server-authoritative sort modes that can rewrite shared inventory order. */
UENUM(BlueprintType)
enum class ERpgInventorySortMode : uint8
{
	/** Preserve the replicated SortIndex order, including manual moves. */
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

	/** Authoritative replicated stack count for this item entry. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	int32 StackCount = 0;

	/** Server-authored order key. Lower values appear earlier in manual/shared sorted views. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	int32 SortIndex = 0;
};

/** One save-ready inventory row used for session export/import and future world container saves. */
USTRUCT(BlueprintType)
struct FRpgInventorySnapshotEntry
{
	GENERATED_BODY()

	/** Stable entry id preserved when restoring a saved container order. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	FGuid EntryId;

	/** Static item definition to recreate for this saved stack. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Saved stack count for this entry. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	int32 StackCount = 0;

	/** Saved shared order key for this entry. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Snapshot")
	int32 SortIndex = 0;
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
	int32 SortIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	bool bOrderChanged = false;
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
	int32 SortIndex = 0;

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
	int32 GetStackCount(URpgInventoryItemInstance* Instance) const;
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
	void AddEntry(URpgInventoryItemInstance* Instance, int32 StackCount = 1);

	void RemoveEntry(URpgInventoryItemInstance* Instance);
	bool RemoveEntryStack(URpgInventoryItemInstance* Instance, int32 StackCount, bool& bOutRemovedEntry);
	bool ApplySort(ERpgInventorySortMode SortMode);
	bool MoveEntry(FGuid EntryId, int32 TargetIndex);
	FRpgInventorySnapshot ExportSnapshot(FName ContainerId) const;
	void ImportSnapshot(const FRpgInventorySnapshot& Snapshot);

private:
	void BroadcastChangeMessage(FRpgInventoryEntry& Entry, int32 OldCount, int32 NewCount, bool bOrderChanged = false);
	int32 GetNextSortIndex() const;
	void NormalizeSortIndices();
	void SortEntriesBySortIndex();
	bool SetOrderFromSortedEntryPointers(const TArray<FRpgInventoryEntry*>& SortedEntries);

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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool CanAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	URpgInventoryItemInstance* AddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddItemInstance(URpgInventoryItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddItemInstanceWithStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void RemoveItemInstance(URpgInventoryItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool RemoveItemInstanceStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<URpgInventoryItemInstance*> GetAllItems() const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<FRpgInventoryEntryView> GetAllEntries() const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	bool ContainsEntry(FGuid EntryId) const;

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

	/** Exports a save-ready snapshot containing item definitions, stack counts, entry ids, and shared order. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Snapshot")
	FRpgInventorySnapshot ExportInventorySnapshot(FName ContainerId) const;

	/** Replaces this inventory with a save-ready snapshot. Server-authoritative and intended for future world-save restore paths. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Snapshot")
	void ImportInventorySnapshot(const FRpgInventorySnapshot& Snapshot);

	//~UObject interface
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;
	//~End of UObject interface

private:
	UPROPERTY(Replicated)
	FRpgInventoryList InventoryList;
};
