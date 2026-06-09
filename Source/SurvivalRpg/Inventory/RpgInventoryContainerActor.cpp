#include "RpgInventoryContainerActor.h"

#include "Components/SceneComponent.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryContainerActor)

ARpgInventoryContainerActor::ARpgInventoryContainerActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InventoryManagerComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("InventoryManagerComponent"));
	ContainerComponent = CreateDefaultSubobject<URpgInventoryContainerComponent>(TEXT("ContainerComponent"));
}
