#include "RpgInventoryContainerActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
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

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitSphereRadius(120.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("Interactable_OverlapDynamic"));
	InteractionCollision->SetGenerateOverlapEvents(true);

	InventoryManagerComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("InventoryManagerComponent"));
	// Spatial storage capacity is defined exclusively by the 10x6 grid and item stack limits.
	InventoryManagerComponent->SetCapacityMode(ERpgInventoryCapacityMode::Unlimited);
	ContainerComponent = CreateDefaultSubobject<URpgInventoryContainerComponent>(TEXT("ContainerComponent"));
}
