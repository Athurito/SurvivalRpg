#include "RpgBaseCampActor.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseBuildableDefinition.h"
#include "RpgBaseStorageComponent.h"
#include "RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseCampActor)

ARpgBaseCampActor::ARpgBaseCampActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BaseStorageComponent = CreateDefaultSubobject<URpgBaseStorageComponent>(TEXT("BaseStorageComponent"));

	ArmoryInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("ArmoryInventoryComponent"));
	ArmoryInventoryComponent->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	ArmoryInventoryComponent->SetFixedMaxEntries(32);
}

void ARpgBaseCampActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, BaseId);
}

bool ARpgBaseCampActor::CanPlaceBuildableAtTransform(const URpgBaseBuildableDefinition* BuildableDefinition, const FTransform& BuildTransform, const AActor* RequestingActor) const
{
	if (!BuildableDefinition || !BuildableDefinition->BuildActorClass)
	{
		return false;
	}

	if (!BuildableDefinition->RequiredUnlockTags.IsEmpty() && !GetGrantedStorageUpgradeTags().HasAllExact(BuildableDefinition->RequiredUnlockTags))
	{
		return false;
	}

	const float EffectiveBaseRadius = BuildableDefinition->MaxPlacementDistanceFromBase > 0.0f
		? BuildableDefinition->MaxPlacementDistanceFromBase
		: BuildRadius;
	if (EffectiveBaseRadius > 0.0f &&
		FVector::DistSquared(GetActorLocation(), BuildTransform.GetLocation()) > FMath::Square(EffectiveBaseRadius))
	{
		return false;
	}

	if (RequestingActor && BuildableDefinition->MaxPlacementDistanceFromBuilder > 0.0f &&
		FVector::DistSquared(RequestingActor->GetActorLocation(), BuildTransform.GetLocation()) > FMath::Square(BuildableDefinition->MaxPlacementDistanceFromBuilder))
	{
		return false;
	}

	return true;
}

void ARpgBaseCampActor::RegisterStorageStation(URpgBaseStorageStationComponent* Station)
{
	if (!Station)
	{
		return;
	}

	RegisteredStorageStations.RemoveAll([](const TWeakObjectPtr<URpgBaseStorageStationComponent>& ExistingStation)
	{
		return !ExistingStation.IsValid();
	});
	RegisteredStorageStations.AddUnique(Station);
}

void ARpgBaseCampActor::UnregisterStorageStation(URpgBaseStorageStationComponent* Station)
{
	RegisteredStorageStations.RemoveAll([Station](const TWeakObjectPtr<URpgBaseStorageStationComponent>& ExistingStation)
	{
		return !ExistingStation.IsValid() || ExistingStation.Get() == Station;
	});
}

TArray<URpgBaseStorageStationComponent*> ARpgBaseCampActor::GetStorageStations() const
{
	TArray<URpgBaseStorageStationComponent*> Results;
	for (const TWeakObjectPtr<URpgBaseStorageStationComponent>& StationPtr : RegisteredStorageStations)
	{
		if (URpgBaseStorageStationComponent* Station = StationPtr.Get())
		{
			Results.Add(Station);
		}
	}
	return Results;
}

FGameplayTagContainer ARpgBaseCampActor::GetGrantedStorageUpgradeTags() const
{
	FGameplayTagContainer GrantedTags;
	for (const TWeakObjectPtr<URpgBaseStorageStationComponent>& StationPtr : RegisteredStorageStations)
	{
		if (const URpgBaseStorageStationComponent* Station = StationPtr.Get())
		{
			GrantedTags.AppendTags(Station->GetGrantedUpgradeTags());
		}
	}
	return GrantedTags;
}

bool ARpgBaseCampActor::HasStorageUpgradeTag(FGameplayTag UpgradeTag) const
{
	return UpgradeTag.IsValid() && GetGrantedStorageUpgradeTags().HasTagExact(UpgradeTag);
}
