#include "Harvesting/RpgHarvestInstanceStockComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestPersistenceComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Misc/Crc.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestInstanceStockComponent)

URpgHarvestInstanceStockComponent::URpgHarvestInstanceStockComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	// Replicates so clients have the component; the stock itself replicates through the shards.
	SetIsReplicatedByDefault(true);
	Shards.SetNum(NumShards);
}

URpgHarvestInstanceStockComponent* URpgHarvestInstanceStockComponent::FindForWorld(const UWorld* World)
{
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<URpgHarvestInstanceStockComponent>() : nullptr;
}

FIntVector URpgHarvestInstanceStockComponent::MakeInstanceKey(const FVector& AuthoredWorldLocation)
{
	auto Quantize = [](const double Value)
	{
		return FMath::RoundToInt32(FMath::Clamp(Value, static_cast<double>(MIN_int32), static_cast<double>(MAX_int32)));
	};
	return FIntVector(Quantize(AuthoredWorldLocation.X), Quantize(AuthoredWorldLocation.Y), Quantize(AuthoredWorldLocation.Z));
}

int32 URpgHarvestInstanceStockComponent::GetShardIndex(const FIntVector& Key)
{
	// A CRC spreads grid-aligned keys evenly; it depends only on the key, so every machine picks the same shard.
	const int32 Components[3] = {Key.X, Key.Y, Key.Z};
	return static_cast<int32>(FCrc::MemCrc32(Components, sizeof(Components)) % static_cast<uint32>(NumShards));
}

FRpgHarvestStockSnapshot URpgHarvestInstanceStockComponent::GetStockSnapshot(const FIntVector& Key, const int32 SectionCount) const
{
	FRpgHarvestStockSnapshot Snapshot;
	Snapshot.SectionCount = FMath::Clamp(SectionCount, 1, URpgHarvestProfile::MaxSectionCount);
	if (const FRpgHarvestInstanceStockEntry* Entry = FindEntry(Key))
	{
		Snapshot.Revision = Entry->Revision;
		Snapshot.HarvestedSections = Entry->HarvestedSections;
		Snapshot.bActive = Entry->bActive;
	}
	return Snapshot;
}

bool URpgHarvestInstanceStockComponent::ExtractSections(
	const FIntVector& Key,
	const int32 SectionCount,
	const int32 SectionsTaken,
	const float RespawnDelaySeconds,
	const float PresentationDelaySeconds,
	const float HarvestYawDegrees)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestInstanceStockComponent::ExtractSections);
	if (!HasStockAuthority() || SectionsTaken <= 0)
	{
		return false;
	}

	ARpgHarvestInstanceStockShard* Shard = FindOrSpawnShard(Key);
	if (!Shard)
	{
		return false;
	}
	FRpgHarvestInstanceStockList& Stock = Shard->GetMutableStock();
	int32 EntryIndex = Stock.FindIndex(Key);
	if (EntryIndex == INDEX_NONE)
	{
		EntryIndex = Stock.AddEntry(Key);
	}

	FRpgHarvestInstanceStockEntry& Entry = Stock.GetEntry(EntryIndex);
	if (!Entry.bActive)
	{
		return false;
	}

	const int32 ClampedSectionCount = FMath::Clamp(SectionCount, 1, URpgHarvestProfile::MaxSectionCount);
	Entry.HarvestedSections = static_cast<uint8>(
		FMath::Clamp(static_cast<int32>(Entry.HarvestedSections) + SectionsTaken, 0, ClampedSectionCount));
	if (Entry.HarvestedSections >= ClampedSectionCount)
	{
		Entry.bActive = false;
		Entry.Revision = FMath::Max(1, Entry.Revision + 1);
		const UWorld* World = GetWorld();
		if (World && RespawnDelaySeconds > 0.0f)
		{
			ScheduleRespawn(Key, World->GetTimeSeconds() + RespawnDelaySeconds);
		}
	}
	Entry.LastChangeServerTime = GetServerWorldTimeSeconds();
	const float ClampedDelay = FMath::IsFinite(PresentationDelaySeconds)
		? FMath::Clamp(PresentationDelaySeconds, 0.0f, MaxPresentationDelaySeconds)
		: 0.0f;
	Entry.PresentationDelayCentiseconds = static_cast<uint8>(FMath::RoundToInt32(ClampedDelay * 100.0f));
	const float Yaw = FMath::IsFinite(HarvestYawDegrees) ? FRotator::ClampAxis(HarvestYawDegrees) : 0.0f;
	Entry.HarvestYaw = static_cast<uint8>(FMath::RoundToInt32(Yaw * 256.0f / 360.0f) & 0xFF);
	Stock.MarkItemDirty(Entry);

	MarkStockChanged(*Shard, Key);
	return true;
}

