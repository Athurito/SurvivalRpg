// Copyright Epic Games, Inc. All Rights Reserved.
#include "RpgInventoryManagerComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"
#include "NativeGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Inventory_Message_StackChanged, "Rpg.Inventory.Message.StackChanged");

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	int32 GetMaxStackSizeForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		if (const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(ItemDef))
		{
			return Traits->GetMaxStackSize();
		}

		return 1;
	}
}

//////////////////////////////////////////////////////////////////////
// FRpgInventoryEntry

FString FRpgInventoryEntry::GetDebugString() const
{
	TSubclassOf<URpgInventoryItemDefinition> ItemDef;
	if (Instance != nullptr)
	{
		ItemDef = Instance->GetItemDef();
	}

	return FString::Printf(TEXT("%s (%d x %s)"), *GetNameSafe(Instance), StackCount, *GetNameSafe(ItemDef));
}

//////////////////////////////////////////////////////////////////////
// FRpgInventoryList

void FRpgInventoryList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FRpgInventoryEntry& Stack = Entries[Index];
		BroadcastChangeMessage(Stack, /*OldCount=*/ Stack.StackCount, /*NewCount=*/ 0);
		Stack.LastObservedCount = 0;
	}
}

void FRpgInventoryList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FRpgInventoryEntry& Stack = Entries[Index];
		BroadcastChangeMessage(Stack, /*OldCount=*/ 0, /*NewCount=*/ Stack.StackCount);
		Stack.LastObservedCount = Stack.StackCount;
	}
}

void FRpgInventoryList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		FRpgInventoryEntry& Stack = Entries[Index];
		check(Stack.LastObservedCount != INDEX_NONE);
		BroadcastChangeMessage(Stack, /*OldCount=*/ Stack.LastObservedCount, /*NewCount=*/ Stack.StackCount);
		Stack.LastObservedCount = Stack.StackCount;
	}
}

void FRpgInventoryList::BroadcastChangeMessage(FRpgInventoryEntry& Entry, int32 OldCount, int32 NewCount)
{
	FRpgInventoryChangeMessage Message;
	Message.InventoryOwner = OwnerComponent;
	Message.Instance = Entry.Instance;
	Message.NewCount = NewCount;
	Message.Delta = NewCount - OldCount;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(OwnerComponent->GetWorld());
	MessageSystem.BroadcastMessage(TAG_Rpg_Inventory_Message_StackChanged, Message);
}

URpgInventoryItemInstance* FRpgInventoryList::AddEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount, TArray<URpgInventoryItemInstance*>& OutNewInstances)
{
	URpgInventoryItemInstance* Result = nullptr;

	check(ItemDef != nullptr);
 	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());

	int32 RemainingCount = StackCount;
	if (RemainingCount <= 0)
	{
		return nullptr;
	}

	const int32 MaxStackSize = GetMaxStackSizeForDefinition(ItemDef);
	if (MaxStackSize > 1)
	{
		for (FRpgInventoryEntry& Entry : Entries)
		{
			if (RemainingCount <= 0)
			{
				break;
			}

			if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDef || Entry.StackCount >= MaxStackSize)
			{
				continue;
			}

			const int32 OldCount = Entry.StackCount;
			const int32 CountToAdd = FMath::Min(MaxStackSize - Entry.StackCount, RemainingCount);
			Entry.StackCount += CountToAdd;
			RemainingCount -= CountToAdd;
			if (!Result)
			{
				Result = Entry.Instance.Get();
			}

			MarkItemDirty(Entry);
			BroadcastChangeMessage(Entry, OldCount, Entry.StackCount);
		}
	}

	while (RemainingCount > 0)
	{
		const int32 NewEntryCount = FMath::Min(MaxStackSize, RemainingCount);
		RemainingCount -= NewEntryCount;

		FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.Instance = NewObject<URpgInventoryItemInstance>(OwnerComponent->GetOwner());  //@TODO: Using the actor instead of component as the outer due to UE-127172
		NewEntry.Instance->SetItemDef(ItemDef);
		for (URpgInventoryItemFragment* Fragment : GetDefault<URpgInventoryItemDefinition>(ItemDef)->Fragments)
		{
			if (Fragment != nullptr)
			{
				Fragment->OnInstanceCreated(NewEntry.Instance);
			}
		}
		NewEntry.StackCount = NewEntryCount;
		if (!Result)
		{
			Result = NewEntry.Instance.Get();
		}
		OutNewInstances.Add(NewEntry.Instance);

		MarkItemDirty(NewEntry);
		BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
	}


	return Result;
}

void FRpgInventoryList::AddEntry(URpgInventoryItemInstance* Instance, int32 StackCount)
{
	//Noot implemented in lyra
	if (Instance == nullptr || StackCount <= 0)
	{
		return;
	}

	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor && OwningActor->HasAuthority());

	FRpgInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = Instance;
	NewEntry.StackCount = StackCount;
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
}

void FRpgInventoryList::RemoveEntry(URpgInventoryItemInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRpgInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			BroadcastChangeMessage(Entry, Entry.StackCount, 0);
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
			return;
		}
	}
}

bool FRpgInventoryList::RemoveEntryStack(URpgInventoryItemInstance* Instance, int32 StackCount, bool& bOutRemovedEntry)
{
	bOutRemovedEntry = false;
	if (Instance == nullptr || StackCount <= 0)
	{
		return false;
	}

	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRpgInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance != Instance)
		{
			continue;
		}

		if (Entry.StackCount < StackCount)
		{
			return false;
		}

		const int32 OldCount = Entry.StackCount;
		Entry.StackCount -= StackCount;

		if (Entry.StackCount <= 0)
		{
			BroadcastChangeMessage(Entry, OldCount, 0);
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
			bOutRemovedEntry = true;
			return true;
		}

		MarkItemDirty(Entry);
		BroadcastChangeMessage(Entry, OldCount, Entry.StackCount);
		return true;
	}

	return false;
}

