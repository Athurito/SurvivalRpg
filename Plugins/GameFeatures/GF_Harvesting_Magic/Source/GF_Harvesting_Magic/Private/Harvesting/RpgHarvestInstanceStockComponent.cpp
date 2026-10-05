#include "Harvesting/RpgHarvestInstanceStockComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Net/UnrealNetwork.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestInstanceStockComponent)

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
}

void FRpgHarvestInstanceStockList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, const int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}

	for (const int32 ArrayIndex : AddedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			const FRpgHarvestInstanceStockEntry& Entry = Entries[ArrayIndex];
			OwnerComponent->NotifyStockChanged(Entry.Key, OwnerComponent->IsInitialState(Entry));
		}
	}
}

void FRpgHarvestInstanceStockList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, const int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}

	for (const int32 ArrayIndex : ChangedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			const FRpgHarvestInstanceStockEntry& Entry = Entries[ArrayIndex];
			OwnerComponent->NotifyStockChanged(Entry.Key, OwnerComponent->IsInitialState(Entry));
		}
	}
}

void FRpgHarvestInstanceStockList::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	TArray<FIntVector> RemovedKeys = MoveTemp(PendingRemovedKeys);
	PendingRemovedKeys.Reset();
	if (!OwnerComponent)
	{
		return;
	}

	for (const FIntVector& Key : RemovedKeys)
	{
		// A key removed and added again in the same update was already presented by the add.
		if (!OwnerComponent->FindEntry(Key))
		{
			OwnerComponent->NotifyStockChanged(Key, false);
		}
	}
}

URpgHarvestInstanceStockComponent::URpgHarvestInstanceStockComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Stock.OwnerComponent = this;
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

	int32 EntryIndex = FindEntryIndex(Key);
	if (EntryIndex == INDEX_NONE)
	{
		EntryIndex = Stock.Entries.AddDefaulted();
		Stock.Entries[EntryIndex].Key = Key;
	}

	FRpgHarvestInstanceStockEntry& Entry = Stock.Entries[EntryIndex];
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
			RespawnDeadlines.Add(Key, World->GetTimeSeconds() + RespawnDelaySeconds);
			ArmNextRespawnTimer();
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

	MarkStockChanged(Key);
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
	const int32 EntryIndex = FindEntryIndex(Key);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	// Restored instances match their authored state again, so they stop costing memory and bandwidth.
	Stock.Entries.RemoveAtSwap(EntryIndex);
	Stock.MarkArrayDirty();
	MarkStockChanged(Key);
	return true;
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

	TArray<FIntVector, TInlineAllocator<16>> Keys;
	Keys.Reserve(Stock.Entries.Num());
	for (const FRpgHarvestInstanceStockEntry& Entry : Stock.Entries)
	{
		Keys.Add(Entry.Key);
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

void URpgHarvestInstanceStockComponent::OnRegister()
{
	Super::OnRegister();
	Stock.OwnerComponent = this;
}

void URpgHarvestInstanceStockComponent::BeginPlay()
{
	Super::BeginPlay();
	Stock.OwnerComponent = this;

	// Representations that began play before the GameFeature added this component register now; later ones
	// register themselves on BeginPlay, for example when World Partition streams them in.
	const UWorld* World = GetWorld();
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
	RespawnDeadlines.Reset();
	RegisteredInstances.Reset();
	Super::EndPlay(EndPlayReason);
}

void URpgHarvestInstanceStockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Stock);
}

const FRpgHarvestInstanceStockEntry* URpgHarvestInstanceStockComponent::FindEntry(const FIntVector& Key) const
{
	const int32 EntryIndex = FindEntryIndex(Key);
	return EntryIndex != INDEX_NONE ? &Stock.Entries[EntryIndex] : nullptr;
}

int32 URpgHarvestInstanceStockComponent::FindEntryIndex(const FIntVector& Key) const
{
	// Entries exist only for instances whose stock currently differs from the authored state; a linear search
	// stays cheap for the expected hundreds of entries and avoids index upkeep on FastArray removals.
	return Stock.Entries.IndexOfByPredicate([&Key](const FRpgHarvestInstanceStockEntry& Entry)
	{
		return Entry.Key == Key;
	});
}

void URpgHarvestInstanceStockComponent::MarkStockChanged(const FIntVector& Key)
{
	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->ForceNetUpdate();
	}
	NotifyStockChanged(Key, false);
}

void URpgHarvestInstanceStockComponent::NotifyStockChanged(const FIntVector& Key, const bool bInitialState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestInstanceStockComponent::NotifyStockChanged);

	// Presentation events may stream representations out, so iterate over a copy.
	const TArray<TWeakObjectPtr<URpgHarvestableInstancesComponent>> Registered = RegisteredInstances;
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

void URpgHarvestInstanceStockComponent::ArmNextRespawnTimer()
{
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

	const float Delay = static_cast<float>(FMath::Max(0.001, EarliestDeadline - World->GetTimeSeconds()));
	World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ThisClass::HandleRespawnTimer, Delay, false);
}

void URpgHarvestInstanceStockComponent::HandleRespawnTimer()
{
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
