#include "RpgBaseCampActor.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseStorageComponent.h"
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
