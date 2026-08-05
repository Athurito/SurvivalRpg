#include "RpgPersonalStorageLockerActor.h"

#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseCampActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPersonalStorageLockerActor)

ARpgPersonalStorageLockerActor::ARpgPersonalStorageLockerActor(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	bNetUseOwnerRelevancy = true;
	SetReplicatingMovement(false);
	SetActorHiddenInGame(true);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InventoryManager =
		CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("InventoryManager"));
	InventoryManager->SetReplicationPolicy(
		ERpgInventoryReplicationPolicy::OwnerOnly);
	InventoryManager->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	InventoryManager->SetFixedMaxEntries(20);
	FRpgInventoryGridSize LockerGridSize;
	LockerGridSize.Width = 4;
	LockerGridSize.Height = 5;
	InventoryManager->SetDefaultGridSize(LockerGridSize);
}

void ARpgPersonalStorageLockerActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, BaseId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, ProfileKey, COND_OwnerOnly);
}

void ARpgPersonalStorageLockerActor::InitializeLocker(
	ARpgBaseCampActor* InBaseCamp,
	APlayerController* OwningController,
	const FString& InProfileKey)
{
	if (!HasAuthority() || !InBaseCamp || !OwningController ||
		InProfileKey.IsEmpty())
	{
		return;
	}

	BaseId = InBaseCamp->GetBaseId();
	OwningBaseCamp = InBaseCamp;
	ProfileKey = InProfileKey;
	SetActorLocation(InBaseCamp->GetActorLocation());
	SetOwner(OwningController);
	ForceNetUpdate();
}

void ARpgPersonalStorageLockerActor::ReassignOwningController(
	APlayerController* OwningController)
{
	if (!HasAuthority() || !OwningController)
	{
		return;
	}

	SetOwner(OwningController);
	ForceNetUpdate();
}
