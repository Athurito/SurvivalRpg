#include "Harvesting/RpgHarvestPersistenceComponent.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestPersistenceComponent)

namespace RpgHarvestPersistence
{
	const FName FeatureId(TEXT("Harvesting"));

	bool IsValidSavedStock(const FRpgHarvestSavedStock& Stock)
	{
		return Stock.Revision >= 0 && Stock.HarvestedSections >= 0 && FMath::IsFinite(Stock.RespawnSeconds);
	}

	/**
	 * Returns whether Actor was loaded with its map or a streamed cell. The level sets this flag before the actor
	 * begins play; actors spawned at runtime never have it. Renewable areas, such as portal realms, start over
	 * instead of keeping their stock.
	 */
	bool IsLoadedWithMap(const AActor& Actor)
	{
		return Actor.bNetStartup && !FRpgHarvestStockRules::IsInRenewableArea(Actor);
	}
}

URpgHarvestPersistenceComponent::URpgHarvestPersistenceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

URpgHarvestPersistenceComponent* URpgHarvestPersistenceComponent::FindForWorld(const UWorld* World)
{
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<URpgHarvestPersistenceComponent>() : nullptr;
}

FName URpgHarvestPersistenceComponent::MakeMapId(const UWorld& World)
{
	return FName(*UWorld::RemovePIEPrefix(World.GetPackage()->GetName()));
}

FName URpgHarvestPersistenceComponent::MakeNodeId(const AActor& Actor)
{
	const ULevel* Level = Actor.GetLevel();
	const UWorld* World = Actor.GetWorld();
	if (!RpgHarvestPersistence::IsLoadedWithMap(Actor) || !Level || !World)
	{
		return NAME_None;
	}

	// Actor names are unique within a level, and World Partition keeps them unique within the whole world. A
	// streamed sublevel of a classic world adds its own package.
	if (Level->IsPersistentLevel() || World->IsPartitionedWorld())
	{
		return Actor.GetFName();
	}
	return FName(*FString::Printf(
		TEXT("%s.%s"),
		*UWorld::RemovePIEPrefix(Level->GetPackage()->GetName()),
		*Actor.GetName()));
}

void URpgHarvestPersistenceComponent::RegisterNode(URpgHarvestableComponent& Node)
{
	const AActor* OwningActor = Node.GetOwner();
	const FName NodeId = OwningActor ? MakeNodeId(*OwningActor) : NAME_None;
	if (!HasPersistenceAuthority() || NodeId.IsNone())
	{
		return;
	}

	const URpgHarvestableComponent* Registered = LiveNodes.FindRef(NodeId).Get();
	if (Registered && Registered != &Node)
	{
		UE_LOG(
			LogRpgHarvesting,
			Warning,
			TEXT("%s shares the save id %s with %s; its stock is not saved."),
			*Node.GetPathName(),
			*NodeId.ToString(),
			*Registered->GetPathName());
		return;
	}

	LiveNodes.Add(NodeId, &Node);
	if (NodeRecords.Contains(NodeId))
	{
		ApplyNodeRecord(Node, NodeId);
	}
	else
	{
		RecordNode(Node);
	}
}

void URpgHarvestPersistenceComponent::UnregisterNode(const URpgHarvestableComponent& Node)
{
	const AActor* OwningActor = Node.GetOwner();
	const FName NodeId = OwningActor ? MakeNodeId(*OwningActor) : NAME_None;
	if (NodeId.IsNone() || LiveNodes.FindRef(NodeId).Get() != &Node)
	{
		return;
	}

	// The node still holds its respawn timer here; keep its stock while it is streamed out.
	RecordNode(Node);
	LiveNodes.Remove(NodeId);
}

void URpgHarvestPersistenceComponent::RecordNode(const URpgHarvestableComponent& Node)
{
	const AActor* OwningActor = Node.GetOwner();
	const FName NodeId = OwningActor ? MakeNodeId(*OwningActor) : NAME_None;
	if (!HasPersistenceAuthority() || NodeId.IsNone() || LiveNodes.FindRef(NodeId).Get() != &Node)
	{
		return;
	}
	SetNodeRecord(NodeId, Node.ExportSavedStock());
}

