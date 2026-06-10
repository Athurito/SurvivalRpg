#include "RpgCraftingStationComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingStationComponent)

URpgCraftingStationComponent::URpgCraftingStationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationComponent::GetResourceInventories(AActor* RequestingActor) const
{
	TArray<URpgInventoryManagerComponent*> Results;

	if (RequestingActor == nullptr)
	{
		return Results;
	}

	if (URpgInventoryManagerComponent* DirectInventory = RequestingActor->FindComponentByClass<URpgInventoryManagerComponent>())
	{
		Results.AddUnique(DirectInventory);
	}

	if (const APawn* RequestingPawn = Cast<APawn>(RequestingActor))
	{
		if (APlayerState* PlayerState = RequestingPawn->GetPlayerState())
		{
			if (URpgInventoryManagerComponent* PlayerInventory = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
			{
				Results.AddUnique(PlayerInventory);
			}
		}

		if (AController* Controller = RequestingPawn->GetController())
		{
			if (APlayerState* PlayerState = Controller->PlayerState)
			{
				if (URpgInventoryManagerComponent* PlayerInventory = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
				{
					Results.AddUnique(PlayerInventory);
				}
			}
		}
	}

	const AActor* StationOwner = GetOwner();
	const UWorld* World = GetWorld();
	if (!StationOwner || !World)
	{
		return Results;
	}

	const float SearchRadiusSq = FMath::Square(StorageSearchRadius);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* CandidateActor = *It;
		if (!CandidateActor || CandidateActor == StationOwner)
		{
			continue;
		}

		URpgInventoryContainerComponent* Container = CandidateActor->FindComponentByClass<URpgInventoryContainerComponent>();
		URpgInventoryManagerComponent* Inventory = CandidateActor->FindComponentByClass<URpgInventoryManagerComponent>();
		if (!Container || !Inventory || !Container->AllowsCraftingAccess() || !Container->IsContainerAccessible())
		{
			continue;
		}

		const bool bSameStorageGroup = !StorageGroupId.IsNone() && Container->GetStorageGroupId() == StorageGroupId;
		const bool bWithinRadius = StorageSearchRadius <= 0.0f ||
			FVector::DistSquared(StationOwner->GetActorLocation(), CandidateActor->GetActorLocation()) <= SearchRadiusSq;

		if (bSameStorageGroup || bWithinRadius)
		{
			Results.AddUnique(Inventory);
		}
	}

	return Results;
}

int32 URpgCraftingStationComponent::GetAvailableResourceCount(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return 0;
	}

	int32 TotalCount = 0;
	for (URpgInventoryManagerComponent* Inventory : GetResourceInventories(RequestingActor))
	{
		if (Inventory)
		{
			TotalCount += Inventory->GetTotalItemCountByDefinition(ItemDefinition);
		}
	}

	if (const URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage())
	{
		TotalCount += BaseStorage->GetResourceCount(ItemDefinition);
	}

	return TotalCount;
}

bool URpgCraftingStationComponent::ConsumeResources(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	for (const FRpgCraftingResourceCost& RequiredItem : RequiredItems)
	{
		if (!RequiredItem.ItemDefinition || RequiredItem.Count <= 0)
		{
			return false;
		}

		if (GetAvailableResourceCount(RequestingActor, RequiredItem.ItemDefinition) < RequiredItem.Count)
		{
			return false;
		}
	}

	TArray<URpgInventoryManagerComponent*> ResourceInventories = GetResourceInventories(RequestingActor);
	for (const FRpgCraftingResourceCost& RequiredItem : RequiredItems)
	{
		int32 RemainingCount = RequiredItem.Count;
		for (URpgInventoryManagerComponent* Inventory : ResourceInventories)
		{
			if (!Inventory || RemainingCount <= 0)
			{
				break;
			}

			const int32 AvailableInInventory = Inventory->GetTotalItemCountByDefinition(RequiredItem.ItemDefinition);
			const int32 CountToConsume = FMath::Min(AvailableInInventory, RemainingCount);
			if (CountToConsume > 0 && Inventory->ConsumeItemsByDefinition(RequiredItem.ItemDefinition, CountToConsume))
			{
				RemainingCount -= CountToConsume;
			}
		}

		if (RemainingCount > 0)
		{
			if (URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage())
			{
				const int32 AvailableInBaseStorage = BaseStorage->GetResourceCount(RequiredItem.ItemDefinition);
				const int32 CountToConsume = FMath::Min(AvailableInBaseStorage, RemainingCount);
				if (CountToConsume > 0 && BaseStorage->WithdrawResource(RequiredItem.ItemDefinition, CountToConsume))
				{
					RemainingCount -= CountToConsume;
				}
			}
		}

		if (RemainingCount > 0)
		{
			return false;
		}
	}

	return true;
}

URpgBaseStorageComponent* URpgCraftingStationComponent::GetLinkedBaseStorage() const
{
	return bUseLinkedBaseStorage && LinkedBaseCamp ? LinkedBaseCamp->GetBaseStorageComponent() : nullptr;
}
