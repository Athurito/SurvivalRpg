// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
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

	/** Authoritative replicated stack count for this item entry. */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	int32 StackCount = 0;
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

	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 NewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 Delta = 0;
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
	int32 StackCount = 0;

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

private:
	void BroadcastChangeMessage(FRpgInventoryEntry& Entry, int32 OldCount, int32 NewCount);

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
	URpgInventoryItemInstance* FindFirstItemStackByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	bool ContainsItemInstance(URpgInventoryItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	int32 GetItemStackCount(URpgInventoryItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	int32 GetTotalItemCountByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool ConsumeItemsByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 NumToConsume);

	//~UObject interface
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;
	//~End of UObject interface

private:
	UPROPERTY(Replicated)
	FRpgInventoryList InventoryList;
};