void URpgHarvestPersistenceComponent::RecordInstance(
	const URpgHarvestableInstancesComponent& Instances,
	const URpgHarvestInstanceStockComponent& Stock,
	const FIntVector& Key)
{
	// Instances spawned at runtime have no authored place in the map, so their stock stays session-scoped.
	const AActor* OwningActor = Instances.GetOwner();
	if (!HasPersistenceAuthority() || !OwningActor || !RpgHarvestPersistence::IsLoadedWithMap(*OwningActor))
	{
		return;
	}

	FRpgHarvestSavedStock Saved;
	Stock.ExportSavedStock(Key, Saved);
	SetInstanceRecord(Key, Saved);
}

void URpgHarvestPersistenceComponent::ForgetInstance(const FIntVector& Key)
{
	if (HasPersistenceAuthority() && InstanceRecords.Remove(Key) > 0)
	{
		MarkSaveDirty();
	}
}

void URpgHarvestPersistenceComponent::ApplyInstanceRecords(URpgHarvestInstanceStockComponent& Stock)
{
	if (!HasPersistenceAuthority())
	{
		return;
	}

	TArray<FIntVector> RespawnedKeys;
	{
		TGuardValue<bool> ApplyGuard(bApplyingSavedState, true);
		for (const TPair<FIntVector, FRecord>& Pair : InstanceRecords)
		{
			// An instance whose respawn came due while it was not applied starts over with its authored stock.
			if (IsRespawnDue(Pair.Value))
			{
				RespawnedKeys.Add(Pair.Key);
				Stock.ApplySavedStock(Pair.Key, FRpgHarvestSavedStock());
				continue;
			}
			Stock.ApplySavedStock(Pair.Key, MakeSavedStock(Pair.Value));
		}
	}
	for (const FIntVector& Key : RespawnedKeys)
	{
		InstanceRecords.Remove(Key);
	}
}

FName URpgHarvestPersistenceComponent::GetWorldSaveFeatureId() const
{
	return RpgHarvestPersistence::FeatureId;
}

bool URpgHarvestPersistenceComponent::CaptureWorldSaveData(FRpgWorldFeatureSaveData& OutData)
{
	// Re-read every loaded node, so the save matches the authoritative stock at this moment.
	{
		TGuardValue<bool> ApplyGuard(bApplyingSavedState, true);
		for (const TPair<FName, TWeakObjectPtr<URpgHarvestableComponent>>& Pair : LiveNodes)
		{
			if (const URpgHarvestableComponent* Node = Pair.Value.Get())
			{
				RecordNode(*Node);
			}
		}
	}

	FRpgHarvestSaveData Data;
	Data.Maps = OtherMaps;
	if (!MapId.IsNone())
	{
		FRpgHarvestSavedMap Current;
		for (const TPair<FName, FRecord>& Pair : NodeRecords)
		{
			// A streamed-out resource whose respawn came due is back to its authored stock.
			if (!IsRespawnDue(Pair.Value))
			{
				FRpgHarvestSavedNode& Saved = Current.Nodes.AddDefaulted_GetRef();
				Saved.NodeId = Pair.Key;
				Saved.Stock = MakeSavedStock(Pair.Value);
			}
		}
		for (const TPair<FIntVector, FRecord>& Pair : InstanceRecords)
		{
			if (!IsRespawnDue(Pair.Value))
			{
				FRpgHarvestSavedInstance& Saved = Current.Instances.AddDefaulted_GetRef();
				Saved.KeyX = Pair.Key.X;
				Saved.KeyY = Pair.Key.Y;
				Saved.KeyZ = Pair.Key.Z;
				Saved.Stock = MakeSavedStock(Pair.Value);
			}
		}

		Current.Nodes.Sort([](const FRpgHarvestSavedNode& A, const FRpgHarvestSavedNode& B)
		{
			return A.NodeId.LexicalLess(B.NodeId);
		});
		Current.Instances.Sort([](const FRpgHarvestSavedInstance& A, const FRpgHarvestSavedInstance& B)
		{
			return A.KeyX != B.KeyX ? A.KeyX < B.KeyX : (A.KeyY != B.KeyY ? A.KeyY < B.KeyY : A.KeyZ < B.KeyZ);
		});
		if (Current.Nodes.IsEmpty() && Current.Instances.IsEmpty())
		{
			Data.Maps.Remove(MapId);
		}
		else
		{
			Data.Maps.Add(MapId, MoveTemp(Current));
		}
	}
	return WritePayload(Data, OutData);
}

