#include "Harvesting/RpgHarvestInstanceStockShard.h"

#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestInstanceStockShard)

void FRpgHarvestInstanceStockList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, const int32 FinalSize)
{
	// The entries still exist here; they are presented once the whole update has been applied.
	for (const int32 ArrayIndex : RemovedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			PendingRemovedKeys.Add(Entries[ArrayIndex].Key);
		}
	}
	bIndexDirty = true;
}

void FRpgHarvestInstanceStockList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, const int32 FinalSize)
{
	for (const int32 ArrayIndex : AddedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			PendingChangedKeys.Add(Entries[ArrayIndex].Key);
		}
	}
	bIndexDirty = true;
}

void FRpgHarvestInstanceStockList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, const int32 FinalSize)
{
	for (const int32 ArrayIndex : ChangedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			PendingChangedKeys.Add(Entries[ArrayIndex].Key);
		}
	}
}

void FRpgHarvestInstanceStockList::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	const TArray<FIntVector> ChangedKeys = MoveTemp(PendingChangedKeys);
	const TArray<FIntVector> RemovedKeys = MoveTemp(PendingRemovedKeys);
	PendingChangedKeys.Reset();
	PendingRemovedKeys.Reset();
	if (OwnerShard && (!ChangedKeys.IsEmpty() || !RemovedKeys.IsEmpty()))
	{
		OwnerShard->HandleReplicatedUpdate(ChangedKeys, RemovedKeys);
	}
}

int32 FRpgHarvestInstanceStockList::FindIndex(const FIntVector& Key) const
{
	if (bIndexDirty)
	{
		IndexByKey.Reset();
		IndexByKey.Reserve(Entries.Num());
		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			IndexByKey.Add(Entries[EntryIndex].Key, EntryIndex);
		}
		bIndexDirty = false;
	}
	const int32* EntryIndex = IndexByKey.Find(Key);
	return EntryIndex ? *EntryIndex : INDEX_NONE;
}

int32 FRpgHarvestInstanceStockList::AddEntry(const FIntVector& Key)
{
	FindIndex(Key);
	const int32 EntryIndex = Entries.AddDefaulted();
	Entries[EntryIndex].Key = Key;
	IndexByKey.Add(Key, EntryIndex);
	return EntryIndex;
}

void FRpgHarvestInstanceStockList::RemoveEntryAt(const int32 EntryIndex)
{
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return;
	}

	FindIndex(Entries[EntryIndex].Key);
	IndexByKey.Remove(Entries[EntryIndex].Key);
	Entries.RemoveAtSwap(EntryIndex);
	if (Entries.IsValidIndex(EntryIndex))
	{
		IndexByKey.Add(Entries[EntryIndex].Key, EntryIndex);
	}
	MarkArrayDirty();
}

ARpgHarvestInstanceStockShard::ARpgHarvestInstanceStockShard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	// Every change forces an update; the low rate only bounds idle checks.
	SetNetUpdateFrequency(1.0f);
	PrimaryActorTick.bCanEverTick = false;
	Stock.OwnerShard = this;
}

void ARpgHarvestInstanceStockShard::InitializeShard(const int32 InShardIndex)
{
	ShardIndex = static_cast<uint8>(FMath::Clamp(InShardIndex, 0, URpgHarvestInstanceStockComponent::NumShards - 1));
}

void ARpgHarvestInstanceStockShard::BeginPlay()
{
	Super::BeginPlay();
	Stock.OwnerShard = this;

	// The server's stock component registers the shards it spawns. A client shard registers with the stock component
	// if that already replicated; otherwise the component registers the shards it finds when it begins play.
	if (!HasAuthority() && !IsRegistered())
	{
		if (URpgHarvestInstanceStockComponent* InstanceStock = URpgHarvestInstanceStockComponent::FindForWorld(GetWorld());
			InstanceStock && InstanceStock->HasBegunPlay())
		{
			InstanceStock->RegisterShard(*this);
		}
	}
}

void ARpgHarvestInstanceStockShard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URpgHarvestInstanceStockComponent* InstanceStock = RegisteredStock.Get())
	{
		InstanceStock->UnregisterShard(*this);
	}
	RegisteredStock.Reset();
	Super::EndPlay(EndPlayReason);
}

void ARpgHarvestInstanceStockShard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Stock);
	DOREPLIFETIME_CONDITION(ThisClass, ShardIndex, COND_InitialOnly);
}

void ARpgHarvestInstanceStockShard::HandleReplicatedUpdate(
	const TConstArrayView<FIntVector> ChangedKeys,
	const TConstArrayView<FIntVector> RemovedKeys)
{
	// Updates before registration, such as the shard's initial state, are presented when the shard registers.
	if (URpgHarvestInstanceStockComponent* InstanceStock = RegisteredStock.Get())
	{
		InstanceStock->HandleShardUpdate(*this, ChangedKeys, RemovedKeys);
	}
}
