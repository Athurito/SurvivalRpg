#include "RpgBaseConstructionSiteActor.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseCampActor.h"
#include "RpgBaseStorageComponent.h"
#include "RpgStorageAccessRules.h"
#include "RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseConstructionSiteActor)

DEFINE_LOG_CATEGORY_STATIC(LogRpgBaseConstructionSite, Log, All);

ARpgBaseConstructionSiteActor::ARpgBaseConstructionSiteActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicatingMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ARpgBaseConstructionSiteActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, BaseCamp);
	DOREPLIFETIME(ThisClass, BuildableDefinition);
	DOREPLIFETIME(ThisClass, ConstructionCosts);
	DOREPLIFETIME(ThisClass, bFinished);
}

void ARpgBaseConstructionSiteActor::InitializeConstructionSite(ARpgBaseCampActor* InBaseCamp, URpgBaseBuildableDefinition* InBuildableDefinition)
{
	if (!HasAuthority() || !InBaseCamp || !InBuildableDefinition || bFinished)
	{
		UE_LOG(LogRpgBaseConstructionSite, Warning, TEXT("Initialize construction site failed: Site=%s Authority=%s BaseCamp=%s Buildable=%s Finished=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(InBaseCamp),
			*GetNameSafe(InBuildableDefinition),
			bFinished ? TEXT("true") : TEXT("false"));
		return;
	}

	BaseCamp = InBaseCamp;
	BuildableDefinition = InBuildableDefinition;
	ConstructionCosts.Reset();

	for (const FRpgBaseBuildResourceCost& Cost : BuildableDefinition->BuildCosts)
	{
		if (!Cost.ItemDefinition || Cost.Count <= 0)
		{
			continue;
		}

		if (FRpgBaseConstructionResourceState* Existing = FindCostState(Cost.ItemDefinition))
		{
			const int64 TotalRequired = static_cast<int64>(Existing->RequiredCount) + Cost.Count;
			if (TotalRequired > MAX_int32)
			{
				ConstructionCosts.Reset();
				BuildableDefinition = nullptr;
				return;
			}
			Existing->RequiredCount = static_cast<int32>(TotalRequired);
			continue;
		}
		FRpgBaseConstructionResourceState& NewState = ConstructionCosts.AddDefaulted_GetRef();
		NewState.ItemDefinition = Cost.ItemDefinition;
		NewState.RequiredCount = Cost.Count;
		NewState.ContributedCount = 0;
	}

	ForceNetUpdate();
	UE_LOG(LogRpgBaseConstructionSite, Log, TEXT("Initialized construction site: Site=%s BaseCamp=%s Buildable=%s CostRows=%d Remaining=%d"),
		*GetNameSafe(this),
		*GetNameSafe(BaseCamp),
		*GetNameSafe(BuildableDefinition),
		ConstructionCosts.Num(),
		GetTotalRemainingCost());
	HandleProgressChanged();
}

int32 ARpgBaseConstructionSiteActor::GetRemainingCostForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const FRpgBaseConstructionResourceState* CostState = FindCostState(ItemDefinition);
	return CostState ? FMath::Max(0, CostState->RequiredCount - CostState->ContributedCount) : 0;
}

int32 ARpgBaseConstructionSiteActor::GetTotalRemainingCost() const
{
	int32 RemainingCost = 0;
	for (const FRpgBaseConstructionResourceState& CostState : ConstructionCosts)
	{
		RemainingCost += FMath::Max(0, CostState.RequiredCount - CostState.ContributedCount);
	}
	return RemainingCost;
}

bool ARpgBaseConstructionSiteActor::IsConstructionComplete() const
{
	return BuildableDefinition && GetTotalRemainingCost() <= 0;
}

bool ARpgBaseConstructionSiteActor::CanActorContribute(const AActor* RequestingActor) const
{
	if (!BaseCamp || !BuildableDefinition || bFinished || !RequestingActor || RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), GetActorLocation()) != BaseCamp)
	{
		return false;
	}

	const APawn* RequestingPawn = Cast<APawn>(RequestingActor);
	const AController* RequestingController = Cast<AController>(RequestingActor);
	if (!RequestingController && RequestingPawn)
	{
		RequestingController = RequestingPawn->GetController();
	}

	if (!RequestingController || !RequestingController->IsPlayerController())
	{
		return false;
	}

	if (ContributionRadius <= 0.0f)
	{
		return true;
	}

	const AActor* Avatar = RequestingController->GetPawn() ? RequestingController->GetPawn() : RequestingActor;
	return FVector::DistSquared(GetActorLocation(), Avatar->GetActorLocation()) <= FMath::Square(ContributionRadius);
}