bool URpgHarvestPersistenceComponent::RestoreWorldSaveData(const FRpgWorldFeatureSaveData* SavedData)
{
	if (!HasPersistenceAuthority())
	{
		return false;
	}

	FRpgHarvestSaveData Data;
	if (SavedData && !ReadPayload(*SavedData, Data))
	{
		UE_LOG(LogRpgHarvesting, Error, TEXT("Saved harvest state has schema %d or does not deserialize."), SavedData->SchemaVersion);
		return false;
	}
	for (const TPair<FName, FRpgHarvestSavedMap>& MapPair : Data.Maps)
	{
		const bool bValidNodes = !MapPair.Value.Nodes.ContainsByPredicate([](const FRpgHarvestSavedNode& Node)
		{
			return Node.NodeId.IsNone() || !RpgHarvestPersistence::IsValidSavedStock(Node.Stock);
		});
		const bool bValidInstances = !MapPair.Value.Instances.ContainsByPredicate([](const FRpgHarvestSavedInstance& Instance)
		{
			return !RpgHarvestPersistence::IsValidSavedStock(Instance.Stock);
		});
		if (MapPair.Key.IsNone() || !bValidNodes || !bValidInstances)
		{
			UE_LOG(LogRpgHarvesting, Error, TEXT("Saved harvest state of map %s is invalid."), *MapPair.Key.ToString());
			return false;
		}
	}

	ResetAppliedRecords();
	OtherMaps = MoveTemp(Data.Maps);
	FRpgHarvestSavedMap Current;
	OtherMaps.RemoveAndCopyValue(MapId, Current);
	for (const FRpgHarvestSavedNode& Saved : Current.Nodes)
	{
		if (!Saved.Stock.IsPristine())
		{
			NodeRecords.Add(Saved.NodeId, MakeRecord(Saved.Stock));
		}
	}
	for (const FRpgHarvestSavedInstance& Saved : Current.Instances)
	{
		if (!Saved.Stock.IsPristine())
		{
			InstanceRecords.Add(Saved.GetKey(), MakeRecord(Saved.Stock));
		}
	}

	// Loaded resources take their saved stock now; others take it when they begin play.
	TArray<FName> NodeIds;
	NodeRecords.GetKeys(NodeIds);
	for (const FName NodeId : NodeIds)
	{
		if (URpgHarvestableComponent* Node = LiveNodes.FindRef(NodeId).Get())
		{
			ApplyNodeRecord(*Node, NodeId);
		}
	}
	if (URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(GetWorld()))
	{
		ApplyInstanceRecords(*Stock);
	}
	return true;
}

bool URpgHarvestPersistenceComponent::WritePayload(const FRpgHarvestSaveData& Data, FRpgWorldFeatureSaveData& OutData)
{
	OutData.SchemaVersion = CurrentSchemaVersion;
	OutData.Payload.Reset();
	FMemoryWriter Writer(OutData.Payload, true);
	FObjectAndNameAsStringProxyArchive Archive(Writer, false);
	Archive.ArIsSaveGame = true;
	FRpgHarvestSaveData::StaticStruct()->SerializeItem(Archive, const_cast<FRpgHarvestSaveData*>(&Data), nullptr);
	return !Archive.IsError();
}

bool URpgHarvestPersistenceComponent::ReadPayload(const FRpgWorldFeatureSaveData& Data, FRpgHarvestSaveData& OutData)
{
	OutData = FRpgHarvestSaveData();
	if (Data.SchemaVersion != CurrentSchemaVersion || Data.Payload.IsEmpty())
	{
		return false;
	}
	FMemoryReader Reader(Data.Payload, true);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	Archive.ArIsSaveGame = true;
	FRpgHarvestSaveData::StaticStruct()->SerializeItem(Archive, &OutData, nullptr);
	return !Archive.IsError() && !Reader.IsError();
}

void URpgHarvestPersistenceComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!HasPersistenceAuthority() || !World)
	{
		return;
	}
	MapId = MakeMapId(*World);

	// Nodes that began play before the GameFeature added this component register now; later ones register
	// themselves on BeginPlay, for example when World Partition streams them in.
	for (TObjectIterator<URpgHarvestableComponent> It; It; ++It)
	{
		URpgHarvestableComponent* Node = *It;
		if (IsValid(Node) && Node->GetWorld() == World && Node->HasBegunPlay())
		{
			RegisterNode(*Node);
		}
	}

	if (ARpgGameModeBase* GameMode = GetGameMode())
	{
		GameMode->RegisterWorldSaveParticipant(this);
	}
}

void URpgHarvestPersistenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARpgGameModeBase* GameMode = GetGameMode())
	{
		GameMode->UnregisterWorldSaveParticipant(this);
	}
	LiveNodes.Reset();
	NodeRecords.Reset();
	InstanceRecords.Reset();
	OtherMaps.Reset();
	Super::EndPlay(EndPlayReason);
}