TArray<URpgInventoryItemInstance*> FRpgInventoryList::GetAllItems() const
{
	TArray<URpgInventoryItemInstance*> Results;
	Results.Reserve(Entries.Num());
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr) //@TODO: Would prefer to not deal with this here and hide it further?
		{
			Results.Add(Entry.Instance);
		}
	}
	return Results;
}

TArray<FRpgInventoryEntryView> FRpgInventoryList::GetAllEntries() const
{
	TArray<FRpgInventoryEntryView> Results;
	Results.Reserve(Entries.Num());

	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr)
		{
			FRpgInventoryEntryView& View = Results.AddDefaulted_GetRef();
			View.InventoryOwner = OwnerComponent;
			View.Instance = Entry.Instance;
			View.StackCount = Entry.StackCount;
		}
	}

	return Results;
}

int32 FRpgInventoryList::GetStackCount(URpgInventoryItemInstance* Instance) const
{
	for (const FRpgInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance)
		{
			return Entry.StackCount;
		}
	}

	return 0;
}

bool FRpgInventoryList::ContainsItemInstance(URpgInventoryItemInstance* Instance) const
{
	return GetStackCount(Instance) > 0;
}

//////////////////////////////////////////////////////////////////////
// URpgInventoryManagerComponent

URpgInventoryManagerComponent::URpgInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InventoryList(this)
{
	SetIsReplicatedByDefault(true);
}

void URpgInventoryManagerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

bool URpgInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount)
{
	return ItemDef != nullptr && StackCount > 0;
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::AddItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 StackCount)
{
	URpgInventoryItemInstance* Result = nullptr;
	if (ItemDef != nullptr)
	{
		TArray<URpgInventoryItemInstance*> NewInstances;
		Result = InventoryList.AddEntry(ItemDef, StackCount, NewInstances);
		
		if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
		{
			for (URpgInventoryItemInstance* NewInstance : NewInstances)
			{
				if (NewInstance)
				{
					AddReplicatedSubObject(NewInstance);
				}
			}
		}
	}
	return Result;
}

void URpgInventoryManagerComponent::AddItemInstance(URpgInventoryItemInstance* ItemInstance)
{
	AddItemInstanceWithStack(ItemInstance, 1);
}

void URpgInventoryManagerComponent::AddItemInstanceWithStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	InventoryList.AddEntry(ItemInstance, StackCount);
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
	{
		AddReplicatedSubObject(ItemInstance);
	}
}

void URpgInventoryManagerComponent::RemoveItemInstance(URpgInventoryItemInstance* ItemInstance)
{
	InventoryList.RemoveEntry(ItemInstance);

	if (ItemInstance && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}
}

bool URpgInventoryManagerComponent::RemoveItemInstanceStack(URpgInventoryItemInstance* ItemInstance, int32 StackCount)
{
	bool bRemovedEntry = false;
	const bool bRemovedStack = InventoryList.RemoveEntryStack(ItemInstance, StackCount, bRemovedEntry);

	if (bRemovedStack && bRemovedEntry && ItemInstance && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}

	return bRemovedStack;
}

TArray<URpgInventoryItemInstance*> URpgInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

TArray<FRpgInventoryEntryView> URpgInventoryManagerComponent::GetAllEntries() const
{
	return InventoryList.GetAllEntries();
}

bool URpgInventoryManagerComponent::ContainsItemInstance(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.ContainsItemInstance(ItemInstance);
}

int32 URpgInventoryManagerComponent::GetItemStackCount(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.GetStackCount(ItemInstance);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::FindFirstItemStackByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

int32 URpgInventoryManagerComponent::GetTotalItemCountByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	int32 TotalCount = 0;
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				TotalCount += Entry.StackCount;
			}
		}
	}

	return TotalCount;
}

bool URpgInventoryManagerComponent::ConsumeItemsByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef, int32 NumToConsume)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	if (GetTotalItemCountByDefinition(ItemDef) < NumToConsume)
	{
		return false;
	}

	int32 RemainingToConsume = NumToConsume;
	while (RemainingToConsume > 0)
	{
		URpgInventoryItemInstance* Instance = FindFirstItemStackByDefinition(ItemDef);
		if (Instance == nullptr)
		{
			return false;
		}

		const int32 CountToConsume = FMath::Min(RemainingToConsume, GetItemStackCount(Instance));
		if (!RemoveItemInstanceStack(Instance, CountToConsume))
		{
			return false;
		}

		RemainingToConsume -= CountToConsume;
	}

	return true;
}

void URpgInventoryManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Register existing URpgInventoryItemInstance
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
		{
			URpgInventoryItemInstance* Instance = Entry.Instance;

			if (IsValid(Instance))
			{
				AddReplicatedSubObject(Instance);
			}
		}
	}
}

bool URpgInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (Instance && IsValid(Instance))
		{
			WroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

//////////////////////////////////////////////////////////////////////
//

// UCLASS(Abstract)
// class URpgInventoryFilter : public UObject
// {
// public:
// 	virtual bool PassesFilter(URpgInventoryItemInstance* Instance) const { return true; }
// };

// UCLASS()
// class URpgInventoryFilter_HasTag : public URpgInventoryFilter
// {
// public:
// 	virtual bool PassesFilter(URpgInventoryItemInstance* Instance) const { return true; }
// };


