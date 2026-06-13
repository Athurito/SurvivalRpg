#include "RpgBaseStorageStationActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "RpgBaseStorageStationComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageStationActor)

ARpgBaseStorageStationActor::ARpgBaseStorageStationActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitSphereRadius(120.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("Interactable_OverlapDynamic"));
	InteractionCollision->SetGenerateOverlapEvents(true);

	StorageStationComponent = CreateDefaultSubobject<URpgBaseStorageStationComponent>(TEXT("StorageStationComponent"));
}