bool URpgHarvestPersistenceComponent::HasPersistenceAuthority() const
{
	const AActor* OwningActor = GetOwner();
	return OwningActor && OwningActor->HasAuthority();
}

double URpgHarvestPersistenceComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

URpgHarvestPersistenceComponent::FRecord URpgHarvestPersistenceComponent::MakeRecord(const FRpgHarvestSavedStock& Stock) const
{
	FRecord Record;
	Record.Stock = Stock;
	Record.RespawnDeadline = !Stock.bActive && Stock.RespawnSeconds >= 0.0f
		? GetWorldTime() + Stock.RespawnSeconds
		: -1.0;
	return Record;
}

FRpgHarvestSavedStock URpgHarvestPersistenceComponent::MakeSavedStock(const FRecord& Record) const
{
	FRpgHarvestSavedStock Stock = Record.Stock;
	Stock.RespawnSeconds = Record.RespawnDeadline >= 0.0
		? static_cast<float>(FMath::Max(0.0, Record.RespawnDeadline - GetWorldTime()))
		: -1.0f;
	return Stock;
}

bool URpgHarvestPersistenceComponent::IsRespawnDue(const FRecord& Record) const
{
	return !Record.Stock.bActive && Record.RespawnDeadline >= 0.0 && Record.RespawnDeadline <= GetWorldTime();
}

void URpgHarvestPersistenceComponent::SetNodeRecord(const FName NodeId, const FRpgHarvestSavedStock& Stock)
{
	// Pristine resources need no record, which keeps untouched resources free.
	if (Stock.IsPristine())
	{
		if (NodeRecords.Remove(NodeId) > 0)
		{
			MarkSaveDirty();
		}
		return;
	}
	NodeRecords.Add(NodeId, MakeRecord(Stock));
	MarkSaveDirty();
}

void URpgHarvestPersistenceComponent::SetInstanceRecord(const FIntVector& Key, const FRpgHarvestSavedStock& Stock)
{
	if (Stock.IsPristine())
	{
		if (InstanceRecords.Remove(Key) > 0)
		{
			MarkSaveDirty();
		}
		return;
	}
	InstanceRecords.Add(Key, MakeRecord(Stock));
	MarkSaveDirty();
}

void URpgHarvestPersistenceComponent::ApplyNodeRecord(URpgHarvestableComponent& Node, const FName NodeId)
{
	const FRecord* Record = NodeRecords.Find(NodeId);
	if (!Record)
	{
		return;
	}

	// A node whose respawn came due while it was streamed out starts over with its authored stock.
	const FRpgHarvestSavedStock Saved = IsRespawnDue(*Record) ? FRpgHarvestSavedStock() : MakeSavedStock(*Record);
	TGuardValue<bool> ApplyGuard(bApplyingSavedState, true);
	if (!Node.ApplySavedStock(Saved))
	{
		UE_LOG(LogRpgHarvesting, Warning, TEXT("%s could not apply its saved stock."), *Node.GetPathName());
	}
	// The node reported the applied stock back, which also dropped a record that came due.
}

void URpgHarvestPersistenceComponent::ResetAppliedRecords()
{
	TGuardValue<bool> ApplyGuard(bApplyingSavedState, true);
	TArray<FName> NodeIds;
	NodeRecords.GetKeys(NodeIds);
	TArray<FIntVector> InstanceKeys;
	InstanceRecords.GetKeys(InstanceKeys);
	NodeRecords.Reset();
	InstanceRecords.Reset();

	for (const FName NodeId : NodeIds)
	{
		if (URpgHarvestableComponent* Node = LiveNodes.FindRef(NodeId).Get())
		{
			Node->ApplySavedStock(FRpgHarvestSavedStock());
		}
	}
	if (URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(GetWorld()))
	{
		for (const FIntVector& Key : InstanceKeys)
		{
			Stock->ApplySavedStock(Key, FRpgHarvestSavedStock());
		}
	}
}

void URpgHarvestPersistenceComponent::MarkSaveDirty() const
{
	if (bApplyingSavedState)
	{
		return;
	}
	if (ARpgGameModeBase* GameMode = GetGameMode())
	{
		GameMode->MarkWorldFeatureSaveDirty(this);
	}
}

ARpgGameModeBase* URpgHarvestPersistenceComponent::GetGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ARpgGameModeBase>() : nullptr;
}