bool URpgHarvestInstanceStockComponent::GetHarvestDirection(const FIntVector& Key, FVector& OutDirection) const
{
	const FRpgHarvestInstanceStockEntry* Entry = FindEntry(Key);
	if (!Entry)
	{
		OutDirection = FVector::ZeroVector;
		return false;
	}
	const double YawRadians = FMath::DegreesToRadians(Entry->HarvestYaw * 360.0 / 256.0);
	OutDirection = FVector(FMath::Cos(YawRadians), FMath::Sin(YawRadians), 0.0);
	return true;
}

float URpgHarvestInstanceStockComponent::GetRemainingPresentationDelay(const FIntVector& Key) const
{
	const FRpgHarvestInstanceStockEntry* Entry = FindEntry(Key);
	if (!Entry || Entry->PresentationDelayCentiseconds == 0)
	{
		return 0.0f;
	}
	// Clients receive the change later than the server made it; that latency is part of the delay.
	const float Elapsed = FMath::Max(0.0f, GetServerWorldTimeSeconds() - Entry->LastChangeServerTime);
	return FMath::Max(0.0f, Entry->PresentationDelayCentiseconds / 100.0f - Elapsed);
}

bool URpgHarvestInstanceStockComponent::RestoreStock(const FIntVector& Key)
{
	if (!HasStockAuthority())
	{
		return false;
	}

	RespawnDeadlines.Remove(Key);
	ARpgHarvestInstanceStockShard* Shard = GetShard(GetShardIndex(Key));
	const int32 EntryIndex = Shard ? Shard->GetStock().FindIndex(Key) : INDEX_NONE;
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	// Restored instances match their authored state again, so they stop costing memory and bandwidth.
	Shard->GetMutableStock().RemoveEntryAt(EntryIndex);
	MarkStockChanged(*Shard, Key);
	if (URpgHarvestPersistenceComponent* Persistence = URpgHarvestPersistenceComponent::FindForWorld(GetWorld()))
	{
		Persistence->ForgetInstance(Key);
	}
	return true;
}

bool URpgHarvestInstanceStockComponent::ExportSavedStock(const FIntVector& Key, FRpgHarvestSavedStock& OutStock) const
{
	OutStock = FRpgHarvestSavedStock();
	const FRpgHarvestInstanceStockEntry* Entry = FindEntry(Key);
	if (!Entry)
	{
		return false;
	}

	OutStock.Revision = Entry->Revision;
	OutStock.HarvestedSections = Entry->HarvestedSections;
	OutStock.bActive = Entry->bActive;
	const UWorld* World = GetWorld();
	if (const double* Deadline = RespawnDeadlines.Find(Key); Deadline && World)
	{
		OutStock.RespawnSeconds = static_cast<float>(FMath::Max(0.0, *Deadline - World->GetTimeSeconds()));
	}
	return true;
}

