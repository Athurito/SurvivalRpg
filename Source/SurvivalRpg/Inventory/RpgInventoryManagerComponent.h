// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttributeSet.h"
#include "Components/ActorComponent.h"
#include "Misc/Guid.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "RpgInventoryManagerComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class UAbilitySystemComponent;
class UObject;
struct FOnAttributeChangeData;
struct FFrame;
struct FRpgInventoryList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

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
	int32 GetUsedEntryCount() const;
	int32 GetRequiredNewEntryCount(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount) const;
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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;
	//~End of UObject interface

private:
	UAbilitySystemComponent* FindCapacityAbilitySystem() const;
	void RefreshCapacityAttributeBinding();
	void ClearCapacityAttributeBinding();
	void HandleCapacityAttributeChanged(const FOnAttributeChangeData& Data);
	void BroadcastCapacityChanged() const;

private:
	/** Source used to determine how many entries this inventory may hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryCapacityMode CapacityMode = ERpgInventoryCapacityMode::Unlimited;

	/** Fixed entry limit and fallback when the configured GAS attribute is unavailable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 FixedMaxEntries = 0;

	/** GAS attribute used as entry capacity when CapacityMode is AbilitySystemAttribute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	FGameplayAttribute CapacityAttribute;

	UPROPERTY(Replicated)
	FRpgInventoryList InventoryList;

	TWeakObjectPtr<UAbilitySystemComponent> BoundCapacityAbilitySystem;
	FDelegateHandle CapacityAttributeChangedHandle;
};