bool ARpgBaseConstructionSiteActor::ContributeMaterial(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, bool bAllowBaseStorage)
{
	if (!HasAuthority() || !CanActorContribute(RequestingActor) || !ItemDefinition || Count <= 0)
	{
		UE_LOG(LogRpgBaseConstructionSite, Warning, TEXT("Contribute material failed: invalid request. Site=%s Authority=%s Actor=%s ItemDef=%s Count=%d CanContribute=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(RequestingActor),
			*GetNameSafe(ItemDefinition),
			Count,
			CanActorContribute(RequestingActor) ? TEXT("true") : TEXT("false"));
		return false;
	}

	FRpgBaseConstructionResourceState* CostState = FindCostState(ItemDefinition);
	if (!CostState)
	{
		UE_LOG(LogRpgBaseConstructionSite, Warning, TEXT("Contribute material failed: item is not part of construction cost. Site=%s ItemDef=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ItemDefinition));
		return false;
	}

	const int32 RemainingCost = FMath::Max(0, CostState->RequiredCount - CostState->ContributedCount);
	const int32 ContributionCount = FMath::Min(Count, RemainingCost);
	if (ContributionCount <= 0 || !ConsumeContribution(RequestingActor, ItemDefinition, ContributionCount, bAllowBaseStorage))
	{
		UE_LOG(LogRpgBaseConstructionSite, Warning, TEXT("Contribute material failed: could not consume resources. Site=%s Actor=%s ItemDef=%s Requested=%d Contribution=%d Remaining=%d AllowBase=%s"),
			*GetNameSafe(this),
			*GetNameSafe(RequestingActor),
			*GetNameSafe(ItemDefinition),
			Count,
			ContributionCount,
			RemainingCost,
			bAllowBaseStorage ? TEXT("true") : TEXT("false"));
		return false;
	}

	UE_LOG(LogRpgBaseConstructionSite, Log, TEXT("Contributed construction material: Site=%s ItemDef=%s Added=%d Progress=%d/%d TotalRemaining=%d"),
		*GetNameSafe(this),
		*GetNameSafe(ItemDefinition),
		ContributionCount,
		CostState->ContributedCount,
		CostState->RequiredCount,
		GetTotalRemainingCost());
	HandleProgressChanged();
	return true;
}

bool ARpgBaseConstructionSiteActor::ContributeAllResources(AActor* RequestingActor, bool bAllowBaseStorage)
{
	if (!HasAuthority() || !CanActorContribute(RequestingActor)) { return false; }
	TArray<FRpgCraftingResourceCost> Costs;
	for (const FRpgBaseConstructionResourceState& State : ConstructionCosts)
	{
		if (State.ContributedCount >= State.RequiredCount) { continue; }
		FRpgCraftingResourceCost& Cost = Costs.AddDefaulted_GetRef();
		Cost.ItemDefinition = State.ItemDefinition;
		Cost.Count = State.RequiredCount - State.ContributedCount;
	}
	if (Costs.IsEmpty()) { return false; }
	TArray<URpgInventoryManagerComponent*> Sources;
	if (bAllowBaseStorage) { RpgStorageAccessRules::ResolveStorageSources(GetWorld(), GetActorLocation(), 0.0f, Sources); }
	TArray<FRpgInventoryBatchOperation> Operations;
	TArray<FRpgCraftingRefundEntry> Credits;
	if (!URpgCraftingStationComponent::BuildResourceConsumptionPlan(RequestingActor, Sources, Costs, 1, Operations, Credits) || Operations.IsEmpty()) { return false; }
	if (!Operations[0].SourceInventory->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, [this]()
	{
		for (FRpgBaseConstructionResourceState& State : ConstructionCosts) { State.ContributedCount = State.RequiredCount; }
		ForceNetUpdate();
	}, MakeContributionRevalidator(RequestingActor, bAllowBaseStorage)).IsSuccess()) { return false; }
	HandleProgressChanged();
	return true;
}

AActor* ARpgBaseConstructionSiteActor::FinishConstruction()
{
	if (!HasAuthority() || bFinished || !IsConstructionComplete() || !BuildableDefinition || !BuildableDefinition->BuildActorClass)
	{
		UE_LOG(LogRpgBaseConstructionSite, Warning, TEXT("Finish construction failed: Site=%s Authority=%s Finished=%s Complete=%s Buildable=%s BuildActorClass=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			bFinished ? TEXT("true") : TEXT("false"),
			IsConstructionComplete() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(BuildableDefinition),
			BuildableDefinition ? *GetNameSafe(BuildableDefinition->BuildActorClass) : TEXT("None"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = BaseCamp;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(BuildableDefinition->BuildActorClass, GetActorTransform(), SpawnParams);
	if (!SpawnedActor)
	{
		UE_LOG(LogRpgBaseConstructionSite, Warning, TEXT("Finish construction failed: final actor spawn failed. Site=%s BuildActorClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(BuildableDefinition->BuildActorClass));
		return nullptr;
	}

	LinkSpawnedActorToBase(SpawnedActor);
	UE_LOG(LogRpgBaseConstructionSite, Log, TEXT("Finished construction: Site=%s SpawnedActor=%s Buildable=%s BaseCamp=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedActor),
		*GetNameSafe(BuildableDefinition),
		*GetNameSafe(BaseCamp));
	bFinished = true;
	ForceNetUpdate();
	HandleProgressChanged();

	if (bDestroyWhenFinished)
	{
		Destroy();
	}

	return SpawnedActor;
}

void ARpgBaseConstructionSiteActor::OnRep_ConstructionState()
{
	HandleProgressChanged();
}

URpgInventoryManagerComponent* ARpgBaseConstructionSiteActor::FindPlayerInventory(const AActor* RequestingActor) const
{
	return URpgCraftingStationComponent::FindRequestingPlayerInventory(RequestingActor);
}

URpgBaseStorageComponent* ARpgBaseConstructionSiteActor::GetBaseStorage() const
{
	return BaseCamp ? BaseCamp->GetBaseStorageComponent() : nullptr;
}





TFunction<bool()> ARpgBaseConstructionSiteActor::MakeContributionRevalidator(AActor* RequestingActor, bool bAllowBaseStorage) const
{
	const TWeakObjectPtr<const ARpgBaseConstructionSiteActor> Site(this);
	const TWeakObjectPtr<AActor> Requester(RequestingActor);
	const FTransform Transform = GetActorTransform();
	const FVector BaseCenter = BaseCamp ? BaseCamp->GetActorLocation() : FVector::ZeroVector;
	const float Radius = BaseCamp ? BaseCamp->GetBuildRadius() : 0.0f;
	TArray<URpgInventoryManagerComponent*> Sources;
	if (bAllowBaseStorage) { RpgStorageAccessRules::ResolveStorageSources(GetWorld(), GetActorLocation(), 0.0f, Sources); }
	return [Site, Requester, Transform, BaseCenter, Radius, Sources, bAllowBaseStorage]()
	{
		if (!Site.IsValid() || !Requester.IsValid() || !Site->CanActorContribute(Requester.Get()) ||
			!Site->GetActorTransform().Equals(Transform) || !Site->BaseCamp ||
			Site->BaseCamp->GetActorLocation() != BaseCenter || Site->BaseCamp->GetBuildRadius() != Radius) { return false; }
		TArray<URpgInventoryManagerComponent*> CurrentSources;
		if (bAllowBaseStorage) { RpgStorageAccessRules::ResolveStorageSources(Site->GetWorld(), Site->GetActorLocation(), 0.0f, CurrentSources); }
		return CurrentSources == Sources;
	};
}

bool ARpgBaseConstructionSiteActor::ConsumeContribution(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, bool bAllowBaseStorage)
{
	TArray<URpgInventoryManagerComponent*> Sources;
	if (bAllowBaseStorage) { RpgStorageAccessRules::ResolveStorageSources(GetWorld(), GetActorLocation(), 0.0f, Sources); }
	FRpgCraftingResourceCost Cost;
	Cost.ItemDefinition = ItemDefinition;
	Cost.Count = Count;
	TArray<FRpgInventoryBatchOperation> Operations;
	TArray<FRpgCraftingRefundEntry> Credits;
	if (!URpgCraftingStationComponent::BuildResourceConsumptionPlan(RequestingActor, Sources, { Cost }, 1, Operations, Credits) || Operations.IsEmpty()) { return false; }
	return Operations[0].SourceInventory->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, [this, ItemDefinition, Count]()
	{
		if (FRpgBaseConstructionResourceState* State = FindCostState(ItemDefinition)) { State->ContributedCount += Count; }
		ForceNetUpdate();
	}, MakeContributionRevalidator(RequestingActor, bAllowBaseStorage)).IsSuccess();
}

FRpgBaseConstructionResourceState* ARpgBaseConstructionSiteActor::FindCostState(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	return ConstructionCosts.FindByPredicate([ItemDefinition](const FRpgBaseConstructionResourceState& CostState)
	{
		return CostState.ItemDefinition == ItemDefinition;
	});
}

const FRpgBaseConstructionResourceState* ARpgBaseConstructionSiteActor::FindCostState(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	return ConstructionCosts.FindByPredicate([ItemDefinition](const FRpgBaseConstructionResourceState& CostState)
	{
		return CostState.ItemDefinition == ItemDefinition;
	});
}

void ARpgBaseConstructionSiteActor::HandleProgressChanged()
{
	OnConstructionSiteChanged.Broadcast(this);

	if (HasAuthority() && bAutoFinishWhenComplete && !bFinished && IsConstructionComplete())
	{
		FinishConstruction();
	}
}

void ARpgBaseConstructionSiteActor::LinkSpawnedActorToBase(AActor* SpawnedActor) const
{
	if (!SpawnedActor || !BaseCamp)
	{
		return;
	}

	if (URpgBaseStorageStationComponent* StorageStation = SpawnedActor->FindComponentByClass<URpgBaseStorageStationComponent>())
	{
		StorageStation->SetLinkedBaseCamp(BaseCamp);
	}

	if (URpgInventoryContainerComponent* Container = SpawnedActor->FindComponentByClass<URpgInventoryContainerComponent>())
	{
		Container->EnsurePersistentContainerId();
		Container->SetResolvedBaseId(BaseCamp->GetBaseId());
		Container->SetRuntimeBuilt(true);
	}

	if (URpgCraftingStationComponent* CraftingStation = SpawnedActor->FindComponentByClass<URpgCraftingStationComponent>())
	{
		CraftingStation->SetLinkedBaseCamp(BaseCamp);
	}
}