bool URpgHarvestInstanceStockComponent::ApplySavedStock(const FIntVector& Key, const FRpgHarvestSavedStock& SavedStock)
{
	UWorld* World = GetWorld();
	if (!HasStockAuthority() || !World)
	{
		return false;
	}

	RespawnDeadlines.Remove(Key);
	ARpgHarvestInstanceStockShard* Shard = SavedStock.IsPristine() ? GetShard(GetShardIndex(Key)) : FindOrSpawnShard(Key);
	if (!Shard)
	{
		// Without a shard, a pristine stock has no entry to drop, and a changed stock cannot be stored.
		if (!SavedStock.IsPristine())
		{
			return false;
		}
		NotifyStockChanged(Key, true);
		return true;
	}

	FRpgHarvestInstanceStockList& Stock = Shard->GetMutableStock();
	int32 EntryIndex = Stock.FindIndex(Key);
	if (SavedStock.IsPristine())
	{
		if (EntryIndex != INDEX_NONE)
		{
			Stock.RemoveEntryAt(EntryIndex);
		}
	}
	else
	{
		if (EntryIndex == INDEX_NONE)
		{
			EntryIndex = Stock.AddEntry(Key);
		}
		FRpgHarvestInstanceStockEntry& Entry = Stock.GetEntry(EntryIndex);
		Entry.Revision = FMath::Max(0, SavedStock.Revision);
		Entry.HarvestedSections = static_cast<uint8>(
			FMath::Clamp(SavedStock.HarvestedSections, 0, URpgHarvestProfile::MaxSectionCount));
		Entry.bActive = SavedStock.bActive;
		// Older than the live window, so every machine snaps to the saved stock instead of animating it.
		Entry.LastChangeServerTime = GetServerWorldTimeSeconds() - LiveChangeWindowSeconds - 1.0f;
		Entry.PresentationDelayCentiseconds = 0;
		Entry.HarvestYaw = 0;
		Stock.MarkItemDirty(Entry);
		if (!SavedStock.bActive && SavedStock.RespawnSeconds >= 0.0f)
		{
			ScheduleRespawn(Key, World->GetTimeSeconds() + FMath::Max(0.001f, SavedStock.RespawnSeconds));
		}
	}

	Shard->ForceNetUpdate();
	NotifyStockChanged(Key, true);
	return true;
}

int32 URpgHarvestInstanceStockComponent::GetNumChangedInstances() const
{
	int32 NumChanged = 0;
	for (const ARpgHarvestInstanceStockShard* Shard : Shards)
	{
		NumChanged += Shard ? Shard->GetStock().GetEntries().Num() : 0;
	}
	return NumChanged;
}

int32 URpgHarvestInstanceStockComponent::GetMaxShardEntries() const
{
	int32 MaxEntries = 0;
	for (const ARpgHarvestInstanceStockShard* Shard : Shards)
	{
		MaxEntries = FMath::Max(MaxEntries, Shard ? Shard->GetStock().GetEntries().Num() : 0);
	}
	return MaxEntries;
}

bool URpgHarvestInstanceStockComponent::HasStockAuthority() const
{
	const AActor* OwningActor = GetOwner();
	return OwningActor && OwningActor->HasAuthority();
}

void URpgHarvestInstanceStockComponent::RegisterInstances(URpgHarvestableInstancesComponent& Instances)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestInstanceStockComponent::RegisterInstances);
	for (const TWeakObjectPtr<URpgHarvestableInstancesComponent>& Registered : RegisteredInstances)
	{
		if (Registered.Get() == &Instances)
		{
			return;
		}
	}
	RegisteredInstances.Add(&Instances);

	TArray<FIntVector> Keys;
	Keys.Reserve(GetNumChangedInstances());
	for (const ARpgHarvestInstanceStockShard* Shard : Shards)
	{
		if (Shard)
		{
			for (const FRpgHarvestInstanceStockEntry& Entry : Shard->GetStock().GetEntries())
			{
				Keys.Add(Entry.Key);
			}
		}
	}
	for (const FIntVector& Key : Keys)
	{
		Instances.HandleStockChanged(Key, true);
	}
}

void URpgHarvestInstanceStockComponent::UnregisterInstances(const URpgHarvestableInstancesComponent& Instances)
{
	RegisteredInstances.RemoveAllSwap([&Instances](const TWeakObjectPtr<URpgHarvestableInstancesComponent>& Registered)
	{
		return !Registered.IsValid() || Registered.Get() == &Instances;
	});
}

int32 URpgHarvestInstanceStockComponent::RestoreInstances(const URpgHarvestableInstancesComponent& Instances)
{
	if (!HasStockAuthority())
	{
		return 0;
	}

	int32 NumRestored = 0;
	TArray<FIntVector> Keys;
	Instances.InstanceIndexByKey.GetKeys(Keys);
	for (const FIntVector& Key : Keys)
	{
		NumRestored += RestoreStock(Key) ? 1 : 0;
	}
	return NumRestored;
}

void URpgHarvestInstanceStockComponent::RegisterShard(ARpgHarvestInstanceStockShard& Shard)
{
	const int32 ShardIndex = Shard.GetShardIndex();
	Shards.SetNum(NumShards);
	if (!Shards.IsValidIndex(ShardIndex) || Shards[ShardIndex] == &Shard)
	{
		return;
	}

	Shards[ShardIndex] = &Shard;
	Shard.SetRegisteredStock(this);
	if (HasStockAuthority())
	{
		return;
	}

	// The shard's entries arrived before it registered, for example with a late join; present them now.
	for (const FRpgHarvestInstanceStockEntry& Entry : Shard.GetStock().GetEntries())
	{
		NotifyStockChanged(Entry.Key, IsInitialState(Entry));
	}
}

