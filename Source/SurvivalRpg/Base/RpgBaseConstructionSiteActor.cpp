#include "RpgBaseConstructionSiteActor.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseCampActor.h"
#include "RpgBaseStorageComponent.h"
#include "RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

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
	if (!BaseCamp || !BuildableDefinition || bFinished || !RequestingActor)
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

	return FVector::DistSquared(GetActorLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(ContributionRadius);
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

	CostState->ContributedCount += ContributionCount;
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
	if (!HasAuthority() || !CanActorContribute(RequestingActor))
	{
		return false;
	}

	bool bContributedAny = false;
	const TArray<FRpgBaseConstructionResourceState> CostsSnapshot = ConstructionCosts;
	for (const FRpgBaseConstructionResourceState& CostState : CostsSnapshot)
	{
		const int32 RemainingCost = GetRemainingCostForDefinition(CostState.ItemDefinition);
		if (RemainingCost > 0)
		{
			bContributedAny |= ContributeMaterial(RequestingActor, CostState.ItemDefinition, RemainingCost, bAllowBaseStorage);
		}
	}

	return bContributedAny;
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
	if (!RequestingActor)
	{
		return nullptr;
	}

	if (const APawn* RequestingPawn = Cast<APawn>(RequestingActor))
	{
		if (const ARpgPlayerState* PlayerState = RequestingPawn->GetPlayerState<ARpgPlayerState>())
		{
			return PlayerState->GetInventoryManagerComponent();
		}

		if (const AController* Controller = RequestingPawn->GetController())
		{
			if (const ARpgPlayerState* PlayerState = Controller->GetPlayerState<ARpgPlayerState>())
			{
				return PlayerState->GetInventoryManagerComponent();
			}
		}
	}

	if (const AController* Controller = Cast<AController>(RequestingActor))
	{
		if (const ARpgPlayerState* PlayerState = Controller->GetPlayerState<ARpgPlayerState>())
		{
			return PlayerState->GetInventoryManagerComponent();
		}
	}

	return RequestingActor->FindComponentByClass<URpgInventoryManagerComponent>();
}

URpgBaseStorageComponent* ARpgBaseConstructionSiteActor::GetBaseStorage() const
{
	return BaseCamp ? BaseCamp->GetBaseStorageComponent() : nullptr;
}

bool ARpgBaseConstructionSiteActor::ConsumeFromPlayer(URpgInventoryManagerComponent* PlayerInventory, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	return Count <= 0 || (PlayerInventory && PlayerInventory->ConsumeItemsByDefinition(ItemDefinition, Count));
}

bool ARpgBaseConstructionSiteActor::ConsumeFromBase(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	URpgBaseStorageComponent* BaseStorage = GetBaseStorage();
	return Count <= 0 || (BaseStorage && BaseStorage->WithdrawResource(ItemDefinition, Count));
}

bool ARpgBaseConstructionSiteActor::ConsumeContribution(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, bool bAllowBaseStorage)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory(RequestingActor);
	URpgBaseStorageComponent* BaseStorage = bAllowBaseStorage ? GetBaseStorage() : nullptr;
	const int32 AvailableInPlayer = PlayerInventory ? PlayerInventory->GetTotalItemCountByDefinition(ItemDefinition) : 0;
	const int32 AvailableInBase = BaseStorage ? BaseStorage->GetResourceCount(ItemDefinition) : 0;

	auto ConsumePlayer = [&](int32& RemainingCount)
	{
		const int32 CountToConsume = FMath::Min(AvailableInPlayer, RemainingCount);
		if (!ConsumeFromPlayer(PlayerInventory, ItemDefinition, CountToConsume))
		{
			return false;
		}
		RemainingCount -= CountToConsume;
		return true;
	};

	auto ConsumeBase = [&](int32& RemainingCount)
	{
		const int32 CountToConsume = FMath::Min(AvailableInBase, RemainingCount);
		if (!ConsumeFromBase(ItemDefinition, CountToConsume))
		{
			return false;
		}
		RemainingCount -= CountToConsume;
		return true;
	};

	int32 RemainingCount = Count;
	switch (ContributionConsumeOrder)
	{
	case ERpgBaseConstructionResourceConsumeOrder::PlayerThenBase:
		return ConsumePlayer(RemainingCount) && (bAllowBaseStorage ? ConsumeBase(RemainingCount) : true) && RemainingCount <= 0;

	case ERpgBaseConstructionResourceConsumeOrder::BaseThenPlayer:
		return (bAllowBaseStorage ? ConsumeBase(RemainingCount) : true) && ConsumePlayer(RemainingCount) && RemainingCount <= 0;

	case ERpgBaseConstructionResourceConsumeOrder::PlayerOnly:
		return ConsumePlayer(RemainingCount) && RemainingCount <= 0;

	case ERpgBaseConstructionResourceConsumeOrder::BaseOnly:
		return bAllowBaseStorage && ConsumeBase(RemainingCount) && RemainingCount <= 0;
	}

	return false;
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

	if (URpgCraftingStationComponent* CraftingStation = SpawnedActor->FindComponentByClass<URpgCraftingStationComponent>())
	{
		CraftingStation->SetLinkedBaseCamp(BaseCamp);
	}
}
