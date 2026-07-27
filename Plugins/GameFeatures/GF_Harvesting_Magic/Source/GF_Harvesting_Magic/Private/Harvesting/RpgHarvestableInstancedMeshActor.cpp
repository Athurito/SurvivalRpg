#include "Harvesting/RpgHarvestableInstancedMeshActor.h"

#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestableInstancedMeshActor)

ARpgHarvestableInstancedMeshActor::ARpgHarvestableInstancedMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(2.0f);

	HarvestableInstances = CreateDefaultSubobject<URpgHarvestableInstancedMeshComponent>(TEXT("HarvestableInstances"));
	SetRootComponent(HarvestableInstances);
}