void URpgHarvestInstanceStockComponent::UnregisterShard(const ARpgHarvestInstanceStockShard& Shard)
{
	const int32 ShardIndex = Shard.GetShardIndex();
	if (Shards.IsValidIndex(ShardIndex) && Shards[ShardIndex] == &Shard)
	{
		Shards[ShardIndex] = nullptr;
	}
}

void URpgHarvestInstanceStockComponent::HandleShardUpdate(
	const ARpgHarvestInstanceStockShard& Shard,
	const TConstArrayView<FIntVector> ChangedKeys,
	const TConstArrayView<FIntVector> RemovedKeys)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestInstanceStockComponent::HandleShardUpdate);
	const FRpgHarvestInstanceStockList& Stock = Shard.GetStock();
	for (const FIntVector& Key : ChangedKeys)
	{
		const int32 EntryIndex = Stock.FindIndex(Key);
		if (EntryIndex != INDEX_NONE)
		{
			NotifyStockChanged(Key, IsInitialState(Stock.GetEntry(EntryIndex)));
		}
	}
	for (const FIntVector& Key : RemovedKeys)
	{
		// A key removed and added again in the same update was already presented with the changes.
		if (Stock.FindIndex(Key) == INDEX_NONE)
		{
			NotifyStockChanged(Key, false);
		}
	}
}

void URpgHarvestInstanceStockComponent::BeginPlay()
{
	Super::BeginPlay();

	// Client shards that replicated before this component register now; later ones register on their BeginPlay.
	UWorld* World = GetWorld();
	if (World && !HasStockAuthority())
	{
		for (TActorIterator<ARpgHarvestInstanceStockShard> It(World); It; ++It)
		{
			if (IsValid(*It) && It->HasActorBegunPlay())
			{
				RegisterShard(**It);
			}
		}
	}

	if (URpgHarvestPersistenceComponent* Persistence = URpgHarvestPersistenceComponent::FindForWorld(World))
	{
		Persistence->ApplyInstanceRecords(*this);
	}

	// Representations that began play before the GameFeature added this component register now; later ones
	// register themselves on BeginPlay, for example when World Partition streams them in.
	for (TObjectIterator<URpgHarvestableInstancesComponent> It; It; ++It)
	{
		URpgHarvestableInstancesComponent* Instances = *It;
		if (IsValid(Instances) && Instances->GetWorld() == World && Instances->HasBegunPlay())
		{
			Instances->CachedStock = this;
			RegisterInstances(*Instances);
		}
	}
}

void URpgHarvestInstanceStockComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
	ArmedRespawnDeadline = TNumericLimits<double>::Max();
	RespawnDeadlines.Reset();
	RegisteredInstances.Reset();

	const bool bDestroyShards = HasStockAuthority();
	for (TObjectPtr<ARpgHarvestInstanceStockShard>& Shard : Shards)
	{
		if (Shard)
		{
			Shard->SetRegisteredStock(nullptr);
			// The stock ends with this component, for example when its GameFeature deactivates.
			if (bDestroyShards && !Shard->IsActorBeingDestroyed())
			{
				Shard->Destroy();
			}
		}
		Shard = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

ARpgHarvestInstanceStockShard* URpgHarvestInstanceStockComponent::GetShard(const int32 ShardIndex) const
{
	return Shards.IsValidIndex(ShardIndex) ? Shards[ShardIndex].Get() : nullptr;
}

const ARpgHarvestInstanceStockShard* URpgHarvestInstanceStockComponent::FindShard(const FIntVector& Key) const
{
	return GetShard(GetShardIndex(Key));
}

ARpgHarvestInstanceStockShard* URpgHarvestInstanceStockComponent::FindOrSpawnShard(const FIntVector& Key)
{
	const int32 ShardIndex = GetShardIndex(Key);
	if (ARpgHarvestInstanceStockShard* Shard = GetShard(ShardIndex))
	{
		return Shard;
	}

	UWorld* World = GetWorld();
	if (!World || !HasStockAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.bDeferConstruction = true;
	ARpgHarvestInstanceStockShard* Shard = World->SpawnActor<ARpgHarvestInstanceStockShard>(SpawnParameters);
	if (!Shard)
	{
		return nullptr;
	}
	Shard->InitializeShard(ShardIndex);
	Shard->FinishSpawning(FTransform::Identity);
	RegisterShard(*Shard);
	return Shard;
}

const FRpgHarvestInstanceStockEntry* URpgHarvestInstanceStockComponent::FindEntry(const FIntVector& Key) const
{
	const ARpgHarvestInstanceStockShard* Shard = FindShard(Key);
	const int32 EntryIndex = Shard ? Shard->GetStock().FindIndex(Key) : INDEX_NONE;
	return EntryIndex != INDEX_NONE ? &Shard->GetStock().GetEntry(EntryIndex) : nullptr;
}

void URpgHarvestInstanceStockComponent::MarkStockChanged(ARpgHarvestInstanceStockShard& Shard, const FIntVector& Key)
{
	Shard.ForceNetUpdate();
	NotifyStockChanged(Key, false);
}

void URpgHarvestInstanceStockComponent::NotifyStockChanged(const FIntVector& Key, const bool bInitialState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestInstanceStockComponent::NotifyStockChanged);
	// Presentation events may stream representations out, so iterate over a copy.
	const TArray<TWeakObjectPtr<URpgHarvestableInstancesComponent>, TInlineAllocator<16>> Registered(RegisteredInstances);
	for (const TWeakObjectPtr<URpgHarvestableInstancesComponent>& WeakInstances : Registered)
	{
		if (URpgHarvestableInstancesComponent* Instances = WeakInstances.Get())
		{
			Instances->HandleStockChanged(Key, bInitialState);
		}
	}
}

bool URpgHarvestInstanceStockComponent::IsInitialState(const FRpgHarvestInstanceStockEntry& Entry) const
{
	// A change older than the live window was made before this client received it (late join or a GameFeature
	// added later) and is presented as initial state.
	return GetServerWorldTimeSeconds() - Entry.LastChangeServerTime > LiveChangeWindowSeconds;
}

float URpgHarvestInstanceStockComponent::GetServerWorldTimeSeconds() const
{
	if (const AGameStateBase* GameState = GetGameState<AGameStateBase>())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}

void URpgHarvestInstanceStockComponent::ScheduleRespawn(const FIntVector& Key, const double Deadline)
{
	RespawnDeadlines.Add(Key, Deadline);
	// Only an earlier deadline re-arms the timer, so scheduling stays constant time however many respawns wait.
	UWorld* World = GetWorld();
	if (World && Deadline < ArmedRespawnDeadline)
	{
		ArmedRespawnDeadline = Deadline;
		const float Delay = static_cast<float>(FMath::Max(0.001, Deadline - World->GetTimeSeconds()));
		World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ThisClass::HandleRespawnTimer, Delay, false);
	}
}

void URpgHarvestInstanceStockComponent::ArmNextRespawnTimer()
{
	ArmedRespawnDeadline = TNumericLimits<double>::Max();
	UWorld* World = GetWorld();
	if (!World || RespawnDeadlines.IsEmpty())
	{
		return;
	}

	double EarliestDeadline = TNumericLimits<double>::Max();
	for (const TPair<FIntVector, double>& Pair : RespawnDeadlines)
	{
		EarliestDeadline = FMath::Min(EarliestDeadline, Pair.Value);
	}

	ArmedRespawnDeadline = EarliestDeadline;
	const float Delay = static_cast<float>(FMath::Max(0.001, EarliestDeadline - World->GetTimeSeconds()));
	World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ThisClass::HandleRespawnTimer, Delay, false);
}

void URpgHarvestInstanceStockComponent::HandleRespawnTimer()
{
	ArmedRespawnDeadline = TNumericLimits<double>::Max();
	const UWorld* World = GetWorld();
	if (!World || !HasStockAuthority())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	TArray<FIntVector> DueKeys;
	for (const TPair<FIntVector, double>& Pair : RespawnDeadlines)
	{
		if (Pair.Value <= Now + UE_KINDA_SMALL_NUMBER)
		{
			DueKeys.Add(Pair.Key);
		}
	}

	for (const FIntVector& Key : DueKeys)
	{
		RestoreStock(Key);
	}
	ArmNextRespawnTimer();
}
